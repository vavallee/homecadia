# Power budget — sensor-01

Target: **≤300µA average excluding display refresh**, from a 2000mAh LiPo.
Modeled numbers below are estimates from datasheets and TinyENV's reported
behavior; the measured column gets filled in milestone 5 with a µA-capable
meter (e.g. Nordic PPK2 or a µCurrent) between battery and XIAO.

## Measured vs modeled

| Item | Modeled | Measured | Notes |
|---|---|---|---|
| Deep/light sleep floor (Thread ICD idle) | ~15–40µA | **~376 µA** | 2026-09-15, PPK2 Source Meter 3700 mV, USB out, shipping image: median of radio-free seconds. ~10× the model. The ~1 kHz pulse train is the XIAO's SGM6029 buck in power-save mode delivering the load in bursts, not wakes; ~95 µA of the load was the modem power domain, held on through every sleep by the battery ADC unit; with that fixed (2026-09-15) the quiet floor is **~282 µA**. The rest was the TOP/peripheral domains: with the option on, the I2C/SPI buses asking for power-down, and the ADC read under a no-light-sleep lock, the floor is **52 µA** and the all-in average **230 µA** over 74 s with a radio burst (107 µA over a quiet 9 s) — 2026-09-15, shipping config (field-notes.md §21). Before the light-sleep fix the same floor was ~350 µA. |
| Awake floor, bench profile | — | **41 mA avg @ 3.4 V** | PPK2 Source Meter 2026-09-09: ~40 mA baseline, radio bursts to ~190 mA, 424 mA one-sample inrush at power-on. Not a budget line — it is what a sleep-disabled build costs, and it flattens a 2000 mAh cell in ~2 days |
| Unpaired, deep sleep after the window closes | — | **19.7 µA** | 2026-10-01, XIAO #4, PPK2 3700 mV, USB out: 19.71 and 19.85 µA over two 10 s windows, whole board. Entered 30 s after the 15-minute window closes; the dial wakes it (field-notes.md §27). An unpaired unit lasts years on a cell instead of days |
| Unpaired, advertising for commissioning | — | **28.1 mA avg** | 2026-09-28, XIAO #4, shipping image, PPK2 3700 mV, 10 s window: baseline 20–40 mA, ~200 mA spikes every 0.5 s. The chip does not light-sleep while unpaired (field-notes.md §26). Not a budget line; about 3 days on 2000 mAh |
| One detent, including its display refresh | — | **~52 mC** | 2026-09-28, node 26, paired and settled: 53.2 mC for the 10 s window holding the detent, against ~0.9 mC for a quiet 10 s. The chip is held awake 2 s (`ENC_AWAKE_MS`) at ~40 mA. About two settled 120 s cycles (24.9 mC each) |
| Battery divider bleed | 2.1µA | | 4.2V / 2MΩ, continuous |
| SHT40 single-shot high-precision read | ~0.5ms·mA class, negligible avg | | ~8ms @ ~0.5mA every 120s |
| ADC battery read (incl. settling) | negligible | | 20 ms settle then 64 one-shot samples, under a no-light-sleep lock (~25 ms awake per 120 s poll). The ADC input loads the 500 kΩ divider ~1.9 %; corrected by `VBAT_SCALE_X1000` in firmware (2026-09-22), not with a stiffer divider |
| Thread poll (ICD idle-mode poll) | ~10–30µA avg contribution | **~300 µC per poll** | 2026-09-15: 5 ms at 250–330 mA peak, every 5.0 s idle ≈ 60 µA avg; every 0.5 s in active mode ≈ 600 µA while it lasts. | depends on idle interval; radio rx window |
| Matter report (attribute change tx) | spike, small avg | | only on delta ≥0.2°C / ≥1%RH |
| Display partial refresh | ~? mC per refresh | | measure in M3: charge per refresh event |
| Display full refresh | ~? mC per refresh | | every N partials for ghosting |
| LED blink | avoided | | commissioning + low-battery only |
| Inter-pin leakage (post-wash) | <1µA | | flux residue washed 2026-08-31; a 100kΩ path would add ~0.2µA at 0.5% refresh duty — re-scan if refresh behaviour changes |
| **Average (no display)** | **≤300µA target** | **208 µA settled / 238 µA full run** | 2026-09-17, 12 h 37 min PPK2 soak at 3700 mV with the slow poll at an effective 15 s: **237.7 µA** over the whole run (10.79 C, includes boot, attach and three interviews at ~0.15 C each), **207.8 µA over one settled 120 s cycle** (24.94 mC), floor 59 µA. The day before at 5 s: 288.5 µA / 240.6 µA. Per cycle now: floor ~59 µA, seven parent polls ~35 µA, the active period around each 120 s report ~113 µA (field-notes.md §22). ≈10 months on 1700 mAh, ≈12 on 2000, at the full-run figure. |
| **Average, paired, SIT ICD** | **≤300µA target** | **185 µA settled / 225 µA incl. one 2 s receive window** | 2026-10-05, node 28 (XIAO #4), PPK2 at 3700 mV on the BAT wires, capture `ppk2-20261005T001646` analysed with `tools/ppk2-events.py`, 620–7230 s: floor 73 µA, parent polls 62 µA (4.4/min at ~0.76 mC), reports 40 µA (16 in 110 min, ~14 mC each with the panel refresh), short wakes 6 µA. Same unit on the LIT build 2026-10-04 (`ppk2-20261004T190206`): **307 µA** — floor 76, polls 35, a report every 2 min 69, 5 s of 500 ms fast polls after each 55, a 60 s short-idle wake 12, panel refreshes the rest. The change: LIT off, active-mode threshold 5000 → 300 ms, idle interval 120 → 600 s (`sdkconfig.defaults`). A 2.0 s receive window at 139 mA (282 mC) appeared once, ~550 s after boot, in two captures; with no attach attempt or parent change in the Thread counters. At 185 µA: ≈12.5 months on 2000 mAh at 85 % usable. |
| **Average, paired, SIT ICD, switch pad fixed** | **≤300µA target** | **125 µA settled** | 2026-10-05, node 28, PPK2 at 3700 mV, capture `ppk2-20261005T033134`, 600–2900 s: floor 57 µA (drifting 50 → 58 over 50 min), parent polls 43 µA (~0.48 mC each), reports 22 µA, no stray wakes (1 in 49 min). The fix: the push switch on HP GPIO20 keeps its awake pad config through light sleep (`ui.cpp` `push_switch_init()`, field-notes.md §28); it had cost ~16 µA of floor, ~0.28 mC on every poll and up to 120 stray wakes a minute since 7d25512. At 125 µA: ≈18 months on 2000 mAh at 85 % usable. |

## Months-of-battery calculator

```
usable_mAh = 2000 × 0.85                                  # brown-out floor + aging margin
display_mA = refreshes_per_day × mC_per_refresh / 86400   # refresh charge spread over the day
avg_mA     = sleep_avg_mA + display_mA
months     = usable_mAh / avg_mA / 730                    # 730 h per month
```

Worked example at target: 1700mAh / 0.3mA / 730 ≈ **7.8 months**, before
display refresh cost and LiPo self-discharge (~2–3%/month) are added. Display
refresh budget therefore matters: if a partial refresh costs ~10mC and the
display refreshes 30×/day, that adds ~3.5µA average — fine. 500 refreshes/day
would add ~58µA — not fine. Refresh policy (only on wake/report/dial) exists
because of this table.

## Known leakage on driver board #1 (measured 2026-08-22)

Board #1 survived bring-up but carries resistive leaks to the rails. Every GPIO
still drives valid logic through them — the C6's push-pull output is ~25Ω
against kΩ-scale leaks, so this is a power problem, not a functional one:

| Net | Leak | Measured |
|---|---|---|
| RST (D0) | to GND | ~0.6–1.2kΩ |
| CS (D1) | to GND | ~9.5–11.6kΩ |
| BUSY (D2) | to GND | kΩ-scale |
| MOSI (D10) | to 3V3 | weak, ~20–50kΩ |

The RST leak is the one that matters: RST is held **high** through the panel's
deep sleep, so 3.3V across ~600Ω is ~5.5mA continuous — roughly **20× the
entire 300µA budget**, and it would flatten a 2000mAh cell in about two weeks.

Board #1 is therefore a **bench board, not a shipping board**. Measure RST-to-GND
on boards #2 and #3 before committing either to a unit, and re-measure any board
that has had rework on the header pins.

## Firmware policies that exist because of this budget

- 2×1MΩ divider, not 100k (21µA → 2.1µA bleed).
- Report on delta, not on every poll.
- Panel deep sleep between refreshes; refresh only on wake/report/dial input.
- Full refresh only every N partials.
- LED only for commissioning state + low battery.
- SIT ICD, 600 s idle interval: a report with nothing new goes out every 10 min.
  LIT was dropped 2026-10-04: Home Assistant registers no check-in client, so
  LIT only added its 5 s minimum active threshold and a 60 s short-idle wake.
