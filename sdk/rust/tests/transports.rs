//! Local protocol fixtures only; these tests never connect to a chassis.
use lyo_d42::{HttpTransport, Transport, WebSocketTransport};
use serde_json::{json, Value};
use std::io::{Read, Write};
use std::net::TcpListener;
use std::time::Duration;

#[test]
fn websocket_ignores_unrelated_results() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    let server = std::thread::spawn(move || {
        let (stream, _) = listener.accept().unwrap();
        stream
            .set_read_timeout(Some(Duration::from_secs(2)))
            .unwrap();
        let mut socket = tungstenite::accept(stream).unwrap();
        let command: Value =
            serde_json::from_str(socket.read().unwrap().to_text().unwrap()).unwrap();
        socket
            .send(tungstenite::Message::Text(
                json!({"rid":99,"ok":false}).to_string(),
            ))
            .unwrap();
        socket
            .send(tungstenite::Message::Text(
                json!({"rid":command["rid"],"ok":true}).to_string(),
            ))
            .unwrap();
    });
    let mut transport =
        WebSocketTransport::open(&format!("ws://{address}/ws"), Duration::from_secs(2)).unwrap();
    let result = transport.exchange(
        &json!({"T":807,"brightness":0.1,"rid":42}),
        Duration::from_secs(2),
    );
    server.join().unwrap();
    assert_eq!(result.unwrap()["rid"], 42);
}

#[test]
fn http_waits_for_matching_execution_result() {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    let server = std::thread::spawn(move || {
        for step in 0..3 {
            let (mut stream, _) = listener.accept().unwrap();
            stream
                .set_read_timeout(Some(Duration::from_secs(2)))
                .unwrap();
            let mut request = Vec::new();
            while !request.ends_with(b"\r\n\r\n") {
                let mut byte = [0];
                stream.read_exact(&mut byte).unwrap();
                request.push(byte[0]);
                assert!(request.len() < 8192);
            }
            let header = String::from_utf8(request).unwrap();
            let (status, body) = if step == 0 {
                assert!(header.starts_with("POST /api/cmd "));
                let length = header
                    .lines()
                    .find_map(|line| {
                        line.to_lowercase()
                            .strip_prefix("content-length:")
                            .map(|n| n.trim().parse::<usize>().unwrap())
                    })
                    .unwrap();
                let mut body = vec![0; length];
                stream.read_exact(&mut body).unwrap();
                let command: Value = serde_json::from_slice(&body).unwrap();
                assert_eq!(command["rid"], 42);
                ("202 Accepted", json!({"accepted":true}))
            } else {
                assert!(header.starts_with("GET /api/result "));
                ("200 OK", json!({"rid":if step==1{99}else{42},"ok":true}))
            };
            let body = body.to_string();
            write!(stream,"HTTP/1.1 {status}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{body}",body.len()).unwrap();
        }
    });
    let mut transport = HttpTransport::new(&format!("http://{address}"));
    let result = transport.exchange(
        &json!({"T":807,"brightness":0.1,"rid":42}),
        Duration::from_secs(2),
    );
    server.join().unwrap();
    assert_eq!(result.unwrap()["rid"], 42);
}
