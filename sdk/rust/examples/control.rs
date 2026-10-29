use lyo_d42::*;
use std::time::Duration;
fn run<T: Transport>(link: T, drive: bool) -> Result<()> {
    let mut robot = Client::new(link);
    println!("{}", robot.status()?);
    if drive {
        robot.drive_for(0.03, 0.0, 0.2, Duration::from_secs(1))?;
    }
    Ok(())
}
fn main() -> Result<()> {
    let args: Vec<_> = std::env::args().collect();
    if args.len() < 3 {
        return Err("usage: control usb|uart|http|ws|slcan|can ADDRESS [--drive]".into());
    }
    let drive = args.iter().any(|x| x == "--drive");
    match args[1].as_str() {
        "usb" => run(SerialTransport::open(&args[2], 115200, false)?, drive),
        "uart" => run(SerialTransport::open(&args[2], 1_000_000, true)?, drive),
        "http" => run(HttpTransport::new(&args[2]), drive),
        "ws" => run(
            WebSocketTransport::open(&args[2], Duration::from_secs(1))?,
            drive,
        ),
        "slcan" => run(CanTransport::new(Slcan::open(&args[2], 115200)?, 1)?, drive),
        #[cfg(target_os = "linux")]
        "can" => run(CanTransport::new(SocketCan::open(&args[2])?, 1)?, drive),
        _ => Err("unsupported transport on this platform".into()),
    }
}
