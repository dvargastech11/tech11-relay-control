# Bill of Materials — Tech 11 Relay Control System

Scope: 12 elevators total (2 towers × 6 elevators each), 48 relay channels per elevator (576 total) — Lobby now occupies a permanent relay slot (firmware commit b62dd49), so no channels are spare.

Central controller (Windows Server or Linux PC) hardware is sourced separately and not tracked here.

_Last updated: see git commit history for this file._

## Purchases To Date

Consolidated across all orders placed so far, with received/in-transit status.

| Order # | Item | Qty | Unit Price | Line Total | Status |
|---|---|---|---|---|---|
| 113-3934632-4821855 | 10A 48V Terminal Block Distribution Module (2×12 screw terminal) | 2 | — | $30.45 | Received (Aug 2) — predates tracking start |
| 113-1737006-4897848 | MCP23017 I/O Expander | 1 | $12.40 | $12.40 | Received |
| 113-1737006-4897848 | MCP23017 I/O Expander | 5 | $12.40 | $62.00 | **Corrected: still in transit**, delayed to est. Aug 22 (previously mislogged as received) |
| 113-8976168-3128228 | ~~Mean Well MDR-20-5~~ | ~~1~~ | ~~$19.99~~ | ~~$19.99~~ | **Refunded** |
| 113-9008593-9929019 | ACEIRMC 16-Channel Relay Board 4-pack | 1 pack | $26.99 | $26.99 | Received |
| 113-9008593-9929019 | HiLetgo ESP-WROOM-32 | 12 | $9.99 | $119.88 | Received |
| 113-9008593-9929019 | Mean Well MDR-60-5 | 1 | $25.14 | $25.14 | Received |
| 114-8767660-6248263 | MDR-10-5 | 1 | $14.34 | $14.34 | Received |
| 114-9883046-3814641 | DWEII LAN8720 2-pack | 1 pack | $11.29 | $11.29 | Received |
| 114-9187607-1805044 | MCP23017 I/O Expander | 30 | $12.40 | $372.00 | In transit, est. Monday-Tuesday |
| 114-9187607-1805044 | naughtystarts ESP32 Screw Terminal Breakout 2-pack | 6 packs | $11.99 | $71.94 | Received |
| 114-9187607-1805044 | ACEIRMC 16-Ch Relay Board 4-pack | 8 packs | $26.99 | $215.92 | Received |
| 114-7642738-2269837 | Mean Well MDR-60-5 | 1 | $23.99 | $23.99 | Received (Aug 19) |
| 114-1959550-5055401 | MDR-10-5 | 1 | $14.34 | $14.34 | Received (Aug 20) |
| 114-1959550-5055401 | EDGELEC 4.7K Resistor 100-pack | 1 | $5.99 | $5.99 | Received |
| 114-1959550-5055401 | DWEII LAN8720 2-pack | 5 packs | $11.29 | $56.45 | Received |
| 114-1959550-5055401 | PCB Board Mounting Feet 60-pk | 1 | $9.99 | $9.99 | Received |
| 114-1959550-5055401 | Antrader FC-10P IDC Connector 50-pack | 1 | $7.99 | $7.99 | Received |
| 114-1959550-5055401 | Connectors Pro IDC Ribbon Cable (33ft) | 1 | $7.99 | $7.99 | Received |
| 114-1959550-5055401 | VAMRONE Aluminum DIN Rail 6-pack | 1 | $6.99 | $6.99 | Received |

**Received to date (before tax): $662.08**
**Still in transit (before tax): $434.00** (35 MCP23017 units total, split across two orders)
**Combined project spend to date (before tax, excluding refund): ~$1,096.08**

_Note: IDC connector 50-pack and IDC ribbon cable were added in Order 114-1959550-5055401 - purpose (enclosure wiring? alternative to jumper wires?) not yet confirmed, not categorized into the sections below._
_Note: DIN rail (6-pack) added for enclosure/panel mounting - not yet reflected in a dedicated enclosure section below._
_Note: Order 113-3934632-4821855 (Aug 1, terminal block distribution boards) predates when this BOM tracking began - only surfaced when reviewing full order history. Worth double-checking no other early orders are similarly missing._

### Additional Purchases — reconciled from Amazon order history, Sep 5 2026

These orders don't appear in the invoice-tracked table above (invoices covering them weren't provided). Per-item pricing isn't available from Amazon's order-history view for multi-item orders, so unit price is marked "—" where it can't be split out; the order total is shown on the relevant line instead.

