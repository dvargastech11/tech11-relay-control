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

REQUEST_TIMEOUT_SEC = 3
HOLD_MS = 5000  # fixed 5 second hold for all floor buttons


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
    # elevator's floor list (1-indexed) - matches how the module's MCP23017
    # channels are wired up in sequence.
    relay_num = elevator["floors"].index(floor) + 1

    success, message = send_relay_command(elevator["device_ip"], elevator.get("device_mac"), relay_num, duration_ms)
    return jsonify({"success": success, "message": message})


# ---- ADMIN: TEST BUTTONS (raw relay test, bypasses floor/schedule config) ----

HARDWARE_CHANNELS_PER_DEVICE = 16  # matches the 16-channel relay board design

@app.route("/admin/test-buttons")
@admin_required
def admin_test_buttons():
    data = bstore.load_data()
    devices_list = []
    for building in data["buildings"]:
        for elevator in building["elevators"]:
            if elevator.get("device_ip"):
                floors = elevator.get("floors") or []

                # Build a 1..16 relay grid: relay N is "configured" if the
                # elevator has a floor at that position, showing its label;
                # anything beyond the configured floor count stays greyed out.
                relay_grid = []
                for relay_num in range(1, HARDWARE_CHANNELS_PER_DEVICE + 1):
                    if relay_num <= len(floors):
                        relay_grid.append({
                            "relay_num": relay_num,
                            "configured": True,
                            "label": floors[relay_num - 1]["label"],
                        })
                    else:
                        relay_grid.append({
                            "relay_num": relay_num,
                            "configured": False,
                            "label": None,
                        })

                devices_list.append({
                    "building_id": building["id"],
                    "building_name": building["name"],
                    "elevator_number": elevator["elevator_number"],
                    "device_name": elevator.get("device_name") or elevator["device_ip"],
                    "device_ip": elevator["device_ip"],
                    "device_mac": elevator.get("device_mac"),
                    "relay_grid": relay_grid,
                    "num_configured": len(floors),
                })
    return render_template(
        "admin_test_buttons.html", active_page="test_buttons",
        devices=devices_list, hardware_channels=HARDWARE_CHANNELS_PER_DEVICE,
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

@app.route("/devices")
@admin_required
def devices_page():
    data = bstore.load_data()

    assigned = []
    assigned_macs = set()
    for building in data["buildings"]:
        for elevator in building["elevators"]:
            if elevator.get("device_mac"):
                assigned_macs.add(elevator["device_mac"].upper())
                online, status = devsvc.poll_status(elevator["device_ip"], elevator["device_mac"])
                assigned.append({
                    "building_name": building["name"],
                    "building_id": building["id"],
                    "elevator_number": elevator["elevator_number"],
                    "name": elevator.get("device_name") or (status.get("name") if status else "Unnamed"),
                    "ip": elevator["device_ip"],
                    "mac": elevator["device_mac"],
                    "online": online,
                    "status": status,
                })

    discovered = devsvc.discover_devices()
    unassigned = [d for d in discovered if d.get("mac", "").upper() not in assigned_macs]

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
        flash("Settings sent - device is rebooting. Refresh the Devices page in a moment to see its new IP."
              if success else "Failed to reach device.")
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

        if not mac or not ip:
            flash("MAC address and IP are both required.")
        else:
            bstore.assign_device(data, building_id, elevator_number, mac, ip, device_name)
            flash(f"Device assigned to elevator {elevator_number}.")
            return redirect(url_for("admin_building_detail", building_id=building_id))

    # Support prefill from the Devices page's "Assign" link (?mac=..&ip=..&name=..)
    # without overwriting what's already saved unless new values were actually passed.
    prefill = {
        "device_mac": request.args.get("mac", elevator.get("device_mac") or ""),
        "device_ip": request.args.get("ip", elevator.get("device_ip") or ""),
        "device_name": request.args.get("name", elevator.get("device_name") or ""),
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
    Platform-aware: Windows Server uses NSSM-managed services (via sc.exe),
    Linux uses systemd.

    IMPORTANT: this runs in a background thread, detached from the request
    that triggered it, and explicitly WAITS for the stop to complete before
    issuing start. Firing 'net stop' and 'net start' back-to-back via
    Popen (no wait) is a real race condition - the start can execute before
    the Service Control Manager has finished tearing down the old process,
    which can leave the service stuck in a 'Paused' or otherwise confused
    state that doesn't respond to a normal Resume/Start."""
    def _do_restart():
        if platform.system() == "Windows":
            subprocess.run(["sc.exe", "stop", SERVICE_NAME], capture_output=True, timeout=15)
            # Poll until the SCM actually reports STOPPED rather than a fixed sleep -
            # more reliable across slower shutdowns (e.g. NSSM waiting on the Python process).
            for _ in range(20):  # up to ~10 seconds
                time.sleep(0.5)
                result = subprocess.run(
                    ["sc.exe", "query", SERVICE_NAME], capture_output=True, text=True, timeout=5
                )
                if "STOPPED" in result.stdout:
                    break
            subprocess.run(["sc.exe", "start", SERVICE_NAME], capture_output=True, timeout=15)
        else:
            subprocess.run(["sudo", "systemctl", "restart", SERVICE_NAME], timeout=15)

    # This request's own process is what's about to be killed by the stop
    # command above, so we don't wait on this thread - just fire it and
    # return a response immediately.
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
