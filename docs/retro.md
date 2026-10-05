# Retrospective — sensor-01, unit 1

Written 2026-10-04, for whoever builds units 2 and 3 or rebuilds unit 1. It
covers 2026-08-08 (first flash) to 2026-10-04 (paired-current analysis): what
went wrong in firmware and hardware bring-up, why, what it cost, and what to do
differently. It does not repeat the procedures. [assembly.md](assembly.md) is
the build procedure, [field-notes.md](field-notes.md) is the incident record
(cited as §n), and [battery-runbook.md](battery-runbook.md) covers the cell.
This file is the index that says which of those mattered, and in what order.

What eight weeks produced: one working unit (XIAO #4, Matter node 28 since
2026-10-02), on a breadboard with a Nordic Power Profiler Kit II (PPK2) as its
supply. What they consumed: XIAO #1 (dead), XIAO #2 and #3 (underside pads
lost), two of three ePaper panels, driver boards #1 and #2 (retired), and one
cell over-discharged twice ([bringup.md](bringup.md), Display and Encoder &
LED; [battery-runbook.md](battery-runbook.md), "Seen on this bench").

## 1. Summary: the lessons that would have saved the most time

1. **Suspect the joint and the instrument before the firmware.** Every
   multi-day hunt in this build ended at a connection, a contaminant or the
   meter: a reversed or short-seated ribbon, bare driver-board holes, plumbing
   flux in socket joints, a wire on the wrong underside pad, a meter lead loose
   in its jack, a battery fed through breadboard springs. The firmware was
   blamed first in most of them. Check the meter against a known 5 V, then
   prove each joint by measurement, before reading any code (§15, §17, §25,
   §27; [assembly.md](assembly.md) rule 1).
2. **A check that cannot fail is a bug.** `busy_wait()` reported success on a
   panel that heard nothing (two days), `attribute::update()` returned `ESP_OK`
   while the controller read null, the first `check-profiles.sh` grepped a file
   that did not exist, Renovate was "running" with every slot blocked, and a
   capture script recorded nothing three times from a dead handle (§1, §9, §12,
   §27; commit `67eb3a2`). For every new check, ask what a disconnected part
   returns; if it is the same as success, fix that first.
3. **Test sleep only on the shipping image, on battery or the PPK2, with USB
   out.** `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y` keeps the ESP32-C6 out of
   light sleep while a USB host is attached (§20). The light-sleep hang and
   the dead dial were invisible on USB (the dial for three weeks, 08-26 to
   09-15), and a deep-sleep design that passed on USB stuck at ~20 mA on
   battery (§27).
4. **One reading is not a measurement.** A single-point divider check passed
   for ten days on the wrong conclusion (§19); a one-minute PPK2 window swings
   ±100 µA (§21); a capture within half an hour of boot is not settled (§22);
   one short capture misattributed 18 mC to the screen (section 3.3, M7).
   Sweep two points, select whole report cycles, save the capture and classify
   events before attributing them.
5. **Solder with electronics flux and thin wire, test before glue, and keep
   breadboards out of the battery path.** AIM Nitro plumbing flux, 22 AWG solid
   wire on underside pads and a breadboard supply path cost days, the pads of
   two XIAOs and more than 6,500 restarts between them (§17, §27;
   [bom.md](bom.md); [battery-runbook.md](battery-runbook.md) B3).
6. **Check that the controller supports a feature before designing around
   it.** Long Idle Time (LIT) Intermittently Connected Device (ICD) mode was
   configured before hardware (commit `bd59374`) and never took effect, because
   the Home Assistant Matter server registers no check-in client. It still
   cost an estimated ~67 µA of a ~307 µA average (section 3.3, M8).

## 2. Timeline

| Dates (2026) | Phase | What happened | Ref |
|---|---|---|---|
| 08-08 | Scaffold, first flash | Milestones 1–4 written before hardware; RF switch and console fixed from the schematic; USB console kept alive under light sleep | `35b3f1a`…`13c3ecb`, `da3c703` |
| 08-11 → 13 | Paper work | Bill of materials (BOM), case revs 1–4, custom PCB evaluated and rejected for three units | `1116aba` |
| 08-17 | Parts arrive | Driver board ships with bare holes; ICD threshold boot loop found at 11,374 reboots | `72298a0`, `f560723` |
| 08-18 → 22 | Display | Reversed and short-seated FPC (flexible printed circuit) ribbon, two panels dead, `busy_wait()` fixed; panel draws 08-22 | `1163829`, bringup.md post-mortem |
| 08-23 → 24 | Matter | MeasuredValue reads null; stale matter-server tag; Bluetooth Low Energy (BLE) gone after commissioning; milestone 2 | §9, §12, §13; `3a0f706`, `7895168` |
| 08-25 → 26 | Encoder, LED, harness | Misseated FPC on MOSI, SCL short in the breadboard, phantom detents; boot-time harness scan | §15, §16; `49a1929`, `b7d808d` |
| 08-31 | Flux post-mortem | AIM Nitro residue found; XIAO #1 killed by source-meter injection; board #3 washed clean | §17, §18; `aafe9fd` |
| 09-02 → 13 | Battery path | MTDI pad lifts on XIAO #2, divider to MTMS; divider "verified" by one point; packs read 0 V; two-point sweep; Thread router below the office | §19; `eac0631`, `dd4e6a6`, `42fc77b` |
| 09-15 → 17 | First real light sleep | Hang, analog-to-digital converter (ADC) holding the modem domain, I2C/SPI power-down flags, ADC sleep lock; floor 376 → 52 µA; soaks 288 → 238 µA | §20–§22; `e0760b0`, `8c0cea3` |
| 09-21 → 22 | Infrastructure, calibration | Border router lost its network on a node reboot; ADC calibrated on the shipping image | §23, §24; `78e3571` |
| 09-23 → 28 | Dial on battery, XIAOs #3 and #4 | Encoder A to MTCK; XIAO #2 loses its last pads; loose meter lead and encoder wire on the 3V3 pad (XIAO #3); XIAO #4 built with the staged procedure; 28 mA unpaired; 416 restarts on a breadboard battery path | §25, §26; `07457a1`, `611de00` |
| 09-28 → 29 | First real cell | 21 h on an EEMB pack, no restart | `256ba56` |
| 09-29 → 10-03 | Unpaired deep sleep | Four designs, a 25 h hung boot, ~6,100 brown-out restarts; restart-then-sleep at 19.7 µA | §27; `068dfc4` |
| 10-03 → 04 | Paired current | 15.5 h without a restart on a direct PPK2 lead; overnight current lost to PC hibernation; 307 µA measured; LIT turned off in the working tree | section 3.3, M8 |

