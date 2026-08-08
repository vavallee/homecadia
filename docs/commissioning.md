# Commissioning — sensor-01 → Home Assistant

## Prerequisites

- Home Assistant with the **Matter server** add-on (or matter-server container).
- **Home Assistant Connect ZBT-2** flashed with OpenThread RCP firmware,
  running as an OpenThread Border Router (OTBR) via the OpenThread Border
  Router add-on. One protocol at a time: in this deployment the ZBT-2 does
  Thread only, not Zigbee.
- A Thread network created in HA (Settings → Devices & Services → Thread),
  with the ZBT-2's OTBR as the preferred network.
- Bluetooth on the commissioning device (phone with the HA companion app, or
  a Bluetooth adapter on the HA host) — initial commissioning is over BLE.

## Pairing codes

Until per-device factory partitions are generated (esp-matter-mfg-tool, open
work item), all units use the compiled-in Matter **test credentials**:
passcode 20202021, discriminator 3840, manual pairing code **34970112332**.
The serial console and (once a panel is attached) the e-ink QR are the
authoritative source if this ever disagrees. Consequence of shared codes:
only put ONE uncommissioned unit in pairing mode at a time.

## If the Matter integration asks for a URL

That prompt means HA found no Matter server — the integration is only a
client:

- **HA OS / Supervised**: cancel, install Settings → Add-ons → **Matter
  Server**, start it, re-add the integration (auto-discovers; manual URL is
  `ws://core-matter-server:5580/ws`).
- **Container/Core**: run the server yourself and point the integration at it
  (`ws://<host>:5580/ws`). `--network host` is required — mDNS and
  commissioning break behind Docker NAT:

  ```sh
  docker run -d --name matter-server --restart=unless-stopped \
    --network host --security-opt apparmor=unconfined \
    -v /opt/matter-server:/data \
    ghcr.io/home-assistant-libs/python-matter-server:stable
  ```

  The same applies to the Thread side on Container installs: no OTBR add-on
  either — the ZBT-2 needs an `openthread/otbr` container with its serial
  device passed through.

## Steps

1. Power the sensor. On first boot (or after factory reset) it prints the QR
   code URL and 11-digit manual pairing code to the serial console **and**
   renders the QR on the e-ink display (from milestone 3).
2. HA → Settings → Devices & Services → Add integration → Matter → scan the QR
   (or enter the manual code).
3. HA commissions over BLE, transfers Thread credentials, and the device joins
   the Thread network as a sleepy end device. Expect the whole flow to take
   1–2 minutes.

## Apple Home instead of (or alongside) HA

Works only with an Apple **Thread border router** in the house — HomePod
mini, HomePod (2nd gen), or a Thread-capable Apple TV 4K. An iPhone alone
cannot host the device: it handles the BLE half of commissioning, but this
firmware is Thread-only (Wi-Fi compiled out) and needs a Thread network to
land on.

Home app → **+** → Add Accessory → More Options… → My Accessory Isn't Shown
Here → enter 34970112332 → accept the "Uncertified Accessory" warning (test
VID). The device appears as temperature + humidity sensors with a battery
level.

Multi-fabric: Matter devices hold up to five fabrics, so Apple Home and HA
can both be commissioned — add to one, then accessory settings → **Turn On
Pairing Mode** and give the new code to the other. For both to work well the
two border routers should share one Thread network (Thread credential
sharing), which is its own setup step.

## Expected warning: uncertified vendor ID

The firmware uses Espressif's **test vendor ID 65521 (0xFFF1)**. It is not
Matter-certified, so HA shows an "uncertified device" warning during
commissioning. Accept it — for a self-built device on a self-hosted controller
this is the normal, supported path. Apple Home shows an equivalent warning and
also accepts the device.

## Factory reset

Hold the encoder button for 10 seconds. The device wipes its fabrics and
settings, reboots, and shows the commissioning QR again.
(`idf.py erase-flash` + reflash also works over USB.)

## Troubleshooting (to be filled during milestone 2 bringup)

- Commissioning stalls at Thread join → check OTBR dataset is active, check
  ZBT-2 firmware.
- Device commissions but goes unavailable → ICD timing; see power-budget.md
  poll intervals.
