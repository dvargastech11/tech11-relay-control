# Bill of Materials — Tech 11 Relay Control System

Scope: 12 elevators total (2 towers × 6 elevators each), 47 relay channels per elevator (564 total).

Central controller (Windows Server or Linux PC) hardware is sourced separately and not tracked here.

_Last updated: see git commit history for this file._

## Purchases To Date

Consolidated across both orders placed so far, with received/in-transit status.

| Item | Qty | Unit Price | Line Total | Status |
|---|---|---|---|---|
| MCP23017 I/O Expander (Order 1) | 6 | $12.40 | $74.40 | Received |
| ACEIRMC 16-Channel Relay Board 4-pack (Order 1) | 1 pack | $26.99 | $26.99 | Received |
| HiLetgo ESP-WROOM-32 (Order 1) | 12 | $9.99 | $119.88 | Received |
| Mean Well MDR-60-5 (Order 1) | 1 | $25.14 | $25.14 | Received |
| MDR-10-5 (Order 1) | 1 | $14.34 | $14.34 | Received |
| DWEII LAN8720 2-pack (Order 1) | 1 pack | $11.29 | $11.29 | Received |
| ~~Mean Well MDR-20-5~~ | ~~1~~ | ~~$19.99~~ | ~~$19.99~~ | Cancelled/returned |
| PCB Board Mounting Feet 60-pk (Order 2) | 1 | $9.99 | $9.99 | Placed, est. Aug 13-14 |
| DWEII LAN8720 2-pack (Order 2) | 5 packs | $11.29 | $56.45 | Placed, est. Aug 13-14 |
| VAMRONE Aluminum DIN Rail 6-pack | 1 | $6.99 | $6.99 | Placed, est. Aug 13-14 |
| Antrader FC-10P IDC Connector 50-pack | 1 | $7.99 | $7.99 | Placed, est. Aug 13-14 |
| Connectors Pro IDC Ribbon Cable (33ft) | 1 | $7.99 | $7.99 | Placed, est. Aug 14 |
| EDGELEC 4.7K Resistor 100-pack | 1 | $5.99 | $5.99 | Placed, est. Aug 13-14 |
| Mean Well MDR-60-5 (Order 2) | 1 | $23.99 | $23.99 | Placed, est. Aug 13-14 |
| MDR-10-5 (Order 2) | 1 | $14.34 | $14.34 | Placed, est. Aug 18 |
| ACEIRMC 16-Ch Relay Board 4-pack (Order 2) | 8 packs | $26.99 | $215.92 | Placed, est. Aug 12-14 |
| naughtystarts ESP32 Screw Terminal Breakout 2-pack | 6 packs | $11.99 | $71.94 | Placed, est. Aug 12-14 |
| MCP23017 I/O Expander (Order 2) | 30 | $12.40 | $372.00 | Placed, est. Aug 21-25 |

**Received to date: $271.04** (excluding cancelled MDR-20-5)
**Order 2 total (with tax): $849.21**
**Combined project spend to date: ~$1,120.25**

_Note: IDC connector 50-pack and IDC ribbon cable were added in Order 2 - purpose (enclosure wiring? alternative to jumper wires?) not yet confirmed, not categorized into the sections below._
_Note: DIN rail (6-pack) added for enclosure/panel mounting - not yet reflected in a dedicated enclosure section below._

## Per-Elevator Electronics

| Item | Qty per Elevator | **Total Needed** | Ordered | Remaining |
|---|---|---|---|---|
| ESP32-WROOM-32D | 1 | 12 | 12 | 0 ✅ |
| ESP32 Screw Terminal Breakout (no-solder wiring, replaces bare jumper wires) | 1 | 12 | 12 | 0 ✅ |
| LAN8720 Ethernet board | 1 | 12 | 12 | 0 ✅ |
| MCP23017 I/O expander | 3 | 36 | 36 | 0 ✅ |
| 16-channel relay board (NO, 5V coil, optocoupled) | 3 | 36 | 36 | 0 ✅ |
| 4.7kΩ resistor pair (I2C pull-ups — only needed if MCP23017 boards lack onboard pull-ups; verify with multimeter on arrival before use) | 1 pair | 12 pairs | 100 individual resistors on hand | Pending verification |

## Per-Room Infrastructure (×2 rooms — one per tower)

| Item | Qty per Room | **Total Needed** | Ordered | Remaining |
|---|---|---|---|---|
| Mean Well MDR-60-5 (relay-coil PSU, 5V/10A) | 1 | 2 | 2 | 0 ✅ — sizing still not re-verified at room scale, see Open Items |
| Logic PSU (MDR-10-5, filling the MDR-20-5 role) | 1 | 2 | 2 | 0 ✅ |
| IEC power cord (AC input to PSUs) | 1-2 | 2-4 | 0 | 2-4 |

## Consumables / Assembly Supplies

| Item | Total Needed | Ordered/Received | Remaining |
|---|---|---|---|
| Dupont/jumper wire assortment (ESP32↔MCP23017↔relay board wiring) | 2-3 packs | 0 | 2-3 packs |
| Wire ferrules/spade connector pack | 2-3 packs | 0 | 2-3 packs |
| Micro USB cable (flashing, shared/reusable) | 1-2 | On hand | 0 ✅ |

## Open Items Before Full-Scale Ordering

- Confirm actual worst-case simultaneous relay draw **per room** (6 elevators × 47 relays, not just per-elevator) before wiring in the 2nd MDR-60-5 per room — both units are now purchased, but sizing hasn't been re-verified at room scale.
- Verify MCP23017 boards' onboard I2C pull-ups with a multimeter once received — determines whether the 100-pack of 4.7kΩ resistors on hand is actually needed.
- Confirm intended use of the IDC connector 50-pack, IDC ribbon cable, and DIN rail (all added in Order 2) so they can be properly categorized above.
- All core per-elevator electronics (ESP32, breakout board, Ethernet, MCP23017, relay boards) are now fully covered for all 12 elevators — next phase is assembly/wiring, not further ordering, aside from IEC cords and any enclosure hardware.

