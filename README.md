# Tech 11 Relay Control System

Web-based relay control system for remotely triggering elevator floor-call
buttons (or any NO-relay use case) across multiple buildings, elevators,
and floors.

## Structure

- `flask-server/` - Flask web application (runs on a Raspberry Pi or any
  always-on machine). Handles building/elevator/floor configuration, user
  login (admin/operator roles), device discovery, and relay activation.
- `firmware/` - ESP32 controller firmware. Each physical elevator gets one
  ESP32 module driving its floor-call relays.
- `tools/` - Standalone utilities (e.g. Windows device discovery app).

## Flask server setup

```bash
cd flask-server
python -m venv venv
source venv/bin/activate      # or venv\Scripts\activate on Windows
python -m pip install flask requests flask-login werkzeug
python app.py
```

Then open `http://<server-ip>:5000` in a browser.

**Default logins** (forced password change on first login):
- `admin` / `T1123456` - full access
- `operator` / `T1123456` - floor buttons only

## ESP32 firmware setup

Open the relevant `.ino` file in Arduino IDE, update WiFi credentials and
any device-specific config, then flash to each elevator's controller module.

## Configuration flow

1. Log in as `admin`, go to **Manage Buildings**
2. Rename the default building or add a new one (specify elevator count)
3. For each elevator: configure floor count (+ skip-13th checkbox), assign
   a controller device (MAC/IP), and manage individual floor
   availability/scheduling under **Manage Floors**
4. Use the **Devices** page to discover controllers on the network, check
   online/offline status, reboot remotely, or change a device's IP

## Security notes (read before production use)

- Change `app.secret_key` in `app.py` to a random value
- Change `MASTER_SECRET` in `devices.py` (must match the value baked into
  ESP32 firmware) before deploying beyond testing
- `users.json` and `buildings.json` are excluded from version control since
  they contain site-specific data and password hashes - back them up
  separately if needed
