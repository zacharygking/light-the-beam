// Resistive touch on the CYD (XPT2046 on its own SPI bus). We only need gestures, not
// coordinates: a tap and a long press.
#pragma once
#include <functional>

void touch_init();
void touch_set_tap(std::function<void()> cb);
void touch_set_long_press(std::function<void()> cb);
void touch_tick();
