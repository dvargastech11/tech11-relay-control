"""
Relay settings that an admin can change from the website.

hold_ms: how long a normal floor press holds its relay (the operator floor
buttons and /api/activate). The devices enforce their own limits too (100 ms
to 10 minutes), so a bad value here can never hold a relay indefinitely.

Stored outside the git repo, like the other stores (TECH11_DATA_DIR aware).
"""

import json
import os
import platform
import threading

_default_data_dir = r"C:\tech11-data" if platform.system() == "Windows" else os.path.expanduser("~/tech11-data")
DATA_DIR = os.environ.get("TECH11_DATA_DIR", _default_data_dir)
DATA_FILE = os.path.join(DATA_DIR, "relay_settings.json")

DEFAULT_HOLD_MS = 5000   # the original fixed value
MIN_HOLD_MS = 100
MAX_HOLD_MS = 600000     # 10 minutes, same as the device's hard cap

_lock = threading.Lock()


def _load():
    try:
        with open(DATA_FILE, "r") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def get_hold_ms():
    try:
        ms = int(_load().get("hold_ms", DEFAULT_HOLD_MS))
    except (TypeError, ValueError):
        return DEFAULT_HOLD_MS
    return min(max(ms, MIN_HOLD_MS), MAX_HOLD_MS)


def set_hold_ms(ms):
    """Raises ValueError if ms is outside MIN_HOLD_MS..MAX_HOLD_MS."""
    ms = int(ms)
    if ms < MIN_HOLD_MS or ms > MAX_HOLD_MS:
        raise ValueError("hold time out of range")
    with _lock:
        data = _load()
        data["hold_ms"] = ms
        os.makedirs(DATA_DIR, exist_ok=True)
        with open(DATA_FILE, "w") as f:
            json.dump(data, f)
    return ms
