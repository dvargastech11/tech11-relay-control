"""
Building / Elevator / Floor configuration store.
---------------------------------------------------
Persists to buildings.json OUTSIDE the git repo folder, so a `git pull`
can never overwrite or delete it.

IMPORTANT (Windows): uses a fixed absolute path rather than
os.path.expanduser("~") deliberately. On Windows, "~" resolves to a
different folder depending on which account runs the process - your own
logged-in profile when testing manually (e.g. C:\\Users\\YourName), but
typically C:\\Windows\\system32\\config\\systemprofile when the NSSM
service runs as Local System. Using expanduser() would silently make the
service and manual testing read/write two different files, making config
appear to "reset" for no visible reason. A fixed path avoids that.

Building -> Elevators (unique elevator_number per building) -> Floors
    Each floor: enabled, always_available, schedule (optional)
    Each elevator: assigned device (mac + ip), floor count, skip_13th
"""

import json
import os
import shutil
import platform
from datetime import datetime

# Fixed, explicit path - same location regardless of which account runs
# the process (your interactive login vs. the NSSM service's account).
if platform.system() == "Windows":
    DATA_DIR = r"C:\tech11-data"
else:
    DATA_DIR = os.path.expanduser("~/tech11-data")

DATA_FILE = os.path.join(DATA_DIR, "buildings.json")

# Old location (inside the repo folder) used before this fix - if data
# exists there and nothing exists yet at the new location, migrate it once.
_OLD_DATA_FILE = "buildings.json"

DAY_NAMES = ["MO", "TU", "WE", "TH", "FR", "SA", "SU"]


def _default_data():
    return {
        "buildings": [
            {
                "id": "default-building",
                "name": "Default Building",
                "elevators": [],
            }
        ]
    }


def _migrate_old_data_if_needed():
    os.makedirs(DATA_DIR, exist_ok=True)
    if not os.path.exists(DATA_FILE) and os.path.exists(_OLD_DATA_FILE):
        shutil.copy2(_OLD_DATA_FILE, DATA_FILE)


def load_data():
    _migrate_old_data_if_needed()
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r") as f:
            return json.load(f)
    data = _default_data()
    save_data(data)
    return data


def save_data(data):
    with open(DATA_FILE, "w") as f:
        json.dump(data, f, indent=2)


def get_building(data, building_id):
    return next((b for b in data["buildings"] if b["id"] == building_id), None)


def get_elevator(building, elevator_number):
    return next((e for e in building["elevators"] if e["elevator_number"] == elevator_number), None)


def slugify(name):
    return "".join(c if c.isalnum() else "-" for c in name.lower()).strip("-")


def create_building(data, name, num_elevators):
    """Creates a new building with N placeholder elevators (numbered 1..N by
    default - elevator_number is editable afterward since it must be unique
    per property, not necessarily sequential)."""
    building_id = slugify(name) or f"building-{len(data['buildings']) + 1}"
    # Ensure uniqueness of building id
    base_id = building_id
    counter = 2
    while get_building(data, building_id):
        building_id = f"{base_id}-{counter}"
        counter += 1

    elevators = []
    for i in range(1, num_elevators + 1):
        elevators.append(_new_elevator_skeleton(str(i)))

    building = {"id": building_id, "name": name, "elevators": elevators}
    data["buildings"].append(building)
    save_data(data)
    return building


def rename_building(data, building_id, new_name):
    building = get_building(data, building_id)
    if building:
        building["name"] = new_name
        save_data(data)
    return building


def delete_building(data, building_id):
    """Removes a building entirely (all its elevators/floors go with it).
    No safeguard against deleting the last remaining building - an empty
    building list is a valid state, the home page just shows a
    'no buildings configured' message in that case."""
    building = get_building(data, building_id)
    if not building:
        return False
    data["buildings"] = [b for b in data["buildings"] if b["id"] != building_id]
    save_data(data)
    return True


def add_elevator(data, building_id, elevator_number):
    building = get_building(data, building_id)
    if not building:
        return None
    if get_elevator(building, elevator_number):
        return None  # elevator_number must be unique within the building
    elevator = _new_elevator_skeleton(elevator_number)
    building["elevators"].append(elevator)
    save_data(data)
    return elevator


def _new_elevator_skeleton(elevator_number):
    return {
        "elevator_number": elevator_number,
        "num_floors": 0,
        "skip_13th": True,
        "device_mac": None,
        "device_ip": None,
        "device_name": None,
        "floors": [],  # list of floor dicts, order preserved
    }


