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
| `docs/` | Bill of materials with links and the overview PDF |

## Quick start

1. Buy the parts in `docs/bom.md` (about $41–50 shipped to SF, filament separate).
2. Print: arena base + lid (220 mm) from Printables, plus `models/exports/*.stl`.
3. Flash: `cd firmware && pio run -t upload`, then join the `KingsBeam-Setup` WiFi network
   from your phone and enter your home WiFi.
4. Wire the strip to the board with three WAGO 221-413 lever nuts (see `docs/bom.md`).

## Data source

`https://site.api.espn.com/apis/site/v2/sports/basketball/nba/teams/sac` → `team.nextEvent[0]`
carries the next or in-progress game, its status, scores and winner. One gotcha found while
building this: ESPN's edge returns **403 to browser-looking user agents sent by non-browser
clients**. The stock `ESP32HTTPClient` and `python-urllib` user agents work; do not spoof
a browser UA.

## Status

Work in progress. See `docs/` for the plan and BOM.
