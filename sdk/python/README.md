# Lyo-D42 Python SDK 0.4.0

Python 3.10+. Install locally (no package registry release is implied):

```sh
python -m pip install './sdk/python[all]'
python sdk/python/examples/control.py usb COM7
python sdk/python/examples/control.py uart COM8
python sdk/python/examples/control.py http http://192.168.4.1
python sdk/python/examples/control.py ws ws://192.168.4.1/ws
python sdk/python/examples/control.py can can0 --interface socketcan
python sdk/python/examples/control.py can PCAN_USBBUS1 --interface pcan
```

The CLI only reads state unless `--drive` is given. `--drive` requests 0.03 m/s for one second and then T800. Support includes USB CDC, UART0 (1 Mbps, enables UART replies), HTTP, WebSocket and python-can backends (SocketCAN/PCAN/Vector/SLCAN according to installed drivers). Do not connect a raw MCU pin directly to CAN-H/L; use a CAN interface/transceiver.

```python
from lyo_d42 import Client, SerialTransport
with Client(SerialTransport('COM7')) as robot:
    print(robot.status())
    # Explicit opt-in to bounded motion:
    # robot.drive_for(0.03, 0, 0.2, 1.0)
```

Methods: `status()`, `twist(v,w,accel_mps2,soft_stop=True)`, `wheels(left,right,accel_mps2,soft_stop=True)`, `move(distance,speed,accel_mps2)`, `turn(degrees,speed,accel_mps2)`, `heartbeat()`, `hill_hold(bool)`, `light(0..1)`, `set_timeout(ms)`, `stop()` and low-level `request(dict)`.

Wheel speed is limited to 0.32 m/s, acceleration to 0.01–5 m/s². CAN acceleration is quantized to 0.01 m/s² and limited to 2.54; distance is limited by an int16 millimeter field, and angle by int16 centidegrees. `request` exposes generic JSON commands on JSON transports; CAN supports T800–811, with T809 limited to timeout configuration in this SDK. Other configuration is available through HTTP/USB/UART/WebSocket.

There is no background heartbeat. Call motion/heartbeat at a period shorter than the configured firmware timeout (default 3 s; examples use 100 ms). Position moves and soft stopping also require heartbeat. `drive_for` ends with a hard stop, not a soft stop. For a soft stop, send `twist(0,0,accel,True)` and continue heartbeat until stopped. `stop` confirms transmission, not mechanical stopping. `close` closes the transport and does not stop another controller's motion. Only one controller should own a chassis.

Serial/WS requests match rid. HTTP 202 means queued and the SDK polls the shared result slot. CAN state is assembled from ACK plus three feedback frames; it has less detail than JSON (no tilt, temperatures, config or state name). Firmware 0.4.0 is required for the current-feedback frame. Error/timeout raises; no automatic retry. Current feedback is raw, with a validity flag, not calibrated amperes.

Tests: `python -m unittest discover -s sdk/python/tests -v` with `PYTHONPATH=sdk/python`.
