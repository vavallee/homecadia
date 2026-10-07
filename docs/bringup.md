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
      milestone 5–6 features still landing. **2026-10-05, shipping build at
      `9ccc68b` on the pinned image: 0x195100 (1,659,136 bytes), 84 % full,
      0x4AF00 (306,944 bytes) free** — 2,192 bytes smaller than on 2026-08-23
      despite the dial wake, unpaired deep sleep and the Thread diagnostics
      counters. Bootloader 0x56A0, 55 % of its slot free. Check after every feature merge; overflow forces a partition
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
- [x] BAT+/BAT− underside pad markings confirmed on this XIAO revision —
      **2026-09-28**, XIAO #4 (node 26): runs from a 3700 mV source on those
      pads and reports 3682 mV.
- [x] **Underside signal wires proven on the right pads — 2026-09-28**, XIAO
      #4, by the unpowered exclusion tests and the powered tests in
      [assembly.md](assembly.md) stages 2–3. On XIAO #3 the encoder wire was
      on the 3V3 pad beside MTCK ([field-notes.md](field-notes.md) §25).
- [ ] EEMB battery lead polarity measured with multimeter — **all 3 cells,
      before first connection** ([assembly.md](assembly.md) warning).
- [x] Battery divider reads Vbat/2 — on **MTMS/GPIO4**, not MTDI (see the
      Encoder & LED section rows dated 2026-09-02 and 2026-09-12).
- [ ] USB A-to-C cable charges the battery through the panel pigtail;
      charge LED on XIAO behaves as documented.

## Power & sleep

- [x] ~~GPIO6 (MTCK) wakes the C6 from deep sleep on encoder press.~~
      **Superseded 2026-09-23, closed 2026-09-28.** The firmware sleeps in
      light sleep, not deep sleep, and GPIO6 carries encoder A, not the
      switch. A detent wakes the chip: see "Dial works on battery" under
      Encoder & LED.
- [x] **Runs on a real cell — 2026-09-28/29, 21 h soak.** Node 26 (XIAO #4,
      shipping image, paired) on an EEMB LP103454 through the BAT pads, USB
      out. 2026-09-28 16:33: 3978 mV / 82 %, `RebootCount` 504. 2026-09-29
      13:42: **3978 mV / 82 %, `RebootCount` 504**, available throughout; the
      matter server logged no availability change, timeout or unresponsive
      peer for the node in those 21 h. The voltage is re-measured with every
      report (`sensor_loop.cpp`), and temperature and humidity moved in the
      same reports, so the reading is live. It agrees with the DIAG view
      (3.98 V) and a meter (3.97 V). What it shows: no heavy drain (28 mA
      would have taken ~590 mAh, about 30 %). What it cannot show: the
      238 µA figure itself, which predicts ~5 mAh (0.25 %) in 21 h, inside
      the reading's resolution. A week-long read is the test for that. The
      battery path is still through breadboard contacts, which caused a
      reset loop before the soak ([battery-runbook.md](battery-runbook.md)
      B3).
- [x] **Unpaired unit deep-sleeps and the dial wakes it — verified
      2026-10-01/02**, XIAO #4. `app_main.cpp`, `unpaired_sleep_check()`
      and `unpaired_sleep_on_boot()`. When the 15-minute commissioning window
      has closed with no fabric (or 16 minutes after boot, whatever the window
      reads, unless a pairing holds the fail-safe), the unit draws the
      onboarding screen with "Asleep. Turn the dial to start pairing.",
      restarts, and enters deep sleep from the top of the next boot, before
      Bluetooth, Thread, the PHY or power management exist. Encoder A (GPIO6,
      an LP pin) wakes it; a woken boot releases the pad hold and restarts
      once more, cleanly. Evidence: **19.71 µA and 19.85 µA** in deep sleep
      (PPK2 3700 mV, USB out, two 10 s windows); dial wake 5 of 5 on battery
      and 5 of 5 on USB, every wake booting in under 4 s; the real path on the
      shipping image: power on 23:46, window closed ~00:01, flat sleep line
      30 s later; two pairings completed, one with the window closing about
      55 s into it (node 28, 2026-10-02). Three earlier designs failed and are
      in [field-notes.md](field-notes.md) §27. Not covered: a unit removed
      from its controller (same reboot path, not re-tested). Bench profile:
      off (`CONFIG_HOMECADIA_UNPAIRED_DEEP_SLEEP`); `tools/check-profiles.sh`
      asserts it is on, and the test shortcut is 0, in shipping.
