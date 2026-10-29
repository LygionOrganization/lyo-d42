"""Read status by default; --drive explicitly enables a one-second motion demo."""
import argparse
from lyo_d42 import Client, SerialTransport, HttpTransport, WebSocketTransport, CanTransport

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("transport", choices=["usb", "uart", "http", "ws", "can"])
parser.add_argument("address")
parser.add_argument("--interface", default="socketcan")
parser.add_argument("--node", type=int, default=1)
parser.add_argument("--drive", action="store_true")
args = parser.parse_args()
if args.transport == "can":
    import can
    link = CanTransport(can.Bus(interface=args.interface, channel=args.address, bitrate=500000), args.node)
elif args.transport in ("usb", "uart"):
    link = SerialTransport(args.address, 1000000 if args.transport == "uart" else 115200, uart=args.transport == "uart")
elif args.transport == "http":
    link = HttpTransport(args.address)
else:
    link = WebSocketTransport(args.address)
with Client(link) as robot:
    print(robot.status())
    if args.drive:
        robot.drive_for(.03, 0, .2, 1)
