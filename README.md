# homecadia

Home-built smart home devices for Home Assistant, Matter over Thread.

<!-- photo goes here once a unit is assembled: ![sensor-01](docs/img/sensor-01.jpg) -->

## Devices

| Device | Status | Description |
|---|---|---|
| [sensor-01](firmware/sensor-01/) | in development | Battery-powered room temp/humidity sensor. 2.9" e-ink display, rotary dial, Matter-over-Thread sleepy end device (LIT ICD). 3 units. |

## sensor-01 hardware

Seeed XIAO ESP32-C6 + Seeed ePaper Driver Board for XIAO V2 + 2.9" mono e-ink
(296×128, SSD1680) + Grove SHT40 + EC11 rotary encoder + 2000mAh LiPo, in a
wall-mount enclosure remixed from [veltoc](https://github.com/danking6/veltoc).

Full bill of materials: [docs/bom.md](docs/bom.md). Pin map: [docs/pinmap.md](docs/pinmap.md).

## Firmware

Espressif [esp-matter](https://github.com/espressif/esp-matter) v1.6 on
ESP-IDF v5.5.5, C++. Thread only (Wi-Fi disabled), commissions over Bluetooth
Low Energy to Home Assistant via an OpenThread border router (Home Assistant
Connect ZBT-2). Build instructions: [docs/build.md](docs/build.md).

## Milestones

| # | Deliverable | Status |
|---|---|---|
| 1 | Repo scaffold, docs, CI compiling an esp-matter skeleton for esp32c6 | done |
| 2 | Matter temp/humidity over Thread, commissions to HA (TinyENV parity) | code complete; commissioning test awaits hardware |
| 3 | Display driver, view 1 rendering readings, measured refresh cost | — |
| 4 | Encoder, views, settings, wake behavior | — |
| 5 | ICD tuning, battery reporting, power budget with measured numbers | — |
| 6 | Factory reset, low-battery behavior, assembly guide final, v1.0.0 | — |

Hardware-dependent code written before parts arrive is marked `HW-VERIFY` in
source and tracked in [docs/bringup.md](docs/bringup.md).

## Repo layout

```
docs/               BOM, pin map, assembly, commissioning, power budget, build
hardware/case/      wall-mount enclosure (veltoc remix, MIT)
firmware/
  components/       drivers shared across future homecadia devices (from M2 on)
  sensor-01/        esp-matter application
.github/workflows/  CI: firmware build on push, .bin artifacts
```

## License

MIT, see [LICENSE](LICENSE). Enclosure and UX patterns derived from
[veltoc](https://github.com/danking6/veltoc) (MIT); architecture informed by
[TinyENV_Sensor-Thread](https://github.com/charles-waite/TinyENV_Sensor-Thread).
