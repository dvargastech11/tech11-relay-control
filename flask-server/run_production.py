"""
Production entrypoint - runs the Flask app under waitress instead of
Flask's built-in development server. This is what the Windows Service
(via NSSM) actually launches.

Run manually for testing:
    venv\\Scripts\\python.exe run_production.py
"""

from waitress import serve
from app import app

if __name__ == "__main__":
    serve(app, host="0.0.0.0", port=5000, threads=8)
