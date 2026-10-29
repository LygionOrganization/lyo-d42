# Hardware and wiring

| Resource | Configuration |
|---|---|
| Chassis | 214.5 ×197.6 ×86.5mm, two top1020 profiles |
| Track / wheel diameter |159mm /69mm|
| Motor / gearbox |3200 steps per motor revolution /5.1818181818182|
| Wheels |ID1 left, ID2 right; public positive speed is forward for both|
| Motor bus |UART1 TX4/RX5,1Mbps, shared TTL and RS485 device bus|
| Host UART |UART0 through Type-C UART,1Mbps|
| Native USB |Type-C USB, USB CDC JSON lines|
| CAN |TX14/RX13,500kbit/s, physical switch set to CAN|
| OLED |128×32, I²C SDA6/SCL7,0x3C|
| Light |GPIO21,20kHz PWM|

Use the matching battery and verify markings and polarity with power off. A5s tool battery is typically labelled18V/20V; lower-voltage3s operation reduces available torque. Bus power contacts expose battery voltage; logic voltage and power supply voltage are different requirements.

TTL uses5264-3P; RS485 uses XH2.54-4P with A/B/power+/−. The CAN2+2 connection includes signal and XT30 power. Use a proper CAN adapter and terminate the physical bus at both ends. Do not connect host UART pins directly to CAN-H/L or the motor device bus.

Expansion devices need unique IDs3–253. Standard operation requires ID1/ID2, not ID30 TTL Node. See [USB/UART](usb-cdc.md) and [CAN wiring](can.md).
