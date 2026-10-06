# Field notes: traps that cost real time

Hard-won during sensor-01 bring-up (2026-08-17 → 24). Ordered by how much time
each one burned. Read this before bringing up units 2 and 3 — most of these
cost hours the first time and minutes once you know them.

## 1. Make "working" distinguishable from "silent" before you need it

`ssd1680.c`'s `busy_wait()` returned `ESP_OK` the instant BUSY read low, which
is also the resting state of a panel that received nothing. Every refresh
reported success while nothing was drawn. **Two days** went into probing pins
with a multimeter because the firmware said the display was fine.

Fixed by requiring BUSY to *rise* after `CMD_MASTER_ACTIVATE` (~1.8s full,
~0.5s partial). The general rule: **a status check that cannot fail is a bug.**
When adding any driver, ask what a disconnected part would return, and if it is
the same as success, fix that first.

## 2. Light sleep kills the USB-serial-JTAG console

With `CONFIG_PM_ENABLE` + `CONFIG_FREERTOS_USE_TICKLESS_IDLE` +
`CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP`, the port still **enumerates**
but every open fails at driver level — on Windows, `A device attached to the
system is not functioning`. It reads exactly like broken hardware. Hours were
lost blaming pyserial.

Use `sdkconfig.bench` for anything needing a console:

```sh
idf.py -B build-bench -DSDKCONFIG=build-bench/sdkconfig \
       -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.bench" build
```

**Nothing verified on the bench profile carries over to the shipping profile.**
ICD timing, radio wake and peripheral power-down all differ — re-validate
Matter and Thread on the default profile before shipping.

## 3. Map a tty to its USB VID before opening it

`node1` carries both the XIAO (`303a:1001`) and the Zooz Z-Wave stick
(`1a86:55d4`). Both appear as `ttyACM*`, and the numbering is not stable.
Opening the wrong one asserts DTR/RTS and resets someone else's radio.

```sh
for t in /sys/class/tty/ttyACM*; do
  root=$(readlink -f $t/device/..)
  echo "$(basename $t) -> $(cat $root/idVendor):$(cat $root/idProduct)"
done
```

Related: the ESP32-C6's USB-serial-JTAG discards output when it decides no host
is attached, and stays that way. If the port opens but yields nothing, reset
with esptool and capture immediately — a plain `cat` often never sees a byte.

## 4. ePaper FPC: go by insertion force, not by which way the copper faces

The correct orientation slides in with light finger pressure; the reversed one
needs pushing. Reversed, tab *n* meets panel pin 25−*n*, putting VDDIO on RST
and VCI on BUSY — the MCU then drives a supply rail and pulling RST low shorts
it. **Two panels died this way.** Seat until the stiffener is inside the
housing; a ribbon stopped short still latches and makes intermittent contact.
Insert and remove only with power off, and treat the bare flex as ESD-sensitive.

The XIAO seats with its **USB-C end away from the FPC connector**. Backwards,
nothing works and nothing looks wrong.

## 5. Matter commissioning needs three things aligned at once

1. **Test DAC trust.** Self-built devices carry esp-matter's test DAC, signed by
   a PAA not in the production DCL. Without `ENABLE_TEST_NET_DCL=true` on
   matter-server, commissioning reaches attestation and dies with
   `PAA not found in trust store`. The server downloads test roots either way;
   the flag is what makes it *trust* them.
2. **A working BLE proxy.** matter-server allows exactly **one** `/ble` client,
   and Home Assistant grabs it. On matter-server >= v0.8.0 that is fine — leave
   it alone and let HA's own proxy do the work. On older builds the bleak client
   could not complete the BTP handshake (matter-js/matterjs-server#1006) and the
   Node/noble client from `docs/ble-proxy-pod.yaml` was needed instead. See §12
   for why that was chased for two evenings rather than fixed in five minutes.
3. **Thread in range.** BLE and Thread must both reach the device *at the same
   time* — `connectNetwork` has to succeed inside the fail-safe window.

The commissioning window is time-limited from boot. **The QR staying on the
e-ink is not evidence the window is open** — it is drawn once and the panel
just holds the image. Power-cycle immediately before commissioning.

## 6. Check the infrastructure is alive before theorising about the device

An OTBR that has lost its dataset presents as `state: disabled` with an empty
child table, and every device then looks like it is out of range. Hours went
into a shielding theory while the border router simply was not running.

```sh
ot-ctl state        # expect leader/router/child, not disabled
ot-ctl ifconfig     # expect up
ot-ctl dataset active | head   # expect a network name
```

Restore from the dataset HA keeps in `/config/.storage/thread.datasets`:

```sh
ot-ctl dataset set active <tlv>
ot-ctl ifconfig up
ot-ctl thread start
```

Same credentials, so nothing needs re-provisioning. **Check this first whenever
a device will not attach.**

## 7. Thread coverage is a deployment problem, not a commissioning one

One border router in the basement does not cover a second-floor office. A
sensor commissions fine next to the ZBT-2 and cannot hold the mesh where it
actually lives. Any mains-powered Matter-over-Thread device becomes a router;
budget for one per floor. A Nanoleaf Shapes is a border router but hosts *its
own* network and cannot be moved onto yours without its credentials, which
Apple will not surrender without an Apple border router present.

## 8. Debugging discipline

- **Attribute traffic to the right device.** A shared Bluetooth adapter carries
  every BLE device in the house. Resolve the connection handle for the address
  you care about before reading anything into a capture.
- **One variable at a time.** Several hours here came from moving the device,
  reflashing and changing config between attempts.
- **Suspect your own instrumentation.** A capture that resets the device, a
  `pkill` that matches its own shell, an `sed` range whose end marker never
  matches — all produced confident, wrong conclusions in this build.

## 9. `attribute::update()` is a no-op for code-driven clusters

**Resolved 2026-08-23** — cause, fix and upstream status below. Symptom: the
device logs correct readings, a controller reads null for temperature and
humidity while battery reads fine, and HA creates no sensor entity at all
(it skips a sensor whose primary attribute reads null).

The investigation is kept in full because the elimination order is the reusable
part — every layer reported success, and the shape recurs.

What was established:

- Endpoints declare the right device types — `770` (0x0302 Temperature Sensor)
  on ep1, `775` (0x0307 Humidity Sensor) on ep2 — with clusters 1026/1029.
- `MinMeasuredValue` (`1/1026/1`) reads back `-4000` over the wire. Set from
  the same `config_t` by the adjacent SDK line. So the cluster is served fine.
- `MeasuredValue` (`1/1026/0`) reads null — whether set at creation via
  `temp_cfg.temperature_measurement.measured_value` or at runtime via
  `attribute::update()`.
- A forced `interview_node` (a live read of everything) still returns null, so
  it is not a matter-server cache artifact.
- Reading the attribute back on the device with `attribute::get_val()`
  immediately after the update returns `type=137` (nullable int16) with
  `i16=-32768` — the null sentinel — while `esp_matter_attribute` has just
  logged `Attribute 0x00000000 is 2437`.

So the value is lost inside `update()`, and that log line prints the requested
value rather than the stored one. Battery attributes on ep3 work at both
creation and runtime; they are the two the app creates explicitly with
`create_bat_percent_remaining` / `create_bat_voltage`, which is the only
structural difference found so far.

A controlled comparison narrows it to the attribute rather than the API. Both
calls are in the same function, one line apart, through the same `update()`:

```
DIAG in:               type=137 i16=2442                      <- value we construct is correct
DIAG after update():   err=ESP_OK  type=137 i16=-32768         <- temperature, nullable int16: LOST
DIAG after set_val():  err=ESP_ERR_NOT_FINISHED type=137 i16=-32768
DIAG battery readback: type=140 u32=680 (wrote 680)            <- battery, nullable uint32: STORED
```

So `esp_matter_nullable_int16()` builds the value correctly, `update()` returns
`ESP_OK`, and the store ends up holding the null sentinel — while the identical
pattern on `BatVoltage` stores fine. The only structural difference found: the
battery attributes are created explicitly by the app with
`create_bat_percent_remaining` / `create_bat_voltage`, whereas the measurement
attributes come from `endpoint::temperature_sensor::create()`.

Note `esp_matter_attribute`'s `Attribute 0x00000000 is 2442` prints the
*requested* value, not the stored one — the same "a log that cannot report
failure" shape as the `busy_wait()` defect in section 1. Do not trust it.

### Root cause: writes and reads use different stores

Established by writing a value that could not already be present and reading it
straight back:

```
DIAG after set_val(4660): err=ESP_OK   type=137 i16=-32768
```

`set_val()` returned `ESP_OK` — it stored — and the read immediately after still
returned the null sentinel. Writing the *same* value twice instead returns
`ESP_ERR_NOT_FINISHED`, which `set_val` only emits when `val_compare()` finds
the stored value equal to the new one. So the writes are landing.

Following the SDK:

- `set_val()` writes `current_attribute->attribute_val` on the `_attribute_t`
  struct, and `val_compare()` compares against that same struct.
- `get_val(attribute_t *)` does **not** read that struct. It resolves the path
  and delegates to `get_val(endpoint_id, cluster_id, attribute_id, ...)`, the
  path-based read that the Matter wire read also goes through.

For `MeasuredValue` those two are not linked, so every write succeeds and every
read returns null. For `BatVoltage` they are — and that attribute is created
explicitly by the app with `create_bat_voltage()` rather than by
`endpoint::temperature_sensor::create()`. That is the only structural
difference found, and it matches which attributes work.

Two consequences worth carrying:

- `attribute::update()` returning `ESP_OK` does not mean a controller will be
  able to read the value back. Verify over the wire, not from the return code.
- `esp_matter_attribute`'s `Attribute 0x... is <value>` log prints the
  *requested* value. It is not evidence of storage.

### Ruled out: bounds

The only difference between the two creation paths in the SDK is one line —
`create_bat_voltage()` calls `attribute::add_bounds()` and
`create_measured_value()` does not. Applying bounds to both MeasuredValue
attributes applied cleanly and did not fix the read. Bounds are not the cause.

### Resolved: reads and writes go to different objects (esp-matter v1.6)

Following the read path end to end: the path-based `attribute::get_val()` is
not a struct read. It runs a full simulated Matter read through
`data_model::provider::ReadAttribute()`, which is why the device-side read-back
and the wire read always agreed. That function checks, in order:

1. `mRegistry.Get(path)` — a registered code-driven server-cluster object
2. an `AttributeAccessInterface` for the cluster
3. `get_val_internal()` — esp-matter's own attribute store

`attribute::update()` writes store 3 only. esp-matter v1.6 registers a
code-driven `TemperatureMeasurementCluster` / `RelativeHumidityMeasurementCluster`
per endpoint (`data_model_provider/clusters/<cluster>/integration.cpp`), so
reads stop at step 1 and return that object's own `mMeasuredValue`, which is
never seeded — its `StartupConfiguration` carries min, max and tolerance only —
and never synced, because `temperature_measurement` has `function_list = NULL`
so `set_val_internal()`'s attribute-changed hook never fires. Min/Max survive
because the object reads them from the store at startup via `GetDefault()`.
PowerSource is exempt because esp-matter keeps it AAI-based ("uses AAI (not
SCI)" in its integration.cpp), so BatVoltage reads fall through to step 3.

**Espressif's own `examples/sensors/main/app_main.cpp` uses the same
`attribute::update()` call and is broken the same way on v1.6.** The same
applies to every measurement cluster with a registered SCI: pressure, flow,
illuminance, soil, air quality, occupancy, boolean state.

Fix (`sensor_loop.cpp`): set the value on the registered object.
`provider::registry()` is public, so

```cpp
auto *iface = esp_matter::data_model::provider::get_instance().registry().Get(
    chip::app::ConcreteClusterPath(endpoint_id, TemperatureMeasurement::Id));
static_cast<TemperatureMeasurementCluster *>(iface)->SetMeasuredValue(MakeNullable(v));
```

under the Matter stack lock. esp-matter's humidity integration already exports
exactly this as `RelativeHumidityMeasurement::SetMeasuredValue(EndpointId, ...)`
in its `integration.h`; the temperature integration has no header. Verified
over Thread: `1/1026/0 = 2463`, `2/1029/0 = 4952`, matching the device log.

Upstream status (checked 2026-08-23): this is **espressif/esp-matter#1798**
(2026-07-20, same root cause, same workaround) and **#1738** (2026-03-23, the
OccupancySensing sibling). Espressif confirmed it as a bug on both. On `main`:
`9a1c0777` (2026-07-23) makes `set_val` return `ESP_ERR_NOT_SUPPORTED` when the
cluster is SCI-served, and `3abe4c20` (2026-08-14) adds
`TemperatureMeasurement::SetMeasuredValue(EndpointId, ...)`. **Neither is on
`release/v1.6`** (tip `31b76ad1`, 2026-08-20), which is what this project pins,
so there the call still silently succeeds and there is no public setter. The
real fix — `update()` routing to the registered object — is still open.

