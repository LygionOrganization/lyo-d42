# Host development with Python and Rust

Firmware0.4.0 supports USB CDC, UART0, HTTP, WebSocket and CAN. SDKs use explicit acceleration in m/s² and enforce a0.32m/s wheel cap. They never start movement or an independent background heartbeat on connection.

## Downloads

- [Complete source, SDKs and examples](assets/Lyo-D42-0.4.0-source.zip)
- [Python SDK](assets/Lyo-D42-0.4.0-python-sdk.zip)
- [Rust SDK](assets/Lyo-D42-0.4.0-rust-sdk.zip)
- [Firmware candidate](assets/Lyo-D42-0.4.0-firmware.zip)
- [Build and SHA256 manifest](assets/manifest.json)
- [SHA256SUMS](assets/SHA256SUMS.txt)

These are release candidates, not hardware qualification. See docs/RELEASE_VALIDATION.md in the source. Firmware offsets are in the package; never flash application-only firmware.bin at0x0.

## Start with state queries

From the full source root:

```sh
python -m pip install './sdk/python[all]'
python sdk/python/examples/control.py usb COM7
python sdk/python/examples/control.py http http://192.168.4.1
python sdk/python/examples/control.py ws ws://192.168.4.1/ws
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- http http://192.168.4.1
```

Standalone Python ZIP: install `.[all]` in its root and run examples/control.py. Standalone Rust ZIP: use its Cargo.toml. Rust requires a Rust toolchain and the platform linker/C++ build tools. Both SDKs include English README, comments and AGENTS.md.

```python
from lyo_d42 import Client, HttpTransport

with Client(HttpTransport('http://192.168.4.1')) as robot:
    print(robot.status())
    # Explicit opt-in only:
    # robot.drive_for(0.03, 0, 0.2, 1)
```

Add --drive to the CLI only to request a bounded1second0.03m/s demo. drive_for requests a hard stop on completion/error; it is not a soft-stop example.

|Interface|Python|Rust|Guide|
|---|---|---|---|
|USB/UART|SerialTransport|SerialTransport|[USB/UART](usb-cdc.md)|
|HTTP|HttpTransport|HttpTransport|POST/api/cmd, then rid-based result polling|
|WebSocket|WebSocketTransport|WebSocketTransport, ws://|ws://192.168.4.1/ws|
|CAN|python-can backends|Linux SocketCAN, SLCAN, custom CanBus|[CAN](can.md)|

## Lifecycle

Both clients offer status, twist, wheels, relative distance, turn, heartbeat, hill_hold, light, set_timeout, stop and request. Python distance method is move; Rust uses move_distance. Pass acceleration with every target. CAN encoding supports less acceleration/range and fewer feedback fields; see the [protocol reference](api.md).

HTTP202 means queued, not executed. Its result slot is shared, so one writer should own the chassis. Serial and WS match rid; CAN matches node/sequence and all required fragments. Exceptions are not automatically retried. stop confirms sending, not mechanical stopping. Closing a read-only client does not stop another controller.

Default watchdog is3s. Position tasks, deceleration and hold need updates. Tie output updates to fresh application input; if navigation stops updating, send stop rather than keeping the last target alive forever. In ROS2, T804 v/w correspond to Twist.linear.x/angular.z. Combined wheel targets beyond0.32 are rejected by the SDK; scale both wheels proportionally before calling wheels if needed. No fused odometry or full ROS2 node is implied.

Current is optional raw register data. Check current_valid and do not label it calibrated amperes.
