// test_subgate_knobs.cpp — the subgate-tier consumer-group bank + the three-way
// resolver (CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC). Compile: see test/run_all.sh
// (header-only, rack-free — the bank + resolver live in dsp/SubgateKnobs.hpp,
// the same discipline as the WarpBank tests).
//
// Covered:
//   1. BYTE-IDENTITY DEFAULT: resolve(unclaimed, ·, followMain=true, ·, main, 0)
//      == main EXACTLY (the spec's degenerate case — until a knob is touched or
//      a switch flips, every subgate decision plays the main threshold).
//   2. TIER 1: a claimed own cell wins outright (the touch-is-claim law).
//   3. TIER 2a: the switch flipped to global — the global knob drives, the main
//      value is ignored.
//   4. TIER 2b: follow-main — the main value drives, the global knob is ignored.
//   5. MOD DELTA: adds POST-resolution, clamped to the decision's range — [0,1]
//      for REST/LEGATO/ACCENT (a threshold is a probability), [-1,+1] for
//      ADVANCE (the SIGNED step-advance threshold — CC_PROMPT_STEP_ADVANCE_
//      CONSUMPTION Stage 2: negative = p(step BACKWARD), the bipolar main
//      arrives through follow-main, bipolar deltas through the mod jacks);
//      the Causeway path cannot push a threshold out of its decision's range.
//   5b. THE ADVANCE RANGE: follow-main carries a bipolar main verbatim; the
//      clamp floors at -1 (not the 0 = "always hold" rectification); the
//      other three decisions keep [0,1] exactly.
//   6. BANK DEFAULTS: fresh bank = globalTh 0.5, followMain true, ownActive
//      false, deltas 0/inactive, publishSeq 0 — the no-new-surface patch state.
//   7. PERSISTENCE ROUND-TRIP: authored cells serialize/parse deep-equal; the
//      delta planes stay session-lived (NOT persisted); publishSeq resets.
//   8. MISSING KEYS ⟹ DEFAULTS: parsing a patch without the subgate keys keeps
//      the constructed defaults (old patches unchanged, bit-identical).
//   9. SHORT ARRAYS keep the default tail; wrong-typed entries keep the default
//      cell (the MacroTapBank parse convention).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "SubgateKnobs.hpp"

using namespace dotModular::SubgateKnobs;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static void checkBits(float got, float want, const char* what) {
    if (std::memcmp(&got, &want, sizeof(float)) == 0) { ++g_pass; return; }
    std::printf("  FAIL: %s (got %.9g, want %.9g)\n", what, got, want);
    ++g_fail;
}