When the toolchain moves to a release containing `3abe4c20`, the wrapper can
replace the downcast in `sensor_loop.cpp`; the registry approach keeps working
either way.

Rule to keep: **`attribute::update()` is only correct for attributes esp-matter
actually serves.** For any cluster with a local SCI integration, the served
value lives on the registered object and must be set there. Check
`data_model_provider/clusters/<name>/integration.cpp` for `registry().Register`.

## 10. Measuring sleep current with a manual-ranging DMM (AstroAI AM33D)

The meter on hand is an AstroAI AM33D: 2000-count 3½-digit, **manual ranging**,
DC current only. Read off the dial 2026-08-23:

| | |
|---|---|
| DC A ranges | **2000 µ · 20 m · 200 m · 10** |
| VΩmA jack | fused, **500 mA MAX**, 600 V MAX |
| 10 A jack | fused, **MAX 10 SEC EACH 15 MIN** |
| DC V ranges | 200 m · 2000 m · 20 · 200 · 600 |

2000 µA on a 2000-count display is 1 µA resolution — enough for a 15–40 µA
sleep floor. The problem is not resolution; it is what the meter does to the
device during a radio burst. Two consequences of the jack ratings: a C6 TX
burst (~100–300 mA) will **not** blow the 500 mA mA-jack fuse, so the fuse trap
below is mostly ruled out on this meter; but the 10 A jack **cannot** stay in
circuit through a boot and commissioning, so the bypass jumper in the procedure
is mandatory, not a convenience.

**The trap, in two forms.** Before trusting any number, rule out the
instrument. A XIAO ESP32-C6 user on the Seeed forum measured 57–80 mA where an
ESP32-H2 running identical code drew 2.5 mA and concluded the board was broken.
It was the meter: a flat meter battery plus the wrong input jack dropped enough
voltage across the meter to restart the C6 continuously. With that fixed the
real figure was ~2 mA. The failure mode is nasty because the device genuinely
misbehaves — it is restarting — so the reading looks like a real fault.

The mechanism is **burden voltage**: on a low current range the meter's shunt
is large (roughly 100 Ω for a 2000 µA range on this class), so a sleep-floor
reading is fine but a Thread TX burst of ~100–300 mA across that shunt drops
volts, browns out the C6, and it reboots — and this device bursts every
`CONFIG_ICD_SLOW_POLL_INTERVAL_MS` = 5 s, so you cannot wait one out. On many
meters of this class the burst also exceeds the mA-jack fuse (a blown fuse
reads as a device that draws nothing); the AM33D's 500 mA rating gives margin,
but check the fuse first if a reading is exactly zero.

**Procedure that works with this meter:**

1. Fresh 9 V in the meter. Check it first, not after.
2. Power the board from the cell (or a bench supply at 3.7–4.2 V) through the
   battery pads. **USB disconnected** — USB powers the board and bypasses the
   measurement entirely.
3. Put the meter in series with the battery **positive** lead.
4. Fit a **bypass jumper across the meter's leads** and keep it closed through
   boot and commissioning. The board never sees the shunt, and the 10 A jack's
   10-second limit never applies because the meter carries nothing until the
   device is asleep.
5. Once the device is attached to Thread and the panel is in deep sleep, move
   the red lead to the µA/mA jack, select the lowest DC range, then open the
   jumper. Read. If the display shows a reboot pattern (reading collapses and
   climbs every few seconds), the burst is browning it out — close the jumper.
6. To read a stable floor with bursts present, either raise
   `CONFIG_ICD_SLOW_POLL_INTERVAL_MS` in a measurement build so bursts are rare
   enough for the display to settle between them, or put a low-ESR bulk
   capacitor (1000–4700 µF) across the board's supply so bursts are sourced
   locally and the meter sees something close to the average. Say which was
   used when recording the number.

**What this meter cannot do:** capture the burst itself or a true average of a
bursty load. The per-poll and per-refresh charge figures in
[power-budget.md](power-budget.md) need a Nordic PPK2 or a µCurrent, as that
file already says. The AM33D answers one question — the sleep floor — and only
when the bursts are kept off it.

Record alongside the reading: meter range and jack, whether a bypass cap or a
longer poll interval was used, panel state, LED state, and whether the display
had refreshed within the previous minute.

## 11. There is no reference implementation for this build

Worth knowing when something does not work: as of 2026-08, nothing published
combines XIAO ESP32-C6 + esp-matter (ESP-IDF, C++) + Matter over Thread as a
LIT ICD + ePaper + battery + encoder. What exists:

- Seeed's wiki and marketing: "supports Matter and Thread", no working device.
- Seeed forum threads at Arduino / ESP LaunchPad level — sketches exceeding the
  1.3 MB limit, needing a "Huge APP" partition scheme, and commissioning that
  "added to Google Home but showed disconnected", HA discovery failing, and
  needing physical proximity to the hub. None of it diagnosed. In hindsight
  those read like the same two causes that cost days here: Thread coverage and
  a BLE proxy that cannot complete the BTP handshake.
- `github.com/Frapais/Sprig-C6` — a competent C6 board with battery management,
  but ESPHome/WiFi oriented, no display, ~32 mA average. Not a sleepy ICD.
- `tomasmcguinness.com/2025/01/06/lowering-power-consumption-in-esp32-c6/` —
  the only material found doing real Matter power work on this silicon. Worth
  reading before milestone 5.