## 3. Problems

Each entry: symptom, what we first thought, the actual cause, how it was found,
what it cost, the rule for next time, and where the full record is. Costs are
as recorded in the sources; where none was recorded the entry says so.

### 3.1 Hardware and assembly

**H1. Driver board ships with bare holes.**
- Symptom: BUSY floating, the SHT40 appearing and disappearing between resets,
  two USB brown-outs.
- First thought: dead panel, flaky sensor.
- Cause: the Seeed ePaper Driver Board V2 arrives with unpopulated
  through-holes; jumpers pushed into plated holes contact by luck.
- Found by: inspection on 2026-08-17.
- Cost: most of a bench session; female headers were missing from the BOM.
- Rule: headers in, washed and scanned before any bring-up.
- Ref: [assembly.md](assembly.md) "The driver board ships with bare holes";
  `72298a0`, `cfe7795`.

**H2. FPC ribbon reversed, then short-seated.**
- Symptom: nothing drawn, while the firmware reported every refresh as done.
- First thought: probed as a pin fault for two days.
- Cause: reversed, the ribbon mirrors the pin order and puts panel supply
  rails on RST and BUSY; seated short of the stiffener, it latches and contacts
  intermittently. The XIAO was also seated 180° out on board #2.
- Found by: insertion force (the correct way slides in), once `busy_wait()`
  was made to require BUSY to rise after `MASTER_ACTIVATE` (`1163829`); before
  that the log showed every refresh as successful (§1).
- Cost: two days (2026-08-18 → 22) and two of three panels. Field-notes §4
  says both died from reversal; [assembly.md](assembly.md) and
  [bringup.md](bringup.md) say one failed after a successful refresh, with
  electrostatic discharge (ESD) suspected and unproven. Board #2's connector was damaged by forcing.
- Rule: insertion force decides orientation, contacts face up on this board,
  stiffener inside the housing, XIAO USB-C away from the FPC, power off for
  every insert, reseat once and inspect rather than reseating blind.
- Ref: §4, §15; [assembly.md](assembly.md) FPC section; `f196e68`.

**H3. Misseated ribbon put a supply rail on MOSI (2026-08-25).**
- Symptom: `BUSY never rose`, stale QR on the glass, MOSI stuck at 3.1 V.
- First thought: dead panel, then a firmware fault; the stale QR read as an
  unpaired device.
- Cause: the ribbon skewed; a panel rail reached the MOSI net only when
  powered, so an unpowered continuity check read open.
- Found by: substitution, one reset per step (XIAO alone, + board, + panel).
- Cost: half a bench session.
- Rule: a short that only exists under power needs a powered measurement; an
  ePaper image is not a liveness signal.
- Ref: §15; `3b418a8`.

**H4. Plumbing flux in socket joints.**
- Symptom: 4–8 kΩ between adjacent socket pins on boards #1 and #2, drifting
  under bias; D7 at 0 V once SPI was up; an ohmmeter read open on every range.
- First thought: board defects, a dead D7 (routed around by moving the
  encoder), then a common fault in the board design.
- Cause: AIM Nitro is an ASTM B-813 plumbing flux, ionic by design; the
  residue conducts electrochemically at 3.3 V and not at a meter's ~0.3 V test
  voltage. Isopropyl alcohol (IPA) alone spread it. D7 followed SCK because
  they are neighbours.
- Found by: `CONFIG_HOMECADIA_BENCH_PIN_HOLD` plus a digital multimeter (DMM)
  on DC volts reading the pull-up divider; then reading the flux's own data
  sheet.
- Cost: days; swapping boards was not an independent trial because both had
  the same flux. XIAO #1 died during the hunt (H5).
- Rule: rosin-core wire or a J-STD-004 no-clean flux only (Kester 951 named in
  [assembly.md](assembly.md) rule 6); two-stage wash; one bench boot that must
  print `coupling scan: no pin follows any other`.
