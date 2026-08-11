"""
Device discovery and remote control helpers.
------------------------------------------------
Talks to Tech 11 controller modules using the MAC-derived API key
(HMAC-SHA256 of the MAC address, keyed by MASTER_SECRET) rather than
the human admin password - this is machine-to-machine authentication,
matching the ESP32 firmware's checkApiKey()/computeDeviceApiKey().
"""

import socket
import json
import time
import hmac
import hashlib
import requests

# Must match MASTER_SECRET baked into the ESP32 firmware exactly.
MASTER_SECRET = "Tech11-Master-Secret-ChangeThisBeforeProduction-2026"

DISCOVERY_MESSAGE = b"TECH11_DISCOVER"
DISCOVERY_PORT = 4210
DISCOVERY_TIMEOUT_SEC = 3
REQUEST_TIMEOUT_SEC = 2


def compute_device_api_key(mac_address):
    normalized_mac = mac_address.upper()
    return hmac.new(MASTER_SECRET.encode(), normalized_mac.encode(), hashlib.sha256).hexdigest()


def _get_broadcast_addresses():
    """Returns directed broadcast addresses for every active IPv4 interface
    on this Pi (e.g. the wired LAN's and the WiFi hotspot's), by parsing
    `ip -4 -o addr show`. Falls back to the generic 255.255.255.255 alone
    if this can't be determined - a plain broadcast to that address often
    only reaches ONE interface's subnet on a multi-homed machine (usually
    whichever one is the default route), which is why a device on the
    other interface (e.g. the WiFi AP subnet) can go undiscovered."""
    import subprocess
    import ipaddress

    broadcasts = set()
    try:
        output = subprocess.check_output(
            ["ip", "-4", "-o", "addr", "show"], text=True, timeout=2
        )
        for line in output.splitlines():
            parts = line.split()
            for i, token in enumerate(parts):
                if "/" in token and token.count(".") == 3:
                    try:
                        iface = ipaddress.ip_interface(token)
                        if not iface.ip.is_loopback:
                            broadcasts.add(str(iface.network.broadcast_address))
                    except ValueError:
                        continue
    except (subprocess.SubprocessError, FileNotFoundError, OSError):
        pass

    broadcasts.add("255.255.255.255")  # always include as a fallback
    return list(broadcasts)


def discover_devices(timeout=DISCOVERY_TIMEOUT_SEC):
    """Broadcasts a UDP discovery request on every active interface's subnet
    and collects replies. Returns a list of {"name", "ip", "mac"} dicts for
    every module that responded."""
    devices = []
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(timeout)

    try:
        for broadcast_addr in _get_broadcast_addresses():
            try:
                sock.sendto(DISCOVERY_MESSAGE, (broadcast_addr, DISCOVERY_PORT))
            except OSError:
                continue  # e.g. a stale/invalid broadcast address - skip it

        seen_macs = set()
        start = time.time()
        while time.time() - start < timeout:
            try:
                data, addr = sock.recvfrom(1024)
                parsed = json.loads(data.decode("utf-8"))
                mac = parsed.get("mac", "").upper()
                if mac and mac not in seen_macs:
                    seen_macs.add(mac)
                    devices.append(parsed)
            except socket.timeout:
                break
            except (json.JSONDecodeError, UnicodeDecodeError):
                continue
    finally:
        sock.close()

    return devices


def poll_status(ip, mac):
    """Returns (is_online: bool, status_dict_or_None)."""
    if not ip or not mac:
        return False, None
    api_key = compute_device_api_key(mac)
    try:
        resp = requests.get(
            f"http://{ip}/status",
            headers={"X-API-Key": api_key},
            timeout=REQUEST_TIMEOUT_SEC,
        )
        if resp.status_code == 200:
            return True, resp.json()
    except requests.RequestException:
        pass
    return False, None


def reboot_device(ip, mac):
    if not ip or not mac:
        return False
    api_key = compute_device_api_key(mac)
    try:
        resp = requests.post(
            f"http://{ip}/reboot",
            headers={"X-API-Key": api_key},
            timeout=REQUEST_TIMEOUT_SEC,
        )
        return resp.status_code == 200
    except requests.RequestException:
        return False


def update_network(ip, mac, payload):
    """payload: {"deviceName", "useDHCP", "staticIP", "gateway", "subnet"}"""
    if not ip or not mac:
        return False
    api_key = compute_device_api_key(mac)
    try:
        resp = requests.post(
            f"http://{ip}/network/api-save",
            json=payload,
            headers={"X-API-Key": api_key},
            timeout=REQUEST_TIMEOUT_SEC,
        )
        return resp.status_code == 200
    except requests.RequestException:
        return False
