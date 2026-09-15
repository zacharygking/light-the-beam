#include "touch.h"
#include <Arduino.h>
#include <SPI.h>
#include "config.h"

// The CYD's XPT2046 sits on its own SPI pins (CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36).
// We only need "is a finger down", so we talk to the chip directly: ~20 lines beats a
// library dependency. Pressure = Z1 + 4095 - Z2, from the 8-bit control bytes below.
static SPIClass touchSpi(VSPI);
static std::function<void()> onTap, onLong;

static bool wasDown = false;
static uint32_t downAt = 0;
static bool longFired = false;
static const uint32_t LONG_MS = 800;
static const uint32_t DEBOUNCE_MS = 40;
static const int Z_THRESHOLD = 400;
static const int RELEASE_SAMPLES = 3;   // x 15 ms poll = 45 ms

static uint16_t readChannel(uint8_t cmd) {
  touchSpi.transfer(cmd);
  uint16_t v = touchSpi.transfer16(0x00);
  return v >> 3;   // 12-bit result left-aligned in 16 bits
}

static bool pressed() {
  if (digitalRead(TOUCH_IRQ) == HIGH) return false;   // PENIRQ is active low
  touchSpi.beginTransaction(SPISettings(2500000, MSBFIRST, SPI_MODE0));
  digitalWrite(TOUCH_CS, LOW);
  int z = 0;
  for (int i = 0; i < 2; i++) {          // average two reads; resistive panels are noisy
    int z1 = readChannel(0xB1);          // Z1, differential, power on
    int z2 = readChannel(0xC1);          // Z2
    z += z1 + 4095 - z2;
  }
  readChannel(0xD0);                     // dummy X read with power-down so PENIRQ re-arms
  digitalWrite(TOUCH_CS, HIGH);
  touchSpi.endTransaction();
  return z / 2 > Z_THRESHOLD;
}

void touch_init() {
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(TOUCH_IRQ, INPUT);
  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
}

void touch_set_tap(std::function<void()> cb) { onTap = cb; }
void touch_set_long_press(std::function<void()> cb) { onLong = cb; }

void touch_tick() {
  static uint32_t lastPoll = 0;
  uint32_t now = millis();
  if (now - lastPoll < 15) return;
  lastPoll = now;

  // a release needs RELEASE_SAMPLES consecutive "up" reads: one pressure dropout mid-press
  // would otherwise register as release + new press = a double tap
  static int upCount = 0;
  bool raw = pressed();
  if (raw) upCount = 0; else if (upCount < RELEASE_SAMPLES) upCount++;
  bool down = raw || (wasDown && upCount < RELEASE_SAMPLES);
  if (down && !wasDown) {
    downAt = now;
    longFired = false;
  } else if (down && wasDown && !longFired && now - downAt > LONG_MS) {
    longFired = true;
    if (onLong) onLong();
  } else if (!down && wasDown) {
    uint32_t held = now - downAt;
    if (!longFired && held > DEBOUNCE_MS && onTap) onTap();
  }
  wasDown = down;
}
