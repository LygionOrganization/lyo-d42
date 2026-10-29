# USB CDC and UART host control

## Select the correct port

The native **Type-C USB** connector carries ESP32-S3 USB CDC JSON. **Type-C UART** connects through a USB/UART bridge to UART0 and is also used for downloads. These are separate from the1Mbps UART1 motor bus. Use a data-capable cable and compare the OS port list before/after connecting. Close monitors that exclusively hold the port.

```sh
python -m pip install pyserial
python -m serial.tools.list_ports -v
```

On Windows expect COM ports; Linux commonly uses /dev/ttyACM* for native CDC and /dev/ttyUSB* for a bridge; macOS uses /dev/cu.*. Names vary; verify rather than copying an example. Linux users need OS permission to open the selected device.

USB CDC does not use a physical UART baud clock; SDK examples select115200. UART0 uses1000000baud,8N1, no flow control. In UART mode enable replies first with T604; SDK UART mode does this automatically. Never connect a host UART directly to the motor TTL bus.

## Read-only SDK examples

Download the source or standalone SDK from [Host SDKs](host.md). From the source root:

```powershell
python -m pip install "./sdk/python[all]"
python sdk/python/examples/control.py usb COM7
python sdk/python/examples/control.py uart COM8
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- usb COM7
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- uart COM8
```

Standalone Python ZIP: install `.[all]` in its root and use examples/control.py. Standalone Rust ZIP: run cargo from its Cargo.toml directory.

## Minimal raw USB query

```python
import json
import time
import serial

with serial.Serial('COM7', 115200, timeout=0.1, write_timeout=0.5) as port:
    port.write(b'{"T":808,"rid":42,"reliable":true}\n')
    deadline = time.monotonic() + 2
    buffer = bytearray()
    while time.monotonic() < deadline:
        buffer.extend(port.read(1))
        if len(buffer) > 8192:
            raise RuntimeError('Reply too large')
        if buffer.endswith(b'\n'):
            try:
                reply = json.loads(buffer)
            except (ValueError, UnicodeError):
                reply = None
            buffer.clear()
            if isinstance(reply, dict) and reply.get('rid') == 42:
                print(reply)
                break
    else:
        raise TimeoutError('No matching reply')
```

For UART change baud to1000000 and send `{"T":604,"echo":0,"uart":1,"usb":1}` plus newline before the query. Keep packets below255bytes excluding newline. Log lines and unrelated results may be interleaved; correlate rid and check ok.

## Explicit motion

Only after verifying feedback and raising the wheels, add `--drive` to run0.03m/s for1second then request T800. SDK drive_for uses explicit acceleration0.2m/s² and a100ms command period. It does not run a background heartbeat. No query or status monitor renews motion. For your application send a target plus accel_mps2 each cycle; send a zero target with soft_stop=true to decelerate and continue heartbeat until stopped. T800 always requests immediate disable and has no JSON execution ACK.

No response: check connector, cable, port ownership, UART baud/reply setting, newline and rid. A reset/re-enumeration can change the native port. Reopen and query state before issuing new motion. The SDK never blindly retries a position move.
