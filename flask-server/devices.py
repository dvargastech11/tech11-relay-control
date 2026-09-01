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
DISCOVERY_TIMEOUT_SEC = 5
DISCOVERY_BROADCAST_ROUNDS = 3  # resend the broadcast a few times - UDP broadcast has
                                 # no delivery guarantee, and a single lost packet means
                                 # that device never even hears the request at all
DISCOVERY_ROUND_INTERVAL_SEC = 1
REQUEST_TIMEOUT_SEC = 2


def compute_device_api_key(mac_address):
    normalized_mac = mac_address.upper()
    return hmac.new(MASTER_SECRET.encode(), normalized_mac.encode(), hashlib.sha256).hexdigest()


def _get_broadcast_addresses():
    """Returns directed broadcast addresses for every active IPv4 interface
    on this machine (e.g. the wired LAN's and any WiFi AP subnet), using
    psutil for cross-platform interface enumeration (works on Windows and
    Linux - the previous version shelled out to Linux's `ip` command, which
    doesn't exist on Windows Server). Falls back to the generic
    255.255.255.255 alone if this can't be determined - a plain broadcast
    to that address often only reaches ONE interface's subnet on a
    multi-homed machine (usually whichever one is the default route),
    which is why a device on another interface can go undiscovered."""
    import ipaddress
    import psutil

    broadcasts = set()
    try:
        for interfaces in psutil.net_if_addrs().values():
            for addr in interfaces:
                if addr.family.name != "AF_INET":  # IPv4 only
                    continue
                if not addr.address or not addr.netmask:
                    continue
                try:
                    iface = ipaddress.ip_interface(f"{addr.address}/{addr.netmask}")
                    if not iface.ip.is_loopback:
                        broadcasts.add(str(iface.network.broadcast_address))
                except ValueError:
                    continue
    except Exception:
        pass

    broadcasts.add("255.255.255.255")  # always include as a fallback
    return list(broadcasts)


def discover_devices(timeout=DISCOVERY_TIMEOUT_SEC):
    """Broadcasts a UDP discovery request on every active interface's subnet
    (several times, since UDP broadcast has no delivery guarantee) and
    collects replies. Returns a list of {"name", "ip", "mac"} dicts for
    every module that responded."""
    devices = []
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(0.5)  # short per-recv timeout so we can interleave re-broadcasts

    broadcast_addrs = _get_broadcast_addresses()
    seen_macs = set()

    def _send_round():
        for broadcast_addr in broadcast_addrs:
            try:
                sock.sendto(DISCOVERY_MESSAGE, (broadcast_addr, DISCOVERY_PORT))
            except OSError:
                continue  # e.g. a stale/invalid broadcast address - skip it

    try:
        _send_round()
        start = time.time()
        next_round_at = start + DISCOVERY_ROUND_INTERVAL_SEC
        rounds_sent = 1

        while time.time() - start < timeout:
            if rounds_sent < DISCOVERY_BROADCAST_ROUNDS and time.time() >= next_round_at:
                _send_round()
                rounds_sent += 1
                next_round_at = time.time() + DISCOVERY_ROUND_INTERVAL_SEC

            try:
                data, addr = sock.recvfrom(1024)
                parsed = json.loads(data.decode("utf-8"))
                mac = parsed.get("mac", "").upper()
                if mac and mac not in seen_macs:
                    seen_macs.add(mac)
                    devices.append(parsed)
            except socket.timeout:
                continue  # just means no packet arrived in this 0.5s slice - keep looping
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


def poll_board_status(ip, mac):
    """Returns (is_online: bool, board_status_dict_or_None) from /status/boards -
    real detected MCP23017 board count/online state, not an assumed value."""
    if not ip or not mac:
        return False, None
    api_key = compute_device_api_key(mac)
    try:
        resp = requests.get(
            f"http://{ip}/status/boards",
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


def push_firmware(ip, mac, firmware_bytes, timeout=60):
    """Pushes a compiled .bin directly to a device's /update/push endpoint.
    This is separate from the device's own GitHub self-update check - this
    is initiated FROM the Pi on demand, e.g. after building new firmware.
    Returns (success: bool, message: str)."""
    if not ip or not mac:
        return False, "Missing IP or MAC for this device"

    api_key = compute_device_api_key(mac)
    try:
        resp = requests.post(
            f"http://{ip}/update/push",
            files={"firmware": ("firmware.bin", firmware_bytes, "application/octet-stream")},
            headers={"X-API-Key": api_key},
            timeout=timeout,  # flashing takes longer than a normal request
        )
        return resp.status_code == 200, resp.text
    except requests.RequestException as e:
        return False, f"Device unreachable: {e}"
