# Development and release workflow

Download [source](host.md), read AGENTS.md, and open platformio.ini in PlatformIO. The fixed environment is esp32-s3-devkitc-1-n8r8 with8MB flash and OPI PSRAM. Dependencies and platform are pinned in platformio.ini.

```sh
python tools/embed_web.py
pio run
python test/run_chassis_tests.py
python test/run_chassis_tests.py --can
python test/run_chassis_tests.py --screen
python test/run_robocore_tests.py
```

Host C++ tests use Windows Visual Studio Build Tools. Web tests use Playwright and tools/preview_web.py; CHROME_PATH may point to installed Chrome. Python SDK tests use PYTHONPATH=sdk/python and unittest discovery; Rust uses cargo test --all-targets --manifest-path sdk/rust/Cargo.toml.

|Location|Responsibility|
|---|---|
|src/LyoD42.cpp|Chassis state, ramp, hold/release, feedback, watchdog|
|include/ChassisMath.h|Geometry, speed cap and conversions|
|src/main.cpp|Command ingress and idle OLED task|
|src/Can.h|CAN mapping and response fragments|
|web/lyo_d42.js,web/index.html|Drive UI and language switch|
|sdk/python,sdk/rust|Host transports and typed control|

Preserve0.32m/s throughout UI, firmware and SDK. Queries must not renew motion. Do not write motor EEPROM or change phase in normal operation. Keep bus-before-state lock ordering. OLED is optional and must never block the control path. Raw debugging transfers control out of the chassis state machine.

After tests, python tools/package_release.py creates four ZIPs and a SHA256 manifest under .pio/releases/0.4.0. tools/sync_lyo_wiki.py --wiki <checkout> copies reviewed docs/downloads locally; it does not deploy. See docs/RELEASING.md and docs/RELEASE_VALIDATION.md. Hardware flashing, registry publication and remote release are separate steps.

Application firmware.bin belongs at0x10000, not0x0. Record saved settings before recovery. After flashing, query state then test low-speed motion with wheels raised. Host mocks cannot certify current, torque, thermal behavior, braking distance or hill holding.

The LyoD42_SBUS Arduino program is a separate teaching experiment requiring an ID30 TTL Node and receiver. Uploading it replaces the web firmware; it is not a dependency of standard chassis operation.
