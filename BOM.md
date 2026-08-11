# Bill of Materials — Tech 11 Relay Control System

Scope: 12 elevators total (2 towers × 6 elevators each), 47 relay channels per elevator (564 total).

Central controller (Windows Server or Linux PC) hardware is sourced separately and not tracked here.

_Last updated: see git commit history for this file._

## Per-Elevator Electronics

| Item | Qty per Elevator | **Total Needed** | Ordered/Received | Remaining |
|---|---|---|---|---|
| ESP32-WROOM-32D | 1 | 12 | 12 | 0 ✅ |
| LAN8720 Ethernet board | 1 | 12 | 2 | 10 |
| MCP23017 I/O expander | 3 | 36 | 2 | 34 |
| 16-channel relay board (NO, 5V coil, optocoupled) | 3 | 36 | 0 | 36 |
| 4.7kΩ resistor pair (I2C pull-ups, one pair per elevator's bus) | 1 pair | 12 pairs | 0 | 12 pairs |

## Per-Room Infrastructure (×2 rooms — one per tower)

| Item | Qty per Room | **Total Needed** | Ordered/Received | Remaining |
|---|---|---|---|---|
| Mean Well MDR-60-5 (relay-coil PSU, 5V/10A) | 1 | 2 | 0 | 2 — **verify sizing against room-level worst-case draw before ordering** |
| Mean Well MDR-20-5 (logic PSU, 5V/3A) | 1 | 2 | 0 | 2 |
| IEC power cord (AC input to PSUs) | 1-2 | 2-4 | 0 | 2-4 |

## Consumables / Assembly Supplies

| Item | Total Needed | Ordered/Received | Remaining |
|---|---|---|---|
| Dupont/jumper wire assortment (ESP32↔MCP23017↔relay board wiring) | 2-3 packs | 0 | 2-3 packs |
| Wire ferrules/spade connector pack | 2-3 packs | 0 | 2-3 packs |
| Micro USB cable (flashing, shared/reusable) | 1-2 | On hand | 0 ✅ |

## Open Items Before Full-Scale Ordering

- Confirm actual worst-case simultaneous relay draw **per room** (6 elevators × 47 relays, not just per-elevator) before finalizing PSU quantity/sizing — the MDR-60-5's 12A may need to be re-evaluated at room scale.
- Current MCP23017/LAN8720 quantities (2 each) are enough for continued single-elevator bench work, not yet sufficient to begin parallel builds across multiple elevators.
