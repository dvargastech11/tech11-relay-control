"""
Tech 11 Relay Control System - Flask Server
---------------------------------------------------------------------------------
Controls elevator modules over HTTP/WiFi. Elevator control pages are fully
public (no login) - the "operator" experience. Admin access (device
management, building/floor config, etc.) requires logging in with a real
Windows account on this server via the "Log in as Admin" button.

Install dependencies first (inside the venv):
    python -m pip install flask requests flask-login werkzeug pywin32

Run with:
    python app.py

Then open http://<server-ip>:5000 in your browser.

ADMIN LOGIN: any local Windows account on this server that is a member of
the local "Administrators" group. Password is validated against Windows
itself (see windows_auth.py) - there is no separate app-level password to
manage or reset; use Windows' own user management (Computer Management >
Local Users and Groups) to add/remove/change passwords for admin accounts.
"""

import os
import ipaddress
import threading
import time
import subprocess
from functools import wraps
import requests
from flask import Flask, jsonify, render_template, request, redirect, url_for, flash
from flask_login import (
    LoginManager, UserMixin, login_user, logout_user,
    login_required, current_user
)
import windows_auth

app = Flask(__name__)
app.secret_key = "CHANGE-THIS-TO-A-RANDOM-SECRET-BEFORE-PRODUCTION"

login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = "login"


class User(UserMixin):
    """A logged-in admin session. Only Windows accounts in the local
    Administrators group can ever reach a successful login (see login()
    below), so role is always 'admin' for any session that exists at all."""
    def __init__(self, username):
        self.id = username
        self.role = "admin"


@login_manager.user_loader
def load_user(username):
    # Re-validating Windows group membership on every request would be
    # expensive; we trust the session once login() has verified it. If an
    # account is removed from Administrators mid-session, they keep access
    # until they log out - acceptable for this deployment's threat model.
    return User(username)


def admin_required(f):
    """Route decorator: must be logged in (which, by construction, always
    means a verified local Windows admin - see login())."""
    @wraps(f)
    @login_required
    def wrapper(*args, **kwargs):
        return f(*args, **kwargs)
    return wrapper


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form.get("username", "")
        password = request.form.get("password", "")

        if not windows_auth.validate_windows_credentials(username, password):
            flash("Invalid Windows username or password.")
        elif not windows_auth.is_local_admin(username):
            flash("That account is valid but is not a member of the local Administrators group.")
        else:
            login_user(User(username))
            next_page = request.args.get("next") or url_for("home")
            return redirect(next_page)

    return render_template("login.html")


@app.route("/logout")
@login_required
def logout():
    logout_user()
    return redirect(url_for("login"))


import building_store as bstore
import devices as devsvc
import ip_acl_store as ipacl
import device_config_store as devcfg
import nxwitness_config as nxcfg
import pending_changes_store as pending

REQUEST_TIMEOUT_SEC = 3
HOLD_MS = 5000  # fixed 5 second hold for all floor buttons


@app.before_request
def enforce_ip_allowlist():
    client_ip = request.remote_addr
    if not ipacl.is_allowed(client_ip):
        return "Access denied.", 403


def send_relay_command(device_ip, device_mac, relay_num, duration_ms):
    """Send an HTTP activate command to an elevator's assigned ESP32 module.

    NOTE: current module firmware expects POST JSON on /trigger, e.g.
        POST http://<ip>/trigger
        Body: {"relay": 1}
    Authenticated via the MAC-derived API key (X-API-Key header), same
    scheme used by devices.py for status/reboot/network calls. It
    currently ignores duration_ms and always holds for a hardcoded
    5000ms server-side.
    """
    if not device_ip:
        return False, "No device assigned to this elevator"
    if not device_mac:
        return False, "No MAC address on file for this elevator's device - re-assign it on the Assign Device page"

    url = f"http://{device_ip}/trigger"
    payload = {"relay": relay_num}
    api_key = devsvc.compute_device_api_key(device_mac)
    headers = {"X-API-Key": api_key}

    try:
        resp = requests.post(url, json=payload, headers=headers, timeout=REQUEST_TIMEOUT_SEC)
        return resp.status_code == 200, resp.text
    except requests.RequestException as e:
        return False, f"Module unreachable: {e}"


# ---- PUBLIC CONTROL ROUTES (no login required - this is the "operator" experience) ----

@app.route("/")
def home():
    data = bstore.load_data()
    buildings = data["buildings"]

    # If there's only one building, skip the list and land directly on its
    # elevator control page - matches "land on the elevator control" intent.
    if len(buildings) == 1:
        return redirect(url_for(
            "building_view",
            building_id=buildings[0]["id"],
            **request.args  # preserves ?operator_view=1 through this redirect
        ))

    return render_template("home.html", active_page="home", buildings=buildings)