- Ref: §16 addendum (D7), §17; `c401b38`, `aafe9fd`.

**H5. Source-meter injection killed XIAO #1.**
- Symptom: 0.68 A bursts on the PPK2, smoke near D0.
- Cause: the PPK2 in Source Meter mode was clipped to D9 while the boot scan
  drove D9 low: a stiff 3.3 V source against an output transistor.
- Cost: one XIAO.
- Rule: measure voltage with the firmware holding the pins; if current must be
  injected, ≥1 kΩ in series and only into a pin provably held as an input.
- Ref: §18.

**H6. EC11 blades and dangling wires.**
- Symptom: one encoder line dead, which one changing between runs; 411
  phantom detents in 25 s with the knob untouched.
- Cause: the encoder's ~0.6 mm blades seat in neither breadboard springs nor
  Dupont sockets; jumpers pulled at the encoder end and left in D7/D9 acted as
  antennas on interrupt pins.
- Cost: several runs on 2026-08-25/26; each phantom detent was a panel
  refresh (~4,900/h against a 1M-cycle panel).
- Rule: solder the encoder leads; "disconnected" means both ends out.
- Ref: §16; `b7d808d`.

**H7. SCL shorted through the breadboard.**
- Symptom: SHT40 gone; the I2C line probe read identical values whatever was
  connected.
- First thought: the panel or the FPC.
- Cause: a displaced wire in a breadboard row under the driver board. The
  probe was also measuring the I2C peripheral, which already owned the pins
  (T6).
- Found by: the timeline (it failed right after encoder wires were pulled on
  the breadboard) and lifting the board out.
- Rule: read the timeline before touching anything; lift the board out first.
- Ref: §16.

