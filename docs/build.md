# Building the firmware

Pinned toolchain: **ESP-IDF v5.5.5 + esp-matter release/v1.6** — the same
combination as the CI image. Don't mix other versions; esp-matter is tightly
coupled to its bundled connectedhomeip submodule and the IDF minor version.

Exact pins (read from inside the pinned image, 2026-10-05):

| Component | Ref |
|---|---|
| esp-matter | `36c2634e99c884830897e2b9501e2d9a6c9d60fd` (head of `release/v1.6` on 2026-08-06) |
| connectedhomeip submodule | `d46cc8c2886cbefc338544bdb2e2f8128f3e9970` |
| ESP-IDF | `v5.5.5` |
| Docker image | `espressif/esp-matter:release-v1.6_idf_v5.5.5` |
| Image digest | `sha256:aabfb665283baf669cd39971763ee1e08788ec876cb4d55c55fa5b125cf8e1d9` (built 2026-08-06) |

Every command here and in CI names the image as `tag@digest`; Docker uses the
digest and ignores the tag, which is kept only so the line is readable.

### Toolchain decision (2026-10-05): stay on this image

**Decision:** stay on esp-matter `release/v1.6` at `36c2634` with ESP-IDF
v5.5.5, pinned by image digest. Do not move to `release/v1.6.1` or to newer
`release/v1.6` commits without a re-measure.

**Why the digest:** Espressif re-pushes its image tags. On 2026-09-17
`release-v1.6_idf_v5.5.5` moved to a new image (`sha256:80664fb6…`) carrying
`release/v1.6` head `c6607128`, 23 commits past the pin. The local image
stayed at `aabfb665`, so from that date CI built against different esp-matter
code from the builds that were flashed and measured. All the measurements in
[power-budget.md](power-budget.md) and field-notes §20–§28 are on `aabfb665`.

**What the alternatives are** (upstream state on 2026-10-05):

| Option | What it brings | Why not now |
|---|---|---|
| `release/v1.6` head (`c6607128`, image `sha256:80664fb6…`) | 23 fixes. The one that touches this build: "Fix SetSlowPollingInterval to enable LIT only if slow polling interval is greater the 15000ms" (2026-08-18). This build runs SIT with the slow poll at exactly 15000 ms, so no change is expected — unverified. The rest are scenes, strings, fan control, bitmaps, a Thread border router fix, and the IDF v6 Docker build | Every change to the ICD path needs a PPK2 capture to confirm the 125 µA; nothing in the 23 is needed |
| `release/v1.6.1` (`717b4bf9`) | 116 commits beyond v1.6: connectedhomeip moved to its v1.6.1 branch, data model regenerated, and "fail loud when setting code-driven cluster attributes" (2026-08-03), which makes the field-notes §9 trap return an error instead of doing nothing silently. It does not fix §9; the workaround stays | Its only image is `release-v1.6.1_idf_v6.0.2`: moving means ESP-IDF 6.0, a major version. Power-management and sleep findings here cite IDF 5.5.5 source by line (`sleep_modes.c`, `ICDManager.cpp`), and the sleep current would have to be re-measured from zero |
| `main` | Development branch | Not a release |

**When to revisit:** when a fix or feature is needed that only a newer
esp-matter has, when ESP-IDF 5.5 drops out of support, or between hardware
revisions. Moving is one change: new digest here and in
`.github/workflows/build-sensor-01.yml`, rebuild, re-read the cited SDK lines,
then a 2 h PPK2 capture on a paired unit analysed with `tools/ppk2-events.py`
against the 125 µA row in power-budget.md.

Assumes Linux. Two paths; Docker is the low-friction one.

## Path A: Docker (matches CI exactly)

```sh
docker pull espressif/esp-matter:release-v1.6_idf_v5.5.5@sha256:aabfb665283baf669cd39971763ee1e08788ec876cb4d55c55fa5b125cf8e1d9

# from the repo root
# (the cd must be inside bash -c: the image's shell init overrides docker's -w
#  and drops you in $ESP_MATTER_PATH, where idf.py would build esp-matter itself.
#  The named ccache volume makes rebuilds after fullclean fast.)
docker run --rm -it -v "$PWD":/work \
  -v homecadia-ccache:/root/.cache/ccache -e IDF_CCACHE_ENABLE=1 \
  espressif/esp-matter:release-v1.6_idf_v5.5.5@sha256:aabfb665283baf669cd39971763ee1e08788ec876cb4d55c55fa5b125cf8e1d9 \
  bash -c 'cd /work/firmware/sensor-01 && idf.py set-target esp32c6 build'
```

Flashing from inside the container needs the serial device passed through:

```sh
docker run --rm -it --device=/dev/ttyACM0 -v "$PWD":/work \
  espressif/esp-matter:release-v1.6_idf_v5.5.5@sha256:aabfb665283baf669cd39971763ee1e08788ec876cb4d55c55fa5b125cf8e1d9 \
  bash -c 'cd /work/firmware/sensor-01 && idf.py -p /dev/ttyACM0 flash monitor'
```

