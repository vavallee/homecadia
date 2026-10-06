# Battery runbook — protected LiPo on the XIAO ESP32-C6

For the EEMB LP103454 2000 mAh pack (with protection board) feeding a XIAO
ESP32-C6 through its underside BAT pads. Use it when a pack reads 0 V, when a
unit is dead or resetting on battery, or before leaving a unit on a cell.

Each entry says how it is known. **Measured** = read on this bench, with the
date. **Vendor** = a vendor page or schematic. **General** = how protection
boards of this class behave; not confirmed for this pack. The pack's own
thresholds are **unverified**: the EEMB specification could not be retrieved
(2026-09-28), so no trip voltage or current is quoted here.

## What the protection board does

The vendor lists five functions: overcharge, over-discharge, over-current,
short-circuit and over-temperature (Vendor: eemb.store product page for
LP103454). The board sits between the cell and the two leads, with separate
switches for the charge and discharge directions. When the discharge switch
opens, **the leads read 0 V while the cell inside is intact.**

## Causes of "0 V" or "dead on battery"

### A. The protection board has opened

| # | Cause | Trigger on this build | What it looks like | Release | Status |
|---|---|---|---|---|---|
| A1 | **Over-discharge** | a unit that never sleeps left on the cell: bench profile at ~41 mA, or an unpaired unit at ~28 mA (about 3 days from full) | 0 V at the leads; still accepts charge; **keeps reading 0 V for hours after charging starts** | charge it; re-measure open-circuit afterwards, not during | Measured twice on pack #1, 2026-09-05 and 2026-09-09, recovered to 3.97 V both times |
| A2 | **Over-current or short-circuit** | bare leads touching; both leads cut in one snip; a strand bridging the BAT+ and BAT− pads | 0 V at the leads on a cell that is **not** flat | remove the short; a board of this class then needs the load removed or a charger applied (General) | Candidate for 2026-09-28: a pack read 0 V, and half an hour later read 3.97 V with the charger already at its 4.2 V limit, so it had not been empty. Not proven |
| A3 | **Overcharge** | charger fault | the charge direction opens; the pack still delivers | falls back by itself as the cell relaxes (General) | Not seen. The XIAO's charger (SGM40567, Vendor: XIAO schematic sheet 3/5) read 4216–4245 mV at the BAT pin with a cell on it |
| A4 | **Over-temperature** | — | — | — | Not seen. Whether the two-lead pack variant has the sensor is unverified |

### B. Not the protection board, same symptom

| # | Cause | What it looks like | Test | Status |
|---|---|---|---|---|
| B1 | **Probe on the connector housing** | 0 V in both polarities | probe bare metal; the JST-PH contacts sit below the rim | Measured 2026-09-03: two packs "dead", 3.87 V once the connector came off |
| B2 | **Meter lead loose in its jack** | 0 V and "open" on everything | probes together on beep, then a known 5 V | Measured 2026-09-25 |
| B3 | **High resistance between cell and XIAO** | pack reads healthy unloaded (3.97 V); unit resets at every boot. External LED flickers faintly and fast, `RebootCount` climbs, Home Assistant values freeze | meter on the XIAO's side of the joint, USB out, watch 10 s; read the DIAG view | Measured 2026-09-28: `RebootCount` 4 → 88 in the first episode and 88 → 504 in a second one of about five minutes; steady after the BAT+ wire was re-seated in the row, DIAG then read 3.98 V / 82 % and the controller 3978 mV. The joint is the best-supported cause, not proven. **Second episode 2026-10-02**, with the PPK2 in the same row: ~6,100 restarts in 54 min during a pairing, ~0.5 s each; the device's next reported restart was `bootReason: 2` (brown-out) |
| B4 | **Reversed leads** | negative reading | measure the sign before connecting | EEMB sells a reversed-polarity variant (Vendor: LP103454RP). Pack #1 was standard, red = + |
| B5 | **No cell, USB in** | unit runs and reports a battery voltage anyway | BAT pin reads ~4.03–4.05 V with no cell, 4.2 V and above with one | Measured 2026-09-24 and 2026-09-28. A battery reading on USB is the charger's output, never the cell's state |

