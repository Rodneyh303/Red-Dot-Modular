// test_ui_to_resolver.cpp — CC_PROMPT_UI_TO_RESOLVER_SPEC: the ONE-PRODUCER contract
// suite. The spec's five contract invariants, at the layer the unit rig can see (the
// engine + the resolver); the rig-invisible halves (the module's step cache, the
// widget routing) are pinned STRUCTURALLY here — the B3 "grep in test" precedent from
// test_warp_voice_isolation — because no Rack host exists in this rig.
//
// THE SPEC (docs/design/CC_PROMPT_UI_TO_RESOLVER_SPEC.md): SequencerEngine::
// effectiveProbability is the SINGLE producer of each per-(voice,lane) effective
// probability — the engine draw, the bars, and the prob-outs ALL read it; no second
// computation anywhere. This file pins:
//
//   T1 ONE-PRODUCER:
//     (a) the accessor's Owner lens IS the engine draw's value — bit-exact against
//         warpRow after the same spread stores the audio path consumes are applied
//         (the display twin reads the SAME resolver-resolved spread + the SAME warp
//         composition the engine consumed; only the published-snapshot basis differs,
//         and settled pubs are bit-equal copies). Engine draw == bars == out, one
//         source, modulo the latch.
//     (b) the bars' source == the out's source: VoiceResolver::laneProbabilityAtStep
//         (the prob-out seam) IS the accessor (MacroOwn lens) — bit-exact, every
//         voice × step. One function, not two resolutions.
//     (c) STRUCTURAL: no independent bar-composition path may exist in the visual
//         files (no SpreadInterp/warpDisplayMix hand-rolls in the bar fills).
//   T2 OWNERSHIP (one-way): East-delegated lane → an East spread edit (the Owner
//     stores combineSpread writes) moves the Owner lens (East's bars/out) and NOT the
//     MacroOwn lens (Macro's bars/out). Macro-delegated → macroOwnSpread drives the
//     MacroOwn lens (Macro drives); a raw East own-slot write to the Owner store is
//     inert on the MacroOwn lens. Kills the East↔Macro leak by construction.
//   T3 VOICE: the accessor takes an EXPLICIT voiceSlot — distinct per-voice rows come
//     back distinct, and the prob-out's channel mapping (ch v → voiceSlot v) lands on
//     the SAME row the bars would draw for that voice. No hardcoded 0 anywhere.
//   T4 DIRECTION (lands with the monoDirAuthority fix): all 7 lanes resolve through
//     resolveDirValue; VAR/LEG follow Macro like 0-4. STRUCTURAL.
//   T5 RATE (lands with the step cache): the heavy resolve runs at STEP rate inside
//     the module's cache — the /32 throttle and the SweepSig fingerprint gate are
//     GONE. STRUCTURAL + the purity/stability behavioural half.
//
// SUBSUMPTION (the C3(d) regression guard, carried from test_macro_own_output): the
// MacroOwn lens reproduces the OLD macroOwnLaneProbability formula bit-exactly — the
// bars' values do not change; only the producer is unified.
//
// Companion sources: PatternEngine.cpp + SequencerEngine.cpp + GateState.cpp
// (the test_warp_voice_isolation pattern).
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include "SequencerEngine.hpp"
#include "SpreadInterp.hpp"
#include "VoiceResolver.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static int s_pass = 0, s_fail = 0;
#define TEST(desc, ...) do { \
    bool _ok = true; std::string _msg; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok = false; _msg = _e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); \
           if (!_msg.empty()) std::cout << " — " << _msg; std::cout << "\n"; } \
} while (0)
#define EXPECT(e) do { if (!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while (0)
#define EXPECTF(e, ...) do { if (!(e)) { char _b[256]; std::snprintf(_b, sizeof _b, __VA_ARGS__); \
    throw std::runtime_error(std::string("EXPECT(" #e ") failed: ") + _b); } } while (0)

// Deterministic, well-spread [0,1) values (golden-ratio scramble — the same basis as
// the warp suites), applied to the PRE-spread slewed buffers and published. pubSlewed*
// is the accessor's base; random_ starts as the spread-0 shared final (== the base).
// Distinct per (voice,lane,step) so any spread/cross-voice mixture is maximally
// distinguishable. (The test_macro_own_output basis, reused verbatim.)
static void fillSlewedGolden(PatternEngine& pe) {
    for (int v = 0; v < 16; ++v)
        for (int l = 0; l < 7; ++l)
            for (int s = 0; s < 16; ++s) {
                const long idx = (long)v * 7 * 16 + l * 16 + s;
                const float u = (float)std::fmod(0.125 + idx * 0.6180339887498949, 1.0);
                switch (l) {
                    case PatternEngine::PL_MELODY:    pe.slewedMelody[v][s] = u; break;
                    case PatternEngine::PL_OCTAVE:    pe.slewedOctave[v][s] = u; break;
                    case PatternEngine::PL_QMIX:      pe.slewedQmix[v][s] = u; break;
                    case PatternEngine::PL_REST:      pe.slewedRhythm[v][s] = u; break;
                    case PatternEngine::PL_ACCENT:    pe.slewedAccent[v][s] = u; break;
                    case PatternEngine::PL_VARIATION: pe.slewedVariation[v][s] = u; break;
                    default:                          pe.slewedLegato[v][s] = u; break;
                }
                pe.random_[v][l][s] = u;   // spread-0 shared final == the base
            }
    pe.publishSlewedRhythm();   // rhythm + accent + variation + legato
    pe.publishSlewedMelody();   // melody + octave
    pe.publishSlewedQmix();
}

// ── Structural-pin helpers (the B3 precedent) ─────────────────────────────────
static std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss; ss << f.rdbuf();
    return ss.str();
}
static bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
static std::string repoRoot() {
    static const char* candidates[] = { ".", "..", "../.." };
    for (const char* m : candidates) {
        std::ifstream f(std::string(m) + "/src/dsp/engines/SequencerEngine.cpp", std::ios::binary);
        if (f) return m;
    }
    throw std::runtime_error("repo root not found — run via test/run_all.sh from the repo");
}

int main() {
    std::cout << "\033[1mUI-to-resolver (CC_PROMPT_UI_TO_RESOLVER_SPEC) Contract Tests\033[0m\n"
              << std::string(70, '=') << "\n";
    const int MEL = PatternEngine::PL_MELODY, OCT = PatternEngine::PL_OCTAVE,
              VAR = PatternEngine::PL_VARIATION;
    using Lens = SequencerEngine::ProbLens;

    // ── T1(a): ONE-PRODUCER — the accessor's Owner lens IS the engine draw ──────
    TEST("T1a ONE-PRODUCER: engine draw (warpRow) == effectiveProbability(Owner) bit-exact — same resolver-resolved spread stores, same warp composition (all voices, all steps)", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        se.pe.cachedSpreadInitialized = true;    // refreshPolyRandomCell's gate
        // The OWNER stores — exactly what the manager's resolver calls publish
        // (sprForLane → spreadERef(0,·) for V1; combineSpread → cachedPolySpread for V2+):
        se.spreadERef(0, MEL) = -0.45f;
        for (int v = 1; v < 16; ++v)
            se.pe.cachedPolySpread[v][MEL] = (v % 3) ? 0.35f : -0.7f;
        se.pe.cachedPolySpread[0][MEL] = se.spreadE(0, MEL);   // the V1 refresh reads the same amount
        // Simulate the engine's own refresh (the manager's V1 applyMono pass + the
        // step-rate refreshPolyRandomCell — which also rebuilds the warp mirror cells):
        for (int i = 0; i < 16; ++i)
            se.pe.polyRandom(0, MEL)[i] =
                redDot::SpreadInterp::applyMono(se.pe, MEL, i, se.spreadE(0, MEL));
        se.pe.rebuildWarpMirrorVoiceLane(0, MEL);
        for (int v = 1; v < 16; ++v)
            for (int i = 0; i < 16; ++i) se.refreshPolyRandomCell(v, MEL, i);
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 16; ++s)
                EXPECTF(se.pe.warpRow(v, MEL)[s] == se.effectiveProbability(v, MEL, s, Lens::Owner),
                        "engine draw != accessor (v=%d step=%d: %.9g vs %.9g) — two producers",
                        v, s, (double)se.pe.warpRow(v, MEL)[s],
                        (double)se.effectiveProbability(v, MEL, s, Lens::Owner));
        // ...and again on a rhythm-family lane beyond the spread core (VAR rides the
        // rhythm axis; the golden fill gives it distinct rows too):
        se.spreadERef(0, VAR) = 0.6f;
        for (int v = 1; v < 16; ++v) se.pe.cachedPolySpread[v][VAR] = -0.2f;
        se.pe.cachedPolySpread[0][VAR] = se.spreadE(0, VAR);
        for (int i = 0; i < 16; ++i)
            se.pe.polyRandom(0, VAR)[i] =
                redDot::SpreadInterp::applyMono(se.pe, VAR, i, se.spreadE(0, VAR));
        se.pe.rebuildWarpMirrorVoiceLane(0, VAR);
        for (int v = 1; v < 16; ++v)
            for (int i = 0; i < 16; ++i) se.refreshPolyRandomCell(v, VAR, i);
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 16; ++s)
                EXPECTF(se.pe.warpRow(v, VAR)[s] == se.effectiveProbability(v, VAR, s, Lens::Owner),
                        "engine draw != accessor on VAR (v=%d step=%d: %.9g vs %.9g)",
                        v, s, (double)se.pe.warpRow(v, VAR)[s],
                        (double)se.effectiveProbability(v, VAR, s, Lens::Owner));
    });

    // ── T1(b): the bars' source == the out's source — ONE function ──────────────
    TEST("T1b ONE-PRODUCER: VoiceResolver::laneProbabilityAtStep (the prob-out seam) IS effectiveProbability(MacroOwn) — bit-exact, every voice × step", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        se.macroOwnSpread[MEL] = 0.55f;   // non-zero so the lens actually composes
        se.macroOwnSpread[OCT] = -0.3f;
        dotModular::VoiceResolver resolver(se);
        for (int voice = 1; voice <= 16; ++voice)
            for (int s = 0; s < 16; ++s) {
                const int slot = voice - 1;   // voiceSlot: V1→0 ... V16→15
                const float want = se.effectiveProbability(slot, MEL, s, Lens::MacroOwn);
                EXPECTF(resolver.laneProbabilityAtStep(voice, MEL, s) == want,
                        "out != accessor (voice=%d step=%d: %.9g vs %.9g) — two sources",
                        voice, s, (double)resolver.laneProbabilityAtStep(voice, MEL, s),
                        (double)want);
                const float wantO = se.effectiveProbability(slot, OCT, s, Lens::MacroOwn);
                EXPECTF(resolver.laneProbabilityAtStep(voice, OCT, s) == wantO,
                        "out != accessor, OCT (voice=%d step=%d)", voice, s);
            }
    });

    // ── T2: OWNERSHIP one-way ────────────────────────────────────────────────────
    TEST("T2a OWNERSHIP (East-delegated): an East spread edit moves the Owner lens (East's bars/out) and NOT the MacroOwn lens (Macro's bars/out)", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        // East owns the lane: the Owner stores carry EAST's resolved amount
        // (what combineSpread publishes when ownerEast); Macro's own store carries
        // Macro's (what publishGlobal publishes).
        se.spreadERef(0, MEL) = -0.8f;
        for (int v = 1; v < 16; ++v) se.pe.cachedPolySpread[v][MEL] = -0.8f;
        se.macroOwnSpread[MEL] = 0.5f;
        const float ownerBefore = se.effectiveProbability(3, MEL, 7, Lens::Owner);
        const float macroBefore = se.effectiveProbability(3, MEL, 7, Lens::MacroOwn);
        EXPECTF(ownerBefore != macroBefore,
                "vacuous: the lenses agree at East=%.2f Macro=%.2f — pick a step where they differ",
                -0.8, 0.5);
        // East's edit (the Owner stores move — the manager republishes East's amount):
        se.spreadERef(0, MEL) = -0.95f;
        for (int v = 1; v < 16; ++v) se.pe.cachedPolySpread[v][MEL] = -0.95f;
        EXPECTF(se.effectiveProbability(3, MEL, 7, Lens::Owner) != ownerBefore,
                "East's edit did not move the Owner lens — East's bars would be dead");
        // THE LEAK KILL: Macro's bars/out are UNCHANGED under East's edit.
        EXPECTF(se.effectiveProbability(3, MEL, 7, Lens::MacroOwn) == macroBefore,
                "LEAK: East's spread edit moved Macro's value (MacroOwn lens) — "
                "the one-way borrowing rule is broken");
    });

    TEST("T2b OWNERSHIP (Macro-delegated): macroOwnSpread drives the MacroOwn lens (Macro drives); a raw East own-slot write to the Owner store is inert on Macro's value", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        // Macro owns the lane: the Owner store carries MACRO's resolved amount
        // (combineSpread's delegated branch); macroOwnSpread carries Macro's own.
        se.macroOwnSpread[OCT] = 0.4f;
        for (int v = 1; v < 16; ++v) se.pe.cachedPolySpread[v][OCT] = 0.4f;
        const float macroBefore = se.effectiveProbability(5, OCT, 3, Lens::MacroOwn);
        // Macro's edit moves its own lens (Macro drives):
        se.macroOwnSpread[OCT] = -0.65f;
        EXPECTF(se.effectiveProbability(5, OCT, 3, Lens::MacroOwn) != macroBefore,
                "Macro's own spread edit did not move the MacroOwn lens — Macro's bars dead");
        // East's own-slot edit (the raw Owner-store write East's pass WOULD land —
        // the manager must not apply it on a Macro-owned lane, and even if it did,
        // Macro's value never reads that store): inert on the MacroOwn lens.
        const float withEdit = se.effectiveProbability(5, OCT, 3, Lens::MacroOwn);
        for (int v = 1; v < 16; ++v) se.pe.cachedPolySpread[v][OCT] = 0.9f;   // rogue East value
        EXPECTF(se.effectiveProbability(5, OCT, 3, Lens::MacroOwn) == withEdit,
                "LEAK: an East own-slot write moved Macro's value — Macro never borrows East");
        // V1 anchor: the MacroOwn lens at voiceSlot 0 is spread-invariant at POSITIVE
        // spread (the anchor self-target no-op — matches the pinned C3(b) contract).
        se.macroOwnSpread[OCT] = 0.75f;
        const float v1Pos = se.effectiveProbability(0, OCT, 3, Lens::MacroOwn);
        se.macroOwnSpread[OCT] = 0.25f;
        EXPECTF(se.effectiveProbability(0, OCT, 3, Lens::MacroOwn) == v1Pos,
                "V1 (anchor) moved under positive Macro spread — the self-target no-op broke");
    });

    // ── T3: VOICE — explicit, no hardcoded 0 ────────────────────────────────────
    TEST("T3 VOICE: the accessor returns the EXPLICIT voice's row (distinct per voiceSlot), and the prob-out channel mapping (ch v → voiceSlot v) reads the SAME row the bars draw", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        // Distinct sentinel rows per voiceSlot on the published base:
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 16; ++s)
                se.pe.pubSlewedPolyMelody[v][s] = 0.05f + 0.06f * v + 0.003f * s;
        // Both lenses, per voice: the value must depend on the VOICE argument (the
        // displayed voice — not a hardcoded row 0):
        for (int v = 0; v < 16; ++v) {
            const float own = se.effectiveProbability(v, MEL, 4, Lens::Owner);
            const float mac = se.effectiveProbability(v, MEL, 4, Lens::MacroOwn);
            const float rowV = se.pe.pubSlewedPolyMelody[v][4];   // spread 0 → base bit-exact
            EXPECTF(own == rowV, "Owner lens (v=%d): %.9g != its own row %.9g — wrong voice",
                    v, (double)own, (double)rowV);
            EXPECTF(mac == rowV, "MacroOwn lens (v=%d): %.9g != its own row %.9g — wrong voice",
                    v, (double)mac, (double)rowV);
            if (v > 0)
                EXPECTF(own != se.effectiveProbability(v - 1, MEL, 4, Lens::Owner) || rowV == se.pe.pubSlewedPolyMelody[v - 1][4],
                        "voices %d and %d returned the same value from distinct rows — a hardcoded voice",
                        v - 1, v);
        }
        // The out's channel mapping: ch ch→voice ch+1 → voiceSlot ch — the SAME
        // accessor call the bars make for that voice (T1b's mapping, pinned directly
        // against the sentinels):
        dotModular::VoiceResolver resolver(se);
        for (int ch = 0; ch < 16; ++ch)
            EXPECTF(resolver.laneProbabilityAtStep(ch + 1, MEL, 4) == se.pe.pubSlewedPolyMelody[ch][4],
                    "channel %d (voice %d) did not read voiceSlot %d's row — voice mismatch",
                    ch, ch + 1, ch);
    });

    // ── SUBSUMPTION: the MacroOwn lens reproduces the OLD formula bit-exactly ───
    TEST("SUBSUMPTION: effectiveProbability(MacroOwn) == the OLD macroOwnLaneProbability formula (pubSlewed base + macroOwnSpread via SpreadInterp + warpDisplayMix) — bit-exact; the bars' values do not change", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        se.macroOwnSpread[MEL] = 0.62f;
        se.macroOwnSpread[VAR] = -0.48f;
        const int lanes[2] = { MEL, VAR };
        for (int li = 0; li < 2; ++li) {
            const int LANE = lanes[li];
            for (int s = 0; s < 16; ++s)
                for (int v = 0; v < 16; ++v) {
                    const float sp = se.macroOwnSpread[LANE];
                    const float base = redDot::SpreadInterp::polySlewedPub(se.pe, LANE, v, s);
                    const float disp = (v == 0)
                        ? redDot::SpreadInterp::interpolate(base, base, sp)
                        : redDot::SpreadInterp::applyAnchorV1Only(se.pe, LANE, s, base, sp);
                    const float want = se.pe.warpDisplayMix(v, LANE, (double)disp);
                    EXPECTF(se.effectiveProbability(v, LANE, s, Lens::MacroOwn) == want,
                            "old-formula mismatch (lane=%d v=%d step=%d: %.9g vs %.9g)",
                            LANE, v, s, (double)se.effectiveProbability(v, LANE, s, Lens::MacroOwn),
                            (double)want);
                }
        }
    });

    // ── T5 (behavioural half — lands now; the structural half lands with the step
    //    cache): PURITY + STABILITY — the property that makes STEP-caching correct.
    TEST("T5 RATE (purity half): the accessor is PURE (same inputs → same value, no engine mutation) and STABLE across reads — the property that makes the module's step-rate cache correct", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        se.spreadERef(0, MEL) = 0.33f;
        se.macroOwnSpread[MEL] = -0.5f;
        for (int v = 1; v < 16; ++v) se.pe.cachedPolySpread[v][MEL] = 0.21f;
        float snap[16][16][2];
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 16; ++s) {
                snap[v][s][0] = se.effectiveProbability(v, MEL, s, Lens::Owner);
                snap[v][s][1] = se.effectiveProbability(v, MEL, s, Lens::MacroOwn);
            }
        // Re-read after a burst of intervening calls on OTHER cells (a "sample-rate
        // stretch" with unchanged inputs): every value must be bit-identical.
        for (int i = 0; i < 1000; ++i) (void)se.effectiveProbability(i & 15, MEL, (i * 7) & 15, Lens::Owner);
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 16; ++s) {
                EXPECTF(se.effectiveProbability(v, MEL, s, Lens::Owner) == snap[v][s][0],
                        "Owner lens not stable (v=%d s=%d) — step-caching would show stale/drift",
                        v, s);
                EXPECTF(se.effectiveProbability(v, MEL, s, Lens::MacroOwn) == snap[v][s][1],
                        "MacroOwn lens not stable (v=%d s=%d)", v, s);
            }
    });

    // ── T1(c) + T5 STRUCTURAL: the ONE producer is the only bar path; the deleted
    //    machinery stays deleted (the B3 "grep in test" precedent — rig-invisible
    //    files, pinned by source). ─────────────────────────────────────────────────
    TEST("T1c ONE-PRODUCER (structural): the Macro bars route through the ONE accessor; no independent bar-composition path (SpreadInterp/warpDisplayMix hand-rolls) in the visual bar fills", {
        const std::string macro = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.cpp");
        EXPECTF(contains(macro, "effectiveProbability("),
                "StraitsSandsMacroVisual.cpp: the bars do not call the ONE accessor");
        EXPECTF(!contains(macro, "macroOwnLaneProbability("),
                "StraitsSandsMacroVisual.cpp: still routes through the deleted "
                "macroOwnLaneProbability — a second producer");
        EXPECTF(!contains(macro, "SpreadInterp::") && !contains(macro, "warpDisplayMix("),
                "StraitsSandsMacroVisual.cpp: a SpreadInterp/warpDisplayMix hand-rolled bar "
                "composition survives — the exact third-computation this spec deletes");
        const std::string east = slurp(repoRoot() + "/src/StraitsEastSandsVisual.cpp");
        EXPECTF(contains(east, "effectiveProbability("),
                "StraitsEastSandsVisual.cpp: the East bars do not call the ONE accessor");
        EXPECTF(!contains(east, "SpreadInterp::") && !contains(east, "warpDisplayMix("),
                "StraitsEastSandsVisual.cpp: a SpreadInterp/warpDisplayMix hand-rolled bar "
                "composition survives — the exact third-computation this spec deletes");
        EXPECTF(!contains(east, "finalRandomByStrand("),
                "StraitsEastSandsVisual.cpp: a bar fill still reads finalRandomByStrand "
                "(the mono-row engine funnel) instead of the voice-explicit accessor");
        // The accessor stays the engine-side authority (the signature + lens anchors the
        // consumers' routing pins hang from):
        const std::string eng = slurp(repoRoot() + "/src/dsp/engines/SequencerEngine.hpp");
        EXPECTF(contains(eng, "float effectiveProbability(int voice, int lane, int step, ProbLens lens) const"),
                "SequencerEngine.hpp: the ONE accessor's signature changed — the consumers' "
                "routing pins must follow it");
        EXPECTF(contains(eng, "enum class ProbLens { Owner, MacroOwn }"),
                "SequencerEngine.hpp: the lens enum is gone — the one-way borrowing "
                "contract has no structural anchor");
    });

    // ── T4 DIRECTION (structural — the manager is rig-invisible): all 7 lanes
    //    resolve through the ONE selection (resolveDirValue/resolveDirItem); the
    //    mono/V1 authority has no lane-range carve-out — VAR/LEG follow Macro
    //    like 0-4. ──────────────────────────────────────────────────────────────
    TEST("T4 DIRECTION: both direction pushes ride resolveDirItem; monoDirAuthority has no lane carve-out — VAR/LEG follow Macro like 0-4", {
        const std::string mgr = slurp(repoRoot() + "/src/dsp/managers/MonsoonExpanderManager.cpp");
        EXPECTF(contains(mgr, "resolveDirItem("),
                "MonsoonExpanderManager.cpp: a direction push left the ONE resolver "
                "selection (DirPolicy) — a second direction path");
        EXPECTF(!contains(mgr, "const bool varleg"),
                "MonsoonExpanderManager.cpp: the stale pre-QMIX `lane >= 4` carve-out is "
                "back — VAR/LEG (and ACC) must follow Macro like 0-4 through the ONE "
                "authority");
        EXPECTF(contains(mgr, "r.macroGlobal = true"),
                "MonsoonExpanderManager.cpp: monoDirAuthority lost its Macro arm");
        // The behavioural twin lives in test_mono_dir_authority (the replica of the
        // authority's precedence, restructured to the same contract).
    });

    std::cout << std::string(70, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