Consequence: when something breaks there is no known-good implementation to
diff against, so budget for first-principles debugging and keep instrumenting.
One independent confirmation did turn up — the same light-sleep serial symptom
in section 2, reported verbatim ("device reports readiness to read but returned
no data") by another XIAO C6 user.

## 12. A version number in someone else's manifest is not a container tag

The manifest for matter-server was written on 2026-08-19 pinning
`ghcr.io/matter-js/matterjs-server:0.7.1`. That tag was released 2026-05-21 and
was already three months and twenty-plus releases stale on the day it was
written; current was 1.4.0.

The `0.7.1` came from the right-hand side of this line in
`kubernetes/apps/default/matter-server/README.md`:

> HA 2026.8.1's `manifest.json` requires `matter-python-client==1.3.0` and
> **`matter-ble-proxy==0.7.1`**

`matter-ble-proxy` is a **PyPI package** on its own numbering. The container
image is a different artefact with different versioning. The number was carried
across because it was sitting on the same line of the same file.

Cost: two evenings debugging a BLE commissioning failure, an upstream bug report
(matter-js/matterjs-server#1006), and a PR that was half redundant — for a
defect fixed upstream **twelve days after** the pinned tag. The maintainer
identified the build from the log format before we thought to check it.

Two things would each have caught it:

- **Ask "is this current?" when pinning anything.** One API call. The version
  was correctly stated in the bug report; nobody compared it to the latest
  release.
- **A working dependency bot.** Renovate was configured and running, had already
  detected the update, and had queued PRs for `v0.8.0` and `v1` — but six
  branches had errored on a `403` (`GET /commits/<sha>/statuses`, missing
  **Commit statuses: Read** on the token) and were holding all six
  `prConcurrentLimit` slots. Nothing new could be proposed. The backlog was
  visible only as unticked checkboxes on the dependency dashboard issue, which
  nobody opens.

Second-order lesson: **a guard that fails silently is worse than no guard**,
because it produces the feeling of coverage. Check that the bot is actually
opening PRs, not merely scheduled.

## 13. BLE is gone after commissioning, so reopening a window is not enough

`kFabricRemoved` reopened the commissioning window so an un-paired device could
be re-adopted without a physical factory reset. It never worked, for two
independent reasons, and the second one is the interesting one.

First: the window was opened with `kDnssdOnly`. Removing the last fabric also
takes the device off Thread, so it advertised on a network it had just left.

Fixing that to `kAllSupported` changed nothing. The device still advertised
**nothing at all** — `removeFabric` returned `statusCode: 0`, the `Leave` event
arrived, and `btmon` on the controller's own adapter saw zero `FFF6` reports.

The reason is in the SDK. With `CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=y`:

```cpp
void BLEManagerImpl::DeinitESPBleLayer()
{
    VerifyOrReturn(DeinitBLE() == CHIP_NO_ERROR);
#ifdef CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING
    BLEManagerImpl::ClaimBLEMemory(nullptr, nullptr);
#endif
}
```

— `connectedhomeip/src/platform/ESP32/nimble/BLEManagerImpl.cpp:1004`

`ClaimBLEMemory` hands the BLE controller's RAM back to the general heap once
commissioning finishes. That cannot be undone in place. **No advertisement mode
could have worked, because there was no BLE stack left to advertise on.**

The fix is to reboot (3-second delay, so the RemoveFabric response and Leave
event get out first). A fresh boot with zero fabrics initialises BLE and
advertises the way first boot does. Verified: 2884 advertisement reports after
`remove_node`, unattended, where the same operation previously produced zero.

The general lesson: **a fix that is not tested on hardware is a hypothesis.**
The first fix was committed with a confident comment explaining reasoning that
turned out to be only half the story, and only failed testing revealed the rest.

## 14. Controllers cache device identity from the commissioning interview

`VendorName` / `ProductName` / `HardwareVersionString` are compile-time strings
(`CONFIG_CHIP_PROJECT_CONFIG` → `main/chip_project_config.h`), and the
controller reads them **once**, during the commissioning interview.

Reflashing corrected strings does not update what the controller shows. Verified:
after flashing, `strings` on the image confirmed `homecadia` present and
`TEST_VENDOR` absent, the flash hash-verified, the device rebooted and rejoined
Thread — and matter-server still reported `TEST_VENDOR` / `TEST_PRODUCT`. Only a
decommission and re-commission changed it.

Forcing a refresh does not work either. `read_attribute` is not a command
matter-server implements — it is silently dropped, no error, no log line. And
`interview_node` times out, because a complete read against a sleepy end device
on a 5-second poll exceeds the interview budget:

```
Interview requested for node @1:16 - do a complete read
WebSocket error response (interview_node) 1 [aborted] Operation aborted
    at abort.timeoutHandler (@matter/protocol/src/peer/Peer.ts:447:27)
```

Practical rule: **get device identity right before commissioning**, and treat
identity changes as requiring a re-pair. Per-unit naming belongs in NodeLabel,
set by the controller, not in VendorName/ProductName — all units share a model.

## 15. The panel FPC is the least reliable joint in the build — check it first

**2026-08-25.** Half a bench session went into a display that would not draw. It
presented as a dead panel and, for a while, as a firmware fault. It was neither:
the FPC was not seated correctly.

The symptom chain, in the order it appeared:

1. `BUSY never rose within 200ms of MASTER_ACTIVATE` on every refresh. The
   readings themselves were fine — `sensor_loop` reported real values every
   poll, so the whole software path ran and only the last step failed.
2. The panel kept showing an old commissioning QR. That is **not** evidence of
   an unpaired device: e-ink holds its last successfully drawn frame with no
   power, and every refresh after it had failed. The device was commissioned
   the whole time (`Fabric index 0x4 ... NodeId 0x17`). See also section 14.
3. MOSI (D10/GPIO18) could not be pulled low — it sat at **3.1 V** while the C6
   drove it low. Something was hard-driving it, not pulling it.

Bisecting by substitution found it, one variable at a time:

| Configuration | MOSI | Meaning |
|---|---|---|
| XIAO alone on USB | follows driver | GPIO18 healthy, no bridge on the XIAO |
| XIAO + driver board #2, no panel | follows driver | not normal behaviour for these boards |
| XIAO + driver board #1, no panel | follows driver | board #1 is fine |
| XIAO + board #1 + panel | **stuck at 3.1 V** | the panel was driving it |

MOSI is an *input* to the SSD1680 and should never drive that line. A misseated
FPC explains it: the ribbon carries supply rails (VDD, VGH, VGL) directly
alongside the signals, so a ribbon that is skewed or not fully home puts a rail
onto a signal net. That also explains why D10-to-3V3 measured **open** with the
board unpowered — the path only exists once the panel's rails come up.

Two things that would have saved the time:

- **A short is not always a bridge.** The unpowered continuity check said "no
  short" and was believed for too long. Anything gated behind a supply reads
  open until the board is powered.
- **A logic-level readback is not a voltage.** `gpio_get_level()` returning 1
  cannot distinguish a hard 3.3 V from an intermediate voltage above V_IH. The
  meter is what turned "stuck high" into "hard-driven by a supply".

After reseating, the fault became *intermittent* before it became fixed —
`BUSY` read high at rest on failing boots and low on working ones, and one boot
got a single command through before dying. Intermittent contact looks like a
flaky driver. It is not: it is a joint.

**Practical rules:**

- **Contacts face UP**, away from the driver board. That is the orientation
  this board expects — confirmed 2026-08-22 and again here. Full detail and the
  insertion-force cross-check are in [assembly.md](assembly.md).
- Reseat the FPC deliberately, once, and inspect it. Latch open before moving
  the ribbon, fully home, square at both edges, equal backing visible each side.
  Repeated blind reseating wears the contacts and is how board #2's connector
  died.
- Suspect the connection before the firmware. Every measurement in this session
  pointed at hardware, and every hour spent on the driver was wasted.
- `ssd1680_init()` now scans and drives each signal at boot and logs the result.
  Read those lines first — `drive hi=1 lo=0 follows the driver` on all five
  outputs is the precondition for anything else being worth investigating.

## 16. A diagnostic that runs after the driver owns the pin measures the driver

**2026-08-25.** Four traps from one bench session, all downstream of
instrumentation that ran too late or looked too shallow.

**The SHT40 probe was measuring the controller, not the wire.** The first I2C
line-state check in `sht40.c` ran *after* `i2c_new_master_bus()` on a failed
probe. It returned byte-identical readings — SDA held HIGH, SCL held LOW —
across every wiring change: sensor connected, sensor disconnected, cable
unplugged entirely. By the time it ran, the I2C peripheral already owned
GPIO22/23, held them with its own internal pull-ups, and was parked in a
stuck-bus state. Several wiring changes were requested on the strength of a
reading that could not have changed no matter what was on the bus.

Rule: **sample a pin before any driver claims it.** That is why the boot-time
harness scan (`firmware/sensor-01/main/bench_selftest.cpp`, gated by
`CONFIG_HOMECADIA_BENCH_SELFTEST`, set in `firmware/sensor-01/sdkconfig.bench`)
is the first call in `app_main()` (`app_main.cpp:199`), ahead of
`board_rf_switch_init()` (`:200`) — nothing has touched a pin yet. The probe
helpers were previously duplicated in `ssd1680.c` and `sht40.c`; they now live
once in `firmware/components/pinprobe/` (`pinprobe_line_state`,
`pinprobe_drive_test`), and `sht40_init()` calls `pinprobe_line_state()` on
SDA/SCL (`sht40.c:59-60`) before the bus is created two lines later.

**"Disconnected" means both ends out.** Same day, second lesson: encoder
jumpers pulled from the encoder end but left plugged into D7/D9 at the XIAO
end are unterminated antennas on two interrupt-enabled pins. The quadrature
ISR (`firmware/components/ec11_encoder/ec11.c` — the Gray table at lines
20–25, the 4-steps-per-detent accumulator at line 37) logged 411 valid-looking
detents in 25s with nobody touching the knob — each one a panel refresh,
~4,900 refreshes/hour against a panel rated for 1,000,000 cycles. Removing the
wires entirely dropped it to 0. An "isolation" test that leaves a wire in the
pin isolates nothing.

**The short that killed the SHT40 was in the breadboard, not on the board or
the panel.** The driver board's header pins sit in breadboard rows, so every
row it occupies is on a signal net — SCL was shorted to ground through the
breadboard itself, not through anything on the board or the FPC. Lifting the
board straight out of the breadboard (USB only) made SCL read floating at
boot. The timeline is the evidence: the sensor read fine through one capture
and failed on the next, and the only change between the two captures was
pulling encoder wires *on the breadboard* — the FPC was not touched. Bisect by
substitution, one reset per step, and read the timeline before touching the
ribbon.

**A line probe on a live bus is itself a fault injector.** A pull-up/pull-down
probe on a live I2C bus is itself a START condition — SDA falling while SCL is
high — so the probe can wedge an attached slave mid-transaction; afterwards
both lines read held LOW, which looks exactly like the dead-bus symptom the
probe was trying to characterise in the first place. Recovery is the standard
nine SCL clocks followed by a STOP. `bench_selftest()` issues that recovery
after probing the I2C pins (`i2c_bus_recover()`, `bench_selftest.cpp:19-47`),
and `sht40_init()` calls `i2c_master_bus_reset()` before its own probe
(`sht40.c:78`) for the same reason.

### Addendum, 2026-08-26 — three more from the encoder bring-up

- **A probe that reconfigures a pin kills whatever driver owns it.** The
  init-step bisect that found the SCL fault (`bench_probe_i2c()`) was left
  wired in after `ui_init()`. Each call ran `gpio_config()` on the encoder pins
  with interrupts disabled and pulls off, so the quadrature ISR never fired
  again and A/B floated. Two rounds of "the encoder produces no events" were
  the instrumentation, not the encoder. The function stays, with a warning in
  its header; the call sites are gone.
- **"Wired as in the diagram" is only right if the diagram is.** The diagram
  had the original D7/D9/D6 assignment; firmware had moved A to D6 and the LED
  to D7 an hour earlier. The drawing is now updated, but the rule is: after a
  pin move, the diagram, `pinmap.md`, `app_config.h` and the breadboard change
  in the same sitting or one of them lies.
- **EC11 blades do not seat.** Breadboard springs and Dupont sockets both
  grip a 0.64 mm square pin; the encoder's flat ~0.6 mm blades sit loose in
  either. Symptom: one line toggles cleanly while the other is stuck, and which
  one changes between runs. Soldered leads fixed it in one go.
- **D7 anomaly — closed 2026-08-31.** On both driver boards GPIO17/D7 read 0 V
  once `display_init()` brought up SPI; the meter read it open unpowered and
  the schematic has nothing on it. Cause found: plumbing-flux residue between
  the adjacent D7/D8 socket joints — see section 17. D7 followed SCK because
  they are neighbours, not because of anything electrical.


## 17. An ohmmeter cannot clear a powered-only path — and plumbing flux makes them

**2026-08-31.** Driver boards #1 and #2 both had 4–8 kΩ conduction between
*adjacent* socket pins (D2↔D3, D3↔D4, D7↔D8, D8↔D9, D9↔D10 — pairs with
nothing electrical in common, only physical adjacency). Every multimeter
resistance measurement of those pairs, on every range, powered off, read
open. Days went into substitution tests that could not succeed: both boards
were soldered with the same flux, so swapping boards was never an independent
trial, and the meter kept "clearing" the true fault.

**Why the meter lies:** a DMM's resistance test drives ~0.3 V. Ionic
contamination conducts electrochemically — it needs bias to move ions and
grow filaments. At 0.3 V: open. At 3.3 V: kΩ. The conduction also *drifts*
under sustained DC (watched live: 0.2–1.0 V wandering, pinning near 0, then
recovering — a fixed resistor cannot do that; ionic residue does).

**Root cause:** the flux was AIM **Nitro Flux — "a plumbing solder paste
flux"** per its own TDS, ASTM B-813 (copper *pipe* flux, water-flushable,
ionic by design). It is not an electronics flux. Every joint made with it
carries corrosive, hygroscopic residue that conducts under bias and keeps
corroding. IPA alone made it *worse* — it dissolves the binder and spreads
the ionic salts over more pads (observed: coupling count rose after an IPA
wipe).

**The measurement that works — pin-hold + voltmeter:**
`CONFIG_HOMECADIA_BENCH_PIN_HOLD` parks the pins with internal pull-ups on
and one neighbour driven low; a 10 MΩ DMM on DC volts reads the divider:

    V = 3.3 · R / (45k + R)
    3.3 V none · 2.3 V ≈ 100k · 1.65 V ≈ 45k · 0.33 V ≈ 5k · 0.03 V ≈ 450R

(The C6 pull-up is specified 10–80 kΩ — read order of magnitude, not value.)

**The clean that works — two stages, matching the two-component residue:**
1. IPA flood + brush, *drained off the board edge* (never wiped in place),
   to strip the rosin/binder.
2. Hot water + drop of dish soap, brush, then a distilled-water rinse, to
   dissolve the ionic salts (B-813 flux is water-flushable by design).
3. Dry hard: shake out socket bores, hair dryer low 10 min, an hour warm.

Verified on board #3: pre-wash 0.2–1.0 V drifting on D9; post-wash flat
3.1 V through every phase, and the first fully clean coupling scan ever
recorded on the assembled stack.

**Decoder rows:**
- *Uniform kΩ between many adjacent pins, resistance growing with distance*
  = a conductive sheet or joint contamination, not a circuit.
- *Coupling that only registers in one direction* = the follower's other
  loads (panel input leakage on MOSI/SCK) moved its threshold — not a diode.
- *Resistance that drifts under steady DC bias* = ionic/electrochemical, and
  no amount of solvent-wiping fixes it; wash the chemistry it matches.

**Standing rule:** electronics joints get rosin-core wire or a no-clean
electronics flux (J-STD-004). The plumbing paste stays with the plumbing.
After soldering headers: two-stage clean, dry, and one bench boot — the scan
must print `coupling scan: no pin follows any other` before anything else is
attached. New parts need that one 30-second scan, not days.

## 18. Never inject from a stiff source into a live GPIO

**2026-08-31.** XIAO #1 died measuring the leak. PPK2 source meter at 3.3 V
(1 A capability) clipped to D9 while the board ran — but the boot-time
coupling scan *drives* every pin, so within milliseconds of reset the C6
pulled D9 low against the source: a direct short through an output
transistor rated 40 mA. The PPK2 logged 0.68 A bursts. The smoke came out at
the opposite corner, by D0 — injected current also forward-biases the pin's
ESD clamp into the 3V3 rail, and the regulator (which lives by the USB end)
ate the rest.

Rules, in order of preference:
1. Measure **voltage** with the firmware holding the pins (pin-hold + DMM).
   Nothing external sources current; nothing can be harmed.
2. If current must be injected, put **≥1 kΩ in series** — caps any mishap at
   ~3 mA — and only into a pin the firmware provably holds as an input for
   the whole window, including reboots.
3. PPK2 source-meter mode is for **supply substitution** (battery pads, USB
   out, single supply) — never for signal pins on a powered board.

## 19. Two nodes within 0.2 V are indistinguishable by a one-point check

**2026-09-12.** The battery divider was "verified" on 2026-09-02 by one
reading: meter 1.57 V at the ADC pin, firmware 1.55 V, 1.2% apart. Both
numbers were wrong in the same direction. 1.57 V on a 10 MΩ meter across a
1 MΩ lower leg is 3.30 V at the top of the divider (the meter makes the
ratio 0.476, not 0.5) — while BAT+ was recorded at ~3.1 V the same day. The
mismatch was in the data and was rationalised as ADC tolerance.

The trap: with no cell fitted, BAT+ (charge IC output) and the 3V3 rail both
sit near 3.3 V. A divider on either node reads the same at that one point.
Ten days of "the battery always reads the same" followed.

**What separates them is a sweep, not a better single reading.** With the
PPK2 substituting for the cell (Source Meter, USB out), move the source
3400 → 4000 mV and re-read the node. A node on BAT+ follows; a node on the
rail stays at 3.3 V. Two points, in-circuit, ten minutes. That is now the
standard for any "verified against a meter" row: two source voltages, or it
is not verified.

The same session produced six readings that looked like faults and were
measurement artifacts, each of which triggered an intervention:

| Looked like | Was |
|---|---|
| Two dead packs, 0 V both polarities | probe on the JST housing rim, not the recessed contact |
| Dead pack after charging, 0 V | latched protection board; releases hours after charge starts |
| XIAO won't charge (LED blinking) | charging normally at 106 mA (spec 100 mA) |
| PPK2 reads 0.00 A | leads reversed; the PPK2 cannot read negative current |
| Short on the board (cell to 0 V "fast") | 41 mA sleep-disabled draw over three days, then the latch |
| Source not reaching BAT+ (0.6 V, decaying) | the wire had been unplugged to measure it |

Rules that came out of it, all cheaper than the interventions they replace:

- **Measure in-circuit.** DC volts with a 10 MΩ meter disturbs nothing. A
  wire unplugged to be measured reads the charge trapped on it.
- **One physical change, then measure-only until it is confirmed.** Two
  matching readings before acting on a surprise.
- **The PPK2 is the battery for bring-up.** It cannot latch, drain
  overnight, or be reversed at a JST. A cell goes on for a bounded session
  and comes off after — the bench profile draws ~41 mA and flattens a
  2000 mAh pack in about two days.
- **Read firmware from the glass.** The DIAG view shows `battery_mv`; no
  USB, so no second supply and no charge IC in the loop. It is the last
  poll's value (120 s), so wait a poll after changing the source.

What the sweep then found, once the wiring was trusted: the ADC reports a
constant 340 mV low at battery level (0.17 V at the pin), the same at 3.4 V
and 4.0 V. That is ~0.34 µA of ADC input current across the divider's
500 kΩ source impedance — the known cost of a high-value divider straight
into an ESP32 ADC, and a firmware constant, not a wiring problem.

## 20. An experimental sleep option hung the sensor the first time it really slept

**2026-09-15.** The shipping image had run for 40 minutes on USB without a
fault. On the PPK2 (Source Meter 3700 mV, USB out) it stopped waking 8
minutes after boot and never recovered: 10–33 µA, drifting, not one wake in
two minutes where a healthy sensor polls its Thread parent every 5 s. The
parent dropped it and the controller marked it unavailable.

**USB hides it.** `CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION=y` keeps the C6 out of
automatic light sleep while a USB host is attached. A shipping-profile check
on USB therefore proves nothing about sleep; the first real light-sleep run
here was this one.

Lining the PPK2 recording up with the Matter server log placed the stop at
00:33:31 — two minutes after the previous sensor poll, i.e. at a poll (I2C
read of the SHT40, one ADC read). Everything before it looked normal: 5 ms
radio polls every 5.00 s, a report plus display refresh at 00:31:31, five
seconds of 500 ms fast polls after it.

Suspect: `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y`. ESP-IDF v5.5.5
labels it EXPERIMENTAL and defaults it to n (`components/esp_pm/Kconfig`), but
it is not an oddity of this repo: both of Espressif's C6 sleepy-Thread
references turn it on (esp-matter `examples/icd_app`, IDF
`examples/openthread/ot_sleepy_device/light_sleep`), and the milestone-1
scaffold copied `icd_app`'s defaults nearly line for line. Those references run
on a DevKit-C with nothing attached. This build adds the XIAO — its antenna
switch is powered from GPIO3 and selected by GPIO14, both driven by the app —
plus SPI to the panel, I2C to the SHT40, the encoder GPIOs and an ADC read. So
the finding is "hangs on this build", not "Espressif's reference is broken".
One-variable test: a build with only that option off — the two sdkconfigs diffed to that
single line — ran 38 minutes on the PPK2 with no poll gap over 5.0 s and no
subscription loss.

| Run | Option | On PPK2 | Result |
|---|---|---|---|
| 00:25 | on | ~8 min | stopped waking at a sensor poll |
| 01:04 | off | 38 min | longest poll gap 5.0 s, no subscription loss |

One run each: strong evidence, not proof. The option stays off.

Wrong turns, each ruled out by reading the source before touching anything:

- *The display BUSY wait blocks the poll timer.* No — `display_show_readings`
  only queues; a separate task refreshes.
- *A stuck timer task stops Thread.* No — OpenThread's alarm runs off its own
  mainloop select timeout (`esp_openthread_alarm.c`), not esp_timer callbacks.
- *The SHT40 read waits forever.* No — every I2C call has a 50 ms timeout.
- *~900 "wakes" per second in the idle floor.* They are 50 µs pulses — too
  short to be light-sleep wakes. Origin still unverified.

**Save PPK2 sessions and parse them.** A `.ppk2` file is a zip:
`metadata.json` (`samplesPerSecond`, `startSystemTime` in epoch ms) and
`session.raw`, 6 bytes per sample — float32 current in µA, then a uint16 of
digital channels. With the start time the trace lines up with any log to the
millisecond; that is what placed the stop at a sensor poll.

Side finding: the +340 mV battery offset calibrated on the bench image reads
214–294 mV *high* on the shipping image. ADC error depends on the sleep
profile; calibrate on the image that ships.

Still open with the option off, and the reason the average is 696 µA against
a 300 µA target: every 20–40 s a 30 ms transmission drops the sensor into ICD
active mode for the 5 s threshold (500 ms fast polls). A bounded capture on the
border router's `wpan0` (Python raw socket inside the OTBR container — the node
has no tcpdump; 150 s, addresses and ports only) saw one Matter exchange in
that window, the 2 min keepalive. So the episodes never leave the link to the
parent, which reports a 20% frame error rate to the sensor at −77 dBm
(`ot-ctl meshdiag childtable 0xa800`). Espressif's SIT table uses a 1 s
threshold; ours is 5 s, inherited from their LIT table while the device runs as
SIT (no check-in client registered, `0/70/8` = 0).


