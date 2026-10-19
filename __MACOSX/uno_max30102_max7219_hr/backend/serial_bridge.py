#!/usr/bin/env python3
"""Forward tagged Arduino telemetry lines to the local CARE SIGNAL API."""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request


PREFIX = b"@VITALS "


def forward(url: str, payload: dict, timeout: float) -> None:
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        response.read()


def main() -> None:
    parser = argparse.ArgumentParser(description="Bridge Arduino @VITALS lines to the local API")
    parser.add_argument("--port", required=True, help="serial port, for example COM5 or /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--url", default="http://127.0.0.1:8000/api/v1/telemetry")
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        parser.error("pyserial is missing; install it with: python -m pip install -r backend/requirements.txt")

    print(f"Reading {args.port} at {args.baud} baud; forwarding @VITALS lines to {args.url}")
    print("Close Arduino Serial Monitor before starting this bridge. Press Ctrl+C to stop.")
    try:
        with serial.Serial(args.port, args.baud, timeout=1) as device:
            while True:
                line = device.readline().strip()
                if not line.startswith(PREFIX):
                    continue
                try:
                    payload = json.loads(line[len(PREFIX) :].decode("utf-8"))
                    forward(args.url, payload, args.timeout)
                    print(f"Forwarded telemetry from {payload.get('device_id', 'unknown device')}")
                except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                    print(f"Skipped malformed telemetry line: {exc}", file=sys.stderr)
                except urllib.error.HTTPError as exc:
                    detail = exc.read().decode("utf-8", errors="replace")
                    print(f"API rejected telemetry ({exc.code}): {detail}", file=sys.stderr)
                except (urllib.error.URLError, TimeoutError, OSError) as exc:
                    print(f"Could not reach the local API: {exc}", file=sys.stderr)
    except KeyboardInterrupt:
        print("\nSerial bridge stopped.")


if __name__ == "__main__":
    main()