@app.route("/building/<building_id>")
def building_view(building_id):
    data = bstore.load_data()
    building = bstore.get_building(data, building_id)
    if not building:
        flash("Building not found.")
        return redirect(url_for("home"))

    # Attach live availability to each floor for rendering (enabled + schedule)
    for elevator in building["elevators"]:
        for floor in elevator["floors"]:
            floor["available_now"] = bstore.is_floor_available_now(floor)

    return render_template(
        "building_control.html",
        active_page="building",
        building=building,
        all_buildings=data["buildings"],
        hold_ms=HOLD_MS,
    )


@app.route("/activate/<building_id>/<elevator_number>/<int:floor_number>/<int:duration_ms>")
def activate(building_id, elevator_number, floor_number, duration_ms):
    data = bstore.load_data()
    building = bstore.get_building(data, building_id)
    if not building:
        return jsonify({"success": False, "error": "Unknown building"}), 404

    elevator = bstore.get_elevator(building, elevator_number)
    if not elevator:
        return jsonify({"success": False, "error": "Unknown elevator"}), 404

    floor = next((f for f in elevator["floors"] if f["number"] == floor_number), None)
    if not floor:
        return jsonify({"success": False, "error": "Unknown floor"}), 404

    if not bstore.is_floor_available_now(floor):
        return jsonify({"success": False, "error": "This floor is not currently available"}), 403

    if duration_ms <= 0:
        return jsonify({"success": False, "error": "Duration must be greater than 0"}), 400

    # Relay number on the physical module = position of this floor in the
    # elevator's floor list, offset by 2 (not 1) - relay 1 is permanently
    # reserved as SWITCH (not tied to any floor, for future use), so the
    # first floor in the list gets relay 2, the second gets relay 3, etc.
    relay_num = elevator["floors"].index(floor) + 2

    success, message = send_relay_command(elevator["device_ip"], elevator.get("device_mac"), relay_num, duration_ms)
    return jsonify({"success": success, "message": message})


# ---- ADMIN: TEST BUTTONS (raw relay test, bypasses floor/schedule config) ----

HARDWARE_CHANNELS_PER_DEVICE = 48  # fallback default only - real count comes from /status/boards when reachable

@app.route("/admin/test-buttons")
@admin_required
def admin_test_buttons():
    data = bstore.load_data()
    devices_list = []
    for building in data["buildings"]:
        for elevator in building["elevators"]:
            if elevator.get("device_ip"):
                floors = elevator.get("floors") or []
                mac = elevator.get("device_mac")
                ip = elevator["device_ip"]

                board_online, board_status = devsvc.poll_board_status(ip, mac)

                if board_online and board_status:
                    hardware_channels = board_status.get("hardwareChannels", HARDWARE_CHANNELS_PER_DEVICE)
                    boards_info = board_status.get("boards", [])
                else:
                    # Device unreachable, or running older firmware without
                    # this endpoint - fall back to the default rather than
                    # showing nothing at all.
                    hardware_channels = HARDWARE_CHANNELS_PER_DEVICE
                    boards_info = []

                # Map each channel number to whether ITS board is actually
                # online right now, so we can grey out a whole board's worth
                # of buttons if that board isn't detected - not just channels
                # beyond the configured floor count.
                board_online_by_channel = {}
                for b in boards_info:
                    online = b.get("online", False)
                    for ch in range(b.get("channelStart", 1), b.get("channelEnd", 16) + 1):
                        board_online_by_channel[ch] = online

                relay_grid = []
                for relay_num in range(1, hardware_channels + 1):
                    is_board_online = board_online_by_channel.get(relay_num, True)

                    if relay_num == 1:
                        # Reserved as SWITCH - not tied to any floor, held
                        # for future use.
                        relay_grid.append({
                            "relay_num": relay_num,
                            "configured": False,
                            "is_switch": True,
                            "board_online": is_board_online,
                            "label": "SWITCH",
                        })
                        continue

                    # Relay 2 = floors[0], relay 3 = floors[1], etc. - relay
                    # 1 being reserved shifts every floor's relay by one.
                    floor_index = relay_num - 2
                    is_floor_configured = 0 <= floor_index < len(floors)
                    relay_grid.append({
                        "relay_num": relay_num,
                        "configured": is_floor_configured,
                        "is_switch": False,
                        "board_online": is_board_online,
                        "label": floors[floor_index]["label"] if is_floor_configured else None,
                    })

                devices_list.append({
                    "building_id": building["id"],
                    "building_name": building["name"],
                    "elevator_number": elevator["elevator_number"],
                    "device_name": elevator.get("device_name") or ip,
                    "device_ip": ip,
                    "device_mac": mac,
                    "relay_grid": relay_grid,
                    "num_configured": len(floors),
                    "hardware_channels": hardware_channels,
                    "boards_detected": len([b for b in boards_info if b.get("online")]),
                    "boards_total": len(boards_info),
                })
    return render_template(
        "admin_test_buttons.html", active_page="test_buttons",
        devices=devices_list,
    )


