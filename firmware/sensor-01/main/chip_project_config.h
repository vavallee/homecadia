/* CHIP project configuration overrides for sensor-01.
 *
 * Wired in by CONFIG_CHIP_PROJECT_CONFIG (sdkconfig.defaults). connectedhomeip
 * includes this before its own defaults in CHIPDeviceConfig.h, so anything set
 * here wins over the TEST_* placeholders.
 *
 * These feed BasicInformation via the device instance info provider and are
 * what a controller shows for the device. They are read at cluster init, not
 * settable at runtime — BasicInformation is code-driven in esp-matter v1.6 and
 * attribute::update() on it fails (see docs/field-notes.md section 9).
 *
 * Do NOT set the vendor or product *ID* here. 0xFFF1/0x8000 are matched to the
 * compiled-in test DAC; changing them breaks attestation. Names are free text
 * and carry no such constraint.
 */
#pragma once

/* Who made it. */
#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME "homecadia"

/* The device *model*, not the individual unit — all three units are the same
 * model and will report this identically. Per-unit naming is NodeLabel, which
 * the controller sets (rename the device in Home Assistant). */
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME "sensor-01"

/* Board revision this firmware expects: XIAO ESP32-C6 on the Seeed ePaper
 * driver board V2. Bump if the carrier changes. */
#define CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING "xiao-c6/driver-v2"