## 21. The battery ADC kept the radio powered through every sleep

**2026-09-15.** With the hang fixed (§20) the shipping image still averaged
~700 µA against a 300 µA target, and the quiet stretches between radio polls
sat near 376 µA. Espressif's C6 DevKit-C trace for the same stack
(esp-matter `examples/icd_app/README.md`, image `C6-sit-icd.png`) reads
32–71 µA there.

**The trace looked like a wake storm. It was the regulator.** Between polls
98% of the charge arrived in ~2340 short pulses per second (they come in
pairs, ~1170 Hz; ~50 µs, 1–14 mA each) with 3–7 µA between them, and the rate
slid ~10% over the 5 s between polls, jumping back at each poll. That is the
XIAO's 3.3 V supply: U1 is an SGM6029 buck (XIAO-ESP32-C6_v1.0 schematic,
sheet 4; L1 0.47 µH). Its VSEL/MODE pin has 249 kΩ to GND, which the datasheet
(Table 1) reads at startup as 3.3 V; after startup a low on the same pin
selects power-save mode. In power-save mode a buck draws from the battery in
bursts whose rate follows the load. Quiescent current is 2.3 µA typ
(datasheet), which with the divider's ~1.9 µA is the 3–7 µA between bursts.
The average is real; the pulse shape only says how big the load is.

