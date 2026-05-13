#!/usr/bin/env python3
"""
Generate docs/symmap.json and docs/FUNCTION_INVENTORY.md from src/main.cpp.
Tracks class scope to produce qualified names (ClassName::method).
"""

import re
import json
import hashlib
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
SRC = REPO_ROOT / "src" / "main.cpp"
SYMMAP_OUT = REPO_ROOT / "docs" / "symmap.json"
INVENTORY_OUT = REPO_ROOT / "docs" / "FUNCTION_INVENTORY.md"

# Recognise "class Foo {" or "struct Foo {" lines (class definition starts)
CLASS_OPEN_RE = re.compile(r'^(?:class|struct)\s+(\w+)\s*(?::\s*.+?)?\{?\s*(?://.*)?$')

# Recognise a function/method definition line.
# Return type may be: void, bool, int, uint8_t/16/32, float, double, String,
#   ClockTime, ClockSettings, const <type>, explicit ctor (no return type keyword).
FUNC_DEF_RE = re.compile(
    r'^'
    r'(?:static\s+|explicit\s+|inline\s+|virtual\s+|override\s+)*'
    r'(?:'
        r'(?:const\s+)?(?:void|bool|int|uint8_t|uint16_t|uint32_t|uint64_t|int8_t|int16_t|int32_t'
        r'|float|double|String|ClockTime|ClockSettings|size_t|char)\s*[&*]?\s*'
    r')'
    r'(\w+)\s*\('   # function name
)

# Constructor: ClassName (no return type)
CTOR_RE = re.compile(r'^(explicit\s+)?(\w+)\s*\(')

def compute_sha256(path: Path) -> str:
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()


def find_closing_brace(lines, brace_start_idx):
    """Return 1-based line of the closing '}' that closes the block opened at brace_start_idx."""
    depth = 0
    for i in range(brace_start_idx, len(lines)):
        raw = lines[i].split('//')[0]  # strip single-line comments
        depth += raw.count('{') - raw.count('}')
        if depth <= 0 and i >= brace_start_idx:
            return i + 1  # 1-based
    return len(lines)