@app.route("/admin/test-buttons/fire", methods=["POST"])
@admin_required
def admin_test_buttons_fire():
    device_ip = request.form.get("device_ip")
    device_mac = request.form.get("device_mac")
    try:
        relay_num = int(request.form.get("relay_num", "0"))
        duration_ms = int(request.form.get("duration_ms", "5000"))
    except ValueError:
        return jsonify({"success": False, "error": "Invalid relay number or duration"}), 400

    if relay_num < 1:
        return jsonify({"success": False, "error": "Relay number must be 1 or greater"}), 400

    success, message = send_relay_command(device_ip, device_mac, relay_num, duration_ms)
    return jsonify({"success": success, "message": message})


# ---- DEVICES: discovery, online/offline status, reboot, remote IP change ----

def format_uptime(seconds):
    """Formats a raw uptime in seconds as a human-readable string showing
    the two largest relevant units, e.g. '3h 25m', '2d 4h', '45s'."""
    if seconds is None:
        return None
    seconds = int(seconds)

    days, remainder = divmod(seconds, 86400)
    hours, remainder = divmod(remainder, 3600)
    minutes, secs = divmod(remainder, 60)

    if days > 0:
        return f"{days}d {hours}h"
    if hours > 0:
        return f"{hours}h {minutes}m"
    if minutes > 0:
        return f"{minutes}m {secs}s"
    return f"{secs}s"


def get_latest_firmware_version():
    """Reads firmware/version.txt from this Pi's own repo clone (kept in
    sync via the 'Pull Latest Code & Restart' button), so no separate
    network call to GitHub is needed."""
    version_file = os.path.join(REPO_DIR, "firmware", "version.txt")
    try:
        with open(version_file, "r") as f:
            return f.read().strip()
    except (FileNotFoundError, OSError):
        return None


@app.route("/devices")
@admin_required
def devices_page():
    data = bstore.load_data()
    latest_firmware_version = get_latest_firmware_version()

    assigned = []
    assigned_macs = set()
    for building in data["buildings"]:
        for elevator in building["elevators"]:
            if elevator.get("device_mac"):
                assigned_macs.add(elevator["device_mac"].upper())
                online, status = devsvc.poll_status(elevator["device_ip"], elevator["device_mac"])

                device_version = status.get("firmwareVersion") if (online and status) else None
                update_available = bool(
                    online and device_version and latest_firmware_version
                    and device_version != latest_firmware_version
                )
                uptime_formatted = format_uptime(status.get("uptimeSec")) if (online and status) else None

                config_drift = {}
                pending_applied = False
                if online and status:
                    mac = elevator["device_mac"]
                    ip = elevator["device_ip"]

                    # If a change was queued while this device was offline,
                    # apply it now that we can actually reach it.
                    queued_payload = pending.get_pending_change(mac)
                    if queued_payload:
                        if devsvc.update_network(ip, mac, queued_payload):
                            pending.clear_pending_change(mac)
                            existing = devcfg.get_canonical_config(mac) or {}
                            backup_status = {
                                "name": queued_payload.get("deviceName"),
                                "useDHCP": queued_payload.get("useDHCP"),
                                "staticIP": queued_payload.get("staticIP"),
                                "gateway": queued_payload.get("gateway"),
                                "subnet": queued_payload.get("subnet"),
                            }
                            existing.update({k: v for k, v in backup_status.items() if v is not None})
                            devcfg.backup_config(mac, existing)
                            pending_applied = True
                        # If the push fails despite the device appearing online
                        # (rare race), just leave it queued - next poll retries.

                    if devcfg.get_canonical_config(mac) is None:
                        # First time we've ever seen this device - treat its
                        # current live config as the canonical baseline.
                        devcfg.backup_config(mac, status)
                    elif not pending_applied:
                        # Skip drift-checking on the same pass we just applied
                        # a pending change - the device is rebooting to apply
                        # it, so a live status re-check would be stale anyway.
                        config_drift = devcfg.check_drift(mac, status)

                assigned.append({
                    "building_name": building["name"],
                    "building_id": building["id"],
                    "elevator_number": elevator["elevator_number"],
                    "name": elevator.get("device_name") or (status.get("name") if status else "Unnamed"),
                    "ip": elevator["device_ip"],
                    "mac": elevator["device_mac"],
                    "online": online,
                    "status": status,
                    "firmware_version": device_version,
                    "uptime_formatted": uptime_formatted,
                    "update_available": update_available,
                    "pending_change": pending.has_pending_change(elevator["device_mac"]),
                    "pending_applied": pending_applied,
                    "config_drift": config_drift,
                })

    discovered = devsvc.discover_devices()
    unassigned = []
    for d in discovered:
        if d.get("mac", "").upper() in assigned_macs:
            continue
        online, status = devsvc.poll_status(d.get("ip"), d.get("mac"))
        device_version = status.get("firmwareVersion") if (online and status) else None
        needs_update = bool(
            online and device_version and latest_firmware_version
            and device_version != latest_firmware_version
        )
        unassigned.append({
            **d,
            "firmware_version": device_version,
            "needs_update": needs_update,
        })

    # Flat list of elevators needing a device, for the "assign" dropdown on unassigned rows
    needs_device = []
    for building in data["buildings"]:
        for elevator in building["elevators"]:
            if not elevator.get("device_mac"):
                needs_device.append({
                    "building_id": building["id"],
                    "building_name": building["name"],
                    "elevator_number": elevator["elevator_number"],
                })

    return render_template(
        "admin_devices.html", active_page="devices",
        assigned=assigned, unassigned=unassigned, needs_device=needs_device,
        latest_firmware_version=latest_firmware_version,
    )


