#!/bin/sh
# Run every check the project has, end to end. Exit non-zero on the first hard failure.
#   tools/validate.sh            # everything
#   tools/validate.sh --quick    # skip the ESP32 builds and OpenSCAD exports
set -u
cd "$(dirname "$0")/.."
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
OPENSCAD="${OPENSCAD:-/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD}"
CHROME="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
QUICK=0; [ "${1:-}" = "--quick" ] && QUICK=1
pass=0; fail=0; skip=0
ok()   { pass=$((pass+1)); printf "  \033[32mok\033[0m   %s\n" "$1"; }
bad()  { fail=$((fail+1)); printf "  \033[31mFAIL\033[0m %s\n" "$1"; }
skp()  { skip=$((skip+1)); printf "  \033[33mskip\033[0m %s\n" "$1"; }
run()  { name=$1; shift; if "$@" >/tmp/validate.log 2>&1; then ok "$name"; else bad "$name"; tail -15 /tmp/validate.log | sed 's/^/       /'; fi; }

echo "== Python tools"
run "py_compile tools/*.py" python3 -m py_compile tools/*.py
run "fixtures parse (python reference)" python3 tools/check_fixtures.py
run "fonts regenerate identically" sh -c 'python3 tools/make_fonts.py >/dev/null && git diff --exit-code --stat -- firmware/src/fonts'
run "logos regenerate identically (offline)" sh -c 'python3 tools/make_logos.py --offline >/dev/null && git diff --exit-code --stat -- firmware/src/logos.h firmware/src/logos.c'
if [ -f models/printables/base_220.stl ]; then
  run "measure_arena writes outline unchanged" sh -c 'python3 tools/measure_arena.py >/dev/null && git diff --exit-code --stat -- models/arena_outline.scad'
else skp "measure_arena (no models/printables/base_220.stl)"; fi

echo "== Firmware"
run "native parser tests" sh -c 'cd firmware && pio test -e native'
if [ $QUICK = 1 ]; then skp "ESP32 builds (--quick)"; else
  run "build cyd" sh -c 'cd firmware && pio run -e cyd'
  run "build cyd_demo" sh -c 'cd firmware && pio run -e cyd_demo'
  sz=$(grep -E "^(RAM|Flash):" /tmp/validate.log | tr -s ' ' | tr '\n' ' '); [ -n "$sz" ] && echo "       $sz"
fi

echo "== Models"
if [ $QUICK = 1 ]; then skp "OpenSCAD exports (--quick)"; elif [ -x "$OPENSCAD" ]; then
  run "export all parts" sh -c "cd models && make -B OPENSCAD='$OPENSCAD'"
  run "parts sanity (size, height <= 256 mm, footprint <= 256 mm)" python3 - <<'EOF'
import sys, glob
sys.path.insert(0, "tools")
from measure_arena import read_stl     # binary or ASCII (OpenSCAD 2021 writes ASCII)
bad = 0
for f in sorted(glob.glob("models/exports/*.stl")):
    tris = read_stl(f)
    xs = [v for t in tris for v in (t[0], t[3], t[6])]; ys = [v for t in tris for v in (t[1], t[4], t[7])]; zs = [v for t in tris for v in (t[2], t[5], t[8])]
    w, h, z = max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)
    flag = "" if (w <= 256 and h <= 256 and z <= 256) else "  <-- exceeds P1S 256 mm"
    print(f"{f.split('/')[-1]:18s} {len(tris):7d} tris  {w:6.1f} x {h:6.1f} x {z:6.1f} mm{flag}")
    if flag or len(tris) < 4: bad += 1
sys.exit(1 if bad else 0)
EOF
  cat /tmp/validate.log | sed 's/^/       /'
else skp "OpenSCAD not found at $OPENSCAD"; fi

echo "== Docs"
run "PDFs regenerate" sh -c "cd docs && for f in overview:kings-beam-overview assembly:kings-beam-assembly shopping-list:kings-beam-shopping-list; do src=\${f%%:*}; out=\${f##*:}; \"$CHROME\" --headless=new --disable-gpu --no-pdf-header-footer --virtual-time-budget=12000 --print-to-pdf=\"\$PWD/\$out.pdf\" \"file://\$PWD/\$src.html\" >/dev/null 2>&1 && [ -s \$out.pdf ]; done"
run "guide page numbers match section order" python3 - <<'EOF'
import re, sys
s = open("docs/assembly.html").read()
secs = [m.start() for m in re.finditer(r'<section class="page', s)]
bad = 0
for m in re.finditer(r'<div class="folio"><span>Light the Beam</span><span>(\d+)</span></div>', s):
    want = sum(1 for x in secs if x <= m.start())
    if int(m.group(1)) != want: print(f"folio {m.group(1)} should be {want}"); bad += 1
steps = [int(x) for x in re.findall(r'<div class="num">(\d+)</div>', s)]
if steps != list(range(1, len(steps) + 1)): print("step numbers not contiguous:", steps); bad += 1
imgs = re.findall(r'src="(render/[^"]+)"', s)
import os
for i in imgs:
    if not os.path.exists("docs/" + i): print("missing image", i); bad += 1
print(f"{len(secs)} pages, {len(steps)} steps, {len(imgs)} images")
sys.exit(1 if bad else 0)
EOF
cat /tmp/validate.log | sed 's/^/       /'
run "external links resolve (non-Amazon)" python3 - <<'EOF'
import re, subprocess, sys
urls = set()
for f in ("README.md", "docs/bom.md", "docs/shopping-list.html"):
    urls |= set(u.rstrip('`.,') for u in re.findall(r'https?://[^\s)"<>&`]+', open(f).read()))
bad = 0
for u in sorted(urls):
    if "badge.svg" in u: continue   # exists only after the first CI run
    if any(h in u for h in ("amazon.com", "bambulab.com", "lowes.com", "homedepot.com", "bestbuy.com", "microcenter.com", "printables.com", "aliexpress", "ebay.com", "jameco.com")):
        continue   # these block bots; checked by hand
    # curl's own user agent on purpose: ESPN 403s browser-looking UAs from non-browsers
    code = subprocess.run(["curl", "-sIL", "-o", "/dev/null", "-w", "%{http_code}", "--max-time", "12", u], capture_output=True, text=True).stdout
    if not code.startswith(("2", "3")): print(f"{code} {u}"); bad += 1
print(f"{len(urls)} links, {bad} broken")
sys.exit(1 if bad else 0)
EOF
cat /tmp/validate.log | sed 's/^/       /'

echo
echo "passed $pass, failed $fail, skipped $skip"
[ $fail = 0 ]