**H8. Encoder wire on the 3V3 pad beside MTCK (XIAO #3).**
- Symptom: board drops off USB at the first detent; encoder A never changes.
- First thought: wire tugging, MTCK bridged to BAT+, flux on D6 (each written
  up as a conclusion before it was tested).
- Cause: the underside labels sit beside the pads and 3V3 is next to MTCK;
  every detent shorted the rail to ground.
- Found by: the MTCK wire beeped to the 3V3 header pin, once the meter worked
  (T1).
- Cost: XIAO #3, which lost its pads in handling after the hot glue was
  removed for rework; part of four days, 2026-09-24 → 28.
- Rule: identify pads by exclusion, unpowered, before glue: each signal wire
  against 3V3, 5V and GND, and against GND with B and R held.
- Ref: §25; [pinmap.md](pinmap.md) schematic facts;
  [assembly.md](assembly.md) stage 2.

**H9. Underside pads lifted.**
- Symptom: pads came away with the wire.
- Cause: 22 AWG solid hookup wire (1.6 mm over the insulation) levers a ~1 mm
  pad when it moves; no heat involved. Glue removal took XIAO #3's pads.
- Cost: XIAO #2 lost MTDI (2026-09-02), then MTDO and MTCK (2026-09-23);
  XIAO #3 lost pads after rework. Each lift also forced a pin move: the divider
  to MTMS (`eac0631`), the push switch to D9 (`7d25512`).
- Rule: four underside joints only (BAT+, BAT−, MTMS, MTCK), 30 AWG silicone
  or wire-wrap wire, test then glue, never remove glue.
- Ref: [assembly.md](assembly.md) rules 2–5; [bom.md](bom.md) "Pad wire, 30
  AWG" (not yet ordered).

**H10. The battery path ran through breadboard contacts.**
- Symptom: a healthy cell (3.97 V unloaded), the unit restarting at every
  boot, an LED flickering faintly; `RebootCount` 4 → 88 → 504 on 2026-09-28,
  then 30 → 6131 in 54 minutes on 2026-10-02, a steady 67 mA on the PPK2.
- First thought: on 2026-10-02, "awake", then a firmware fault in the new
  deep-sleep code. Two reproductions on USB paired cleanly.
- Cause: brown-out; confirmed for the 2026-10-02 episode, best-supported for
  the 2026-09-28 one. Radio bursts of 250–330 mA through breadboard springs sag
  the supply under the brown-out detector (`CONFIG_ESP_BROWNOUT_DET_LVL=7` in
  the generated sdkconfig; the voltage it corresponds to is unverified). The
  PPK2 measures at its own terminals and cannot see the sag.
- Found by: `bootReason: 2` (brown-out) read from the controller on
  2026-10-03. With the PPK2 leads wired straight to the BAT wires: no restart
  in 15.5 h.
- Cost: about two days of inference; more than 6,500 restarts, each one a
  flash write for the reboot counter.
- Rule: solder the supply leads; read `bootReason` and `RebootCount` before
  theorising about a restart loop.
- Ref: [battery-runbook.md](battery-runbook.md) B3; §27.

### 3.2 Power and sleep

**S1. Light sleep hung the device at a sensor poll.**
- Symptom: on the first real light-sleep run (PPK2, USB out, 2026-09-15) the
  device stopped waking ~8 min after boot, 10–33 µA, dropped by its parent.
  Forty minutes on USB had shown nothing.
- First thought: `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP` itself (it
  was turned off as a workaround).
- Cause: a light sleep landing between `adc_oneshot_new_unit()` and
  `adc_oneshot_del_unit()`, which change the sleep power-domain configuration;
  a sleep that started mid-change never woke. The "4th poll" pattern was
  chance.
- Found by: bisecting the poll with diagnostic-image variants: no poll, poll
  without the ADC, full poll with the ADC under a lock.
- Cost: two hangs; found, worked around and fixed within 2026-09-15.
- Rule: hold `ESP_PM_NO_LIGHT_SLEEP` across any sequence that reconfigures
  sleep domains (`battery.cpp:60`); test sleep with USB out.
- Ref: §20, §21; `03dd4bb`, `e0760b0`.

**S2. The battery ADC kept the modem domain powered in every sleep.**
- Symptom: quiet floor ~376 µA against Espressif's ~55 µA reference; the
  trace looked like a wake storm.
- First thought: wakes, the USB console, the panel, GPIO3 held low, BLE.
- Cause: the oneshot ADC unit was created once and never deleted; on the C6
  the ADC front end is in the modem power domain
  (`ADC_LL_ADC_FE_ON_MODEM_DOMAIN`, `hal/esp32c6/include/hal/adc_ll.h:41`) and
  only deleting the unit releases it. The pulse train was the SGM6029 buck in
  power-save mode, not wakes.
- Found by: `CONFIG_HOMECADIA_SLEEP_DIAG`, which counts sleeps and records the
  power-down flags, read from the glass.
- Cost: ~95 µA (376 → 282 µA); same day.
- Rule: instrument the sleep (flags, retention bitmaps) before guessing from
  the trace shape.
- Ref: §21; `ac3f5ad`, `e691024`.

**S3. TOP domain stayed up until the buses asked to power down.**
- Cause: IDF's I2C and SPI drivers build sleep-retention entries only when the
  bus sets `flags.allow_pd` / `SPICOMMON_BUSFLAG_SLP_ALLOW_PD`, and TOP cannot
  power down until every inited module is created.
- Found by: the retention bitmap on the diagnostic screen (I2C0 and GPSPI2
  inited, not created).
- Cost: 279 µA → 56 µA once fixed.
- Rule: every bus in a sleepy build sets its power-down flag
  (`sht40.c:72`, `ssd1680.c:302`).
- Ref: §21; `4141940`.

**S4. The dial did nothing on battery.**
- Symptom: found 2026-09-15 on the PPK2: a detent did not wake the chip. Every
  dial test before then ran on USB, where the chip never sleeps.
- Cause: the encoder used edge interrupts, which cannot wake light sleep
  (`gpio_wakeup_enable` accepts level types only); with peripheral power-down
  only LP (low-power) GPIOs 0–7 can wake (ESP-IDF sleep_modes docs, per commit
  `07457a1`); and `CONFIG_PM_SLP_DISABLE_GPIO=y` (generated sdkconfig)
  disables pin input in sleep. The header's LP pins D0–D2 belong to the
  display.
- Fix: encoder A to the MTCK pad (GPIO6, `app_config.h:31`), armed for the
  level it is not at, plus a 2 s no-sleep window (`ENC_AWAKE_MS`) so encoder B
  and the push switch on HP (high-power domain) pins still decode.
- Cost: eight days from finding to fix, five to verification; XIAO #2's last
  two pads (H9).
- Rule: decide wake pins against the LP GPIO list before wiring; verify input
  wake on battery.
- Ref: §21; bringup.md "Dial works on battery"; `07457a1`, `7d25512`.

**S5. An unpaired unit drew 28 mA.**
- Symptom: 28.1 mA average before commissioning (2026-09-28); a cell would
  last about three days.
- First thought: the OpenThread `ot_sleep` power-management lock, held until a
  Thread network exists (written up 2026-09-29).
- Cause: not known. The `ot_sleep` explanation was retracted on 2026-10-02:
  that lock is `ESP_PM_APB_FREQ_MAX`, which does not block light sleep. Later
  measurement puts the cost in the commissioning window's advertising (29.2 mA
  with the window open, a few mA after it closes).
- Cost: one over-discharge risk per unpaired unit; the deep-sleep work in S6.
- Rule: a mechanism read from SDK source is a hypothesis until the lock or
  register is read on hardware (the sleep diagnostic image can list locks).
- Ref: §26; [bringup.md](bringup.md) still carries the retracted explanation.