- [x] **Unpaired current — measured 2026-09-28: 28.1 mA average.** Shipping
      image on XIAO #4 before commissioning, PPK2 3700 mV, 10 s window:
      baseline 20–40 mA, ~200 mA spikes every 0.5 s (the 500 ms pairing
      advertisement interval). The chip does not enter light sleep while
      unpaired; cause not established. The `ot_sleep` lock first suspected
      (field-notes.md §26) is `ESP_PM_APB_FREQ_MAX` and does not block light
      sleep; that explanation was retracted 2026-10-02. Worked around: an
      unpaired unit deep-sleeps once its window closes (row above). Three minutes after
      commissioning the same unit averaged **91 µA** over a quiet 10 s with
      one parent poll in the window.
- [x] Light sleep does not hang — **fixed 2026-09-15** by turning off
      `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP`. ESP-IDF v5.5.5 marks it
      experimental (default n), but Espressif's own C6 sleepy references turn
      it on — esp-matter `examples/icd_app`, IDF `ot_sleepy_device/light_sleep`
      — and the milestone-1 scaffold copied `icd_app`. With it on,
      the first real light-sleep run (PPK2 Source Meter, USB out) stopped
      waking 8 min after boot, at a 120 s sensor poll: 10–33 µA, no wakes at
      all, dropped by its Thread parent, marked unavailable by the controller.
      With it off — the only difference, sdkconfigs diffed — 38 min on the
      PPK2 with no radio-poll gap over 5.0 s and no subscription loss. A USB
      run cannot show this: `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y` keeps the
      chip out of light sleep while a host is attached
      ([field-notes.md](field-notes.md) §20). **Reproduced 2026-09-15** with
      the ADC fix in and the TOP domain genuinely powering down: stopped again
      at the 4th 120 s poll, ~480 s after boot. **Cause found the same
      evening** — a light sleep landing inside the battery ADC sequence, which
      changes the sleep power-domain config as the oneshot unit is created and
      deleted. Fixed with an `ESP_PM_NO_LIGHT_SLEEP` lock in `battery.cpp`;
      the workaround (option off) is retired.
- [x] Sleep current measured — **2026-09-15, over budget.** Shipping image
      (with the fix above), PPK2 Source Meter 3700 mV, USB out, no cell, 38 min
      settled: **696 µA average**, radio-free seconds **~376 µA** median.
      Target ≤300 µA ([power-budget.md](power-budget.md)); projects to ~3.3
      months on 1700 mAh usable. Two separate costs, both open: the floor
      between polls is ~10× the modeled 15–40 µA (the ~1 kHz pulse train is
      the XIAO's SGM6029 buck in power-save mode, not wakes; ~95 µA of the load
      behind it was the modem power domain, held on by the battery ADC — see
      the next item), and every 20–40 s
      a 30 ms transmission drops the device into ICD active mode, 500 ms fast
      polls for the 5 s threshold, with no controller exchange to explain it.
      A 150 s capture on the border router's `wpan0` saw one Matter exchange
      (the 2 min keepalive), so the episodes stay on the link to the parent,
      which reports a 20% frame error rate to the sensor at −77 dBm. Our
      active-mode threshold is 5 s against `icd_app`'s 1 s for SIT (the device
      runs SIT: no check-in client registered), so each episode costs ~5× the
      reference's.
      Seeed's ~15µA figure remains unverified
      ([source-reliability.md](source-reliability.md)).
