#include "ui.h"
#include <Arduino.h>
#include <lvgl.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "fonts/fonts.h"
#include "logos.h"

// ---- palette ----------------------------------------------------------------
static const lv_color_t C_BG_TOP   = LV_COLOR_MAKE(0x1a, 0x10, 0x30);
static const lv_color_t C_BG_BOT   = LV_COLOR_MAKE(0x0b, 0x08, 0x14);
static const lv_color_t C_WHITE    = LV_COLOR_MAKE(0xf2, 0xe9, 0xff);
static const lv_color_t C_MUTED    = LV_COLOR_MAKE(0x8d, 0x86, 0xa3);
static const lv_color_t C_PURPLE   = LV_COLOR_MAKE(0x5a, 0x2d, 0x81);
static const lv_color_t C_LAVENDER = LV_COLOR_MAKE(0xb4, 0x8a, 0xe6);
static const lv_color_t C_GLOW     = LV_COLOR_MAKE(0xc9, 0xa2, 0xf0);
static const lv_color_t C_PILL     = LV_COLOR_MAKE(0x2a, 0x1f, 0x45);
static const lv_color_t C_LIVE_BG  = LV_COLOR_MAKE(0x3a, 0x0f, 0x1a);
static const lv_color_t C_LIVE_DOT = LV_COLOR_MAKE(0xff, 0x3b, 0x5c);
static const lv_color_t C_LIVE_TXT = LV_COLOR_MAKE(0xff, 0x8a, 0x9e);

// ---- state ------------------------------------------------------------------
static lv_display_t* disp = nullptr;
static lv_obj_t* offlineBadge = nullptr;
static lv_obj_t* countdownLbl = nullptr;   // PRE
static lv_obj_t* liveDot = nullptr;        // LIVE
static lv_obj_t* scoreUsLbl = nullptr;
static lv_obj_t* scoreThemLbl = nullptr;
static GameState shown;                    // what's on screen now
static bool shownBeam = false;
static bool haveScreen = false;
static int animScoreUs = -1, animScoreThem = -1;

static uint32_t tick_cb() { return millis(); }

// ---- helpers ----------------------------------------------------------------
static void upper(char* s) { for (; *s; ++s) *s = toupper((unsigned char)*s); }

static lv_obj_t* label(lv_obj_t* parent, const char* txt, const lv_font_t* font, lv_color_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  return l;
}

static lv_obj_t* eyebrow(lv_obj_t* parent, const char* txt, lv_color_t color) {
  lv_obj_t* l = label(parent, txt, &barlow_cond_semibold_12, color);
  lv_obj_set_style_text_letter_space(l, 2, 0);
  return l;
}

static lv_obj_t* box(lv_obj_t* parent, int w, int h, lv_color_t bg, int radius) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

static lv_obj_t* fresh_screen() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_remove_style_all(scr);
  lv_obj_set_style_bg_color(scr, C_BG_TOP, 0);
  lv_obj_set_style_bg_grad_color(scr, C_BG_BOT, 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  countdownLbl = liveDot = scoreUsLbl = scoreThemLbl = nullptr;
  offlineBadge = nullptr;
  return scr;
}

static void add_offline_badge(lv_obj_t* scr, bool visible) {
  offlineBadge = box(scr, 62, 18, C_PILL, 9);
  lv_obj_align(offlineBadge, LV_ALIGN_BOTTOM_RIGHT, -12, -10);
  lv_obj_t* l = eyebrow(offlineBadge, "OFFLINE", C_MUTED);
  lv_obj_center(l);
  if (!visible) lv_obj_add_flag(offlineBadge, LV_OBJ_FLAG_HIDDEN);
}

static lv_color_t team_color(const char* abbr, lv_color_t fallback) {
  const kb_team_t* t = kb_find_team(abbr);
  return t ? lv_color_hex(t->color) : fallback;
}