int main() {
    // ── 1. BYTE-IDENTITY DEFAULT ──────────────────────────────────────────────
    {
        const float main = 0.123f;
        const float r = resolve(/*ownActive=*/false, /*ownTh=*/0.9f, /*followMain=*/true,
                                /*globalTh=*/0.7f, main, /*delta=*/0.f, REST);
        checkBits(r, main, "default resolve == main bit-exact");
        // Across the whole matrix: every (level, dec) cell of a fresh bank
        // resolves to the main value it is fed.
        SubgateBank b;
        bool allIdentity = true;
        for (int l = 0; l < LEVELS; ++l)
            for (int d = 0; d < DECS; ++d) {
                const float mm = 0.1f + 0.05f * (l * DECS + d);
                const float rr = resolve(b.ownActive[3][l][d], b.ownTh[3][l][d],
                                         b.followMain[l][d], b.globalTh[l][d], mm,
                                         b.globalDelta[l][d] + b.ownDelta[3][l][d], d);
                if (std::memcmp(&rr, &mm, sizeof(float)) != 0) allIdentity = false;
            }
        check(allIdentity, "fresh bank resolves identity for every cell");
    }

    // ── 2-4. THE THREE-WAY PRECEDENCE ────────────────────────────────────────
    {
        const float main = 0.123f, own = 0.9f, glob = 0.7f;
        // Tier 1: own claim wins outright — even against a flipped switch.
        checkBits(resolve(true, own, false, glob, main, 0.f, REST), own, "tier 1: own claim wins");
        checkBits(resolve(true, own, true, glob, main, 0.f, REST), own, "tier 1 beats follow-main too");
        // Tier 2a: switch = global (no claim).
        checkBits(resolve(false, own, false, glob, main, 0.f, REST), glob, "tier 2a: global wins when switched");
        // Tier 2b: switch = follow-main (the default).
        checkBits(resolve(false, own, true, glob, main, 0.f, REST), main, "tier 2b: follow-main when not switched");
    }

    // ── 5. MOD DELTA: post-resolution, clamped ───────────────────────────────
    {
        const float main = 0.5f;
        checkBits(resolve(false, 0.f, true, 0.f, main, 0.25f, REST), 0.75f, "delta adds post-resolution");
        check(resolve(false, 0.f, true, 0.f, main, +2.f, REST) == 1.f, "delta clamps high");
        check(resolve(false, 0.f, true, 0.f, main, -2.f, REST) == 0.f, "delta clamps low");
        checkBits(resolve(true, 0.25f, false, 0.f, main, 0.1f, REST), 0.35f, "delta composes with tier 1 too");
    }

    // ── 5b. THE ADVANCE RANGE (CC_PROMPT_STEP_ADVANCE_CONSUMPTION Stage 2) ───
    // ADVANCE is the DIRECTION-bearing decision: its resolved range is SIGNED
    // [-1,+1] — the bipolar main value arrives through follow-main and bipolar
    // deltas through the mod jacks, and the clamp must NOT rectify the negative
    // half to 0 (= "always hold"). REST/LEGATO/ACCENT stay [0,1] as pinned.
    {
        const float bipolarMain = -0.4f;
        checkBits(resolve(false, 0.f, true, 0.f, bipolarMain, 0.f, ADVANCE), bipolarMain,
                  "ADVANCE: follow-main carries the bipolar main value verbatim");
        check(resolve(false, 0.f, true, 0.f, bipolarMain, -2.f, ADVANCE) == -1.f,
              "ADVANCE: clamps low at -1 (NOT rectified to 0)");
        check(resolve(false, 0.f, true, 0.f, bipolarMain, +2.f, ADVANCE) == 1.f,
              "ADVANCE: clamps high at +1");
        checkBits(resolve(false, 0.f, true, 0.f, 0.5f, -0.6f, ADVANCE), 0.5f - 0.6f,
                  "ADVANCE: a bipolar delta reaches the negative half (own/global too)");
        const float m2 = -0.75f;
        checkBits(resolve(false, 0.f, true, 0.f, m2, 0.f, ADVANCE), m2,
                  "ADVANCE: the byte-identity default holds across the signed range");
        // The other three decisions keep the probability clamp — a negative main
        // rectifies to 0 there (a rate is unipolar).
        check(resolve(false, 0.f, true, 0.f, bipolarMain, 0.f, REST) == 0.f,
              "REST: still clamps [0,1] — a negative main rectifies");
        check(resolve(false, 0.f, true, 0.f, bipolarMain, 0.f, LEGATO) == 0.f,
              "LEGATO: still clamps [0,1]");
        check(resolve(false, 0.f, true, 0.f, bipolarMain, 0.f, ACCENT) == 0.f,
              "ACCENT: still clamps [0,1]");
    }

    // ── 6. BANK DEFAULTS ─────────────────────────────────────────────────────
    {
        SubgateBank b;
        bool ok = true;
        for (int l = 0; l < LEVELS; ++l)
            for (int d = 0; d < DECS; ++d) {
                ok = ok && b.globalTh[l][d] == kDefaultThreshold && b.followMain[l][d]
                        && b.globalDelta[l][d] == 0.f;
            }
        check(ok, "bank defaults: globals 0.5 + followMain + zero deltas");
        bool ok2 = true;
        for (int v = 0; v < VOICES; ++v)
            for (int l = 0; l < LEVELS; ++l)
                for (int d = 0; d < DECS; ++d)
                    ok2 = ok2 && !b.ownActive[v][l][d] && b.ownDelta[v][l][d] == 0.f;
        check(ok2, "bank defaults: no claims, no own deltas");
        check(!b.deltasActive && b.publishSeq[0] == 0 && b.publishSeq[1] == 0,
              "bank defaults: deltas inactive, heartbeats zero");
    }

    // ── 7. PERSISTENCE ROUND-TRIP ────────────────────────────────────────────
    {
        SubgateBank a;
        a.globalTh[GHOST][REST] = 0.8f;
        a.globalTh[RATCHET][ADVANCE] = 0.15f;
        a.followMain[GHOST][REST] = false;
        a.followMain[RATCHET][LEGATO] = false;
        a.ownTh[0][RATCHET][REST] = 0.33f;       // slot 0 = the V1 mono row
        a.ownTh[7][GHOST][ACCENT]  = 0.62f;
        a.ownActive[0][RATCHET][REST] = true;
        a.ownActive[15][GHOST][ADVANCE] = true;  // slot 15 = V16
        a.globalDelta[RATCHET][REST] = 0.4f;     // session-lived: must NOT persist
        a.ownDelta[7][GHOST][ACCENT] = -0.2f;
        a.deltasActive = true;
        ++a.publishSeq[0]; ++a.publishSeq[1];

        json_t* root = json_object();
        SubgateBank::serialize(a, root, "editorSubgate");

        SubgateBank b;   // fresh — parse must reset then overwrite
        b.globalTh[RATCHET][REST] = 0.123f;   // stale state the parse must clear
        SubgateBank::parse(root, "editorSubgate", b);

        bool ok = true;
        for (int l = 0; l < LEVELS; ++l)
            for (int d = 0; d < DECS; ++d)
                ok = ok && b.globalTh[l][d] == a.globalTh[l][d]
                        && b.followMain[l][d] == a.followMain[l][d];
        check(ok, "round-trip: globals + switches deep-equal");
        bool ok2 = true;
        for (int v = 0; v < VOICES; ++v)
            for (int l = 0; l < LEVELS; ++l)
                for (int d = 0; d < DECS; ++d)
                    ok2 = ok2 && b.ownTh[v][l][d] == a.ownTh[v][l][d]
                            && b.ownActive[v][l][d] == a.ownActive[v][l][d];
        check(ok2, "round-trip: own cells deep-equal");
        check(b.globalTh[GHOST][REST] == 0.8f && b.ownTh[7][GHOST][ACCENT] == 0.62f
              && b.ownActive[15][GHOST][ADVANCE] && !b.ownActive[14][GHOST][ADVANCE],
              "round-trip: spot cells landed");
        check(!b.deltasActive && b.globalDelta[RATCHET][REST] == 0.f
              && b.ownDelta[7][GHOST][ACCENT] == 0.f,
              "deltas stay session-lived (not persisted)");
        check(b.publishSeq[0] == 0 && b.publishSeq[1] == 0, "heartbeats reset on load");

        // The spec's tier checks THROUGH the resolver, post-restore:
        checkBits(resolve(b.ownActive[0][RATCHET][REST], b.ownTh[0][RATCHET][REST],
                          b.followMain[RATCHET][REST], b.globalTh[RATCHET][REST],
                          /*main=*/0.9f, 0.f, REST),
                  0.33f, "restored V1 ratchet-rest claim resolves to its own knob");
        checkBits(resolve(b.ownActive[0][GHOST][REST], b.ownTh[0][GHOST][REST],
                          b.followMain[GHOST][REST], b.globalTh[GHOST][REST],
                          /*main=*/0.9f, 0.f, REST),
                  0.8f, "restored ghost-rest global switch resolves to the global knob");
        checkBits(resolve(b.ownActive[0][RATCHET][ADVANCE], b.ownTh[0][RATCHET][ADVANCE],
                          b.followMain[RATCHET][ADVANCE], b.globalTh[RATCHET][ADVANCE],
                          /*main=*/0.42f, 0.f, ADVANCE),
                  0.42f, "untouched ratchet-advance follows main");

        json_decref(root);
    }

    // ── 8. MISSING KEYS ⟹ DEFAULTS (old patches unchanged) ───────────────────
    {
        json_t* root = json_object();
        SubgateBank b;
        SubgateBank::parse(root, "editorSubgate", b);
        bool ok = true;
        for (int l = 0; l < LEVELS; ++l)
            for (int d = 0; d < DECS; ++d)
                ok = ok && b.globalTh[l][d] == kDefaultThreshold && b.followMain[l][d];
        check(ok, "absent keys: constructed defaults survive");
        json_decref(root);
    }

    // ── 9. SHORT ARRAYS + WRONG-TYPED ENTRIES ────────────────────────────────
    {
        json_t* root = json_object();
        json_t* g = json_array();
        json_array_append_new(g, json_real(0.25f));   // only ONE cell — tail keeps defaults
        json_array_append_new(g, json_string("junk")); // wrong type: keeps default
        json_object_set_new(root, "editorSubgateGlobal", g);
        json_t* f = json_array();
        json_array_append_new(f, json_false());       // only the first switch flips
        json_object_set_new(root, "editorSubgateFollowMain", f);

        SubgateBank b;
        SubgateBank::parse(root, "editorSubgate", b);
        checkBits(b.globalTh[0][0], 0.25f, "short array: first cell parsed");
        check(b.globalTh[0][1] == kDefaultThreshold, "short array: default tail survives");
        check(b.globalTh[0][2] == kDefaultThreshold, "wrong-typed entry keeps the default cell");
        check(!b.followMain[0][0], "bool read strictly: false parsed");
        check(b.followMain[0][1], "bool tail default true");
        json_decref(root);
    }

    std::printf("test_subgate_knobs: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