**Instrument before guessing.** A diagnostic image
(`CONFIG_HOMECADIA_SLEEP_DIAG`, `main/sleep_diag.cpp`; how to build it is in
[build.md](build.md)) counts every automatic light-sleep request through the
esp_pm light-sleep callbacks, records the power-down flags IDF applied to each
sleep (`CONFIG_ESP_SLEEP_DEBUG` sleep context, bits from
`esp_private/esp_pmu.h`), and redraws the lot on the DIAG screen every 60 s.
It is read from the glass: a USB host keeps the chip awake.

| Measured (PPK2 3700 mV, USB out, 300–1020 s uptime) | Value |
|---|---|
| Light sleeps | ~2/s, all ≥3 ms, all ended by the timer; no GPIO wakes |
| Time asleep | 98–99% |
| Sleep time with the modem power domain on | **100%** |
| Sleep time with the 40 MHz XTAL on | 0% |
| BLE controller / NimBLE host task | idle / gone (torn down after commissioning) |

Ruled out on the way, each by source or by the counters:

- *A wake storm* — 2 wakes/s.
- *The USB-Serial-JTAG console* (ours; `icd_app` uses the UART) — the C6 does
  not define `SOC_USB_SERIAL_JTAG_SUPPORT_LIGHT_SLEEP` (soc_caps.h:354), so IDF
  disables the USJ pad and clock on every light sleep
  (`sleep_modes.c` `misc_modules_sleep_prepare`).
- *The panel left awake* — `ssd1680.c` sends deep sleep (mode 1) after every
  refresh.
- *GPIO3 held low into sleep.* 3.3 V across Q3's 10 kΩ gate pull-up is 330 µA,
  which fit the number exactly. Its IO_MUX register read `0x1802`: sleep select
  on, sleep output, pulls and input off. IDF already isolates it.
- *BLE keeping the XTAL and modem up* (`CONFIG_BT_LE_LP_CLK_SRC_MAIN_XTAL=y`)
  — controller idle, task gone, XTAL on 0% of sleep time.

**Cause.** `battery.cpp` created its oneshot ADC unit once in `battery_init()`
and never deleted it. On the C6 the ADC front end sits in the modem power
domain (`ADC_LL_ADC_FE_ON_MODEM_DOMAIN`, `hal/esp32c6/include/hal/adc_ll.h:41`),
so `adc_oneshot_new_unit()` calls
`esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ESP_PD_OPTION_ON)` and only
`adc_oneshot_del_unit()` sets it back to OFF (`esp_adc/adc_oneshot.c`, IDF
v5.5.5). IDF powers the modem domain down only when that option is not ON
(`sleep_modes.c`, the `RTC_SLEEP_PD_MODEM` condition). With
`CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y` the same call also pins the
TOP domain on. Espressif's reference has no ADC. The fix creates and deletes the unit around each reading (every
120 s); the calibration handle is eFuse coefficients only and is kept.

**Measured the same day, diagnostic image with the fix:** modem domain on for
0% of sleep time (was 100%); last sleep's flags `0x20003c16`, MODEM bit set.
Quiet floor over a 1.3 s window with nothing above the buck's 13 mA bursts:
**282 µA**, against 376 µA before with the same config (PPK2 3700 mV). The fix
is worth ~95 µA: real, but a third of the excess, not the ~300 µA it was
expected to be. Still powered in every sleep: the TOP domain and the HP
peripherals (flag bits 0 and 3 never set). That is what
`CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=n` (§20) leaves on, and
Espressif's reference runs with that option on. Before the fix the held ADC
unit also pinned TOP on in the option-on build, so no run here has ever
powered TOP down, including the one that hung. Next test: option on plus the
fix, watching for bit 0 and for the hang.

Also found in the same session:

- **The dial does not work on battery.** The encoder uses edge interrupts
  (`components/ec11_encoder/ec11.c`), which cannot wake the chip from light
  sleep (`gpio_wakeup_enable` accepts level types only, `esp_driver_gpio`
  `gpio.c`), and `CONFIG_PM_SLP_DISABLE_GPIO` disables every pin's input while
  asleep (`sleep_gpio.c` `esp_sleep_config_gpio_isolate`). Every dial test so
  far ran on USB, where the chip never sleeps. Open.
- **The CLOCK_SYSTEM retention module is inited but never created** (retention
  bitmaps `0x60000006` inited, `0x60000004` created). `top_domain_pd_allowed()`
  requires the clock domain to be allowed to power down, so this probably
  blocks TOP power-down whatever the peripheral option says — unverified.
- **One-minute PPK2 averages swing ±100 µA with radio traffic** (827 vs 645 µA
  on identical behaviour). Compare quiet windows or long averages.
- **Battery offset on the shipping image:** 3400 → 3676, 3700 → 3988,
  4000 → 4288 mV with the bench image's +340 mV applied — a constant +276 to
  +288 mV, slope 1.02. The fix changes the ADC's power state between
  readings, so it is re-swept after the fix rather than patched now.
**Later the same day: the TOP domain needs two driver flags, and the option
still hangs.** With the ADC fixed and
`CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y` back on, the TOP domain
*still* never powered down (flag bit 0 never set, floor unchanged at 279 µA).
The retention bitmap said why: I2C0 (bit 12) and GPSPI2 (bit 22) were inited
but never created. IDF's I2C and SPI drivers only build their sleep-retention
entries when the bus asks for it — `i2c_master_bus_config_t.flags.allow_pd`
(`driver/i2c_master.h`) and `SPICOMMON_BUSFLAG_SLP_ALLOW_PD`
(`driver/spi_common.h`, gate in `spi_common.c`) — and `peripheral_domain_pd_allowed()`
refuses TOP power-down until every inited module is created. Our SHT40 bus
(`components/sht40/sht40.c`) and panel bus (`components/ssd1680/ssd1680.c`)
set neither.