@app.route("/devices/push-firmware", methods=["GET", "POST"])
@admin_required
def devices_push_firmware():
    data = bstore.load_data()
    all_devices = []
    for building in data["buildings"]:
        for elevator in building["elevators"]:
            if elevator.get("device_ip"):
                all_devices.append({
                    "label": f"{building['name']} - Elevator {elevator['elevator_number']} "
                             f"({elevator.get('device_name') or elevator['device_ip']})",
                    "ip": elevator["device_ip"],
                    "mac": elevator.get("device_mac"),
                })

    if request.method == "POST":
        ip = request.form.get("ip")
        mac = request.form.get("mac")
        firmware_file = request.files.get("firmware")

        if not firmware_file or firmware_file.filename == "":
            flash("Select a .bin file to push.")
            return redirect(url_for("devices_push_firmware"))

        firmware_bytes = firmware_file.read()
        success, message = devsvc.push_firmware(ip, mac, firmware_bytes)

        flash(f"Push {'succeeded' if success else 'failed'}: {message}")
        return redirect(url_for("devices_push_firmware"))

    return render_template(
        "admin_push_firmware.html", active_page="devices", devices=all_devices,
    )


@app.route("/devices/reboot", methods=["POST"])
@admin_required
def devices_reboot():
    ip = request.form.get("ip")
    mac = request.form.get("mac")
    success = devsvc.reboot_device(ip, mac)
    return jsonify({"success": success})


@app.route("/devices/resync", methods=["POST"])
@admin_required
def devices_resync():
    """Pushes the server's canonical config back to a device, overwriting
    whatever it's currently running - the server always wins."""
    ip = request.form.get("ip")
    mac = request.form.get("mac")

    canonical = devcfg.get_canonical_config(mac)
    if not canonical:
        return jsonify({"success": False, "error": "No canonical config on file for this device to restore."})

    payload = {}
    for field in devcfg.TRACKED_FIELDS:
        value = canonical.get(field)
        if value is None:
            continue
        payload["deviceName" if field == "name" else field] = value

    success = devsvc.update_network(ip, mac, payload)

    if success:
        return jsonify({"success": True, "message": "Server config pushed - device is rebooting to apply it."})
    return jsonify({"success": False, "error": "Could not reach device to push the config."})


@app.route("/admin/buildings/<building_id>/elevators/<elevator_number>/unassign-device", methods=["POST"])
@admin_required
def admin_unassign_device(building_id, elevator_number):
    data = bstore.load_data()

    building = bstore.get_building(data, building_id)
    elevator = bstore.get_elevator(building, elevator_number) if building else None
    mac = elevator.get("device_mac") if elevator else None

    success = bstore.unassign_device(data, building_id, elevator_number)

    if success:
        if mac:
            devcfg.delete_config(mac)
            pending.clear_pending_change(mac)
        flash(f"Device unassigned from elevator {elevator_number} and its saved server data "
              f"(remembered name/config) has been wiped. It will show up under Unassigned "
              f"Devices again on next scan if it's still online.")
    else:
        flash("Could not find that elevator to unassign.")
    return redirect(url_for("devices_page"))


