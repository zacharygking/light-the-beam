// Plain data describing the one game the display cares about. No Arduino dependencies so
// the parser can be unit-tested on the host.
#pragma once
#include <stdint.h>

enum class GameStatus : uint8_t { None = 0, Pre, Live, Post, Postponed };

struct TeamSide {
  char abbr[5]   = "";     // "SAC"
  char name[24]  = "";     // "Kings"
  int  score     = -1;     // -1 until the game starts
  bool home      = false;
  bool winner    = false;
};

struct GameState {
  GameStatus status = GameStatus::None;
  char eventId[16]  = "";
  char dateUtc[24]  = "";   // "2026-10-06T02:00Z"
  int64_t tipoffEpoch = 0;  // derived from dateUtc, seconds since 1970 UTC (0 if unparsed)
  char shortName[16] = "";  // "LAL @ SAC"
  char detail[40]   = "";   // "10/5 - 10:00 PM EDT" / "2:31 - 4th" / "Final"
  char clock[8]     = "";   // "2:31"
  int  period       = 0;    // 1-4, 5+ = OT
  bool completed    = false;
  TeamSide us;              // our team (TEAM_ABBR)
  TeamSide them;            // the opponent
  bool valid        = false;

  bool weWon() const { return status == GameStatus::Post && us.winner; }
  bool weAreHome() const { return us.home; }
};