def extract_functions(src_path: Path):
    text = src_path.read_text(encoding='utf-8', errors='replace')
    lines = text.splitlines()

    # Strip raw string literals so we don't parse embedded HTML/JS
    # Replace each line inside R"...(" ... )..." with a blank line
    clean_lines = list(lines)
    in_raw = False
    raw_end_marker = None
    for i, line in enumerate(lines):
        if not in_raw:
            m = re.search(r'R"(\w*)\(', line)
            if m:
                raw_end_marker = ')' + m.group(1) + '"'
                in_raw = True
                # blank out content after the opening
                pre = line[:line.index('R"' + m.group(1) + '(')]
                clean_lines[i] = pre
                if raw_end_marker in line[line.index('R"'):]:
                    in_raw = False
                    raw_end_marker = None
        else:
            if raw_end_marker and raw_end_marker in line:
                in_raw = False
                raw_end_marker = None
            clean_lines[i] = ''

    # Track brace depth and class names to qualify methods
    class_stack = []   # list of (class_name, open_depth)
    brace_depth = 0
    functions = []

    # Set of known class names (populated as we see 'class Foo {')
    known_classes = set()

    # Pre-pass: collect class names
    for line in clean_lines:
        s = line.strip()
        m = CLASS_OPEN_RE.match(s)
        if m:
            known_classes.add(m.group(1))

    for i, line in enumerate(clean_lines):
        lineno = i + 1
        stripped = line.strip()

        # Update class stack based on class/struct openings
        class_match = CLASS_OPEN_RE.match(stripped)
        if class_match:
            class_name = class_match.group(1)
            # Opening brace may or may not be on this line
            opens = stripped.count('{')
            closes = stripped.count('}')
            if opens > closes:
                class_stack.append((class_name, brace_depth + opens - closes))
            # If no brace yet, we'll catch it next iteration

        # Update brace depth
        raw = stripped.split('//')[0]
        brace_depth += raw.count('{') - raw.count('}')

        # Pop class stack when we exit a class scope
        while class_stack and brace_depth < class_stack[-1][1]:
            class_stack.pop()

        # Skip preprocessor, comments, blank lines
        if not stripped or stripped.startswith('#') or stripped.startswith('//') \
                or stripped.startswith('*') or stripped.startswith('/*'):
            continue

        # Skip forward-declarations (end with ; no {)
        if stripped.endswith(';') and '{' not in stripped:
            continue

        # Skip lambdas: [] ( ...
        lambda_m = re.match(r'^\s*\[', stripped)
        if lambda_m:
            continue

        # Must have opening paren (parameter list)
        if '(' not in stripped:
            continue

        # Try to match a function definition
        fn_name = None

        m = FUNC_DEF_RE.match(stripped)
        if m:
            fn_name = m.group(1)
        else:
            # Try constructor (no return type): name must be a known class name
            cm = CTOR_RE.match(stripped)
            if cm:
                candidate = cm.group(2)
                if candidate in known_classes:
                    fn_name = candidate

        if fn_name is None:
            continue

        # Skip keywords that look like functions
        if fn_name in ('if', 'for', 'while', 'switch', 'return', 'sizeof',
                       'static_assert', 'constexpr', 'case', 'else', 'delete',
                       'new', 'throw', 'catch', 'namespace', 'template',
                       'typename', 'constrain', 'map', 'max', 'min', 'abs',
                       'log10', 'snprintf', 'printf', 'Serial', 'delay',
                       'millis', 'micros', 'digitalWrite', 'digitalRead',
                       'pinMode', 'attachInterrupt', 'detachInterrupt',
                       'analogRead', 'analogWrite', 'noInterrupts', 'interrupts',
                       'memcpy', 'memset', 'strcpy', 'strcmp', 'strlen',
                       'localtime_r', 'time', 'static_cast', 'reinterpret_cast',
                       'dynamic_cast', 'const_cast'):
            continue

        # Find opening brace
        has_brace = False
        brace_start_idx = i
        for j in range(i, min(i + 5, len(clean_lines))):
            test = clean_lines[j].split('//')[0]
            if '{' in test:
                has_brace = True
                brace_start_idx = j
                break
        if not has_brace:
            continue

        end_line = find_closing_brace(clean_lines, brace_start_idx)

        # Qualify with class name if we're inside a class
        current_class = class_stack[-1][0] if class_stack else None
        if current_class:
            qualified = f"{current_class}::{fn_name}"
        else:
            qualified = fn_name

        # Deduplicate: skip if same name+start already recorded
        if functions and functions[-1]['name'] == qualified and functions[-1]['start_line'] == lineno:
            continue

        functions.append({
            'name': qualified,
            'start_line': lineno,
            'end_line': end_line
        })

    return functions


def build_inventory(functions) -> str:
    rows = []
    rows.append("# ChronoBloom Function Inventory")
    rows.append("")
    rows.append("Auto-generated from `src/main.cpp`. Do not edit manually — run `tools/gen_symmap.py` to regenerate.")
    rows.append("")
    rows.append(f"Total functions: {len(functions)}")
    rows.append("")
    rows.append("| # | Function | Lines | Span |")
    rows.append("|---|----------|-------|------|")

    for idx, fn in enumerate(functions, 1):
        name = fn['name']
        start = fn['start_line']
        end = fn['end_line']
        span = end - start + 1
        rows.append(f"| {idx} | `{name}` | {start}–{end} | {span} |")

    rows.append("")
    rows.append("---")
    rows.append("*Generated by `tools/gen_symmap.py`*")
    return "\n".join(rows) + "\n"


def main():
    print(f"Reading {SRC}")
    sha = compute_sha256(SRC)
    functions = extract_functions(SRC)
    print(f"Found {len(functions)} functions")

    symmap = {
        "generated_by": "tools/gen_symmap.py",
        "source_file": "src/main.cpp",
        "source_sha256": sha,
        "functions": functions
    }

    SYMMAP_OUT.write_text(json.dumps(symmap, indent=2) + "\n", encoding='utf-8')
    print(f"Wrote {SYMMAP_OUT}")

    inventory = build_inventory(functions)
    INVENTORY_OUT.write_text(inventory, encoding='utf-8')
    print(f"Wrote {INVENTORY_OUT}")

    print("\nFunction list:")
    for fn in functions:
        print(f"  {fn['name']:55s}  {fn['start_line']:4d}–{fn['end_line']:4d}")


if __name__ == '__main__':
    main()
