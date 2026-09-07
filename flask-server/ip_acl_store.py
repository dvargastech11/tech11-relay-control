"""
IP allowlist - controls which client IPs are allowed to connect to this
server at all. Different from BIND_IP (which controls which local
interface the server listens ON) - this controls which remote clients are
allowed to connect TO it.

Stored OUTSIDE the git repo folder (same reasoning as the other data
stores), so it survives code updates and isn't accidentally committed.

SAFETY: an empty list means UNRESTRICTED (everyone allowed) - this is the
default, so a fresh install never accidentally locks everyone out before
anyone's had a chance to configure it. Once you add at least one entry,
enforcement begins. 127.0.0.1/localhost is ALWAYS allowed regardless of
the list, as a recovery path if you ever lock yourself out remotely (RDP
into the server and browse to localhost to fix the list).

Supports both exact IPs ("192.168.1.50") and CIDR ranges
("192.168.1.0/24").
"""

import ipaddress
import json
import os
import platform

if platform.system() == "Windows":
    DATA_DIR = r"C:\tech11-data"
else:
    DATA_DIR = os.path.expanduser("~/tech11-data")

DATA_FILE = os.path.join(DATA_DIR, "ip_acl.json")

ALWAYS_ALLOWED = ["127.0.0.1", "::1"]  # localhost - recovery path, never blocked


def load_acl():
    os.makedirs(DATA_DIR, exist_ok=True)
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    return []


def save_acl(entries):
    os.makedirs(DATA_DIR, exist_ok=True)
    with open(DATA_FILE, "w") as f:
        json.dump(entries, f, indent=2)


def add_entry(entry):
    entries = load_acl()
    entry = entry.strip()
    if entry and entry not in entries:
        entries.append(entry)
        save_acl(entries)
    return entries


def remove_entry(entry):
    entries = load_acl()
    entries = [e for e in entries if e != entry]
    save_acl(entries)
    return entries


def is_allowed(client_ip):
    """True if this client IP should be let through."""
    if client_ip in ALWAYS_ALLOWED:
        return True

    entries = load_acl()
    if not entries:
        return True  # empty list = unrestricted (see module docstring)

    try:
        addr = ipaddress.ip_address(client_ip)
    except ValueError:
        return False  # malformed/unparseable address - fail closed

    for entry in entries:
        try:
            if "/" in entry:
                if addr in ipaddress.ip_network(entry, strict=False):
                    return True
            else:
                if addr == ipaddress.ip_address(entry):
                    return True
        except ValueError:
            continue  # skip malformed entries in the stored list rather than erroring

    return False