// Logo on a colored disc. Kings always get their purple.
static lv_obj_t* logo_badge(lv_obj_t* parent, const char* abbr, int cx, int cy) {
  const kb_team_t* t = kb_find_team(abbr);
  lv_obj_t* disc = box(parent, 72, 72, t ? lv_color_hex(t->color) : C_PILL, LV_RADIUS_CIRCLE);
  lv_obj_set_pos(disc, cx - 36, cy - 36);
  if (t) {
    lv_obj_t* img = lv_image_create(disc);
    lv_image_set_src(img, t->logo);
    lv_obj_center(img);
  } else {
    lv_obj_t* l = label(disc, abbr, &barlow_cond_bold_22, C_WHITE);
    lv_obj_center(l);
  }
  return disc;
}

static void format_tipoff(const GameState& gs, char* out, size_t n) {
  if (gs.tipoffEpoch == 0) { snprintf(out, n, "%s", gs.detail); upper(out); return; }
  time_t t = (time_t)gs.tipoffEpoch;
  struct tm lt;
  localtime_r(&t, &lt);
  // "MON OCT 5 · 7:00 PM"
  char day[16], mon[8];
  strftime(day, sizeof day, "%a", &lt);
  strftime(mon, sizeof mon, "%b", &lt);
  int h12 = lt.tm_hour % 12; if (h12 == 0) h12 = 12;
  snprintf(out, n, "%s %s %d \xC2\xB7 %d:%02d %s", day, mon, lt.tm_mday, h12, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM");
  upper(out);
}

static void format_countdown(const GameState& gs, time_t now, char* out, size_t n) {
  if (gs.tipoffEpoch == 0 || now < 1600000000) { snprintf(out, n, "TIPOFF SOON"); return; }
  long s = (long)(gs.tipoffEpoch - now);
  if (s <= 0) { snprintf(out, n, "TIPPING OFF"); return; }
  long d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
  if (d > 0) snprintf(out, n, "TIPOFF IN %ldD %ldH", d, h);
  else if (h > 0) snprintf(out, n, "TIPOFF IN %ldH %ldM", h, m);
  else snprintf(out, n, "TIPOFF IN %ldM", m < 1 ? 1 : m);
}

static void format_period(const GameState& gs, char* out, size_t n) {
  // ESPN: period 1-4, 5+ = OT; clock "0.0" at period end; detail like "Halftime", "End of 3rd"
  if (strstr(gs.detail, "Half") || strstr(gs.detail, "HALF")) { snprintf(out, n, "HALFTIME"); return; }
  if (strncmp(gs.clock, "0.0", 3) == 0 && gs.period > 0 && gs.period < 4) { snprintf(out, n, "END Q%d", gs.period); return; }
  if (gs.period > 4) snprintf(out, n, "%s%d \xC2\xB7 %s", "OT", gs.period - 4, gs.clock);
  else if (gs.period > 0) snprintf(out, n, "Q%d \xC2\xB7 %s", gs.period, gs.clock);
  else snprintf(out, n, "%s", gs.detail);
  if (gs.period == 5) snprintf(out, n, "OT \xC2\xB7 %s", gs.clock);
}

// ---- screens ----------------------------------------------------------------
static void build_pre(const GameState& gs, time_t now) {
  lv_obj_t* scr = fresh_screen();
  lv_obj_t* e = eyebrow(scr, "NEXT GAME", C_LAVENDER);
  lv_obj_set_pos(e, 16, 18);
  lv_obj_t* tag = eyebrow(scr, gs.weAreHome() ? "HOME" : "AWAY", C_MUTED);
  lv_obj_align(tag, LV_ALIGN_TOP_RIGHT, -16, 18);

  logo_badge(scr, gs.us.abbr, 76, 104);
  logo_badge(scr, gs.them.abbr, 244, 104);
  lv_obj_t* vs = label(scr, gs.weAreHome() ? "VS" : "@", &barlow_cond_bold_22, C_MUTED);
  lv_obj_align(vs, LV_ALIGN_TOP_MID, 0, 92);

  char when[40];
  format_tipoff(gs, when, sizeof when);
  lv_obj_t* w = label(scr, when, &barlow_cond_bold_22, C_WHITE);
  lv_obj_align(w, LV_ALIGN_TOP_MID, 0, 160);

  lv_obj_t* pill = box(scr, 130, 24, C_PILL, 12);
  lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, 196);
  char cd[32];
  format_countdown(gs, now, cd, sizeof cd);
  countdownLbl = eyebrow(pill, cd, C_GLOW);
  lv_obj_center(countdownLbl);
  add_offline_badge(scr, false);
}

