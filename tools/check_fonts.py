#!/usr/bin/env python3
"""Check that the committed LVGL fonts match what make_fonts.py produces, by metrics.

Bitmap bytes differ between FreeType/Pillow versions (antialiasing), so a byte-for-byte diff
fails on CI. Instead this regenerates the fonts into a temp dir and compares, per font:
glyph count, line height, base line, and each glyph's advance width and box size (±1 px).
Exit 1 on any mismatch.
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FONTS = ROOT / "firmware" / "src" / "fonts"


def metrics(path: Path):
    s = path.read_text()
    glyphs = re.findall(r"\{\.bitmap_index = \d+, \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}", s)
    lh = re.search(r"\.line_height = (\d+), \.base_line = (\d+)", s)
    return [tuple(map(int, g)) for g in glyphs], (int(lh.group(1)), int(lh.group(2)))


def main():
    with tempfile.TemporaryDirectory() as tmp:
        env = {"PYTHONPATH": str(ROOT / "tools")}
        code = (
            "import make_fonts, pathlib, sys; "
            f"make_fonts.OUT_DIR = pathlib.Path({tmp!r}); make_fonts.main()"
        )
        subprocess.run([sys.executable, "-c", code], check=True, env={**__import__('os').environ, **env}, cwd=ROOT, stdout=subprocess.DEVNULL)
        bad = 0
        for f in sorted(FONTS.glob("*.c")):
            g_ref, lh_ref = metrics(f)
            g_new, lh_new = metrics(Path(tmp) / f.name)
            problems = []
            if len(g_ref) != len(g_new):
                problems.append(f"glyph count {len(g_ref)} vs {len(g_new)}")
            if lh_ref != lh_new:
                problems.append(f"line/base {lh_ref} vs {lh_new}")
            for i, (a, b) in enumerate(zip(g_ref, g_new)):
                # adv_w is 8.4 fixed point: allow 1 px; boxes and offsets allow 1 px
                if abs(a[0] - b[0]) > 16 or any(abs(a[k] - b[k]) > 1 for k in (1, 2, 3, 4)):
                    problems.append(f"glyph {i}: {a} vs {b}")
                    if len(problems) > 5:
                        break
            status = "ok " if not problems else "BAD"
            print(f"{status} {f.name}: {len(g_ref)} glyphs, line {lh_ref[0]}, base {lh_ref[1]}" + ("; " + "; ".join(problems[:5]) if problems else ""))
            bad += bool(problems)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
