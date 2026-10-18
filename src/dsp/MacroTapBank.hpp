#pragma once
// ── dotModular::MacroTapBank — the per-voice send-tap bank (pure store contract) ───────────────
// The PRE/POST send-tap, made PER-VOICE to match the per-voice send (the P9b global-tap
// regression fix; see docs/design/CC_PROMPT_per_voice_send_tap.md). 16 voice-slots × 7 poly
// lanes × 2 kinds (0 = LOR items 0-2, 1 = spread item 3), mirroring macroSend's shape and
// conventions (VoiceResolver::voiceSlot; mono = slot 0).
//
// This header is the PURE, Rack-free contract — the store itself lives in Monsoon's editor
// slice, the JSON round-trip is called by MonsoonPersistenceManager, and the unit test
// drives both from here. Default 1.0 (POST): a patch with no macroTap key loads every
// voice at POST.
#include <jansson.h>

namespace dotModular {

struct MacroTapBank {
    static constexpr int   N = 224;         // 16 voice-slots × 7 poly lanes × 2 kinds
    static constexpr float kDefault = 1.f;  // POST (the old single-tap behaviour at its default)

    // (v*7 + lane)*2 + which — the same stride convention as macroSend's (v*7+lane)*4+item.
    static int index(int v, int lane, int which) { return (v * 7 + lane) * 2 + which; }

    // Save the bank as a JSON array under `key` (appends to the object; house style).
    static void serialize(const float* taps, json_t* root, const char* key) {
        json_t* ja = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(ja, json_real((double)taps[i]));
        json_object_set_new(root, key, ja);
    }

    // Load the bank from `key`. Missing / non-array / short arrays default-fill (1.0,
    // POST) first and overwrite only the entries actually present — so an absent key
    // (pre-fix patch) loads ALL voices at POST. Returns true when a full array was read.
    static bool parse(json_t* root, const char* key, float* out) {
        for (int i = 0; i < N; ++i) out[i] = kDefault;
        json_t* ja = json_object_get(root, key);
        if (!json_is_array(ja)) return false;
        const size_t n = json_array_size(ja);
        for (int i = 0; i < N && (size_t)i < n; ++i) {
            json_t* jv = json_array_get(ja, (size_t)i);
            if (json_is_number(jv)) out[i] = (float)json_number_value(jv);
        }
        return n >= (size_t)N;
    }
};

} // namespace dotModular