@app.route("/admin/ip-acl", methods=["GET", "POST"])
@admin_required
def admin_ip_acl():
    if request.method == "POST":
        action = request.form.get("action")

        if action == "add":
            new_entry = request.form.get("entry", "").strip()
            if new_entry:
                # Validate before saving - reject anything that wouldn't
                # actually parse as an IP or CIDR range, rather than
                # silently storing a typo that then blocks everyone.
                try:
                    if "/" in new_entry:
                        ipaddress.ip_network(new_entry, strict=False)
                    else:
                        ipaddress.ip_address(new_entry)
                except ValueError:
                    flash(f"'{new_entry}' isn't a valid IP address or CIDR range - not added.")
                    return redirect(url_for("admin_ip_acl"))

                ipacl.add_entry(new_entry)

                # Safety check: warn (don't block) if the list is about to
                # start enforcing and the person's OWN current IP isn't on
                # it - a self-lockout risk, but 127.0.0.1 stays available
                # as a recovery path regardless, so this is a warning, not
                # a hard stop.
                current_ip = request.remote_addr
                if not ipacl.is_allowed(current_ip):
                    flash(f"Added {new_entry}. Warning: your current IP ({current_ip}) is NOT "
                          f"on the list - you'll be blocked from the website (except via "
                          f"localhost/RDP) after this. Add your own IP too if that's not intended.")
                else:
                    flash(f"Added {new_entry} to the allowlist.")
            return redirect(url_for("admin_ip_acl"))

        elif action == "remove":
            entry = request.form.get("entry", "").strip()
            ipacl.remove_entry(entry)
            flash(f"Removed {entry} from the allowlist.")
            return redirect(url_for("admin_ip_acl"))

    entries = ipacl.load_acl()
    return render_template(
        "admin_ip_acl.html", active_page="ip_acl",
        entries=entries, current_ip=request.remote_addr,
    )


@app.route("/admin/nxwitness-settings", methods=["GET", "POST"])
@admin_required
def admin_nxwitness_settings():
    if request.method == "POST":
        existing = nxcfg.load_config()

        new_api_key = request.form.get("api_key", "").strip()
        new_password = request.form.get("password", "").strip()

        config = {
            "enabled": request.form.get("enabled") == "on",
            "server_address": request.form.get("server_address", "").strip(),
            "port": int(request.form.get("port") or 7001),
            "use_api_key": request.form.get("use_api_key") == "on",
            # Leaving a secret field blank keeps whatever was saved before,
            # rather than wiping it out - matches the "leave blank to keep
            # current" placeholder text on the form.
            "api_key": new_api_key or existing.get("api_key", ""),
            "username": request.form.get("username", "").strip(),
            "password": new_password or existing.get("password", ""),
            "verify_ssl": request.form.get("verify_ssl") == "on",
        }
        nxcfg.save_config(config)
        flash("Nx Witness settings saved.")
        return redirect(url_for("admin_nxwitness_settings"))

    config = nxcfg.load_config()
    return render_template(
        "admin_nxwitness_settings.html", active_page="nxwitness_settings",
        config=config, is_configured=nxcfg.is_configured(config),
    )


@app.route("/devices/network", methods=["GET", "POST"])
@admin_required
def devices_network():
    ip = request.values.get("ip")
    mac = request.values.get("mac")
    name = request.values.get("name", "")

    if request.method == "POST":
        payload = {
            "deviceName": request.form.get("devname"),
            "useDHCP": request.form.get("mode") == "dhcp",
            "staticIP": request.form.get("new_ip"),
            "gateway": request.form.get("gw"),
            "subnet": request.form.get("sn"),
        }
        success = devsvc.update_network(ip, mac, payload)

        if success:
            # This was an intentional, server-initiated change - update the
            # canonical backup to match, so future drift checks compare
            # against this new state, not the old one.
            backup_status = {
                "name": payload["deviceName"],
                "useDHCP": payload["useDHCP"],
                "staticIP": payload["staticIP"],
                "gateway": payload["gateway"],
                "subnet": payload["subnet"],
            }
            existing = devcfg.get_canonical_config(mac) or {}
            existing.update({k: v for k, v in backup_status.items() if v})
            devcfg.backup_config(mac, existing)
            pending.clear_pending_change(mac)  # in case an older queued change is now superseded
            flash("Settings sent - device is rebooting. Refresh the Devices page in a moment to see its new IP.")
        else:
            # Device unreachable right now - this is a live push, not a
            # queue the device itself checks, so the change would otherwise
            # just be lost. Queue it instead - it gets applied automatically
            # the next time the Devices page sees this device come online.
            pending.queue_change(mac, payload)
            flash("Device is offline right now - change queued, and will be applied automatically "
                  "the next time it's seen online.")
        return redirect(url_for("devices_page"))

    online, status = devsvc.poll_status(ip, mac)
    return render_template(
        "admin_device_network.html",
        ip=ip, mac=mac, name=name, online=online, status=status,
        active_page="devices",
    )


