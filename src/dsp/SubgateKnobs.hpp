#pragma once
// SubgateKnobs — the subgate-tier consumer-group threshold bank + the three-way
// source resolver (CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC.md). HOME (Rodney's
// ruling, overriding the interim distribution-expander siting): the PANEL
// surface is the NEW STRAITS EXPANDER BASE (global knobs) + its NEW SUBPANELS
// (per-voice knobs) — Sands/distribution is the DRAW-generation plane, Straits
// the decision plane reacting to the draws, so the decision thresholds live on
// the Straits side. The BANK itself lives in Monsoon's EditorState (the engine
// host both families attach to); the Straits surfaces address it store-backed
// (the de-param law), never as Rack params. Rack-free (no Rack headers; the
// battery compiles it directly, same discipline as RhythmDecisionCore / WarpF).
//
// THE CONSUMER-GROUP MODEL (STRAITS_POLY_SUBGATE_KNOBS.md): subgate articulation
// decisions are CONSUMERS of a draw event. Within a SINGLE draw event all
// consumers read the SAME sampled shaped value; DIFFERENT events (main vs
// ratchet vs ghost) are independent draws → SEPARATE THRESHOLDS express
// different rates (not fake independence). The main tier keeps the main lane's
// own thresholds; this bank adds the SUBGATE tier on top.
//
// THE DECISION SET (Rodney: "Add subgate accent too"): {RATCHET, GHOST} ×
// {REST, LEGATO, ADVANCE, ACCENT} = 8. REST/LEGATO/ACCENT are the per-subgate
// articulation; ADVANCE is the Stage-3 step-advance threshold (the ratchet/
// ghost gate-rise advance probabilities — STORED here, CONSUMED by the
// step-advance build: Stages 1+3 landed b7345992, Stage 2 the bipolar
// sourcing — CC_PROMPT_STEP_ADVANCE_CONSUMPTION.md). ADVANCE is the
// DIRECTION-bearing decision: its RESOLVED range is SIGNED [-1,+1]
// (negative = p(step BACKWARD)) while the store knobs stay [0,1] — the
// negative half arrives through the follow-main tier (the bipolar main
// Variation value) and through bipolar mod deltas, and must survive the
// resolver's clamp (see resolve/clampFor below).
//
// THE THREE-WAY SOURCE per (voice, level, decision) — the spec's precedence,
// translated onto the expander surface (the spec's "own poly-knob subpanel
// attached" becomes the codebase's own-cell opt-in law — the eastOwns /
// varlegDeleg boolean model, the spec's own "mirrors the Macro-default /
// per-lane-opt-in pattern" shape):
//   1. OWN per-voice knob (ownActive) — the East-attached expander's per-voice
//      subgate knob CLAIMED this cell (touch = claim). Tier 1, wins outright.
//   2. ELSE the source switch per decision: FOLLOW MAIN (default — the subgate
//      decision inherits the MAIN lane's resolved per-voice value: ratchet-rest
//      follows the voice's main REST; advance follows main VARIATION, the
//      step-advance lane in gate mode; etc.) or GLOBAL (the cross-voice knob
//      row the Macro-attached expander authors — genuinely NEW values, not
//      Monsoon copies; the clarified spec's ruling).
//   3. The MOD delta (the Causeway path — "a Straits knob without its mod path
//      is a modulation-layer seam": every threshold has its expander mod jack)
//      adds POST-resolution, clamped to the DECISION's range: [0,1] for
//      REST/LEGATO/ACCENT (a probability), [-1,+1] for ADVANCE (the signed
//      step-advance threshold — CC_PROMPT_STEP_ADVANCE_CONSUMPTION Stage 2).
// DEFAULTS ARE THE BYTE-IDENTITY PIN: ownActive=false + followMain=true +
// delta=0 resolves to the MAIN value — until a knob is touched or a switch
// flips, every subgate decision behaves exactly as before (the spec's
// degenerate case: everything transparently follows the main lane).
#include <jansson.h>

#include <algorithm>
#include <cstdint>   // uint32_t — the publishSeq heartbeats
#include <cstring>

namespace dotModular {
namespace SubgateKnobs {

    // ── the decision axes ─────────────────────────────────────────────────────
    // constexpr ints, NOT enums: C++20 deprecates enum-enum arithmetic (the
    // bank's LEVELS*DECS plane sizes trip -Wdeprecated-enum-enum-conversion in
    // the warn sweep), and every consumer treats these as plain ints.
    static constexpr int RATCHET = 0, GHOST = 1;
    static constexpr int LEVELS  = 2;
    // REST/LEGATO/ACCENT mirror the main articulation lanes; ADVANCE is the
    // step-advance threshold (main lane: VARIATION in gate mode — the spec's
    // degenerate-case mapping "subgates' STEP-ADVANCE follow MONSOON VARIATION").
    static constexpr int REST = 0, LEGATO = 1, ADVANCE = 2, ACCENT = 3;
    static constexpr int DECS  = 4;

