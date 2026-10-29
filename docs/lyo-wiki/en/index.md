# Lyo-D42 mobile chassis

Firmware 0.4.0 runs on ESP32-S3 / RoboCore S3 and controls two stepper drivers: left ID1 and right ID2. Wheel speed is capped at **0.32 m/s**. Host interfaces include native USB CDC, UART0, HTTP, WebSocket and classic CAN. Python and Rust SDKs are supplied with examples that read state by default.

| Goal | Guide |
|---|---|
| First operation and web control | [Quick start](quickstart.md) |
| Wiring and board resources | [Hardware](hardware.md) |
| Commands and units | [JSON / CAN API](api.md) |
| Python / Rust applications | [Host SDKs and downloads](host.md) |
| Wired host serial | [USB CDC / UART](usb-cdc.md) |
| CAN adapter integration | [CAN tutorial](can.md) |
| Build or modify firmware | [Development](development.md) |
| Diagnose faults | [Troubleshooting](faq.md) |

Standard hardware does not require a TTL Node. OLED and IMU are optional. Release candidates are locally compiled and tested; actual braking distance, temperature, current readings and hill holding require hardware acceptance.
