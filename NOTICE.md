# Third-party attribution

homecadia itself is MIT licensed — see [LICENSE](LICENSE). The following parts
are derived from other people's work and carry their own attribution.

| Where | Source | License |
|---|---|---|
| `hardware/case/` | "veltoc" by danking6, [github.com/danking6/veltoc](https://github.com/danking6/veltoc) — enclosure remixed; encoder/display UX patterns ported into the firmware | MIT |
| `firmware/sensor-01/` scaffolding | `examples/icd_app` in [espressif/esp-matter](https://github.com/espressif/esp-matter) | Apache-2.0 / public-domain example code |
| `firmware/components/monogfx/font5x7.c` | classic 5×7 bitmap font from Adafruit-GFX `glcdfont.c`, © 2012 Adafruit Industries | BSD |
| `firmware/components/ssd1680/` | command sequences cross-checked against [Adafruit_EPD](https://github.com/adafruit/Adafruit_EPD) (© 2018 Adafruit Industries) and the Solomon Systech SSD1680 datasheet | MIT (reference only) |
| Firmware architecture | Thread LIT ICD configuration and battery-aware Matter endpoints informed by ["TinyENV_Sensor-Thread"](https://github.com/charles-waite/TinyENV_Sensor-Thread) by charles-waite | — |

Espressif's ESP-IDF and esp-matter are consumed as an unmodified toolchain, not
vendored into this repo.

Not affiliated with or endorsed by Seeed Studio, Espressif, the Connectivity
Standards Alliance, Home Assistant, Nabu Casa, or Apple. "Matter", "Thread",
and "Works with Home Assistant" are the trademarks of their respective owners;
this project is not certified under any of those programs.
