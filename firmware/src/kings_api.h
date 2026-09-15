// Network side: fetch the ESPN team endpoint over HTTPS and parse it.
#pragma once
#include "game_state.h"

namespace kings {

enum class FetchResult { Ok, NoWifi, HttpError, ParseError };

// Blocking HTTPS GET + parse. Takes ~1-3 s on the ESP32 (TLS handshake dominates).
// On failure `out` is left untouched so the caller keeps showing the last good state.
FetchResult fetch(GameState& out, int* httpCode = nullptr);

const char* fetchResultName(FetchResult r);

}  // namespace kings
