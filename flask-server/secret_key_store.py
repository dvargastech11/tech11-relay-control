"""
Flask session secret key. Replaces the old hardcoded placeholder.

Order of precedence:
  1. SECRET_KEY environment variable, if set (e.g. via NSSM
     AppEnvironmentExtra), so the key can be managed outside the server.
  2. A random key generated once and saved in the data directory
     (secret_key.txt). Same pattern as the other stores: lives outside the
     git repo, survives code updates, and a test instance using its own
     TECH11_DATA_DIR gets its own separate key.

Changing the key logs everyone out once (existing sessions stop validating).
"""

import os
import platform
import secrets

_default_data_dir = r"C:\tech11-data" if platform.system() == "Windows" else os.path.expanduser("~/tech11-data")
DATA_DIR = os.environ.get("TECH11_DATA_DIR", _default_data_dir)
KEY_FILE = os.path.join(DATA_DIR, "secret_key.txt")


def get_secret_key():
    env_key = os.environ.get("SECRET_KEY", "").strip()
    if env_key:
        return env_key

    os.makedirs(DATA_DIR, exist_ok=True)
    if os.path.exists(KEY_FILE):
        with open(KEY_FILE, "r") as f:
            saved = f.read().strip()
        if saved:
            return saved

    new_key = secrets.token_hex(32)
    with open(KEY_FILE, "w") as f:
        f.write(new_key)
    return new_key
