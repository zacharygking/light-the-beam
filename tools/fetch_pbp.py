#!/usr/bin/env python3
"""Download NBA play-by-play from ESPN for the win-probability model (README, "Ideas for v2").

Two steps, both resumable:
  list   scoreboard for a season's date range -> data/games/<season>.jsonl (one completed game per line)
  fetch  summary for each listed game -> data/raw/<id>.json.gz (cache) and
         data/pbp/<season>.jsonl.gz  (one compact row per play)

Row fields: game, date, season, type (2 regular, 3 playoffs, 5 play-in), home, away, period,
secs_left (in the period), secs_left_reg (regulation clock, 0 in OT), home_score,
away_score, home_won, espn_wp (ESPN's home win probability after the play, or null).

Usage:
  python3 tools/fetch_pbp.py list  2025 --dates 20241001-20250630
  python3 tools/fetch_pbp.py fetch 2025
Season is named by the year it ends (2025 = 2024-25); --dates is walked one month at a time. ESPN's edge only answers
non-browser clients whose User-Agent is not browser-like; the stock urllib one works.
"""
import argparse
import gzip
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
UA = "python-urllib/3.9"
BASE = "https://site.api.espn.com/apis/site/v2/sports/basketball/nba"
DELAY = 0.3  # seconds between requests
# ESPN abbreviations of the 30 teams; filters out the All-Star game, which ESPN files as regular season.
TEAMS = set("ATL BOS BKN CHA CHI CLE DAL DEN DET GS HOU IND LAC LAL MEM MIA MIL MIN NO NY OKC ORL PHI PHX POR SAC SA TOR UTAH WSH".split())


def get(url, tries=6):
    for i in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=40) as r:
                return json.load(r)
        except (OSError, json.JSONDecodeError) as e:  # URLError, HTTPError and socket.timeout are all OSError
            if i == tries - 1:
                raise
            time.sleep(2 * (i + 1))
            print(f"  retry {i + 1}: {e}", file=sys.stderr)


def clock_secs(s):
    """'4:50' -> 290, '57.4' -> 57.4, '0:03.7' -> 3.7, '' -> 0. Uses only the last two fields."""
    parts = s.split(":") if s else []
    sec = float(parts[-1]) if parts else 0.0
    mins = int(parts[-2]) if len(parts) >= 2 else 0
    return mins * 60 + sec


def cmd_list(season, dates):
    out = DATA / "games" / f"{season}.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    # ESPN hangs on a whole-season range, so walk it one calendar month at a time.
    start, end = dates.split("-")
    y, m = int(start[:4]), int(start[4:6])
    events = []
    while f"{y}{m:02d}01" <= end:
        last = 31 if m in (1, 3, 5, 7, 8, 10, 12) else 30 if m != 2 else 29
        sb = get(f"{BASE}/scoreboard?dates={y}{m:02d}01-{y}{m:02d}{last}&limit=1000")
        events += sb.get("events", [])
        time.sleep(DELAY)
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    events = list({e["id"]: e for e in events}.values())
    kept, skipped = [], {}
    for e in events:
        st = e["season"]["type"]
        comp = e["competitions"][0]
        if not e["status"]["type"]["completed"] or st not in (2, 3, 5):
            skipped[st] = skipped.get(st, 0) + 1
            continue
        home = next(c for c in comp["competitors"] if c["homeAway"] == "home")
        away = next(c for c in comp["competitors"] if c["homeAway"] == "away")
        if home["team"]["abbreviation"] not in TEAMS or away["team"]["abbreviation"] not in TEAMS:
            skipped["all-star"] = skipped.get("all-star", 0) + 1
            continue
        kept.append({
            "game": e["id"], "date": e["date"], "season": season, "type": st,
            "home": home["team"]["abbreviation"], "away": away["team"]["abbreviation"],
            "home_won": bool(home.get("winner", False)),
            "home_score": int(home["score"]), "away_score": int(away["score"]),
        })
    kept.sort(key=lambda g: g["date"])
    out.write_text("".join(json.dumps(g) + "\n" for g in kept))
    print(f"{season}: {len(kept)} completed games written to {out.relative_to(ROOT)}; skipped by season type: {skipped}")


def rows_for_game(g, summary):
    wp = {w["playId"]: w.get("homeWinPercentage") for w in summary.get("winprobability", [])}
    rows = []
    for p in summary.get("plays", []):
        period = p["period"]["number"]
        left = clock_secs(p.get("clock", {}).get("displayValue", ""))
        reg_left = max(0.0, (4 - period) * 720 + left) if period <= 4 else 0.0
        rows.append({
            "game": g["game"], "date": g["date"][:10], "season": g["season"], "type": g["type"],
            "home": g["home"], "away": g["away"], "period": period,
            "secs_left": round(left, 1), "secs_left_reg": round(reg_left, 1),
            "home_score": p.get("homeScore", 0), "away_score": p.get("awayScore", 0),
            "home_won": g["home_won"], "espn_wp": wp.get(p["id"]),
        })
    return rows


def cmd_fetch(season):
    games = [json.loads(l) for l in (DATA / "games" / f"{season}.jsonl").read_text().splitlines()]
    raw = DATA / "raw"
    raw.mkdir(parents=True, exist_ok=True)
    out = DATA / "pbp" / f"{season}.jsonl.gz"
    out.parent.mkdir(parents=True, exist_ok=True)
    n_rows, n_new, empty = 0, 0, []
    t0 = time.time()
    with gzip.open(out, "wt") as f:
        for i, g in enumerate(games, 1):
            cache = raw / f"{g['game']}.json.gz"
            if cache.exists():
                with gzip.open(cache, "rt") as c:
                    summary = json.load(c)
            else:
                full = get(f"{BASE}/summary?event={g['game']}")
                summary = {"plays": full.get("plays", []), "winprobability": full.get("winprobability", [])}
                with gzip.open(cache, "wt") as c:
                    json.dump(summary, c)
                n_new += 1
                time.sleep(DELAY)
            rows = rows_for_game(g, summary)
            if not rows:
                empty.append(g["game"])
            for r in rows:
                f.write(json.dumps(r) + "\n")
            n_rows += len(rows)
            if i % 100 == 0 or i == len(games):
                print(f"{season}: {i}/{len(games)} games, {n_rows} plays, {n_new} downloaded, {time.time() - t0:.0f}s", flush=True)
    print(f"{season}: wrote {out.relative_to(ROOT)} ({out.stat().st_size / 1e6:.1f} MB); games with no plays: {len(empty)} {empty[:10]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("list"); a.add_argument("season", type=int); a.add_argument("--dates", required=True)
    b = sub.add_parser("fetch"); b.add_argument("season", type=int)
    args = ap.parse_args()
    if args.cmd == "list":
        cmd_list(args.season, args.dates)
    else:
        cmd_fetch(args.season)


if __name__ == "__main__":
    main()
