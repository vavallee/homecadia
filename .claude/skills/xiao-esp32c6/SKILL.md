---
name: xiao-esp32c6
description: Board facts, power behaviour, sleep traps and Matter-over-Thread practice for the Seeed XIAO ESP32-C6 on ESP-IDF v5.5 / esp-matter v1.6, learned building the homecadia battery sensor. Use when writing or debugging firmware for a XIAO ESP32-C6 (pins, light/deep sleep, battery sensing, brown-out, flashing from WSL/Windows, Matter ICD tuning, OTA), when measuring its current with a PPK2, or when designing hardware or an enclosure around it.
---

# Seeed XIAO ESP32-C6

Facts and rules specific to this board, each traced to the homecadia docs
(`docs/field-notes.md` §n, `docs/pinmap.md`, `docs/power-budget.md`). For
general bench doctrine (wiring first, one change per step, measuring pins
before drivers own them) use the homelab skill `iot-electronics`; this skill
does not repeat it. Cite the schematic, datasheet or SDK source for any new
pin or register claim, or mark it unverified.

## 1. The board (XIAO-ESP32-C6_v1.0 schematic)

- **Power path** (sheet 3/5): BAT → P-FET Q1 → **SGM6029 buck** (U1, L1
  0.47 µH, VSEL 249 kΩ = 3.3 V) → 3V3. Charger **SGM40567**, ~120 mA
  (IREF 200 kΩ), between VBUS and BAT only.
- **No battery divider on the board.** A0's "battery" snippets on the wiki
  are XIAO boilerplate. Add your own (homecadia: 1 MΩ / 1 MΩ, BAT+ → GPIO4).
- **Underside pads** GPIO4–7 (MTMS, MTDI, MTCK, MTDO), BOOT, EN, 3V3, GND.
  Labels sit *beside* the pads, and **3V3 is next to MTCK**: a wire meant for
  MTCK on 3V3 shorted the rail through the encoder (§25). Prove pad identity
  electrically (`docs/assembly.md` stages 0–4), never by eye.
- **RF switch is off at reset** (sheet 4/5): drive GPIO3 low to power it;
  GPIO14 low = onboard antenna. Not on the header.
- **Onboard LED GPIO15, active low, strapping pin**: leave it high-Z.
- Seeed's own note: avoid GPIO4, 5, 8, 9, 15 (straps). GPIO4/5 strap only
  the SDIO clock edge, so a divider on GPIO4 is harmless when SDIO is unused
  (`pinmap.md`, datasheet Table 3-4).
- **Native USB only** (USB Serial/JTAG): any reset re-enumerates the port.
- **LP GPIOs are 0–7.** Only these wake the chip from deep sleep; light-sleep
  GPIO wake on HP pins needs a level interrupt and care (§4 below).

## 2. Power: what to expect

| State (3.7 V on BAT, USB out) | Current | Source |
|---|---|---|
| Deep sleep, whole board incl. panel | 19.7–21.2 µA | power-budget.md |
| Paired Matter SED, SIT ICD, settled | ~108–125 µA average, 40–57 µA floor, ~470 µC per poll | §28, §29 |
| Unpaired, commissioning window open | ~28 mA, never sleeps | §26 |
| Bench build without PM | ~41 mA | power-budget.md |

- **The SGM6029 runs in power-save mode**: the "floor" is a train of buck
  bursts (~300/s), not wakes. Rate tracks the 3V3 load.
- **Below ~3.3 V in, the buck goes to 100 % duty** and passes BAT through;
  the C6 then runs under its 3.0 V minimum. Deep sleep at 2.9 V drew
  **298 µA** against 21 µA at 3.7 V (§29). Hardware; firmware cannot fix it.
- **A PPK2 in Source Meter mode below ~3.7 V is not a battery-life
  measurement**: near dropout a third of samples read exactly zero and the
  event splitter misclassifies the bursts (§29). Measure at 3.7 V.
- **USB keeps the C6 awake and backfeeds BAT** (~4.03–4.05 V with no cell).
  A battery reading with USB in is the charger, never the cell. **USB and a
  PPK2 must never feed BAT+ together.**

## 3. Battery sensing and the bottom of the cell

- **Create and delete the ADC oneshot unit per reading.** A live unit holds
  the modem power domain on through every light sleep (~95 µA; §21,
  `adc_oneshot.c`). Take the reading under an `ESP_PM_NO_LIGHT_SLEEP` lock:
  a sleep landing inside the unit's create/delete window hung the chip (§21).
- 500 kΩ source: the ADC input loads it ~1.9 %. Correct with a fitted gain
  (homecadia `VBAT_SCALE_X1000` 2038, ±11 mV from 3.7 to 3.1 V against a
  PPK2), not a stiffer divider. 20 ms settle, 64 samples.
