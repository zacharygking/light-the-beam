// The beam: a short WS2812B strip inside the diffuser tube.
#pragma once
#include <stdint.h>

enum class BeamMode : uint8_t {
  Off,        // dark
  Pulse,      // slow dim purple breathing (during a live game)
  Rise,       // the win animation: purple climbs from the base, then holds
  Hold,       // solid purple
};

void beam_init();
void beam_set_mode(BeamMode m);
BeamMode beam_mode();
bool beam_is_lit();            // Rise or Hold
void beam_toggle();            // manual: Off <-> Rise
void beam_cycle_brightness();  // 3 levels, wraps
void beam_tick();              // call from loop(); non-blocking
