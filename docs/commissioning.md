# Commissioning — sensor-01 → Home Assistant

Verified end to end, most recently **2026-08-24 on matter-server 1.4.0**: the
device commissions over BLE through **Home Assistant's own proxy** in 18-19
seconds, attaches as a sleepy child of the ZBT-2, and serves
temperature/humidity/battery over Thread.

The 2026-08-23 run needed a separate Node/noble proxy pod because HA's proxy
could not complete the BTP handshake. **That is fixed** — it was
matter-js/matterjs-server#1006, resolved upstream in v0.8.0, and this deployment
was three months behind. The noble pod and the disable/enable dance are gone.

## What is deployed

Home Assistant here is a **container on k3s, not HAOS** — there is no add-on
store, so the Matter server and the border router are separate workloads.

| | |
|---|---|
| Matter server | `ghcr.io/matter-js/matterjs-server:1.4.0` (matter.js 0.17.9) — **not** python-matter-server |
| Runs on | the node holding the ZBT-2 (`zbt2` label), `hostNetwork`, port 5580 |
| Key env | `BLE_PROXY=true`, `ENABLE_TEST_NET_DCL=true`, `PRIMARY_INTERFACE` pinned |
| Border router | `otbr`, same node, ZBT-2 passed through as a char device |
| Home Assistant | `ha2-home-assistant` on `k8sn1-master` (the node with the Bluetooth adapter) |
| Thread network | `ha-thread-5df4`, channel 15 |

HA's Matter integration points at `ws://<matter-server-host>:5580/ws`. The
manifests live in the homelab repo under `kubernetes/apps/default/`.

**Keep the image current.** This deployment sat on 0.7.1 for months against a
then-current 1.4.0, and the cost was two evenings chasing a BLE bug that had
been fixed upstream twelve days after the pinned tag. See
[field-notes.md](field-notes.md) §12.

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
automatically whenever its Matter integration is enabled. On 1.4.0 that is all
you need — leave HA's Matter integration on and do nothing else.

Confirm the proxy is attached:

```
BleProxyConnection   [ble0] BLE proxy handshake complete (version 1)
```

That is the *proxy protocol* handshake between the server and the client. It is
not the BTP handshake with the device, and it is not evidence that the device
will commission.

On releases before v0.8.0 this needed a separate Node/noble pod, because HA's
bleak/BlueZ client dropped the device's BTP handshake response
(matter-js/matterjs-server#1006 — `ProxyBleChannel.openChannel` awaited the
`write_and_subscribe` command before registering the binary-frame observer, so
on BlueZ the indication arrived with no listener attached). `ble-proxy-pod.yaml`
in this directory is kept only for that case.

The proxy runs on the node with the Bluetooth adapter, so **the device must be
within BLE range of that node**, not of the border router.

One live defect remains, and it costs throughput rather than blocking: bleak
reports the ATT MTU as 23, so BTP fragments are capped at 20 bytes instead of
244. The server logs it plainly:

```
ProxyBleChannel  Connected to <addr>, handle=1, BTP segment size=20 bytes (peripheral ATT_MTU up to 23)
```

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

1. **Get the sensor into its commissioning window.** It is time-limited from
   boot. A device that has never been paired is already advertising; one that
   was un-paired with `remove_node` reboots itself and comes back advertising
   (see Factory reset below). Otherwise power-cycle it.
   **The QR staying on the e-ink is not evidence the window is open** — it is
   drawn once and the panel simply holds the image.
2. Confirm it is advertising. From the Bluetooth host, look for service data
   under UUID `fff6`; the payload encodes discriminator 3840 and VID `0xFFF1`.
3. Confirm HA's BLE proxy is attached (precondition 2). Nothing to start.
4. Drive commissioning against the server's WebSocket API — **not** the HA UI,
   which gates Matter commissioning behind the mobile companion app:

   ```json
   {"message_id":"1","command":"commission_with_code",
    "args":{"code":"34970112332","network_only":false}}
   ```

   on `ws://<matter-server-host>:5580/ws`. Expect 18-19 seconds.

## Verify

```sh
ot-ctl child table          # the device should appear as a child, age near 0
```

and read attributes over Thread. `read_attribute` is **not** a command this
server implements — it is silently dropped, with no error and no log line. Use
`get_nodes` and read from the returned `attributes` map:
`1/1026/0` temperature (centi-°C), `2/1029/0` humidity (centi-%RH),
`3/47/11` battery millivolts.

Identity should read `homecadia` / `sensor-01` / `xiao-c6/driver-v2`
(`0/40/1`, `0/40/3`, `0/40/8`). If it reads `TEST_VENDOR` / `TEST_PRODUCT`, the
device was commissioned before `main/chip_project_config.h` existed — see
**Device identity is fixed at commissioning time** below.

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

