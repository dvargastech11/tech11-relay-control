# Review Note: Relay Settings, Flash Messages, IP Allowlist, ALL BUTTONS

This describes work already merged to `main` in commit `723ae85`. It is a
record for later reference, not a proposal - the code is live.

## 1. Adjustable floor-press hold time

**Why:** the hold time was a fixed 5 seconds in code. Some sites need it
longer or shorter per the physical elevator hardware.

- **Admin page:** new "Relay Settings" page sets the hold time (0.1-600s,
  default 5s).
- **Data flow:** admin sets seconds on the page -> `relay_settings_store`
  saves it as ms -> `app.py` sends that value as `durationMs` on every
  `/trigger` and `/api/activate` call -> the ESP32 clamps it again before
  using it.
- **Device (`/trigger`, ESP32 side):**
  - Missing or `<= 0` -> falls back to the original fixed 5s.
  - `1-99 ms` -> raised to 100 ms.
  - Over 10 minutes -> capped at 10 minutes.
  - This also covers a missing or corrupt `relay_settings.json` on the
    server - the server itself falls back to 5s in that case too.
- **Server (`/activate`, public operator route):** the duration is never
  taken from the caller. Only the admin-set value from Relay Settings is
  used, regardless of what's in the request.
- **Also uses this setting:** `/api/activate` (the external API). ALL
  BUTTONS (below) does not - it keeps its own fixed 10-minute hold.

**Files:**
- `flask-server/relay_settings_store.py` (new) - stores/clamps the hold
  time.
- `flask-server/app.py` (modify) - new `/admin/relay-settings` route;
  `/activate`, `/api/activate`, and the device command path now send the
  stored value.
- `flask-server/templates/admin_relay_settings.html` (new) - the admin
  page.
- `firmware/tech11_relay_module/web_handlers.cpp`,
  `firmware/tech11_relay_module/config.h` (modify) - `/trigger` now reads
  and clamps `durationMs`; new `/hold` command used by ALL BUTTONS (see
  below).

**Firmware rollout:** this landed in firmware source on `main`, but no
firmware release has shipped with it yet - `firmware/version.txt` and
`CURRENT_FIRMWARE_VERSION` are still `1.0.3`, same as what's on the
devices. That's fine to ship this way: because the server always sends a
value and the device falls back safely on old firmware, the server did
not need to wait for a firmware release before going live. When a
release with this feature does ship, bump the version so the Devices page
can flag anything still on the old build.

## 2. Flash messages on every page

**Why:** only the login page displayed `flash()` messages. Roughly 40
other confirmations and warnings across the admin pages (API key
regenerated, device unassigned, settings saved, allowlist changes, etc.)
were being silently dropped.

**Fix:** added one message block to `base.html`, so every page that
extends it now shows flashed messages.

**Files:**
- `flask-server/templates/base.html` (modify)

## 3. IP allowlist self-lockout guard

**Why:** the allowlist could be changed in a way that blocked the admin
making the change. The old warning fired after saving, which could never
actually be seen - the very next page load was already blocked.

**Fix:** both add and remove now check, before saving, whether the
resulting list would block the IP making the request, and refuse the
change if so.

**Scope and limits:**
- Protects only the admin currently making the change - not other admins
  who might be using a different address.
- `127.0.0.1`/`::1` (localhost) is always allowed regardless of the list -
  this is the intended recovery path if a lockout ever does happen (RDP
  into the server, browse to localhost).
- The check-then-save has no lock around it, so two admins editing the
  list at the same moment could still end up locking each other out. Not
  addressed here.

**Files:**
- `flask-server/ip_acl_store.py` (modify) - `is_allowed_by()` used for the
  pre-save check.
- `flask-server/app.py` (modify) - `/admin/ip-acl` route.
- `flask-server/templates/admin_ip_acl.html` (modify)

## 4. ALL BUTTONS failure reasons

**What ALL BUTTONS is:** an on/off control on the public building page
(`building_control.html`) for operators - not an admin-only tool. Turning
it on holds every currently-available floor relay, on every elevator in
the building, for 10 minutes (auto-off enforced on the devices
themselves via the new `/hold` firmware command); off releases them.
State is shared per building, not per browser session.

**Why this change:** the page was dropping the per-elevator failure
reasons when every elevator failed, showing only a generic error.

**Fix:**
- Response now always includes a `failed` list (one entry per elevator
  that didn't accept the command, with its reason) and an `error` field.
- "Accepted" replaces "reached" in the wording: an elevator counts as
  accepted if its device returned HTTP 200 for the hold/release request.
  If none did, the response's `error` says "No elevator accepted the
  command".

**Files:**
- `flask-server/app.py` (modify) - `/all-buttons/<building_id>/<action>`.
- `flask-server/templates/building_control.html` (modify) - renders
  `failed`/`error`.
- `flask-server/devices.py` (modify) - `hold_relays()` used to send the
  `/hold` command.

## Risks / assumptions

- Firmware rollout gap described above (section 1) - accepted as safe
  given the fallback behavior.
- The allowlist checks `request.remote_addr` directly. If this server is
  ever put behind a reverse proxy, every client would appear as the
  proxy's own address (often `127.0.0.1`, which is always allowed) and the
  allowlist would stop doing anything. Not an issue today - noted for if
  that ever changes.
- Localhost's permanent exemption from the allowlist is intentional (the
  recovery path), not an oversight.

## Non-goals (not part of this work)

- Per-building hold time (one setting covers every building).
- Reverse-proxy support for the IP allowlist.
- A firmware release/version bump - tracked separately.

## Manual test checklist

- [ ] A device still on old firmware holds for 5s regardless of the
      Relay Settings value.
- [ ] `durationMs` of 50, missing, and 700000 are clamped to 100,
      5000 (default), and 600000 respectively.
- [ ] Saving an allowlist entry that would block the current admin's own
      IP is refused, with the lockout message.
- [ ] Removing the last allowlist entry is allowed (list becomes empty =
      unrestricted).
- [ ] A flash message appears correctly on a non-login admin page (e.g.
      after generating a new API key).
- [ ] ALL BUTTONS: turn on with one elevator's device offline - confirm
      `failed` lists that elevator with a reason, and the other elevators
      still hold.

## Related follow-up (fixed separately, not part of this merge)

The EGM design review of this document also surfaced defects in the
merged code itself, unrelated to the document's accuracy. These were
fixed in later commits on `main`:

- `6479a06` - Relay Settings page no longer claims a nonexistent "1.0.5"
  firmware release; switched to `flash()` + redirect so refreshing the
  page after saving doesn't trigger a resubmit warning.
- `e01c997` - `ip_acl_store.load_acl()` no longer raises on a missing or
  corrupt `ip_acl.json`. It runs on every request via `before_request`,
  so an unhandled error there would have 500'd the entire server,
  including the admin page needed to fix the file.
- `f3eb483` - removed the dead `duration_ms` path segment on the public
  `/activate` route (it was validated then ignored); moved IP/CIDR
  validation into `ip_acl_store.is_valid_entry()` instead of duplicating
  it in `app.py`; `/admin/test-buttons/fire` now clamps `duration_ms`
  server-side to match the device's own clamp.
