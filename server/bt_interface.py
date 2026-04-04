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
    UID_PATTERN = re.compile(r"UID:([0-9A-Fa-f]{8})")
    NODE_PATTERN = re.compile(r"EVENT:NODE")

    def __init__(self, port: str, rx_timeout: float = 0.1):
        bridge_class = _load_bridge_class()
        self.bridge = bridge_class(port=port, rx_timeout=rx_timeout)

    def read_payload(self) -> str:
        return self.bridge.listen() or ""

    def read_uid(self) -> Optional[str]:
        payload = self.read_payload()
        if not payload:
            return None

        match = self.UID_PATTERN.search(payload)
        if match is None:
            return None

        return match.group(1).upper()

    def read_events(self) -> List[Tuple[str, str]]:
        payload = self.read_payload()
        if not payload:
            return []

        events: List[Tuple[str, str]] = []
        for uid in self.UID_PATTERN.findall(payload):
            events.append(("uid", uid.upper()))

        for _ in self.NODE_PATTERN.findall(payload):
            events.append(("node", ""))

        return events

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