    static constexpr int VOICES           = 16;   // storage slots (0 = V1 — the mono subgate row: V1's own ratchet/ghost articulation thresholds, authored by the subpanel's mono knob; 1..15 = poly voices 2..16)
    static constexpr float kDefaultThreshold = 0.5f;

    inline int lvlDec(int level, int dec)          { return level * DECS + dec; }
    inline int voiceCell(int v, int level, int dec) { return (v * LEVELS + level) * DECS + dec; }

    // UI captions (the expander's jack tooltips + strip lettering — one authority).
    inline constexpr const char* LEVEL_CAPTIONS[LEVELS] = { "RATCHET", "GHOST" };
    inline constexpr const char* DEC_CAPTIONS[DECS]     = { "REST", "LEG", "ADV", "ACC" };

    // ── the bank (POD, Monsoon::EditorState member — the authored stores) ─────
    // Writer split (the house law): the authored cells (globalTh / followMain /
    // ownTh / ownActive) are UI-THREAD-written by the Straits subgate base's
    // store-backed knobs (globals + source switches) and its subpanels' knobs
    // (per-voice, touch = claim) (store-trusts-its-writer); the delta planes
    // (globalDelta / ownDelta / deltasActive) are AUDIO-THREAD-ONLY — the base's
    // process() publishes globalDelta, the subpanels' process() publishes
    // ownDelta, and MONSOON's watchdog drops them when that publisher dies (the
    // dist-delta law verbatim). Deltas are NOT persisted (the dist planes aren't
    // either — publish-overwrites).
    //
    // publishSeq — the per-publisher heartbeat Monsoon's watchdog shadows
    // (Mirrors WarpBank::distPublishSeq): [0] = the global publisher (the
    // subgate BASE, bumped every block it lives), [1] = the per-voice publisher
    // (the SUBPANELS — shared slot; either subpanel living keeps the plane
    // fresh). Session-lived like the deltas they vouch for; never persisted.
    struct SubgateBank {
        float globalTh[LEVELS][DECS];
        bool  followMain[LEVELS][DECS];
        float ownTh[VOICES][LEVELS][DECS];
        bool  ownActive[VOICES][LEVELS][DECS];
        float globalDelta[LEVELS][DECS];
        float ownDelta[VOICES][LEVELS][DECS];
        bool  deltasActive;
        uint32_t publishSeq[2];

        SubgateBank() { reset(); }

        void reset() {
            for (int l = 0; l < LEVELS; ++l)
                for (int d = 0; d < DECS; ++d) {
                    globalTh[l][d]  = kDefaultThreshold;
                    followMain[l][d] = true;
                    globalDelta[l][d] = 0.f;
                }
            for (int v = 0; v < VOICES; ++v)
                for (int l = 0; l < LEVELS; ++l)
                    for (int d = 0; d < DECS; ++d) {
                        ownTh[v][l][d]      = kDefaultThreshold;
                        ownActive[v][l][d]  = false;
                        ownDelta[v][l][d]   = 0.f;
                    }
            deltasActive = false;
            publishSeq[0] = 0;
            publishSeq[1] = 0;
        }

        // ── persistence (the WarpBank::serialize contract: missing keys ⟹ the
        // defaults survive — old patches unchanged, bit-identical). The authored
        // cells only; the delta planes stay session-lived.
        static void serialize(const SubgateBank& b, json_t* root, const char* prefix) {
            const std::string pfx(prefix);
            json_t* jg = json_array();
            for (int l = 0; l < LEVELS; ++l)
                for (int d = 0; d < DECS; ++d)
                    json_array_append_new(jg, json_real((double)b.globalTh[l][d]));
            json_object_set_new(root, (pfx + "Global").c_str(), jg);
            json_t* jf = json_array();
            for (int l = 0; l < LEVELS; ++l)
                for (int d = 0; d < DECS; ++d)
                    json_array_append_new(jf, b.followMain[l][d] ? json_true() : json_false());
            json_object_set_new(root, (pfx + "FollowMain").c_str(), jf);
            json_t* jo = json_array();
            for (int v = 0; v < VOICES; ++v)
                for (int l = 0; l < LEVELS; ++l)
                    for (int d = 0; d < DECS; ++d)
                        json_array_append_new(jo, json_real((double)b.ownTh[v][l][d]));
            json_object_set_new(root, (pfx + "Own").c_str(), jo);
            json_t* ja = json_array();
            for (int v = 0; v < VOICES; ++v)
                for (int l = 0; l < LEVELS; ++l)
                    for (int d = 0; d < DECS; ++d)
                        json_array_append_new(ja, b.ownActive[v][l][d] ? json_true() : json_false());
            json_object_set_new(root, (pfx + "OwnActive").c_str(), ja);
        }

