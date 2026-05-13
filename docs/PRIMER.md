TASK: [paste here]
FUNCTIONS/HARDWARE FOCUS: [paste here]

---

Project: ChronoBloom ESP32-C3
Repo: C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
Variants: esp32c3_v3_8inch | esp32c3_v3_15inch (8inch is current default env)
Firmware: src/main.cpp (single file, ~2200 lines)

Rules: read WORKFLOW.md before any implementation work.
Role: Chat = planning only. Claude Code = all implementation.
Source of truth: local repo supersedes GitHub and all prior chat context.

Build:       pio run -e esp32c3_v3_8inch
OTA flash:   pio run -e esp32c3_v3_8inch -t upload          (espota default, targets 192.168.1.110)
Web UI OTA:  http://esp32c3-v3-8inch.local/update           (most convenient -- upload firmware.bin in browser)
USB flash:   pio run -e esp32c3_v3_8inch -t upload --upload-protocol esptool --upload-port COMx  (first flash / recovery only)

Filesystem MCP: full read/write access to repo path is pre-authorized for all sessions. No per-operation confirmation required for files under C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3\.

Read minimum files the task requires. State what you read and why.
