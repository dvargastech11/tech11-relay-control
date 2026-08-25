"""
Device configuration store - the SERVER'S canonical record of what each
ESP32's network config SHOULD be. This is the source of truth: if a live
device's reported config differs from what's stored here, that's treated
as drift, and the server's copy wins when resyncing.

Stored OUTSIDE the git repo folder (same reasoning as building_store.py
and users.json before it) so a `git pull` can never touch it, and so
config survives firmware/code updates untouched.

Keyed by MAC address (uppercase, colon-separated) since that's the one
stable identifier that doesn't change even if IP/name change.
"""

import json
import os
import platform
from datetime import datetime

if platform.system() == "Windows":
    DATA_DIR = r"C:\tech11-data"
else:
    DATA_DIR = os.path.expanduser("~/tech11-data")

DATA_FILE = os.path.join(DATA_DIR, "device_configs.json")

# Fields that make up a device's "config" for drift-checking purposes.
# Deliberately excludes things that are expected to vary normally
# (uptimeSec, rssi, firmwareVersion is tracked separately by the
# firmware-update feature, not here).
TRACKED_FIELDS = ["name", "useDHCP", "staticIP", "gateway", "subnet", "dns1", "dns2", "ntpServer"]


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


def get_canonical_config(mac):
    """Returns the server's stored canonical config for this MAC, or None
    if this device has never been backed up before."""
    data = _load_all()
    return data.get(mac.upper())


def backup_config(mac, live_status):
    """Saves (or overwrites) the canonical config for this MAC from a live
    device status response - call this whenever the server has just
    intentionally set a device's config (assignment, rename, resync), so
    the server's own record reflects reality after a known-good push."""
    data = _load_all()
    record = {field: live_status.get(field) for field in TRACKED_FIELDS}
    record["_backed_up_at"] = datetime.now().isoformat(timespec="seconds")
    data[mac.upper()] = record
    _save_all(data)
    return record


def check_drift(mac, live_status):
    """Compares a live device's reported config against the server's
    canonical copy. Returns a dict of {field: (canonical_value, live_value)}
    for every field that differs, or an empty dict if in sync (or if no
    canonical config exists yet, since there's nothing to compare against)."""
    canonical = get_canonical_config(mac)
    if canonical is None:
        return {}

    drift = {}
    for field in TRACKED_FIELDS:
        canonical_value = canonical.get(field)
        live_value = live_status.get(field)
        if canonical_value is not None and live_value is not None and str(canonical_value) != str(live_value):
            drift[field] = (canonical_value, live_value)
    return drift


def list_all_backed_up_macs():
    return list(_load_all().keys())
