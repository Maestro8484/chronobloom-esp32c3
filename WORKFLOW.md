# 3-Ring NeoPixel Clock — Development Workflow

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

1. **Local repo** (`C:\Users\SuperMaster\Documents\PlatformIO\neopixelClock-esp32c3-v3`) = #1 truth source
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

**Rule anchor phrase**: "3-RingNeoPixelClock workflow rules active" (use in first message of new sessions to confirm)

---

## File Structure

```
neopixelClock-esp32c3-v3/
├── WORKFLOW.md          ← This file (development workflow rules)
├── README.md            ← Build instructions, features, hardware specs
├── REVIEW.md            ← Technical issues, debugging notes, firmware audit
├── platformio.ini       ← Build configuration for 8" and 15" variants
├── src/
│   └── main.cpp         ← Single-file monolith firmware (~1600 lines)
└── .git/                ← Local git repo
```

**Documentation Priority**:
1. Check WORKFLOW.md for development process
2. Check REVIEW.md for known issues and debugging status
3. Check README.md for build/flash commands and feature list
