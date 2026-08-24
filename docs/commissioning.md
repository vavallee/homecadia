# Commissioning — sensor-01 → Home Assistant

Verified end to end on 2026-08-23: node 20 on the fabric, attached as a sleepy
child of the ZBT-2, temperature/humidity/battery readable over Thread and
visible in Home Assistant. Commissioning itself took 15 seconds once the three
preconditions below were true; getting them true was the hard part.

## What is deployed

Home Assistant here is a **container on k3s, not HAOS** — there is no add-on
store, so the Matter server and the border router are separate workloads.

| | |
|---|---|
| Matter server | `ghcr.io/matter-js/matterjs-server:0.7.1` (matter.js 0.17.0) — **not** python-matter-server |
| Runs on | the node holding the ZBT-2 (`zbt2` label), `hostNetwork`, port 5580 |
| Key env | `BLE_PROXY=true`, `ENABLE_TEST_NET_DCL=true`, `PRIMARY_INTERFACE` pinned |
| Border router | `otbr`, same node, ZBT-2 passed through as a char device |
| Home Assistant | `ha2-home-assistant` on `k8sn1-master` (the node with the Bluetooth adapter) |
| Thread network | `ha-thread-5df4`, channel 15 |

HA's Matter integration points at `ws://<matter-server-host>:5580/ws`. The
manifests live in the homelab repo under `kubernetes/apps/default/`.

## Three preconditions

Commissioning fails in a different, confusing way for each of these. Check all
three before starting.

### 1. The server must trust test certificates

Self-built devices carry esp-matter's **test DAC**, signed by a PAA that is not
in the production DCL. Without `ENABLE_TEST_NET_DCL=true` the flow runs all the
way through BLE, PASE and attestation and then dies with:

```
Commission failed: PAA not found in trust store for authority key identifier ...
```

The server downloads the test PAA roots either way; the flag is what makes it
*trust* them. Confirm it took effect — the log should show a much larger trust
store on boot (179 certificates rather than 74).

### 2. A working BLE proxy, and only one

The server accepts **exactly one** `/ble` client, and Home Assistant takes it
automatically whenever its Matter integration is enabled.

HA's proxy cannot currently complete the BTP handshake — the server receives the
device's handshake response and then reports that it never arrived
(matter-js/matterjs-server#1006). The Node/noble client works. So:

1. **Disable** HA's Matter integration (Settings → Devices & Services → Matter →
   ⋮ → Disable) to free the slot. Confirm it actually released — it has been
   observed holding the socket after being disabled, in which case restart HA.
2. Start the noble proxy from `ble-proxy-pod.yaml` in this directory. It needs
   two patches to the published client until
   matter-js/matterjs-server#1005 / #1003 land — both are described in that file.
3. Re-enable HA's Matter integration afterwards.

The proxy runs on the node with the Bluetooth adapter, so **the device must be
within BLE range of that node**, not of the border router.

### 3. BLE and Thread must both reach the device at the same time

`connectNetwork` has to succeed inside the fail-safe window, so the device needs
the Bluetooth host *and* a live Thread router in range simultaneously. One
border router in a basement does not cover an upstairs room; a sensor that
commissions fine next to the ZBT-2 will not hold the mesh where it actually
lives. See [field-notes.md](field-notes.md) section 7.

**Check the border router is actually running first.** An OTBR that has lost its
dataset presents as `state: disabled` with an empty child table, and then every
device looks out of range:

```sh
ot-ctl state          # expect leader/router/child, never disabled
ot-ctl ifconfig       # expect up
ot-ctl dataset active | head    # expect a network name
```

Recovery, and how to restore it, is in [field-notes.md](field-notes.md) §6.

## Procedure

1. **Power-cycle the sensor.** The commissioning window is time-limited from
   boot. **The QR staying on the e-ink is not evidence the window is open** — it
   is drawn once and the panel simply holds the image.
2. Confirm it is advertising. From the Bluetooth host, look for service data
   under UUID `fff6`; the payload encodes discriminator 3840 and VID `0xFFF1`.
3. Start the noble BLE proxy pod (precondition 2).
4. Drive commissioning against the server's WebSocket API — **not** the HA UI,
   which gates Matter commissioning behind the mobile companion app:

   ```json
   {"message_id":"1","command":"commission_with_code",
    "args":{"code":"34970112332","network_only":false}}
   ```

   on `ws://<matter-server-host>:5580/ws`. Expect ~15 seconds.
