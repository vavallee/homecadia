#!/usr/bin/env bash
# Verify the shipping and bench profiles are actually different in the one way
# that matters: sleep.
#
# The bench profile exists because automatic light sleep powers down the
# USB-serial-JTAG at runtime. The port still enumerates, so every open fails at
# the driver level and it reads as broken hardware rather than a sleep state
# (field-notes.md section 2). If the bench profile silently stops disabling
# sleep, the console dies on the bench and the cause is invisible.
#
# Checks are POSITIVE on both sides. An earlier version of this only checked
# that the bench profile lacked "=y", which passes just as happily when the
# file is missing or truncated -- the same shape of silent-guard failure this
# repo has been bitten by before.
#
# Run locally after building both profiles:  tools/check-profiles.sh
set -euo pipefail

SHIPPING="${1:-firmware/sensor-01/sdkconfig}"
BENCH="${2:-firmware/sensor-01/build-bench/sdkconfig}"

KEYS=(CONFIG_PM_ENABLE CONFIG_FREERTOS_USE_TICKLESS_IDLE CONFIG_BT_LE_SLEEP_ENABLE)

fail() { echo "::error::$*" >&2; echo "FAIL: $*" >&2; exit 1; }

# A generated sdkconfig always carries the target. Without this, a missing or
# truncated file would sail through every check below.
sane() {
  [ -f "$1" ] || fail "$2 sdkconfig not found at $1 -- did that profile build?"
  grep -q "^CONFIG_IDF_TARGET=" "$1" \
    || fail "$1 does not look like a generated sdkconfig (no CONFIG_IDF_TARGET)"
}

sane "$SHIPPING" "shipping"
sane "$BENCH" "bench"

echo "shipping: $SHIPPING"
echo "bench:    $BENCH"
echo

rc=0
for k in "${KEYS[@]}"; do
  # Shipping MUST have it on. If it does not, either the profile regressed or
  # this script is reading the wrong file -- both worth failing on.
  if grep -qx "$k=y" "$SHIPPING"; then
    ship="on"
  else
    ship="OFF"
    echo "::error::$k is not enabled in the shipping profile ($SHIPPING)"
    rc=1
  fi

  # Bench MUST have it off, and Kconfig must have said so explicitly -- either
  # "# ... is not set", or absent because its dependency (PM_ENABLE) is off.
  if grep -qx "$k=y" "$BENCH"; then
    bench="ON"
    echo "::error::$k is enabled in the bench profile ($BENCH); the USB console will not survive"
    rc=1
  elif grep -qx "# $k is not set" "$BENCH"; then
    bench="off"
  else
    # Absent. Legitimate only when PM_ENABLE is off and this key depends on it.
    if grep -qx "# CONFIG_PM_ENABLE is not set" "$BENCH"; then
      bench="off (dep)"
    else
      bench="ABSENT"
      echo "::error::$k is absent from $BENCH and PM_ENABLE is not disabled -- cannot confirm sleep is off"
      rc=1
    fi
  fi

  printf '  %-38s shipping=%-4s bench=%s\n' "$k" "$ship" "$bench"
done

# The reverse case: bench-only features that must NOT reach a shipping image.
# The harness scan adds boot time and log noise; a shipping build that carries
# it is a shipping build nobody re-validated on the default profile.
BENCH_ONLY=(CONFIG_HOMECADIA_BENCH_SELFTEST)
for k in "${BENCH_ONLY[@]}"; do
  if grep -qx "$k=y" "$SHIPPING"; then
    ship="ON"
    echo "::error::$k is enabled in the shipping profile ($SHIPPING); bench-only"
    rc=1
  elif grep -qx "# $k is not set" "$SHIPPING"; then
    ship="off"
  else
    ship="ABSENT"
    echo "::error::$k is absent from $SHIPPING -- Kconfig.projbuild not picked up?"
    rc=1
  fi

  if grep -qx "$k=y" "$BENCH"; then
    bench="on"
  else
    bench="OFF"
    echo "::error::$k is not enabled in the bench profile ($BENCH)"
    rc=1
  fi
  printf '  %-38s shipping=%-4s bench=%s\n' "$k" "$ship" "$bench"
done

[ "$rc" -eq 0 ] || exit 1
echo
echo "OK - profiles differ as intended"
