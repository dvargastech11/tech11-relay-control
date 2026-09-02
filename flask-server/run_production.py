"""
Production entrypoint - runs the Flask app under waitress instead of
Flask's built-in development server. This is what the Windows Service
(via NSSM) actually launches.

By default binds to 0.0.0.0 (all network interfaces). To restrict the
server to a single specific IP - e.g. so it's not reachable from every
NIC on a multi-homed server - set the BIND_IP environment variable to
that IP before starting the service:

    Via NSSM (persists across service restarts):
        nssm set Tech11RelayServer AppEnvironmentExtra BIND_IP=192.168.1.50

    Via PowerShell (current session only, for manual testing):
        $env:BIND_IP = "192.168.1.50"
        venv\\Scripts\\python.exe run_production.py

Run manually for testing (uses 0.0.0.0 unless BIND_IP is set):
    venv\\Scripts\\python.exe run_production.py
"""

import os
from waitress import serve
from app import app

if __name__ == "__main__":
    bind_ip = os.environ.get("BIND_IP", "0.0.0.0")
    print(f"Starting server bound to {bind_ip}:5000")
    serve(app, host=bind_ip, port=5000, threads=8)
