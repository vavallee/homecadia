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
- [x] **Driver board is a pure pass-through on every data line — schematic-
      verified 2026-08-25** (`ePaper_Driver_Board.pdf`, Seeed, rev 1.0,
      2024-12-09): CN1/CN2/CN4 route D0–D10 straight to the FPC or to the
      header pins with no pull-ups, series resistors, buffer or level shifter.
      D4/D5/D6/D7/D9 reach nothing on the board except the header. The slide
      switch (CN6) is in the **battery** path only — it does not gate 3V3 or
      the panel. So any pull-up seen on D3/D8/D10 is the panel's, and a
      "held HIGH" on D4/D5 is coupling from a neighbour, not a board pull-up
      (an earlier row here claimed I2C pull-ups on D4/D5; it was wrong).
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
- [x] **FPC seating is the least reliable joint — re-verified 2026-08-25.** A
      misseated ribbon put a panel supply rail onto the MOSI net: D10 sat at
      3.1V and could not be driven low, so every byte clocked to the panel was
      `0xFF` and it went deaf. It read as a dead panel, and the stale QR left on
      the glass read as an unpaired device; neither was true. **Check the
      connection before the firmware** — see [field-notes.md](field-notes.md)
      section 15. `ssd1680_init()` now scans and drive-tests every panel signal
      at boot; `drive hi=1 lo=0 follows the driver` on all five outputs is the
      precondition for anything else being worth investigating.
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

## Encoder & LED

- [x] **LED drives — verified 2026-08-25/26** on D7/GPIO16→17 (see below),
      330 Ω, harness-scan drive test `hi=1 lo=0`; the low-battery pulse
      (`firmware/sensor-01/main/led.cpp`, 100 ms on / 10 s gap while the battery
      reads 0 % with no divider fitted) is visible.
- [x] **Encoder rotation — verified 2026-08-26.** Clean quadrature on both
      lines (`(1,0)→(0,0)→(0,1)→(1,1)`), 11 decoded events in one back-and-forth
      run, zero phantom events with the knob still. Only after the three encoder
      pins were **soldered to leads**: EC11 blades are ~0.6 mm and seat in
      neither breadboard springs nor Dupont sockets — every earlier run had one
      line dead or intermittent.
- [x] **Clockwise = view advances — verified 2026-08-26.** With A on D6 it read
      `dir=-1`, so A/B were swapped in `app_config.h` (A=D9, B=D6), not on the
      board. About 1 detent in 4 is dropped on a slow turn — one lost quadrature
      transition per run; acceptable for a view selector, noted for later.
- [x] **Pin move: encoder off D7.** On both driver boards D7/GPIO17 read 0 V
      from the moment `display_init()` brought the SPI bus up, and a 45 k
      internal pull-up could not lift it; the meter read it open with no power.
      **Explained 2026-08-31:** plumbing-flux residue conducting between the
      adjacent D7/D8 socket joints under bias — field-notes.md section 17. The
      pin assignment stays (LED on D7, encoder A/B on D9/D6): it is verified
      and there is no reason to churn it.
- [x] **Boards #1/#2 retired; board #3 washed and clean — 2026-08-31.** Flux
      residue (AIM Nitro, a plumbing flux) put drifting kΩ paths between
      adjacent socket pins on every board soldered with it. Two-stage wash
      (IPA, then hot soapy water + distilled rinse) on board #3: pin-hold
      metered flat 3.1 V on D9 through every phase and the full stack scanned
      `no pin follows any other` — panel, SHT40 (0x44, readings on the loop),
      encoder (one detent CW `dir=+1`, CCW `dir=-1`, zero idle events) and LED
      all on one reset. XIAO #1 died to a source-meter injection during the
      hunt (field-notes.md section 18); XIAO #2 is in service.
- [x] **Encoder events during a panel refresh — 0, verified 2026-08-31.**
      Three refreshes across ~30 min of monitoring on washed board #3, knob
      untouched, zero `on_rotate` (the only events all session were the two
      deliberate detents). Backed by the direct measurement: post-wash, D9
      metered a flat 3.1 V with MOSI and SCK each driven low — no path left
      to fake an edge. (The pre-wash D7↔D8 flag was flux residue,
      field-notes.md section 17.)
- [ ] GPIO6 (MTCK) push switch — deferred until the underside pad is
      soldered; the deep-sleep-wake row under "## Power & sleep" stays open.
- [x] Harness scan: `firmware/sensor-01/main/bench_selftest.cpp` logs every
      pin's electrical state at boot in the bench profile. Read it first
      after any wiring change; all six outputs must say "follows the driver"
      before anything else is worth investigating.

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

- [x] Commissions to HA via ZBT-2 OTBR — **re-verified 2026-08-24 on
      matter-server 1.4.0**, node 23, 17.7-19.0s across three runs, through
      **Home Assistant's own BLE proxy**. The separate noble pod is no longer
      needed: the BTP-handshake failure was matter-js/matterjs-server#1006,
      fixed upstream in v0.8.0, and this deployment had been pinned to 0.7.1.
      Remaining precondition is `ENABLE_TEST_NET_DCL=true`, plus BLE and Thread
      both reaching the device. Commissioning survives a reflash (NVS retains
      the fabric). Readings verified over Thread after the MeasuredValue fix
      ([field-notes.md](field-notes.md) §9).
- [x] Device identity correct on the controller — **2026-08-24**: reads
      `homecadia` / `sensor-01` / `xiao-c6/driver-v2`. Note this only took
      effect after a re-commission; a reflash alone does not update it, because
      the controller caches BasicInformation from the interview
      ([commissioning.md](commissioning.md)).
- [x] Un-pairing recovers without physical access — **2026-08-24**, `7895168`.
      `remove_node` makes the device reboot itself and re-advertise (2884 FFF6
      reports on the controller's adapter, unattended). Before the fix the same
      operation left it silent, twice ([field-notes.md](field-notes.md) §12).
- [ ] Survives HA restart / OTBR restart without falling off the fabric.
- [ ] ICD: HA shows fresh readings at the configured report cadence. Partial
      **2026-08-23**: on the shipping profile (light sleep on) the device stays
      attached as a sleepy child and serves live reads over Thread 150s+ after
      boot. Cadence as seen from HA not yet observed over a longer window.