# ---- ADMIN: BUILDING / ELEVATOR / FLOOR SETUP ----

@app.route("/admin/buildings")
@admin_required
def admin_buildings():
    data = bstore.load_data()
    return render_template("admin_buildings.html", active_page="admin_buildings", buildings=data["buildings"])


@app.route("/admin/buildings/new", methods=["GET", "POST"])
@admin_required
def admin_new_building():
    if request.method == "POST":
        name = request.form.get("name", "").strip()
        try:
            num_elevators = int(request.form.get("num_elevators", "0"))
        except ValueError:
            num_elevators = 0

        if not name or num_elevators < 1:
            flash("Building name and a valid number of elevators are required.")
            return render_template("admin_building_form.html")

        data = bstore.load_data()
        building = bstore.create_building(data, name, num_elevators)
        flash(f"Building '{name}' created with {num_elevators} elevator(s). Configure each elevator below.")
        return redirect(url_for("admin_building_detail", building_id=building["id"]))

    return render_template("admin_building_form.html")


@app.route("/admin/buildings/<building_id>", methods=["GET", "POST"])
@admin_required
def admin_building_detail(building_id):
    data = bstore.load_data()
    building = bstore.get_building(data, building_id)
    if not building:
        flash("Building not found.")
        return redirect(url_for("admin_buildings"))

    if request.method == "POST":
        new_name = request.form.get("name", "").strip()
        if new_name:
            bstore.rename_building(data, building_id, new_name)
            flash("Building renamed.")
        return redirect(url_for("admin_building_detail", building_id=building_id))

    return render_template("admin_building_detail.html", building=building, active_page="admin_buildings")


@app.route("/admin/buildings/<building_id>/delete", methods=["POST"])
@admin_required
def admin_delete_building(building_id):
    data = bstore.load_data()
    building = bstore.get_building(data, building_id)
    if not building:
        flash("Building not found.")
        return redirect(url_for("admin_buildings"))

    building_name = building["name"]
    bstore.delete_building(data, building_id)
    flash(f"Building '{building_name}' deleted, along with all its elevators and floors.")
    return redirect(url_for("admin_buildings"))


@app.route("/admin/buildings/<building_id>/elevators/new", methods=["POST"])
@admin_required
def admin_add_elevator(building_id):
    """The '+' button on the building detail page - adds one more elevator.
    Elevator numbers must be unique per property (per building)."""
    elevator_number = request.form.get("elevator_number", "").strip()
    data = bstore.load_data()

    if not elevator_number:
        flash("Elevator number is required.")
    elif bstore.add_elevator(data, building_id, elevator_number) is None:
        flash(f"Elevator number '{elevator_number}' already exists in this building - numbers must be unique.")
    else:
        flash(f"Elevator {elevator_number} added. Configure its floors and assign a device below.")

    return redirect(url_for("admin_building_detail", building_id=building_id))


@app.route("/admin/buildings/<building_id>/elevators/<elevator_number>/setup", methods=["GET", "POST"])
@admin_required
def admin_elevator_setup(building_id, elevator_number):
    data = bstore.load_data()
    building = bstore.get_building(data, building_id)
    if not building:
        flash("Building not found.")
        return redirect(url_for("admin_buildings"))
    elevator = bstore.get_elevator(building, elevator_number)
    if not elevator:
        flash("Elevator not found.")
        return redirect(url_for("admin_building_detail", building_id=building_id))

    if request.method == "POST":
        try:
            num_floors = int(request.form.get("num_floors", "0"))
        except ValueError:
            num_floors = 0
        skip_13th = request.form.get("skip_13th") == "on"

        if num_floors < 1:
            flash("Enter a valid number of floors.")
        else:
            bstore.configure_elevator_floors(data, building_id, elevator_number, num_floors, skip_13th)
            flash(f"Elevator {elevator_number} floors configured ({num_floors} floors, 13th {'skipped' if skip_13th else 'included'}).")
        return redirect(url_for("admin_elevator_floors", building_id=building_id, elevator_number=elevator_number))

    return render_template(
        "admin_elevator_setup.html",
        building=building, elevator=elevator, active_page="admin_buildings"
    )