(XIAO ESP32-C6 enumerates as USB CDC, typically `/dev/ttyACM0`. If flashing
won't start, hold BOOT while plugging in.)

### Diagnostic image: light-sleep counters

To see what the chip does while it sleeps, build `build-diag`: the shipping
`sdkconfig` plus `CONFIG_HOMECADIA_SLEEP_DIAG`, which selects the esp_pm
light-sleep callbacks and `CONFIG_ESP_SLEEP_DEBUG`
([field-notes.md](field-notes.md) §21):

```sh
docker run --rm -v "$PWD":/work espressif/esp-matter:release-v1.6_idf_v5.5.5@sha256:aabfb665283baf669cd39971763ee1e08788ec876cb4d55c55fa5b125cf8e1d9 bash -c '
  cd /work/firmware/sensor-01 && mkdir -p build-diag &&
  cp sdkconfig build-diag/sdkconfig &&
  echo CONFIG_HOMECADIA_SLEEP_DIAG=y >> build-diag/sdkconfig &&
  idf.py -B build-diag -DSDKCONFIG=build-diag/sdkconfig reconfigure build'
diff <(grep -E "^CONFIG_|is not set" firmware/sensor-01/sdkconfig) \
     <(grep -E "^CONFIG_|is not set" firmware/sensor-01/build-diag/sdkconfig)
```

The diff must show only the option and what it selects. Use `reconfigure`
after touching `Kconfig.projbuild`: a plain `build` once left a newly
`select`ed option unset.

Run it on the PPK2 with USB out and read the DIAG screen, which the image
redraws every 60 s (the dial cannot open it on battery). Lines, top down:
sleep calls / attempts / % of uptime asleep; requested sleep lengths; actual
sleep lengths and early wakes; wake causes; power-down flags of the last
sleep, those set in every sleep (AND) and in any sleep (OR), bit meanings in
`esp_private/esp_pmu.h` (`PMU_SLEEP_PD_*`: TOP 0, MODEM 2, HP_PERIPH 3, CPU 4,
XTAL 10, RC_FAST 11); share of sleep time with the modem domain and XTAL on;
BLE controller status and the sleep-retention bitmaps.

## Path B: native install

```sh
# ESP-IDF v5.5.5
git clone -b v5.5.5 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c6 && cd ..

# esp-matter v1.6, at the pinned commit (the branch head has moved past it)
git clone -b release/v1.6 https://github.com/espressif/esp-matter.git
cd esp-matter
git checkout 36c2634e99c884830897e2b9501e2d9a6c9d60fd
git submodule update --init --depth 1
./connectedhomeip/connectedhomeip/scripts/checkout_submodules.py --platform esp32 linux --shallow
./install.sh
cd ..
```

Per shell session:

```sh
source esp-idf/export.sh
source esp-matter/export.sh
```

Then:

```sh
cd firmware/sensor-01
idf.py set-target esp32c6 build
idf.py -p /dev/ttyACM0 flash monitor
```

Expect the first esp-matter build to take a long time (it compiles
connectedhomeip) and the native clone step to download several GB.

## Versioning

`PROJECT_VER` (Matter `SoftwareVersionString`) and `PROJECT_VER_NUMBER` (the
`SoftwareVersion` integer OTA compares) are both derived from the git tag in
`firmware/sensor-01/version.cmake`. Do not set them by hand — they were
hand-set once and drifted to `0.1` / `1` while the repo was tagged `v0.6.0`.

Encoding is `MAJOR*10000 + MINOR*100 + PATCH`, so `v1.2.3` is `10203`.

| Tree state | String | Number |
|---|---|---|
| exactly at `v0.6.0` | `0.6.0` | 600 |
| 63 commits past it, dirty | `0.6.0-dev.63+ebc14cd-dirty` | 600 |
| no tags reachable | `0.0.0-untagged` | 0 |

**The number moves only at tags.** Dev builds keep the tag's number because
they are flashed over USB and never OTA'd, so releasing is `git tag v0.7.0`
and rebuilding — nothing else to remember.

Commits-since-tag is deliberately *not* folded into PATCH. `v0.6.0` plus 63
commits would be 663, and a later real `v0.6.1` would be 601 — lower than what
is already on the device. OTA would then decline the update silently, forever.

Check it after a build, locally or in CI:

```sh
tools/check-version.sh
```

It verifies the tag parses, that the built image actually carries that version
(read from `esp_app_desc_t` at offset 0x30, which catches a stale build
directory), and that the number exceeds the previous tag's.

**CI needs `fetch-depth: 0`.** A shallow checkout has no tags and would build
`0.0.0-untagged` without failing.

## Flashing from WSL2

USB devices reach WSL through usbipd-win. Field-tested sequence (unit 1):

```sh
# Windows, admin PowerShell, once per board+port:
usbipd bind --busid <BUSID>          # find BUSID with: usbipd list  (XIAO = 303a:1001)

# WSL, per plug-in:
usbipd.exe attach --wsl --busid <BUSID>
# board appears as /dev/ttyACM0; flash via the docker command above with
# --device=/dev/ttyACM0 and `idf.py -p /dev/ttyACM0 flash`
```

**The reliable path is Windows-native esptool**, not usbipd: the C6's
native USB re-enumerates on every reset and usbipd loses it mid-flash about
half the time. Leave the board *Not shared*, take the COM port from
`usbipd.exe list` (it follows the physical port), copy the four images from
`build/` (or `build-bench/`, `build-diag/`) to a Windows folder, and run from
WSL:

```sh
cmd.exe /c "cd /d C:\\path\\to\\images && python.exe -m esptool --chip esp32c6 -p COM11 \
  -b 460800 --before default-reset --after hard-reset write-flash --flash-mode dio \
  --flash-freq 80m --flash-size 4MB 0x0 bootloader.bin 0xc000 partition-table.bin \
  0x1d000 ota_data_initial.bin 0x20000 homecadia-sensor-01.bin"
```

NVS survives, so the device keeps its fabric. On a board powered from the
PPK2, lift the PPK2's VOUT lead before plugging USB in: the two must never
feed BAT+ together.

Gotchas, all hit in practice:

- **A board running firmware without `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION`
  cannot be flashed normally** — light sleep kills the USB port within ~2s of
  boot (host log: `device descriptor read/8, error -32`) and the
  replug-and-race approach loses to usbipd's attach latency. Recovery: hold
  the **B** button (tiny switch beside the USB-C), plug in while holding,
  release after ~2s — ROM download mode never sleeps. Current firmware pins
  the port awake whenever USB is connected, so this is only needed for
  boards with old/foreign firmware.
- **`usbipd attach` does not survive the device re-enumerating** (e.g. after
  `esptool` exits download mode). `usbipd attach --wsl --busid <BUSID>
  --auto-attach` re-attaches automatically, with ~2–7s latency.
- **Don't script raw reads of `/dev/ttyACM0` for boot logs.** Opening the tty
  pulses the USB-Serial-JTAG control-line latch and can hard-reset the chip —
  sometimes into download mode. Use `idf.py -p /dev/ttyACM0 monitor` in an
  interactive terminal (it owns the reset/reconnect dance); expect the boot
  banner only from its own reset, and expect the port to drop in deep sleep.

## CI

`.github/workflows/build-sensor-01.yml` runs on every push touching
`firmware/**`, in one job using the same Docker image as above. The image pull
dominates the runtime, so extra steps are near-free but a second *job* would
pay that cost again — append steps, do not split.

| Step | Catches |
|---|---|
| Build, shipping profile | ordinary breakage |
| Build, bench profile | code behind `#if CONFIG_PM_ENABLE` that compiles in one profile and not the other; `sdkconfig.bench` also enables the boot-time harness scan (`CONFIG_HOMECADIA_BENCH_SELFTEST`) |
| `tools/check-profiles.sh` | the bench profile silently no longer disabling sleep, and `CONFIG_HOMECADIA_BENCH_SELFTEST` reaching a shipping image — it asserts `# CONFIG_HOMECADIA_BENCH_SELFTEST is not set` in the shipping sdkconfig and `=y` in the bench one, since bench-only diagnostics must not reach a shipping image |
| `tools/check-version.sh` | a `SoftwareVersion` that is stale, untagged, or fails to increase |

## Reading a node over Matter

`tools/matter-node.py` reads a node through the matter-server WebSocket
(`ws://192.168.1.173:5580/ws`, override with `MATTER_WS`) and prints version,
reboot count, temperature, humidity and battery. One shot, 90 s bound.

```sh
tools/matter-node.py get        # server's subscription cache: free for the device
tools/matter-node.py interview  # full read of every attribute on the device
```

`interview` costs the sensor ~150 mC, about ten minutes of its normal running
(field-notes §22); use it once to prove liveness after a reflash, not as a
poll. For calibration or soak work, listen for `attribute_updated` events
instead: every 120 s report arrives with no extra radio traffic.

Flash images (app, bootloader, partition table, `flasher_args.json`) are
uploaded as an artifact.

Both check scripts run locally too, and both fail on a missing or malformed
input rather than passing vacuously — a guard that quietly does nothing is
worse than no guard, because it manufactures confidence
([field-notes.md](field-notes.md) §12).

### What is deliberately not tested

No unit tests. Nearly everything here is I/O — SPI to the panel, I2C to the
sensor, the Matter stack, the ADC — and the small amount of pure logic (the
EC11 Gray-code table, the settings clamp) has not been where the bugs were.
The defects that actually cost days on this project were a reversed FPC, a
`busy_wait()` that could not tell a working panel from a silent one, a stale
container tag, `attribute::update()` writing to a store nothing reads, and BLE
memory being reclaimed after commissioning. No unit test finds any of those.

Hardware-in-the-loop is the right answer eventually and the wrong one now:
there is one working panel and one bench board.
