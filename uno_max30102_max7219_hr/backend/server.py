#!/usr/bin/env python3
"""Local-only demo API and static server for CARE SIGNAL."""

from __future__ import annotations

import argparse
import json
import math
import re
import sqlite3
from contextlib import contextmanager
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


ROOT = Path(__file__).resolve().parents[1]
WEB_ROOT = ROOT / "web"
DB_PATH = ROOT / "backend" / "data" / "monitoring_demo.sqlite"
DEVICE_ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,40}$")
MAX_BODY_BYTES = 16_384
REVIEW_LOW_BPM = 50
REVIEW_HIGH_BPM = 110


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def db_connect() -> sqlite3.Connection:
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(DB_PATH)
    connection.row_factory = sqlite3.Row
    connection.execute(
        """CREATE TABLE IF NOT EXISTS telemetry (
             id INTEGER PRIMARY KEY AUTOINCREMENT,
             device_id TEXT NOT NULL,
             received_at TEXT NOT NULL,
             device_ts_ms INTEGER,
             heart_rate_bpm REAL,
             ir_raw INTEGER NOT NULL,
             finger_detected INTEGER NOT NULL,
             session_active INTEGER NOT NULL,
             source TEXT NOT NULL,
             demo INTEGER NOT NULL DEFAULT 0
           )"""
    )
    connection.execute(
        "CREATE INDEX IF NOT EXISTS idx_telemetry_device_id ON telemetry(device_id, id DESC)"
    )
    return connection


@contextmanager
def db_session():
    connection = db_connect()
    try:
        yield connection
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


def reading_dict(row: sqlite3.Row) -> dict:
    return {
        "id": row["id"],
        "device_id": row["device_id"],
        "received_at": row["received_at"],
        "device_ts_ms": row["device_ts_ms"],
        "heart_rate_bpm": row["heart_rate_bpm"],
        "ir_raw": row["ir_raw"],
        "finger_detected": bool(row["finger_detected"]),
        "session_active": bool(row["session_active"]),
        "source": row["source"],
        "demo": bool(row["demo"]),
    }


def latest_rows(connection: sqlite3.Connection) -> list[sqlite3.Row]:
    return connection.execute(
        """SELECT t.* FROM telemetry t
           INNER JOIN (SELECT device_id, MAX(id) AS latest_id
                       FROM telemetry GROUP BY device_id) latest
             ON latest.latest_id = t.id
           ORDER BY t.device_id"""
    ).fetchall()


def is_recent(timestamp: str, seconds: int = 15) -> bool:
    try:
        parsed = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
        return (datetime.now(timezone.utc) - parsed).total_seconds() <= seconds
    except (TypeError, ValueError):
        return False


def dashboard_payload() -> dict:
    with db_session() as connection:
        latest = [reading_dict(row) for row in latest_rows(connection)]
        total = connection.execute("SELECT COUNT(*) FROM telemetry").fetchone()[0]

    devices = []
    review_queue = []
    for reading in latest:
        demo = reading["demo"]
        bpm = reading["heart_rate_bpm"]
        if demo:
            state = "demo"
        elif is_recent(reading["received_at"]):
            state = "online"
        else:
            state = "offline"
        devices.append({**reading, "state": state})

        if bpm is not None and (bpm < REVIEW_LOW_BPM or bpm > REVIEW_HIGH_BPM):
            review_queue.append(
                {
                    "device_id": reading["device_id"],
                    "heart_rate_bpm": bpm,
                    "received_at": reading["received_at"],
                    "demo": demo,
                    "message": (
                        "演示规则触发；请人工核实佩戴、信号质量和设备状态。"
                        if demo
                        else "显示带外读数；请人工核实设备和信号，不代表诊断。"
                    ),
                }
            )

    return {
        "generated_at": utc_now(),
        "devices": devices,
        "review_queue": review_queue,
        "total_readings": total,
        "review_rule": {
            "min_bpm": REVIEW_LOW_BPM,
            "max_bpm": REVIEW_HIGH_BPM,
            "label": "仅供界面演示，不是医学阈值",
        },
    }


def validate_telemetry(payload: object) -> tuple[dict | None, str | None]:
    if not isinstance(payload, dict):
        return None, "JSON body must be an object"

    device_id = payload.get("device_id")
    if not isinstance(device_id, str) or not DEVICE_ID_RE.fullmatch(device_id):
        return None, "device_id must contain 1-40 letters, digits, underscores, or hyphens"

    bpm = payload.get("heart_rate_bpm")
    if bpm is not None:
        if (
            isinstance(bpm, bool)
            or not isinstance(bpm, (int, float))
            or not 20 <= bpm <= 250
            or not math.isfinite(float(bpm))
        ):
            return None, "heart_rate_bpm must be null or a number from 20 to 250"

    ir_raw = payload.get("ir_raw")
    if isinstance(ir_raw, bool) or not isinstance(ir_raw, int) or not 0 <= ir_raw <= 262143:
        return None, "ir_raw must be an integer from 0 to 262143"

    finger = payload.get("finger_detected")
    session = payload.get("session_active")
    if not isinstance(finger, bool) or not isinstance(session, bool):
        return None, "finger_detected and session_active must be booleans"

    device_ts_ms = payload.get("ts_ms")
    if device_ts_ms is not None and (
        isinstance(device_ts_ms, bool)
        or not isinstance(device_ts_ms, int)
        or not 0 <= device_ts_ms <= 2**32 - 1
    ):
        return None, "ts_ms must be null or an unsigned 32-bit integer"

    source = payload.get("source", "unknown")
    if not isinstance(source, str) or len(source) > 40:
        return None, "source must be a string of at most 40 characters"

    return {
        "device_id": device_id,
        "received_at": utc_now(),
        "device_ts_ms": device_ts_ms,
        "heart_rate_bpm": bpm,
        "ir_raw": ir_raw,
        "finger_detected": finger,
        "session_active": session,
        "source": source,
        "demo": False,
    }, None