- [x] **Modem power domain off in sleep — fixed 2026-09-15, worth ~95 µA.** A diagnostic image (`CONFIG_HOMECADIA_SLEEP_DIAG`) showed
      the chip asleep 98–99% of the time with ~2 timer wakes/s and no pin
      wakes, but the modem power domain **on for 100% of sleep time**. Cause:
      `battery.cpp` held its oneshot ADC unit from boot, and on the C6
      `adc_oneshot_new_unit()` pins the modem domain on until the unit is
      deleted (`ADC_LL_ADC_FE_ON_MODEM_DOMAIN`, `esp_adc/adc_oneshot.c`). Fix:
      create and delete the unit around each reading. Measured with the fix:
      modem on 0% of sleep time, quiet floor **376 → 282 µA**. Still ~225 µA
      above Espressif's 55 µA reference.
- [x] **Quiet floor near the reference — 52 µA, 2026-09-15.**
      The TOP domain only powers down once the I2C and SPI buses ask for it
      (`flags.allow_pd`, `SPICOMMON_BUSFLAG_SLP_ALLOW_PD`; added 2026-09-15 to
      `sht40.c` and `ssd1680.c`) *and*
      `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y`. With both: flags
      `0x20007c17`, quiet floor **52 µA**, all-in average **230 µA** over 74 s
      with a heavy radio burst in it. The §20 hang that this option used to
      bring was bisected the same evening to a light sleep landing inside the
      battery ADC sequence; `battery.cpp` now holds an `ESP_PM_NO_LIGHT_SLEEP`
      lock across it (20 min clean, full poll). Option back on in
      `sdkconfig.defaults` ([field-notes.md](field-notes.md) §21).
- [x] **Overnight soak of the shipping image — 2026-09-16, no hang.**
      `0.6.0-dev.84+e0760b0`, PPK2 3700 mV, 15:18 → 22:56 (7 h 39 min), node 25
      answered a fresh `interview_node` at +85 min, +7.7 h and at the end.
      Whole run **288.5 µA** (7.94 C); one settled 120 s cycle **240.6 µA**;
      quiet floor 59 µA. Radio is now ~155 µA of the 240: the 5 s slow poll
      (~22 × ~0.58 mC per cycle) and the ~10-poll fast tail after each report.
      Slow poll raised 5 s → 30 s in `sdkconfig.defaults`
      ([field-notes.md](field-notes.md) §22).
- [x] **Slow poll re-measured — 2026-09-17.** 12 h 37 min, no hang. Polls
      land 15 s apart, not 30: no ICD client is registered, so the stack runs
      SIT mode and clamps. Whole run **237.7 µA**, settled 120 s cycle
      **207.8 µA** (was 288.5 / 240.6). One `interview_node` costs ~150 mC,
      about ten minutes of normal running — keep liveness checks rare.
- [x] **ADC calibration at two voltages — 2026-09-12.** *(Superseded
      2026-09-22 by the shipping-image calibration below; `VBAT_OFFSET_MV` no
      longer exists.)* PPK2 as the cell
      (Source Meter, USB out). True BAT+ 3.36 V → firmware 3.02 V; true
      3.97 V → firmware 3.64 V. Same 0.33–0.34 V short at both points, slope
      1.02: a **constant offset, not a gain error**. ~0.17 V at the pin across
      the divider's 500 kΩ source impedance = ~0.34 µA of ADC input current.
      Divider itself is exact (SENSE tracks the source at 0.476× on a 10 MΩ
      meter, the correct number for 1M+1M). Fix is `+340 mV` in
      `battery_read_mv()` — **applied 2026-09-13** as `VBAT_OFFSET_MV`
      (bench image, flashed from Windows, fabric kept). The same USB-no-cell
      source read 3972 mV before the flash and 4308 mV after, a +336 mV step:
      proof the offset is in the image, not a third calibration point — the
      charge chip's no-cell output was never metered. A 0 reading stays 0.
