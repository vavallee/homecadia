# Hardware bring-up checklist

Every `HW-VERIFY` marker in source or docs gets a row here. Nothing ships
(milestone 6) with open rows. Parts are still in transit; this list grows as
pre-hardware code is written.

Bench wiring diagrams and the no-solder connectivity procedure live in
[diagrams/](diagrams/).

## Flash & console (verified on unit 1, 2026-08-08)

- [x] Flashes over native USB from WSL2 (usbipd path; see
      [build.md](build.md) for the gotchas). Chip: ESP32-C6FH4 (QFN32) v0.2 —
      FH4 = **4MB embedded flash, confirmed**; matches the `partitions.csv`
      4MB OTA layout.
- [ ] **OTA slot headroom.** App image is 0x195990 (1.66MB) of the 1.92MB
      (0x1E0000) OTA slot — **84% full, 16% free** as of 2026-08-23 with
      milestone 5–6 features still landing. Check after every feature merge; overflow forces a partition
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
- [x] Onboarding codes verified from a live console — **2026-08-23**: console
      printed `QRCODE: MT:Y.K9042C00KA0648G00` and the manual code 34970112332
      commissioned the device.

## Pins & wiring

- [ ] XIAO ESP32-C6 D-pin → GPIO mapping matches [pinmap.md](pinmap.md)
      (check against the official pinout diagram and a continuity test).
- [x] ePaper driver board V2 actually uses D0=RST, D1=CS, D2=BUSY, D3=DC,
      D8=SCK, D10=MOSI — **silkscreen-confirmed 2026-08-17** (back of the
      board labels the D0–D3/D8/D10 positions RST/CS/BUSY/DC/SCK/MOSI by
      function). **Electrically confirmed 2026-08-22** — the panel refreshed
      with these assignments. Bonus: the board breaks out D4/D5 as
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

- [x] **Panel draws — verified 2026-08-22** on driver board #1 with the XIAO
      seated directly (no jumpers), SHT40 on Grove. Full refresh holds BUSY
      high 1790ms, partial 540ms; readings render with `LOW BATT` (no cell
      fitted, 824mV on the divider reads as 0%). Image upright with
      `DISPLAY_FLIP_LONG_AXIS 1`, short axis 0.
- [ ] Panel deep-sleep current measured (panel + driver board leakage).
- [ ] Partial refresh charge cost measured; full refresh cost measured.
- [ ] Ghosting acceptable with chosen full-refresh-every-N policy.

### Bring-up post-mortem, 2026-08-18 → 22

Two days were spent probing pins for a fault that was mostly mechanical. What
actually went wrong, in the order it mattered:

1. **The FPC was in reversed.** Flipping the ribbon mirrors the pin order (tab
   *n* meets panel pin 25−*n*), which puts the panel's VDDIO on RST and VCI on
   BUSY — the MCU ends up driving a supply rail, and pulling RST low browns the
   board out. The insertion-force rule and the rest of the handling procedure
   are in [assembly.md](assembly.md#fpc-orientation-go-by-insertion-force-not-by-which-way-the-copper-faces).
2. **The ribbon was not seated to the stiffener.** It latched and made
   intermittent contact, which looks identical to a dead panel.
3. **The XIAO was seated 180° out** on board #2. USB-C points *away* from the
   FPC connector.
4. **The firmware could not tell a working panel from a silent one.** `busy_wait()`
   returned `ESP_OK` the moment BUSY read low, which is also the resting state
   of a panel that received nothing, so every refresh reported success while
   nothing was drawn. Fixed by requiring BUSY to *rise* after
   `CMD_MASTER_ACTIVATE` (`ssd1680.c`). Without that check the hardware fault
   was invisible from the log, which is why it was hunted with a multimeter.

Instrumentation that was wasted effort: pin-to-pin bridge sweeps, ADC line
voltages, and an output-drive test (which gave a false STUCK verdict on RST/CS
because configuring an ADC channel disconnects the digital driver). The one
useful measurement was the BUSY timing above.

### Component status, 2026-08-22

| Item | State |
|---|---|
| Panel A (first used) | **dead** — SDA shorted to VDDIO, 16–50Ω |
| Panel B | **dead** — same short, failed after one successful refresh |
| Panel C | **working** — the only good panel left |
| Driver board #1 | working; retains kΩ leaks (see [power-budget.md](power-budget.md)) |
| Driver board #2 | **connector damaged** — bridges tabs 14/15 (SDA↔VDDIO) at 19–49Ω with any ribbon seated, open with none. Not usable for a display |
| Driver board #3 | unopened, headers not fitted |
| XIAO (unit 1) | healthy — D10 to 3V3 open with the board removed |

Blocker: **2 of 3 panels are gone and there is no spare.** Reorder Seeed SKU
104990853 before the next two units are assembled.

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

- [x] Commissions to HA via ZBT-2 OTBR — **2026-08-23**, node 20, 14.8s once
      the preconditions held: `ENABLE_TEST_NET_DCL=true` on matter-server, the
      noble BLE proxy (`docs/ble-proxy-pod.yaml`), and the device in range of
      the border router. Readings visible in HA as `sensor.test_product_temperature`
      / `_humidity` after the MeasuredValue fix ([field-notes.md](field-notes.md) §9).
      Commissioning survives a reflash (NVS retains the fabric).
- [ ] Survives HA restart / OTBR restart without falling off the fabric.
- [ ] ICD: HA shows fresh readings at the configured report cadence. Partial
      **2026-08-23**: on the shipping profile (light sleep on) the device stays
      attached as a sleepy child and serves live reads over Thread 150s+ after
      boot. Cadence as seen from HA not yet observed over a longer window.
