"""
External API config - a dedicated API key for the /api/activate endpoint,
which lets an external system (not a logged-in browser session) trigger a
relay by specifying Building + Elevator + Floor, without needing to know
internal MAC addresses, IPs, or raw relay numbers.

Stored OUTSIDE the git repo folder (same reasoning as the other data
stores).
"""

import json
import os
import platform
import secrets

# TECH11_DATA_DIR lets a test instance point at a separate data
# directory (isolated from production) - falls back to the normal
# location if not set.
_default_data_dir = r"C:\tech11-data" if platform.system() == "Windows" else os.path.expanduser("~/tech11-data")
DATA_DIR = os.environ.get("TECH11_DATA_DIR", _default_data_dir)

DATA_FILE = os.path.join(DATA_DIR, "external_api_config.json")


def _load():
    os.makedirs(DATA_DIR, exist_ok=True)
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    return {"api_key": None}


def _save(config):
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(DATA_FILE, "w") as f:
        json.dump(config, f, indent=2)


def get_api_key():
    return _load().get("api_key")


def generate_new_key():
    """Generates and saves a new random API key, returning it. Overwrites
    any previous key - old integrations using it will need updating."""
    new_key = secrets.token_urlsafe(32)
    _save({"api_key": new_key})
    return new_key


def is_valid_key(provided_key):
    current = get_api_key()
    if not current or not provided_key:
        return False
    return secrets.compare_digest(current, provided_key)
