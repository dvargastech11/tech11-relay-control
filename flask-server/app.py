"""
Tech 11 Relay Control System - Flask Server (single Pi, both towers)
---------------------------------------------------------------------------------
This Pi lives in the MDF and controls all 12 elevator modules (6 in NT tower,
6 in ST tower) over the building's network backbone via HTTP/WiFi.

Install dependencies first (inside the venv):
    python -m pip install flask requests flask-login werkzeug

Run with:
    python app.py

Then open http://<this-pi-ip>:5000 in your browser.

DEFAULT LOGINS (must be changed on first login - enforced automatically):
    admin    / T1123456   - full access (devices, config, Git pull, reboot)
    operator / T1123456   - floor buttons only
"""

import json
import os
import shutil
import subprocess
from functools import wraps
import requests
from flask import Flask, jsonify, render_template, request, redirect, url_for, flash
from flask_login import (
    LoginManager, UserMixin, login_user, logout_user,
    login_required, current_user
)
from werkzeug.security import generate_password_hash, check_password_hash

app = Flask(__name__)
app.secret_key = "CHANGE-THIS-TO-A-RANDOM-SECRET-BEFORE-PRODUCTION"

login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = "login"

# ---- USER STORE (persisted to disk so password changes survive restarts) ----
# Stored OUTSIDE the git repo folder, same reasoning as buildings.json in
# building_store.py - a `git pull` can never touch this location.
_DATA_DIR = os.path.expanduser("~/tech11-data")
USERS_FILE = os.path.join(_DATA_DIR, "users.json")
_OLD_USERS_FILE = "users.json"  # previous in-repo location, migrated once below

DEFAULT_USERS = {
    "admin": {
        "password_hash": generate_password_hash("T1123456"),
        "role": "admin",
        "must_change_password": True,
    },
    "operator": {
        "password_hash": generate_password_hash("T1123456"),
        "role": "operator",
        "must_change_password": True,
    },
}


def load_users():
    os.makedirs(_DATA_DIR, exist_ok=True)
    if not os.path.exists(USERS_FILE) and os.path.exists(_OLD_USERS_FILE):
        shutil.copy2(_OLD_USERS_FILE, USERS_FILE)  # one-time migration

    if os.path.exists(USERS_FILE):
        with open(USERS_FILE, "r") as f:
            return json.load(f)
    save_users(DEFAULT_USERS)
    return DEFAULT_USERS


def save_users(users_dict):
    os.makedirs(_DATA_DIR, exist_ok=True)
    with open(USERS_FILE, "w") as f:
        json.dump(users_dict, f, indent=2)


USERS = load_users()


class User(UserMixin):
    def __init__(self, username, role, must_change_password):
        self.id = username
        self.role = role
        self.must_change_password = must_change_password


@login_manager.user_loader
def load_user(username):
    record = USERS.get(username)
    if not record:
        return None
    return User(username, record["role"], record["must_change_password"])


def admin_required(f):
    """Route decorator: must be logged in AND have the admin role."""
    @wraps(f)
    @login_required
    def wrapper(*args, **kwargs):
        if current_user.role != "admin":
            return jsonify({"success": False, "error": "Admin access required"}), 403
        return f(*args, **kwargs)
    return wrapper


@app.before_request
def enforce_password_change():
    """Redirect any authenticated request to /change-password until the
    user has replaced their default password - applies to both roles."""
    if not current_user.is_authenticated:
        return
    allowed_endpoints = {"change_password", "logout", "static"}
    if current_user.must_change_password and request.endpoint not in allowed_endpoints:
        return redirect(url_for("change_password"))


@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form.get("username", "")
        password = request.form.get("password", "")
        record = USERS.get(username)

        if record and check_password_hash(record["password_hash"], password):
            login_user(User(username, record["role"], record["must_change_password"]))
            next_page = request.args.get("next") or url_for("home")
            return redirect(next_page)

        flash("Invalid username or password.")

    return render_template("login.html")


@app.route("/logout")
@login_required
def logout():
    logout_user()
    return redirect(url_for("login"))


@app.route("/change-password", methods=["GET", "POST"])
@login_required
def change_password():
    if request.method == "POST":
        new_password = request.form.get("newpass", "")
        confirm_password = request.form.get("confirmpass", "")

        if len(new_password) < 8:
            flash("Password must be at least 8 characters.")
        elif new_password != confirm_password:
            flash("Passwords do not match.")
        else:
            USERS[current_user.id]["password_hash"] = generate_password_hash(new_password)
            USERS[current_user.id]["must_change_password"] = False
            save_users(USERS)
            current_user.must_change_password = False
            flash("Password updated.")
            return redirect(url_for("home"))

    return render_template(
        "change_password.html",
        forced=current_user.must_change_password,
    )


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


# ---- PUBLIC CONTROL ROUTES (both admin and operator) ----

@app.route("/")
@login_required
def home():
    data = bstore.load_data()
    return render_template("home.html", active_page="home", buildings=data["buildings"])


@app.route("/building/<building_id>")
@login_required
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
@login_required
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

# Path to this repo's flask-server folder on the Pi, and the systemd
# service name that runs it. Update these to match your actual deployment.
REPO_DIR = "/home/admin/tech11-relay-control"
SERVICE_NAME = "vyzcayne-elevator.service"


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
        # that's expected. systemd's Restart=on-failure brings it back up
        # automatically within a second or two.
        subprocess.Popen(["sudo", "systemctl", "restart", SERVICE_NAME])

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
