# Source reliability

Traps hit during research, recorded so nobody re-learns them. Where a claim
here conflicts with a vendor doc, this file wins until hardware measurement
says otherwise.

- **espboards.dev pin tables are wrong for the XIAO ESP32-C6.** They are
  shared boilerplate across boards, not board-specific. Use the official
  Seeed pinout diagram and schematic; the resolved mapping lives in
  [pinmap.md](pinmap.md).
- **The Seeed wiki battery-sense example code is unreliable.** Its
  `analogReadMilliVolts(A0)` snippet is shared XIAO boilerplate that assumes
  an onboard divider; the C6 schematic shows none populated
  (schematic-verified in [pinmap.md](pinmap.md)). Any divider is user-added —
  and on this build it is on GPIO5/MTDI, not A0.
- **Seeed's deep-sleep current claims (~15µA) are optimistic** and
  regulator-dependent. Treated as unverified until measured on our stack
  ([bringup.md](bringup.md), [power-budget.md](power-budget.md)).
- **esp-matter version labels: pin the SHA, not the branch name.** During
  early planning, "v1.6" in esp-matter docs referred to unpinned `main` (no
  release branch existed yet); `release/v1.6` has since been cut, and this
  repo pins its exact commit plus the matching Docker image
  ([build.md](build.md)). Never build from a floating label — a version
  string in Espressif docs is not evidence of a pinned release.
