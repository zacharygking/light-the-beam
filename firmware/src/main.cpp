// Kings "Light the Beam" scoreboard — main loop.
//
//   boot → WiFi (captive portal on first run) → NTP → poll ESPN → drive screen + beam
//
// Build with -DDEMO_MODE to skip WiFi and cycle NEXT → LIVE → FINAL from canned data.
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"
#include "game_state.h"
#include "beam_policy.h"
#include "kings_api.h"
#include "ui.h"
#include "winprob_train.h"
#include "beam.h"
#include "touch.h"

// TLS handshakes and LVGL's software renderer both run on the Arduino loop task; the default
// 8 KB stack is the usual overflow on CYD + LVGL 9 builds.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

#ifndef DEMO_MODE
#include <WiFiManager.h>
static WiFiManager wm;
static bool portalActive = false;
static uint32_t noWifiSince = 0;              // millis() of the first consecutive NoWifi result
static const uint32_t PORTAL_TIMEOUT_S = 180;  // portal closes itself after this
static const uint32_t REOPEN_PORTAL_MS = 10UL * 60UL * 1000UL;   // no WiFi this long → offer setup again
#endif

static Preferences prefs;
static GameState game;
static bool haveGame = false;
static bool offline = false;
static uint32_t nextPollAt = 0;
static uint32_t beamLitAt = 0;
static char celebratedEvent[16] = "";
static bool timeSynced = false;

static int64_t nowEpoch() {
  time_t t = time(nullptr);
  return t > 1600000000 ? (int64_t)t : 0;   // 0 = clock not synced yet
}

// ---- game state → screen + beam --------------------------------------------------
static void applyGame(const GameState& gs) {
  int64_t now = nowEpoch();
  BeamDecision d = beamPolicy(beam_mode(), game, gs, celebratedEvent, now, BEAM_HOLD_HOURS);
  game = gs;
  haveGame = true;

  if (d.celebrate) {
    strncpy(celebratedEvent, gs.eventId, sizeof celebratedEvent - 1);
    prefs.putString("celeb", celebratedEvent);   // survives a reboot: no encore after a power blip
    beamLitAt = millis();
    log_i("KINGS WIN %d-%d — light the beam", gs.us.score, gs.them.score);
  }
  if (d.mode != beam_mode()) beam_set_mode(d.mode);

  ui_show_game(game, beam_is_lit());
  nextPollAt = millis() + pollIntervalMs(game, now, POLL_PRE_MS, POLL_PRE_SOON_MS, POLL_LIVE_MS, POLL_POST_MS, PRE_SOON_WINDOW_S);
}

static void poll() {
#ifndef DEMO_MODE
  GameState gs;
  int code = 0;
  if (!haveGame) { ui_show_boot("FETCHING SCHEDULE"); ui_flush(); }
  kings::FetchResult r = kings::fetch(gs, &code);
  log_i("fetch: %s (http %d) heap %u", kings::fetchResultName(r), code, ESP.getFreeHeap());
  if (r == kings::FetchResult::Ok) {
    offline = false;
    noWifiSince = 0;
    applyGame(gs);
  } else {
    offline = true;
    if (r == kings::FetchResult::NoWifi) { if (!noWifiSince) noWifiSince = millis(); }
    else noWifiSince = 0;
    if (!haveGame) {
      ui_show_boot(r == kings::FetchResult::NoWifi ? "NO WIFI \xC2\xB7 RETRYING" : "ESPN UNREACHABLE \xC2\xB7 RETRYING");
      ui_flush();
    }
    nextPollAt = millis() + POLL_ERROR_MS;
  }
  ui_set_offline(offline);
#endif
}

// ---- touch -------------------------------------------------------------------
static void onTap() {
  beam_toggle();
  beamLitAt = millis();
  if (haveGame) ui_show_game(game, beam_is_lit());
}
static void onLongPress() { beam_cycle_brightness(); }

