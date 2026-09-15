#include "beam.h"
#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

static CRGB leds[BEAM_NUM_LEDS];
static BeamMode mode = BeamMode::Off;
static uint32_t modeStart = 0;
static bool dirty = true;   // push to the strip on the next tick
static const uint8_t levels[] = {BEAM_MAX_BRIGHTNESS / 3, (BEAM_MAX_BRIGHTNESS * 2) / 3, BEAM_MAX_BRIGHTNESS};
static uint8_t levelIdx = 2;

static const CRGB PURPLE(0x5A, 0x2D, 0x81);
static const CRGB PURPLE_BRIGHT(0x9B, 0x5F, 0xD6);
static const CRGB LAVENDER(0xC9, 0xA2, 0xF0);

void beam_init() {
  FastLED.addLeds<WS2812B, BEAM_PIN, GRB>(leds, BEAM_NUM_LEDS);
  FastLED.setBrightness(levels[levelIdx]);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 900);   // leaves ~1 A for the board on a 2 A supply
  fill_solid(leds, BEAM_NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void beam_set_mode(BeamMode m) {
  if (m == mode) return;
  mode = m;
  modeStart = millis();
  dirty = true;
  log_i("beam -> %d", (int)m);
}

BeamMode beam_mode() { return mode; }
bool beam_is_lit() { return mode == BeamMode::Rise || mode == BeamMode::Hold; }

void beam_toggle() {
  beam_set_mode(beam_is_lit() ? BeamMode::Off : BeamMode::Rise);
}

void beam_cycle_brightness() {
  levelIdx = (levelIdx + 1) % 3;
  FastLED.setBrightness(levels[levelIdx]);
  dirty = true;
}

// The real beam at Golden 1 Center rises from the roof: we light LED 0 (bottom) first and
// climb, with a brighter head, then settle to solid purple.
static void renderRise(uint32_t t) {
  const uint32_t riseMs = 1800;
  if (t >= riseMs) {
    fill_solid(leds, BEAM_NUM_LEDS, PURPLE_BRIGHT);
    // gentle shimmer so it doesn't look static
    uint8_t s = beatsin8(12, 0, 40);
    for (int i = 0; i < BEAM_NUM_LEDS; i++) leds[i] = blend(PURPLE_BRIGHT, LAVENDER, s);
    if (t > riseMs + 3000) beam_set_mode(BeamMode::Hold);
    return;
  }
  float head = (float)t / riseMs * (BEAM_NUM_LEDS + 2);
  for (int i = 0; i < BEAM_NUM_LEDS; i++) {
    float d = head - i;
    if (d < 0) leds[i] = CRGB::Black;
    else if (d < 1.5f) leds[i] = LAVENDER;
    else leds[i] = PURPLE_BRIGHT;
  }
}

void beam_tick() {
  static uint32_t lastShow = 0;
  uint32_t now = millis();
  if (now - lastShow < 20) return;   // ~50 fps cap
  lastShow = now;
  uint32_t t = now - modeStart;

  // Off is static: write it once, then leave the RMT channel alone (less ISR load next to WiFi)
  if (mode == BeamMode::Off) {
    if (dirty) { fill_solid(leds, BEAM_NUM_LEDS, CRGB::Black); FastLED.show(); dirty = false; }
    return;
  }
  dirty = false;
  switch (mode) {
    case BeamMode::Off:
      break;
    case BeamMode::Pulse: {
      uint8_t b = beatsin8(6, 8, 60);   // 6 bpm, dim
      fill_solid(leds, BEAM_NUM_LEDS, PURPLE);
      nscale8(leds, BEAM_NUM_LEDS, b);
      break;
    }
    case BeamMode::Rise:
      renderRise(t);
      break;
    case BeamMode::Hold: {
      uint8_t s = beatsin8(8, 0, 30);
      for (int i = 0; i < BEAM_NUM_LEDS; i++) leds[i] = blend(PURPLE_BRIGHT, LAVENDER, s);
      break;
    }
  }
  FastLED.show();
}