Why B3 resets the chip: a boot switches the radio on, which pulls bursts of
250–330 mA (Measured 2026-09-15, [power-budget.md](power-budget.md)) with a
424 mA inrush at power-on (Measured 2026-09-09). A fraction of an ohm in the
path is enough to drop the supply under the brownout detector
(`CONFIG_ESP_BROWNOUT_DET_LVL=7`; the voltage that level corresponds to on
the 3V3 rail is unverified; on the BAT side a unit runs at 2900 mV and fails
to boot there, [field-notes.md](field-notes.md) §29). Breadboard springs and thin stranded leads are that fraction of
an ohm. The PPK2 fed the same row without trouble, which is what pointed at
the cell's own lead.

## Procedure, cheapest test first

1. **Meter check:** probes together, then a known voltage.
2. **Measure the pack alone, on bare metal, and note the sign.**
   - negative: leads reversed. Stop.
   - +3.5 to +4.2 V: the pack is fine; go to 5.
   - 0 V: go to 3.
3. **Put it on a charger** (a XIAO on USB, pack on its BAT pads; about 100 mA).
   The PPK2 must not be on BAT+ at the same time.
4. **Re-measure open-circuit after charging, pack disconnected.**
   - back within minutes to an hour, near 4 V: it was not flat (A2, or B1).
   - back after hours: over-discharge (A1).
   - never rises: the cell is gone.
5. **Pack healthy, unit dead or resetting on it:** USB out, meter on the
   XIAO's BAT+ wire against GND for 10 s.
   - steady near the pack voltage: look at the unit, not the battery.
   - sagging or jumping: the joint (B3). Solder the pack's leads to the BAT
     wires; do not rely on a breadboard for the battery path.
6. **Ask the unit why it last restarted:** the controller holds
   `bootReason` (`2` = brown-out); a climbing `RebootCount` with brown-out
   as the reason is the battery path, not the firmware.
7. **Confirm from the unit itself:** the DIAG view shows the voltage the
   firmware measures, with no network needed.

## Rules

- **Pair a unit before it goes on a cell.** Unpaired it draws 28 mA and never
  sleeps ([field-notes.md](field-notes.md) §26).
- **Disconnect the cell at the end of a bench session** while the bench
  profile is flashed.
- **USB and the PPK2 never feed BAT+ together.**
- **Cut pack leads one at a time, at staggered lengths.** One snip through
  both is a short through the cutter.
- **A boot loop wears flash:** each boot writes the reboot counter. Put USB
  back in to stop one, then diagnose.
- **Do not judge a pack by terminal voltage alone.** A latched board, a probe
  on plastic and a failed cell all read 0 V.

## Seen on this bench

| Date | Pack | Event | Outcome |
|---|---|---|---|
| 2026-09-03 | #1, #2 | both read 0 V at the connector | probe on the housing; 3.87 V on bare leads |
| 2026-09-05 | #1 | drained by the bench profile, read 0 V | recovered to 3.97 V on charge |
| 2026-09-09 | #1 | same, after three days at ~41 mA | recovered to 3.97 V; read 0 V for hours while taking 106 mA |
| 2026-09-28 | — | read 0 V before first connection to XIAO #4 | 3.97 V after about half an hour on the charger; cause not determined |
| 2026-09-28 | same | unit reset ~84 times on the cell, then ~416 times with USB out | steady after the BAT+ joint was re-seated; DIAG 3.98 V / 82 %, controller 3978 mV at 16:33 |
| 2026-09-29 | same | 21 h on the cell, paired, USB out | 3978 mV, no restart (`RebootCount` 504 at both ends) |
