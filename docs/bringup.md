# Hardware bring-up checklist

Every `HW-VERIFY` marker in source or docs gets a row here. Nothing ships
(milestone 6) with open rows. Parts are still in transit; this list grows as
pre-hardware code is written.

## Flash & console (verified on unit 1, 2026-08-08)

- [x] Flashes over native USB from WSL2 (usbipd path; see
      [build.md](build.md) for the gotchas). Chip: ESP32-C6FH4 (QFN32) v0.2.
- [x] USB console stays enumerated with the app running —
      `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y` verified: port alive
      indefinitely; without it, dead in <2s (light sleep powers down USB-SJ).
- [x] Firmware boots on hardware (bare board: no SHT40/panel attached;
      sensor loop correctly refuses to start, node stays up).
- [ ] Onboarding codes verified from a live console (`idf.py monitor`,
      interactive) against the compiled-in test defaults (34970112332).

## Pins & wiring

- [ ] XIAO ESP32-C6 D-pin → GPIO mapping matches [pinmap.md](pinmap.md)
      (check against the official pinout diagram and a continuity test).
- [ ] ePaper driver board V2 actually uses D0=RST, D1=CS, D2=BUSY, D3=DC,
      D8=SCK, D10=MOSI (probe if display init fails).
- [ ] BAT+/BAT− underside pad markings confirmed on this XIAO revision.
- [ ] EEMB battery lead polarity measured with multimeter **before** connect.
- [ ] Battery divider on MTDI/GPIO5 reads plausible voltage (Vbat/2 ±5%).
- [ ] USB A-to-C cable charges the battery through the panel pigtail;
      charge LED on XIAO behaves as documented.

## Power & sleep

- [ ] GPIO6 (MTCK) wakes the C6 from deep sleep on encoder press.
- [ ] Sleep floor current measured (target table in
      [power-budget.md](power-budget.md)).
- [ ] ADC settling: reading stable with 2×1M + 100nF divider; calibrate
      against multimeter at 2–3 battery voltages.

## Display

- [ ] Panel deep-sleep current measured (panel + driver board leakage).
- [ ] Partial refresh charge cost measured; full refresh cost measured.
- [ ] Ghosting acceptable with chosen full-refresh-every-N policy.

## Radio / Matter

- [ ] Commissions to HA via ZBT-2 OTBR (uncertified-VID warning accepted).
- [ ] Survives HA restart / OTBR restart without falling off the fabric.
- [ ] ICD: HA shows fresh readings at the configured report cadence.
