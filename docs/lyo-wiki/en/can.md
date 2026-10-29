# CAN bus host tutorial

## Physical connection

Use a host CAN interface supported by your OS: Linux SocketCAN, a USB CAN adapter with a vendor driver, or a LAWICEL/SLCAN serial adapter. A USB/UART converter alone is not a CAN adapter. Set the chassis physical switch to CAN. Connect CAN-H to CAN-H, CAN-L to CAN-L and the reference ground according to the adapter's isolation requirements. Verify power contacts separately; do not connect battery voltage to signal pins.

Use twisted-pair wiring, a line topology, short stubs and120Ω termination at each of the two physical ends. With power off, two120Ω terminators normally measure about60Ω across H/L. Avoid extra terminators. Host and chassis must both use500kbit/s classic CAN with29-bit extended IDs. The chassis uses TX14/RX13 internally. Do not use CAN FD or remote requests.

## SocketCAN read-only check

After installing the adapter driver and can-utils on Linux, replace can0 if needed:

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
ip -details -statistics link show can0
candump can0
```

In another terminal query node1 with sequence42:

```sh
cansend can0 18EF0188#2A00000000000000
```

Expect ACK18EE0108 with sequence42/code0 plus18ED0100,18ED0101 and18ED0102, each with sequence42. All four can arrive in any order. The eight hex digits specify an extended ID in can-utils. Request DLC must be8; ACK DLC is3 and status DLC8. A timeout can mean wrong node, baud, termination, switch selection or older firmware; it is not permission to replay a motion command.

## Python SDK

```sh
python -m pip install './sdk/python[all]'
python sdk/python/examples/control.py can can0 --interface socketcan --node 1
python sdk/python/examples/control.py can PCAN_USBBUS1 --interface pcan --node 1
```

```python
import can
from lyo_d42 import Client, CanTransport

bus = can.Bus(interface='socketcan', channel='can0', bitrate=500000)
with Client(CanTransport(bus, node=1)) as robot:
    print(robot.status())
    # robot.drive_for(0.03, 0, 0.2, 1)
```

python-can also supports Vector and SLCAN when the appropriate backend/driver is available. Device channel names and driver installation depend on the adapter. The SDK owns and closes the supplied bus; use a dedicated bus instance.

## Rust SDK

```sh
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- can can0
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- slcan COM9
```

Native SocketCan is Linux-only. Slcan works over a serial adapter on supported desktop OSes, uses115200 serial baud by default in the CLI, and sends C/S6/O to close/configure500kbit/s/open. This requires LAWICEL-compatible firmware, not an arbitrary vendor binary protocol. Adapt other hardware through the CanBus trait. Examples read state unless --drive is supplied.

## Encode control and heartbeat

Full op0–12 table, byte scales, bounds and feedback layout: [CAN command set](api.md#complete-chassis-can-command-set).

```text
ID = 0x18EF0000 | node<<8 | op
op bit7 = reliable FIFO
bytes0..1 = uint16 sequence, little endian
```

Node1/seq42, v0.1m/s,w0,a0.2m/s², soft stop: ID18EF0184 and `2A 00 64 00 00 00 14 81`. This is a motion command; do not send it merely to check wiring. op2–6 byte7bit7 selects physical acceleration, byte6 is acceleration×100, byte7bit0 is soft stop. CAN acceleration range0.01–2.54m/s²; JSON supports up to5. CAN fields quantize distance to mm and angle to0.01degrees.

While disabled, set3s timeout with ID18EF018C, data `2B 00 B8 0B 00 00 00 00`. For heartbeat use op10 (e.g.18EF018A) with a new sequence and zero remaining bytes. Default timeout3s; use a bounded control loop, typically100ms. Position tasks/soft stop/hold need updates too. Queries do not renew it. Do not keep stale navigation targets alive using an independent heartbeat thread.

A hard stop uses op0 and zero payload after sequence. ACK only means received; confirm state and actual stopping. The SDK does not retry uncertain motion. CAN status omits temperatures, tilt and full JSON state; use USB/HTTP/WS when those fields are needed. current_raw is not amperes and current_valid must be checked.
