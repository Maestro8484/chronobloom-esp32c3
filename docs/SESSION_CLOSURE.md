# ChronoBloom ESP32-C3 — Session Closure Protocol
**Mandatory at the end of every Claude Code session.**

This file is the authoritative checklist. PRIMER.md references it. SESSIONS.md depends on it.
A session is not complete until every applicable item below is checked and committed.

---

## When to Run This Protocol

Run at the end of every session — including docs-only sessions, partial sessions, and sessions
that end in a build failure. The closure documents what was attempted, not just what succeeded.

---

## Checklist

### Step 1 — Determine session scope
Read the task description and what was actually completed. Note the firmware version if changed.

### Step 2 — SESSIONS.md: status table row
Add or update the row for this session in the status table at the top of SESSIONS.md.

Format:
```
| N | Short session name | One-line task description | Done (YYYY-MM-DD[, vX.Y.Z]) |
```

Rules:
- N is the next sequential session number. Check the table for the current highest number.
- Use "Done" for fully completed, "Partial (YYYY-MM-DD)" for incomplete, "Abandoned" for cancelled.
- Include the version tag only if firmware was built and flashed.

### Step 3 — SESSIONS.md: completed session narrative block
Add a narrative block under "## Completed Sessions" immediately after the status table entry is confirmed.

Format:
```markdown
### Session N — Short Name (Done YYYY-MM-DD[, vX.Y.Z])
- Bullet 1: what was changed and why (be specific: function names, file names, line counts)
- Bullet 2: decisions made and their rationale
- Bullet 3: build result if applicable (both envs clean, RAM %, Flash %)
- Bullet 4: commit SHA if committed
- Note any follow-up items or known issues introduced
```

Rules:
- Every step actually executed gets a bullet. Do not summarize multiple changes into one vague line.
- If the session was docs-only: say "No firmware changes."
- If the session was abandoned: say why.

### Step 4 — CHANGELOG.md (firmware sessions only)
If main.cpp, web_html.h, or platformio.ini changed:

1. Add a new `## [vX.Y.Z] - YYYY-MM-DD` section at the top (below the header).
2. Use Added / Changed / Fixed / Removed subsections as applicable.
3. List every function modified and what changed.
4. List every file changed under "### Files changed".

If docs-only: skip this step. Add a brief docs entry if doc changes are significant.

### Step 5 — Symmap regeneration (firmware sessions only)
If main.cpp or web_html.h changed (functions added, removed, renamed, or line numbers shifted):

```powershell
python tools/gen_symmap.py
```

Verify output: "N functions found" count is plausible. Check for parser errors or zero-function output.
If gen_symmap.py fails: document the failure in SESSIONS.md and stop. Do not commit stale symmap.

### Step 6 — PRIMER-extended.md staleness check
Scan PRIMER-extended.md for any line that is now wrong:
- Feature state list (add completed features, remove planned items that shipped)
- Pin reference (if hardware changed)
- File structure section (if new source files added)
- Build commands (if platformio.ini upload config changed)
- Function count in code navigation section

Update any stale lines. This file is read by humans before every session; staleness has compounding cost.

### Step 7 — Commit
Commit all changed documentation files together with the firmware commit, or as a follow-up
docs commit immediately after. Never leave doc changes uncommitted at session end.

Suggested commit message format:
```
docs: session N closure — [short description]
```
or combined with firmware:
```
vX.Y.Z: [firmware change summary]; docs: session N closure
```

---

## Quick Reference — What Triggers Each Step

| Change type | Step 2 | Step 3 | Step 4 | Step 5 | Step 6 |
|---|---|---|---|---|---|
| Firmware changed (main.cpp / web_html.h) | Always | Always | Yes | Yes | Check |
| platformio.ini changed | Always | Always | Yes | No | Check |
| Docs only | Always | Always | No | No | Check |
| Build attempted but failed | Always | Always | No | No | No |
| Session abandoned mid-task | Always | Always | No | No | No |

---

## Anti-Patterns That Caused Past Documentation Gaps

These are documented failure modes. Avoid them.

| Anti-pattern | Example | Correct action |
|---|---|---|
| Skipping closure on "quick" sessions | web_html.h modularization undocumented | All sessions get closure. No exceptions. |
| Closing only the status table row | Session 9 had no completed narrative | Always write the narrative block too. |
| Assuming symmap is still current | Modularization shifted line numbers | Run gen_symmap.py any time main.cpp changes. |
| Leaving PRIMER-extended.md stale | GPIO3/4 buttons, 85 functions still listed after v2.2 | Check PRIMER-extended.md at every closure. |
| Undocumented hardware changes | Sacrificial LED re-add not in CHANGELOG | Hardware changes get a CHANGELOG entry like code changes. |

---

## Session Numbering Reference
Check docs/SESSIONS.md status table for the current highest session number before assigning the next one.
As of 2026-05-17: last documented session is Session 21 (outerRingBrightness + theme export/import, 2026-05-17, v2.3.0).
Next session to be documented: Session 22.
