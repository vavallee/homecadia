#!/usr/bin/env bash
# Verify the firmware version is derived from the git tag, carried into the
# built image, and monotonically increasing.
#
# Matter's SoftwareVersion is a uint32 that OTA compares numerically. If it
# fails to increase, a controller declines the update with NO error -- the
# device simply never updates. That is invisible until someone notices a
# sensor running months-old firmware, so it is checked here instead.
#
# Run locally after a build:  tools/check-version.sh
set -euo pipefail

BIN="${1:-firmware/sensor-01/build/homecadia-sensor-01.bin}"

fail() { echo "::error::$*" >&2; echo "FAIL: $*" >&2; exit 1; }

# --- what the tag says ------------------------------------------------------
desc=$(git describe --tags --long --dirty --match 'v[0-9]*' 2>/dev/null) \
  || fail "no matching tag found. A shallow clone has none -- actions/checkout needs fetch-depth: 0."
echo "git describe:        $desc"

[[ "$desc" =~ ^v([0-9]+)\.([0-9]+)\.([0-9]+)- ]] \
  || fail "tag does not parse as vMAJOR.MINOR.PATCH: $desc"
major=${BASH_REMATCH[1]}; minor=${BASH_REMATCH[2]}; patch=${BASH_REMATCH[3]}

[ "$minor" -lt 100 ] && [ "$patch" -lt 100 ] \
  || fail "MINOR and PATCH must stay under 100 for the MAJOR*10000+MINOR*100+PATCH encoding: $desc"

num=$(( major * 10000 + minor * 100 + patch ))
semver="${major}.${minor}.${patch}"
echo "SoftwareVersion:     $num  ($semver)"

# --- what the image actually carries ---------------------------------------
# esp_app_desc_t begins at 0x20 of the app binary; its 32-byte version field is
# at 0x30. A stale build directory or a failed re-configure shows up here and
# nowhere else.
if [ -f "$BIN" ]; then
  built=$(python3 - "$BIN" <<'PY'
import sys
d = open(sys.argv[1], "rb").read()
sys.stdout.write(d[0x30:0x50].split(b"\0")[0].decode(errors="replace"))
PY
)
  echo "image version:       ${built:-<empty>}"
  [ -n "$built" ] || fail "app_desc.version is empty in $BIN"
  [ "$built" != "0.0.0-untagged" ] \
    || fail "image was built without tags -- version.cmake fell back. Fetch tags before building."
  [ "${built%%-*}" = "$semver" ] \
    || fail "image version '$built' does not match tag '$semver' -- stale build directory?"
else
  echo "image version:       (no binary at $BIN, skipping)"
fi

# --- monotonicity against the previous tag ---------------------------------
prev=$(git tag --list 'v[0-9]*' --sort=-v:refname | grep -vFx "v${semver}" | head -n1 || true)
if [ -n "$prev" ] && [[ "$prev" =~ ^v([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
  pnum=$(( BASH_REMATCH[1] * 10000 + BASH_REMATCH[2] * 100 + BASH_REMATCH[3] ))
  echo "previous tag:        $prev  ($pnum)"
  [ "$num" -gt "$pnum" ] \
    || fail "SoftwareVersion $num is not greater than previous $pnum ($prev). OTA would silently decline the update."
else
  echo "previous tag:        (none to compare against)"
fi

echo "OK"
