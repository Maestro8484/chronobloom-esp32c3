TASK: [paste here]

ChronoBloom ESP32-C3 LED clock project. Claude Code on SuperMaster Desktop.
Single firmware file: src/main.cpp. Two variants: 8inch, 15inch.

Repo: C:\Users\SuperMaster\Documents\PlatformIO\chronobloom-esp32c3
Local repo is source of truth. GitHub is secondary mirror.

Read CLAUDE.md first. It contains session rules and workflow constraints.

Build: pio run -e esp32c3_v3_8inch
Flash: pio run -e esp32c3_v3_8inch -t upload
OTA:   pio run -e esp32c3_v3_8inch -t upload --upload-port esp32c3-v3-8inch.local:3232

Read minimum files the task requires. State what you read and why.