| Order # | Item | Qty | Unit Price | Line Total | Status |
|---|---|---|---|---|---|
| 114-5040181-5361013 | 3.3V Buck Converter (Mini360-style) 5-pack | 1 pack | — | $9.30 (order total) | Received (Aug 25) |
| 114-8605121-8772205 | 2.54mm 1x10P Connector Housing, Female, 50pcs | 1 pack | — | — (mixed order, total $40.83 — also contains a Milwaukee tool battery and Hakko desoldering filter, both non-project, not split out) | Received (Aug 23) |
| 113-3799953-6450621 | M2 x 4mm SS Button-Head Screws, 100pcs | 1 pack | — | $17.09 (order total, split w/ next row) | Received (Aug 28) |
| 113-3799953-6450621 | MCMASKE M2 Screw/Nut/Washer Kit | 1 kit | — | — | Received (Aug 30) |
| 113-1101301-6289844 | Amazon Basics Electrical Tape, 6-pack | 2 packs | — | $38.99 (order total) | Received (Aug 26) |
| 114-6776673-9305822 | 3.3V Buck Converter 5-pack | 2 packs | — | $47.99 (order total, split w/ next 2 rows) | Received (Aug 28) |
| 114-6776673-9305822 | PCB Board Mounting Feet 60-pk | 2 packs | — | — | Received (Aug 27-28) |
| 114-6776673-9305822 | EDGELEC Dupont Wire F-F 120pcs, 15cm | 1 pack | — | — | Received (Aug 26) |
| 113-7216251-6065858 | EDGELEC Dupont Wire F-F 120pcs, 15cm | 4 packs | — | $62.46 (order total, split w/ next row) | Received (Sep 3) |
| 113-7216251-6065858 | 2.54mm 1x9P Dupont Connector Housing, Female, 50pcs | 3 packs | — | — | Received (Sep 3) |
| 113-3769407-2415403 | MCP23017 I/O Expander | 3 | — | $39.81 (order total) | **In transit**, arriving Tuesday — 3 spares beyond the 36 already needed |
| 114-3225666-1351415 | VOGOOPOI Right-Angle 2.54mm JST Terminal Housing Kit (2P/3P/4P/5P, 40 sets) | 1 kit | — | $12.83 (order total) | Received (Sep 1) |
| 114-0341565-7929812 | Taiss 2.54mm JST-XH Connector Kit (2/3/4/5/6-pin, 560pcs) | 1 kit | — | $10.69 (order total) | Received (Aug 31) |
| 114-6677983-2653821 | EDGELEC Dupont Wire F-F 120pcs, 15cm | 4 packs | — | $30.39 (order total) | Received (Sep 1) |
| 113-3320578-3181863 | JST XH 6-pin pre-wired connector pairs, 26AWG 150mm, 60 pairs | 60 pairs | — | $27.37 (order total, split w/ next row) | Received (Aug 30) |
| 113-3320578-3181863 | JST XH mini/micro 6-pin pre-wired connector pairs, 26AWG 150mm, 20 pairs | 20 pairs | — | — | Received (Aug 29) |
| 114-9236577-1050667 | ELEGOO PETG 3D Printer Filament, 1.75mm, 1kg | 1 spool | — | $14.97 (order total) | Received (Aug 31) — **project relevance unconfirmed**, see note below |

**Additional received (before tax, confident project spend): $257.11**
**Additional in transit: $39.81**
**Updated received to date: $919.19** ($662.08 + $257.11)
**Updated still in transit: $473.81** ($434.00 + $39.81)
**Updated combined project spend (excluding refund, mixed order, and unconfirmed filament): ~$1,393.00**

