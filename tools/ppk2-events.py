#!/usr/bin/env python3
"""Break a Nordic PPK2 capture (.ppk2) into what the current is spent on.

  tools/ppk2-events.py CAPTURE.ppk2 [--from S] [--to S]

Prints the average, the sleep floor, and every wake event grouped by kind,
with the charge each kind costs per minute. A .ppk2 is a zip; session.raw is
6 bytes per sample: float32 microamps, then uint16 digital channels.

Needs numpy. Streams the file, so multi-GB captures are fine.
"""
import argparse, json, sys, zipfile
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument("file")
ap.add_argument("--from", dest="t0", type=float, default=0.0, help="start, seconds into the capture")
ap.add_argument("--to", dest="t1", type=float, default=None, help="end, seconds into the capture")
ap.add_argument("--awake-ua", type=float, default=2000.0, help="bin average above this counts as awake")
ap.add_argument("--gap-ms", type=float, default=30.0, help="awake bins closer than this are one event")
ap.add_argument("--list", type=int, default=0, help="also list the first N events")
a = ap.parse_args()

z = zipfile.ZipFile(a.file)
sps = json.loads(z.read("metadata.json"))["metadata"]["samplesPerSecond"]
BIN = sps // 1000                      # 1 ms bins
rec = np.dtype([("ua", "<f4"), ("bits", "<u2")])
means, peaks = [], []
with z.open("session.raw") as f:
    chunk = rec.itemsize * BIN * 4000  # 4 s at a time
    left = b""
    while True:
        b = f.read(chunk)
        if not b:
            break
        b = left + b
        n = (len(b) // (rec.itemsize * BIN)) * rec.itemsize * BIN
        left = b[n:]
        ua = np.frombuffer(b[:n], dtype=rec)["ua"].reshape(-1, BIN)
        means.append(ua.mean(axis=1)); peaks.append(ua.max(axis=1))
m = np.concatenate(means); pk = np.concatenate(peaks)   # per-ms mean and peak, in uA
i0 = int(a.t0 * 1000); i1 = len(m) if a.t1 is None else min(len(m), int(a.t1 * 1000))
m, pk = m[i0:i1], pk[i0:i1]
dur = len(m) / 1000.0
total_uc = m.sum() / 1000.0            # uA*ms -> uC
print(f"span {dur:.1f} s   average {m.mean():.1f} uA   charge {total_uc/1000:.2f} mC")

awake = m > a.awake_ua
# Mean, not median: asleep, the XIAO's buck delivers the load in ~1 kHz
# pulses, so most 1 ms bins read near zero and the median is meaningless.
floor = m[~awake].mean() if (~awake).any() else 0.0
print(f"sleep floor (mean of asleep ms) {floor:.1f} uA   asleep {100*(~awake).mean():.2f} % of the time")

# group awake bins into events
idx = np.flatnonzero(awake)
events = []
if len(idx):
    gap = int(a.gap_ms)
    start = prev = idx[0]
    for i in idx[1:]:
        if i - prev > gap:
            events.append((start, prev)); start = i
        prev = i
    events.append((start, prev))
rows = []
for s, e in events:
    seg = m[s:e + 1]
    q = (seg - floor).sum() / 1000.0   # uC above the floor
    rows.append((s / 1000.0 + a.t0, e - s + 1, q, pk[s:e + 1].max() / 1000.0))
rows = np.array(rows) if rows else np.zeros((0, 4))

def kind(d_ms, q_uc, peak_ma):
    if peak_ma < 150:  return "wake, no transmit"
    if d_ms < 40:      return "single radio exchange (poll)"
    if d_ms < 400:     return "short burst"
    return "long burst (>0.4 s)"
kinds = {}
for t, d, q, p in rows:
    kinds.setdefault(kind(d, q, p), []).append((t, d, q, p))
ev_uc = rows[:, 2].sum() if len(rows) else 0.0
print(f"\nfloor alone: {floor*dur/1000:.2f} mC ({floor:.0f} uA)   events: {ev_uc/1000:.2f} mC ({ev_uc/dur:.0f} uA)")
print(f"{'kind':32s} {'count':>6s} {'per min':>8s} {'med ms':>7s} {'med uC':>8s} {'total mC':>9s} {'avg uA':>7s}")
for k, v in sorted(kinds.items(), key=lambda kv: -sum(x[2] for x in kv[1])):
    v = np.array(v)
    print(f"{k:32s} {len(v):6d} {len(v)/dur*60:8.1f} {np.median(v[:,1]):7.0f} {np.median(v[:,2]):8.1f} {v[:,2].sum()/1000:9.2f} {v[:,2].sum()/dur:7.1f}")
if a.list:
    print("\n   t(s)   ms      uC  peak mA  kind")
    for t, d, q, p in rows[: a.list]:
        print(f"{t:8.2f} {d:5.0f} {q:8.1f} {p:7.1f}  {kind(d, q, p)}")
