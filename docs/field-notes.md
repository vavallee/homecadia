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
