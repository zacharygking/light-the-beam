// Kings "Light the Beam" scoreboard — build-time configuration
#pragma once

// ---- Team --------------------------------------------------------------------
#define TEAM_ABBR        "SAC"
#define TEAM_SLUG        "sac"          // ESPN team slug in the URL
#define ESPN_URL         "https://site.api.espn.com/apis/site/v2/sports/basketball/nba/teams/" TEAM_SLUG
// ESPN's edge (Akamai) returns 403 to browser-looking user agents from non-browser clients.
// The stock ESP32 HTTPClient UA passes, so we send that and never spoof a browser.
#define ESPN_USER_AGENT  "ESP32HTTPClient"

// ---- Time --------------------------------------------------------------------
#define TZ_POSIX         "PST8PDT,M3.2.0,M11.1.0"   // America/Los_Angeles
#define NTP_SERVER       "pool.ntp.org"

// ---- Poll cadence (ms) -------------------------------------------------------
#define POLL_PRE_MS          (10UL * 60UL * 1000UL)  // next game far away
#define POLL_PRE_SOON_MS     (30UL * 1000UL)         // within PRE_SOON_WINDOW of tipoff
#define POLL_LIVE_MS         (20UL * 1000UL)
#define POLL_POST_MS         (5UL * 60UL * 1000UL)
#define POLL_ERROR_MS        (60UL * 1000UL)
#define PRE_SOON_WINDOW_S    (20L * 60L)             // seconds before tipoff to speed up
#define HTTP_TIMEOUT_MS      15000

// ---- Beam (WS2812B strip) ----------------------------------------------------
#define BEAM_PIN             22      // CYD connector CN1: GND, IO22, IO27, 3V3
#define BEAM_NUM_LEDS        15
#define BEAM_MAX_BRIGHTNESS  128     // 0-255; caps current so a 2 A USB charger is plenty
#define BEAM_HOLD_HOURS      12      // stay lit this long after a win (or until tapped off)
#define BEAM_PURPLE          0x5A2D81
#define BEAM_PURPLE_BRIGHT   0x8A4DD0

// ---- Display / touch (CYD) ---------------------------------------------------
#define SCREEN_W             320
#define SCREEN_H             240
#define TFT_BL_PIN           21
#define TOUCH_CLK            25
#define TOUCH_MOSI           32
#define TOUCH_MISO           39
#define TOUCH_CS             33
#define TOUCH_IRQ            36
#define LDR_PIN              34      // onboard light sensor (higher ADC = darker)

// ---- Night dimming -----------------------------------------------------------
#define NIGHT_START_HOUR     23
#define NIGHT_END_HOUR       7
#define BL_DAY               255
#define BL_NIGHT             25

// ---- WiFi portal -------------------------------------------------------------
#define WIFI_PORTAL_SSID     "KingsBeam-Setup"
