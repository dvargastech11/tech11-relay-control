# PR Plan: Relay Settings, Flash Messages, IP Allowlist, ALL BUTTONS

## Changes
- Adjustable floor-press hold time added to Relay Settings admin page, default 5s
- /trigger endpoint accepts optional durationMs (100ms to 10min), server does not trust duration from public operator URL
- Flash messages now render on every page, not just login
- IP allowlist refuses any add/remove that would lock out the admin making the change
- ALL BUTTONS test shows per-elevator failure reasons, uses "accepted" instead of "reached"

## Files touched
- flask-server/relay_settings_store.py
- flask-server/ip_acl_store.py
- flask-server/app.py
- flask-server/templates/base.html
- flask-server/templates/admin_relay_settings.html
- flask-server/templates/admin_ip_acl.html
- flask-server/templates/admin_test_buttons.html
- firmware/tech11_relay_module/relay_control.cpp
- firmware/tech11_relay_module/relay_control.h
