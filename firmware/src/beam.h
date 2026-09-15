// The beam: a short WS2812B strip inside the diffuser tube.
#pragma once
#include <stdint.h>
#include "beam_policy.h"   // BeamMode

void beam_init();
void beam_set_mode(BeamMode m);
BeamMode beam_mode();
bool beam_is_lit();            // Rise or Hold
void beam_toggle();            // manual: Off <-> Rise
void beam_cycle_brightness();  // 3 levels, wraps
void beam_tick();              // call from loop(); non-blocking