// ---- night dimming -----------------------------------------------------------
static void nightTick() {
  static uint32_t last = 0;
  if (millis() - last < 30000) return;
  last = millis();
  if (!nowEpoch()) return;   // clock not set yet
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  bool night = (NIGHT_START_HOUR > NIGHT_END_HOUR)
                   ? (lt.tm_hour >= NIGHT_START_HOUR || lt.tm_hour < NIGHT_END_HOUR)
                   : (lt.tm_hour >= NIGHT_START_HOUR && lt.tm_hour < NIGHT_END_HOUR);
  // the CYD's LDR reads higher in the dark; blend it in so a dark room also dims
  int ldr = analogRead(LDR_PIN);
  bool darkRoom = ldr > 3000;
  ui_set_backlight(night || darkRoom ? BL_NIGHT : BL_DAY);
}

// ---- demo mode ---------------------------------------------------------------
#ifdef DEMO_MODE
static void demoTick() {
  static uint32_t phaseStart = 0;
  static int phase = -1;
  static GameState d;
  uint32_t now = millis();
  int wanted = ((now / 9000) % 3);
  if (wanted != phase) {
    phase = wanted;
    phaseStart = now;
    d = GameState{};
    strcpy(d.eventId, phase == 0 ? "demo-pre" : "demo-game");
    strcpy(d.us.abbr, TEAM_ABBR); strcpy(d.us.name, "Kings"); d.us.home = true;
    strcpy(d.them.abbr, "LAL"); strcpy(d.them.name, "Lakers");
    strcpy(d.dateUtc, "2026-10-06T02:00Z"); d.tipoffEpoch = 1791252000LL;
    strcpy(d.shortName, "LAL @ SAC");
    d.valid = true;
    if (phase == 0) { d.status = GameStatus::Pre; strcpy(d.detail, "10/5 - 7:00 PM"); }
    if (phase == 1) { d.status = GameStatus::Live; d.us.score = 98; d.them.score = 96; d.period = 4; strcpy(d.clock, "2:31"); strcpy(d.detail, "2:31 - 4th"); }
    if (phase == 2) { d.status = GameStatus::Post; d.completed = true; d.us.score = 112; d.them.score = 108; d.us.winner = true; strcpy(d.detail, "Final"); celebratedEvent[0] = 0; }
    applyGame(d);
  }
  if (phase == 1 && now - phaseStart > 3000 && d.us.score == 98) {   // a 3-pointer lands
    d.us.score = 101; strcpy(d.clock, "2:04");
    applyGame(d);
  }
}
#endif

// ---- WiFi --------------------------------------------------------------------
#ifndef DEMO_MODE
static void onWifiUp(const char* how) {
  portalActive = false;
  noWifiSince = 0;
  log_i("WiFi connected (%s): %s", how, WiFi.localIP().toString().c_str());
  configTzTime(TZ_POSIX, NTP_SERVER);
  ui_show_boot("CONNECTED");
  ui_flush();
  nextPollAt = millis() + 1000;
}

static void openPortal() {
  portalActive = true;
  ui_show_wifi_setup(WIFI_PORTAL_SSID);
  ui_flush();
  wm.startConfigPortal(WIFI_PORTAL_SSID);   // non-blocking; loop() drives wm.process()
}
#endif

// ---- arduino -----------------------------------------------------------------
// The binned 2024-25 play-by-play table linked into the image (platformio.ini embed_files).
extern const uint8_t winprobTableStart[] asm("_binary_data_winprob_train_bin_start");
extern const uint8_t winprobTableEnd[]   asm("_binary_data_winprob_train_bin_end");
// Coefficients in use: refit on boot from the table, else the compiled Python fit.
kings::WinProbCoef winprobCoef = kings::WINPROB_COMPILED;

