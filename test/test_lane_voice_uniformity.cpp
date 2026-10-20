// test_lane_voice_uniformity.cpp — SEAM_ERADICATION_PLAN.md Stage 0: the UNIFORMITY HARNESS.
//
// The one defect the plan hunts: a per-(voice,lane) distinction the code still branches on in
// some places and not others ("no voice special, no lane special"). This harness loops ALL
// 16 voiceSlots × ALL 7 lanes for each per-(voice,lane) feature, sets a DISTINCT value on ONE
// cell through the real headless write path, drives the real rebuild/promote, and asserts:
//   (a) the consumer for (v,l) reads IT            — no DROP
//   (b) every other (v',l') cell is UNCHANGED      — no LEAK
// with slot 0 (V1) and lanes 5,6 (VAR/LEG) exercised EXPLICITLY — the cells current bugs
// distinguish (Macro→East leak, V1 asymmetry, VAR/LEG second-class).
//
// EXIT CONTRACT (the Stage-0 gate):
//   - Failures matching an entry in KNOWN_SEAMS print as "KNOWN SEAM (expected red)" with the
//     plan stage that closes them. They do NOT fail CI by default: the harness is RED on
//     today's code BY DESIGN (it catches the warp consumer gate + the direction slot-0 split),
//     and run_all.sh green must keep gating every OTHER task.
//   - UNEXPECTED failures exit 1 — that is the harness doing its job (a seam nobody mapped).
//   - UNIFORMITY_STRICT=1 in the environment fails on ANY failure (use it as the per-stage
//     gate: a stage is done when its manifest entries are DELETED and strict passes).
//   Each later stage DELETES its manifest lines. The manifest only ever shrinks.
//
// Headless reach (honest scope): the harness drives the ENGINE + headless-constructible
// MANAGER layers (PatternEngine/SequencerEngine/WarpBank/SpreadManager/SpreadResolver/
// SandsTopology). The Monsoon-module-layer sites (MonsoonSandsManager's warpDelegated_ mirror
// from topo.owner, MonsoonExpanderManager's direction push + combineLOR, the editor store
// accessors) need live expander modules and are listed in the RESIDUALS block below — they
// are Stage-1 (SEAM_AUDIT_RESULT.md) static-audit + Rack feel-test surface, per the plan's
// own "needs Rack feel-test" rule.
//
// Companion sources: PatternEngine.cpp, SequencerEngine.cpp, GateState.cpp (run_all.sh).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "WarpF.hpp"
#include "managers/SpreadManager.hpp"
#include "SpreadInterp.hpp"
#include "SpreadResolver.hpp"
#include "SandsTopology.hpp"
#include "VoiceResolver.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

using dotModular::WarpBank;
using redDot::SpreadManager;

// ── failure ledger + KNOWN_SEAMS manifest ──────────────────────────────────────

struct Fail {
    std::string feature;   // section id, e.g. "U3-warp-consumer"
    int v = -1, l = -1;    // the (voiceSlot, lane) cell under test, -1 = n/a
    std::string what;
};

static std::vector<Fail> g_fails;
static int g_checks = 0;

static void expectCell(bool ok, const std::string& feature, int v, int l, const std::string& what) {
    ++g_checks;
    if (!ok) g_fails.push_back({feature, v, l, what});
}

struct KnownSeam {
    const char* id;
    const char* closedBy;   // the plan stage / merge that deletes this entry
    const char* why;        // grounded one-liner (code-verified on HEAD)
    bool (*match)(const Fail&);
};

// The manifest. Stage discipline: each stage DELETES its entries; nothing is ever added
// without a plan-stage pointer. A green that required adding an entry is NOT a fix.
static bool seam_warp_consumer(const Fail& f) {
    return f.feature == "U3-warp-consumer" && f.l >= 0
        && (f.l == PatternEngine::PL_QMIX || f.l == PatternEngine::PL_REST
            || f.l == PatternEngine::PL_ACCENT || f.l == PatternEngine::PL_VARIATION
            || f.l == PatternEngine::PL_LEGATO);
}
static bool seam_dir_slot0(const Fail& f) {
    return f.feature == "U7-direction-promote" && f.v == 0;
}

static const std::vector<KnownSeam>& knownSeams() {
    static const std::vector<KnownSeam> K = {
        { "WARP-CONSUMER-GATE",
          "route ALL lanes through warpRow (feat/warp-ui CC_PROMPT_05, unmerged) — then delete",
          "SequencerEngine.hpp:542 polyRandomSrc returns pe.warpRow ONLY for PL_MELODY/PL_OCTAVE; "
          "every other lane reads the raw bank even when a warp is authored+mirrored (boolean "
          "lanes deferred by proof, -18b §3; q-mix with the w send/tap routing). S6 of "
          "test_warp_mirror pins the same state; this is its full-matrix form.",
          seam_warp_consumer },
        { "DIR-SLOT0-ROW-INERT",
          "Stage 3 (unify executeStep/executePolyVoice into ONE per-voice path) — then delete",
          "advancePlayhead promotes laneDirVPending_ for v=1..numPolyVoices ONLY "
          "(SequencerEngine.cpp:189/306); slot 0's poly row is never promoted — V1 is driven by "
          "the separate mono channel laneDirPending_→laneDir_. The mono/poly execution-path "
          "split (Seam 1) in miniature, pinned here until the paths unify.",
          seam_dir_slot0 },
    };
    return K;
}

