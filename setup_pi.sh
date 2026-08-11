#!/bin/bash
# ============================================================
# Tech 11 Relay Control System - Pi Setup Script
# ------------------------------------------------------------
# Run this on the Raspberry Pi as the user that should own the
# service (e.g. "admin"). Safe to re-run - it checks state before
# acting rather than assuming a clean slate.
#
# Usage:
#   chmod +x setup_pi.sh
#   ./setup_pi.sh
# ============================================================

set -e  # stop on first real error

REPO_URL="https://github.com/dvargastech11/tech11-relay-control.git"
INSTALL_DIR="$HOME/tech11-relay-control"
SERVICE_NAME="vyzcayne-elevator.service"
CURRENT_USER=$(whoami)

echo "=============================================="
echo " Tech 11 Relay Control System - Pi Setup"
echo "=============================================="
echo "Installing as user: $CURRENT_USER"
echo "Target directory:   $INSTALL_DIR"
echo ""

# ---- 1. Install git if missing ----
if ! command -v git &> /dev/null; then
    echo "[1/7] git not found - installing..."
    sudo apt update
    sudo apt install -y git
else
    echo "[1/7] git already installed - skipping."
fi

# ---- 2. Stop the service if it's already running (safe to re-run) ----
if systemctl list-unit-files | grep -q "$SERVICE_NAME"; then
    echo "[2/7] Stopping existing service (if running)..."
    sudo systemctl stop "$SERVICE_NAME" || true
else
    echo "[2/7] No existing service found - skipping stop."
fi

# ---- 3. Clone or update the repo ----
if [ -d "$INSTALL_DIR/.git" ]; then
    echo "[3/7] Repo already exists at $INSTALL_DIR - pulling latest..."
    git -C "$INSTALL_DIR" pull
else
    echo "[3/7] Cloning repo into $INSTALL_DIR..."
    git clone "$REPO_URL" "$INSTALL_DIR"
fi

# ---- 4. Preserve runtime data from a prior install, if present ----
for OLD_DIR in "$HOME/relay_web_poc" "$HOME/relay_web_poc_backup"; do
    if [ -f "$OLD_DIR/users.json" ]; then
        echo "[4/7] Found existing users.json in $OLD_DIR - copying it over..."
        cp "$OLD_DIR/users.json" "$INSTALL_DIR/flask-server/"
    fi
    if [ -f "$OLD_DIR/buildings.json" ]; then
        echo "[4/7] Found existing buildings.json in $OLD_DIR - copying it over..."
        cp "$OLD_DIR/buildings.json" "$INSTALL_DIR/flask-server/"
    fi
done
echo "[4/7] Runtime data check complete."

# ---- 5. Set up the Python virtual environment ----
cd "$INSTALL_DIR/flask-server"
if [ ! -d "venv" ]; then
    echo "[5/7] Creating virtual environment..."
    python3 -m venv venv
else
    echo "[5/7] Virtual environment already exists - skipping creation."
fi

echo "      Installing/updating Python dependencies..."
"$INSTALL_DIR/flask-server/venv/bin/python" -m pip install --upgrade pip --quiet
"$INSTALL_DIR/flask-server/venv/bin/python" -m pip install flask requests flask-login werkzeug --quiet

# ---- 6. Write/update the systemd service file ----
echo "[6/7] Writing systemd service file..."
SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME"

sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=Tech 11 Relay Control Flask Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$CURRENT_USER
WorkingDirectory=$INSTALL_DIR/flask-server
ExecStart=$INSTALL_DIR/flask-server/venv/bin/python $INSTALL_DIR/flask-server/app.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"

# ---- 7. Grant passwordless sudo for the restart command only ----
# This is required for the "Pull Latest Code & Restart" button on the
# Devices page to work, since Flask runs as a normal user, not root.
SUDOERS_LINE="$CURRENT_USER ALL=(ALL) NOPASSWD: /bin/systemctl restart $SERVICE_NAME"
SUDOERS_FILE="/etc/sudoers.d/tech11-relay-restart"

if [ ! -f "$SUDOERS_FILE" ]; then
    echo "[7/7] Granting passwordless restart permission..."
    echo "$SUDOERS_LINE" | sudo tee "$SUDOERS_FILE" > /dev/null
    sudo chmod 440 "$SUDOERS_FILE"
else
    echo "[7/7] Passwordless restart permission already configured - skipping."
fi

# ---- Start the service ----
echo ""
echo "Starting service..."
sudo systemctl restart "$SERVICE_NAME"
sleep 2
sudo systemctl status "$SERVICE_NAME" --no-pager

echo ""
echo "=============================================="
echo " Setup complete."
echo " App directory: $INSTALL_DIR/flask-server"
echo " Check REPO_DIR in app.py matches: $INSTALL_DIR"
echo " Visit: http://$(hostname -I | awk '{print $1}'):5000"
echo "=============================================="
