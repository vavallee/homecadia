# Field notes: traps that cost real time

Hard-won during sensor-01 bring-up (2026-08-17 → 23). Ordered by how much time
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
   and Home Assistant grabs it. Disable HA's Matter integration to free the slot.
   The Python/bleak client cannot complete the BTP handshake (see
   matter-js/matterjs-server#1006); use the Node/noble client from
   `docs/ble-proxy-pod.yaml`, which needs two local patches until
   matter-js/matterjs-server#1005 and #1003 land.
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
