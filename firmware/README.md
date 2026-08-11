# Firmware

## `tech11_relay_module/` - current production sketch

This is the real, actively-maintained firmware. Open
`tech11_relay_module.ino` in Arduino IDE - it's a multi-file sketch, so the
other `.h`/`.cpp` files in this same folder will load automatically as tabs.

See the file-by-file breakdown in the main repo README.

## `archive/` - superseded test/bench files

Early single-file test sketches used while bench-testing relay wiring,
WiFi behavior, and JSON parsing before the code was split into the modular
`tech11_relay_module/` sketch above. Kept for reference only - **do not
flash these**, they're missing fixes (auth, correct relay parsing, DNS/NTP
config, etc.) that only exist in the current sketch.
