"""
Nx Witness connection settings - configured through the Settings page,
not hardcoded. Stored OUTSIDE the git repo folder (same reasoning as
buildings.json and device_configs.json before it), so a `git pull` never
touches it and it survives code updates untouched.

Prefers API KEY authentication (generated per-user in Nx Witness's System
Administration, revocable independently of the account password) over
username/password. Username/password is kept as a fallback for older Nx
Witness versions that don't yet support API keys.
"""

import json
import os
import platform

if platform.system() == "Windows":
    DATA_DIR = r"C:\tech11-data"
else:
    DATA_DIR = os.path.expanduser("~/tech11-data")

DATA_FILE = os.path.join(DATA_DIR, "nxwitness_config.json")

DEFAULT_CONFIG = {
    "enabled": False,       # global on/off switch for the whole Nx Witness integration
    "server_address": "",   # e.g. "192.168.1.50" or "nxserver.local"
    "port": 7001,
    "use_api_key": True,    # preferred - if False, falls back to username/password below
    "api_key": "",
    "username": "",
    "password": "",
    "verify_ssl": False,    # most on-prem Nx Witness servers use a self-signed cert
}


def load_config():
    os.makedirs(DATA_DIR, exist_ok=True)
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r") as f:
            saved = json.load(f)
        merged = dict(DEFAULT_CONFIG)
        merged.update(saved)
        return merged
    return dict(DEFAULT_CONFIG)


def save_config(config):
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(DATA_FILE, "w") as f:
        json.dump(config, f, indent=2)


def is_configured(config=None):
    """True if enough info is present to actually attempt a connection -
    doesn't verify credentials are correct, just that fields are filled in."""
    config = config or load_config()
    if not config.get("enabled") or not config.get("server_address"):
        return False
    if config.get("use_api_key"):
        return bool(config.get("api_key"))
    return bool(config.get("username")) and bool(config.get("password"))
