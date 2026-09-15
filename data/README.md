# Play-by-play data for the win-probability model

Downloaded from ESPN's public NBA endpoints by `tools/fetch_pbp.py` on 2026-09-15 for the
live win-probability idea in the README ("Ideas for v2"). Training on this costs the
scoreboard nothing: the model's inputs are fields the firmware already polls.

| File | What | Size |
|---|---|---|
| `games/<season>.jsonl` | One completed game per line: id, date, type, home/away, final score, winner | ~200 KB each |
| `pbp/<season>.jsonl.gz` | One row per play with the running score and clock, plus ESPN's win probability | ~5 MB each |
| `raw/<id>.json.gz` | Per-game cache of ESPN's `plays` and `winprobability` arrays. Gitignored; regenerable | ~40 KB each |

Season is named by the year it ends: `2025` is 2024-25. Each season has the 1,230 regular-season
games plus the NBA Cup final (type 2), the play-in (type 5) and the playoffs (type 3). Preseason
and the All-Star game are excluded.

## Row format (`pbp/`)

```json
{"game": "401705663", "date": "2025-04-01", "season": 2025, "type": 2, "home": "ATL", "away": "POR",
 "period": 2, "secs_left": 290.0, "secs_left_reg": 1730.0, "home_score": 43, "away_score": 46,
 "home_won": true, "espn_wp": 0.609}
```

- `secs_left` is the clock within the period; `secs_left_reg` is the regulation clock (0 in overtime).
- `espn_wp` is ESPN's home win probability after the play, present on every row. It is the
  benchmark a homegrown model has to beat, or at least match, on calibration and Brier score.

## Known quirks

- ESPN issues scoring corrections in-feed: a basket credited to the wrong team is fixed a play
  or two later, so a team's score decreases in about 200 plays per season (147 games in 2024-25,
  173 in 2025-26). Filter rows whose score is below the running maximum, or ignore it.
- A handful of games per season end a free throw short of the listed final (5 in 2024-25, 11 in 2025-26).
  The `games/` file carries the official final and winner; use that for the label.
- Play-in and playoff games have no home-court structure change but are labelled by `type`
  so they can be held out.

## Refresh

```sh
python3 tools/fetch_pbp.py list 2027 --dates 20261001-20270630
python3 tools/fetch_pbp.py fetch 2027
```

Both steps resume from what is already on disk. Roughly 0.6 s per game.