// ── deterministic fixtures ─────────────────────────────────────────────────────

static float golden(long idx) {   // well-spread [0,1), distinct per (v,l,s)
    return (float)std::fmod(0.125 + idx * 0.6180339887498949, 1.0);
}

static void fillRandomGolden(PatternEngine& pe) {
    for (int v = 0; v < 16; ++v)
        for (int l = 0; l < 7; ++l)
            for (int s = 0; s < 16; ++s)
                pe.random_[v][l][s] = golden((long)v * 112 + l * 16 + s);
}

static void fillSlewFamilies(PatternEngine& pe) {
    // Distinct per (v,l,s) values in every slewed family, so spread-interp isolation
    // asserts are meaningful (original != target for poly voices; V1's original == mono row).
    const long l16[7] = {0, 1, 2, 3, 4, 5, 6};
    for (int v = 0; v < 16; ++v)
        for (int s = 0; s < 16; ++s) {
            const long base = (long)v * 112;
            pe.slewedRhythm[v][s]     = golden(base + l16[3] * 16 + s + 11);
            pe.slewedMelody[v][s]     = golden(base + l16[0] * 16 + s + 11);
            pe.slewedOctave[v][s]     = golden(base + l16[1] * 16 + s + 11);
            pe.slewedQmix[v][s]       = golden(base + l16[2] * 16 + s + 11);
            pe.slewedAccent[v][s]     = golden(base + l16[4] * 16 + s + 11);
            pe.slewedVariation[v][s]  = golden(base + l16[5] * 16 + s + 11);
            pe.slewedLegato[v][s]     = golden(base + l16[6] * 16 + s + 11);
            pe.pubSlewedPolyRhythm[v][s]    = pe.slewedRhythm[v][s];
            pe.pubSlewedPolyMelody[v][s]    = pe.slewedMelody[v][s];
            pe.pubSlewedPolyOctave[v][s]    = pe.slewedOctave[v][s];
            pe.pubSlewedPolyQmix[v][s]      = pe.slewedQmix[v][s];
            pe.pubSlewedPolyAccent[v][s]    = pe.slewedAccent[v][s];
            pe.pubSlewedPolyVariation[v][s] = pe.slewedVariation[v][s];
            pe.pubSlewedPolyLegato[v][s]    = pe.slewedLegato[v][s];
            pe.preRemapSlewedRhythm[v][s]    = pe.slewedRhythm[v][s];
            pe.preRemapSlewedMelody[v][s]    = pe.slewedMelody[v][s];
            pe.preRemapSlewedOctave[v][s]    = pe.slewedOctave[v][s];
            pe.preRemapSlewedQmix[v][s]      = pe.slewedQmix[v][s];
            pe.preRemapSlewedAccent[v][s]    = pe.slewedAccent[v][s];
            pe.preRemapSlewedVariation[v][s] = pe.slewedVariation[v][s];
            pe.preRemapSlewedLegato[v][s]    = pe.slewedLegato[v][s];
        }
}

static const char* LANE_NAME[7] = { "MEL", "OCT", "QMIX", "REST", "ACC", "VAR", "LEG" };

// ══════════════════════════════════════════════════════════════════════════════
// U1 — WARP own-slot: bank → rebuildWarpMirrorAll → warpRow. 16×7 cells.
// Distinct steep curve @ w=1 on ONE cell; the mirror must carry it THERE and
// nowhere else. V1 (slot 0) flows through the same loop — no mono special.
// ══════════════════════════════════════════════════════════════════════════════
static void U1_warp_own_slot() {
    const std::string F = "U1-warp-own-slot";
    for (int v0 = 0; v0 < 16; ++v0)
        for (int l0 = 0; l0 < 7; ++l0) {
            PatternEngine pe; fillRandomGolden(pe);
            WarpBank bank;
            bank.warpF[WarpBank::index(v0, l0)] = dotModular::KumaraswamyWarp{3, 12.0};
            bank.warpW[WarpBank::index(v0, l0)] = 1.f;
            pe.setWarpBank(&bank);
            pe.rebuildWarpMirrorAll();

            bool landed = false;
            for (int s = 0; s < 16; ++s)
                if (pe.warped_[v0][l0][s] != pe.random_[v0][l0][s]) { landed = true; break; }
            expectCell(landed, F, v0, l0, "warp authored on (v,l) did not land in the mirror (drop)");

            bool leak = false; int lv = -1, ll = -1, ls = -1;
            for (int v = 0; v < 16 && !leak; ++v)
                for (int l = 0; l < 7 && !leak; ++l) {
                    if (v == v0 && l == l0) continue;
                    for (int s = 0; s < 16; ++s)
                        if (pe.warped_[v][l][s] != pe.random_[v][l][s]) {
                            leak = true; lv = v; ll = l; ls = s; break;
                        }
                }
            char buf[128];
            std::snprintf(buf, sizeof buf, "mirror leaked to (%d,%d) step %d", lv, ll, ls);
            expectCell(!leak, F, v0, l0, buf);
        }
}

