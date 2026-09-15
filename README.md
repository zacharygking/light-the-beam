# Light the Beam — Golden 1 Center with a live Kings scoreboard

A 3D-printed Golden 1 Center that lights its purple beam when the Sacramento Kings win, with
a 2.8" screen on the base showing the next game or the live score.

Based on Dave Lack's free model, [Golden 1 Center – Light The Beam](https://www.printables.com/model/338758-golden-1-center-light-the-beam),
with two changes: the beam is an addressable LED strip driven by an ESP32 instead of an
app-controlled light bar, and the arena sits on a new printed plinth that holds the display.

```
        ___________
       /  ARENA    \      stock 220 mm print, purple LED beam rising from the roof
      |   .:BEAM:.  |
       \___________/
  ____________________________
 |  [SAC 102 · LAL 98  Q4 2:31] |   2.8" screen in the plinth, one USB cable
 |____________________________|
```

## What it does

- **Next game**: opponent logo, home/away, tipoff in Pacific time, countdown.
- **Live**: score, quarter and clock, refreshed every 20 s from ESPN's public API (no key).
- **Final**: if the Kings win, the beam rises from the base and holds purple. "LIGHT THE BEAM."
- Tap the screen to light the beam yourself. Long-press cycles brightness.
- Dims at night. WiFi is set up from your phone the first time it boots.
- Solder-free: three lever-nut connections.

## Repo layout

| Path | What |
|---|---|
| `firmware/` | PlatformIO project for the ESP32-2432S028R board (LVGL UI, FastLED beam) |
| `models/` | OpenSCAD sources for the plinth, beam tube and strip spine; `printables/` is where the downloaded arena STLs go |
| `tools/` | `espn_probe.py` (check the API), `make_fixtures.py` (test data), `make_logos.py` (team logos → C header) |
| `docs/` | Bill of materials, the overview PDF, the assembly guide PDF, and `render/` (three.js scene + rendered views) |

## Quick start

1. **Buy** the parts in [`docs/bom.md`](docs/bom.md): about $41 with tax, filament separate.
2. **Flash** the board (PlatformIO; `pip3 install --user platformio` if you don't have it):
   ```
   cd firmware
   pio run -e cyd -t upload          # real firmware
   pio run -e cyd_demo -t upload     # demo: cycles NEXT → LIVE → FINAL, no WiFi needed
   pio device monitor -b 115200      # logs
   ```
   On first boot join the `KingsBeam-Setup` WiFi network from your phone and pick your home
   WiFi. The board remembers it.
3. **Print**: download the 220 mm arena base and no-hole lid from Printables into
   `models/printables/` as `base_220.stl` and `lid_220_nohole.stl`, run
   `python3 tools/measure_arena.py` (writes the footprint outline the plinth recess is cut
   to), then `cd models && make` to export the plinth, beam tube, spine and socket. Print
   `plinth_test.stl` (the console alone) first to check the board fit.
4. **Wire** the strip to the board with three WAGO 221-413 lever nuts (diagram in
   `docs/bom.md`). Board VIN → strip 5V, GND → GND, GPIO 22 → DIN.

Documents: [`docs/kings-beam-overview.pdf`](docs/kings-beam-overview.pdf) (what it is) and
[`docs/kings-beam-assembly.pdf`](docs/kings-beam-assembly.pdf) (step-by-step build guide).

## Developing

```
cd firmware
pio test -e native                 # parser tests against real ESPN fixtures
python3 ../tools/espn_probe.py     # what the board will see right now
python3 ../tools/make_fixtures.py  # refresh live/post fixtures from last season's games
python3 ../tools/make_fonts.py     # regenerate LVGL fonts from tools/fonts/*.ttf (needs Pillow)
python3 ../tools/make_logos.py     # regenerate team logos + colors from ESPN
python3 ../tools/measure_arena.py  # measure the downloaded arena STL, write models/arena_outline.scad
../tools/render_views.sh           # re-render docs/render/*.png from the three.js scene (headless Chrome)
```

The PDFs are printed from `docs/overview.html` and `docs/assembly.html` with headless Chrome:

```
cd docs && "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new \
  --no-pdf-header-footer --virtual-time-budget=10000 --print-to-pdf="$PWD/kings-beam-assembly.pdf" "file://$PWD/assembly.html"
```

Board notes: the ESP32-2432S028R comes in two variants. If colors look inverted, switch the
TFT driver flags in `firmware/platformio.ini` (comment there). If the screen is upside down,
change `LV_DISPLAY_ROTATION_90` to `_270` in `firmware/src/ui.cpp`.

## Data source

`https://site.api.espn.com/apis/site/v2/sports/basketball/nba/teams/sac` → `team.nextEvent[0]`
carries the next or in-progress game, its status, scores and winner. One gotcha found while
building this: ESPN's edge returns **403 to browser-looking user agents sent by non-browser
clients**. The stock `ESP32HTTPClient` and `python-urllib` user agents work; do not spoof
a browser UA.

## Status

- Firmware builds (`cyd` and `cyd_demo`), parser tested against real fixtures. Not yet run on
  hardware: display rotation, touch threshold and the first-LED data level are the things most
  likely to need a tweak.
- Models are parametric OpenSCAD, exported and previewed with OpenSCAD 2021.01. The plinth recess is
  cut to the measured arena footprint (`tools/measure_arena.py`); the wire slot sits under the
  arena's built-in channel exit at the back right.
- Preseason opener is Oct 5, 2026 (LAL @ SAC); the live and final screens get their first
  real data then. `tools/make_fixtures.py` already exercised them with last season's games.
