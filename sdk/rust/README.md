# Lyo-D42 Rust SDK 0.4.0

Build and test with `cargo test --manifest-path sdk/rust/Cargo.toml`. Use a path dependency:

```toml
[dependencies]
lyo-d42 = { path = "../Lyo-D42/sdk/rust" }
```

```rust,no_run
use lyo_d42::{Client, SerialTransport, Result};
fn main() -> Result<()> {
    let mut robot = Client::new(SerialTransport::open("COM7", 115200, false)?);
    println!("{}", robot.status()?);
    Ok(())
}
```

Examples (status only by default):

```sh
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- usb COM7
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- uart COM8
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- http http://192.168.4.1
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- ws ws://192.168.4.1/ws
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- slcan COM9
cargo run --manifest-path sdk/rust/Cargo.toml --example control -- can can0
```

`can` uses Linux SocketCAN; `slcan` uses a LAWICEL-compatible adapter on Windows/Linux/macOS and configures CAN at 500 kbit/s (serial link 115200 in the example). Vendor binary USB-CAN adapters are not SLCAN; implement `CanBus` for their vendor API, or use Python's python-can adapter support. `--drive` explicitly moves at 0.03 m/s for one second, then stops.

Public methods: `status`, `twist`, `wheels`, `move_distance`, `turn`, `heartbeat`, `hill_hold`, `light`, `set_timeout`, `stop`, `drive_for`, `request`. Speed/acceleration units and CAN limitations match the Python SDK README. Speed is capped at 0.32 m/s; acceleration is explicit in m/s². Firmware 0.4.0 is required. The generic transport and CanBus traits allow integration with existing application I/O.

No independent heartbeat or automatic motion retry. Continue motion/heartbeat during position moves and soft stops. `drive_for` requests a hard stop on normal completion and operation errors; process termination/panic cannot guarantee a sent stop, so the firmware watchdog remains required. JSON T800 has no execution acknowledgement. `drop` only releases I/O resources. HTTP results share one firmware result slot; use one owner. Current feedback has raw units, not calibrated amperes.

Windows builds need Visual Studio C++ Build Tools. Cargo.lock pins tested dependencies; Linux SocketCAN should also be checked on Linux before release. SDK source and comments are English. See AGENTS.md for contribution rules.

The default build supports the device HTTP endpoint without TLS dependencies. Enable the optional `tls` Cargo feature for an HTTPS gateway. WebSocket remains `ws://`. Cargo.lock is included for reproducible example/test dependencies; use `--locked`.