// ══════════════════════════════════════════════════════════════════════════════
// U2 — WARP delegation: the flag MonsoonSandsManager mirrors from topo.owner==MACRO
// (MonsoonSandsManager.cpp:144-155). warpDelegated_[v][l]=true → the EFFECTIVE warp
// for (v,l) is Macro's global curve, for EVERY voice including 0, every lane.
// ══════════════════════════════════════════════════════════════════════════════
static void U2_warp_delegation() {
    const std::string F = "U2-warp-delegation";
    for (int l0 = 0; l0 < 7; ++l0) {
        PatternEngine pe; fillRandomGolden(pe);
        WarpBank bank;
        bank.globalWarpF[l0] = dotModular::TriangularWarp{0.75};
        bank.globalWarpW[l0] = 1.f;
        pe.setWarpBank(&bank);
        for (int v0 = 0; v0 < 16; ++v0) {
            pe.setWarpDelegated(v0, l0, true);
            pe.rebuildWarpMirrorAll();

            bool ok = true; char buf[160]; buf[0] = 0;
            for (int s = 0; s < 16 && ok; ++s) {
                const double u = (double)pe.random_[v0][l0][s];
                const float want = (float)bank.warpedValue(v0, l0, u, /*delegated=*/true);
                if (pe.warped_[v0][l0][s] != want) {
                    std::snprintf(buf, sizeof buf, "delegated cell step %d: got %.9g want %.9g",
                                  s, (double)pe.warped_[v0][l0][s], (double)want);
                    ok = false;
                }
            }
            expectCell(ok, F, v0, l0, buf);

            bool leak = false; int lv = -1;
            for (int v = 0; v < 16 && !leak; ++v) {
                if (v == v0) continue;
                for (int s = 0; s < 16; ++s)
                    if (pe.warped_[v][l0][s] != pe.random_[v][l0][s]) { leak = true; lv = v; break; }
            }
            std::snprintf(buf, sizeof buf, "delegation of voice %d leaked to voice %d (lane %d)",
                          v0, lv, l0);
            expectCell(!leak, F, v0, l0, buf);

            pe.setWarpDelegated(v0, l0, false);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// U3 — WARP consumer gate: polyRandomSrc must hand EVERY lane's consumer the
// mirror row (same row object), for every voice. RED for lanes 2..6 on HEAD —
// KNOWN SEAM "WARP-CONSUMER-GATE" (feat/warp-ui CC_PROMPT_05 routes all lanes).
// ══════════════════════════════════════════════════════════════════════════════
static void U3_warp_consumer_gate() {
    const std::string F = "U3-warp-consumer";
    SequencerEngine se; fillRandomGolden(se.pe);
    WarpBank bank; se.pe.setWarpBank(&bank);
    for (int v0 = 0; v0 < 16; ++v0)
        for (int l0 = 0; l0 < 7; ++l0) {
            for (int i = 0; i < WarpBank::N; ++i) {
                bank.warpF[i] = dotModular::IdentityWarp{}; bank.warpW[i] = 0.f;
            }
            bank.warpF[WarpBank::index(v0, l0)] = dotModular::KumaraswamyWarp{3, 12.0};
            bank.warpW[WarpBank::index(v0, l0)] = 1.f;
            se.pe.rebuildWarpMirrorAll();

            const float (&src)[16] = se.polyRandomSrc(v0, l0);
            const bool sameRow = (&src[0] == &se.pe.warpRow(v0, l0)[0]);
            char buf[128];
            std::snprintf(buf, sizeof buf, "polyRandomSrc(%d,%s) is the RAW row, not the mirror",
                          v0, LANE_NAME[l0]);
            expectCell(sameRow, F, v0, l0, buf);
        }
}

// ══════════════════════════════════════════════════════════════════════════════
// U4 — WARP dirty-mask scopes: each scoped rebuild must reproduce the full
// rebuild EXACTLY inside its scope and leave every cell outside bit-identical
// (no over-reach, no under-reach) — across the WHOLE 16×7×16 matrix.
// ══════════════════════════════════════════════════════════════════════════════
static void U4_warp_dirty_scopes() {
    const std::string F = "U4-warp-dirty-scope";
    PatternEngine pe; fillRandomGolden(pe);
    WarpBank bank;
    // All 112 cells authored DISTINCT (curve kind + params vary per cell).
    const dotModular::WarpF curves[4] = {
        dotModular::PowerWarp{2}, dotModular::TriangularWarp{0.3},
        dotModular::KumaraswamyWarp{2, 6}, dotModular::MobiusWarp{0.7},
    };
    for (int i = 0; i < WarpBank::N; ++i) {
        bank.warpF[i] = curves[i % 4];
        bank.warpW[i] = 0.5f + 0.5f * (float)((i * 7) % 11) / 10.f;   // w in [0.5, 1]
    }
    pe.setWarpBank(&bank);
    pe.rebuildWarpMirrorAll();
    static float ref[16][7][16];
    std::memcpy(ref, pe.warped_, sizeof ref);

    // Determinism: a second full rebuild is byte-identical (the oracle is stable).
    pe.rebuildWarpMirrorAll();
    expectCell(std::memcmp(ref, pe.warped_, sizeof ref) == 0, F, -1, -1,
               "rebuildWarpMirrorAll not deterministic — oracle unstable");

    // Voice scope: row v0 recomputed == oracle, all other rows bit-untouched.
    {
        const int v0 = 5;
        pe.rebuildWarpMirrorVoice(v0);
        bool ok = true; char buf[128]; buf[0] = 0;
        for (int v = 0; v < 16 && ok; ++v)
            for (int l = 0; l < 7 && ok; ++l)
                for (int s = 0; s < 16; ++s)
                    if (pe.warped_[v][l][s] != ref[v][l][s]) {
                        std::snprintf(buf, sizeof buf,
                                      "rebuildWarpMirrorVoice(%d) moved cell (%d,%d,%d)", v0, v, l, s);
                        ok = false; break;
                    }
        expectCell(ok, F, v0, -1, buf);
    }
    // VoiceLane scope: exactly that row-lane may move (it recomputes to the same value here).
    {
        const int v0 = 9, l0 = 3;
        pe.rebuildWarpMirrorVoiceLane(v0, l0);
        bool ok = true; char buf[128]; buf[0] = 0;
        for (int v = 0; v < 16 && ok; ++v)
            for (int l = 0; l < 7 && ok; ++l)
                for (int s = 0; s < 16; ++s)
                    if (pe.warped_[v][l][s] != ref[v][l][s]) {
                        std::snprintf(buf, sizeof buf,
                                      "rebuildWarpMirrorVoiceLane(%d,%d) moved cell (%d,%d,%d)",
                                      v0, l0, v, l, s);
                        ok = false; break;
                    }
        expectCell(ok, F, v0, l0, buf);
    }
    // Cell scope: exactly ONE cell.
    {
        const int v0 = 2, l0 = 6, s0 = 7;
        pe.rebuildWarpMirrorCell(v0, l0, s0);
        bool ok = true; char buf[128]; buf[0] = 0;
        for (int v = 0; v < 16 && ok; ++v)
            for (int l = 0; l < 7 && ok; ++l)
                for (int s = 0; s < 16; ++s)
                    if (pe.warped_[v][l][s] != ref[v][l][s]) {
                        std::snprintf(buf, sizeof buf,
                                      "rebuildWarpMirrorCell(%d,%d,%d) moved cell (%d,%d,%d)",
                                      v0, l0, s0, v, l, s);
                        ok = false; break;
                    }
        expectCell(ok, F, v0, l0, buf);
    }
    // LaneDelegating scope with NOBODY delegated: an empty scope must move NOTHING.
    {
        pe.rebuildWarpMirrorLaneDelegating(4);
        expectCell(std::memcmp(ref, pe.warped_, sizeof ref) == 0, F, -1, 4,
                   "rebuildWarpMirrorLaneDelegating moved cells with zero delegating voices");
    }
    // LaneDelegating POSITIVE case (fresh basis): delegate voices {0,3,7} on lane 2,
    // move EVERY voice's random_ basis on that lane; the scope must refresh exactly
    // the delegating voices' lane rows to the fresh oracle and leave the rest stale.
    {
        const int lane = 2;
        pe.setWarpDelegated(0, lane, true);
        pe.setWarpDelegated(3, lane, true);
        pe.setWarpDelegated(7, lane, true);
        static float basis[16][16];
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 16; ++s) {
                basis[v][s] = golden(500000 + v * 16 + s);
                pe.random_[v][lane][s] = basis[v][s];
            }
        pe.rebuildWarpMirrorLaneDelegating(lane);
        bool ok = true; char buf[160]; buf[0] = 0;
        for (int v = 0; v < 16 && ok; ++v) {
            const bool delegating = (v == 0 || v == 3 || v == 7);
            for (int s = 0; s < 16 && ok; ++s) {
                if (delegating) {
                    const float want = (float)bank.warpedValue(v, lane, (double)basis[v][s], true);
                    if (pe.warped_[v][lane][s] != want) {
                        std::snprintf(buf, sizeof buf,
                                      "delegating voice %d lane %d not refreshed to fresh basis", v, lane);
                        ok = false;
                    }
                } else if (pe.warped_[v][lane][s] != ref[v][lane][s]) {
                    std::snprintf(buf, sizeof buf,
                                  "non-delegating voice %d lane %d refreshed (over-reach)", v, lane);
                    ok = false;
                }
            }
            if (ok)
                for (int l = 0; l < 7 && ok; ++l) {
                    if (l == lane) continue;
                    for (int s = 0; s < 16; ++s)
                        if (pe.warped_[v][l][s] != ref[v][l][s]) {
                            std::snprintf(buf, sizeof buf,
                                          "lane %d edit leaked to lane %d (voice %d)", lane, l, v);
                            ok = false; break;
                        }
                }
        }
        expectCell(ok, F, -1, lane, buf);
        pe.setWarpDelegated(0, lane, false);
        pe.setWarpDelegated(3, lane, false);
        pe.setWarpDelegated(7, lane, false);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// U5 — SPREAD store + interp isolation (SpreadManager — the real Sands write
// surface): a distinct amount on ONE (v,l) moves THAT cell's interpolated value
// (vs its SpreadInterp oracle) and no other cell. V1 (slot 0) is the mono
// self-target for POSITIVE spread (documented no-op) — exercised via NEGATIVE
// spread, which must INVERT for V1 exactly like any voice.
// ══════════════════════════════════════════════════════════════════════════════
static void U5_spread_isolation() {
    const std::string F = "U5-spread-isolation";
    PatternEngine pe; fillSlewFamilies(pe);
    SpreadManager sm(&pe, 16, 0);

    for (int v0 = 0; v0 < 16; ++v0)
        for (int l0 = 0; l0 < 7; ++l0) {
            for (int v = 0; v < 16; ++v)
                for (int l = 0; l < 7; ++l) sm.setSpread(v, l, 0.f);
            const float amount = (v0 == 0) ? -0.75f : 0.75f;
            sm.setSpread(v0, l0, amount);
            if (sm.getSpread(v0, l0) != amount) {
                expectCell(false, F, v0, l0, "SpreadManager store round-trip failed");
                continue;
            }

            bool ok = true; char buf[160]; buf[0] = 0;
            // (a) the target cell matches the SpreadInterp oracle for its amount...
            for (int s = 0; s < 16 && ok; ++s) {
                const float original = redDot::SpreadInterp::polySlewedPub(pe, l0, v0, s);
                const float want = redDot::SpreadInterp::applyAnchorV1Only(pe, l0, s, original, amount);
                const float got = sm.getInterpolatedValue(v0, l0, s);
                if (got != want) {
                    std::snprintf(buf, sizeof buf, "cell (%d,%d) step %d: got %.9g want %.9g",
                                  v0, l0, s, (double)got, (double)want);
                    ok = false;
                }
            }
            // ...and actually MOVED (no drop) — for at least one step.
            if (ok) {
                bool moved = false;
                for (int s = 0; s < 16 && !moved; ++s)
                    if (sm.getInterpolatedValue(v0, l0, s)
                        != redDot::SpreadInterp::polySlewedPub(pe, l0, v0, s)) moved = true;
                if (!moved) {
                    std::snprintf(buf, sizeof buf,
                                  "spread %.2f on (%d,%s) moved nothing (self-target or drop)",
                                  (double)amount, v0, LANE_NAME[l0]);
                    ok = false;
                }
            }
            // (b) every OTHER cell still reads its own raw draw (no leak).
            for (int v = 0; v < 16 && ok; ++v)
                for (int l = 0; l < 7 && ok; ++l) {
                    if (v == v0 && l == l0) continue;
                    for (int s = 0; s < 16; ++s) {
                        const float orig = redDot::SpreadInterp::polySlewedPub(pe, l, v, s);
                        if (sm.getInterpolatedValue(v, l, s) != orig) {
                            std::snprintf(buf, sizeof buf,
                                          "(%d,%s) spread leaked into (%d,%s) step %d",
                                          v0, LANE_NAME[l0], v, LANE_NAME[l], s);
                            ok = false; break;
                        }
                    }
                }
            expectCell(ok, F, v0, l0, buf);
        }
}

// ══════════════════════════════════════════════════════════════════════════════
// U6 — SPREAD RESOLVER: the delegation algebra (SpreadResolver::effective) has
// NO lane table — identical Inputs give identical outputs for every lane, both
// branches. (Lane ORDER risk lives in the callers; the resolver must be inert.)
// ══════════════════════════════════════════════════════════════════════════════
static void U6_spread_resolver_lane_free() {
    const std::string F = "U6-spread-resolver";
    using redDot::SpreadResolver;
    SpreadResolver::Inputs in;
    in.base = 0.3f;
    in.ownCv  = {true, 0.5f, 0.4f};    // +0.2 after scaleCv ×2
    in.eastCv = {true, 1.0f, -0.3f};   // -0.6
    in.macroPresent = true;
    in.macroBase = 0.1f;
    in.macroSend = 0.8f;
    in.macroPreDelta = 0.05f;
    in.macroPostDelta = 0.25f;
    in.macroTap = 0.25f;

    float refFree = 0.f, refDeleg = 0.f;
    for (int l = 0; l < 7; ++l) {
        in.delegated = false;
        const float f = SpreadResolver::effective(in);
        in.delegated = true;
        const float d = SpreadResolver::effective(in);
        if (l == 0) { refFree = f; refDeleg = d; }
        char buf[96];
        std::snprintf(buf, sizeof buf, "non-delegated effective differs on lane %d", l);
        expectCell(f == refFree, F, -1, l, buf);
        std::snprintf(buf, sizeof buf, "delegated effective differs on lane %d", l);
        expectCell(d == refDeleg, F, -1, l, buf);
    }
    // And the algebra itself (SpreadResolver.hpp:90-103): delegated =
    // clamp(macroBase + tappedDelta); free = clamp(base + ownCv + eastCv + tappedSend).
    // NOTE: CvTerm::contribution() ALREADY carries the ×2 kBipolarSpan (:47) — the real
    // header differs from the SandsModifierResolver_SKETCH (which moves ×2 into
    // SpreadPolicy::scaleCv); reconcile during the extraction step.
    const float td = in.macroPreDelta + (in.macroPostDelta - in.macroPreDelta) * in.macroTap;
    expectCell(refDeleg == std::clamp(in.macroBase + td, -1.f, 1.f), F, -1, -1,
               "delegated branch != clamp(macroBase + tappedDelta)");
    expectCell(refFree == std::clamp(in.base + in.ownCv.contribution()
                                     + in.eastCv.contribution() + td * in.macroSend, -1.f, 1.f),
               F, -1, -1, "non-delegated branch != clamp(base + ownCv + eastCv + tappedSend)");
}

// ══════════════════════════════════════════════════════════════════════════════
// U7 — DIRECTION promotion (engine boundary): the exact store the manager push
// writes (MonsoonExpanderManager.cpp:398 laneDirVPending_[v][l] = ...) must
// promote uniformly for EVERY (v,l) — and the mono channel for every lane.
// Slot 0's poly row is INERT on HEAD → KNOWN SEAM "DIR-SLOT0-ROW-INERT".
// ══════════════════════════════════════════════════════════════════════════════
static void U7_direction_promote() {
    const std::string F = "U7-direction-promote";
    const int NP = 15;
    for (int v0 = 0; v0 < 16; ++v0)
        for (int l0 = 0; l0 < 7; ++l0) {
            SequencerEngine se;
            se.numPolyVoices = NP;
            se.laneFlipQuant = SequencerEngine::LaneFlipQuant::StepEdge;   // promote every advance
            se.locked = false;

            for (int v = 0; v < 16; ++v)
                for (int l = 0; l < 7; ++l) {
                    se.laneDirVPending_[v][l] = SequencerEngine::LaneDir::Forward;
                    se.laneDirV_[v][l]        = SequencerEngine::LaneDir::Forward;
                }
            for (int l = 0; l < 7; ++l) {
                se.laneDirPending_[l] = SequencerEngine::LaneDir::Forward;
                se.laneDir_[l]        = SequencerEngine::LaneDir::Forward;
            }

            se.laneDirVPending_[v0][l0] = SequencerEngine::LaneDir::Reverse;
            se.advancePlayhead(+1);

            char buf[160];
            if (v0 >= 1) {
                std::snprintf(buf, sizeof buf, "pending Reverse for (%d,%s) did not promote",
                              v0, LANE_NAME[l0]);
                expectCell(se.laneDirV_[v0][l0] == SequencerEngine::LaneDir::Reverse, F, v0, l0, buf);
            } else {
                // KNOWN SEAM: slot 0's poly row is never promoted (v loop starts at 1).
                expectCell(se.laneDirV_[0][l0] == SequencerEngine::LaneDir::Forward, F, 0, l0,
                           "slot-0 poly row promoted (unexpected — manifest says inert)");
            }

            bool leak = false; int lv = -1, ll = -1;
            for (int v = 1; v <= NP && !leak; ++v)
                for (int l = 0; l < 7 && !leak; ++l) {
                    if (v == v0 && l == l0) continue;
                    if (se.laneDirV_[v][l] != SequencerEngine::LaneDir::Forward) {
                        leak = true; lv = v; ll = l;
                    }
                }
            std::snprintf(buf, sizeof buf, "direction of (%d,%s) leaked to (%d,%s)",
                          v0, LANE_NAME[l0], lv, (ll >= 0 ? LANE_NAME[ll] : "?"));
            expectCell(!leak, F, v0, l0, buf);
            expectCell(se.laneDir_[l0] == SequencerEngine::LaneDir::Forward, F, v0, l0,
                       "poly-row pending write perturbed the MONO channel");
        }

    // Mono channel: every lane promotes; poly rows untouched.
    for (int l0 = 0; l0 < 7; ++l0) {
        SequencerEngine se;
        se.numPolyVoices = NP;
        se.laneFlipQuant = SequencerEngine::LaneFlipQuant::StepEdge;
        se.locked = false;
        se.laneDirPending_[l0] = SequencerEngine::LaneDir::Reverse;
        se.advancePlayhead(+1);
        char buf[96];
        std::snprintf(buf, sizeof buf, "mono laneDirPending_(%s) did not promote", LANE_NAME[l0]);
        expectCell(se.laneDir_[l0] == SequencerEngine::LaneDir::Reverse, F, 0, l0, buf);
        bool polyTouched = false;
        for (int v = 1; v <= NP && !polyTouched; ++v)
            for (int l = 0; l < 7 && !polyTouched; ++l)
                if (se.laneDirV_[v][l] != SequencerEngine::LaneDir::Forward) polyTouched = true;
        expectCell(!polyTouched, F, 0, l0, "mono pending write perturbed a poly row");
    }

    // Sign derivation uniformity: polyLaneSign is lane/voice-agnostic (0/+1 → +1, <0 → -1).
    {
        SequencerEngine se;
        bool ok = true; char buf[128]; buf[0] = 0;
        for (int v = 0; v < 16 && ok; ++v)
            for (int l = 0; l < 7 && ok; ++l) {
                se.laneSignV_[v][l] = 0;
                if (se.polyLaneSign(v, l) != +1) { std::snprintf(buf, sizeof buf, "sign 0 → +1 failed at (%d,%d)", v, l); ok = false; }
                se.laneSignV_[v][l] = -1;
                if (ok && se.polyLaneSign(v, l) != -1) { std::snprintf(buf, sizeof buf, "sign -1 → -1 failed at (%d,%d)", v, l); ok = false; }
                se.laneSignV_[v][l] = 0;
            }
        expectCell(ok, F, -1, -1, buf);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// U8 — TOPOLOGY ownership matrix (SandsTopology::owner — the single source the
// manager layer mirrors): flipping ONE cell's owner flag moves EXACTLY that
// cell to MACRO, everywhere else stays EAST; V1 row per-lane; MACRO_SOLE owns
// the full 16×7 uniformly.
// ══════════════════════════════════════════════════════════════════════════════
static void U8_topology_owner_matrix() {
    const std::string F = "U8-topology-owner";
    using ST = dotModular::SandsTopology;
    for (int v0 = 0; v0 < 16; ++v0)
        for (int l0 = 0; l0 < 7; ++l0) {
            ST::Inputs in;
            in.eastPresent = true;
            in.macroPresent = true;
            in.polyBaseActive = true;
            in.polyVoiceCount = 15;
            for (int l = 0; l < 7; ++l) in.eastV1Owner[l] = true;
            for (int v = 0; v < 16; ++v)
                for (int l = 0; l < 7; ++l) in.eastPolyOwner[v][l] = true;
            if (v0 == 0) in.eastV1Owner[l0] = false;
            else         in.eastPolyOwner[v0][l0] = false;
            const ST topo = ST::build(in);

            bool ok = true; char buf[128]; buf[0] = 0;
            if (topo.owner(v0, l0) != ST::Role::MACRO) {
                std::snprintf(buf, sizeof buf, "ceded cell (%d,%s) is not MACRO", v0, LANE_NAME[l0]);
                ok = false;
            }
            for (int v = 0; v < 16 && ok; ++v)
                for (int l = 0; l < 7 && ok; ++l) {
                    if (v == v0 && l == l0) continue;
                    if (topo.owner(v, l) != ST::Role::EAST) {
                        std::snprintf(buf, sizeof buf, "owner flip (%d,%s) moved (%d,%s) off EAST",
                                      v0, LANE_NAME[l0], v, LANE_NAME[l]);
                        ok = false;
                    }
                }
            expectCell(ok, F, v0, l0, buf);
        }

    // MACRO_SOLE: Macro owns every cell — uniform, no lane/voice special.
    {
        ST::Inputs in;
        in.eastPresent = false;
        in.macroPresent = true;
        in.polyBaseActive = true;
        in.polyVoiceCount = 15;
        const ST topo = ST::build(in);
        bool ok = true; int bv = -1, bl = -1;
        for (int v = 0; v < 16 && ok; ++v)
            for (int l = 0; l < 7 && ok; ++l)
                if (topo.owner(v, l) != ST::Role::MACRO) { ok = false; bv = v; bl = l; }
        char buf[96];
        std::snprintf(buf, sizeof buf, "MACRO_SOLE does not own (%d,%s)", bv, (bl >= 0 ? LANE_NAME[bl] : "?"));
        expectCell(ok, F, -1, -1, buf);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// U9 — LOR store round-trip: polyLORRef/polyLOR cover ALL 16 slots × 7 lanes ×
// 3 items with the voiceSlot base (the varlegStoreBank identity included).
// ══════════════════════════════════════════════════════════════════════════════
static void U9_lor_store_roundtrip() {
    const std::string F = "U9-lor-store";
    SequencerEngine se;
    for (int v = 0; v < 16; ++v)
        for (int l = 0; l < 7; ++l)
            for (int item = 0; item < SequencerEngine::LOR_ITEMS; ++item) {
                const int tag = (v * 7 + l) * 3 + item + 1;
                se.polyLORRef(v, l, item) = tag;
                char buf[96];
                std::snprintf(buf, sizeof buf, "polyLOR(%d,%s,item %d)=%d != wrote %d",
                              v, LANE_NAME[l], item, se.polyLOR(v, l, item), tag);
                expectCell(se.polyLOR(v, l, item) == tag, F, v, l, buf);
            }
}

// ── driver ─────────────────────────────────────────────────────────────────────

int main() {
    const bool strict = std::getenv("UNIFORMITY_STRICT") != nullptr;

    std::printf("lane/voice UNIFORMITY HARNESS — SEAM_ERADICATION_PLAN Stage 0 "
                "(16 voiceSlots x 7 lanes, V1 + VAR/LEG explicit)\n");

    U1_warp_own_slot();
    U2_warp_delegation();
    U3_warp_consumer_gate();
    U4_warp_dirty_scopes();
    U5_spread_isolation();
    U6_spread_resolver_lane_free();
    U7_direction_promote();
    U8_topology_owner_matrix();
    U9_lor_store_roundtrip();

    // Partition failures against the manifest.
    std::vector<Fail> known, unexpected;
    for (const auto& f : g_fails) {
        bool matched = false;
        for (const auto& k : knownSeams())
            if (k.match(f)) { matched = true; break; }
        (matched ? known : unexpected).push_back(f);
    }

    std::printf("\n  checks: %d   clean: %d   KNOWN-SEAM (expected red): %zu   UNEXPECTED: %zu\n",
                g_checks, g_checks - (int)g_fails.size(), known.size(), unexpected.size());

    if (!known.empty()) {
        std::printf("\n  KNOWN SEAMS (the map — each stage DELETES its lines; never add):\n");
        for (const auto& k : knownSeams()) {
            std::vector<Fail> cells;
            for (const auto& f : known)
                if (k.match(f)) cells.push_back(f);
            if (cells.empty()) continue;
            std::printf("  - [%s]  %zu failing checks\n      why: %s\n      closed by: %s\n",
                        k.id, cells.size(), k.why, k.closedBy);
            // Compact map: if the seam is lane-uniform across ALL 16 voices, print the
            // lane set once; otherwise list the distinct (v,l) cells.
            bool allVoicesEveryLane = true;
            std::vector<int> lanes;
            for (const auto& f : cells) {
                bool seen = false;
                for (int L : lanes) if (L == f.l) seen = true;
                if (!seen && f.l >= 0) lanes.push_back(f.l);
                if (f.v < 0) allVoicesEveryLane = false;
            }
            if (allVoicesEveryLane && (int)cells.size() == 16 * (int)lanes.size()) {
                std::printf("      cells: ALL 16 voiceSlots x lanes {");
                for (size_t i = 0; i < lanes.size(); ++i)
                    std::printf("%s%s", (i ? "," : ""), LANE_NAME[lanes[i]]);
                std::printf("}\n");
            } else {
                std::printf("      cells: ");
                for (size_t i = 0; i < cells.size(); ++i)
                    std::printf("(%d,%s)%s", cells[i].v,
                                cells[i].l >= 0 ? LANE_NAME[cells[i].l] : "-",
                                (i + 1 < cells.size()) ? " " : "");
                std::printf("\n");
            }
        }
    }
    if (!unexpected.empty()) {
        std::printf("\n  UNEXPECTED FAILURES — a seam NOT in the manifest (the harness's job):\n");
        for (const auto& f : unexpected)
            std::printf("  x [%s] (v=%d l=%s): %s\n", f.feature.c_str(), f.v,
                        f.l >= 0 ? LANE_NAME[f.l] : "-", f.what.c_str());
    }

    std::printf("\n  RESIDUALS this harness cannot reach headless (Stage-1 audit + Rack feel-test):\n"
                "   - MonsoonSandsManager.cpp:144-155 — warpDelegated_ mirror from topo.owner\n"
                "   - MonsoonExpanderManager.cpp:378-400 — per-voice DIRECTION push (VAR/LEG follow,\n"
                "     CC_PROMPT_08) + combineLOR (the LOR duplicate algebra)\n"
                "   - Monsoon editor store accessors — editor.laneDir[96] poly-bank v*6+lane indexing\n"
                "     (flagged debt, Monsoon.hpp:790)\n"
                "   - executeStep (V1) vs executePolyVoice (V2+) consumers — the Seam-1 split itself\n");

    if (!unexpected.empty()) { std::printf("\nUNIFORMITY: %zu UNEXPECTED failure(s)\n", unexpected.size()); return 1; }
    if (strict && !known.empty()) {
        std::printf("\nUNIFORMITY (strict): %zu known-seam failure(s) — stage gate NOT met\n", known.size());
        return 1;
    }
    std::printf("\nUNIFORMITY: no unexpected failures%s\n",
                known.empty() ? " — manifest EMPTY, all cells uniform" : " (known seams mapped above)");
    return 0;
}
