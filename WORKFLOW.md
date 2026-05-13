# ChronoBloom ESP32-C3 — Development Workflow

## Claude Chat vs Claude Code Role Separation

**Claude Chat (claude.ai web interface)**:
- Planning and strategy
- Design decisions and architecture discussions
- Hardware specifications and pin mapping documentation
- Feature proposals and enhancement roadmaps
- Historical research and documentation review
- Visual mockups and web UI design concepts
- Snapshot updates and handoff preparation

**Claude Code (PlatformIO project)**:
- ALL implementation work
- File creation, editing, and deletion
- Firmware builds via PlatformIO
- Code deployment and flashing
- SSH operations (Pi4, GandalfAI, NAS)
- Python script edits
- Overlayfs persistence on Pi4
- Hardware testing and debugging

**Gate Rule**: Never implement or deploy from Claude Chat when Claude Code is available. Always route execution work to Claude Code with current snapshot as context.

This rule applies to every session regardless of topic or urgency.

---

## Source of Truth Hierarchy

1. **Local repo** (`C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3`, project: ChronoBloom ESP32-C3) = #1 truth source
2. **GitHub repo** = #1.5 backup/reference
3. **Snapshot files** = session context, derived from local repo

**Conflict Resolution**:
- If snapshot conflicts with GitHub: local snapshot wins
- If GitHub conflicts with local repo: local repo wins
- GitHub is authoritative ONLY when local repo is unavailable

---

## User-Applied Updates

**No partial inserts/additions for**:
- User-applied documentation updates
- User-applied config changes
- User-applied code edits

**If user reports "I added X to the repo"**:
- Acknowledge the change
- Update snapshot/planning docs to reflect the change
- Do NOT attempt to re-implement or merge the change via Claude Code
- Trust local repo state over snapshot state

---

## Session Handoff Protocol

**When transitioning to Claude Code**:
1. Summarize proposed changes in Claude Chat
2. User confirms scope
3. Provide snapshot context file to Claude Code session
4. Claude Code reads local repo state first, snapshot second
5. Claude Code implements, tests, commits

**Never**:
- Implement in Chat then ask user to copy-paste
- Write full code files in Chat "for reference"
- Provide step-by-step terminal commands expecting user to execute manually (unless explicitly requested)

---

## Session Continuity

These rules persist across:
- New chat sessions
- Different projects discussed in same session
- Hardware vs software topic switches
- Planning vs debugging mode transitions

**Rule anchor phrase**: "Iris Clock workflow rules active" (use in first message of new sessions to confirm)

---

## File Structure

```
chronobloom-esp32c3/        ← repo folder name (ChronoBloom ESP32-C3 project)
├── WORKFLOW.md          ← This file (development workflow rules)
├── README.md            ← Build instructions, features, hardware specs
├── REVIEW.md            ← Technical issues, debugging notes, firmware audit
├── CLAUDE.md            ← Documentation router for Claude sessions
├── platformio.ini       ← Build configuration for 8" and 15" variants
├── src/
│   └── main.cpp         ← Single-file monolith firmware (~2200 lines, ~34k tokens)
└── docs/
    ├── PRIMER.md        ← Quick session primer (read first)
    ├── FUNCTION_INVENTORY.md ← All 85 functions documented (purpose, lines, state, env effects)
    ├── symmap.json      ← Machine-readable { name, start_line, end_line } symbol map
    ├── HARDWARE.md      ← Pin maps, sensors, physical specs
    ├── FEATURES.md      ← Current/planned/rejected features
    ├── ANIMATIONS.md    ← Animation catalog and triggers
    ├── API.md           ← Web endpoints, settings structure
    ├── TROUBLESHOOTING.md ← Common issues and debugging
    └── HISTORY.md       ← Project lineage
```

**Documentation Priority**:
1. Check WORKFLOW.md for development process
2. Check REVIEW.md for known issues and debugging status
3. Check README.md for build/flash commands and feature list

---

## End-of-Session Close-the-Loop

Run this checklist after every successful Claude Code deployment session before committing.

### 1. Symmap regeneration (if `src/main.cpp` changed)
```powershell
cd C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
python tools/gen_symmap.py
```
This updates `docs/symmap.json` and `docs/FUNCTION_INVENTORY.md`. Always commit these alongside `main.cpp`.

### 2. Doc sweep — update every affected file
| File | What to update |
|------|----------------|
| `docs/CHANGELOG.md` | Add `[x.y.z] - date` entry: Added/Changed/Files changed |
| `REVIEW.md` | Mark task Done in maturation checklist with date |
| `docs/FEATURES.md` | Move Planned → Done; update or fix Removed section |
| `docs/SESSIONS.md` | Mark session Done in status table; add Completed Session block |
| `README.md` | Update pin table, hardware notes, known issues if hardware changed |
| `docs/TROUBLESHOOTING.md` | Mark resolved issues; update code snippets if APIs changed |
| `docs/PRIMER.md` | Update build/OTA commands only if workflow changed |
| `memory/project_state.md` | Update version, milestones, maturation task status |

Skip a file if it is genuinely unaffected. Do not open files just to check.

### 3. Build + OTA flash
```powershell
pio run -e esp32c3_v3_8inch
curl -s -F "image=@.pio\build\esp32c3_v3_8inch\firmware.bin" http://esp32c3-v3-8inch.local/update
```
Confirm `200 Update successful` before committing.

### 4. Commit and push
Stage only files that changed this session (never `git add -A`):
```powershell
git add src/main.cpp platformio.ini docs/CHANGELOG.md REVIEW.md docs/FEATURES.md docs/SESSIONS.md docs/symmap.json docs/FUNCTION_INVENTORY.md
# add any other changed files explicitly
git commit -m "feat/fix/docs(<scope>): <summary>

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
git push
```

### 5. Verify push
```powershell
git log --oneline -3
git status
```
Clean working tree + commit appears in log = session closed.

---

## Token Reduction — Reading main.cpp

`src/main.cpp` is ~2200 lines (~34k tokens). Reading it whole exceeds context limits and wastes budget.

**Required approach for any main.cpp work**:
1. Find the relevant function in `docs/FUNCTION_INVENTORY.md` (purpose, state, dependencies)
2. Get its exact line range from `docs/symmap.json`
3. Use `Read` with `offset` and `limit` to fetch only those lines
4. If `symmap.json` line ranges are stale (SHA256 mismatch), do a targeted `Grep` instead of a full read

Never open `src/main.cpp` without a specific line target.