@app.route("/admin/buildings/<building_id>/elevators/<elevator_number>/assign-device", methods=["GET", "POST"])
@admin_required
def admin_assign_device(building_id, elevator_number):
    data = bstore.load_data()
    building = bstore.get_building(data, building_id)
    elevator = bstore.get_elevator(building, elevator_number) if building else None
    if not building or not elevator:
        flash("Elevator not found.")
        return redirect(url_for("admin_buildings"))

    if request.method == "POST":
        mac = request.form.get("mac", "").strip()
        ip = request.form.get("ip", "").strip()
        device_name = request.form.get("device_name", "").strip()
        external_input_monitoring = request.form.get("external_input_monitoring") == "on"

        if not mac or not ip:
            flash("MAC address and IP are both required.")
        else:
            # Check the device's actual current firmware before assigning -
            # push the latest firmware.bin first if it's outdated, so a
            # device never gets assigned into production still running old
            # code. This causes its own reboot, so we do it before (and
            # instead of, in the same request) the name push below.
            firmware_pushed = False
            online, status = devsvc.poll_status(ip, mac)
            latest_version = get_latest_firmware_version()
            device_version = status.get("firmwareVersion") if (online and status) else None

            if online and device_version and latest_version and device_version != latest_version:
                bin_path = os.path.join(REPO_DIR, "firmware", "firmware.bin")
                try:
                    with open(bin_path, "rb") as f:
                        firmware_bytes = f.read()
                    firmware_pushed, push_message = devsvc.push_firmware(ip, mac, firmware_bytes)
                except (FileNotFoundError, OSError) as e:
                    firmware_pushed = False
                    push_message = f"Could not read local firmware.bin: {e}"

            bstore.assign_device(data, building_id, elevator_number, mac, ip, device_name)
            bstore.set_external_input_monitoring(data, building_id, elevator_number, external_input_monitoring)

            # Push the name to the actual device too - previously this only
            # saved a local label in buildings.json, which is why renaming
            # here never showed up on the ESP32 itself. Sending just
            # {"deviceName": ...} is safe - the firmware preserves every
            # other network setting it already has (see handleApiNetworkSave()).
            # Skipped if we just pushed a firmware update this same request -
            # the device is already rebooting from that, sending another
            # request to it right now would likely just fail or conflict.
            pushed_ok = False
            if device_name and not firmware_pushed:
                pushed_ok = devsvc.update_network(ip, mac, {"deviceName": device_name})
                if pushed_ok:
                    existing = devcfg.get_canonical_config(mac) or {}
                    existing["name"] = device_name
                    devcfg.backup_config(mac, existing)

            if firmware_pushed:
                flash(f"Device assigned to elevator {elevator_number}. It was running outdated firmware "
                      f"({device_version} vs latest {latest_version}) - pushed the update, it's rebooting now. "
                      f"{'Re-save the name once it is back online.' if device_name else ''}")
            elif device_name and not pushed_ok:
                flash(f"Device assigned to elevator {elevator_number}, but couldn't reach it to update "
                      f"its name on-device (it will show the local label here, but not on the device itself "
                      f"until it's back online and reachable, or you rename it again from Devices > Change IP).")
            elif device_name and pushed_ok:
                flash(f"Device assigned to elevator {elevator_number}. Name pushed to the device - it will reboot briefly to apply it.")
            else:
                flash(f"Device assigned to elevator {elevator_number}.")
            return redirect(url_for("admin_building_detail", building_id=building_id))

    # Support prefill from the Devices page's "Assign" link (?mac=..&ip=..&name=..)
    # without overwriting what's already saved unless new values were actually passed.
    prefill = {
        "device_mac": request.args.get("mac", elevator.get("device_mac") or ""),
        "device_ip": request.args.get("ip", elevator.get("device_ip") or ""),
        "device_name": request.args.get("name", elevator.get("device_name") or ""),
        "external_input_monitoring_enabled": elevator.get("external_input_monitoring_enabled", False),
    }

    return render_template(
        "admin_assign_device.html",
        building=building, elevator=prefill, elevator_number=elevator_number,
        active_page="admin_buildings"
    )


@app.route("/admin/buildings/<building_id>/elevators/<elevator_number>/floors")
@admin_required
def admin_elevator_floors(building_id, elevator_number):
    data = bstore.load_data()
    building = bstore.get_building(data, building_id)
    elevator = bstore.get_elevator(building, elevator_number) if building else None
    if not building or not elevator:
        flash("Elevator not found.")
        return redirect(url_for("admin_buildings"))

    return render_template(
        "admin_elevator_floors.html",
        building=building, elevator=elevator, active_page="admin_buildings",
        day_names=bstore.DAY_NAMES,
    )


