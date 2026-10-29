//! Explicit control; no automatic motion retries or background heartbeat.
use serde_json::{json, Value};
use std::io::{Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

pub type Error = Box<dyn std::error::Error + Send + Sync>;
pub type Result<T> = std::result::Result<T, Error>;
pub const MAX_SPEED: f64 = 0.32;
pub const TRACK: f64 = 0.159;

fn finite(x: f64, lo: f64, hi: f64) -> Result<()> {
    if !x.is_finite() || x < lo || x > hi {
        return Err("value outside supported range".into());
    }
    Ok(())
}
fn number(c: &Value, key: &str) -> Result<f64> {
    c[key]
        .as_f64()
        .ok_or_else(|| format!("missing numeric field {key}").into())
}
fn encoded(c: &Value) -> Result<String> {
    let s = serde_json::to_string(c)?;
    if s.len() > 255 {
        return Err("command exceeds 255 bytes".into());
    }
    Ok(s)
}
fn checked(v: Value) -> Result<Value> {
    if v["ok"] != true {
        return Err(format!("device rejected command: {v}").into());
    }
    Ok(v)
}

pub trait Transport {
    fn send(&mut self, command: &Value) -> Result<()>;
    fn exchange(&mut self, command: &Value, timeout: Duration) -> Result<Value>;
}

pub struct SerialTransport {
    port: Box<dyn serialport::SerialPort>,
}
impl SerialTransport {
    /// USB CDC ignores baud timing; UART0 requires 1_000_000 baud and UART replies.
    pub fn open(path: &str, baud: u32, uart: bool) -> Result<Self> {
        let mut result = Self {
            port: serialport::new(path, baud)
                .timeout(Duration::from_millis(50))
                .open()?,
        };
        if uart {
            result.send(&json!({"T":604,"echo":0,"uart":1,"usb":1}))?;
        }
        Ok(result)
    }
}
impl Transport for SerialTransport {
    fn send(&mut self, c: &Value) -> Result<()> {
        self.port
            .write_all(format!("{}\n", encoded(c)?).as_bytes())?;
        Ok(())
    }
    fn exchange(&mut self, c: &Value, timeout: Duration) -> Result<Value> {
        self.send(c)?;
        let end = Instant::now() + timeout;
        let mut line = Vec::new();
        let mut dropping = false;
        while Instant::now() < end {
            let mut b = [0u8];
            match self.port.read(&mut b) {
                Ok(0) => continue,
                Err(e) if e.kind() == std::io::ErrorKind::TimedOut => continue,
                Err(e) => return Err(e.into()),
                Ok(_) => {}
            }
            if b[0] == b'\n' {
                let parsed = if dropping {
                    None
                } else {
                    serde_json::from_slice::<Value>(&line).ok()
                };
                line.clear();
                dropping = false;
                if let Some(v) = parsed {
                    if v["rid"] == c["rid"] {
                        return checked(v);
                    }
                }
            } else if !dropping {
                line.push(b[0]);
                if line.len() > 8192 {
                    line.clear();
                    dropping = true;
                }
            }
        }
        Err("serial result timed out; command was not retried".into())
    }
}

pub struct HttpTransport {
    base: String,
    agent: ureq::Agent,
}
impl HttpTransport {
    pub fn new(base: &str) -> Self {
        Self {
            base: base.trim_end_matches('/').to_owned(),
            agent: ureq::AgentBuilder::new()
                .timeout(Duration::from_secs(1))
                .build(),
        }
    }
}
impl Transport for HttpTransport {
    fn send(&mut self, c: &Value) -> Result<()> {
        let r = self
            .agent
            .post(&format!("{}/api/cmd", self.base))
            .set("Content-Type", "application/json")
            .send_string(&encoded(c)?)?;
        if r.status() != 202 {
            return Err("expected HTTP 202".into());
        }
        Ok(())
    }
    fn exchange(&mut self, c: &Value, timeout: Duration) -> Result<Value> {
        if c["T"] == 808 {
            return checked(
                self.agent
                    .get(&format!("{}/api/chassis", self.base))
                    .timeout(timeout)
                    .call()?
                    .into_json()?,
            );
        }
        let end = Instant::now() + timeout;
        self.send(c)?;
        while Instant::now() < end {
            let v: Value = self
                .agent
                .get(&format!("{}/api/result", self.base))
                .timeout(end.saturating_duration_since(Instant::now()))
                .call()?
                .into_json()?;
            if v["rid"] == c["rid"] {
                return checked(v);
            }
            std::thread::sleep(Duration::from_millis(20));
        }
        Err("HTTP execution result timed out; 202 only means accepted".into())
    }
}

pub struct WebSocketTransport {
    socket: tungstenite::WebSocket<TcpStream>,
}
impl WebSocketTransport {
    /// The device exposes ws://; TLS termination belongs to an external gateway.
    pub fn open(address: &str, timeout: Duration) -> Result<Self> {
        let u = url::Url::parse(address)?;
        if u.scheme() != "ws" {
            return Err("device WebSocket URL must use ws://".into());
        }
        let host = u.host_str().ok_or("missing host")?;
        let target = (host, u.port().unwrap_or(80))
            .to_socket_addrs()?
            .next()
            .ok_or("no address")?;
        let stream = TcpStream::connect_timeout(&target, timeout)?;
        stream.set_read_timeout(Some(timeout))?;
        stream.set_write_timeout(Some(timeout))?;
        let (socket, _) = tungstenite::client(address, stream)?;
        Ok(Self { socket })
    }
}
impl Transport for WebSocketTransport {
    fn send(&mut self, c: &Value) -> Result<()> {
        self.socket.send(tungstenite::Message::Text(encoded(c)?))?;
        Ok(())
    }
    fn exchange(&mut self, c: &Value, timeout: Duration) -> Result<Value> {
        self.send(c)?;
        let end = Instant::now() + timeout;
        while Instant::now() < end {
            self.socket
                .get_mut()
                .set_read_timeout(Some(end.saturating_duration_since(Instant::now())))?;
            match self.socket.read()? {
                tungstenite::Message::Text(text) if text.len() <= 8192 => {
                    if let Ok(v) = serde_json::from_str::<Value>(&text) {
                        if v["rid"] == c["rid"] {
                            return checked(v);
                        }
                    }
                }
                tungstenite::Message::Close(_) => return Err("WebSocket closed".into()),
                _ => {}
            }
        }
        Err("WebSocket result timed out".into())
    }
}

#[derive(Clone, Debug)]
pub struct CanFrame {
    pub id: u32,
    pub data: Vec<u8>,
}
pub trait CanBus {
    fn send(&mut self, frame: &CanFrame) -> Result<()>;
    /// Only extended, non-RTR, non-error classic CAN frames may be returned.
    fn receive(&mut self, timeout: Duration) -> Result<Option<CanFrame>>;
}

pub fn can_encode(c: &Value, node: u8) -> Result<CanFrame> {
    if !(1..=127).contains(&node) {
        return Err("CAN node must be 1..127".into());
    }
    let t = c["T"].as_u64().ok_or("missing T")?;
    let mut op = t.checked_sub(800).ok_or("unsupported CAN command")? as u32;
    let mut d = vec![0u8; 8];
    d[..2].copy_from_slice(&(c["rid"].as_u64().unwrap_or(0) as u16).to_le_bytes());
    match t {
        800 | 808 | 810 => {}
        801 | 811 => {
            d[2] = c[if t == 801 { "enable" } else { "hold" }]
                .as_bool()
                .ok_or("boolean required")? as u8;
        }
        802..=806 => {
            let (a, b) = match t {
                802 => (number(c, "left")? * 100., number(c, "right")? * 100.),
                803 => (number(c, "left")? * 1000., number(c, "right")? * 1000.),
                804 => (number(c, "v")? * 1000., number(c, "w")? * 1000.),
                805 => (number(c, "distance")? * 1000., number(c, "speed")? * 1000.),
                _ => (number(c, "degrees")? * 100., number(c, "speed")? * 1000.),
            };
            finite(a.round(), -32768., 32767.)?;
            finite(b.round(), -32768., 32767.)?;
            d[2..4].copy_from_slice(&(a.round() as i16).to_le_bytes());
            d[4..6].copy_from_slice(&(b.round() as i16).to_le_bytes());
            let accel = number(c, "accel_mps2")?;
            finite(accel, 0.01, 2.54)?;
            d[6] = (accel * 100.).round() as u8;
            d[7] = 0x80 | c["soft_stop"].as_bool().unwrap_or(true) as u8;
        }
        807 => {
            let v = number(c, "brightness")?;
            finite(v, 0., 1.)?;
            d[2..4].copy_from_slice(&((v * 1000.).round() as u16).to_le_bytes());
        }
        809 => {
            for key in c.as_object().ok_or("object required")?.keys() {
                if !["T", "rid", "reliable", "timeout_ms"].contains(&key.as_str()) {
                    return Err("CAN configuration supports timeout_ms only".into());
                }
            }
            let ms = c["timeout_ms"].as_u64().ok_or("integer timeout required")?;
            if !(100..=60000).contains(&ms) {
                return Err("timeout out of range".into());
            }
            op = 12;
            d[2..4].copy_from_slice(&(ms as u16).to_le_bytes());
        }
        _ => return Err("unsupported CAN command".into()),
    }
    Ok(CanFrame {
        id: 0x18EF0000
            | (node as u32) << 8
            | op
            | if c["reliable"].as_bool().unwrap_or(true) {
                0x80
            } else {
                0
            },
        data: d,
    })
}

pub struct CanTransport<B: CanBus> {
    pub bus: B,
    node: u8,
}
impl<B: CanBus> CanTransport<B> {
    pub fn new(bus: B, node: u8) -> Result<Self> {
        can_encode(&json!({"T":808}), node)?;
        Ok(Self { bus, node })
    }
}
impl<B: CanBus> Transport for CanTransport<B> {
    fn send(&mut self, c: &Value) -> Result<()> {
        self.bus.send(&can_encode(c, self.node)?)
    }
    fn exchange(&mut self, c: &Value, timeout: Duration) -> Result<Value> {
        let frame = can_encode(c, self.node)?;
        let op = frame.id & 0x7f;
        self.bus.send(&frame)?;
        let seq = c["rid"].as_u64().unwrap_or(0) as u16;
        let end = Instant::now() + timeout;
        let mut ack = false;
        let mut parts: [Option<Vec<u8>>; 3] = [None, None, None];
        while Instant::now() < end {
            let Some(f) = self
                .bus
                .receive(end.saturating_duration_since(Instant::now()))?
            else {
                break;
            };
            if f.data.len() < 3 || u16::from_le_bytes([f.data[0], f.data[1]]) != seq {
                continue;
            }
            if f.id == (0x18EE0000 | (self.node as u32) << 8 | op) {
                if f.data[2] != 0 {
                    return Err("CAN command rejected".into());
                }
                ack = true;
            } else if c["T"] == 808 && f.data.len() == 8 {
                for (i, part) in parts.iter_mut().enumerate() {
                    if f.id == (0x18ED0000 | (self.node as u32) << 8 | i as u32) {
                        *part = Some(f.data.clone());
                    }
                }
            }
            if ack && c["T"] != 808 {
                return Ok(json!({"T":c["T"],"ok":true}));
            }
            if ack && parts.iter().all(Option::is_some) {
                let (a, b, d) = (
                    parts[0].as_ref().unwrap(),
                    parts[1].as_ref().unwrap(),
                    parts[2].as_ref().unwrap(),
                );
                let wheels:Vec<_>=(0..2).map(|i|json!({"id":i+1,"valid":a[4+i]!=0,"mps":i16::from_le_bytes([b[2+2*i],b[3+2*i]]) as f64/1000.,"status":b[6+i],"current_raw":i16::from_le_bytes([d[2+2*i],d[3+2*i]]),"current_valid":d[6+i]!=0,"voltage":if i==0{Some(a[6] as f64/10.)}else{None}})).collect();
                return Ok(
                    json!({"T":808,"ok":true,"ready":a[2]!=0,"enabled":a[3]!=0,"led":a[7] as f64/100.,"wheels":wheels}),
                );
            }
        }
        Err("CAN result incomplete; no retry was sent".into())
    }
}

/// Cross-platform LAWICEL/SLCAN USB adapter. S6 selects 500 kbit/s.
pub struct Slcan {
    port: Box<dyn serialport::SerialPort>,
    line: Vec<u8>,
}
impl Slcan {
    pub fn open(port: &str, baud: u32) -> Result<Self> {
        let mut p = serialport::new(port, baud)
            .timeout(Duration::from_millis(20))
            .open()?;
        p.write_all(b"C\rS6\rO\r")?;
        Ok(Self {
            port: p,
            line: Vec::new(),
        })
    }
}
impl CanBus for Slcan {
    fn send(&mut self, f: &CanFrame) -> Result<()> {
        if f.id > 0x1fffffff || f.data.len() > 8 {
            return Err("invalid CAN frame".into());
        }
        let mut s = format!("T{:08X}{:X}", f.id, f.data.len());
        for b in &f.data {
            s.push_str(&format!("{b:02X}"));
        }
        s.push('\r');
        self.port.write_all(s.as_bytes())?;
        Ok(())
    }
    fn receive(&mut self, timeout: Duration) -> Result<Option<CanFrame>> {
        let end = Instant::now() + timeout;
        while Instant::now() < end {
            let mut b = [0];
            match self.port.read(&mut b) {
                Ok(0) => continue,
                Err(e) if e.kind() == std::io::ErrorKind::TimedOut => continue,
                Err(e) => return Err(e.into()),
                Ok(_) => {}
            }
            if b[0] == 7 {
                return Err("SLCAN adapter reported an error".into());
            }
            if b[0] == b'\r' {
                let s = String::from_utf8_lossy(&self.line).to_string();
                self.line.clear();
                if s.is_ascii() && s.starts_with('T') && s.len() >= 10 {
                    if let (Ok(id), Ok(n)) = (
                        u32::from_str_radix(&s[1..9], 16),
                        usize::from_str_radix(&s[9..10], 16),
                    ) {
                        if n <= 8 && s.len() >= 10 + n * 2 {
                            let data = (0..n)
                                .map(|i| u8::from_str_radix(&s[10 + i * 2..12 + i * 2], 16))
                                .collect::<std::result::Result<Vec<_>, _>>();
                            if let Ok(data) = data {
                                return Ok(Some(CanFrame { id, data }));
                            }
                        }
                    }
                }
            } else if self.line.len() < 128 {
                self.line.push(b[0]);
            } else {
                self.line.clear();
            }
        }
        Ok(None)
    }
}

#[cfg(target_os = "linux")]
pub struct SocketCan(socketcan::CanSocket);
#[cfg(target_os = "linux")]
impl SocketCan {
    pub fn open(interface: &str) -> Result<Self> {
        use socketcan::Socket;
        Ok(Self(socketcan::CanSocket::open(interface)?))
    }
}
#[cfg(target_os = "linux")]
impl CanBus for SocketCan {
    fn send(&mut self, f: &CanFrame) -> Result<()> {
        use socketcan::{EmbeddedFrame, Socket};
        let id = socketcan::ExtendedId::new(f.id).ok_or("invalid ID")?;
        let frame = socketcan::CanFrame::new(id, &f.data).ok_or("invalid frame")?;
        self.0.set_write_timeout(Duration::from_millis(500))?;
        self.0.write_frame(&frame)?;
        Ok(())
    }
    fn receive(&mut self, timeout: Duration) -> Result<Option<CanFrame>> {
        use socketcan::{EmbeddedFrame, Frame, Socket};
        let end = Instant::now() + timeout;
        while Instant::now() < end {
            self.0
                .set_read_timeout(end.saturating_duration_since(Instant::now()))?;
            match self.0.read_frame() {
                Ok(f) if f.is_extended() && !f.is_remote_frame() && !f.is_error_frame() => {
                    return Ok(Some(CanFrame {
                        id: f.raw_id(),
                        data: f.data().to_vec(),
                    }))
                }
                Ok(_) => continue,
                Err(e)
                    if [std::io::ErrorKind::TimedOut, std::io::ErrorKind::WouldBlock]
                        .contains(&e.kind()) =>
                {
                    return Ok(None)
                }
                Err(e) => return Err(e.into()),
            }
        }
        Ok(None)
    }
}

pub struct Client<T: Transport> {
    pub transport: T,
    seq: u32,
    pub timeout: Duration,
}
impl<T: Transport> Client<T> {
    pub fn new(transport: T) -> Self {
        Self {
            transport,
            seq: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap_or_default()
                .subsec_nanos(),
            timeout: Duration::from_secs(1),
        }
    }
    pub fn request(&mut self, mut c: Value) -> Result<Value> {
        if !c.is_object() {
            return Err("command must be an object".into());
        }
        self.seq = self.seq.wrapping_add(1);
        c["rid"] = json!(self.seq);
        c["reliable"] = json!(true);
        self.transport.exchange(&c, self.timeout)
    }
    pub fn status(&mut self) -> Result<Value> {
        self.request(json!({"T":808}))
    }
    pub fn heartbeat(&mut self) -> Result<Value> {
        self.request(json!({"T":810}))
    }
    pub fn stop(&mut self) -> Result<()> {
        self.transport.send(&json!({"T":800}))
    }
    pub fn twist(&mut self, v: f64, w: f64, accel: f64, soft_stop: bool) -> Result<Value> {
        finite(v, -MAX_SPEED, MAX_SPEED)?;
        finite(w, -2. * MAX_SPEED / TRACK, 2. * MAX_SPEED / TRACK)?;
        finite(accel, 0.01, 5.)?;
        if (v - w * TRACK / 2.).abs().max((v + w * TRACK / 2.).abs()) > MAX_SPEED + 1e-9 {
            return Err("combined wheel speed exceeds 0.32 m/s".into());
        }
        self.request(json!({"T":804,"v":v,"w":w,"accel_mps2":accel,"soft_stop":soft_stop}))
    }
    pub fn wheels(&mut self, left: f64, right: f64, accel: f64, soft_stop: bool) -> Result<Value> {
        finite(left, -MAX_SPEED, MAX_SPEED)?;
        finite(right, -MAX_SPEED, MAX_SPEED)?;
        finite(accel, 0.01, 5.)?;
        self.request(
            json!({"T":803,"left":left,"right":right,"accel_mps2":accel,"soft_stop":soft_stop}),
        )
    }
    pub fn move_distance(&mut self, distance: f64, speed: f64, accel: f64) -> Result<Value> {
        finite(distance, -100., 100.)?;
        finite(speed, 0.005, MAX_SPEED)?;
        finite(accel, 0.01, 5.)?;
        self.request(json!({"T":805,"distance":distance,"speed":speed,"accel_mps2":accel}))
    }
    pub fn turn(&mut self, degrees: f64, speed: f64, accel: f64) -> Result<Value> {
        finite(degrees, -36000., 36000.)?;
        finite(speed, 0.005, MAX_SPEED)?;
        finite(accel, 0.01, 5.)?;
        self.request(json!({"T":806,"degrees":degrees,"speed":speed,"accel_mps2":accel}))
    }
    pub fn hill_hold(&mut self, hold: bool) -> Result<Value> {
        self.request(json!({"T":811,"hold":hold}))
    }
    pub fn light(&mut self, brightness: f64) -> Result<Value> {
        finite(brightness, 0., 1.)?;
        self.request(json!({"T":807,"brightness":brightness}))
    }
    pub fn set_timeout(&mut self, ms: u16) -> Result<Value> {
        if !(100..=60000).contains(&ms) {
            return Err("timeout must be 100..60000 ms".into());
        }
        self.request(json!({"T":809,"timeout_ms":ms}))
    }
    /// Always requests a hard stop on normal completion or an operation error.
    pub fn drive_for(&mut self, v: f64, w: f64, accel: f64, duration: Duration) -> Result<()> {
        if duration.is_zero() || duration > Duration::from_secs(60) {
            return Err("duration must be 0..60 seconds".into());
        }
        let end = Instant::now() + duration;
        let result: Result<()> = (|| {
            while Instant::now() < end {
                self.twist(v, w, accel, true)?;
                std::thread::sleep(
                    Duration::from_millis(100).min(end.saturating_duration_since(Instant::now())),
                );
            }
            Ok(())
        })();
        let stopped = self.stop();
        result.and(stopped)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    struct Fake(Vec<Value>);
    impl Transport for Fake {
        fn send(&mut self, c: &Value) -> Result<()> {
            self.0.push(c.clone());
            Ok(())
        }
        fn exchange(&mut self, c: &Value, _: Duration) -> Result<Value> {
            self.send(c)?;
            Ok(json!({"ok":true}))
        }
    }
    #[test]
    fn speed_validation() {
        let mut c = Client::new(Fake(vec![]));
        assert!(c.twist(0.321, 0., 0.2, true).is_err());
        assert!(c.twist(0.32, 1., 0.2, true).is_err());
        assert!(c.twist(f64::NAN, 0., 0.2, true).is_err());
        assert!(c.twist(0.1, 0., 0.2, true).is_ok());
        assert_eq!(c.transport.0.len(), 1);
    }
    #[test]
    fn can_bytes() {
        let f = can_encode(
            &json!({"T":804,"v":0.1,"w":0,"accel_mps2":0.2,"rid":42,"soft_stop":true}),
            1,
        )
        .unwrap();
        assert_eq!(f.id, 0x18EF0184);
        assert_eq!(f.data, vec![42, 0, 100, 0, 0, 0, 20, 129]);
        assert!(can_encode(&json!({"T":804,"v":0.1,"w":0,"accel_mps2":5}), 1).is_err());
    }
    struct Bus(std::collections::VecDeque<CanFrame>);
    impl CanBus for Bus {
        fn send(&mut self, _: &CanFrame) -> Result<()> {
            Ok(())
        }
        fn receive(&mut self, _: Duration) -> Result<Option<CanFrame>> {
            Ok(self.0.pop_front())
        }
    }
    #[test]
    fn can_status_out_of_order() {
        let frames = vec![
            CanFrame {
                id: 0x18ED0102,
                data: vec![42, 0, 123, 0, 255, 255, 1, 1],
            },
            CanFrame {
                id: 0x18EE0108,
                data: vec![41, 0, 1],
            },
            CanFrame {
                id: 0x18EE0208,
                data: vec![42, 0, 1],
            },
            CanFrame {
                id: 0x18EE0108,
                data: vec![42, 0, 0],
            },
            CanFrame {
                id: 0x18ED0101,
                data: vec![42, 0, 100, 0, 156, 255, 0, 0],
            },
            CanFrame {
                id: 0x18ED0100,
                data: vec![42, 0, 1, 1, 1, 1, 200, 0],
            },
        ];
        let mut transport = CanTransport::new(Bus(frames.into()), 1).unwrap();
        let state = transport
            .exchange(&json!({"T":808,"rid":42}), Duration::from_secs(1))
            .unwrap();
        assert_eq!(state["wheels"][0]["current_raw"], 123);
        assert_eq!(state["wheels"][1]["current_raw"], -1);
        assert_eq!(state["wheels"][1]["mps"], -0.1);
        assert!(transport
            .exchange(&json!({"T":808,"rid":43}), Duration::from_secs(1))
            .is_err());
    }
    #[test]
    fn can_timeout_and_negative_encoding() {
        let f = can_encode(&json!({"T":809,"timeout_ms":3000,"rid":43}), 1).unwrap();
        assert_eq!(f.id, 0x18EF018C);
        assert_eq!(&f.data[2..4], &[184, 11]);
        let f = can_encode(
            &json!({"T":803,"left":-0.1,"right":0.1,"accel_mps2":0.2}),
            1,
        )
        .unwrap();
        assert_eq!(&f.data[2..6], &[156, 255, 100, 0]);
        assert!(can_encode(&json!({"T":809,"timeout_ms":3000,"max_speed":0.3}), 1).is_err());
    }
    #[test]
    fn helper_stops_after_transport_error() {
        struct Failing(Vec<Value>);
        impl Transport for Failing {
            fn send(&mut self, c: &Value) -> Result<()> {
                self.0.push(c.clone());
                Ok(())
            }
            fn exchange(&mut self, _: &Value, _: Duration) -> Result<Value> {
                Err("disconnected".into())
            }
        }
        let mut c = Client::new(Failing(vec![]));
        assert!(c
            .drive_for(0.03, 0., 0.2, Duration::from_millis(100))
            .is_err());
        assert_eq!(c.transport.0.last().unwrap()["T"], 800);
    }
}
