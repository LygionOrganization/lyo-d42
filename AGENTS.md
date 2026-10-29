# Lyo-D42 contributor instructions

## Scope and invariants
- This is ESP32-S3 chassis firmware, not generic RoboCore firmware. Keep left ID1, right ID2, 69 mm wheels, 159 mm track and the documented reduction ratio unless hardware requirements change.
- The public wheel speed cap is 0.32 m/s. Enforce it in firmware, UI, SDKs and examples. Register quantization must never exceed this cap.
- No autonomous motion at startup or after a fault. T800 and the firmware watchdog bypass soft stopping. Queries must not renew motion heartbeat.
- Preserve automatic hill hold versus actual holding as separate states. Disabling hold releases outputs after stopping; confirm torque disable and retry failures.
- OLED is optional. Use cached voltage and a nonblocking low-priority idle refresh, at most once per minute. Do not put OLED writes in the command loop.
- Never change motor phase or write motor EEPROM during normal runtime.

## Workflow
- Read relevant sources before editing. Preserve existing local changes; do not reset or clean the working tree.
- Edit web/index.html and web/lyo_d42.js, then run python tools/embed_web.py. src/Web_page.h is generated.
- New user-facing strings must support the existing English/Chinese switch. Preserve form values when switching languages.
- Run the relevant host tests in test/ and pio run. Hardware-dependent claims must be labelled as unverified unless tested on hardware.
- Keep protocol docs, sdk/, examples and wiki copies consistent. SDK code and comments are English.
- Do not flash hardware, publish packages, deploy the wiki or create a release remotely unless requested.

## Validation
Windows host tests: python test/run_chassis_tests.py (also --can, --screen), python test/run_robocore_tests.py. UI logic: node test/chassis_ui_test.cjs. Browser tests require Playwright and tools/preview_web.py. Build: pio run.
Release packaging: python tools/package_release.py; inspect the SHA256 manifest and release validation report. Do not invent licensing for inherited libraries; retain their notices.
