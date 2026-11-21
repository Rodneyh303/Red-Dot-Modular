#pragma once
// QmixOctave — the qmix-octave tier's threshold bank + the three-way source resolver
// (CC_PROMPT_QMIX_OCTAVE + QMIX_OCTAVE_PLAN.md Stages 2-3). Rack-free (no Rack headers;
// the battery compiles it directly, the SubgateKnobs discipline — this bank IS that
// pattern's shape minus the axes the octave decision does not have: ONE threshold per
// voice, no level/dec grid, NO delta planes).
//
// WHAT IT RESOLVES (the spec's CONTROLS section, Rodney's message verbatim): the octave
// axis of the split q-mix read (QMIX_OCTAVE Stage 1's second comparison of the same
// r_qmix draw at SequencerEngine's voicePitch callers). One threshold per voice:
//   1. OWN per-voice knob (ownActive) — the qmix-octave per-voice subpanel's knob CLAIMED
//      this cell (touch = claim). Tier 1, wins outright.
//   2. ELSE the source switch: FOLLOW DEGREE (default — "qmix octave to follow main qmix
//      poly voice subpanel for both pitch and octave": the octave threshold IS the voice's
//      degree qmix level, so the two decisions are the same comparison — today's behaviour)
//      or GLOBAL (the Qmix-octave knob on the Straits base — "global knob on straits for
//      octave": one cross-voice octave threshold).
// DEFAULTS ARE THE BYTE-IDENTITY PIN: ownActive=false + followDegree=true resolves to the
// voice's degree qmix level EXACTLY — until a knob is touched or the switch flips, the
// octave axis plays exactly as the Stage 1 split (which IS today's behaviour) plays.
//
// THE RESOLVED RANGE is [0,1] (a threshold is a probability — the qmixLevel class); the
// resolver clamps. NO delta planes (adjudication 4/5): the spec asks for a knob; the subgate
// per-voice panels carry no mod jacks post-merge (modulation is Causeway and not in scope);
// no deltas ⟹ no audio-thread publisher ⟹ no watchdog/heartbeat machinery for this bank.
// The writer split (the house law): the authored cells are UI-THREAD-written by the Straits
// base's store-backed knobs/switch and the subpanel's knobs (store-trusts-its-writer);
// nothing writes audio-side.
//
// HOME: the BANK lives in Monsoon's EditorState (the engine host both families attach to);
// the Straits surfaces address it store-backed through Monsoon accessors (the de-param law).
// Persistence: the SubgateBank codec shape (authored cells only, missing keys ⟹ defaults —
// old patches bit-identical; short arrays keep the default tail; wrong-typed entries keep
// the default cell).
#include <jansson.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace dotModular {
namespace QmixOctave {

    static constexpr int VOICES = 16;   // storage slots (0 = V1 — the mono row: V1's own
                                        // octave threshold, authored by the subpanel's mono
                                        // knob; 1..15 = poly voices 2..16)
    static constexpr float kDefaultThreshold = 0.5f;   // inert under the follow-degree default

    // ── the bank (POD, Monsoon::EditorState member — the authored store) ─────────────────
    struct Bank {
        float globalTh;
        bool  followDegree;         // true (default) = the octave threshold follows the
                                    // voice's degree qmix level; false = the global knob
        float ownTh[VOICES];
        bool  ownActive[VOICES];

        Bank() { reset(); }

        void reset() {
            globalTh    = kDefaultThreshold;
            followDegree = true;
            for (int v = 0; v < VOICES; ++v) { ownTh[v] = kDefaultThreshold; ownActive[v] = false; }
        }

        // ── persistence (the SubgateBank::serialize contract: missing keys ⟹ the defaults
        // survive — old patches unchanged, bit-identical). Authored cells only.
        static void serialize(const Bank& b, json_t* root, const char* prefix) {
            const std::string pfx(prefix);
            json_t* jg = json_array();
            json_array_append_new(jg, json_real((double)b.globalTh));
            json_object_set_new(root, (pfx + "Global").c_str(), jg);
            json_t* jf = json_array();
            json_array_append_new(jf, b.followDegree ? json_true() : json_false());
            json_object_set_new(root, (pfx + "FollowDegree").c_str(), jf);
            json_t* jo = json_array();
            for (int v = 0; v < VOICES; ++v)
                json_array_append_new(jo, json_real((double)b.ownTh[v]));
            json_object_set_new(root, (pfx + "Own").c_str(), jo);
            json_t* ja = json_array();
            for (int v = 0; v < VOICES; ++v)
                json_array_append_new(ja, b.ownActive[v] ? json_true() : json_false());
            json_object_set_new(root, (pfx + "OwnActive").c_str(), ja);
        }

        // Load: RESETS to defaults first (deterministic — never merges into stale state),
        // then overwrites from the keys actually present. Short arrays keep the default
        // tail; booleans read strictly (json_is_true); a wrong-typed entry keeps the
        // default cell (the MacroTapBank parse convention, SubgateBank::parse verbatim).
        static void parse(json_t* root, const char* prefix, Bank& out) {
            out.reset();
            if (!root || !json_is_object(root)) return;
            const std::string pfx(prefix);
            auto readReals = [&](const char* key, int n, float* dst) {
                json_t* arr = json_object_get(root, key);
                if (!json_is_array(arr)) return;
                const int nAvail = (int)json_array_size(arr);
                for (int i = 0; i < n && i < nAvail; ++i) {
                    json_t* x = json_array_get(arr, i);
                    if (json_is_real(x) || json_is_integer(x))
                        dst[i] = (float)json_number_value(x);
                }
            };
            auto readBools = [&](const char* key, int n, bool* dst) {
                json_t* arr = json_object_get(root, key);
                if (!json_is_array(arr)) return;
                const int nAvail = (int)json_array_size(arr);
                for (int i = 0; i < n && i < nAvail; ++i)
                    dst[i] = json_is_true(json_array_get(arr, i));
            };
            float g[1], o[VOICES];
            bool fd[1], oa[VOICES];
            // Seed the locals with the DEFAULTS first: the readers only fill the entries
            // actually present, and the copy below lands EVERY cell — absent keys / short
            // arrays / wrong-typed entries must keep the defaults (the old-patch guarantee).
            g[0] = kDefaultThreshold; fd[0] = true;
            for (int v = 0; v < VOICES; ++v) { o[v] = kDefaultThreshold; oa[v] = false; }
            readReals((pfx + "Global").c_str(), 1, g);
            readBools((pfx + "FollowDegree").c_str(), 1, fd);
            readReals((pfx + "Own").c_str(), VOICES, o);
            readBools((pfx + "OwnActive").c_str(), VOICES, oa);
            out.globalTh     = g[0];
            out.followDegree = fd[0];
            for (int v = 0; v < VOICES; ++v) { out.ownTh[v] = o[v]; out.ownActive[v] = oa[v]; }
        }
    };

    // ── THE THREE-WAY RESOLVER (pure, the one authority) ─────────────────────────────────
    // Precedence: own (tier 1, claimed) > { follow-degree (default) | global } by the
    // switch; clamped to [0,1] (a threshold is a probability — the qmixLevel class). The
    // byte-identity default: resolve(false, ·, true, ·, degreeValue) == degreeValue, EXACTLY
    // (the degree value is produced inside [0,1] by its writer).
    inline float resolve(bool ownActive, float ownTh, bool followDegree, float globalTh,
                         float degreeValue) {
        const float base = ownActive ? ownTh : (followDegree ? degreeValue : globalTh);
        return std::clamp(base, 0.0f, 1.0f);
    }

} // namespace QmixOctave
} // namespace dotModular
