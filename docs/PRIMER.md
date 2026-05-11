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

Build:  pio run -e esp32c3_v3_8inch
Flash:  pio run -e esp32c3_v3_8inch -t upload
OTA:    pio run -e esp32c3_v3_8inch -t upload --upload-port esp32c3-v3-8inch.local:3232

Read minimum files the task requires. State what you read and why.
