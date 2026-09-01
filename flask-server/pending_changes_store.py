"""
Pending network config changes - if you try to change a device's network
settings (IP, DHCP, name, etc.) while it's offline, the change can't be
delivered right then (it's a live HTTP push, not a message queue the
device checks). Instead of failing silently and losing the intent, the
requested change is queued here and automatically applied the next time
the device is seen online (checked whenever the Devices page polls it).

Stored OUTSIDE the git repo folder (same reasoning as the other data
stores), keyed by MAC address.
"""

import json
import os
import platform
from datetime import datetime

if platform.system() == "Windows":
    DATA_DIR = r"C:\tech11-data"
else:
    DATA_DIR = os.path.expanduser("~/tech11-data")

DATA_FILE = os.path.join(DATA_DIR, "pending_network_changes.json")


def _load_all():
    os.makedirs(DATA_DIR, exist_ok=True)
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    return {}


def _save_all(data):
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(DATA_FILE, "w") as f:
        json.dump(data, f, indent=2)


def queue_change(mac, payload):
    """Saves (overwriting any previous pending change for this MAC) the
    payload to push next time the device is online. payload uses the same
    keys as devices.update_network() expects (deviceName, useDHCP,
    staticIP, gateway, subnet, dns1, dns2, ntpServer)."""
    data = _load_all()
    data[mac.upper()] = {
        "payload": payload,
        "queued_at": datetime.now().isoformat(timespec="seconds"),
    }
    _save_all(data)


def get_pending_change(mac):
    data = _load_all()
    entry = data.get(mac.upper())
    return entry["payload"] if entry else None


def clear_pending_change(mac):
    data = _load_all()
    if mac.upper() in data:
        del data[mac.upper()]
        _save_all(data)


def has_pending_change(mac):
    return get_pending_change(mac) is not None
