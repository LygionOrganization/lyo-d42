"""Compatibility entry point for the packaged Lyo-D42 SDK; status only by default.

Install first: python -m pip install './sdk/python[all]'
Use sdk/python/examples/control.py for all transports.
"""
import argparse
from lyo_d42 import Client, SerialTransport

class LyoD42Client(Client):
    def __init__(self, port):
        super().__init__(SerialTransport(port))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--drive", action="store_true")
    args = parser.parse_args()
    with LyoD42Client(args.port) as robot:
        print(robot.status())
        if args.drive:
            robot.drive_for(.03, 0, .2, 1)
