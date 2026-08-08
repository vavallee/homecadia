# Hardware bring-up checklist

Every `HW-VERIFY` marker in source or docs gets a row here. Nothing ships
(milestone 6) with open rows. Parts are still in transit; this list grows as
pre-hardware code is written.

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