- **Brown-out level 7 ≈ 2.51 V on the 3V3 rail** (IDF `Kconfig.power`
  estimate). Before the app sets it the hardware default (~2.7 V) applies, so
  **a running unit survives 2.9 V on BAT but boots fail there and loop about
  once a second** (§29). Decide "empty" from a battery reading taken before
  the radio starts, with hysteresis, and deep-sleep instead of booting on.
- A breadboard joint in the BAT path is enough to brown out on radio peaks
  (250–330 mA): solder the cell to the BAT wires (`battery-runbook.md` B3).
  A divider that shared that breadboard row loses its reference when the
  joint is remade.

## 4. Sleep traps (ESP-IDF v5.5, `CONFIG_PM_SLP_DISABLE_GPIO=y`,
`CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y`)

- **Every non-wake pin is isolated in light sleep.** An HP-GPIO input with a
  level interrupt reads low while isolated: the push switch on GPIO20 raised
  the floor 45 → 73 µA and fired up to 120 stray wakes a minute.
  `gpio_sleep_sel_dis()` on that pin fixes it (§28).
- **An output that must hold through light sleep needs the pad hold.** With
  peripheral power-down the GPIO domain goes down; `gpio_sleep_sel_dis()` is
  not enough. Release, set, re-latch: `gpio_hold_dis(); gpio_set_level();
  gpio_hold_en();` (`driver/gpio.h` hold notes; §29 LED pulse).
- **Check timed outputs with the chip actually light-sleeping.** A bench build
  without PM proves nothing about them (§29).
- **Enter deep sleep from early boot, not from a running Matter stack.**
  Draw the final screen, set an RTC_NOINIT flag, `esp_restart()`, and sleep
  from the top of `app_main()` before Bluetooth, Thread or the PHY exist.
  Entering from the running system stuck at ~20 mA on battery (§27).
- **Deep-sleep GPIO wake pins need `gpio_sleep_sel_dis()` and a pull-up**, or
  they float and wake the chip within seconds (§27). On a dial wake, release
  the pad hold and wake config, then restart cleanly.
- The USB console dies in light sleep (§2): read state from the panel, LEDs,
  PPK2 traces or the controller instead.

## 5. Matter over Thread (esp-matter v1.6)

- **Use SIT, not LIT, under Home Assistant**: no check-in client registers,
  so LIT only adds its 5 s active threshold and a 60 s short-idle wake.
  Working set: slow poll 15000 ms, active threshold 300 ms, idle interval
  600 s (`sdkconfig.defaults`; 307 → 185 µA).
- **`attribute::update()` silently does nothing for code-driven clusters**
  (temperature, humidity): set the value on the registered cluster object
  (§9).
- Optional attributes the legacy data model omits must be created after
  `node::create`: Thread diagnostics need all four features or none;
  GeneralDiagnostics **BootReason** needs `create_boot_reason()` or reads
  return nothing.
- An unpaired unit never light-sleeps (OpenThread's `ot_sleep` lock): close
  the window into deep sleep with a dial wake (§26, §27).
- Report battery falls with hysteresis, every point below the warning level,
  and rises of 5+ points at once (charging); otherwise a rise waits for the
  forced report.
- **OTA**: `CONFIG_ENABLE_OTA_REQUESTOR=y` plus `CONFIG_CHIP_OTA_IMAGE_BUILD=y`
  emits `build/<project>-ota.bin`. `SoftwareVersion` must increase or the
  controller declines silently; derive it from the git tag (homecadia
  `version.cmake`). Test vendor IDs (0xFFF1) are not in the public DCL, so
  matterjs-server needs `ENABLE_TEST_NET_DCL=true` **and**
  `OTA_PROVIDER_DIR` with the `.ota` files (`docs/build.md`).
- Infrastructure first: a silent node is as often the border router or the
  Matter server as the device (§6, §24). A bare C6 as a Thread router fixes
  coverage; ESP32-C6 transmits at 20 dBm, an OTBR may be at 5 dBm.

## 6. Flashing from WSL / Windows

- The chip enumerates as `303a:1001`. Flash with **Windows-native esptool**
  (`python.exe -m esptool`, v5 dashed options), driven from WSL through
  `cmd.exe /c`; usbipd lost the device mid-flash about half the time.
- Pass `< /dev/null` to `cmd.exe` in scripts, or it reads the script's stdin.
- pyserial's `open()` asserts DTR/RTS (RTS = reset, DTR = boot strap) and can
  wedge USB until a replug: set both False before `open()`.
- **A light-sleeping unit on its battery shows no USB port.** Flash in the
  boot window: power it from USB alone (cell or PPK2 off), and let a retry
  loop catch the port. App-only flashes (`0x20000`) keep NVS and the fabric.
- Toolchain: `espressif/esp-matter:release-v1.6_idf_v5.5.5` **pinned by
  digest**; Espressif re-pushes tags (`docs/build.md`).

## 7. Mechanical

- Stacked on the Seeed ePaper driver board with sockets: 15.5 mm from the
  XIAO's USB-C top to the driver board's underside; the board's header pins
  stand 9.0 mm below it until trimmed (`hardware/case/README.md`, rev 5).
