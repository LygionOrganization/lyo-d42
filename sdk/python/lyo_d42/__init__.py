"""Explicit, bounded control of a Lyo-D42 chassis. No implicit motion or retries."""
import json
import math
import struct
import threading
import time
import urllib.request

MAX_SPEED = 0.32
TRACK = 0.159


class DeviceError(RuntimeError):
    pass


def finite(value, low, high, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError(f"{name} must be finite and within {low}..{high}")
    return value


def encode_json(command):
    data = json.dumps(command, separators=(",", ":"), allow_nan=False).encode()
    if len(data) > 255:
        raise ValueError("Command exceeds the firmware's 255-byte limit")
    return data


def checked(result):
    if not result.get("ok", False):
        raise DeviceError(result.get("error", "Device rejected the command"))
    return result


class SerialTransport:
    """Native USB CDC or UART0 JSON lines. UART0 uses 1 Mbps by default."""
    def __init__(self, port, baudrate=115200, uart=False):
        import serial
        self.port = serial.Serial(port, baudrate, timeout=0.05, write_timeout=0.5)
        if uart:
            # Enable UART replies; this setting is volatile and leaves USB replies on.
            self.send({"T": 604, "echo": 0, "uart": 1, "usb": 1})

    def send(self, command):
        data = encode_json(command) + b"\n"
        if self.port.write(data) != len(data):
            raise TimeoutError("Incomplete serial write; command was not retried")

    def exchange(self, command, timeout):
        self.send(command)
        end = time.monotonic() + timeout
        buffer = bytearray()
        dropping = False
        while time.monotonic() < end:
            byte = self.port.read(1)
            if byte == b"\n":
                try:
                    result = json.loads(buffer) if not dropping else None
                except (ValueError, UnicodeError):
                    result = None
                buffer.clear()
                dropping = False
                if isinstance(result, dict) and result.get("rid") == command["rid"]:
                    return checked(result)
            elif byte and not dropping:
                buffer.extend(byte)
                if len(buffer) > 8192:
                    buffer.clear()
                    dropping = True
        raise TimeoutError("Serial reply timed out; inspect state before retrying")

    def close(self):
        self.port.close()


class HttpTransport:
    def __init__(self, base_url="http://192.168.4.1"):
        self.url = base_url.rstrip("/")

    def _get(self, path, timeout):
        with urllib.request.urlopen(self.url + path, timeout=timeout) as r:
            return json.load(r)

    def send(self, command, timeout=1):
        req = urllib.request.Request(self.url + "/api/cmd", encode_json(command), {"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            if r.status != 202:
                raise DeviceError(f"Unexpected HTTP status {r.status}")

    def exchange(self, command, timeout):
        if command["T"] == 808:
            return checked(self._get("/api/chassis", timeout))
        end = time.monotonic() + timeout
        self.send(command, timeout)
        while time.monotonic() < end:
            result = self._get("/api/result", max(0.01, end - time.monotonic()))
            if result.get("rid") == command["rid"]:
                return checked(result)
            time.sleep(0.02)
        raise TimeoutError("HTTP execution result missing; HTTP 202 only means accepted")

    def close(self):
        pass


class WebSocketTransport:
    def __init__(self, url="ws://192.168.4.1/ws", timeout=2):
        import websocket
        self.socket = websocket.create_connection(url, timeout=timeout)

    def send(self, command):
        self.socket.send(encode_json(command).decode())

    def exchange(self, command, timeout):
        self.send(command)
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.socket.settimeout(max(0.01, end - time.monotonic()))
            data = self.socket.recv()
            if not data:
                raise ConnectionError("WebSocket closed")
            if len(data) > 8192:
                continue
            try:
                result = json.loads(data)
            except (ValueError, UnicodeError):
                continue
            if isinstance(result, dict) and result.get("rid") == command["rid"]:
                return checked(result)
        raise TimeoutError("WebSocket result timed out")

    def close(self):
        self.socket.close()


def quantize(x):
    """Round half away from zero, matching the Rust and firmware encoders."""
    return math.floor(x + .5) if x >= 0 else math.ceil(x - .5)


def can_encode(command, node=1):
    """Return a classic extended CAN ID and exactly eight data bytes."""
    if type(node) is not int or not 1 <= node <= 127:
        raise ValueError("CAN node must be 1..127")
    t = command["T"]
    op = t - 800
    data = bytearray(struct.pack("<H", command.get("rid", 0) & 0xffff) + bytes(6))
    if t == 809:
        if set(command) - {"T", "rid", "reliable", "timeout_ms"}:
            raise ValueError("CAN SDK configuration supports timeout_ms only; use JSON for other settings")
        op = 12
        struct.pack_into("<H", data, 2, int(finite(command["timeout_ms"], 100, 60000, "timeout_ms")))
    elif t in (801, 811):
        data[2] = int(command["enable" if t == 801 else "hold"])
    elif 802 <= t <= 806:
        if t in (802, 803):
            values = [command["left"], command["right"]]
            scale = 100 if t == 802 else 1000
            encoded = [quantize(x * scale) for x in values]
        elif t == 804:
            encoded = [quantize(command["v"] * 1000), quantize(command["w"] * 1000)]
        else:
            encoded = [quantize(command["distance" if t == 805 else "degrees"] * (1000 if t == 805 else 100)), quantize(command["speed"] * 1000)]
        if any(not -32768 <= x <= 32767 for x in encoded):
            raise ValueError("Target exceeds classic CAN field range")
        struct.pack_into("<hh", data, 2, *encoded)
        acceleration = finite(command["accel_mps2"], .01, 2.54, "CAN accel_mps2")
        data[6] = quantize(acceleration * 100)
        data[7] = 0x80 | int(command.get("soft_stop", True))
    elif t == 807:
        struct.pack_into("<H", data, 2, quantize(finite(command["brightness"], 0, 1, "brightness") * 1000))
    elif t not in (800, 808, 810):
        raise ValueError("Command has no CAN mapping")
    return 0x18EF0000 | node << 8 | op | (0x80 if command.get("reliable", True) else 0), bytes(data)


class CanTransport:
    """Accept a python-can Bus: SocketCAN, PCAN, Vector, SLCAN, etc."""
    def __init__(self, bus, node=1):
        self.bus, self.node = bus, node
        can_encode({"T": 808}, node)

    def send(self, command):
        import can
        identifier, data = can_encode(command, self.node)
        self.bus.send(can.Message(arbitration_id=identifier, is_extended_id=True, data=data), timeout=.5)

    def exchange(self, command, timeout):
        identifier, _ = can_encode(command, self.node)
        self.send(command)
        op = identifier & 0x7f
        seq = command["rid"] & 0xffff
        end = time.monotonic() + timeout
        ack, parts = False, {}
        while time.monotonic() < end:
            frame = self.bus.recv(max(0, end - time.monotonic()))
            if frame is None:
                break
            d = frame.data
            if not frame.is_extended_id or frame.is_remote_frame or frame.is_error_frame or len(d) < 3 or struct.unpack_from("<H", d)[0] != seq:
                continue
            if frame.arbitration_id == 0x18EE0000 | self.node << 8 | op:
                if d[2]:
                    raise DeviceError("CAN command rejected; query chassis state")
                ack = True
            elif command["T"] == 808 and len(d) == 8:
                for part in range(3):
                    if frame.arbitration_id == 0x18ED0000 | self.node << 8 | part:
                        parts[part] = d
            if ack and command["T"] != 808:
                return {"T": command["T"], "ok": True}
            if ack and len(parts) == 3:
                a, b, c = (parts[i] for i in range(3))
                return {"T": 808, "ok": True, "ready": bool(a[2]), "enabled": bool(a[3]), "led": a[7]/100,
                        "wheels": [{"id": i+1, "valid": bool(a[4+i]), "mps": struct.unpack_from("<h", b, 2+i*2)[0]/1000,
                                    "status": b[6+i], "current_raw": struct.unpack_from("<h", c, 2+i*2)[0],
                                    "current_valid": bool(c[6+i]), **({"voltage": a[6]/10} if i == 0 else {})} for i in range(2)]}
        raise TimeoutError("CAN reply incomplete; no command was retried")

    def close(self):
        self.bus.shutdown()


class Client:
    """One controller per chassis. Methods are serialized; no background heartbeat."""
    def __init__(self, transport, timeout=1):
        self.transport, self.timeout = transport, timeout
        self._rid = time.time_ns() & 0x7fffffff
        self._lock = threading.Lock()

    def request(self, command):
        with self._lock:
            self._rid = (self._rid + 1) & 0x7fffffff
            return self.transport.exchange({**command, "rid": self._rid, "reliable": True}, self.timeout)

    def status(self):
        return self.request({"T": 808})

    def twist(self, v, w, accel_mps2, soft_stop=True):
        finite(v, -MAX_SPEED, MAX_SPEED, "v")
        finite(w, -2*MAX_SPEED/TRACK, 2*MAX_SPEED/TRACK, "w")
        if max(abs(v-w*TRACK/2), abs(v+w*TRACK/2)) > MAX_SPEED + 1e-9:
            raise ValueError("Combined v/w exceeds a wheel's 0.32 m/s limit")
        finite(accel_mps2, .01, 5, "accel_mps2")
        return self.request({"T": 804, "v": v, "w": w, "accel_mps2": accel_mps2, "soft_stop": bool(soft_stop)})

    def wheels(self, left, right, accel_mps2, soft_stop=True):
        finite(left, -MAX_SPEED, MAX_SPEED, "left")
        finite(right, -MAX_SPEED, MAX_SPEED, "right")
        finite(accel_mps2, .01, 5, "accel_mps2")
        return self.request({"T": 803, "left": left, "right": right, "accel_mps2": accel_mps2, "soft_stop": bool(soft_stop)})

    def move(self, distance, speed, accel_mps2):
        finite(distance, -100, 100, "distance")
        return self._position(805, "distance", distance, speed, accel_mps2)

    def turn(self, degrees, speed, accel_mps2):
        finite(degrees, -36000, 36000, "degrees")
        return self._position(806, "degrees", degrees, speed, accel_mps2)

    def _position(self, t, key, value, speed, accel):
        finite(speed, .005, MAX_SPEED, "speed")
        finite(accel, .01, 5, "accel_mps2")
        return self.request({"T": t, key: value, "speed": speed, "accel_mps2": accel})

    def heartbeat(self):
        return self.request({"T": 810})

    def hill_hold(self, enabled):
        return self.request({"T": 811, "hold": bool(enabled)})

    def light(self, brightness):
        finite(brightness, 0, 1, "brightness")
        return self.request({"T": 807, "brightness": brightness})

    def set_timeout(self, timeout_ms):
        if type(timeout_ms) is not int or not 100 <= timeout_ms <= 60000:
            raise ValueError("timeout_ms must be an integer in 100..60000")
        return self.request({"T": 809, "timeout_ms": timeout_ms})

    def stop(self):
        # T800 has no JSON execution reply. A CAN ACK confirms reception only.
        with self._lock:
            self.transport.send({"T": 800})

    def drive_for(self, v, w, accel_mps2, seconds, interval=.1):
        finite(seconds, .01, 60, "seconds")
        finite(interval, .02, .5, "interval")
        end = time.monotonic() + seconds
        try:
            while time.monotonic() < end:
                self.twist(v, w, accel_mps2)
                time.sleep(min(interval, max(0, end-time.monotonic())))
        finally:
            self.stop()

    def close(self):
        self.transport.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
