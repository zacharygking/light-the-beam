#!/usr/bin/env python3
"""Measure the downloaded Printables arena base and write its floor outline for OpenSCAD.

Reads models/printables/base_220.stl (binary STL), slices it, and reports:
  - footprint extents at floor level and the simplified outline -> models/arena_outline.scad
  - floor thickness (first height where an interior cavity appears)
  - the inner well (where the tube socket goes) and its centre
  - the wire channel exit: where the wall is open near the floor
  - the height of the inner box the lid seats on

Usage: python3 tools/measure_arena.py [--png]   (--png also writes slice images to docs/render/)
"""
import math
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STL = ROOT / "models" / "printables" / "base_220.stl"
OUT = ROOT / "models" / "arena_outline.scad"


def read_stl(path):
    with open(path, "rb") as f:
        f.read(80)
        n = struct.unpack("<I", f.read(4))[0]
        data = f.read()
    return [struct.unpack_from("<12fH", data, i * 50)[3:12] for i in range(n)]


def slice_z(tris, z):
    segs = []
    for t in tris:
        P = [(t[0], t[1], t[2]), (t[3], t[4], t[5]), (t[6], t[7], t[8])]
        pts = []
        for i in range(3):
            a, b = P[i], P[(i + 1) % 3]
            if (a[2] - z) * (b[2] - z) < 0:
                f = (z - a[2]) / (b[2] - a[2])
                pts.append((a[0] + f * (b[0] - a[0]), a[1] + f * (b[1] - a[1])))
        if len(pts) == 2:
            segs.append(pts)
    return segs


def loops(segs, tol=0.05):
    key = lambda p: (round(p[0] / tol), round(p[1] / tol))
    adj = defaultdict(list)
    for i, (a, b) in enumerate(segs):
        adj[key(a)].append(i)
        adj[key(b)].append(i)
    used = [False] * len(segs)
    out = []
    for i in range(len(segs)):
        if used[i]:
            continue
        used[i] = True
        a, b = segs[i]
        loop = [a, b]
        cur = b
        while True:
            nxt = [j for j in adj[key(cur)] if not used[j]]
            if not nxt:
                break
            j = nxt[0]
            used[j] = True
            p, q = segs[j]
            cur = q if key(p) == key(cur) else p
            loop.append(cur)
            if key(cur) == key(a):
                break
        out.append(loop)
    return out


def perim(l):
    return sum(math.dist(l[i], l[i + 1]) for i in range(len(l) - 1))


def bbox(l):
    xs = [p[0] for p in l]
    ys = [p[1] for p in l]
    return min(xs), max(xs), min(ys), max(ys)


def rdp(pts, eps):
    if len(pts) < 3:
        return pts
    a, b = pts[0], pts[-1]
    dmax, idx = 0, 0
    for i in range(1, len(pts) - 1):
        p = pts[i]
        if a == b:
            d = math.dist(p, a)
        else:
            t = ((p[0] - a[0]) * (b[0] - a[0]) + (p[1] - a[1]) * (b[1] - a[1])) / ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2)
            t = max(0, min(1, t))
            d = math.dist(p, (a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])))
        if d > dmax:
            dmax, idx = d, i
    if dmax > eps:
        return rdp(pts[: idx + 1], eps)[:-1] + rdp(pts[idx:], eps)
    return [a, b]


def main():
    if not STL.exists():
        sys.exit(f"missing {STL}: download the 220 mm base from Printables first (see models/printables/README.md)")
    tris = read_stl(STL)
    zs = [v for t in tris for v in (t[2], t[5], t[8])]
    print(f"{len(tris):,} triangles, height {max(zs):.1f} mm")

    # footprint
    foot = max(loops(slice_z(tris, 0.3)), key=perim)
    x0, x1, y0, y1 = bbox(foot)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    simp = rdp([(p[0] - cx, p[1] - cy) for p in foot], 0.4)
    print(f"footprint {x1 - x0:.1f} x {y1 - y0:.1f} mm, centre ({cx:.1f}, {cy:.1f}) in STL coords, {len(simp)} outline points")

    # floor thickness: first z where a second big loop (the well) appears
    floor_t = None
    for z10 in range(10, 60):
        z = z10 / 10
        big = [l for l in loops(slice_z(tris, z)) if perim(l) > 200]
        if len(big) >= 2:
            floor_t = z
            well = min(big, key=perim)
            break
    wx0, wx1, wy0, wy1 = bbox(well)
    print(f"floor thickness ~{floor_t} mm; inner well {wx1 - wx0:.0f} x {wy1 - wy0:.0f} mm, centre offset from footprint centre ({(wx0 + wx1) / 2 - cx:+.1f}, {(wy0 + wy1) / 2 - cy:+.1f})")

    # wire channel: heights where the outer wall and the well are joined into one loop
    joined = [z for z in (3, 4, 5, 6, 7, 8, 9, 10, 11, 12) if len([l for l in loops(slice_z(tris, z)) if perim(l) > 200]) == 1]
    ring = max(loops(slice_z(tris, 5)), key=perim)
    jumps = []
    for i in range(len(ring) - 1):
        a, b = ring[i], ring[i + 1]
        ra, rb = math.hypot(a[0] - cx, a[1] - cy), math.hypot(b[0] - cx, b[1] - cy)
        if abs(ra - rb) > 6:
            jumps.append((a[0] - cx, a[1] - cy))
    if jumps:
        ex = sum(p[0] for p in jumps) / len(jumps)
        ey = sum(p[1] for p in jumps) / len(jumps)
        print(f"wire exit through the wall at ~({ex:+.0f}, {ey:+.0f}) from footprint centre, angle {math.degrees(math.atan2(ey, ex)):.0f} deg, open at z {min(joined)}..{max(joined)} mm")
    # lid seat: top of the inner box = highest z with an inner-box loop
    seat = None
    for z10 in range(560, 300, -5):
        z = z10 / 10
        if any(200 < perim(l) < 800 for l in loops(slice_z(tris, z))):
            seat = z
            break
    print(f"lid seats on the inner box at ~{seat} mm")

    OUT.write_text(
        "// Footprint of the 220 mm arena base at floor level, extracted from the Printables STL\n"
        "// by tools/measure_arena.py. Centred on the footprint's bounding box. Units mm.\n"
        "// The flat edge is at +X in this frame; plinth.scad rotates it so the flat edge faces the back (+Y).\n"
        "arena_outline = [\n" + ",\n".join(f"  [{x:.2f}, {y:.2f}]" for x, y in simp) + "\n];\n"
        f"arena_outline_size = [{x1 - x0:.2f}, {y1 - y0:.2f}];\n"
        f"arena_well_offset = [{(wx0 + wx1) / 2 - cx:.2f}, {(wy0 + wy1) / 2 - cy:.2f}];\n"
        + (f"arena_wire_exit = [{ex:.1f}, {ey:.1f}];\n" if jumps else "")
    )
    print(f"wrote {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
