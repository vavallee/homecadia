# Building the firmware

Pinned toolchain: **ESP-IDF v5.5.5 + esp-matter release/v1.6** — the same
combination as the CI image. Don't mix other versions; esp-matter is tightly
coupled to its bundled connectedhomeip submodule and the IDF minor version.

Exact pins (extracted from the `release-v1.6_idf_v5.5.5` image):

| Component | Ref |
|---|---|
| esp-matter | `36c2634e99c884830897e2b9501e2d9a6c9d60fd` (head of `release/v1.6`) |
| connectedhomeip submodule | `d46cc8c2886cbefc338544bdb2e2f8128f3e9970` |
| ESP-IDF | `v5.5.5` |
| Docker image | `espressif/esp-matter:release-v1.6_idf_v5.5.5` |

Assumes Linux. Two paths; Docker is the low-friction one.

## Path A: Docker (matches CI exactly)

```sh
docker pull espressif/esp-matter:release-v1.6_idf_v5.5.5

# from the repo root
# (the cd must be inside bash -c: the image's shell init overrides docker's -w
#  and drops you in $ESP_MATTER_PATH, where idf.py would build esp-matter itself.
#  The named ccache volume makes rebuilds after fullclean fast.)
docker run --rm -it -v "$PWD":/work \
  -v homecadia-ccache:/root/.cache/ccache -e IDF_CCACHE_ENABLE=1 \
  espressif/esp-matter:release-v1.6_idf_v5.5.5 \
  bash -c 'cd /work/firmware/sensor-01 && idf.py set-target esp32c6 build'
```

Flashing from inside the container needs the serial device passed through:

```sh
docker run --rm -it --device=/dev/ttyACM0 -v "$PWD":/work \
  espressif/esp-matter:release-v1.6_idf_v5.5.5 \
  bash -c 'cd /work/firmware/sensor-01 && idf.py -p /dev/ttyACM0 flash monitor'
```

(XIAO ESP32-C6 enumerates as USB CDC, typically `/dev/ttyACM0`. If flashing
won't start, hold BOOT while plugging in.)

## Path B: native install

```sh
# ESP-IDF v5.5.5
git clone -b v5.5.5 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32c6 && cd ..

# esp-matter v1.6
git clone -b release/v1.6 --depth 1 https://github.com/espressif/esp-matter.git
cd esp-matter
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

## CI

`.github/workflows/build-sensor-01.yml` builds on every push touching
`firmware/**` using the same Docker image and uploads the flash images
(app, bootloader, partition table, `flasher_args.json`) as an artifact.
