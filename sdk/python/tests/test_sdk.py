import json
import sys
import threading
import types
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from lyo_d42 import Client, CanTransport, HttpTransport, SerialTransport, WebSocketTransport, can_encode, encode_json


class Fake:
    def __init__(self):
        self.commands = []
    def send(self, command):
        self.commands.append(command)
    def exchange(self, command, timeout):
        self.send(command)
        return {"ok": True, "rid": command["rid"]}
    def close(self):
        pass


class Tests(unittest.TestCase):
    def test_limits_and_units(self):
        link = Fake()
        c = Client(link)
        for args in [(.321, 0, .2), (.32, 1, .2), (float("nan"), 0, .2), (0, 0, 0)]:
            with self.assertRaises(ValueError):
                c.twist(*args)
        self.assertFalse(link.commands)
        c.twist(.1, 0, .2)
        self.assertEqual(link.commands[-1]["accel_mps2"], .2)
        self.assertNotIn("acc", link.commands[-1])
        c.stop()
        self.assertEqual(link.commands[-1], {"T": 800})
        c.close()
        self.assertEqual(len(link.commands), 2)

    def test_can_wire_layout(self):
        c = {"T": 804, "v": .1, "w": 0, "accel_mps2": .2, "soft_stop": True, "rid": 42}
        self.assertEqual(can_encode(c), (0x18EF0184, bytes([42, 0, 100, 0, 0, 0, 20, 129])))
        c["v"] = -.1
        self.assertEqual(can_encode(c)[1][2:4], b"\x9c\xff")
        c["accel_mps2"] = 5
        with self.assertRaises(ValueError):
            can_encode(c)
        self.assertEqual(can_encode({"T": 809, "timeout_ms": 3000})[1][2:4], b"\xb8\x0b")
        with self.assertRaises(ValueError):
            can_encode({"T": 809, "max_speed": .2})

    def test_serial_partial_and_unrelated_lines(self):
        class Port:
            def __init__(self):
                self.data = bytearray(b'noise\n{"rid":99,"ok":true}\n{"rid":42,"ok":true}\n')
            def write(self, data):
                return len(data)
            def read(self, _):
                return bytes([self.data.pop(0)]) if self.data else b""
        t = SerialTransport.__new__(SerialTransport)
        t.port = Port()
        self.assertTrue(t.exchange({"T": 808, "rid": 42}, .1)["ok"])

    def test_can_status_correlation(self):
        class Bus:
            def send(self, message, timeout):
                seq = bytes(message.data[:2])
                def frame(identifier, suffix):
                    return types.SimpleNamespace(arbitration_id=identifier, data=seq+bytes(suffix), is_extended_id=True, is_remote_frame=False, is_error_frame=False)
                self.frames = [frame(0x18ED0102, [123,0,211,255,1,1]), frame(0x18ED0100,[1,0,1,1,200,0]), frame(0x18EE0108,[0]), frame(0x18ED0101,[100,0,156,255,16,16])]
            def recv(self, timeout):
                return self.frames.pop(0) if self.frames else None
        previous = sys.modules.get("can")
        sys.modules["can"] = types.SimpleNamespace(Message=lambda **kw: types.SimpleNamespace(**kw))
        try:
            result = CanTransport(Bus()).exchange({"T": 808, "rid": 42}, .1)
            self.assertEqual(result["wheels"][1]["current_raw"], -45)
            self.assertEqual(result["wheels"][1]["mps"], -.1)
        finally:
            if previous is None:
                del sys.modules["can"]
            else:
                sys.modules["can"] = previous

    def test_http_acceptance_and_execution(self):
        class Handler(BaseHTTPRequestHandler):
            result = {}
            def do_POST(self):
                command = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                Handler.result = {"rid": command["rid"], "ok": True}
                self.send_response(202)
                self.end_headers()
            def do_GET(self):
                self.send_response(200)
                self.end_headers()
                self.wfile.write(json.dumps(Handler.result).encode())
            def log_message(self, *_):
                pass
        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        try:
            c = Client(HttpTransport(f"http://127.0.0.1:{server.server_port}"))
            self.assertTrue(c.light(.1)["ok"])
        finally:
            server.shutdown()
            thread.join()
            server.server_close()

    def test_bounded_helper_stops_on_error(self):
        class Broken(Fake):
            def exchange(self, command, timeout):
                raise TimeoutError("simulated loss")
        link = Broken()
        with self.assertRaises(TimeoutError):
            Client(link).drive_for(.03, 0, .2, 1)
        self.assertEqual(link.commands, [{"T": 800}])

    def test_payload_limit(self):
        with self.assertRaises(ValueError):
            encode_json({"T": 1, "data": "x"*256})

    def test_websocket_correlation_and_close(self):
        class Socket:
            def __init__(self):
                self.messages = iter(['noise', '{"rid":99,"ok":false}', '{"rid":42,"ok":true}', ''])
            def send(self, data):
                self.sent = json.loads(data)
            def settimeout(self, timeout):
                self.timeout = timeout
            def recv(self):
                return next(self.messages)
        t = WebSocketTransport.__new__(WebSocketTransport)
        t.socket = Socket()
        self.assertTrue(t.exchange({"T":808,"rid":42}, .1)["ok"])
        with self.assertRaises(ConnectionError):
            t.exchange({"T":808,"rid":43}, .1)


if __name__ == "__main__":
    unittest.main()