- [x] **Battery ADC calibrated on the shipping image — 2026-09-22.**
      `VBAT_OFFSET_MV` is gone; `battery.cpp` now scales the pin reading by
      `VBAT_SCALE_X1000 2038` (ideal 1M:1M is 2000; the ADC input loads the
      500 kΩ source ~1.9 %). Also `SAMPLES` 8 → 64 and `VBAT_ADC_SETTLE_MS`
      5 → 20: report-to-report scatter fell from ±120 mV to 4 mV. PPK2 source,
      BAT+ metered in-circuit, settled Matter reports of `3/47/11`:

      | BAT+ | Reported | Error | Role |
      |---|---|---|---|
      | 3.68 V | 3616 → ×1.0176 (n=5) | — | fit |
      | 4.00 V | 3921 → ×1.0201 (n=3) | — | fit |
      | 3.39 V | **3389, 3387** | −1 to −3 mV | check, fitted image |

      The first report after each boot reads ~80 mV low (3307 at 3.39 V;
      3536 at 3.68 V on the previous image); cause not established. Earlier
      2026-09-22 this row was nearly closed with an offset of 0 from a
      comparison against the divider midpoint — wrong, see field-notes §23.

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
- [x] **Panel deep-sleep current — bounded 2026-10-06, not isolated.** The
      driver sends the panel to deep sleep (command 0x10, mode 1) after every
      refresh (`ssd1680.c:201-204`, `:247`). Whole unit in deep sleep at
      3700 mV: 21.2 µA (field-notes.md §29). Known parts: ESP32-C6 7 µA
      (datasheet v1.5 Table 5-11), SGM6029 2.3 µA (datasheet p.5), divider
      1.85 µA. Panel, driver board and SHT40 together: **≤ ~10 µA**. Isolating
      it needs the XIAO unseated from the driver board's header (3V3 runs
      through a header pin); not worth it against a 108–125 µA average.
- [x] **Partial and full refresh charge — measured 2026-10-04/05**, node 28:
      partial ~6–7.5 mC, full ~19 mC ([power-budget.md](power-budget.md)).
- [x] **Ghosting acceptable at `DISPLAY_FULL_REFRESH_EVERY_N` 10 — 2026-10-06**,
      by the builder after weeks on the readings screen: no leftover digits
      between full refreshes. The brief dark screen on boot and on wake is
      the full refresh's clearing pass, not a fault.

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
      reads 0 % with no divider fitted) is visible. **That check ran on a
      build that never light-slept.** On the sleeping shipping build the pulse
      was two ~2.5 ms flashes; fixed with the pad hold and re-verified
      2026-10-06 at 3500 mV as a single full flash
      ([field-notes.md](field-notes.md) §29).
- [x] **Low battery and empty battery — verified 2026-10-05/06** on node 28
      with a PPK2 sweep 3700 → 2900 mV: warning below 3530 mV, every
      percentage point reported below 10 %, and below 3100 mV a "BATTERY
      EMPTY" screen and deep sleep instead of a reset loop; the dial brings
      it back once the supply is restored (field-notes.md §29).
- [x] **Encoder rotation — verified 2026-08-26.** Clean quadrature on both
      lines (`(1,0)→(0,0)→(0,1)→(1,1)`), 11 decoded events in one back-and-forth
      run, zero phantom events with the knob still. Only after the three encoder
      pins were **soldered to leads**: EC11 blades are ~0.6 mm and seat in
      neither breadboard springs nor Dupont sockets — every earlier run had one
      line dead or intermittent.