With both flags set and the option on:

| | |
|---|---|
| Last sleep's flags | `0x20007c17` — TOP (bit 0) and LP_PERIPH (bit 14) now powered down |
| Retention inited / created | `6040133e` / `6040133e` — everything created |
| Quiet floor | **56 µA** (106 ms selection), against Espressif's 55 µA reference |
| Sleeps ending early | ~2/s, still timer wakes; IDF appears to wake early to cover the retention restore (inferred) |

**And the §20 hang came back.** Last report 478 s after boot (matter-server:
subscription timeout at 16:48:04 less its 2 min 38 s), screen frozen at
`up 480s` with 882 sleeps counted, `is offline` 16:47:35, unavailable
16:50:35. §20's hang was ~490 s. Both stopped at the **4th** 120 s sensor
poll, not the first, and nothing in `poll_cb()` (`sensor_loop.cpp`) is
every-4th-poll: each poll does the same SHT40 read, battery read and
conditional report. Unexplained.

So the floor is a choice between two known states, until the hang is
understood:

| Shipping option | Quiet floor | Runs |
|---|---|---|
| `PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=n` (current) | 282 µA | 38 min clean (§20), plus several runs today |
| `=y` with both driver flags | 56 µA | hangs at ~480 s, twice |

The driver flags stay in regardless: they cost nothing with the option off,
and they are required the moment it goes on.

**Cause found, same evening: a light sleep inside the ADC sequence.** Bisected
with `CONFIG_HOMECADIA_SLEEP_DIAG`'s poll variants, each run on the PPK2 with
the option on:

| Poll variant | Result |
|---|---|
| Full poll | hangs ~480 s (twice) |
| No poll at all | 1490 s clean |
| Poll without the battery ADC read | 1176 s clean (SHT40, report and refresh all running) |
| Full poll, ADC sequence under an `ESP_PM_NO_LIGHT_SLEEP` lock | **1231 s clean** |

`adc_oneshot_new_unit()` changes the sleep power-domain configuration (modem,
and TOP when peripheral power-down is on) and `adc_oneshot_del_unit()` changes
it back. Nothing stopped a light sleep landing between those two calls, and a
sleep that started mid-change never woke. That also explains the "4th poll"
pattern that looked like a counter: the window is a few ms per 120 s poll, so
which poll loses the race is chance. IDF has no ADC sleep retention yet
(`esp_adc/adc_oneshot.c`, `TODO: IDF-8475`), which is why it pins domains
instead.

Fix: `battery.cpp` holds an `ESP_PM_NO_LIGHT_SLEEP` lock across create, read
and delete, ~10 ms per poll. With it, `CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_
LIGHT_SLEEP=y` is back on in `sdkconfig.defaults` and the numbers are:

