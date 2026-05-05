# Claude Development Instructions

This file acts as a router to project-specific documentation.

## Quick Reference

- **Workflow rules**: See [WORKFLOW.md](WORKFLOW.md) — Claude Chat vs Claude Code role separation, source of truth hierarchy
- **Current issues**: See [REVIEW.md](REVIEW.md) — Technical debugging notes, GPIO3/4 button issue, render safety audit
- **Build/flash**: See [README.md](README.md) — PlatformIO commands, pin assignments, hardware specs
- **Features**: See [README.md](README.md) — Current feature set, LED mapping, web UI capabilities

## Critical Rules

1. Local repo (`C:\Users\SuperMaster\Documents\PlatformIO\neopixelClock-esp32c3-v3`) is #1 truth source
2. Claude Chat = planning only, Claude Code = all implementation
3. Never implement from Chat when Code is available
4. Always read WORKFLOW.md before starting work

## Session Anchor

Use phrase: **"3-RingNeoPixelClock workflow rules active"** in first message of new sessions to confirm workflow rules are loaded.