def insert_reading(connection: sqlite3.Connection, reading: dict) -> None:
    connection.execute(
        """INSERT INTO telemetry
           (device_id, received_at, device_ts_ms, heart_rate_bpm, ir_raw,
            finger_detected, session_active, source, demo)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
        (
            reading["device_id"],
            reading["received_at"],
            reading["device_ts_ms"],
            reading["heart_rate_bpm"],
            reading["ir_raw"],
            int(reading["finger_detected"]),
            int(reading["session_active"]),
            reading["source"],
            int(reading["demo"]),
        ),
    )


class CareSignalHandler(BaseHTTPRequestHandler):
    server_version = "CareSignalLocal/1.0"

    def log_message(self, format_string: str, *args) -> None:
        print(f"[{self.log_date_time_string()}] {format_string % args}")

    def send_json(self, status: int, payload: dict) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        parsed = urlparse(self.path)
        if parsed.path == "/api/v1/dashboard":
            self.send_json(200, dashboard_payload())
            return

        if parsed.path == "/api/v1/readings":
            query = parse_qs(parsed.query)
            device_id = query.get("device_id", [""])[0]
            if not DEVICE_ID_RE.fullmatch(device_id):
                self.send_json(400, {"error": "valid device_id is required"})
                return
            try:
                limit = max(1, min(int(query.get("limit", ["60"])[0]), 240))
            except ValueError:
                limit = 60
            with db_session() as connection:
                rows = connection.execute(
                    "SELECT * FROM telemetry WHERE device_id = ? ORDER BY id DESC LIMIT ?",
                    (device_id, limit),
                ).fetchall()
            self.send_json(200, {"readings": [reading_dict(row) for row in rows]})
            return

        static_files = {
            "/": WEB_ROOT / "index.html",
            "/index.html": WEB_ROOT / "index.html",
            "/styles.css": WEB_ROOT / "styles.css",
            "/app.js": WEB_ROOT / "app.js",
        }
        target = static_files.get(parsed.path)
        if target is None or not target.is_file():
            self.send_json(404, {"error": "not found"})
            return
        content_type = {
            ".html": "text/html; charset=utf-8",
            ".css": "text/css; charset=utf-8",
            ".js": "text/javascript; charset=utf-8",
        }.get(target.suffix, "application/octet-stream")
        body = target.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        if self.path not in ("/api/v1/telemetry", "/api/v1/demo/seed"):
            self.send_json(404, {"error": "not found"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.send_json(400, {"error": "invalid Content-Length"})
            return
        if not 0 < length <= MAX_BODY_BYTES:
            self.send_json(413, {"error": "body size must be between 1 and 16384 bytes"})
            return
        try:
            payload = json.loads(self.rfile.read(length))
        except (json.JSONDecodeError, UnicodeDecodeError):
            self.send_json(400, {"error": "invalid JSON"})
            return

        if self.path == "/api/v1/telemetry":
            reading, error = validate_telemetry(payload)
            if error:
                self.send_json(400, {"error": error})
                return
            with db_session() as connection:
                insert_reading(connection, reading)
            self.send_json(201, {"accepted": True, "device_id": reading["device_id"]})
            return

        if not isinstance(payload, dict) or payload.get("confirm") is not True:
            self.send_json(400, {"error": "send {\"confirm\": true} to create synthetic demo records"})
            return

        now = datetime.now(timezone.utc)
        profiles = {
            "unit-a-01": [65, 66, 67, 66, 68, 70, 69, 68, 67, 66, 65, 67, 69, 71, 70, 68, 67, 66, 68, 70, 72, 73, 71, 72],
            "unit-a-02": [76, 77, 78, 79, 78, 80, 82, 84, 85, 87, 89, 91, 94, 96, 99, 103, 108, 112, 109, 106, 104, 101, 99, 112],
        }
        with db_session() as connection:
            connection.execute("DELETE FROM telemetry WHERE demo = 1")
            for device_id, values in profiles.items():
                for index, bpm in enumerate(values):
                    received_at = (now - timedelta(minutes=len(values) - index - 1)).isoformat(timespec="seconds").replace("+00:00", "Z")
                    insert_reading(
                        connection,
                        {
                            "device_id": device_id,
                            "received_at": received_at,
                            "device_ts_ms": index * 60_000,
                            "heart_rate_bpm": bpm,
                            "ir_raw": 58_000 + (index * 431 % 9_000),
                            "finger_detected": True,
                            "session_active": True,
                            "source": "synthetic-demo",
                            "demo": True,
                        },
                    )
        self.send_json(201, {"created": 48, "demo": True})


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the local CARE SIGNAL prototype server")
    parser.add_argument("--host", default="127.0.0.1", help="bind address (default: localhost only)")
    parser.add_argument("--port", type=int, default=8000, help="port (default: 8000)")
    args = parser.parse_args()

    with db_session():
        pass
    server = ThreadingHTTPServer((args.host, args.port), CareSignalHandler)
    print(f"CARE SIGNAL local demo: http://{args.host}:{args.port}")
    print(f"SQLite database: {DB_PATH}")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping local demo server.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