| | Before today | Now |
|---|---|---|
| Quiet floor | 376 µA | **52 µA** (4.1 s selection; Espressif's DevKit reference is 55) |
| All-in average | 645–827 µA | **230 µA** over 74 s including a heavy radio burst; 107 µA over a quiet 9 s |

That is ~10 months on 1700 mAh at the pessimistic figure, against 3.3 months
this morning, and it is the first number inside the 300 µA budget.

**Flashing a power-down-on image wedged the USB port.** The first plug-in
after that image ran gave Windows `Unknown USB Device (Device Descriptor
Request Failed)` and esptool could not open the port. Holding the **B** button
while plugging in (ROM download mode never sleeps) flashed first try. Six
plug-ins on option-off images had been fine.

- **The first `get_node` after a reflash can be the server's cache.** The
  interview returned and the attributes still showed the previous boot's
  reboot count and uptime; the next read 25 s later was current. Check
  `0/51/1` (reboot count) moved before trusting anything else.

## 22. After the sleep floor, the radio is the budget

**When:** 2026-09-16, the first overnight soak of the shipping image
(`0.6.0-dev.84+e0760b0`, both §21 fixes,
`CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP=y`).

**Run:** PPK2 Source Meter 3700 mV, USB out, 15:18 → 22:56. The PC lost power
before the intended start the previous evening, so the run began the next
afternoon. Node 25 answered a fresh `interview_node` at +85 min, +7.7 h and
after the run; no hang (the §20/§21 hang reproduced at ~480 s).

| Selection | Average | Charge | Note |
|---|---|---|---|
| Whole run, 7:38:36 | **288.5 µA** | 7.94 C | includes boot, Thread attach, five interviews |
| One 120 s report cycle at +5 h 31 min | **240.6 µA** | 28.87 mC | the settled number |
| 1.79 s between polls | 59 µA | 105.8 µC | matches the 52 µA §21 floor |

**What the 240 µA is.** The 2-minute window shows three things: a flat
59 µA floor; 22 evenly spaced ~250 mA spikes 5.3 s apart; and a cluster of
~10 spikes 0.5 s apart right after the report. Per spike ~0.58 mC (from a
1-minute window: 9.54 mC minus 60 s of floor, over 11 spikes). So:

| | per 120 s | µA |
|---|---|---|
| floor | 7.1 mC | 59 |
| slow polls, `CONFIG_ICD_SLOW_POLL_INTERVAL_MS=5000` (SDK default, never set by us) | ~12.8 mC | ~107 |
| fast-poll tail, `CONFIG_ICD_FAST_POLL_INTERVAL_MS=500` for `ACTIVE_MODE_THRESHOLD_MS=5000` after the report | ~5.8 mC | ~48 |
| sensor read + report | ~3 mC | ~25 |

The Thread parent polls cost twice what the chip does asleep. Yesterday's
"107 µA over a quiet 9 s" was a window with no report in it; a 1-minute window
today read 159 µA for the same reason. **Only a window that spans a full
report cycle is a settled average**; anything shorter is a floor-plus-luck
number.

**What changed.** `CONFIG_ICD_SLOW_POLL_INTERVAL_MS=30000` in
`sdkconfig.defaults`. Two things verified in the SDK first:

- The active-mode threshold stays at 5000. It is the LIT spec minimum;
  1000 trips `ICDManager.cpp:82` and boot-loops (already recorded in
  `sdkconfig.defaults`). The fast tail is not recoverable by config.
- A LIT-capable device with no registered ICD client operates as SIT and
  uses `min(LIT interval, SIT interval)` with the SIT interval capped at
  `CHIP_DEVICE_CONFIG_ICD_SIT_SLOW_POLL_LIMIT` = 15 s
  (`ICDConfigurationData.cpp:25-40`, `CHIPDeviceConfig.h:159`, release/v1.6).
  So the poll spacing in the next trace tells whether the matter-server
  registered as a client: 30 s means LIT, 15 s means SIT. Either is a cut.

Expected: ~107 µA → ~18 µA (30 s) or ~36 µA (15 s), settled cycle 240 →
~150–170 µA. Cost: a controller-initiated command waits up to one interval;
subscriptions and the 120 s report are unaffected.

**Result, 2026-09-17 (12 h 37 min, 23:26 → 12:03, no hang).**

| | 5 s poll (09-16) | 30 s configured (09-17) |
|---|---|---|
| Poll spacing in the trace | 5.3 s | **15 s** — SIT clamp, no ICD client registered |
| Whole run | 288.5 µA over 7.6 h, five interviews | **237.7 µA** over 12.6 h, three interviews |
| One settled 120 s cycle | 240.6 µA, 28.87 mC | **207.8 µA**, 24.94 mC |

The cycle saved 3.9 mC, about half of what the poll count alone predicted
(22 → 7 polls ≈ 8.5 mC). The other half went into the active period around
the report, which grew from ~8.8 mC to ~13.6 mC: about twelve spikes reaching
390 mA where the 5 s trace had about ten at 250 mA. Cause not established.
Per cycle now: floor ~59 µA, polls ~35 µA, report active period ~113 µA. The
next lever is how often that active period happens
(`CONFIG_ICD_IDLE_MODE_INTERVAL_SEC` and the 120 s sensor poll), not the polls.

Two measurement traps from this run:

- **A window taken ten minutes after boot read 309 µA** with a cluster of
  twelve 300–400 mA spikes, and looked like the change had made things worse.
  The same image at +12 h read 208 µA. The matter-server re-subscribes and
  re-reads after a node returns; nothing measured in the first half hour is a
  settled number.
- **One `interview_node` costs ~150 mC** — a 2:00 window containing one read
  1.26 mA against 0.21 mA without. It reads every attribute on the node. Three
  of them are 0.45 C of this run's 10.79 C; the five in the 09-16 run were
  ~10 % of its total. A liveness check is not free on a device budgeted in
  microamps; use the PPK2 trace (polls still arriving) as the liveness signal
  instead.

## 23. A meter on a high-impedance node is a load, not a reference

**When:** 2026-09-22, calibrating the battery ADC on the shipping image.

With the PPK2 at 3.70 V the meter read BAT+ 3.67 V and the divider midpoint
1.73 V, a ratio of 0.471 for a 1M:1M divider. I took 0.471 as the divider's
real ratio, compared the firmware's pin voltage against 1.73 V, found them
within 13 mV, and concluded the offset should be 0. Flashed, and the reading
came out 64–144 mV **low**.

The midpoint has a 500 kΩ source impedance. The meter's 10 MΩ input is in
parallel with the bottom leg and pulls the node down ~5 %: 0.5 × 10/10.5 =
0.476, which is what it read (0.471 with tolerance). The ADC's own input does
the same thing, less strongly (~1.9 %). Comparing one loaded reading against
another loaded reading measured nothing.

The fix was to calibrate only against **BAT+**, which is driven by the PPK2
and has near-zero impedance, so the meter cannot move it. Two points, BAT+
3.68 and 4.00 V, gave the same gain (×1.0176, ×1.0201) and no offset, which
is what input loading should produce. A third point the fit never saw, 3.39 V,
read 3389 and 3387.

Rule: **calibrate an ADC chain end to end, from a low-impedance source to the
number the firmware reports. Never use a meter reading of a node with more
than ~10 kΩ source impedance as the reference.** A meter on such a node is
fine for "is it roughly there", not for millivolts.

Two other things this sweep showed:

- **One Matter report is not a measurement.** At 8 samples after a 5 ms
  settle, reports at a fixed voltage scattered ±120 mV; a single report
  looked like an offset change twice. 64 samples after a 20 ms settle (the
  500 kΩ ‖ 100 nF node has a 50 ms time constant) brought it to 4 mV. Take
  several reports per point and hold each point for ≥ 3 report periods; one
  slider move inside a 120 s poll produced a mixed reading.
- **The "dead node" of 2026-09-22 was a broken hookup wire.** The PPK2 read
  1.84 µA at 3.7 V, exactly 3.7 V over the 2 MΩ divider: the divider was the
  only load, so the XIAO was not on the node. Arithmetic on a flat current
  identifies what is connected faster than any probe.

## 24. The border router forgot the network; the node never did

**When:** 2026-09-21 20:19 → 2026-09-22 21:30.

The Kubernetes node hosting the OTBR rebooted. The OTBR pod came back on
OpenThread factory defaults (PAN ID `0xffff`, channel 11, Thread disabled):
its settings file lives in `/var/lib/thread`, and the persistent volume had
been mounted at `/data` since 2026-08-19, a path the image's entrypoint never
links. Fixed in the homelab repo (PR #373: mount at `/var/lib/thread`); the
dataset was restored from Home Assistant's store.

What that looks like from this project's side:

- matter-server logs `ENETUNREACH` to the node's old mesh-local address every
  2 min, and the HA cache keeps serving the last values, so a `get` read
  looks healthy with stale numbers. Check `last_interview` and the OTBR
  (`ot-ctl state`) before blaming the device.
- The off-mesh-routable prefix is regenerated when the OTBR loses its
  settings (`fd20:22ab:77:1::/64` → `fd98:ce54:c5cd:1::/64`). Nodes re-register
  with SRP on the new prefix by themselves; no re-commissioning. The node's
  own Thread credentials are in its NVS and survived untouched.
- `srp server host` stayed empty for an hour after the OTBR recovered. That
  was not the prefix: node 25 was unpowered (§23, broken hookup wire). Once
  powered it attached, registered and was re-interviewed by matter-server
  within two minutes, `RebootCount` +1.

**2026-09-28, the fix held.** The same node (`k8sn3-agent`) went down twice
in one afternoon. Both times the OTBR pod came back attached as a router on
channel 15 with no restore, and node 26 rejoined by itself: `RebootCount` did
not move across the first outage, and the controller resubscribed within
about a minute of the matter server returning (16:27:24).

## 25. Three faults, none where the symptoms pointed

**When:** 2026-09-24 → 2026-09-28, XIAO #3 then #4.

The dial did nothing on a freshly soldered XIAO. Four days of symptoms:
D6 at 0 V with nothing attached, every encoder wire reading open, the board
vanishing from USB whenever the knob turned, the press never registering.
The causes, in the order they were found:

| Symptom | Cause | How it was found |
|---|---|---|
| 0 V on D6, D5, the 3V3 pin and the 5V pin; no beep on any encoder wire | **meter probe lead loose in its jack** | the 5V pin read 0 V while the board was enumerated on USB, which no board fault can produce |
| board drops off USB on the first detent; encoder A never changes in the log | **encoder wire soldered to the 3V3 pad, which sits beside MTCK** | MTCK wire beeped to the 3V3 header pin |
| press never registers | **wire loose in the D9 breadboard row** | found by hand after rotation worked |

Wrong turns on the way, each of which was written up as a conclusion before
it was tested:

- **Flux residue on D6.** The 0 V readings matched §17 exactly (0 V under
  bias, open unpowered), and a pin swap in firmware was planned around a
  "dead" GPIO16. The flux was a no-clean pen and the pin was fine: 3.27 V
  once the meter worked.
- **Encoder wires not reaching their pins.** Both beep tests "failed" on
  wires that were connected. A loose probe cannot beep.
- **MTCK bridged to BAT+.** Proposed from the board dying when contact A
  closed. One voltage reading (3.26 V on the wire, 4.04 V on BAT+) killed it.
- **Wire tugging.** The reset followed the knob, so handling was blamed. The
  log showed the last line before the drop was the first contact closure.

What decided it each time was a measurement that could only come out one way:

- **A reading the fault cannot explain identifies the instrument.** A board
  that is enumerated on USB has 5 V on its 5V pin. Check the meter against a
  known source before believing a run of zeros.
- **The chip's own reads stayed right throughout.** `encoder raw` lines and
  the harness verdicts come from the pins, not the meter. When the two
  disagree about a powered board, the firmware is the better witness.
- **The harness said `held HIGH` for GPIO6 on the miswired board.** The probe
  applies a pull-down, so a free pad is expected to follow it. The note in
  `bench_selftest.cpp` explained the reading away as the JTAG pull-up. What a
  correctly wired MTCK reads is not recorded, so this stays a hint, not a
  verdict; turning the knob is the test.
- **Isolate by removing the other signal.** With B pulled from its row and
  only A connected, the first detent still killed the board and no line
  reached the log. That put the fault on A with nothing inferred.
- **An ePaper panel is not a liveness signal.** It kept its image through
  every power loss. The COM port was the only indicator.

Process changes, now in [assembly.md](assembly.md):

- Test before glue. The glue hid which pad each wire was on, and XIAO #3
  lost its pads in the handling that followed the glue's removal.
- Identify pads by exclusion, unpowered: beep each signal wire against 3V3,
  5V and GND, and against GND with the B and R buttons held (BOOT and EN).
  What is left can be corrected in `app_config.h`.
- Prove them powered: the battery reading proves MTMS and BAT+, three
  detents each way prove MTCK.

## 26. An unpaired unit does not sleep

**When:** 2026-09-28, XIAO #4, shipping image, PPK2 3700 mV, USB out.

Before commissioning: **28.1 mA average** over 10 s, baseline 20–40 mA,
~200 mA spikes every 0.5 s, matching the 500 ms advertisement interval the
firmware logs after its first 30 s. `CONFIG_PM_ENABLE` and
`CONFIG_BT_LE_SLEEP_ENABLE` are both set, so the build is able to sleep.
Three minutes after commissioning, same unit, same source: 91 µA over a quiet
10 s.

**Cause: not known.** The explanation written here on 2026-09-29 was wrong
and is kept below, struck, because the mistake is instructive: ESP-IDF's
OpenThread port creates a power-management lock named
`ot_sleep` and takes it at init (`components/openthread/src/port/
esp_openthread_sleep.c`, `esp_openthread_sleep_init()`). It gives the lock
back only when the 802.15.4 radio reports the state
`ESP_IEEE802154_RADIO_SLEEP` (`esp_openthread_sleep_process()`, same file). A
unit with no Thread network never starts the Thread interface, so nothing
puts the radio into that state and the lock stays held. Any held lock blocks
light sleep. Bluetooth is not the cause by this reading: its controller has
its own lock and releases it between advertisements. No lock in this
project's own code is tied to pairing state (`battery.cpp`, `ui.cpp` and
`ec11.c` hold theirs for milliseconds to seconds). To confirm: run the sleep
diagnostic image (`CONFIG_HOMECADIA_SLEEP_DIAG`) on an unpaired unit and read
which locks are held.

**Retracted 2026-10-02:** that lock is created as `ESP_PM_APB_FREQ_MAX`,
which only keeps the bus clock up while awake; it does not block light
sleep (`ESP_PM_NO_LIGHT_SLEEP` does). The USB console of an unpaired unit
also drops about 30 s after boot, when Bluetooth moves to slow advertising,
which suggests it does light-sleep from then on. Measured since: with the
window closed but before deep sleep, the unit draws a few mA; it is the
window's advertising that costs ~28 mA (29.2 mA, 2026-10-01).

**Decision 2026-09-29, done 2026-10-02:** an unpaired unit goes to deep
sleep once its window has closed, and a turn of the dial brings it back with
a new window. 19.7 µA asleep; how it got there is §27.

Two consequences:

- A dial test on an unpaired unit proves the wiring and nothing about wake
  from sleep. The first "works on battery" result was exactly that and was
  withdrawn. The valid test is a detent on a paired, settled unit, with the
  trace showing a flat baseline before and after
  ([bringup.md](bringup.md), Encoder & LED).
- At 28 mA a 2000 mAh cell lasts about three days. Pair a unit before it
  goes on a cell.

## 27. Deep sleep from a running Matter device: four designs, one that works

**When:** 2026-09-29 → 2026-10-02, XIAO #4. Goal: an unpaired unit asleep,
woken by the dial.

| Design | On USB | On battery |
|---|---|---|
| 1. `esp_deep_sleep_start()` from the running system | slept; the boot after a wake **hung for 25 h** (task watchdog, `main` stuck in GPIO interrupt setup) | not tried |
| 2. same, light sleep off (debug image) | slept, woke on the dial | not tried |
| 3. same, shipping config, wake pin taken out of sleep isolation, clean restart after a wake | slept 12 min untouched; dial woke it 7 of 8 | **stuck at ~20 mA**, never reached sleep, deaf to the dial |
| 4. **restart first, sleep from the top of the next boot** | 5 of 5 | **19.7 µA, 5 of 5** |

What each failure turned out to be, as far as it was established:

- **Woke by itself within seconds** (design 1 on the shipping config):
  `CONFIG_PM_SLP_DISABLE_GPIO` sets every pin to an isolated, floating state
  in sleep. The wake pin floated to its wake level. Fixed by taking the wake
  pin out of the sleep switch and setting its pull-up explicitly.
- **Hung boot after a wake:** the woken boot inherits the wake pin's pad
  hold and LP wake setting. Fixed by releasing both and taking one ordinary
  software restart before anything else runs.
- **Stuck at ~20 mA on battery only:** ESP-IDF's sleep docs require the
  radios to be stopped before deep sleep (`sleep_modes.rst`), and the
  deep-sleep entry runs PHY shutdown hooks with interrupts off. On USB the
  connection blocks light sleep, so the radio block is never powered down
  between advertisements; on battery it is. The best-supported reading is
  that entry stalls on a powered-down radio. Not proven; design 4 removes
  the question by sleeping before any radio exists.
- **Awake indefinitely after a failed pairing:** the SDK re-opens the window
  for a retry with no timeout of its own (`CommissioningWindowManager::
  HandleFailedAttempt`). Fixed by an uptime cap: 16 min after boot the unit
  sleeps whatever the window says, unless a pairing holds the fail-safe.

**The 6,100 restarts.** During a failed phone pairing on 2026-10-02 the
unit restarted about 6,100 times in 54 minutes (`RebootCount` 30 → 6131),
which the PPK2 showed as a steady 67 mA with a spike every ~0.6 s. It was
first read as "awake" and then blamed on the firmware. Two deliberate
reproductions on USB, one with the window closing mid-pairing, paired
cleanly. The next restart the device reported, on 2026-10-03, gave
`bootReason: 2`, a **brown-out reset**. The rate (~0.5 s) matches the
battery-path episode of 2026-09-28 ([battery-runbook.md](battery-runbook.md)
B3): a supply through breadboard contacts sags under the radio's bursts.
The PPK2 measures at its own terminals and cannot see that sag.

What the debugging tools did, because each one misled at least once:

- **The USB console is not evidence of anything on the shipping config.**
  The port stays listed while the chip sleeps and every open fails; it also
  drops ~30 s into an unpaired boot. A "port vanished" was read as deep
  sleep when it was light sleep. Captures need a listener that reopens a
  dead handle (the first one held it and recorded nothing three times).
- **The panel is not a liveness signal** (§15). A test build drew
  `rst<reason> wakes<count> cause<cause>` on the onboarding screen instead;
  that line answered questions the console could not.
- **The PPK2 needs the right zoom.** At 1-minute zoom a buck converter's
  ~1 kHz pulse train looks like radio activity; at 1-second zoom it is a
  flat 20 mA. Read the 10 s average, then zoom to the event.
- **`bootReason` in the controller** names the cause of the last restart
  and costs nothing to read. It settled the restart loop after two days of
  inference.
- **A build directory can keep an sdkconfig from before new Kconfig
  options existed** and compile a feature out without an error. The profile
  check now asserts the option is on in shipping.


## 28. A level interrupt on a pin that light sleep isolates

**When:** 2026-10-04 → 2026-10-05, XIAO #4 (node 28), PPK2 at 3700 mV.

**Symptom.** Paired, settled, the sleep floor was 73–76 µA against 59 µA in
September, every parent poll cost ~0.76 mC against ~0.54, and the chip made
1 ms wakes with no radio activity at 4–120 a minute. The extra floor charge
arrived as ~10 mA × 50 µs pulses, ~120 a second on a ~3.1 ms grid.

**Ruled out first, from the captures and without touching the board:**

- the measurement method (the same script gives September's capture 59 µA);
- longer BAT leads (series resistance adds no current; leakage would show in
  the between-pulse current, which went *down*);
- the encoder A pull-up against a closed contact (floor flat across detents);
- the dial's GPIO wake keeping `RTC_PERIPH` on (`sleep_modes.c:2697-2699`):
  `CONFIG_HOMECADIA_DIAG_DIAL_WAKE_DELAY_S` A/B in one capture, ≤3 µA;
- Thread reattachment (diagnostics counters: no attach attempt or parent
  change in 2 h).

**Bisect, app-only flashes on the same board:**

| Image | Floor | Charge per poll | Stray wakes |
|---|---|---|---|
| `f798d25` (2026-09-22) | 49 µA | 457 µC | 0.2/min |
| `07457a1` (2026-09-23, switch on MTDO/GPIO7) | 45 µA | 446 µC | 0.6/min |
| `58a3cb5` (switch on D9/GPIO20 since `7d25512`) | 73 µA | 745 µC | 4–120/min |
| `58a3cb5` + fix | 57 µA | 478 µC | 1 in 49 min |

The resolved sdkconfigs of the first and third differ only in ICD settings,
`CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS` (deep-sleep wake pins only,
`esp_hw_support/Kconfig:207-212`) and `CONFIG_MDNS_ENABLE_BROWSE`. Between
`07457a1` and the regression, only `7d25512` touches the paired sleep path:
the push switch moved from LP GPIO7 to HP GPIO20, still on a
`GPIO_INTR_LOW_LEVEL` interrupt.

**Cause, as far as it is established.** A wake-source pin keeps its pad
config in light sleep; any other pin is isolated by
`CONFIG_PM_SLP_DISABLE_GPIO` (input and pull off), and an isolated input reads
low — the level the switch interrupt is armed for. Keeping GPIO20's awake
config through sleep (`gpio_sleep_sel_dis()`, `ui.cpp` `push_switch_init()`)
removes all three symptoms; the press still registers. How the false level
turned into the pulse grid and the heavier polls was not traced (inferred:
the interrupt fires on each wake and runs the switch task's 20 ms debounce).

**Result:** 185 → 125 µA settled (power-budget.md). ~10 µA of floor above
`07457a1` remains, unexplained; the floor also drifted 50 → 58 µA within one
50 min capture.

**Rules:**

- Any HP GPIO with a level interrupt needs `gpio_sleep_sel_dis()` (or an
  edge interrupt) when `CONFIG_PM_SLP_DISABLE_GPIO` is on.
- Moving a pin between LP and HP GPIOs changes its sleep behaviour; re-measure
  the floor after any pin move, not only after "power" changes.
- An app-only bisect on the same board separates firmware from hardware in
  one flash; keep the captures and the bisect images.

## 29. The bottom of the battery: warning, brown-out, empty

**Setup.** 2026-10-05/06, node 28 (XIAO #4) paired, PPK2 Source Meter on the
BAT wires, USB out, the supply stepped down by hand. Readings are the
`3/47/11` reports through the Matter server (`tools/matter-node.py`).

| PPK2 (mV) | Reported (mV) | % | What happened |
|---|---|---|---|
| 3700 | 3690 | 40 | |
| 3553 | 3564 | 14 | |
| 3500 | 3511 | 8 | low-battery mode on (below `LOW_BATTERY_PCT` 10, i.e. under 3530 mV) |
| 3400 | 3401 | 2 | |
| 3300 | 3297 | 1 | no report from the % change: 1 % hysteresis needs a 2-point drop |
| 3100 | 3099 | 0 | (forced report) |
| 3000 | — | — | runs |
| 2900 | — | — | old firmware: `RebootCount` 141 → 639 in ~10 min, panel redrawing |

The reading is within ±11 mV of the source from 3.7 V down to 3.1 V.

**Three defects, fixed:**

1. *The low-battery LED pulse was two ~2.5 ms flashes.* The chip
   light-sleeps inside the 100 ms pulse; with peripheral power-down the GPIO
   domain goes down with it. `gpio_sleep_sel_dis()` (the §28 fix) did not
   help — capture `ppk2-20261006T015052`: wake pairs 100 ms apart every 10 s,
   under 2 mA between them. The pad hold does: `gpio_hold_en()` keeps a level
   through "peripheral power-down in Light-sleep" (`driver/gpio.h`, IDF
   v5.5.5). `led.cpp` `set_led()` re-latches it on every change; now a single
   full flash. Between the wakes the capture shows ~51 µA, not the ~4 mA a
   330 Ω LED should take — unexplained, visibly lit.
2. *Below 2 % Home Assistant stopped seeing the battery fall.* Below
   `LOW_BATTERY_PCT`, any 1-point drop now reports (`sensor_loop.cpp`).
3. *A flat cell restarts the unit about once a second*, radio and panel each
   time — the worst load for a cell at its protection cutoff. A *running*
   unit rode out 2900 mV on the new firmware (uptime unbroken, 2879 mV
   reported); the loop is boots failing, which fits about one per second.
   `empty_battery_on_boot()` (`app_main.cpp`) reads the battery before the
   radio exists: below 3100 mV (3300 mV after a brown-out, or once an empty
   spell has begun) it draws one large-text "BATTERY EMPTY" screen, restarts,
   and deep-sleeps with the dial and an hourly timer as wakes. A running unit
   reading under 3050 mV restarts into the same check (`sensor_loop.cpp`).
   Verified: a cold boot at 2900 mV and a running unit taken to 3000 mV both
   end on the screen and asleep; at 3700 mV a turn of the dial brought the
   unit back paired.

**The empty sleep costs depend on the supply, not the firmware.** Same deep
sleep, never woken: **298 µA at 2900 mV** (captures `…T030820`,
`…T031355`), **21.2 µA at 3700 mV** (`…T031731`, 145 s flat). Drawing,
restarting and then sleeping (the unpaired path's order, §27) made no
difference at 2900 mV. Inferred, not checked against the datasheet: the
XIAO's 3.3 V regulator out of regulation below ~3.3 V input.

**Not settled:** the brown-out level-7 threshold on the 3V3 rail (the BAT
side runs at 2900 mV and fails to boot there); the ~4 µA sleep floor seen
while running at 3500 mV against 50–57 µA at 3700 mV on 2026-10-05 (and
radio peaks of ~650 mA against ~250 mA in the same capture), which may be
the PPK2's ranging rather than the board.

**Rules:**

- Check every timed output with the chip actually light-sleeping; a bench
  build that never sleeps (the 2026-08-25 LED check) proves nothing about it.
- A pin that must hold a level through light sleep with peripheral power-down
  needs the pad hold, not only `gpio_sleep_sel_dis()`.
- Decide "empty" before the radio starts, and give it hysteresis.
