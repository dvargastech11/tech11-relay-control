"""
Windows-native authentication.
--------------------------------
Validates admin login credentials against real local Windows Server
accounts (via the LogonUser Win32 API), and grants admin access only to
accounts that are members of the local "Administrators" group.

Requires pywin32:
    python -m pip install pywin32

This intentionally does NOT do full SSO/Kerberos passthrough (that
requires IIS or another SSPI-aware front end, which this deployment
skips per design). The person still enters username/password on our
login form - we just validate those credentials against Windows itself
instead of maintaining a separate password store.
"""

import win32security
import win32net
import pywintypes

LOGON32_LOGON_NETWORK = 3
LOGON32_PROVIDER_DEFAULT = 0


def validate_windows_credentials(username, password, domain="."):
    """Returns True if username/password is a valid Windows login on this
    machine. domain='.' means 'this local computer' (local accounts only,
    per current design - not a domain controller lookup)."""
    try:
        handle = win32security.LogonUser(
            username,
            domain,
            password,
            LOGON32_LOGON_NETWORK,
            LOGON32_PROVIDER_DEFAULT,
        )
        handle.Close()
        return True
    except pywintypes.error:
        return False


def is_local_admin(username):
    """Returns True if username is a member of the local Administrators group."""
    try:
        members, _, _ = win32net.NetLocalGroupGetMembers(None, "Administrators", 1)
        member_names = [m["name"].lower() for m in members]
        return username.lower() in member_names
    except Exception:
        return False
