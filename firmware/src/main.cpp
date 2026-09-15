// Kings "Light the Beam" scoreboard — main loop.
//
//   boot → WiFi (captive portal on first run) → NTP → poll ESPN → drive screen + beam
//
// Build with -DDEMO_MODE to skip WiFi and cycle NEXT → LIVE → FINAL from canned data.
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "game_state.h"
#include "kings_api.h"
#include "ui.h"
#include "beam.h"
#include "touch.h"

#ifndef DEMO_MODE
#include <WiFiManager.h>
static WiFiManager wm;
static bool portalActive = false;
#endif

static GameState game;
static bool haveGame = false;
static bool offline = false;
static uint32_t nextPollAt = 0;
static uint32_t beamLitAt = 0;
static char celebratedEvent[16] = "";
static bool timeSynced = false;

// ---- scheduling --------------------------------------------------------------
static uint32_t pollIntervalFor(const GameState& gs, time_t now) {
  switch (gs.status) {
    case GameStatus::Live: return POLL_LIVE_MS;
    case GameStatus::Post: return POLL_POST_MS;
    case GameStatus::Pre: {
      if (gs.tipoffEpoch && now > 1600000000 && gs.tipoffEpoch - now < PRE_SOON_WINDOW_S) return POLL_PRE_SOON_MS;
      return POLL_PRE_MS;
    }
    default: return POLL_PRE_MS;
  }
}

static void applyGame(const GameState& gs) {
  time_t now = time(nullptr);
  bool newEvent = strcmp(gs.eventId, game.eventId) != 0;
  game = gs;
  haveGame = true;

  // Beam policy
  if (gs.status == GameStatus::Live) {
    if (!beam_is_lit()) beam_set_mode(BeamMode::Pulse);
  } else if (gs.status == GameStatus::Post && gs.weWon()) {
    if (strcmp(celebratedEvent, gs.eventId) != 0) {
      strncpy(celebratedEvent, gs.eventId, sizeof celebratedEvent - 1);
      beam_set_mode(BeamMode::Rise);
      beamLitAt = millis();
      log_i("KINGS WIN %d-%d — light the beam", gs.us.score, gs.them.score);
    }
  } else if (gs.status == GameStatus::Post) {
    if (beam_mode() == BeamMode::Pulse) beam_set_mode(BeamMode::Off);
  } else if (gs.status == GameStatus::Pre && newEvent) {
    // a new game is up: previous celebration is over
    if (beam_is_lit() || beam_mode() == BeamMode::Pulse) beam_set_mode(BeamMode::Off);
  }
  ui_show_game(game, beam_is_lit());
  nextPollAt = millis() + pollIntervalFor(game, now);
}

static void poll() {
#ifndef DEMO_MODE
  GameState gs;
  int code = 0;
  ui_show_boot(haveGame ? "REFRESHING" : "FETCHING SCHEDULE");
  if (!haveGame) ui_tick(time(nullptr));
  kings::FetchResult r = kings::fetch(gs, &code);
  log_i("fetch: %s (http %d) heap %u", kings::fetchResultName(r), code, ESP.getFreeHeap());
  if (r == kings::FetchResult::Ok) {
    offline = false;
    applyGame(gs);
  } else {
    offline = true;
    if (haveGame) ui_show_game(game, beam_is_lit());
    else ui_show_boot(r == kings::FetchResult::NoWifi ? "NO WIFI \xC2\xB7 RETRYING" : "ESPN UNREACHABLE \xC2\xB7 RETRYING");
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
  time_t now = time(nullptr);
  if (now < 1600000000) return;   // clock not set yet
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

// ---- arduino -----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  log_i("Kings beam boot, heap %u", ESP.getFreeHeap());

  ui_init();
  ui_show_boot("STARTING");
  ui_tick(0);
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
  wm.setAPCallback([](WiFiManager*) {
    portalActive = true;
    ui_show_wifi_setup(WIFI_PORTAL_SSID);
  });
  ui_show_boot("CONNECTING TO WIFI");
  ui_tick(0);
  if (wm.autoConnect(WIFI_PORTAL_SSID)) {
    log_i("WiFi connected: %s", WiFi.localIP().toString().c_str());
    configTzTime(TZ_POSIX, NTP_SERVER);
  }
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
    if (WiFi.status() == WL_CONNECTED) {
      portalActive = false;
      log_i("WiFi connected via portal");
      configTzTime(TZ_POSIX, NTP_SERVER);
      ui_show_boot("CONNECTED");
      nextPollAt = millis() + 1000;
    }
    return;
  }
  if (!timeSynced && now > 1600000000) {
    timeSynced = true;
    log_i("time synced");
    if (haveGame) ui_show_game(game, beam_is_lit());
  }
  if ((int32_t)(millis() - nextPollAt) >= 0) {
    poll();
  }
#endif

  // beam hold timeout after a win
  if (beam_is_lit() && beamLitAt && millis() - beamLitAt > (uint32_t)BEAM_HOLD_HOURS * 3600000UL) {
    beam_set_mode(BeamMode::Off);
    beamLitAt = 0;
    if (haveGame) ui_show_game(game, false);
  }
  delay(5);
}
