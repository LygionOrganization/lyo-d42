# Lyo-D42 release procedure

1. Update version, protocol docs, SDK examples and wiki source pages together. Keep the0.32m/s cap in firmware, UI and SDKs. Preserve all third-party license notices.
2. Run python tools/embed_web.py, then pio run. Run chassis/CAN/OLED/IMU host tests and UI tests; run Python SDK tests and cargo test --all-targets for Rust. Record actual results in RELEASE_VALIDATION.md, distinguishing host/platform/hardware checks.
3. Run python tools/package_release.py after successful compilation. It creates source, firmware, Python SDK and Rust SDK ZIPs under .pio/releases/0.4.0 plus a SHA256 manifest. Firmware/source hashes bind the build inputs; hardware_validated remains false until separately established.
4. Use tools/sync_lyo_wiki.py to copy current docs and candidate downloads to the existing wiki workspace. Build MkDocs locally, inspect warnings and check relative links.
5. Review artifacts, license attribution, protocol compatibility and physical acceptance. Flashing, package-registry publication, GitHub releases and remote wiki deployment are separate explicit actions; this workflow does not perform them.

Inherited RoboCore packaging tools are legacy references. Only package_release.py is the Lyo-D42 distribution entry point. No secrets, toolchains, caches, .git, local settings or absolute workspace paths belong in ZIPs. Firmware offsets: bootloader0x0, partitions0x8000, application0x10000; flash8MB. Do not write application-only firmware.bin at0x0.