- [x] **Dial works on battery — found broken 2026-09-15, fixed 2026-09-23,
      verified 2026-09-28.** On the PPK2 (no USB) a detent did nothing: the
      encoder used edge interrupts (`components/ec11_encoder/ec11.c`), which
      cannot wake light sleep, and with
      `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP` only LP GPIOs 0–7 can
      wake at all. Fix: encoder A on the MTCK pad (GPIO6), armed with
      `esp_deep_sleep_enable_gpio_wakeup()` for the level it is not at, and a
      no-light-sleep lock held for `ENC_AWAKE_MS` (2 s) after the last
      change. Evidence, XIAO #4 / node 26, PPK2 3700 mV, USB out, paired and
      settled: flat baseline (91 µA average over 10 s), then one detent gave
      a ~2 s burst at ~40 mA with peaks to 570 mA, the screen changed view,
      and the baseline returned. 53.2 mC for the 10 s window containing the
      detent and its refresh. On USB with the bench image the same unit
      decoded 6 of 6 detents, three each way.
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
- [x] **Battery divider wiring — 2026-09-02, re-verified 2026-09-12.** 2×1M +
      100nF on **MTMS/GPIO4** (moved from MTDI/GPIO5 after that pad lifted on
      XIAO #2). The 2026-09-02 check (meter 1570mV at the pin vs 1551mV from
      `battery_read_mv()`, "1.2% apart") was a **false pass**: 1.57 V on a
      10 MΩ meter is 3.30 V at the top of the divider, while BAT+ was
      recorded at ~3.1 V that day — the meter and the ADC were both reading
      low, by different amounts, and happened to agree. A single-point check
      cannot tell a correct divider from one landed on the 3V3 rail when both
      nodes sit near 3.3 V. The sweep below can.
- [x] **Divider against a battery voltage — 2026-09-12.** PPK2 substituted
      for the cell (Source Meter into BAT+/BAT−, USB out). In-circuit, black
      on PPK2 GND: source 4000 → BAT+ pad 3.97, SENSE 1.87; source 3400 →
      SENSE 1.60. SENSE follows the source at 0.476× (10 MΩ meter across the
      lower 1M) — the divider is on BAT+ and exact. Firmware offset is the
      separate row under Power & sleep. Encoder confirmed changing views in
      both directions during the same session. Wiring reference:
      [diagrams/bench-verify.html](diagrams/bench-verify.html).
- [x] **Push switch on D9/GPIO20 — verified 2026-09-28** on USB, bench
      image, XIAO #4: `on_push` logged per press. D9 is an HP pin, so a press
      registers only while the chip is awake: turn first, then press within
      2 s. **On battery 2026-09-28** (node 26, PPK2 3700 mV, USB out, paired):
      turned to SETTINGS, pressed inside the window, the menu opened and both
      items (poll interval, units) could be selected and edited.
- [x] **XIAO inventory — 2026-09-28.** #1 dead (source-meter injection,
      field-notes.md §18). #2 lost the MTDI, MTDO and MTCK pads; node 25,
      retired. #3 had the encoder wire on the 3V3 pad, then lost pads during
      handling after its hot glue was removed for rework. #4 is node 26.
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
- [ ] **rev 5 depth verified in the flesh**: the cell flat on the floor, the
      tray on it, the XIAO stack with the driver-board pins trimmed to ~1.5 mm
      and the panel — ~30.8 mm of parts in ~33.6 mm — with the lid closing.
      Measure the panel's thickness first; it is the one part not measured.
      (Replaces the rev 2 check, which counted length only.)
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
- [x] XIAO #2 commissions the same way — **2026-09-12**, node 25, about 4s
      from `commission_with_code` to first interview. Device on USB from
      `k8sn1-master` (power + BLE range in one plug), the API driven from the
      office over the LAN; one trip to the basement. First read over Thread:
      21.72°C / 48.39%RH / 3968mV reported (charge-IC output on USB with no
      cell, through the uncorrected ADC — not a cell voltage). Child table:
      RLOC16 0x681d, LQ In 3. The iOS companion-app route was tried first the
      same evening and failed exactly as [commissioning.md](commissioning.md)
      predicts — see its troubleshooting table for the two dialog texts.