**S6. Deep sleep from a running Matter device: four designs.**
- Design 1, `esp_deep_sleep_start()` from the running system: on the
  shipping config it woke itself within seconds, because
  `CONFIG_PM_SLP_DISABLE_GPIO` floats every pin in sleep and the wake pin
  drifted to its wake level; and the boot after a wake **hung for 25 h** at
  full power in `main`, inside GPIO interrupt setup (task watchdog; the woken
  boot inherited the wake pin's pad hold and LP wake setting).
- Design 2, the same with light sleep off (debug image): slept and woke on the
  dial, on USB only.
- Design 3, wake pin out of sleep isolation and a clean restart after wake:
  worked on USB, **stuck at ~20 mA on battery** and deaf to the dial. Best
  reading, not proven: deep-sleep entry stalls on a radio that light sleep has
  already powered down; IDF's sleep docs require radios stopped first.
- Design 4, restart first and sleep at the top of the next boot before
  Bluetooth, Thread, the radio PHY (physical layer) or power management
  exist: 19.7 µA, dial wake 5 of 5 on battery and on USB
  (`app_main.cpp:140-187`, `:346`).
- Also: the SDK reopens the window after a failed pairing with no timeout, so
  a 16-minute uptime cap was added.
- Cost: four days (2026-09-29 → 10-02); misreadings on the way included "port
  vanished = deep sleep" (it was light sleep) and "the turn did not wake it"
  (the board was hung).
- Rule: enter deep sleep from a clean boot with nothing running; prove a
  sleep design on battery, not USB.
- Ref: §27; `068dfc4`.

**S7. Light sleep powers down the USB console.**
- Symptom: the port enumerates and every open fails ("A device attached to the
  system is not functioning"); reads like broken hardware.
- Cause: automatic light sleep powers down the USB-Serial-JTAG (USJ).
- Cost: hours blaming pyserial (§2); later, a power-down-on image wedged the
  port so that only download mode (B held while plugging in) could flash it
  (§21).
- Rule: two profiles. `sdkconfig.bench` has no power management and the
  console; the shipping profile is validated with USB out.
  `tools/check-profiles.sh` asserts the difference positively on both sides.
- Ref: §2, §20; `13c3ecb`, `67eb3a2`.

**S8. ICD active-mode threshold below the LIT minimum boot-looped unit 1.**
- Symptom: `abort()` ~1.3 s after `app_main()`; 11,374 reboots on the
  counter in non-volatile storage (NVS) by the time it was caught.
- Cause: `CONFIG_ICD_ACTIVE_MODE_THRESHOLD_MS=1000` with LIT enabled trips a
  `VerifyOrDie` in connectedhomeip `ICDManager.cpp:82`.
- Cost: undetected from 2026-08-08 to 08-17 because the 08-08 boot check was a
  seconds-long look at the banner.
- Rule: watch the console ≥30 s for a boot verification; read the reboot
  counter.
- Ref: [bringup.md](bringup.md) Flash & console; `f560723`.

### 3.3 Matter and Thread

**M1. `attribute::update()` silently dropped MeasuredValue.**
- Symptom: correct readings in the log, null over Matter for temperature and
  humidity, no Home Assistant entity; battery fine.
- First thought: controller cache, bounds, creation path.
- Cause: esp-matter v1.6 serves these clusters from a registered code-driven
  cluster object (a server cluster interface) whose own value is never seeded
  or synced; `update()` writes a store that reads never reach. PowerSource
  uses an AttributeAccessInterface and falls through to the store. Upstream
  espressif/esp-matter#1798 and #1738; fixes on `main` only.
- Found by: writing a value that could not already be present and reading it
  back; then following the read path through the SDK.
- Cost: one day (2026-08-23). Espressif's own sensors example is broken the
  same way.
- Rule: for any cluster with a registered server cluster object, set the value
  on the object; verify over the wire, not from a return code or the
  `Attribute … is …` log line.
- Ref: §9; `3a0f706`.

**M2. A container tag copied from a PyPI version.**
- Symptom: BLE commissioning failed at the BLE transport protocol (BTP)
  handshake.
- Cause: matter-server pinned at `0.7.1`, a PyPI package version on the same
  line of a README, three months stale; the bug was fixed upstream twelve days
  after that tag. Renovate had the update queued but six errored branches held
  every PR slot.
- Cost: two evenings, an upstream bug report, a half-redundant PR.
- Rule: ask "is this current?" when pinning; check the dependency bot opens
  PRs, not just that it runs.
- Ref: §12; [commissioning.md](commissioning.md).

**M3. BLE memory is reclaimed after commissioning.**
- Symptom: after the last fabric was removed, the device advertised nothing.
- Cause: `CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=y` hands the BLE controller's
  RAM to the heap (`BLEManagerImpl.cpp:1004`); the first fix (`cb326d6`) was
  committed with confident reasoning and failed on hardware.
- Rule: reboot on last-fabric removal; a fix not tested on hardware is a
  hypothesis.
- Ref: §13; `7895168`.

**M4. Identity is cached at the commissioning interview.** Reflashing
VendorName/ProductName does not change what the controller shows; only a
re-pair does. Get identity right before pairing (§14; `27a3ea1`).

**M5. Thread coverage and border-router state.**
- One border router in the basement did not hold a second-floor sensor
  (−83 to −90 dBm, two drops in six minutes); the OpenThread Border Router
  (OTBR) transmits at 5 dBm. A bare XIAO C6 Thread router below the office
  fixed it (§7; [bringup.md](bringup.md) Radio / Matter; `42fc77b`).
- An OTBR at `state: disabled` makes every device look out of range: hours
  went into a shielding theory (§6). On 2026-09-21 the OTBR came back on
  factory defaults after a node reboot because its persistent volume was
  mounted at the wrong path (§24). The node's own credentials survived.
- Rule: `ot-ctl state` before any theory about the device.

**M6. Each interview costs the sensor ~150 mC.** About ten minutes of normal
running per `interview_node`; five interviews were ~10 % of one soak's charge.
Use `tools/matter-node.py get` or the PPK2 trace for liveness (§22;
[build.md](build.md)).

**M7. Paired current measured at 307 µA, and the first explanation was wrong.**
- Symptom: 2026-10-04, node 28 on the final firmware: 307.76 µA over a 16¾-min
  selection, against 208 µA settled and 238 µA full-run on 2026-09-17
  (node 25, XIAO #2).
- First thought (in order): the phone's second fabric (removed: 294 µA, no
  change); the weaker link (7.6 % frame errors against 2.7–3.5 %); then, from
  a saved 387 s capture split by `tools/ppk2-events.py`, "the extra is the
  screen" at 15–18 mC per refresh, with a projection of 130–150 µA if refreshes
  and reports were cut.
- Cause, after re-reading the same capture in 20 ms steps: only one event had
  the panel's shape, about 6.3 mC (0.5 s at ~13 mA while `busy_wait()` polls).
  The 18 mC event was the radio receiver held on for 240 ms, seen once, cause
  unknown. The projection was withdrawn the same hour. Breakdown as
  re-analysed: floor 76, parent polls 35, the 2-min report 69, 5 s fast-poll
  tail 55, the 240 ms receive window 47, screen 16, 60 s wake 12 (µA).
- Cost: one analysis cycle and a withdrawn recommendation; no hardware cost.
- Rule: classify each event by shape at fine resolution before naming it, and
  say how many instances a figure rests on. One instance is not a rate.
- Ref: this section; `tools/ppk2-events.py` (untracked as of 2026-10-04). Not
  yet in [power-budget.md](power-budget.md).

**M8. LIT mode bought nothing and cost ~67 µA.**
- Cause: LIT needs a controller that registers as a check-in client; the Home
  Assistant Matter server registers none (`0/70/3` empty, operating mode
  Short Idle Time (SIT), [bringup.md](bringup.md) Radio / Matter). The device
  ran SIT anyway, while paying LIT's costs: a 5000 ms minimum active-mode
  threshold (`ICDManager.cpp:80-82`), so every report was followed by 5 s of
  500 ms fast polls (~55 µA), and a 60 s short-idle wake per cycle (~12 µA,
  `ICDConfigurationData.cpp:112-121`). Line references are as cited in the
  working-tree `sdkconfig.defaults`, not re-read for this file.
- Status on 2026-10-04: the working tree (uncommitted) turns LIT off, sets the
  active-mode threshold to 300 ms, the slow poll to 15 s (the SIT maximum) and
  the idle interval to 600 s. Measured 2026-10-05 on node 28 over 2 h:
  **307 → 185 µA settled**, then 125 µA once the push-switch pad regression was fixed (field-notes.md §28) (225 µA including a one-off 2 s receive window),
  [power-budget.md](power-budget.md). Home Assistant kept the node without a
  re-pair. The 600 s report
  interval does not depend on the controller (this deployment runs
  matterjs-server, [commissioning.md](commissioning.md)): the device offers
  its idle interval and the ceiling is at least 60 min
  (`ReadHandler.cpp:42-52`, `:777-802`); only a controller min-interval floor
  above 600 s would change it.
- Earlier, related: raising the slow poll 5 s → 30 s on 2026-09-17 gave an
  effective 15 s (SIT clamp), saving 3.9 mC per cycle, about half the
  prediction; the active period around each report grew, cause not
  established (§22).
- Rule: read the controller's ICD client table before choosing LIT; a mode the
  controller does not use is pure cost.

### 3.4 Measurement and tooling

**T1. Measurement artifacts acted on as faults.** Each of these triggered an
intervention:

| Looked like | Was | Ref |
|---|---|---|
| 0 V on D6, D5, 3V3 and 5V; no beep on good wires | meter lead loose in its jack (the 5V pin read 0 V while the board was enumerated) | §25 |
| two dead packs, 0 V both polarities | probe on the JST housing rim, not the recessed contact | §19; runbook B1 |
| dead pack after charging | protection board latched after over-discharge by the ~41 mA bench profile; it reads 0 V for hours while taking charge | §19; runbook A1 |
| PPK2 reads 0.00 A charging a cell | leads reversed in Ampere Meter mode; the PPK2 cannot read negative current | §19 |
| source not reaching BAT+ (0.6 V, decaying) | the wire had been unplugged to measure it | §19 |
| "divider top on the 3V3 rail", 3.36 V regardless of setting | most likely the PPK2 still at 3400 mV because a typed value had not applied (set it with the slider); the two-point sweep showed the divider on BAT+ throughout | [bringup.md](bringup.md) 2026-09-12 rows; §19 |
| "dead node" | a broken hookup wire; the PPK2 read 1.84 µA, exactly 3.7 V over the 2 MΩ divider | §23 |
| overnight paired current | the PC running the PPK2 app hibernated; the device's own counters (RebootCount, controller log) were still valid | section 3.3, M7 |
| a 361 µA "average" | a 60 s selection that happened to hold a report burst | 2026-10-04 bench session; not yet in the docs |

Rules that came out of these: check the meter against a known source first;
measure in-circuit (a 10 MΩ meter disturbs nothing); one physical change, then
measure only; two matching readings before acting on a surprise; disable PC
sleep and hibernation before an overnight PPK2 run.

**T2. The battery divider "verified" by one reading (2026-09-02).**
- Cause: with no cell, BAT+ and the 3V3 rail both sit near 3.3 V, so a
  single reading cannot tell which node the divider is on. The 2026-09-02 row
  (meter 1570 mV against firmware 1551 mV) agreed by coincidence.
- It has been retold as "the divider was on the 3V3 rail". The record does not
  show that: the rail was proposed twice during the 2026-09-09/12 sessions and
  retracted both times, and the 2026-09-12 sweep found the divider on BAT+ and
  exact. [assembly.md](assembly.md)'s symptom table still lists
  "divider top on the 3V3 rail" as a found cause.
- Cost: ten days of "the battery always reads the same" (§19).
- Rule: a "verified against a meter" row needs two source voltages.

**T3. Calibrating against a loaded node.** A 10 MΩ meter on the divider
midpoint (500 kΩ source) reads ~5 % low; an offset of 0 derived from it came
out 64–144 mV low. Calibrate end to end from BAT+ (low impedance) to the
reported number, on the shipping image: the bench image's +340 mV offset read
~280 mV high on the shipping image (§19, §20, §23; `78e3571`).

**T4. Flashing and console from WSL2.**
- usbipd loses the C6 on every re-enumeration; flashing from Windows-native
  esptool is the reliable path ([build.md](build.md) "Flashing from WSL2").
- pyserial asserts DTR/RTS on `open()`, which resets the C6 and can wedge its
  USB into "Device Descriptor Request Failed" until a replug. Map a tty to its
  USB vendor ID before opening it: another radio on the same host appears as
  `ttyACM*` too (§3).
- A capture script that holds a handle across a port drop records nothing;
  three captures came back empty before it was rewritten to reopen (§27).
- Windows Python writes stdout as cp1252; a capture died on a `≥` in a log
  line (seen on the companion Thread-router build). Reconfigure stdout to
  UTF-8 before printing device output.

**T5. A stale build directory compiled a feature out.** A shipping build kept
an sdkconfig from before the new Kconfig options existed and dropped the
unpaired deep-sleep feature with no error. Use `idf.py reconfigure` after
touching `Kconfig.projbuild`; `tools/check-profiles.sh` now asserts the option
is on in shipping (§27; [build.md](build.md); `check-profiles.sh:106-121`).

**T6. A probe that runs after a driver owns the pin measures the driver.**
The I2C line probe after `i2c_new_master_bus()` returned identical readings
across every wiring change; a `gpio_config()` probe left in after `ui_init()`
disabled the encoder interrupt for two rounds of "no events"; a line probe on
a live I2C bus is a START condition and wedges the slave. The harness scan now
runs first in `app_main()`, behind a bench-only Kconfig (§16; `49a1929`).

**T7. Settled numbers need settled windows.** Nothing in the first half hour
after boot is settled (the controller re-subscribes; a window ten minutes
after boot read 309 µA against 208 µA at +12 h); only a selection spanning whole report
cycles is an average; one-minute averages swing ±100 µA (§21, §22). Save
`.ppk2` sessions: they are a zip with `session.raw` (float32 µA plus uint16
digital channels per sample) and a start time that lines up with any log to
the millisecond (§20).

### 3.5 Process

- **One bench step per message, ending in an explicit "say go".** The builder
  asked for it during the 2026-09-09 divider hunt, after multi-step probe
  sequences, with the observation that each new intervention created new
  issues.
- **Prefer checks that do not depend on reading small text or a precise button
  sequence.** The B-then-R download-mode sequence and small debug text on the
  panel both failed repeatedly. PPK2 traces, logs, LEDs and whole-screen
  changes worked; an awake board can be flashed with esptool's automatic
  reset in a retry loop.
- **Conclusions were written up before they were tested** (§25 lists four;
  §26's `ot_sleep` explanation was committed in `21f9440` and retracted). Keep
  a claim as "best-supported, not proven" until a measurement can only come
  out one way.
- **After a pin move, change `app_config.h`, [pinmap.md](pinmap.md), the
  diagrams and the breadboard in one sitting.** "Wired as in the diagram" was
  wrong for an hour (§16 addendum); the README pinout drawing lagged a month
  (`34e15f0`).
- **A copied reference config is a set of claims to check.** The milestone-1
  scaffold copied esp-matter's `icd_app` defaults; that brought peripheral
  power-down (which hung this build until S1 was found) and a LIT design no
  controller here uses (§20, M8).
- **Bench work and infrastructure share a failure surface.** The border
  router, the matter server and the PC running the PPK2 each produced a
  "device fault" (M5, T1). Check them first.

## 4. Checklist for the next build

Ordered. Where [assembly.md](assembly.md) has the procedure, this list only
says what to watch for.

**Before soldering**

1. Parts on hand. Only one panel (C) and one driver board (#3) are recorded as
   good ([bringup.md](bringup.md) component status, Encoder & LED). Units 2
   and 3 need two panels (Seeed SKU 104990853) and two driver boards, or a
   washed and re-scanned board #1; whether replacements were ordered is not
   recorded in [bom.md](bom.md). Board #1 also measured RST-to-GND at
   0.6–1.2 kΩ before its wash ([power-budget.md](power-budget.md)); measure it
   on any board before it goes in a unit.
2. 30 AWG silicone or wire-wrap wire for the four pad leads (not yet ordered,
   [bom.md](bom.md)). Not the 22 AWG solid.
3. Electronics flux only; no AIM Nitro on the bench.
4. Meter check: probes together on beep, then the XIAO's 5V pin on USB
   ([assembly.md](assembly.md) rule 1). Repeat it whenever a run of zeros
   appears.
5. Cell polarity measured on bare metal, for each pack, before first
   connection ([assembly.md](assembly.md) battery section; the bringup row is
   still open for all three cells).

**Driver board and XIAO**

6. Driver board: decide which face the XIAO seats on, solder the 2×7 headers,
   two-stage wash, dry, one bench boot with `no pin follows any other`
   ([assembly.md](assembly.md) stage 5.1). Do this before anything else is
   attached to that board.
7. Bare XIAO, stage 0: enumerates as `303A:1001`, bench image, all six outputs
   `follows the driver`.
8. Stage 1–2: four underside wires, the seven unpowered exclusion tests,
   then glue. Do not remove glue afterwards.
9. Stage 3 on USB with the bench image: battery line reads the charger output,
   SHT40 at 0x44, three detents each way with `encoder raw` changing on A and
   B, three presses, and the COM port present throughout.
10. FPC by insertion force, contacts up, stiffener inside the housing, power
    off; XIAO USB-C away from the FPC. Read the harness lines before
    suspecting the panel.

**On the PPK2, shipping image**

11. Supply leads soldered to the BAT wires, not through a breadboard row
    (H10). The divider's top lead joins the same node.
12. Border router up (`ot-ctl state`), then pair. Identity strings are final
    before pairing (M4). One controller fabric.
13. Unpaired: ~28 mA while the window is open, then 19.7 µA deep sleep about
    30 s after it closes; a detent wakes it with a new window. Do not leave an
    unpaired unit on a cell inside the window.
14. Paired: flat baseline, a detent gives a ~2 s burst and a view change
    ([assembly.md](assembly.md) stage 4, which waits 3 min; any number that
    gets recorded needs 30 min of settling first, T7).
15. Battery reading: sweep two source voltages and check a third
    ([bringup.md](bringup.md) 2026-09-22 row). The scale constant was fitted on
    one XIAO and read 3682 mV at 3700 mV on a second
    ([pinmap.md](pinmap.md)); check it per unit rather than assume it.
16. Power acceptance: PC sleep and hibernation off; save a `.ppk2` of at
    least 30 min after the first half hour; split it with
    `tools/ppk2-events.py`; compare floor, polls and report cost against the
    figures in section 3.3. A first-half-hour or sub-cycle window does not
    count.
17. Liveness and restarts from the device's own counters (`RebootCount`,
    `bootReason`) and `tools/matter-node.py get`, not `interview_node`.
18. Close each unit's rows in [bringup.md](bringup.md) with the date and the
    evidence line.

**Firmware choice for units 2 and 3**

19. Do not flash the SIT / 600 s configuration onto new units until it has been
    measured on unit 1 and Home Assistant's behaviour after the change is
    known (M8). Until then the last measured configuration is commit
    `068dfc4`.

## 5. Open questions

| Question | What is known | Ref |
|---|---|---|
| Why does an unpaired unit draw 28 mA? | The `ot_sleep` lock does not block light sleep; the cost tracks the window's advertising. Worked around with deep sleep, not explained | §26 |
| Why is the floor 57 µA on the current build against 45 µA on 07457a1, same board? | The 73 µA regression is found and fixed (field-notes.md §28); ~10 µA remains, and the floor drifted 50 → 58 µA within one 50 min capture. Not investigated | [power-budget.md](power-budget.md) |
| What is the 240 ms receiver window? | One instance in a 387 s capture, ~18 mC; rate unknown | M7 |
| What is the 2 s receive window ~550 s after boot? | 139 mA flat for 2.0 s, once per boot in two captures; no attach attempt or parent change in the Thread counters; not OpenThread parent search (FTD-only in this build) | [power-budget.md](power-budget.md) |
| ~~Why do parent polls cost ~0.76 mC each?~~ | Answered: the push-switch pad (field-notes.md §28); 0.48 mC after the fix. The 4.4/min is 15 s polls plus exchanges around reports | [power-budget.md](power-budget.md) |
| Why did deep sleep from the running system stick at ~20 mA on battery only? | Best reading is entry stalling on a powered-down radio; not proven, design 4 avoids it | §27 |
| Why did the report's active period grow when the slow poll went 5 → 15 s? | ~8.8 → ~13.6 mC per cycle; cause not established | §22 |
| Why is the first battery report after boot ~80 mV low? | Seen at 3.39 and 3.68 V; cause not established | [bringup.md](bringup.md) 2026-09-22 row |
| What killed panel B? | Failed after a successful refresh; ESD suspected, unproven | [assembly.md](assembly.md) FPC section |
| Does the Thread router stall again? | Silent once, 7.5 min after first placement; not seen since | [bringup.md](bringup.md) Radio / Matter |
| What voltage is brown-out level 7? | Unverified | [battery-runbook.md](battery-runbook.md) |
| What are the EEMB pack's protection thresholds? | Specification not retrieved; unverified | [battery-runbook.md](battery-runbook.md) |
| Still-open bring-up rows | panel deep-sleep current, ghosting policy, over-the-air (OTA) update slot headroom (84 % full on 2026-08-23), Home Assistant and OTBR restart survival, the enclosure (rev 4 does not fit the cell or the stacked headers) | [bringup.md](bringup.md) |
