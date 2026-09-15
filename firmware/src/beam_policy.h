// What the beam should do given the game state. Pure function so it can be unit-tested on the
// host; main.cpp applies the result.
#pragma once
#include <stdint.h>
#include <string.h>
#include "game_state.h"

enum class BeamMode : uint8_t {
  Off,        // dark
  Pulse,      // slow dim purple breathing (during a live game)
  Rise,       // the win animation: purple climbs from the base, then holds
  Hold,       // solid purple
};

inline bool beamModeLit(BeamMode m) { return m == BeamMode::Rise || m == BeamMode::Hold; }

struct BeamDecision {
  BeamMode mode;      // mode to apply (may equal the current one)
  bool celebrate;     // true when this decision starts a new win celebration
};

// cur: the beam's current mode. prev: the game state last applied (eventId "" if none).
// gs: the freshly fetched state. celebrated: eventId of the last celebrated win (persisted).
// now: epoch seconds, or 0 if the clock is not synced. holdHours: BEAM_HOLD_HOURS.
inline BeamDecision beamPolicy(BeamMode cur, const GameState& prev, const GameState& gs,
                               const char* celebrated, int64_t now, int holdHours) {
  BeamDecision d{cur, false};
  bool newEvent = strcmp(prev.eventId, gs.eventId) != 0;
  switch (gs.status) {
    case GameStatus::Live:
      if (!beamModeLit(cur)) d.mode = BeamMode::Pulse;   // a manually lit beam stays lit
      break;
    case GameStatus::Post:
      if (gs.weWon()) {
        bool fresh = (now == 0 || gs.tipoffEpoch == 0) ||
                     (now - gs.tipoffEpoch) < (int64_t)holdHours * 3600 + 4 * 3600;   // game + hold window
        if (strcmp(celebrated, gs.eventId) != 0 && fresh) { d.mode = BeamMode::Rise; d.celebrate = true; }
      } else if (cur == BeamMode::Pulse) {
        d.mode = BeamMode::Off;
      }
      break;
    case GameStatus::Pre:
      if (newEvent && (beamModeLit(cur) || cur == BeamMode::Pulse)) d.mode = BeamMode::Off;
      break;
    default:   // None, Postponed
      if (cur == BeamMode::Pulse) d.mode = BeamMode::Off;
      break;
  }
  return d;
}

// How long to wait before the next ESPN poll, in ms.
inline uint32_t pollIntervalMs(const GameState& gs, int64_t now, uint32_t preMs, uint32_t preSoonMs,
                               uint32_t liveMs, uint32_t postMs, int64_t soonWindowS) {
  switch (gs.status) {
    case GameStatus::Live: return liveMs;
    case GameStatus::Post: return postMs;
    case GameStatus::Pre: {
      if (gs.tipoffEpoch && now) {
        int64_t delta = gs.tipoffEpoch - now;
        // near tipoff, but stop hammering if the tipoff time is stale by more than 3 h
        if (delta < soonWindowS && delta > -3 * 3600) return preSoonMs;
      }
      return preMs;
    }
    default: return preMs;
  }
}