- [x] Office link to the basement ZBT-2 is reliable — **yes, through a
      Thread router (2026-09-13)**. Without one, node 25 attached from the
      2nd-floor office (bench profile, USB, bare breadboard) at −83 dBm /
      LQ In 2, then −89 to −90 dBm / LQ In 1 after the reflash, and the
      controller lost it twice in six minutes (`peer-unresponsive`, then a
      subscription timeout). The OTBR transmits at 5 dBm (`ot-ctl txpower`)
      against the ESP32-C6's 20 dBm, so the border router's downlink is
      probably the weak direction (inferred).
      Fix: a bare XIAO ESP32-C6 as a Thread router (`~/src/xiao-thread-router`,
      local repo), on a wall charger on the 1st floor directly below the
      office. It joined by MeshCoP next to the ZBT-2 (at the office desk it
      heard no reply to its discovery scan), then was moved and resumed from
      NVS unattended. Router to BR: LQ 3/3. Node 25 to router: −74 to −80 dBm,
      2.7–3.5% frame errors (`meshdiag childtable`). Evidence: 30-minute soak
      17:52–18:22, node 25 the router's child throughout (connection time
      never reset, BR child table empty); `kubectl logs deploy/matter-server
      --since=30m | grep "@1:19"` at 18:23 returned nothing, so no `timed out`
      and no `peer-unresponsive`. Control: the same grep over 35 min returns
      the 17:51 resubscribe.
      Placement matters. At the first 1st-floor spot (router hears the BR at
      −71 dBm) node 25 reached the router at −90 to −103 dBm with up to 36%
      frame errors, and its subscription still timed out. After a parent change
      the controller stayed offline until the BR's stale child entry for node
      25 expired (240 s timeout); it resubscribed about 50 s after that
      (cause inferred). Open: the router went silent once, 7.5 min after its
      first placement (BR saw `NoAck`, and it did not reboot back onto the
      mesh). Not seen again in about 70 min of running since.
- [x] Device identity correct on the controller — **2026-08-24**: reads
      `homecadia` / `sensor-01` / `xiao-c6/driver-v2`. Note this only took
      effect after a re-commission; a reflash alone does not update it, because
      the controller caches BasicInformation from the interview
      ([commissioning.md](commissioning.md)).
- [x] Un-pairing recovers without physical access — **2026-08-24**, `7895168`.
      `remove_node` makes the device reboot itself and re-advertise (2884 FFF6
      reports on the controller's adapter, unattended). Before the fix the same
      operation left it silent, twice ([field-notes.md](field-notes.md) §12).
- [x] **Survives HA restart / OTBR restart — verified 2026-10-06** on node 28
      (`0.6.0-dev.116+6a37072`, PPK2 at 3700 mV): Home Assistant and then the
      OTBR pod restarted at ~21:45; direct reads every 30 s never went
      unanswered, `RebootCount` stayed 659 and `UpTime` ran on unbroken (246 s
      at 21:41:43, 741 s at 21:49:56). Limits: a gap under 30 s would be
      missed, and whether the Home Assistant restart also restarted the
      matter.js server was not checked. (It did not: the matter-server pod was
      9 days old on 2026-10-07, so that test restarted HA and OTBR only.) The unit's parent is the bare C6
      router, so an OTBR restart cuts its route out, not its parent.
- [ ] ICD: HA shows fresh readings at the configured report cadence. Partial
      **2026-08-23**: on the shipping profile (light sleep on) the device stays
      attached as a sleepy child and serves live reads over Thread 150s+ after
      boot. Cadence as seen from HA not yet observed over a longer window.
      **2026-09-15:** 30 min on the PPK2 (USB out, real light sleep), every
      temperature change reached the controller, no subscription loss. The
      device reports operating mode SIT (`0/70/8` = 0) with no registered
      check-in client (`0/70/3` empty), so LIT is not in effect with this
      controller.