@app.route("/admin/buildings/<building_id>/elevators/<elevator_number>/floors/add", methods=["POST"])
@admin_required
def admin_add_custom_floor(building_id, elevator_number):
    """The '+' button for naming one-off floors/doors (e.g. Lobby, PH)."""
    label = request.form.get("label", "").strip()
    data = bstore.load_data()

    if label:
        bstore.add_custom_floor(data, building_id, elevator_number, label)
        flash(f"Added floor/door '{label}'.")
    else:
        flash("Enter a name for the floor/door.")

    return redirect(url_for("admin_elevator_floors", building_id=building_id, elevator_number=elevator_number))


@app.route("/admin/buildings/<building_id>/elevators/<elevator_number>/floors/<int:floor_number>/update", methods=["POST"])
@admin_required
def admin_update_floor(building_id, elevator_number, floor_number):
    data = bstore.load_data()

    enabled = request.form.get("enabled") == "on"
    always_available = request.form.get("always_available") == "on"
    label = request.form.get("label", "").strip() or None

    schedule = None
    if request.form.get("schedule_enabled") == "on":
        days = request.form.getlist("days")
        start = request.form.get("start_time", "")
        end = request.form.get("end_time", "")
        if days and start and end:
            schedule = {"days": days, "start": start, "end": end}

    bstore.update_floor(
        data, building_id, elevator_number, floor_number,
        enabled=enabled, always_available=always_available, schedule=schedule,
        **({"label": label} if label else {})
    )
    flash("Floor updated.")
    return redirect(url_for("admin_elevator_floors", building_id=building_id, elevator_number=elevator_number))


@app.route("/admin/buildings/<building_id>/elevators/<elevator_number>/floors/<int:floor_number>/remove", methods=["POST"])
@admin_required
def admin_remove_floor(building_id, elevator_number, floor_number):
    data = bstore.load_data()
    bstore.remove_floor(data, building_id, elevator_number, floor_number)
    flash("Floor removed.")
    return redirect(url_for("admin_elevator_floors", building_id=building_id, elevator_number=elevator_number))


# ---- OTHER ADMIN-ONLY ROUTES (devices, Git pull, reboot) ----

import platform

# Path to this repo's checkout on the server, and the service name that
# runs it. Update these to match your actual deployment.
#   Windows: REPO_DIR = r"C:\tech11-relay-control", SERVICE_NAME = the NSSM
#            service name you used (e.g. "Tech11RelayServer")
#   Linux:   REPO_DIR = "/home/admin/tech11-relay-control",
#            SERVICE_NAME = "vyzcayne-elevator.service"
REPO_DIR = r"C:\tech11-relay-control"
SERVICE_NAME = "Tech11RelayServer"


def _restart_service():
    """Restarts the running service so a git pull's changes take effect.

    IMPORTANT (Windows): this does NOT run 'sc.exe stop' followed by
    'sc.exe start' from within this process. That was tried and is
    fundamentally broken: the stop command kills THIS process (the one
    running the background thread that's supposed to issue the start
    command next), so the start never runs and the service just stays
    stopped. Instead, NSSM is configured with 'AppExit Default Restart'
    (set in setup_windows.ps1) - NSSM itself watches for this process
    exiting and restarts it automatically, from OUTSIDE this process,
    where it can't get killed mid-sequence. So on Windows we just exit
    cleanly and let NSSM handle the rest.

    Linux still uses systemctl restart, which is atomic (systemd handles
    stop+start as one external operation, not two sequential ones issued
    by the dying process itself), so that approach is fine as-is."""
    def _do_restart():
        time.sleep(1.5)  # give the HTTP response time to actually flush back to the browser first
        if platform.system() == "Windows":
            os._exit(0)  # NSSM's AppExit=Restart policy brings it back up
        else:
            subprocess.run(["sudo", "systemctl", "restart", SERVICE_NAME], timeout=15)

    # Runs in a background thread so the HTTP response can be sent back
    # before this process exits (Windows) or gets restarted (Linux).
    threading.Thread(target=_do_restart, daemon=True).start()


@app.route("/admin/update-from-git", methods=["POST"])
@admin_required
def update_from_git():
    try:
        pull_result = subprocess.run(
            ["git", "-C", REPO_DIR, "pull"],
            capture_output=True, text=True, timeout=30
        )
        if pull_result.returncode != 0:
            return jsonify({"success": False, "output": pull_result.stderr}), 500

        # The restart kills this very process mid-response on some systems -
        # that's expected. The service manager (NSSM on Windows, systemd on
        # Linux) brings it back up automatically within a second or two.
        _restart_service()

        return jsonify({
            "success": True,
            "output": pull_result.stdout,
            "message": "Pulled changes, restarting service..."
        })
    except subprocess.TimeoutExpired:
        return jsonify({"success": False, "error": "git pull timed out"}), 500
    except FileNotFoundError:
        return jsonify({"success": False, "error": "git is not installed or not on PATH"}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