static void set_score_text(lv_obj_t* l, int v) {
  char b[8];
  snprintf(b, sizeof b, "%d", v < 0 ? 0 : v);
  lv_label_set_text(l, b);
}
static void anim_us(void* obj, int32_t v) { set_score_text((lv_obj_t*)obj, v); }

static void animate_score(lv_obj_t* l, int from, int to) {
  if (from < 0 || from == to) { set_score_text(l, to); return; }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, l);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, 450);
  lv_anim_set_exec_cb(&a, anim_us);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

static void build_live(const GameState& gs) {
  lv_obj_t* scr = fresh_screen();
  // LIVE pill
  lv_obj_t* pill = box(scr, 56, 20, C_LIVE_BG, 10);
  lv_obj_set_pos(pill, 16, 16);
  liveDot = box(pill, 8, 8, C_LIVE_DOT, LV_RADIUS_CIRCLE);
  lv_obj_align(liveDot, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_t* lt = eyebrow(pill, "LIVE", C_LIVE_TXT);
  lv_obj_align(lt, LV_ALIGN_LEFT_MID, 22, 0);

  char per[24];
  format_period(gs, per, sizeof per);
  lv_obj_t* p = label(scr, per, &barlow_cond_bold_22, C_WHITE);
  lv_obj_align(p, LV_ALIGN_TOP_MID, 0, 12);

  // color bars
  lv_obj_t* b1 = box(scr, 136, 4, C_PURPLE, 2);  lv_obj_set_pos(b1, 16, 56);
  lv_obj_t* b2 = box(scr, 136, 4, team_color(gs.them.abbr, C_MUTED), 2); lv_obj_set_pos(b2, 168, 56);

  lv_obj_t* a1 = label(scr, gs.us.abbr, &barlow_cond_semibold_16, C_LAVENDER);
  lv_obj_set_style_text_letter_space(a1, 2, 0);
  lv_obj_align(a1, LV_ALIGN_TOP_MID, -76, 70);
  lv_obj_t* a2 = label(scr, gs.them.abbr, &barlow_cond_semibold_16, C_MUTED);
  lv_obj_set_style_text_letter_space(a2, 2, 0);
  lv_obj_align(a2, LV_ALIGN_TOP_MID, 76, 70);

  bool usLead = gs.us.score > gs.them.score, themLead = gs.them.score > gs.us.score;
  scoreUsLbl = label(scr, "0", &barlow_cond_bold_84, usLead || !themLead ? C_WHITE : C_MUTED);
  scoreThemLbl = label(scr, "0", &barlow_cond_bold_84, themLead || !usLead ? C_WHITE : C_MUTED);
  set_score_text(scoreUsLbl, gs.us.score);
  set_score_text(scoreThemLbl, gs.them.score);
  lv_obj_align(scoreUsLbl, LV_ALIGN_TOP_MID, -76, 84);
  lv_obj_align(scoreThemLbl, LV_ALIGN_TOP_MID, 76, 84);
  animScoreUs = gs.us.score; animScoreThem = gs.them.score;

  if (usLead || themLead) {
    lv_obj_t* mk = box(scr, 16, 4, C_LAVENDER, 2);
    lv_obj_align(mk, LV_ALIGN_TOP_MID, usLead ? -76 : 76, 192);
  }
  lv_obj_t* f = eyebrow(scr, gs.weAreHome() ? "GOLDEN 1 CENTER \xC2\xB7 SACRAMENTO" : "ON THE ROAD", C_MUTED);
  lv_obj_align(f, LV_ALIGN_BOTTOM_MID, 0, -14);
  add_offline_badge(scr, false);
}

static void update_live(const GameState& gs) {
  if (!scoreUsLbl || !scoreThemLbl) return;
  bool changed = gs.us.score != shown.us.score || gs.them.score != shown.them.score ||
                 gs.period != shown.period || strcmp(gs.clock, shown.clock) != 0;
  if (!changed) return;
  // lead/marker/period may have changed: rebuild is cheap, but animate the score first
  int fromUs = shown.us.score, fromThem = shown.them.score;
  build_live(gs);
  animate_score(scoreUsLbl, fromUs, gs.us.score);
  animate_score(scoreThemLbl, fromThem, gs.them.score);
}

static void glow_anim(void* obj, int32_t v) { lv_obj_set_y((lv_obj_t*)obj, v); }

static void build_final(const GameState& gs, bool beamLit) {
  lv_obj_t* scr = fresh_screen();
  bool won = gs.weWon();
  if (won) {
    // vertical beam glow rising up the middle of the screen
    lv_obj_t* glow = box(scr, 22, SCREEN_H, C_GLOW, 0);
    lv_obj_set_style_bg_grad_color(glow, C_BG_BOT, 0);
    lv_obj_set_style_bg_grad_dir(glow, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(glow, LV_OPA_40, 0);
    lv_obj_set_x(glow, SCREEN_W / 2 - 11);
    lv_obj_set_y(glow, SCREEN_H);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, glow);
    lv_anim_set_values(&a, SCREEN_H, 0);
    lv_anim_set_duration(&a, 1800);
    lv_anim_set_exec_cb(&a, glow_anim);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
    lv_obj_t* core = box(scr, 4, SCREEN_H, C_WHITE, 0);
    lv_obj_set_style_bg_opa(core, LV_OPA_50, 0);
    lv_obj_set_x(core, SCREEN_W / 2 - 2);
  }
  lv_obj_t* e = eyebrow(scr, "FINAL", C_LAVENDER);
  lv_obj_set_pos(e, 16, 18);
  lv_obj_t* tag = eyebrow(scr, won ? "KINGS WIN" : (gs.us.score > gs.them.score ? "KINGS WIN" : "KINGS LOSS"), C_MUTED);
  lv_obj_align(tag, LV_ALIGN_TOP_RIGHT, -16, 18);

  lv_obj_t* s1 = label(scr, "0", &barlow_cond_bold_40, won ? C_WHITE : C_MUTED);
  lv_obj_t* s2 = label(scr, "0", &barlow_cond_bold_40, won ? C_MUTED : C_WHITE);
  set_score_text(s1, gs.us.score);
  set_score_text(s2, gs.them.score);
  lv_obj_align(s1, LV_ALIGN_TOP_MID, -76, 48);
  lv_obj_align(s2, LV_ALIGN_TOP_MID, 76, 48);
  lv_obj_t* a1 = eyebrow(scr, gs.us.abbr, C_LAVENDER);   lv_obj_align(a1, LV_ALIGN_TOP_MID, -76, 104);
  lv_obj_t* a2 = eyebrow(scr, gs.them.abbr, C_MUTED);    lv_obj_align(a2, LV_ALIGN_TOP_MID, 76, 104);

  if (won) {
    lv_obj_t* w = label(scr, "LIGHT THE BEAM", &barlow_cond_bold_40, C_WHITE);
    lv_obj_set_style_text_letter_space(w, 3, 0);
    lv_obj_align(w, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_t* f = eyebrow(scr, beamLit ? "BEAM IS LIT \xC2\xB7 TAP TO TURN OFF" : "TAP TO LIGHT THE BEAM", C_MUTED);
    lv_obj_align(f, LV_ALIGN_BOTTOM_MID, 0, -14);
  } else {
    char d[48];
    snprintf(d, sizeof d, "%s", gs.detail[0] ? gs.detail : "FINAL");
    upper(d);
    lv_obj_t* w = label(scr, "NEXT TIME", &barlow_cond_bold_40, C_MUTED);
    lv_obj_set_style_text_letter_space(w, 3, 0);
    lv_obj_align(w, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_t* f = eyebrow(scr, "TAP TO LIGHT THE BEAM ANYWAY", C_MUTED);
    lv_obj_align(f, LV_ALIGN_BOTTOM_MID, 0, -14);
  }
  add_offline_badge(scr, false);
}

static void build_none() {
  lv_obj_t* scr = fresh_screen();
  lv_obj_t* e = eyebrow(scr, "SACRAMENTO KINGS", C_LAVENDER);
  lv_obj_set_pos(e, 16, 18);
  logo_badge(scr, TEAM_ABBR, SCREEN_W / 2, 100);
  lv_obj_t* w = label(scr, "NO GAME SCHEDULED", &barlow_cond_bold_22, C_WHITE);
  lv_obj_align(w, LV_ALIGN_TOP_MID, 0, 160);
  lv_obj_t* f = eyebrow(scr, "TAP TO LIGHT THE BEAM", C_MUTED);
  lv_obj_align(f, LV_ALIGN_BOTTOM_MID, 0, -14);
  add_offline_badge(scr, false);
}

// ---- public -----------------------------------------------------------------
void ui_init() {
  lv_init();
  lv_tick_set_cb(tick_cb);
  static uint8_t drawBuf[SCREEN_W * 40 * 2];   // 40-line partial buffer, 25.6 KB
  // The panel is 240x320 portrait; rotate to landscape. Use ROTATION_270 if yours is upside down.
  disp = lv_tft_espi_create(240, 320, drawBuf, sizeof drawBuf);
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
  // backlight PWM (TFT_eSPI drove it as a plain output during init)
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL_PIN, 0);
  ledcWrite(0, BL_DAY);
  haveScreen = true;
}

void ui_set_backlight(uint8_t level) { ledcWrite(0, level); }

void ui_show_boot(const char* msg) {
  lv_obj_t* scr = fresh_screen();
  logo_badge(scr, TEAM_ABBR, SCREEN_W / 2, 92);
  lv_obj_t* t = label(scr, "LIGHT THE BEAM", &barlow_cond_bold_22, C_WHITE);
  lv_obj_set_style_text_letter_space(t, 3, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 146);
  lv_obj_t* m = eyebrow(scr, msg, C_MUTED);
  lv_obj_align(m, LV_ALIGN_TOP_MID, 0, 186);
  shown = GameState{};
}

void ui_show_wifi_setup(const char* portalSsid) {
  lv_obj_t* scr = fresh_screen();
  lv_obj_t* e = eyebrow(scr, "WIFI SETUP", C_LAVENDER);
  lv_obj_set_pos(e, 16, 18);
  lv_obj_t* t = label(scr, "CONNECT ME TO WIFI", &barlow_cond_bold_22, C_WHITE);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 44);
  char l1[64];
  snprintf(l1, sizeof l1, "1.  On your phone, join the WiFi network  \"%s\"", portalSsid);
  const char* l2 = "2.  A setup page opens (or visit 192.168.4.1)";
  const char* l3 = "3.  Pick your home WiFi and enter its password";
  const char* l4 = "4.  Done. This screen changes when it connects.";
  int y = 92;
  for (const char* s : {(const char*)l1, l2, l3, l4}) {
    lv_obj_t* l = label(scr, s, &barlow_cond_semibold_16, C_WHITE);
    lv_obj_set_pos(l, 16, y);
    y += 28;
  }
  shown = GameState{};
}

void ui_show_game(const GameState& gs, bool beamLit) {
  bool sameEvent = strcmp(gs.eventId, shown.eventId) == 0 && gs.status == shown.status;
  if (sameEvent && gs.status == GameStatus::Live) {
    update_live(gs);
  } else if (sameEvent && gs.status == GameStatus::Post && beamLit == shownBeam) {
    // nothing changed
  } else {
    switch (gs.status) {
      case GameStatus::Pre:  build_pre(gs, time(nullptr)); break;
      case GameStatus::Live: build_live(gs); break;
      case GameStatus::Post: build_final(gs, beamLit); break;
      default:               build_none(); break;
    }
  }
  shown = gs;
  shownBeam = beamLit;
}

void ui_set_offline(bool offline) {
  if (!offlineBadge) return;
  if (offline) lv_obj_remove_flag(offlineBadge, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(offlineBadge, LV_OBJ_FLAG_HIDDEN);
}

void ui_tick(time_t now) {
  static uint32_t lastSec = 0;
  lv_timer_handler();
  uint32_t ms = millis();
  if (ms - lastSec < 1000) return;
  lastSec = ms;
  if (countdownLbl && shown.status == GameStatus::Pre) {
    char cd[32];
    format_countdown(shown, now, cd, sizeof cd);
    lv_label_set_text(countdownLbl, cd);
  }
  if (liveDot) {
    static bool on = true;
    on = !on;
    lv_obj_set_style_bg_opa(liveDot, on ? LV_OPA_COVER : LV_OPA_30, 0);
  }
}
