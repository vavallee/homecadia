# Hardware bring-up checklist

Every `HW-VERIFY` marker in source or docs gets a row here. Nothing ships
(milestone 6) with open rows. Parts are still in transit; this list grows as
pre-hardware code is written.

## Flash & console (verified on unit 1, 2026-08-08)

- [x] Flashes over native USB from WSL2 (usbipd path; see
      [build.md](build.md) for the gotchas). Chip: ESP32-C6FH4 (QFN32) v0.2 —
      FH4 = **4MB embedded flash, confirmed**; matches the `partitions.csv`
      4MB OTA layout.
- [ ] **OTA slot headroom.** Current app image is 1.66MB of the 1.92MB
      (0x1E0000) OTA slot — **~84% full** with milestone 5–6 features still
      landing. Check after every feature merge; overflow forces a partition
      rework and a full reflash of deployed units (`partitions.csv` note).
- [x] USB console stays enumerated with the app running —
      `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y` verified: port alive
      indefinitely; without it, dead in <2s (light sleep powers down USB-SJ).
- [x] Firmware boots on hardware — **re-verified 2026-08-17 after fixing a
      boot loop the 2026-08-08 check missed.** `abort()` fired ~1.3s after
      `app_main()`: `CONFIG_ICD_ACTIVE_MODE_THRESHOLD_MS=1000` is below the
      5s LIT ICD minimum enforced by a VerifyOrDie in connectedhomeip
      `ICDManager.cpp:82`. The chip had accumulated **11,374 reboots** by the
      time the loop was caught on the bench (NVS reboot-count), so a
      seconds-long look at a boot banner is not a boot verification — watch
      the console ≥30s. Now boots to `Server Listening...` with the SHT40
      attached.
- [ ] Onboarding codes verified from a live console (`idf.py monitor`,
      interactive) against the compiled-in test defaults (34970112332).

## Pins & wiring

- [ ] XIAO ESP32-C6 D-pin → GPIO mapping matches [pinmap.md](pinmap.md)
      (check against the official pinout diagram and a continuity test).
- [x] ePaper driver board V2 actually uses D0=RST, D1=CS, D2=BUSY, D3=DC,
      D8=SCK, D10=MOSI — **silkscreen-confirmed 2026-08-17** (back of the
      board labels the D0–D3/D8/D10 positions RST/CS/BUSY/DC/SCK/MOSI by
      function). Electrical confirmation comes free when the display first
      draws; probe only if init fails. Bonus: the board breaks out D4/D5 as
      labelled through-holes — a candidate solder point for the SHT40 in
      final assembly instead of the XIAO pins.
- [ ] BAT+/BAT− underside pad markings confirmed on this XIAO revision.
- [ ] EEMB battery lead polarity measured with multimeter — **all 3 cells,
      before first connection** ([assembly.md](assembly.md) warning).
- [ ] Battery divider on MTDI/GPIO5 reads plausible voltage (Vbat/2 ±5%).
- [ ] USB A-to-C cable charges the battery through the panel pigtail;
      charge LED on XIAO behaves as documented.

## Power & sleep

- [ ] GPIO6 (MTCK) wakes the C6 from deep sleep on encoder press.
- [ ] Sleep floor current measured (target table in
      [power-budget.md](power-budget.md)). Seeed's ~15µA figure is optimistic
      and regulator-dependent — treat as unverified
      ([source-reliability.md](source-reliability.md)).
- [ ] ADC settling: reading stable with 2×1M + 100nF divider; calibrate
      against multimeter at 2–3 battery voltages.

## Display

- [ ] Panel deep-sleep current measured (panel + driver board leakage).
- [ ] Partial refresh charge cost measured; full refresh cost measured.
- [ ] Ghosting acceptable with chosen full-refresh-every-N policy.

## Case & mechanical

- [ ] Encoder knob bore is 5.87×6.10mm against the 6mm D-shaft — tight;
      expect to ream post-print. Test on the single test set before
      committing to 3 sets.
- [ ] Test-fit on ONE printed set: panel, driver board, encoder bushing, USB
      pigtail bezel, #6 screw head in keyhole slot
      ([hardware/case](../hardware/case/README.md) tolerances are derived
      from published dims, not test-fitted).
- [ ] **rev 2 stretch verified in the flesh**: the 56mm LP103454 cell and the
      48.5mm insert-tray both sit in the 106.64mm interior with the lid closing
      (2.1mm modelled clearance — confirm nothing else eats it).
- [ ] Front and back still mate after the stretch: corner screws line up, seam
      closes, display aperture centres on the panel.
- [ ] Wall screws drilled at **92mm** centres, not the rev 1 85mm.
- [ ] Battery and SHT40 retention decided — the cavity has no cradle and no
      sensor mount; today both are foam tape / zip tie by default.
- [ ] Laser-engraved wordmark on the test set: depth legible, no scorching,
      placement matches `hardware/case/artwork/wordmark-placement.png`
      (54.09mm from the left edge, 3.07mm up).
- [ ] USB-C pigtail seats: the Gebildet connector wants a Ø12mm hole and the
      case has Ø12.8mm, so the M11 nut clamps with ~0.8mm slop — confirm it
      holds square and doesn't rotate in use.

## Radio / Matter

- [ ] Commissions to HA via ZBT-2 OTBR (uncertified-VID warning accepted).
- [ ] Survives HA restart / OTBR restart without falling off the fabric.
- [ ] ICD: HA shows fresh readings at the configured report cadence.
