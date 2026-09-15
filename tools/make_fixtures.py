#!/usr/bin/env python3
"""Build test fixtures for the firmware parser from real, completed Kings games.

The firmware reads ESPN's team endpoint (team.nextEvent[0]). Out of season that only ever
shows a "pre" game, so this script pulls completed games from the scoreboard endpoint
(same event schema) and wraps them in the team-endpoint shape:

  firmware/test/fixtures/post_win.json   real completed game the Kings won
  firmware/test/fixtures/post_loss.json  real completed game the Kings lost
  firmware/test/fixtures/live.json       post_win with status rewritten to "in" (synthetic,
                                         marked with "_synthetic": true)

Usage: python3 tools/make_fixtures.py [--from 20260401] [--to 20260415]
"""
import argparse
import copy
import json
import sys
import urllib.request
from datetime import date, timedelta
from pathlib import Path

SCOREBOARD = "https://site.api.espn.com/apis/site/v2/sports/basketball/nba/scoreboard?dates={d}"
UA = "ESP32HTTPClient"  # see espn_probe.py: browser-like UAs get 403 from non-browser clients
FIXTURES = Path(__file__).resolve().parent.parent / "firmware" / "test" / "fixtures"
TEAM = "SAC"


def get(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.loads(r.read())


def kings_games(d0: date, d1: date):
    d = d0
    while d <= d1:
        doc = get(SCOREBOARD.format(d=d.strftime("%Y%m%d")))
        for ev in doc.get("events", []):
            comp = ev["competitions"][0]
            abbrs = [c["team"]["abbreviation"] for c in comp["competitors"]]
            if TEAM in abbrs and comp["status"]["type"]["state"] == "post":
                yield ev
        d += timedelta(days=1)


def wrap(ev: dict, synthetic: bool = False) -> dict:
    """Shape the event like GET /teams/sac -> {"team": {..., "nextEvent": [ev]}}."""
    doc = {"team": {"id": "23", "abbreviation": TEAM, "displayName": "Sacramento Kings",
                    "color": "5a2d81", "nextEvent": [ev]}}
    if synthetic:
        doc["_synthetic"] = True
    return doc


def us_won(ev: dict) -> bool:
    for c in ev["competitions"][0]["competitors"]:
        if c["team"]["abbreviation"] == TEAM:
            return bool(c.get("winner"))
    return False


def make_live(ev: dict) -> dict:
    live = copy.deepcopy(ev)
    comp = live["competitions"][0]
    comp["status"] = {
        "clock": 151.0, "displayClock": "2:31", "period": 4,
        "type": {"id": "2", "name": "STATUS_IN_PROGRESS", "state": "in", "completed": False,
                 "description": "In Progress", "detail": "2:31 - 4th Quarter", "shortDetail": "2:31 - 4th"},
    }
    for c in comp["competitors"]:
        c.pop("winner", None)
        # trim the final score so it reads like a close 4th quarter
        c["score"] = str(max(0, int(c["score"]) - 10))
    return live


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="d0", default="20260401")
    ap.add_argument("--to", dest="d1", default="20260415")
    args = ap.parse_args()
    d0 = date(int(args.d0[:4]), int(args.d0[4:6]), int(args.d0[6:]))
    d1 = date(int(args.d1[:4]), int(args.d1[4:6]), int(args.d1[6:]))

    win = loss = None
    for ev in kings_games(d0, d1):
        print(f"{ev['date']}  {ev['shortName']}  won={us_won(ev)}", file=sys.stderr)
        if us_won(ev) and win is None:
            win = ev
        elif not us_won(ev) and loss is None:
            loss = ev
        if win and loss:
            break
    if not win or not loss:
        sys.exit("need both a win and a loss in the date range; widen --from/--to")

    FIXTURES.mkdir(parents=True, exist_ok=True)
    (FIXTURES / "post_win.json").write_text(json.dumps(wrap(win), separators=(",", ":")))
    (FIXTURES / "post_loss.json").write_text(json.dumps(wrap(loss), separators=(",", ":")))
    (FIXTURES / "live.json").write_text(json.dumps(wrap(make_live(win), synthetic=True), separators=(",", ":")))
    for n in ("post_win", "post_loss", "live"):
        print(f"wrote {n}.json ({(FIXTURES / (n + '.json')).stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