        // Load: RESETS to defaults first (deterministic — never merges into stale
        // state), then overwrites from the keys actually present. Short arrays
        // keep the default tail. Booleans read strictly (json_is_true); a wrong-
        // typed entry keeps the default cell (the MacroTapBank parse convention).
        static void parse(json_t* root, const char* prefix, SubgateBank& out) {
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
            float g[LEVELS * DECS], o[VOICES * LEVELS * DECS];
            bool fm[LEVELS * DECS], oa[VOICES * LEVELS * DECS];
            // Seed the locals with the DEFAULTS first: readReals/readBools only
            // fill the entries actually present, and the copy below lands EVERY
            // cell — absent keys / short arrays / wrong-typed entries must keep
            // the defaults (the old-patch guarantee), not uninitialized garbage
            // (the battery caught exactly that).
            for (int i = 0; i < LEVELS * DECS; ++i) { g[i] = kDefaultThreshold; fm[i] = true; }
            for (int i = 0; i < VOICES * LEVELS * DECS; ++i) { o[i] = kDefaultThreshold; oa[i] = false; }
            readReals((pfx + "Global").c_str(), LEVELS * DECS, g);
            readBools((pfx + "FollowMain").c_str(), LEVELS * DECS, fm);
            readReals((pfx + "Own").c_str(), VOICES * LEVELS * DECS, o);
            readBools((pfx + "OwnActive").c_str(), VOICES * LEVELS * DECS, oa);
            for (int l = 0; l < LEVELS; ++l)
                for (int d = 0; d < DECS; ++d) {
                    out.globalTh[l][d]   = g[lvlDec(l, d)];
                    out.followMain[l][d] = fm[lvlDec(l, d)];
                }
            for (int v = 0; v < VOICES; ++v)
                for (int l = 0; l < LEVELS; ++l)
                    for (int d = 0; d < DECS; ++d) {
                        out.ownTh[v][l][d]     = o[voiceCell(v, l, d)];
                        out.ownActive[v][l][d] = oa[voiceCell(v, l, d)];
                    }
        }
    };

    // ── THE DECISION RANGE (CC_PROMPT_STEP_ADVANCE_CONSUMPTION Stage 2) ───────
    // REST/LEGATO/ACCENT thresholds are probabilities: [0,1]. ADVANCE is the
    // DIRECTION-bearing decision — its resolved threshold is SIGNED [-1,+1]
    // (negative = p(step BACKWARD), the probabilistic retrograde). The STORE
    // knobs stay [0,1] (own-claim/global cells author forward-or-hold); the
    // negative half arrives (a) through the FOLLOW-MAIN tier — the main
    // Variation value is bipolar in gate mode (the unclamped knob+CV2+Junction
    // sum), so a follow-main ADVANCE cell inherits the sign — and (b) through
    // bipolar mod deltas into the ADVANCE mod jacks (a bipolar modulation
    // source makes the sequence WANDER — Rodney's design note). The clamp must
    // NOT rectify that negative half to 0 (= "always hold") — hence the
    // per-decision range. REST/LEGATO/ACCENT keep [0,1] exactly as pinned.
    inline float clampFor(int dec, float x) {
        return (dec == ADVANCE) ? std::clamp(x, -1.0f, 1.0f)
                                : std::clamp(x,  0.0f, 1.0f);
    }

    // ── THE THREE-WAY RESOLVER (pure, the one authority) ──────────────────────
    // Precedence: own (tier 1, claimed) > { follow-main (default) | global } by
    // the switch; the mod delta adds post-resolution, clamped to the DECISION's
    // range (clampFor above). The byte-identity default:
    // resolve(false, ·, true, ·, main, 0, dec) == main, EXACTLY (main is
    // produced inside the decision's range by its writer).
    inline float resolve(bool ownActive, float ownTh, bool followMain, float globalTh,
                         float mainValue, float delta, int dec) {
        const float base = ownActive ? ownTh : (followMain ? mainValue : globalTh);
        return clampFor(dec, base + delta);
    }

} // namespace SubgateKnobs
} // namespace dotModular
