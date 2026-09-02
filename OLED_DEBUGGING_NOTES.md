# OLED Display - Shelved, Debugging Notes

**Status as of this branch:** display initializes successfully over I2C
(both SSD1306 and SH1106 drivers were tried) but the physical screen stays
completely blank. Shelved pending further hardware investigation - not
worth blocking other work on.

## What's confirmed working
- I2C detection succeeds at 0x3C (`display.begin()` returns true, no
  "Not detected" error)
- OLED module's own VCC pin reads a correct, stable ~3.3V
- Module is genuinely 4-pin (VCC/GND/SDA/SCL) - no separate RES/RST pin
  being left unconnected

## What's been tried
1. SSD1306 driver (Adafruit_SSD1306 library) - init succeeded, blank screen
2. SH1106 driver (Adafruit_SH110X library) - init succeeded, still blank
   screen (this is the current state of the code on this branch)
3. Multiple different physical OLED units - **all fail identically**,
   which argues against a single defective unit

## What HASN'T been tried yet (the key next step)
All testing so far has been with the OLED sharing the I2C bus with the
3 MCP23017 relay boards, using our full project firmware. **The bare
Adafruit example sketch, with the OLED as the ONLY device on the I2C
bus (MCP23017 boards physically disconnected), has not been tried.**

## Leading theory
Each MCP23017 board likely has its own onboard I2C pull-up resistors.
With 3 boards + the OLED all sharing one bus, the combined parallel
pull-up resistance could be low enough to degrade signal integrity -
possibly good enough for a single ACK bit (what `begin()` checks) but
not clean enough for the fuller command sequence needed to actually
turn the display on.

## Next step when resuming
1. Physically disconnect all 3 MCP23017 boards
2. Flash the bare Adafruit SH110X example sketch (File > Examples >
   Adafruit SH110X > sh110x_128x64_i2c), OLED as the only I2C device
3. **If it works**: confirms bus loading is the cause. Fix = add
   dedicated pull-up resistors (2.2k-4.7k ohm from SDA and SCL to 3.3V)
   physically near the OLED, or investigate reducing pull-up load
   elsewhere on the bus.
4. **If it's still blank even alone**: the driver/wiring theories are
   likely both ruled out - would need to reconsider from scratch (bad
   batch of units, wrong assumption about the module entirely, etc.)
