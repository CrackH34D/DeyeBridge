#!/usr/bin/env python3
"""Web UI test harness for DeyeBridge.

Modes:
  check   Static validation of the real firmware source (WebConfig.cpp):
          - every form field has a matching CopyFormField handler
          - every CopyFormField field exists in the HTML
          - snprintf format placeholders match the provided arguments
          - CSS percent escapes are balanced
  serve   Renders the config page from the firmware source with sample values
          and serves it plus stub /values, /save and /update endpoints, so the
          UI can be exercised in a browser without hardware.
"""

import re
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent / "src" / "web" / "WebConfig.cpp"

SAMPLE_SETTINGS = {
    "wifi_ssid": "HomeNet",
    "wifi_pass": "secret-pass",
    "mqtt_host": "192.168.1.10",
    "mqtt_port": "1883",
    "mqtt_user": "ha",
    "mqtt_pass": "ha-password",
    "mqtt_client": "deye-bridge",
    "mqtt_base": "deye",
    "slave_id": "1",
    "rtu_baud": "9600",
    "rtu_tx": "12",
    "rtu_rx": "11",
    "rtu_de": "18",
    "poll_ms": "2000",
    "pub_ms": "5000",
}


def extract_page_segment(source: str) -> str:
    """Extracts the config-page snprintf call (format string part)."""
    start = source.index('snprintf(page, sizeof(page),')
    end = source.index('s.wifiSsid, s.wifiPassword,')
    return source[start:end]


def joined_format_string(segment: str) -> str:
    """Joins the C string literals of the format and decodes escapes."""
    literals = re.findall(r'"((?:[^"\\]|\\.)*)"', segment)
    text = "".join(literals)
    # Decode the escapes the compiler would handle (no \t etc. used)
    text = text.replace('\\n', '\n').replace('\\"', '"').replace("\\'", "'")
    return text


def render_format(text: str, settings: dict) -> str:
    """Replaces %s/%u/%lu/%d placeholders with sample values in order."""
    order = ["wifi_ssid", "wifi_pass", "mqtt_host", "mqtt_port", "mqtt_user",
             "mqtt_pass", "mqtt_client", "mqtt_base", "slave_id", "rtu_baud",
             "rtu_tx", "rtu_rx", "rtu_de", "poll_ms", "pub_ms"]
    values = iter(settings[k] for k in order)
    return re.sub(r"%%", "\x00", re.sub(r"%(?:s|u|lu|d)", lambda m: next(values), text)).replace("\x00", "%")


def html_fields(page: str) -> list:
    # Only <input>/<select> elements, not <meta name='viewport'>
    return re.findall(r"<(?:input|select)[^>]*name='([^']+)'", page)


def handler_fields(source: str) -> list:
    return re.findall(r'CopyFormField\(body, "([^"]+)"', source)


def format_placeholders(text: str) -> list:
    cleaned = text.replace("%%", "")   # literal percent (CSS)
    return re.findall(r"%(?:s|u|lu|d|zu|i)", cleaned)


def run_check() -> int:
    source = SOURCE.read_text()
    page_text = joined_format_string(extract_page_segment(source))

    failures = []

    html = html_fields(page_text)
    handlers = handler_fields(source)

    missing_handler = [f for f in html if f not in handlers]
    if missing_handler:
        failures.append(f"HTML fields without save handler: {missing_handler}")

    missing_field = [f for f in handlers if f not in html]
    if missing_field:
        failures.append(f"Save handlers without HTML field: {missing_field}")

    # Format placeholder count must match the argument list (15 values)
    placeholders = format_placeholders(page_text)
    if len(placeholders) != 15:
        failures.append(f"Expected 15 format placeholders, found {len(placeholders)}: {placeholders}")

    # Rendered page must not contain a stray % (would indicate broken escaping)
    rendered = render_format(page_text, SAMPLE_SETTINGS)
    stray = format_placeholders(rendered)
    if stray:
        failures.append(f"Rendered page contains stray format specifiers: {stray}")

    # The page must contain the key UI elements
    for needle in ["otaUpload", "x.open('POST','/update')", "action='/save'", "'/values'"]:
        if needle not in rendered:
            failures.append(f"Missing UI element: {needle!r}")

    if failures:
        print("FAILED:")
        for f in failures:
            print(f"  - {f}")
        return 1

    print(f"OK: {len(html)} form fields <-> {len(handlers)} save handlers, "
          f"15 format placeholders, all UI elements present")
    return 0


class MockHandler(BaseHTTPRequestHandler):
    page_html = ""

    def _send(self, code, body, content_type="text/html; charset=utf-8"):
        data = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path.startswith("/values"):
            values = ('{"pv_total_power":2220,"battery_soc":78,"battery_power":-1200,'
                      '"grid_power":250,"grid_connected":true,"time_slot_1":"06:30",'
                      '"overall_state":"normal"}')
            self._send(200, values, "application/json")
        else:
            self._send(200, MockHandler.page_html)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0) or 0)
        body = self.rfile.read(length).decode(errors="replace") if length else ""
        if self.path.startswith("/save"):
            fields = urllib.parse.parse_qs(body, keep_blank_values=True)
            print("[mock] /save fields:")
            for key, vals in fields.items():
                shown = vals[0] if key != "wifi_pass" and key != "mqtt_pass" else "***"
                print(f"    {key} = {shown}")
            self._send(200, "<html><body><p>Saved (mock). Fields logged to console.</p></body></html>")
        elif self.path.startswith("/update"):
            print(f"[mock] /update received {length} bytes")
            self._send(200, "Update successful (mock), rebooting...")
        else:
            self._send(404, "not found")

    def log_message(self, *args):
        pass


def run_serve(port: int) -> int:
    source = SOURCE.read_text()
    page_text = joined_format_string(extract_page_segment(source))
    MockHandler.page_html = render_format(page_text, SAMPLE_SETTINGS)
    print(f"Serving rendered DeyeBridge web UI at http://localhost:{port}/ "
          "(Ctrl+C to stop)")
    server = HTTPServer(("127.0.0.1", port), MockHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "check"
    if mode == "check":
        sys.exit(run_check())
    elif mode == "serve":
        sys.exit(run_serve(int(sys.argv[2]) if len(sys.argv) > 2 else 8080))
    else:
        print("usage: webui_test.py check|serve [port]")
        sys.exit(2)