_Not included in the totals above: the $40.83 mixed order (partial project relevance, can't be split without a line-item invoice) and the $14.97 ELEGOO filament (project relevance unconfirmed). If both turn out to be fully project-related, add $55.80 to reach ~$1,448.80._

_Also excluded entirely (non-project, found in the same Amazon order history but unrelated to this build): Milwaukee tool battery + Hakko desoldering filter (bundled in the $40.83 mixed order above), XMTYAN earbud tips ($10.57, order 113-6427869-7712206), Sticky Tack poster stickers + 2x Hatsune Miku wall posters ($29.93, order 114-2350662-1472206), Kasa Matter Smart Light Switch KS205P3 x2 ($106.98, order 114-3957491-6351425), Helping Hands soldering clamp stand ($27.81, order 114-0428163-6706655). Also seen but cancelled/not charged: two M2 screw-kit orders, a 500pc low-voltage wire connector order, and a Loctite super glue order._


## Per-Elevator Electronics

| Item | Qty per Elevator | **Total Needed** | Ordered | Remaining |
|---|---|---|---|---|
| ESP32-WROOM-32D | 1 | 12 | 12 | 0 ✅ |
| ESP32 Screw Terminal Breakout (no-solder wiring, replaces bare jumper wires) | 1 | 12 | 12 | 0 ✅ |
| LAN8720 Ethernet board | 1 | 12 | 12 | 0 ✅ |
| MCP23017 I/O expander | 3 | 36 | 36 (+3 spares in transit, arriving Tuesday) | 0 ✅ |
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
| Dupont/jumper wire assortment (EDGELEC 120pcs F-F, 15cm) | 2-3 packs | **9 packs (1080 wires)** ✅ | 0 — well covered |
| Wire ferrules/spade connector pack | 2-3 packs | 0 | 2-3 packs |
| Micro USB cable (flashing, shared/reusable) | 1-2 | On hand | 0 ✅ |
| Amazon Basics electrical tape (3/4" x 60', 6-pack) | TBD | 2 packs (12 rolls) | TBD |

## Connectors & Wiring Hardware (new — reconciled from Amazon order history)

| Item | Ordered/Received | Note |
|---|---|---|
| 2.54mm 1x10P Connector Housing, Female (50pcs) | 1 pack | Possibly a functional duplicate of the Antrader FC-10P IDC Connector 50-pack above (different seller, same order-history batch) — worth confirming before ordering more of either |
| 2.54mm 1x9P Dupont Connector Housing, Female (50pcs) | 3 packs (150pcs) | |
| VOGOOPOI right-angle 2.54mm JST terminal housing kit (2P/3P/4P/5P, 40 sets) | 1 kit | |
| Taiss 2.54mm JST-XH connector kit (2/3/4/5/6-pin, 560pcs) | 1 kit | |
| JST XH 6-pin pre-wired connector pairs, 26AWG 150mm | 60 pairs | |
| JST XH mini/micro 6-pin pre-wired connector pairs, 26AWG 150mm | 20 pairs | |
| 3.3V buck converter module (Mini360-style, 5-30V in), 5-pack | 3 packs (15pcs) | **Purpose still unconfirmed** — flagging so it can be tied to a specific rail (LAN8720 3.3V supply? sensor logic?) rather than guessed |

*The JST-XH / right-angle connector purchases look like they're for building removable wiring harnesses between the MCP23017 boards, relay boards, and room power buses — worth confirming so these can be folded into a proper per-elevator harness line instead of sitting as loose consumables.*

## Mounting / Enclosure Hardware (new — reconciled from Amazon order history)

| Item | Ordered/Received | Note |
|---|---|---|
| PCB Board Mounting Feet, 60-pk (L-shape standoffs) | **3 packs total (180pcs)** — 1 pack already in the Purchases table (order 114-1959550-5055401) + 2 more from order 114-6776673-9305822 | Likely for standoff-mounting MCP23017/relay boards in enclosures — confirm against 36+36 board count |
| VAMRONE Aluminum DIN Rail, 6-pack | 1 pack (already in Purchases table, order 114-1959550-5055401) | For enclosure/panel mounting — not yet confirmed which enclosures |
| M2 x 4mm SS button-head screws (100pcs) | 1 pack | |
| MCMASKE M2 button-head screw/nut/washer kit | 1 kit | |

## Possibly Project-Related — Unconfirmed

| Item | Ordered/Received | Note |
|---|---|---|
| ELEGOO PETG 3D printer filament, 1.75mm, 1kg | 1 spool | Could be for 3D-printed brackets/enclosures on this project, or unrelated — flagging rather than assuming |

## Open Items Before Full-Scale Ordering

- Confirm actual worst-case simultaneous relay draw **per room** (6 elevators × 48 relays, not just per-elevator) before wiring in the 2nd MDR-60-5 per room — both units are now purchased, but sizing hasn't been re-verified at room scale.
- Verify MCP23017 boards' onboard I2C pull-ups with a multimeter once received — determines whether the 100-pack of 4.7kΩ resistors on hand is actually needed.
- Confirm intended use of the IDC connector 50-pack, IDC ribbon cable, and DIN rail (all added in Order 2) so they can be properly categorized above.
- All core per-elevator electronics (ESP32, breakout board, Ethernet, MCP23017, relay boards) are now fully covered for all 12 elevators — next phase is assembly/wiring, not further ordering, aside from IEC cords and any enclosure hardware.
- **New this update:** confirm intended use of the 3.3V buck converters and ELEGOO PETG filament, and whether the 2.54mm 1x10P connector housing (Amazon order) duplicates the Antrader FC-10P 50-pack already on hand — three open questions before these get folded cleanly into the sections above.
- The mixed $40.83 order and the $14.97 filament order are excluded from the running project-spend total pending confirmation — see notes above.

