import re
import sys
from pathlib import Path
from typing import List, Optional, Tuple


def _load_bridge_class():
    try:
        from chat_hm10.hm10_esp32.hm10_esp32_bridge import HM10ESP32Bridge
        return HM10ESP32Bridge
    except ImportError:
        repo_root = Path(__file__).resolve().parents[1]
        if str(repo_root) not in sys.path:
            sys.path.append(str(repo_root))
        from chat_hm10.hm10_esp32.hm10_esp32_bridge import HM10ESP32Bridge
        return HM10ESP32Bridge


class BTInterface:
    UID_PATTERN = re.compile(r"UID:([0-9A-Fa-f]{20}|[0-9A-Fa-f]{14}|[0-9A-Fa-f]{8})")
    NODE_PATTERN = re.compile(r"EVENT:NODE")
    EVENT_PATTERN = re.compile(r"UID:([0-9A-Fa-f]{20}|[0-9A-Fa-f]{14}|[0-9A-Fa-f]{8})|EVENT:NODE")
    MAX_BUFFER_CHARS = 128

    def __init__(self, port: str, rx_timeout: float = 0.02):
        bridge_class = _load_bridge_class()
        self.bridge = bridge_class(port=port, rx_timeout=rx_timeout)
        self._buffer = ""

    def read_payload(self) -> str:
        payload = self.bridge.listen() or ""
        if payload:
            self._buffer += payload
            if len(self._buffer) > self.MAX_BUFFER_CHARS:
                self._buffer = self._buffer[-self.MAX_BUFFER_CHARS :]
        return payload

    def _extract_events_from_buffer(self) -> List[Tuple[str, str]]:
        events: List[Tuple[str, str]] = []

        while True:
            match = self.EVENT_PATTERN.search(self._buffer)
            if match is None:
                if len(self._buffer) > self.MAX_BUFFER_CHARS:
                    self._buffer = self._buffer[-self.MAX_BUFFER_CHARS :]
                break

            if match.group(1) is not None:
                events.append(("uid", match.group(1).upper()))
            else:
                events.append(("node", ""))

            self._buffer = self._buffer[match.end() :]

        return events

    def read_uid(self) -> Optional[str]:
        self.read_payload()
        for event_type, payload in self._extract_events_from_buffer():
            if event_type == "uid":
                return payload
        return None

    def read_events(self) -> List[Tuple[str, str]]:
        self.read_payload()
        return self._extract_events_from_buffer()

    def send(self, text: str):
        self.bridge.send(text)

    def send_command(self, command: str):
        if len(command) != 1:
            raise ValueError(f"Expected a single command character, got: {command}")
        self.send(command)

    def get_hm10_name(self, timeout: float = 2.0):
        return self.bridge.get_hm10_name(timeout=timeout)

    def set_hm10_name(self, name: str, timeout: float = 2.0):
        return self.bridge.set_hm10_name(name, timeout=timeout)

    def get_status(self, timeout: float = 2.0):
        return self.bridge.get_status(timeout=timeout)

    def reset(self):
        return self.bridge.reset()

    def close(self):
        serial_handle = getattr(self.bridge, "ser", None)
        if serial_handle is not None and getattr(serial_handle, "is_open", False):
            serial_handle.close()
