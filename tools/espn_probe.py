#!/usr/bin/env python3
"""Probe ESPN's public Kings endpoint and print what the beam firmware will see.

Usage:
  python3 tools/espn_probe.py            # print parsed state
  python3 tools/espn_probe.py --save pre # also save raw JSON to firmware/test/fixtures/pre.json
  python3 tools/espn_probe.py --team lal # another team, for testing

No API key needed. Run this from a normal shell (not a sandbox) — ESPN's CDN rejects some
proxy egress with 403.
"""
import argparse
import json
import sys
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

URL = "https://site.api.espn.com/apis/site/v2/sports/basketball/nba/teams/{team}"
# ESPN's edge (Akamai) returns 403 for browser-looking or unfamiliar user agents sent by
# non-browser clients; the stock library UAs ("python-urllib/3.9", "ESP32HTTPClient") pass.
# So: send the client's default UA and never spoof a browser. Same rule applies in firmware.
UA = "ESP32HTTPClient"
FIXTURES = Path(__file__).resolve().parent.parent / "firmware" / "test" / "fixtures"


def fetch(team: str) -> dict:
    req = urllib.request.Request(URL.format(team=team), headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=15) as r:
        raw = r.read()
    print(f"fetched {len(raw):,} bytes", file=sys.stderr)
    return json.loads(raw)


def parse(doc: dict, us: str) -> dict:
    """Mirror of the ArduinoJson filter in firmware/src/kings_api.cpp."""
    team = doc["team"]
    events = team.get("nextEvent") or []
    if not events:
        return {"state": "none", "team": team["abbreviation"]}
    ev = events[0]
    comp = ev["competitions"][0]
    status = comp["status"]
    out = {
        "event_id": ev["id"],
        "date_utc": ev["date"],
        "short_name": ev.get("shortName"),
        "state": status["type"]["state"],          # pre | in | post
        "completed": status["type"]["completed"],
        "detail": status["type"].get("shortDetail"),
        "clock": status.get("displayClock"),
        "period": status.get("period"),
        "teams": [],
    }
    for c in comp["competitors"]:
        t = c["team"]
        out["teams"].append({
            "abbr": t["abbreviation"],
            "name": t.get("shortDisplayName"),
            "color": t.get("color"),
            "home_away": c.get("homeAway"),
            "score": c.get("score"),
            "winner": c.get("winner"),
            "is_us": t["abbreviation"].lower() == us.lower(),
        })
    return out


def describe(p: dict) -> str:
    if p["state"] == "none":
        return "No upcoming game listed."
    us = next(t for t in p["teams"] if t["is_us"])
    them = next(t for t in p["teams"] if not t["is_us"])
    vs = "vs" if us["home_away"] == "home" else "@"
    when = datetime.fromisoformat(p["date_utc"].replace("Z", "+00:00"))
    local = when.astimezone()  # Mac's local zone
    if p["state"] == "pre":
        return f"NEXT: {us['abbr']} {vs} {them['abbr']}  {local:%a %b %-d %-I:%M %p}  ({p['detail']})"
    if p["state"] == "in":
        return f"LIVE: {us['abbr']} {us['score']} - {them['abbr']} {them['score']}  P{p['period']} {p['clock']}"
    won = us.get("winner")
    tag = "KINGS WIN — LIGHT THE BEAM" if won else "Final"
    return f"FINAL: {us['abbr']} {us['score']} - {them['abbr']} {them['score']}  {tag}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--team", default="sac")
    ap.add_argument("--save", metavar="NAME", help="save raw JSON as firmware/test/fixtures/NAME.json")
    args = ap.parse_args()

    doc = fetch(args.team)
    parsed = parse(doc, args.team)
    print(json.dumps(parsed, indent=2))
    print()
    print(describe(parsed))

    if args.save:
        FIXTURES.mkdir(parents=True, exist_ok=True)
        path = FIXTURES / f"{args.save}.json"
        path.write_text(json.dumps(doc, separators=(",", ":")))
        print(f"\nsaved raw response to {path.relative_to(Path.cwd()) if path.is_relative_to(Path.cwd()) else path}")


if __name__ == "__main__":
    main()