**Preferred: `remove_node`.** As of `7895168` the device handles the rest itself:

```json
{"message_id":"1","command":"remove_node","args":{"node_id":<id>}}
```

The server sends `removeFabric`, the device reboots 3 seconds later, and it
comes back advertising over BLE with no physical access. Verified 2026-08-24.
This also clears the server-side node, so there is no ghost to tidy up.

Before that fix the device went **silent** after `remove_node` and needed a
power cycle. Two causes, both real:

- `kFabricRemoved` reopened the window with `kDnssdOnly`, advertising over
  DNS-SD on a network the device had just left.
- With `CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=y` the stack tears BLE down after
  commissioning and hands its RAM back to the heap
  (`BLEManagerImpl::DeinitESPBleLayer` → `ClaimBLEMemory`), so **no**
  advertisement mode could have worked. Hence the reboot.

**Fallback: erase NVS over USB.** Needed if the device is unreachable, or to
wipe user settings as well as the fabric:

```sh
esptool --chip esp32c6 -p <port> --before default_reset --after hard_reset \
        erase_region 0x10000 0xC000      # nvs, per partitions.csv
```

This is only possible from a host whose kernel will drive the C6's native USB
control lines. On the k3s nodes here it fails at `pyserial`'s `open()` with
`OSError: [Errno 71] Protocol error` on the `TIOCMBIC/TIOCM_RTS` ioctl, on both
esptool 4.7.0 and 5.3.1 — the reset sequence *is* DTR/RTS, so there is nothing
to route around. Flash and erase from the bench over usbipd instead.

**A plain reflash does not decommission.** The fabric lives in NVS and survives
`write_flash`, so firmware can be iterated without re-pairing — verified.

## Device identity is fixed at commissioning time

`VendorName`, `ProductName` and `HardwareVersionString` come from
`main/chip_project_config.h` at compile time (`CONFIG_CHIP_PROJECT_CONFIG`;
`CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER` is **not** set, so nothing is read
from the `fctry` partition).

The controller caches BasicInformation from the commissioning interview and does
not refresh it. **A reflash alone does not change what the controller shows** —
verified 2026-08-24: after flashing the corrected strings the device still
appeared as `TEST_VENDOR` / `TEST_PRODUCT`, and only read `homecadia` /
`sensor-01` after a decommission and re-commission.

Forcing it with `interview_node` does not work either. A complete read against a
sleepy end device on a 5-second poll exceeds the interview budget:

```
Controller~ndHandler  Interview requested for node @1:16 - do a complete read
WebSocketC~erHandler  [a] WebSocket error response (interview_node) 1 [aborted] Operation aborted
    at abort.timeoutHandler (@matter/protocol/src/peer/Peer.ts:447:27)
```

**Get identity right before commissioning the remaining units**, or plan to
decommission and re-commission each one.

Per-unit naming does *not* belong here — all three units share a model. Use
**NodeLabel**, set by the controller.

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
| `BTP handshake response not received` | matter-server older than v0.8.0 — upgrade, do not chase it |
| `No commissionable device was discovered` | window closed, or out of BLE range of the Bluetooth host. Confirm with `btmon` before assuming range: look for `Service Data: Matter Profile ID (0xfff6)` |
| Device silent after `remove_node` | firmware older than `7895168`; power-cycle it |
| Controller shows `TEST_VENDOR` after a reflash | cached interview — see **Device identity is fixed at commissioning time** |
| Reaches `connectNetwork`, then CASE fails | device out of Thread range, or the OTBR is not running |
| Commissions, then goes unavailable | sleepy ICD — normal between polls; if permanent, check the child table |
| Temperature/humidity null, battery fine | esp-matter defect, [field-notes.md](field-notes.md) §9 |

## Upstream status

| Issue | State |
|---|---|
| matter-js/matterjs-server#1006 | **resolved** — fixed in #710, released v0.8.0. Retested on 1.4.0 2026-08-24; HA's own proxy commissions in 18.8s. The noble pod and disable/enable dance are gone from this document. |
| matter-js/matterjs-server#1003 | open — consolidates the `write_and_subscribe` gap and the connect race from our #1005. Only affects the noble client, which is no longer on the critical path. |
| ATT MTU reported as 23 | open, being split out of #1006. Throughput only: 20-byte BTP fragments instead of 244. Upstream's fix is `char.max_write_without_response_size + 3`, which reads the cached D-Bus property (`bleak/backends/bluezdbus/manager.py:162`) and does **not** go through the `_acquire_mtu()`/`AcquireWrite` path that Matter's characteristics cannot satisfy. |
| espressif/esp-matter#1798 | open — `attribute::update()` routing to code-driven clusters would let `sensor_loop.cpp` drop its registry workaround. |

Re-test this procedure after any of them, and simplify it here.
