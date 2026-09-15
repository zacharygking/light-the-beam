// LVGL scoreboard UI for the 320x240 CYD screen.
#pragma once
#include <time.h>
#include "game_state.h"

void ui_init();                                  // lv_init, display, first screen
void ui_show_boot(const char* msg);
void ui_show_wifi_setup(const char* portalSsid); // captive-portal instructions
void ui_show_game(const GameState& gs, bool beamLit);
void ui_set_offline(bool offline);               // small badge, keeps the last screen
void ui_tick(time_t now);                        // countdowns, blink; call every loop
void ui_set_backlight(uint8_t level);            // 0-255
