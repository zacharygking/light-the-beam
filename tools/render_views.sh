#!/bin/sh
# Render the named views of docs/render/scene.html to PNGs with headless Chrome (software WebGL).
#   tools/render_views.sh            # all views
#   tools/render_views.sh hero       # one view
set -e
cd "$(dirname "$0")/.."
CHROME="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
OUT=docs/render
VIEWS="${*:-hero screen exploded beam strip cutaway wiring lid plinth_top}"

for v in $VIEWS; do
  case $v in
    hero) W=1400; H=800 ;;
    exploded) W=1000; H=1000 ;;
    *) W=1000; H=700 ;;
  esac
  "$CHROME" --headless=new --use-angle=swiftshader --enable-unsafe-swiftshader --ignore-gpu-blocklist \
    --hide-scrollbars --virtual-time-budget=10000 --window-size=$W,$H \
    --screenshot="$PWD/$OUT/$v.png" "file://$PWD/$OUT/scene.html?view=$v&w=$W&h=$H" 2>/dev/null
  echo "rendered $OUT/$v.png"
done
