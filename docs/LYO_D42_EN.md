# Lyo-D42 0.4.0

ESP32-S3 dual-stepper chassis firmware with a language-switchable English/Chinese web console and USB CDC, UART0, HTTP, WebSocket and CAN control. Python and Rust SDKs are included in sdk/.

The wheel speed hard cap is **0.32 m/s**. Native speed quantization is capped at register489 (about0.31963m/s). Physical acceleration is sent as accel_mps2 (0.01–5m/s²; CAN0.01–2.54). Soft stopping ramps down; T800, faults and watchdog expiry disable directly. Auto hill hold allows driving and holds at current register60 after stopping. Disabling it releases outputs; firmware verifies register40 and retries failed releases.

The default watchdog is3000ms, configurable with T809 timeout_ms (100..60000) while disabled. Queries do not renew motion. Position moves, hill hold and soft stopping also require heartbeat. No autonomous motion at boot or fault recovery. OLED voltage is cached and refreshed by a low-priority task only when disabled and traffic has been quiet for one second, no more than once per minute after the first idle refresh.

## Host control

- [Python SDK](../sdk/python/README.md): USB/UART, HTTP, WS, python-can backends.
- [Rust SDK](../sdk/rust/README.md): USB/UART, HTTP, ws://, Linux SocketCAN, cross-platform SLCAN and custom CanBus trait.
- [Protocol reference](LYO_D42_API.md): complete JSON and CAN layout.

```sh
python -m pip install './sdk/python[all]'
python sdk/python/examples/control.py usb COM7
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- http http://192.168.4.1
```

Examples read state by default. Add --drive only to request the bounded one-second0.03m/s demo. SDK calls require explicit acceleration. There is no independent background heartbeat and movement is never automatically retried. Only one controller should own the chassis.

Native USB CDC uses the Type-C USB port. Type-C UART connects UART0 at1Mbps and requires UART replies enabled (SDK UART mode handles this). The motor bus is UART1 TX4/RX5 at1Mbps, separate from host communication. CAN is500kbit/s, extended classic frames, DLC8, TX14/RX13 with the switch set to CAN. Use a proper host CAN interface and termination.

Left wheel ID1, right ID2; public wheel speeds are positive forward. Wheel diameter69mm, track159mm, gearbox5.1818181818182,3200 microsteps/motor revolution. Do not reverse the public right-wheel sign again.

Current feedback is optional raw register data with a validity flag, not calibrated amperes. Chassis tilt swaps the original Roll/Pitch and subtracts90° from the new Pitch; no chassis yaw is integrated. The IMU debug view retains relative yaw.

Build with pio run; regenerate web sources with python tools/embed_web.py. Use tools/package_release.py for Lyo-D42 release candidates. See RELEASE_VALIDATION.md for actual checks. Hardware electrical behavior, current readings, braking distance and thermal performance remain subject to physical validation; no artifact claims factory qualification.
