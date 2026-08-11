"""
Tech 11 Device Discovery Tool
--------------------------------
Broadcasts a UDP discovery request and lists every Tech 11 relay
controller module that responds, showing its name, IP, and MAC address.

Designed to be deployment-agnostic - works on any network/site running
Tech 11 controller firmware, not tied to a specific building or project.

Install dependencies:
    (none required beyond standard Python 3 - tkinter is built in)

Run:
    python tech11_discovery.py

To package as a standalone .exe (no Python required to run it):
    pip install pyinstaller
    pyinstaller --onefile --windowed --name "Tech11Discovery" tech11_discovery.py
    (find the .exe in the generated dist/ folder)
"""

import socket
import json
import threading
import time
import webbrowser
import tkinter as tk
from tkinter import ttk, messagebox

DISCOVERY_MESSAGE = b"TECH11_DISCOVER"
DISCOVERY_PORT = 4210
TIMEOUT_SEC = 3


def discover_devices():
    """Broadcasts the discovery message and collects replies for TIMEOUT_SEC seconds."""
    devices = []
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(TIMEOUT_SEC)

    try:
        sock.sendto(DISCOVERY_MESSAGE, ("255.255.255.255", DISCOVERY_PORT))

        seen_ips = set()
        start = time.time()
        while time.time() - start < TIMEOUT_SEC:
            try:
                data, addr = sock.recvfrom(1024)
                parsed = json.loads(data.decode("utf-8"))
                if parsed.get("ip") not in seen_ips:
                    seen_ips.add(parsed.get("ip"))
                    devices.append(parsed)
            except socket.timeout:
                break
            except (json.JSONDecodeError, UnicodeDecodeError):
                continue
    finally:
        sock.close()

    return devices


class DiscoveryApp:
    def __init__(self, root):
        self.root = root
        root.title("Tech 11 Device Discovery")
        root.geometry("650x400")
        root.configure(bg="#1a1a1a")

        header = tk.Label(
            root, text="Tech 11 - Device Discovery",
            font=("Segoe UI", 16, "bold"), bg="#1a1a1a", fg="white"
        )
        header.pack(pady=15)

        self.status_label = tk.Label(
            root, text="Click 'Scan Network' to find devices.",
            font=("Segoe UI", 10), bg="#1a1a1a", fg="#aaaaaa"
        )
        self.status_label.pack(pady=(0, 10))

        style = ttk.Style()
        style.theme_use("default")
        style.configure("Treeview", background="#262626", foreground="white",
                         fieldbackground="#262626", rowheight=28, font=("Segoe UI", 10))
        style.configure("Treeview.Heading", background="#2d6cdf", foreground="white",
                         font=("Segoe UI", 10, "bold"))
        style.map("Treeview", background=[("selected", "#2fbf4f")])

        columns = ("name", "ip", "mac")
        self.tree = ttk.Treeview(root, columns=columns, show="headings", height=10)
        self.tree.heading("name", text="Device Name")
        self.tree.heading("ip", text="IP Address")
        self.tree.heading("mac", text="MAC Address")
        self.tree.column("name", width=220)
        self.tree.column("ip", width=150)
        self.tree.column("mac", width=180)
        self.tree.pack(padx=20, pady=10, fill="both", expand=True)

        btn_frame = tk.Frame(root, bg="#1a1a1a")
        btn_frame.pack(pady=10)

        self.scan_btn = tk.Button(
            btn_frame, text="Scan Network", command=self.start_scan,
            bg="#2d6cdf", fg="white", font=("Segoe UI", 11, "bold"),
            padx=20, pady=8, relief="flat", cursor="hand2"
        )
        self.scan_btn.pack(side="left", padx=5)

        self.open_btn = tk.Button(
            btn_frame, text="Open Selected in Browser", command=self.open_selected,
            bg="#333333", fg="white", font=("Segoe UI", 11),
            padx=20, pady=8, relief="flat", cursor="hand2"
        )
        self.open_btn.pack(side="left", padx=5)

    def start_scan(self):
        self.scan_btn.config(state="disabled", text="Scanning...")
        self.status_label.config(text="Broadcasting discovery request...")
        for row in self.tree.get_children():
            self.tree.delete(row)

        thread = threading.Thread(target=self.run_scan, daemon=True)
        thread.start()

    def run_scan(self):
        devices = discover_devices()
        self.root.after(0, self.display_results, devices)

    def display_results(self, devices):
        if not devices:
            self.status_label.config(text="No devices found. Check network connection and try again.")
        else:
            self.status_label.config(text=f"Found {len(devices)} device(s).")
            for d in devices:
                self.tree.insert("", "end", values=(d.get("name", "Unknown"), d.get("ip", ""), d.get("mac", "")))

        self.scan_btn.config(state="normal", text="Scan Network")

    def open_selected(self):
        selection = self.tree.selection()
        if not selection:
            messagebox.showinfo("No Selection", "Select a device from the list first.")
            return
        ip = self.tree.item(selection[0])["values"][1]
        webbrowser.open(f"http://{ip}")


if __name__ == "__main__":
    root = tk.Tk()
    app = DiscoveryApp(root)
    root.mainloop()
