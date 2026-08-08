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

## Steps

1. Power the sensor. On first boot (or after factory reset) it prints the QR
   code URL and 11-digit manual pairing code to the serial console **and**
   renders the QR on the e-ink display (from milestone 3).
2. HA → Settings → Devices & Services → Add integration → Matter → scan the QR
   (or enter the manual code).
3. HA commissions over BLE, transfers Thread credentials, and the device joins
   the Thread network as a sleepy end device. Expect the whole flow to take
   1–2 minutes.

## Expected warning: uncertified vendor ID

The firmware uses Espressif's **test vendor ID 65521 (0xFFF1)**. It is not
Matter-certified, so HA shows an "uncertified device" warning during
commissioning. Accept it — for a self-built device on a self-hosted controller
this is the normal, supported path. Apple Home shows an equivalent warning and
also accepts the device.

## Factory reset

Hold the encoder button for 10 seconds (from milestone 6). Until then:
`idf.py erase-flash` and reflash.

## Troubleshooting (to be filled during milestone 2 bringup)

- Commissioning stalls at Thread join → check OTBR dataset is active, check
  ZBT-2 firmware.
- Device commissions but goes unavailable → ICD timing; see power-budget.md
  poll intervals.
