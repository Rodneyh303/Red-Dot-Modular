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
//   T6 THE OUT ENTRY (CC_PROMPT_UI_TO_RESOLVER_FINISH_START — the CC_PROMPT_05 lesson,
//     made structural): a green suite at the ACCESSOR is worthless if the OUT still
//     reads the old path — this suite was green while masterLaneProbability (the
//     master prob-out entry: VoiceResolver::laneProbability's V1 branch, East's ch0
//     out channel) still called finalRandomByStrand → warpRow(0), the hardcoded
//     voice-0 residue. T6 drives the REAL entry: the one-producer identity (T6a:
//     masterLaneProbability IS effectiveProbability(kMonoSlot, ·, MacroOwn) at the
//     master's own step), the one-way no-leak rule (T6b: the East spread write never
//     moves it), the explicit-voice rule (T6c: it does NOT follow warpRow(0)) — all
//     THROUGH the entry, never the accessor in isolation. RED on the pre-FINISH tree;
//     the structural half pins the wiring in source.
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
#include "WarpF.hpp"   // T7: author an East own-slot curve + Macro's global cell (bank-wired warp)
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

// Simulate the manager's East-spread pass on `lane` (the test_macro_own_output basis,
// reused verbatim — the manager is rig-invisible; these are the exact writes it
// performs, at the engine's own seams): the control-rate mono write (polyRandom(0,·) =
// applyMono with the OWNER's spread) + the step-rate poly writes (cachedPolySpread +
// refreshPolyRandomCell, which also refreshes the warp mirror cells). NEGATIVE spread
// so the mono row actually moves (positive self-target spread is the anchor no-op).
// These writes touch random_/warpRow ONLY — never the pubSlewed* published base nor
// macroOwnSpread — the exact seam the master out entry must not read.
static void eastSpreadWrite(SequencerEngine& se, int lane, float eastSpread) {
    PatternEngine& pe = se.pe;
    pe.cachedSpreadInitialized = true;   // refreshPolyRandomCell's gate (set in sync())
    se.spreadERef(0, lane) = eastSpread; // the store holds the OWNER's spread on this lane
    for (int i = 0; i < 16; ++i)
        pe.polyRandom(0, lane)[i] = redDot::SpreadInterp::applyMono(pe, lane, i, eastSpread);
    for (int v = 1; v < 16; ++v) {
        pe.cachedPolySpread[v][lane] = eastSpread;
        for (int i = 0; i < 16; ++i) se.refreshPolyRandomCell(v, lane, i);
    }
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
// T9: the FINISH2 literal-acceptance counts — occurrences of a substring, and LINES
// containing any of a set of substrings (the acceptance greps are raw line counts:
// comment tombstones count exactly like live refs).
static int countOccurrences(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + needle.size()))
        ++n;
    return n;
}
static int countLinesWith(const std::string& hay, const char* const subs[], int nSubs) {
    int lines = 0;
    size_t pos = 0;
    while (pos != std::string::npos) {
        const size_t eol = hay.find('\n', pos);
        const std::string line = hay.substr(pos, (eol == std::string::npos ? hay.size() : eol) - pos);
        for (int i = 0; i < nSubs; ++i)
            if (line.find(subs[i]) != std::string::npos) { ++lines; break; }
        pos = (eol == std::string::npos) ? std::string::npos : eol + 1;
    }
    return lines;
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

    // ── T6: THE OUT ENTRY (CC_PROMPT_UI_TO_RESOLVER_FINISH_START) ────────────────
    // The CC_PROMPT_05 lesson, made structural: this suite was GREEN while the real
    // output wiring (masterLaneProbability → finalRandomByStrand → warpRow(0)) never
    // called the accessor — a green test at the accessor ≠ the output using it. Every
    // T6 assertion goes THROUGH masterLaneProbability (the actual prob-out entry —
    // VoiceResolver::laneProbability's V1 branch, East's ch0 out channel), never the
    // accessor in isolation.
    TEST("T6a THE OUT ENTRY: masterLaneProbability IS effectiveProbability(V1=kMonoSlot, MacroOwn) at the master's own step — the one-producer identity at the OUTPUT entry, all 7 lanes, driven playhead steps", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        // Macro's OWN spread, NEGATIVE per lane: the V1 anchor self-target no-op holds
        // only at spread >= 0 (negative inverts toward the complement), so the lens
        // actually composes and the accessor's value is maximally distinguishable from
        // the OLD path's raw-row read (the RED condition).
        for (int l = 0; l < 7; ++l) se.macroOwnSpread[l] = -0.15f - 0.07f * l;
        // Identity LOR per strand so masterLaneStep follows the driven tick (lorStore_
        // zero-inits: len 0 would pin every step to 0 — set len 16, off/rot 0):
        for (int s = 0; s < 7; ++s) {
            se.strandLenRef(s) = 16; se.strandOffRef(s) = 0; se.strandRotRef(s) = 0;
        }
        dotModular::VoiceResolver resolver(se);
        const int ticks[4] = { 0, 3, 7, 15 };
        int seen[4] = { -1, -1, -1, -1 };
        for (int l = 0; l < 7; ++l) {
            const int strand = SequencerEngine::polyLaneStrand(l);
            for (int ti = 0; ti < 4; ++ti) {
                se.laneTick_[strand] = ticks[ti];          // drive the master playhead
                const int step = se.masterLaneStep(l);
                if (l == 0) seen[ti] = step;
                const float want = se.effectiveProbability(0, l, step, Lens::MacroOwn);
                EXPECTF(se.masterLaneProbability(l) == want,
                        "master out != the ONE accessor (lane=%d step=%d: %.9g vs %.9g) — "
                        "the out entry still reads its own path",
                        l, step, (double)se.masterLaneProbability(l), (double)want);
                // ONE V1 out value: the resolver's at-step entry (the Macro-out V1
                // channel) and the master entry are the SAME source at the same step.
                EXPECTF(resolver.laneProbabilityAtStep(1, l, step) == want,
                        "the master entry diverged from the resolver's V1 at-step entry "
                        "(lane=%d step=%d) — two V1 out values",
                        l, step);
                EXPECTF(resolver.laneProbability(1, l) == want,
                        "VoiceResolver::laneProbability(1,·) diverged (lane=%d step=%d) — "
                        "the V1 branch left the master entry",
                        l, step);
            }
        }
        for (int ti = 0; ti < 4; ++ti)
            for (int tj = ti + 1; tj < 4; ++tj)
                EXPECTF(seen[ti] != seen[tj],
                        "vacuous: the driven ticks did not produce distinct steps "
                        "(%d,%d,%d,%d) — the identity ran at one step only",
                        seen[0], seen[1], seen[2], seen[3]);
    });

    TEST("T6b NO-LEAK THROUGH THE OUT ENTRY: the East spread write (the raw write East's pass would land — owner either way, the C2 strong form) does NOT move masterLaneProbability on ANY lane", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        float before[7], frBefore[7];
        for (int l = 0; l < 7; ++l) {
            before[l] = se.masterLaneProbability(l);
            frBefore[l] = se.pe.finalRandomByStrand(SequencerEngine::polyLaneStrand(l),
                                                    se.masterLaneStep(l));
        }
        for (int l = 0; l < 7; ++l) {
            eastSpreadWrite(se, l, -0.8f);
            // Vacuous guard: the shared voice-0 row (what the OLD out read) really
            // moved on this lane — the write took.
            EXPECTF(se.pe.finalRandomByStrand(SequencerEngine::polyLaneStrand(l),
                                              se.masterLaneStep(l)) != frBefore[l],
                    "vacuous: the East write did not move the shared row (lane %d)", l);
            // THE ONE-WAY RULE at the master entry: East's spread never reaches it.
            EXPECTF(se.masterLaneProbability(l) == before[l],
                    "LEAK through the master out entry (lane %d: %.9g -> %.9g) — East's "
                    "spread write moved the prob-out",
                    l, (double)before[l], (double)se.masterLaneProbability(l));
        }
    });

    TEST("T6c EXPLICIT VOICE AT THE OUT ENTRY: after the East write, masterLaneProbability does NOT follow finalRandomByStrand → warpRow(0) (the hardcoded voice-0 residue) — it follows the explicit-voice accessor", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        for (int l = 0; l < 7; ++l) {
            const int strand = SequencerEngine::polyLaneStrand(l);
            const int step = se.masterLaneStep(l);
            const float clean = se.pe.finalRandomByStrand(strand, step);
            eastSpreadWrite(se, l, -0.85f);
            const float dirty = se.pe.finalRandomByStrand(strand, step);
            EXPECTF(dirty != clean,
                    "vacuous: the East write did not move the voice-0 row (lane %d)", l);
            // The OLD wiring returned exactly this dirty row (the leak Rodney scoped).
            EXPECTF(se.masterLaneProbability(l) != dirty,
                    "the master out still tracks warpRow(0) (lane %d: %.9g == %.9g) — "
                    "the hardcoded voice-0 residue is still on the out's route",
                    l, (double)se.masterLaneProbability(l), (double)dirty);
            // The NEW wiring: the explicit-voice accessor's value, in this divergent
            // state too (macroOwnSpread defaults 0 → the published base, bit-exact).
            EXPECTF(se.masterLaneProbability(l) ==
                        se.effectiveProbability(0, l, step, Lens::MacroOwn),
                    "the master out diverged from the explicit-voice accessor (lane %d)",
                    l);
        }
    });

    // ── T7: THE WARP LAYER (CC_PROMPT_UI_TO_RESOLVER_FINISH2 BUG 1) ────────────────
    // The ONE-LAYER-DOWN leak. The MacroOwn lens's three inputs were audited layer by
    // layer: the BASE (pubSlewedPoly*, via polySlewedPub) is written ONLY by
    // publishSlewed* copying the authored A/B slewed blend — East's spread passes write
    // random_ (applyMono → *Random, cachedPolySpread + refreshPolyRandomCell →
    // polyRandom), never the pubs — and the AMOUNT (macroOwnSpread) is the manager's
    // publishGlobal output. Both clean. The WARP layer was NOT: the lens composed via
    // warpDisplayMix, which resolves (F,w) through warpDelegated_[v][lane] — on an
    // EAST-OWNED lane that selects the per-(voice,lane) OWN-SLOT pair
    // (warpW/warpF/warpTilt/warpAtom*[v*7+lane]), which EAST'S V1/poly tabs author.
    // East's curve/w edit on an East-owned lane therefore modulated Macro's bars and
    // prob-outs — Rodney's "East STILL modulates Macro warp" — while every spread-layer
    // test stayed green. The contract: the MacroOwn lens composes with MACRO'S GLOBAL
    // bank cell (delegated=TRUE semantics) all the way down; the Owner lens keeps the
    // own-slot selection (the engine draw's twin — the audio consumes the own-slot
    // curve on East-owned lanes, and East's bars must show exactly that).
    TEST("T7 THE WARP LAYER: effectiveProbability(MacroOwn) composes with MACRO'S GLOBAL curve — an East-authored own-slot curve (deleg=false, the East-owned topology) never reaches the lens or the master out; the Owner lens (the engine twin) still follows the own slot", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        se.macroOwnSpread[MEL] = 0.f;   // isolate the warp layer: disp == base bit-exact
        // EAST's authorship: strong non-identity curves on the OWN slots (V1 + a poly voice).
        // MACRO's global cell stays neutral (w=0, identity — Macro authored nothing).
        dotModular::WarpBank bank;
        bank.warpW[dotModular::WarpBank::index(0, MEL)] = 0.9f;
        bank.warpF[dotModular::WarpBank::index(0, MEL)] = dotModular::PowerWarp{2.5};
        bank.warpW[dotModular::WarpBank::index(4, MEL)] = 0.85f;
        bank.warpF[dotModular::WarpBank::index(4, MEL)] = dotModular::PowerWarp{0.4};
        se.pe.setWarpBank(&bank);
        se.pe.setWarpDelegated(0, MEL, false);   // East-owned lane: own slot selected pre-fix
        se.pe.setWarpDelegated(4, MEL, false);
        // (a) THE COMPOSITION: the MacroOwn value is the GLOBAL-cell mix over the base
        //     (neutral global ⇒ bit-exact the raw base) — NOT the East-slot mix.
        for (int s = 0; s < 16; ++s) {
            const float base0 = redDot::SpreadInterp::polySlewedPub(se.pe, MEL, 0, s);
            EXPECTF(se.effectiveProbability(0, MEL, s, Lens::MacroOwn) == base0,
                    "MacroOwn(V1) composes with the EAST own-slot curve (step %d: %.9g != "
                    "base %.9g) — the warp layer is not Macro-own",
                    s, (double)se.effectiveProbability(0, MEL, s, Lens::MacroOwn),
                    (double)base0);
            const float base4 = redDot::SpreadInterp::polySlewedPub(se.pe, MEL, 4, s);
            EXPECTF(se.effectiveProbability(4, MEL, s, Lens::MacroOwn) == base4,
                    "MacroOwn(v=4) composes with the EAST own-slot curve (step %d: %.9g != "
                    "base %.9g) — the warp layer is not Macro-own",
                    s, (double)se.effectiveProbability(4, MEL, s, Lens::MacroOwn),
                    (double)base4);
        }
        // (b) THE PERTURBATION (the one-layer-down catcher): EDIT the East-authored
        //     slot curve + depth → MacroOwn UNCHANGED; the Owner lens DOES move (the
        //     own slot is East's to author — the vacuous guard).
        float macBefore[16], ownBefore[16];
        for (int s = 0; s < 16; ++s) {
            macBefore[s] = se.effectiveProbability(0, MEL, s, Lens::MacroOwn);
            ownBefore[s] = se.effectiveProbability(0, MEL, s, Lens::Owner);
        }
        bank.warpF[dotModular::WarpBank::index(0, MEL)] = dotModular::PowerWarp{0.3};
        bank.warpW[dotModular::WarpBank::index(0, MEL)] = 1.0f;
        int ownMoved = 0;
        for (int s = 0; s < 16; ++s) {
            EXPECTF(se.effectiveProbability(0, MEL, s, Lens::MacroOwn) == macBefore[s],
                    "LEAK: an East own-slot CURVE edit moved Macro's value (step %d: %.9g -> "
                    "%.9g) — East still modulates Macro warp",
                    s, (double)macBefore[s],
                    (double)se.effectiveProbability(0, MEL, s, Lens::MacroOwn));
            if (se.effectiveProbability(0, MEL, s, Lens::Owner) != ownBefore[s]) ++ownMoved;
        }
        EXPECTF(ownMoved == 16,
                "vacuous: the East slot edit did not move the Owner lens (only %d/16) — "
                "the own-slot curve is not being consumed anywhere",
                ownMoved);
        // (c) MACRO'S OWN CURVE DRIVES THE LENS: author the GLOBAL cell → the MacroOwn
        //     value is exactly the global-cell mix over the base (the authoritative
        //     formula), and it MOVED vs (a).
        bank.globalWarpW[MEL] = 0.8f;
        bank.globalWarpF[MEL] = dotModular::PowerWarp{3.0};
        int macMovedVsA = 0;
        for (int s = 0; s < 16; ++s) {
            const float base0 = redDot::SpreadInterp::polySlewedPub(se.pe, MEL, 0, s);
            const float want =
                (float)bank.warpedValue(0, MEL, (double)base0, /*delegated=*/true);
            EXPECTF(se.effectiveProbability(0, MEL, s, Lens::MacroOwn) == want,
                    "MacroOwn != the GLOBAL-cell mix (step %d: %.9g vs %.9g) — the warp "
                    "composition left Macro's curve",
                    s, (double)se.effectiveProbability(0, MEL, s, Lens::MacroOwn),
                    (double)want);
            if (want != base0) ++macMovedVsA;
        }
        EXPECTF(macMovedVsA == 16,
                "vacuous: Macro's global curve did not move the lens (%d/16) — the "
                "global cell is inert", macMovedVsA);
        // (d) THROUGH THE REAL ENTRY: the master prob-out does not move under the East
        //     slot edit (identity LOR so masterLaneStep is driven, the T6a setup).
        for (int s = 0; s < 7; ++s) {
            se.strandLenRef(s) = 16; se.strandOffRef(s) = 0; se.strandRotRef(s) = 0;
        }
        se.laneTick_[SequencerEngine::polyLaneStrand(MEL)] = 5;
        const float masterBefore = se.masterLaneProbability(MEL);
        bank.warpF[dotModular::WarpBank::index(0, MEL)] = dotModular::PowerWarp{0.7};
        bank.warpW[dotModular::WarpBank::index(0, MEL)] = 1.0f;
        EXPECTF(se.masterLaneProbability(MEL) == masterBefore,
                "LEAK through the master out: an East own-slot curve edit moved "
                "masterLaneProbability (%.9g -> %.9g) — the out carries East's warp",
                (double)masterBefore, (double)se.masterLaneProbability(MEL));
        // (e) the delegated (Macro-owned) topology is unchanged by the fix: the global
        //     cell serves the lens there too (the pre-fix behaviour on those lanes).
        se.pe.setWarpDelegated(0, MEL, true);
        const int step = se.masterLaneStep(MEL);
        const float baseNow = redDot::SpreadInterp::polySlewedPub(se.pe, MEL, 0, step);
        const float wantNow = (float)bank.warpedValue(0, MEL, (double)baseNow, true);
        EXPECTF(se.masterLaneProbability(MEL) == wantNow,
                "the delegated (Macro-owned) lane left the global-cell composition "
                "(%.9g vs %.9g)",
                (double)se.masterLaneProbability(MEL), (double)wantNow);
    });

    // ── T8: BASE-INDEPENDENCE — the direct shared-array perturbation ─────────────
    // The prompt's (b): write the arrays East's passes write, DIRECTLY, in the test —
    // every voice × lane × step of random_ (the post-spread finals: the applyMono mono
    // rows + the refreshPolyRandomCell poly rows), the warp mirror rebuilt from them
    // (East's own companion write), cachedPolySpread, and the engine spread store —
    // and the MacroOwn lens must come back bit-identical on every (voice, lane, step).
    TEST("T8 BASE-INDEPENDENCE: clobbering EVERY East-written shared array (random_ all rows, the mirror rebuilt from it, cachedPolySpread, spreadE) leaves effectiveProbability(MacroOwn) bit-identical — the lens reads none of them", {
        SequencerEngine se;
        fillSlewedGolden(se.pe);
        for (int l = 0; l < 7; ++l) se.macroOwnSpread[l] = ((l & 1) ? -0.4f : 0.55f) - 0.03f * l;
        // A wired bank with BOTH topologies live (some delegated, some not) and a
        // non-neutral global — the warp layer is exercised, not bypassed.
        dotModular::WarpBank bank;
        bank.globalWarpW[OCT] = 0.5f;
        bank.globalWarpF[OCT] = dotModular::PowerWarp{2.0};
        bank.warpW[dotModular::WarpBank::index(3, OCT)] = 0.9f;
        bank.warpF[dotModular::WarpBank::index(3, OCT)] = dotModular::PowerWarp{0.5};
        se.pe.setWarpBank(&bank);
        for (int l = 0; l < 7; ++l)
            for (int v = 0; v < 16; ++v) se.pe.setWarpDelegated(v, l, (v + l) & 1);
        static float snap[16][7][16];
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s)
                    snap[v][l][s] = se.effectiveProbability(v, l, s, Lens::MacroOwn);
        // ── THE PERTURBATION: every array an East pass writes, at full field size ──
        for (int l = 0; l < 7; ++l) {
            se.spreadERef(0, l) = 0.77f;                        // the V1 spread store (sprForLane's publish)
            for (int v = 0; v < 16; ++v) se.pe.cachedPolySpread[v][l] = -0.66f;   // combineSpread's cache
            for (int v = 0; v < 16; ++v)
                for (int s = 0; s < 16; ++s)
                    se.pe.random_[v][l][s] = 0.123f + 0.0001f * (v * 7 + l);      // the finals: applyMono + refreshPolyRandomCell
        }
        se.pe.rebuildWarpMirrorAll();   // the mirror rebuilt from the dirty random_ — East's companion write
        int unchanged = 0;
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; ++s)
                    if (se.effectiveProbability(v, l, s, Lens::MacroOwn) == snap[v][l][s]) ++unchanged;
        EXPECTF(unchanged == 16 * 7 * 16,
                "LEAK: the East-written arrays moved the MacroOwn lens (%d/1792 cells "
                "unchanged) — the lens reads an East-written shared array",
                unchanged);
        // Vacuous guard: the writes TOOK — the Owner lens (which legitimately reads the
        // spread stores + mirror) moved on a sample of cells.
        int ownerMoved = 0;
        for (int v = 0; v < 16; v += 5)
            for (int l = 0; l < 7; ++l)
                for (int s = 0; s < 16; s += 3)
                    if (se.effectiveProbability(v, l, s, Lens::Owner) !=
                        redDot::SpreadInterp::polySlewedPub(se.pe, l, v, s)) ++ownerMoved;
        EXPECTF(ownerMoved > 0,
                "vacuous: the perturbation moved nothing the Owner lens reads — the "
                "writes did not land");
    });

    // ── T9: THE LITERAL DELETIONS (the FINISH2 acceptance greps, pinned) ─────────
    // Raw counts, comments included — a comment tombstone counts exactly like a live
    // ref (the acceptance is grep -c, a line count). The deleted third-computation
    // accessor's name must be GONE from the engine header; the control-rate throttle
    // literal and the shadow-signature gate name must be GONE from the Macro module.
    TEST("T9 THE LITERAL DELETIONS: the deleted accessor's name has ZERO occurrences in SequencerEngine.hpp; the throttle literal + the shadow-sig gate name have ZERO lines in StraitsSandsMacroVisual.cpp (comments count)", {
        const std::string eng = slurp(repoRoot() + "/src/dsp/engines/SequencerEngine.hpp");
        EXPECTF(countOccurrences(eng, "macroOwnLaneProbability") == 0,
                "SequencerEngine.hpp still names the deleted third-computation accessor "
                "(%d occurrences) — the acceptance grep is a raw count; comment "
                "tombstones count",
                countOccurrences(eng, "macroOwnLaneProbability"));
        const std::string macro = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.cpp");
        static const char* const throttleAndGate[] = { "/32", "SweepSig" };
        const int lines = countLinesWith(macro, throttleAndGate, 2);
        EXPECTF(lines == 0,
                "StraitsSandsMacroVisual.cpp still carries %d lines naming the deleted "
                "control-rate throttle and/or the shadow-signature gate — the acceptance "
                "grep is a raw line count; comment tombstones count",
                lines);
    });

    // ── SUBSUMPTION: the MacroOwn lens reproduces the a2ccb842 formula, warp layer
    //    reconciled (FINISH2 BUG 1): pubSlewed base + macroOwnSpread via SpreadInterp +
    //    the MACRO-GLOBAL warp composition (warpDisplayMixMacroOwn — was warpDisplayMix,
    //    the East-authorable own-slot selection that leaked East's curve edits into
    //    Macro's bars/outs; the pinned-test reconciliation for the deliberate warp-layer
    //    change — bank-less rigs are bit-identical either way, so the a2ccb842-era
    //    VALUES this pin guarded are unchanged wherever no East-authored curve exists).
    TEST("SUBSUMPTION: effectiveProbability(MacroOwn) == the a2ccb842 formula (pubSlewed base + macroOwnSpread via SpreadInterp + Macro's GLOBAL warp cell) — bit-exact", {
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
                    const float want = se.pe.warpDisplayMixMacroOwn(v, LANE, (double)disp);
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

    // ── T6 STRUCTURAL (the FINISH wiring pin — the aspirational-comment failure
    //    mode): masterLaneProbability's BODY calls the ONE accessor and NOT the old
    //    path. The pre-FINISH tree's accessor comment claimed "the prob-outs ALL read
    //    THIS function" while the master entry still called the old path — comments
    //    are not wiring; this pin is.
    TEST("T6 STRUCTURAL: masterLaneProbability (the master prob-out entry) calls effectiveProbability — NOT finalRandomByStrand (the warpRow(0) voice-0 residue)", {
        const std::string eng = slurp(repoRoot() + "/src/dsp/engines/SequencerEngine.hpp");
        const size_t fn = eng.find("inline float masterLaneProbability");
        EXPECTF(fn != std::string::npos,
                "SequencerEngine.hpp: masterLaneProbability not found");
        const size_t end = eng.find("}", fn);
        EXPECTF(end != std::string::npos,
                "SequencerEngine.hpp: masterLaneProbability body not found");
        const std::string body = eng.substr(fn, end - fn);
        EXPECTF(contains(body, "effectiveProbability("),
                "masterLaneProbability does not call the ONE accessor — the out entry "
                "is off the one-producer route (the aspirational-comment failure mode)");
        EXPECTF(!contains(body, "finalRandomByStrand("),
                "masterLaneProbability still calls finalRandomByStrand — the old path "
                "(warpRow(0), the hardcoded voice-0 residue) is still on the out's route");
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

    // ── T5 RATE (structural — the module is rig-invisible): the heavy resolve runs at
    //    STEP rate inside the cache; the /32 throttle, the change-fingerprint gate and
    //    the S&H latch state are GONE; the write-elimination stays. ─────────────────
    TEST("T5 RATE (structural): the prob-out resolve is STEP-cached — inside the step-change gate; the /32 throttle + the SweepSig fingerprint gate + the S&H latch are GONE; write-elimination kept", {
        const std::string macro = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.cpp");
        const std::string hpp   = slurp(repoRoot() + "/src/StraitsSandsMacroVisual.hpp");
        EXPECTF(contains(macro, "step != row.step"),
                "StraitsSandsMacroVisual.cpp: the resolve left the STEP-change gate — it "
                "would run per-sample again");
        EXPECTF(contains(macro, "laneProbabilityAtStep(ch + 1, l, step)"),
                "StraitsSandsMacroVisual.cpp: the out no longer reads the ONE accessor "
                "through the resolver seam (ch v → voice v+1)");
        EXPECTF(!contains(macro, "probOutDiv") && !contains(hpp, "probOutDiv"),
                "the /32 probOutDiv throttle survives — superseded by the step cache");
        EXPECTF(!contains(macro, "sweepSig_") && !contains(hpp, "sweepSig_"),
                "the SweepSig change-fingerprint gate survives — the recompute it guarded "
                "no longer exists");
        EXPECTF(!contains(hpp, "probHeld") && !contains(hpp, "probLastStep"),
                "the S&H latch state survives — superseded by the step-keyed cache");
        EXPECTF(contains(macro, "memcmp(&out.voltages[ch], &v"),
                "the output write-elimination was lost — steady-state writes are back");
    });

    std::cout << std::string(70, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
