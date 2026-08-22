# Power budget — sensor-01

Target: **≤300µA average excluding display refresh**, from a 2000mAh LiPo.
Modeled numbers below are estimates from datasheets and TinyENV's reported
behavior; the measured column gets filled in milestone 5 with a µA-capable
meter (e.g. Nordic PPK2 or a µCurrent) between battery and XIAO.

## Measured vs modeled

| Item | Modeled | Measured | Notes |
|---|---|---|---|
| Deep/light sleep floor (Thread ICD idle) | ~15–40µA | | C6 light sleep + RTC; dominates the average |
| Battery divider bleed | 2.1µA | | 4.2V / 2MΩ, continuous |
| SHT40 single-shot high-precision read | ~0.5ms·mA class, negligible avg | | ~8ms @ ~0.5mA every 120s |
| ADC battery read (incl. settling) | negligible | | settle delay then one-shot |
| Thread poll (ICD idle-mode poll) | ~10–30µA avg contribution | | depends on idle interval; radio rx window |
| Matter report (attribute change tx) | spike, small avg | | only on delta ≥0.2°C / ≥1%RH |
| Display partial refresh | ~? mC per refresh | | measure in M3: charge per refresh event |
| Display full refresh | ~? mC per refresh | | every N partials for ghosting |
| LED blink | avoided | | commissioning + low-battery only |
| **Average (no display)** | **≤300µA target** | | |

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
- LIT ICD: long idle interval, HA subscribes rather than device chattering.
