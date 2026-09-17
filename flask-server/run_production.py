"""
Production entrypoint - runs the Flask app under waitress instead of
Flask's built-in development server. This is what the Windows Service
(via NSSM) actually launches.

By default binds to 0.0.0.0 (all network interfaces) on port 5000. Two
environment variables let you run a separate TEST instance alongside
production on the same machine, fully isolated:

    BIND_IP           - restrict which interface the server listens on
    PORT              - which port to listen on (default 5000)
    TECH11_DATA_DIR   - where persistent data lives (buildings.json, device
                        configs, etc.) - set this to a SEPARATE directory
                        for a test instance so it never touches production
                        data (see building_store.py and friends)

    Via NSSM (persists across service restarts):
        nssm set Tech11RelayServerTest AppEnvironmentExtra "PORT=5001`nTECH11_DATA_DIR=C:\tech11-data-test"

    Via PowerShell (current session only, for manual testing):
        $env:PORT = "5001"
        $env:TECH11_DATA_DIR = "C:\tech11-data-test"
        venv\\Scripts\\python.exe run_production.py

Run manually for testing (uses 0.0.0.0:5000 and the normal data dir unless overridden):
    venv\\Scripts\\python.exe run_production.py
"""

import os
from waitress import serve
from app import app

if __name__ == "__main__":
    bind_ip = os.environ.get("BIND_IP", "0.0.0.0")
    port = int(os.environ.get("PORT", "5000"))
    print(f"Starting server bound to {bind_ip}:{port}")
    serve(app, host=bind_ip, port=port, threads=8)
