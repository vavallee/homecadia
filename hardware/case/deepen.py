#!/usr/bin/env python3
"""Deepen a closed STL shell: cut at z = Z_CUT, lift everything above by DZ,
and join the two cut edges with a vertical band. Usage: deepen.py IN OUT Z_CUT DZ"""
import struct, sys
import numpy as np

def load(fn):
    b = open(fn, 'rb').read()
    n = struct.unpack('<I', b[80:84])[0]
    a = np.frombuffer(b[84:84 + n * 50], dtype=np.dtype([('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')]))
    return a['v'].astype(np.float64)

def save(fn, tris, header=b'homecadia case-back rev 5'):
    tris = np.asarray(tris, dtype=np.float64)
    nrm = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    ln = np.linalg.norm(nrm, axis=1, keepdims=True); ln[ln == 0] = 1
    nrm /= ln
    rec = np.zeros(len(tris), dtype=np.dtype([('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')]))
    rec['n'] = nrm; rec['v'] = tris
    with open(fn, 'wb') as f:
        f.write(header.ljust(80, b' ')[:80]); f.write(struct.pack('<I', len(tris))); f.write(rec.tobytes())

def weld(tris, q=1e-4):
    v = tris.reshape(-1, 3)
    key = np.round(v / q).astype(np.int64)
    uniq, inv = np.unique(key, axis=0, return_inverse=True)
    pts = np.zeros((len(uniq), 3)); np.add.at(pts, inv.ravel(), v); pts /= np.bincount(inv.ravel())[:, None]
    return pts, inv.reshape(-1, 3)

def manifold_report(tris):
    pts, f = weld(tris)
    e = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]])
    und = np.sort(e, axis=1)
    _, cu = np.unique(und, axis=0, return_counts=True)
    _, cd = np.unique(e, axis=0, return_counts=True)
    vol = np.einsum('ij,ij->i', tris[:, 0], np.cross(tris[:, 1], tris[:, 2])).sum() / 6
    return dict(tris=len(tris), edges_not_2=int((cu != 2).sum()), directed_dupes=int((cd > 1).sum()), volume=round(vol, 2))

def deepen(tris, zc, dz):
    pts, f = weld(tris)
    if np.any(np.abs(pts[:, 2] - zc) < 1e-6):
        raise SystemExit("a vertex lies on the cut plane; pick another Z_CUT")
    above = pts[:, 2] > zc
    lifted = pts.copy(); lifted[above, 2] += dz
    cut_cache = {}
    def cut_point(i, j):
        k = (min(i, j), max(i, j))
        if k not in cut_cache:
            a, b = pts[k[0]], pts[k[1]]
            t = (zc - a[2]) / (b[2] - a[2]); p = a + t * (b - a); p[2] = zc
            cut_cache[k] = p
        p = cut_cache[k]
        return p.copy(), p + np.array([0, 0, dz])        # (lower copy, upper copy)
    out = []
    for tri, (i0, i1, i2) in zip(tris, f):
        idx = [i0, i1, i2]; side = [above[i] for i in idx]
        if all(side) or not any(side):
            out.append([lifted[i] for i in idx]); continue
        # rotate so that idx[0] is the lone vertex
        lone = [s for s in side].count(True) == 1
        r = next(k for k in range(3) if side[k] == lone)
        a, b, c = idx[r], idx[(r + 1) % 3], idx[(r + 2) % 3]
        ab_lo, ab_up = cut_point(a, b); ac_lo, ac_up = cut_point(a, c)
        if lone:   # a above, b and c below
            out.append([lifted[a], ab_up, ac_up])
            out.append([ab_lo, lifted[b], lifted[c]]); out.append([ab_lo, lifted[c], ac_lo])
            p_lo, p_up, q_lo, q_up = ab_lo, ab_up, ac_lo, ac_up
        else:      # a below, b and c above
            out.append([lifted[a], ab_lo, ac_lo])
            out.append([ab_up, lifted[b], lifted[c]]); out.append([ab_up, lifted[c], ac_up])
            p_lo, p_up, q_lo, q_up = ab_lo, ab_up, ac_lo, ac_up
        # band quad between the cut segment's lower and upper copies, facing like the original face
        n0 = np.cross(tri[1] - tri[0], tri[2] - tri[0]); n0[2] = 0
        quad = [[p_lo, q_lo, q_up], [p_lo, q_up, p_up]]
        nq = np.cross(quad[0][1] - quad[0][0], quad[0][2] - quad[0][0])
        if np.dot(nq, n0) < 0:
            quad = [[p_lo, q_up, q_lo], [p_lo, p_up, q_up]]
        out.extend(quad)
    return np.array(out)

if __name__ == '__main__':
    src, dst, zc, dz = sys.argv[1], sys.argv[2], float(sys.argv[3]), float(sys.argv[4])
    t = load(src)
    print("in ", manifold_report(t))
    o = deepen(t, zc, dz)
    print("out", manifold_report(o))
    v = o.reshape(-1, 3); print("bbox", v.min(0).round(2), v.max(0).round(2))
    save(dst, o)
