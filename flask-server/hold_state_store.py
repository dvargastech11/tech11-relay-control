"""
State for the operator "ACTIVATE ALL BUTTONS" test, one entry per building.

The devices do the actual holding (and release themselves after 10 minutes
even if this server dies). This just remembers WHEN each building's hold
ends, so every operator's browser shows the same on/off state and the
countdown survives a server restart.

Stored outside the git repo, like the other stores (TECH11_DATA_DIR aware).
"""

import json
import os
import platform
import threading
import time

_default_data_dir = r"C:\tech11-data" if platform.system() == "Windows" else os.path.expanduser("~/tech11-data")
DATA_DIR = os.environ.get("TECH11_DATA_DIR", _default_data_dir)
DATA_FILE = os.path.join(DATA_DIR, "all_buttons_hold.json")

_lock = threading.Lock()


def _load():
    if os.path.exists(DATA_FILE):
        try:
            with open(DATA_FILE, "r") as f:
                data = json.load(f)
        except (OSError, ValueError):
            return {}
        now = time.time()
        return {k: v for k, v in data.items() if v > now}  # drop expired entries
    return {}


def _save(data):
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(DATA_FILE, "w") as f:
        json.dump(data, f)


def start(building_id, seconds):
    with _lock:
        data = _load()
        data[building_id] = time.time() + seconds
        _save(data)


def stop(building_id):
    with _lock:
        data = _load()
        data.pop(building_id, None)
        _save(data)


def remaining_seconds(building_id):
    expires_at = _load().get(building_id, 0)
    return max(0, int(round(expires_at - time.time())))


def is_on(building_id):
    return remaining_seconds(building_id) > 0