def _make_floor(number, label=None):
    return {
        "number": number,
        "label": label or ordinal_label(number),
        "enabled": True,
        "always_available": False,
        "schedule": None,  # {"days": ["MO","TU",...], "start": "08:00", "end": "18:00"}
    }


def ordinal_label(n):
    suffix = "th"
    if n % 100 not in (11, 12, 13):
        suffix = {1: "st", 2: "nd", 3: "rd"}.get(n % 10, "th")
    return f"{n}{suffix}"


def configure_elevator_floors(data, building_id, elevator_number, num_floors, skip_13th):
    """(Re)generates the floor list 2..num_floors+1 (skipping 13th if set).
    Existing per-floor customizations (enabled/schedule) are preserved for
    floor numbers that still exist after reconfiguration; new floors get
    defaults, removed floors are dropped."""
    building = get_building(data, building_id)
    if not building:
        return None
    elevator = get_elevator(building, elevator_number)
    if not elevator:
        return None

    existing_by_number = {f["number"]: f for f in elevator["floors"]}

    new_floor_numbers = []
    floor_num = 2
    count = 0
    while count < num_floors:
        if skip_13th and floor_num == 13:
            floor_num += 1
            continue
        new_floor_numbers.append(floor_num)
        floor_num += 1
        count += 1

    new_floors = []
    for n in new_floor_numbers:
        if n in existing_by_number:
            new_floors.append(existing_by_number[n])
        else:
            new_floors.append(_make_floor(n))

    elevator["num_floors"] = num_floors
    elevator["skip_13th"] = skip_13th
    elevator["floors"] = new_floors
    save_data(data)
    return elevator


def add_custom_floor(data, building_id, elevator_number, label):
    """The '+' button use case: add a one-off named floor/door (e.g. 'Lobby',
    'Mezzanine', 'PH') outside the normal numeric sequence. Uses a synthetic
    negative/offset number internally to keep floor numbers unique without
    colliding with real floor numbers."""
    building = get_building(data, building_id)
    if not building:
        return None
    elevator = get_elevator(building, elevator_number)
    if not elevator:
        return None

    existing_numbers = [f["number"] for f in elevator["floors"]]
    synthetic_number = min(existing_numbers, default=0) - 1  # push custom floors before floor 2
    new_floor = _make_floor(synthetic_number, label=label)
    elevator["floors"].insert(0, new_floor)
    save_data(data)
    return new_floor


def remove_floor(data, building_id, elevator_number, floor_number):
    building = get_building(data, building_id)
    if not building:
        return False
    elevator = get_elevator(building, elevator_number)
    if not elevator:
        return False
    elevator["floors"] = [f for f in elevator["floors"] if f["number"] != floor_number]
    save_data(data)
    return True


def assign_device(data, building_id, elevator_number, mac, ip, device_name):
    building = get_building(data, building_id)
    if not building:
        return None
    elevator = get_elevator(building, elevator_number)
    if not elevator:
        return None
    elevator["device_mac"] = mac
    elevator["device_ip"] = ip
    elevator["device_name"] = device_name
    save_data(data)
    return elevator


def update_floor(data, building_id, elevator_number, floor_number, **kwargs):
    """kwargs may include: enabled (bool), always_available (bool),
    schedule (dict or None), label (str)."""
    building = get_building(data, building_id)
    if not building:
        return None
    elevator = get_elevator(building, elevator_number)
    if not elevator:
        return None
    floor = next((f for f in elevator["floors"] if f["number"] == floor_number), None)
    if not floor:
        return None
    floor.update(kwargs)
    save_data(data)
    return floor


def is_floor_available_now(floor, now=None):
    """A floor button should be shown/enabled if:
    - enabled is True, AND
    - (always_available is True, OR no schedule is set, OR current time falls
      within the configured schedule window)
    """
    if not floor.get("enabled", True):
        return False
    if floor.get("always_available"):
        return True

    schedule = floor.get("schedule")
    if not schedule:
        return True  # no schedule restriction - available whenever enabled

    now = now or datetime.now()
    today_code = DAY_NAMES[now.weekday()]
    if today_code not in schedule.get("days", []):
        return False

    current_time = now.strftime("%H:%M")
    return schedule["start"] <= current_time <= schedule["end"]
