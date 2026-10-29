# Lyo-D42

**English** | [简体中文](README.zh-CN.md)

An ESP32-S3 differential-drive mobile robot platform with two stepper motors, a browser control panel, and Python and Rust SDKs.

**Firmware 0.4.0** · **0.32 m/s wheel speed limit** · **USB CDC / UART / HTTP / WebSocket / CAN**

[Quick start](#quick-start) · [Host SDKs](#host-sdks) · [Documentation](#documentation) · [Build](#build-from-source)

## Features

| Capability | Behavior |
|---|---|
| Web driving | Joystick and keyboard control; switch between English and Chinese |
| Motion control | Wheel or body velocity, relative distance and rotation; explicit acceleration in m/s² |
| Soft stopping | Optional deceleration using the selected acceleration |
| Automatic hill hold | Drive normally, then hold at current register 60 after stopping; release torque when disabled |
| Communication watchdog | Default 3 seconds; configurable from the API or web settings |
| Feedback | Wheel speed, battery voltage, driver temperatures and optional raw current |
| Tilt | Roll and Pitch adjusted for the vertical PCB installation; relative Yaw remains in IMU debugging |
| Lighting and display | Spotlight PWM starts at 40%; OLED voltage refreshes only during quiet idle periods |

All chassis motion interfaces enforce the **0.32 m/s** wheel limit. Faults, watchdog expiry and T800 bypass soft stopping. Startup and fault recovery never resume an old motion command.

## Quick start

1. Raise the drive wheels and check power polarity and motor wiring. USB power alone does not power the motors.
2. Connect to Wi-Fi **Robot**, password **12345678**, unless saved settings differ.
3. Open **http://192.168.4.1** and check feedback from left driver **ID1** and right driver **ID2**.
4. Select a low speed and acceleration. Hold the joystick, W/A/S/D or arrow keys to drive; release to stop. Space requests a hard stop.
5. Verify direction, stopping and connection-loss behavior before testing on the floor.

Use the top-right language button to switch the interface. Auto hill hold permits driving and holds after stopping. With it off, stopping releases torque. The optional OLED reads cached voltage and refreshes at most once per minute after its first idle refresh; active control postpones refreshes.

## Host SDKs

Both SDKs support USB CDC, UART0, HTTP, WebSocket and CAN. Examples **read state by default**. Add `--drive` only to request the bounded one-second demonstration at 0.03 m/s.

### Python

Requires Python 3.10 or newer. From the project root:

```sh
python -m pip install "./sdk/python[all]"
python sdk/python/examples/control.py usb COM7
python sdk/python/examples/control.py http http://192.168.4.1
```

```python
from lyo_d42 import Client, SerialTransport

with Client(SerialTransport("COM7")) as robot:
    print(robot.status())
    # Explicit opt-in after checking wheel clearance:
    # robot.drive_for(0.03, 0, 0.2, 1.0)
```

### Rust

Requires Rust and the platform linker/build tools:

```sh
cargo test --locked --manifest-path sdk/rust/Cargo.toml
cargo run --locked --manifest-path sdk/rust/Cargo.toml --example control -- usb COM7
```

Replace COM7 with the actual port. For constructors, transport backends and control methods, see the [Python SDK](sdk/python/README.md) and [Rust SDK](sdk/rust/README.md).

Use one active controller. Send acceleration with each target and keep updates tied to fresh application input. Position moves, hill hold and soft stopping also need heartbeat updates. SDKs do not automatically retry motion or run an independent background heartbeat.

## Hardware and interfaces

| Resource | Configuration |
|---|---|
| Controller | ESP32-S3, 8 MB Flash / 8 MB OPI PSRAM |
| Wheels | 69 mm diameter; 159 mm track |
| Drive | 3200 microsteps per motor revolution; gearbox ratio 5.1818181818182 |
| Motor bus | UART1 TX4 / RX5, 1 Mbps; left ID1, right ID2 |
| Native USB | Type-C USB: USB CDC host communication |
| Host UART | Type-C UART: UART0 JSON at 1 Mbps; enable replies with T604 |
| CAN | 500 kbit/s, classic extended frames; TX14 / RX13; physical switch set to CAN |
| OLED | Optional 128 × 32 display, I²C SDA6 / SCL7 |
| Spotlight | GPIO21 PWM |

Public wheel speeds are positive forward for both wheels; firmware handles the right motor's reversed installation. Standard operation does not require a TTL Node or RC receiver. Verify battery voltage and connector markings before connecting expansion devices.

## Documentation

| Topic | Reference |
|---|---|
| Chassis JSON and CAN commands | [Protocol reference](docs/lyo-wiki/en/api.md) · [中文](docs/LYO_D42_API.md) |
| USB CDC and UART | [Tutorial](docs/lyo-wiki/en/usb-cdc.md) · [中文](docs/LYO_D42_USB_CDC.md) |
| CAN wiring, frames and adapters | [Tutorial](docs/lyo-wiki/en/can.md) · [中文](docs/LYO_D42_CAN.md) |
| SDKs and examples | [Python](sdk/python/README.md) · [Rust](sdk/rust/README.md) |
| Release process | [Packaging](docs/RELEASING.md) |
| Verification | [Software checks and hardware acceptance](docs/RELEASE_VALIDATION.md) |
| Contributions and provenance | [AGENTS.md](AGENTS.md) · [Upstream](docs/UPSTREAM.md) |

HTTP 202 confirms queue acceptance, not execution. Match results by `rid`. JSON T800 has no execution ACK. CAN has its own ACK and compact status frames; it does not expose every JSON field. Current feedback is raw register data with a validity flag, **not calibrated amperes**. Relative distance/rotation controls motor steps, not ground-truth position.

## Build from source

Open this project in PlatformIO. Platform and library versions are pinned in `platformio.ini`.

```sh
python tools/embed_web.py
pio run
```

Edit `web/index.html` and `web/lyo_d42.js`, then regenerate the embedded page before compiling. The build environment is `esp32-s3-devkitc-1-n8r8`.

```sh
python tools/package_release.py
```

Packaging creates source, firmware, Python SDK and Rust SDK ZIPs plus a SHA256 manifest under `.pio/releases/0.4.0/`. It does not flash hardware or publish remotely. Application-only `firmware.bin` belongs at **0x10000**, not 0x0; use the firmware package's offset manifest.

## Project status

This is a release candidate with firmware builds, host regression tests, SDK tests and browser checks. See the [validation record](docs/RELEASE_VALIDATION.md) for the exact scope. Electrical behavior, motor noise, temperature, braking distance and hill-hold capability still require hardware acceptance; packages retain `hardware_validated: false`.

The display name is **Lyo-D42** and the project/NVS identifier is **lyo-d42**. Legacy settings are read only when the current namespace is absent; new saves use the current namespace. Raw stepper debugging is separate from the chassis watchdog and requires explicit output disable.

## License and attribution

The project's [GPLv3 license](LICENSE) is retained alongside the inherited RoboCore [AGPLv3 license](LICENSE-AGPL). Individual libraries retain their own notices. See [upstream provenance](docs/UPSTREAM.md) for the source revision and scope. Inherited generic examples are references, not the Lyo-D42 protocol specification.
