# Network infrastructure

HA-side hardware the homecadia fleet depends on. Separate from any single
device — the sensor-01 parts list is [bom.md](bom.md); pairing procedure is
[commissioning.md](commissioning.md).

## Home Assistant Connect ZBT-2 (Nabu Casa NC-ZBT-9741)

- Silicon Labs MG24 (Zigbee 3.0 / Thread radio) + ESP32-S3 as USB-serial
  bridge
- USB-C, 5V DC 500mA; 460800 baud (4× the ZBT-1's 115200)
- 83×83×179mm incl. antenna; antenna 24mm dia × 164mm, 4.16 dBi peak,
  omnidirectional
- Transmit power 8 dBm (rest of world) / 10 dBm (Europe)
- Supports ZHA, Zigbee2MQTT, and OpenThread Border Router
- **One protocol at a time.** Vendor design decision, not a config
  limitation: Nabu Casa tested multiprotocol (MultiPAN) on the ZBT-1, found
  it unstable, and will not implement it. Running Zigbee and Thread networks
  in parallel needs two units.
- **Role here: dedicated Thread border router (OTBR)** for the
  Matter-over-Thread mesh sensor-01 commissions onto.
- Cannot act as a standalone Thread border router independent of Home
  Assistant.
- Experimental repeater firmware exists via the Open Home Foundation toolbox;
  reflashing to repeater mode removes coordinator capability until stock
  firmware is restored.

## Home Assistant Voice Preview Edition (Nabu Casa NC-VK-9727)

- ESP32-S3, 16MB flash, 8MB octal PSRAM; XMOS XU316 audio DSP (echo
  cancellation, noise removal, automatic gain control)
- Wi-Fi 2.4GHz + BLE 5.0. **Not a Thread device** — it does not join the
  homecadia Thread mesh.
- ESPHome preloaded; open firmware for both the ESP32 and the XMOS
- USB-C 5V 2A; cable and PSU not included
- Grove port for sensor expansion; 3.5mm out with TI AIC3204 DAC

### STT host requirement

Fully-local speech-to-text wants an **Intel N100 or better**. Weaker HA hosts
need Speech-to-Phrase (limited to predefined home-control sentences) or an HA
Cloud subscription.

Our HA host (from the homelab repo, `docs/INVENTORY.md`): HA 2026.5.0 runs as
a container (`ha2-home-assistant`, hostNetwork) on **k8sn1-master** (x86,
Debian 13) in the home k3s cluster, and **wyoming-whisper + wyoming-piper are
already deployed** there — so the fully-local path is the default.

- `TODO/UNVERIFIED`: k8sn1's CPU model is not recorded anywhere (a known
  inventory gap in the homelab repo). Confirm it is N100-class or better
  under real Whisper load before relying on local STT latency.
- HA runs as a container, not HAOS — there is no add-on store. The
  Speech-to-Phrase fallback would be another cluster workload, not an add-on
  install.

## Order status — ameriDroid, pending as of 2026-08-11

| Item | Qty | Price (CAD) |
|---|---|---|
| Connect ZBT-2 | 1 | 68.40 (after 5OFFEMAIL) |
| Voice PE | 1 | 81.70 (after 5OFFEMAIL) |
| Global Post DDP shipping | | 40.00 |
| **Total** | | **190.10** |

DDP selected deliberately: duties and brokerage included, nothing owed on
delivery. Transit 6–21 days via Canada Post, not guaranteed. The non-DDP
Global Post option was $37 but would have added ~$28 HST plus Canada Post
handling at the door. UPS Standard was rejected — brokerage disbursement
exposure.

## Decided: one ZBT-2, Thread only (2026-08-11)

The parallel Zigbee + Thread plan is dropped. One unit, dedicated to Thread.
No second order, no second $40 freight charge.

Nothing is lost today: HA itself runs **Z-Wave** (Zooz stick on k8sn1,
zwave-js-ui) with no Zigbee network of its own — `docs/INVENTORY.md` in the
homelab repo shows no ZHA, Zigbee2MQTT, or Zigbee devices. Zigbee currently
lives on a **Samsung SmartThings hub**, outside HA.

**Watch item:** the SmartThings hub is slated for decommissioning. Whatever
Zigbee devices are on it need somewhere to go, and this ZBT-2 cannot take
them while it is serving Thread — one protocol at a time (see above). The
options at that point are a second coordinator (freight paid twice, as
avoided here), replacing those devices with Thread/Matter or Z-Wave
equivalents, or keeping the SmartThings hub alive as a Zigbee-only bridge.

- `TODO`: inventory the Zigbee devices on SmartThings before decommissioning
  it, so the migration cost is known rather than discovered.

## Open questions

- Voice PE stock was unconfirmed at checkout across all NA distributors. If
  it backorders, does the ZBT-2 ship separately or does the whole order hold?