5. Re-enable HA's Matter integration. If the device does not appear, **reload**
   the integration — after a disable/enable cycle HA has been seen reconnecting
   its BLE proxy without re-running node discovery.

## Verify

```sh
ot-ctl child table          # the device should appear as a child, age near 0
```

and read attributes over Thread via `read_attribute`:
`1/1026/0` temperature (centi-°C), `2/1029/0` humidity (centi-%RH),
`3/47/11` battery millivolts. In HA the device appears as `TEST_PRODUCT` with
temperature, humidity, battery, uptime and Thread diagnostic entities.

If temperature and humidity read null while battery reads fine, that is the
esp-matter defect in [field-notes.md](field-notes.md) §9 — the fix is in
`sensor_loop.cpp`; do not go looking for a wiring fault.

## Pairing codes

Until per-device factory partitions are generated (`esp-matter-mfg-tool`, open
work item), all units use the compiled-in Matter **test credentials**: passcode
20202021, discriminator 3840, manual pairing code **34970112332**. The serial
console and the e-ink QR are authoritative if this ever disagrees. Consequence
of shared codes: only put **one** uncommissioned unit in pairing mode at a time.

These are Espressif's public test credentials, not secrets. Real per-device
codes, once generated, must **not** be committed — this repo is public.

## Factory reset / re-commissioning

The encoder is not soldered yet, so the 10-second button hold is not available.
Over USB, erasing the NVS partition wipes the fabric and settings while leaving
the app image intact:

```sh
esptool --chip esp32c6 -p <port> --before default_reset --after hard_reset \
        erase_region 0x10000 0xC000      # nvs, per partitions.csv
```

The device then boots uncommissioned and advertises again. Remove the stale node
from the server with `remove_node` so it does not linger as a ghost, and delete
the corresponding device in HA.

**A plain reflash does not decommission.** The fabric lives in NVS and survives
`write_flash`, so firmware can be iterated without re-pairing — verified.

## Expected warning: uncertified vendor ID

The firmware uses Espressif's **test vendor ID 65521 (0xFFF1)**. It is not
Matter-certified, so controllers show an "uncertified device" warning. Accept
it — for a self-built device on a self-hosted controller this is the normal,
supported path.

## Apple Home: not available here

Apple's Matter commissioning requires an **Apple** Thread border router —
HomePod mini, HomePod (2nd gen), or a Thread-capable Apple TV 4K. Confirmed
2026-08-23 that neither Apple TV in this house qualifies: `AppleTV5,3` is an
Apple TV HD with no 802.15.4 radio, and `AppleTV6,2` is the 2017 Apple TV 4K,
which predates Thread (it arrived with the 2021 2nd gen). The iPhone reports
"Thread Border Router Required" and stops before any BLE traffic.

This also blocks the iOS companion-app route, since it hands Matter
commissioning to Apple's MatterSupport framework — which is why the API path
above exists.

## Troubleshooting

Symptoms actually seen during milestone 2, with what each one meant:

| Symptom | Cause |
|---|---|
| `PAA not found in trust store` | `ENABLE_TEST_NET_DCL` not set |
| `Only one BLE proxy client allowed` | HA holds the `/ble` slot, or an orphaned client does |
| `BTP handshake response not received` | HA's bleak-based proxy; use the noble client |
| `No commissionable device was discovered` | window closed (power-cycle), or out of BLE range of the Bluetooth host |
| Reaches `connectNetwork`, then CASE fails | device out of Thread range, or the OTBR is not running |
| Commissions, then goes unavailable | sleepy ICD — normal between polls; if permanent, check the child table |
| Temperature/humidity null, battery fine | esp-matter defect, [field-notes.md](field-notes.md) §9 |

## What changes when upstream lands

- matter-js/matterjs-server#1005 / #1003 — removes the need to patch the noble
  client by hand.
- matter-js/matterjs-server#1006 — would make HA's own proxy work, removing the
  disable/enable dance and the separate pod entirely.
- espressif/esp-matter#1798 — `attribute::update()` routing to code-driven
  clusters would let `sensor_loop.cpp` drop its registry workaround.

Re-test this procedure after any of them, and simplify it here.