static void trainWinProbOnBoot() {
  kings::WinProbFit fit;
  uint32_t t0 = millis();
  bool ok = kings::winProbTrain(winprobTableStart, winprobTableEnd - winprobTableStart, WINPROB_EPS, fit);
  uint32_t ms = millis() - t0;
  if (!ok) { log_w("win-prob refit failed, using compiled coefficients"); return; }
  winprobCoef = fit.coef;
  log_i("win-prob refit: %u plays in %u cells, %d iterations, %u ms: a=%.5f b_ms=%.5f b_s=%.5f b_m=%.5f (compiled %.5f %.5f %.5f %.5f)",
        fit.plays, fit.cells, fit.iterations, ms, fit.coef.a, fit.coef.b_ms, fit.coef.b_s, fit.coef.b_m,
        WINPROB_A, WINPROB_B_MS, WINPROB_B_S, WINPROB_B_M);
  char msg[48];
  snprintf(msg, sizeof msg, "TRAINED ON %u PLAYS IN %u MS", fit.plays, ms);
  ui_show_boot(msg);
  ui_flush();
  delay(1500);   // long enough to read; the flex is the point
}

void setup() {
  Serial.begin(115200);
  delay(100);
  log_i("Kings beam boot, heap %u", ESP.getFreeHeap());

  prefs.begin("kingsbeam", false);
  prefs.getString("celeb", celebratedEvent, sizeof celebratedEvent);

  ui_init();
  ui_show_boot("STARTING");
  ui_flush();
  trainWinProbOnBoot();
  beam_init();
  touch_init();
  touch_set_tap(onTap);
  touch_set_long_press(onLongPress);
  setenv("TZ", TZ_POSIX, 1);
  tzset();

#ifdef DEMO_MODE
  log_i("DEMO MODE: no WiFi");
  // pretend the clock is set so countdowns render
  struct timeval tv = {.tv_sec = 1791252000 - 2 * 86400 - 4 * 3600, .tv_usec = 0};
  settimeofday(&tv, nullptr);
#else
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  wm.setConnectTimeout(20);
  // The portal closes itself after 3 minutes. If credentials are saved (a router that was
  // still booting after a power cut), restart and try them again instead of sitting on the
  // setup screen forever.
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setConfigPortalTimeoutCallback([]() {
    if (wm.getWiFiIsSaved()) { log_i("portal timed out with saved creds: restarting"); ESP.restart(); }
  });
  wm.setAPCallback([](WiFiManager*) {
    portalActive = true;
    ui_show_wifi_setup(WIFI_PORTAL_SSID);
    ui_flush();
  });
  ui_show_boot("CONNECTING TO WIFI");
  ui_flush();
  if (wm.autoConnect(WIFI_PORTAL_SSID)) onWifiUp("boot");
  nextPollAt = millis() + 1500;
#endif
}

void loop() {
  time_t now = time(nullptr);
  ui_tick(now);
  beam_tick();
  touch_tick();
  nightTick();

#ifdef DEMO_MODE
  demoTick();
#else
  if (portalActive) {
    wm.process();
    if (WiFi.status() == WL_CONNECTED) onWifiUp("portal");
    else if (!wm.getConfigPortalActive()) {   // closed by its timeout without saved creds
      portalActive = false;
      ui_show_boot("NO WIFI \xC2\xB7 RETRYING");
      ui_flush();
      nextPollAt = millis() + POLL_ERROR_MS;
    }
    return;
  }
  if (!timeSynced && nowEpoch()) {
    timeSynced = true;
    log_i("time synced");
    if (haveGame) ui_show_game(game, beam_is_lit());
  }
  if ((int32_t)(millis() - nextPollAt) >= 0) {
    poll();
  }
  // Saved network gone for good (moved house, new router): offer the setup portal again.
  if (noWifiSince && millis() - noWifiSince > REOPEN_PORTAL_MS) {
    log_i("no WiFi for %lu s: reopening the setup portal", (unsigned long)((millis() - noWifiSince) / 1000));
    noWifiSince = 0;
    openPortal();
    return;
  }
#endif

  // beam hold timeout after a win (or a manual light)
  if (beam_is_lit() && beamLitAt && millis() - beamLitAt > (uint32_t)BEAM_HOLD_HOURS * 3600000UL) {
    beam_set_mode(BeamMode::Off);
    beamLitAt = 0;
    if (haveGame) ui_show_game(game, false);
  }
  delay(5);
}
