// test_subgate_accent.cpp — the ACCENT verdict on subgate (ghost + ratchet) onsets.
//
// COVERAGE GAP this closes (Rodney, 2026-11-24): the subgate drive's interaction with
// LEGATO/REST is asserted directly (test_subgate), but the ACCENT outcome on a ghost or
// ratchet onset was only IMPLICIT — folded into the march digests (pv.accented) as a
// tripwire, never stated as a rule. This makes it explicit, mono AND poly-per-voice.
//
// THE RULE (SequencerEngine): a subgate onset draws the accent lane and decides
//     accented = (r_accent < accTh),  accTh = subResLive ? subRes[subLevel][ACCENT]
//                                                        : accentProb
// where subLevel is RATCHET or GHOST (the rising edge's level). A rest is never accented;
// a tie carries the previous onset's accent. The poly path is the same, per voice, with the
// voice's OWN subRes/accentProb and polySubLevel_.
//
// ROBUST THRESHOLDS: the accent lane draw r_accent is a uniform in [0,1). So accTh = 1.0
// accents ALWAYS (r<1) and accTh = 0.0 NEVER (r<0 is false) — independent of the exact draw
// or any warp, which lets the test pin the ROUTING (which level/voice the threshold comes
// from) without controlling the RNG.
//
// Build: see test/run_all.sh (entry "test_subgate_accent|$SE $GS $PE").

#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "SubgateKnobs.hpp"
#include "star_seam.hpp"

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;
namespace SK = dotModular::SubgateKnobs;
using D = MonoDecision;
static dotModular::VoiceSlot SLOT(int v){ return dotModular::VoiceResolver::slot(v + 1); }

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.5f;
    in.octaveLo = 0; in.octaveHi = 0;
    return in;
}

// Set the mono voice's subgate bank so a given level sounds (REST=0), re-articulates
// (LEGATO=0), and carries the ACCENT threshold we want. subResLive engages the subgate bank.
static void setMonoSub(SequencerEngine& eng, float ratchetAcc, float ghostAcc) {
    auto& v = eng.voices[SLOT(0)];
    v.subResLive = true;
    for (int lvl = 0; lvl < SK::LEVELS; ++lvl) {
        v.subRes[lvl][SK::REST]    = 0.f;   // never rest -> the onset sounds
        v.subRes[lvl][SK::LEGATO]  = 0.f;   // never tie  -> a fresh re-articulated onset
        v.subRes[lvl][SK::ADVANCE] = 0.f;
    }
    v.subRes[SK::RATCHET][SK::ACCENT] = ratchetAcc;
    v.subRes[SK::GHOST][SK::ACCENT]   = ghostAcc;
}

// Drive one mono edge through the real engine. restProb/legatoProb are the MAIN-path values
// (used when subResLive is false); the subgate bank governs when subResLive is true.
static StepResult stepMono(SequencerEngine& eng, bool mainRise, bool ratchetRise, bool ghostRise,
                           bool mainHigh, bool ghostHigh, float restProb, float legatoProb,
                           float noteVal, float variation = 0.5f) {
    PatternInput in = makeInput();
    auto& v = eng.voices[SLOT(0)];
    v.restProb = restProb; v.legatoProb = legatoProb; v.variationProb = variation; v.qmixLevel = 0.f;
    StepResult r = eng.executeModeBSubdivided(mainRise, mainHigh, ratchetRise, noteVal, in, ghostRise, ghostHigh);
    // Mirror the module-layer gate maintenance between edges (as test_subgate's step() does),
    // so the next call sees the held predecessor (ties) / the sustaining ghost.
    const bool isRest = (r.decision == MonoDecision::Rest);
    const bool ghostSounding = eng.ghostActive && ghostHigh;
    const bool gateOpen = !isRest && (mainHigh || ghostSounding);
    eng.gs.gateHeld = gateOpen;
    if (!gateOpen) eng.gs.holdRemain = 0.f;
    eng.gsStep.gateHeld = gateOpen;
    return r;
}

int main() {
    std::cout << "\033[1mSubgate accent — the ghost/ratchet onset accent verdict\033[0m\n";
    std::cout << std::string(64, '=') << "\n";

    // Pin the accent lane draw low (robust: any value in [0,1) is < 1.0 and not < 0.0).
    auto prime = [](SequencerEngine& eng){
        for (int i = 0; i < 16; ++i) {
            eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
            eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.1f; eng.pe.variationRandom[i]=.7f;
            eng.pe.legatoRandom[i]=.1f;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G1 — RATCHET onset: accent is live and comes from subRes[RATCHET][ACCENT]");
    TEST("ratchet onset accents when subRes[RATCHET][ACCENT]=1, not when =0", {
        {   SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
            setMonoSub(eng, /*ratchetAcc=*/1.f, /*ghostAcc=*/0.f);
            stepMono(eng, true, true, false, true, false, 0.f, 0.f, 4.f);           // main+ratchet onset
            StepResult r = stepMono(eng, false, true, false, true, false, 0.f, 0.f, 4.f); // ratchet sub-cell
            EXPECT(r.decision != D::Rest && r.decision != D::MidNote);
            EXPECT(r.accented);                                                      // accTh=1 -> accented
        }
        {   SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
            setMonoSub(eng, /*ratchetAcc=*/0.f, /*ghostAcc=*/1.f);
            stepMono(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
            StepResult r = stepMono(eng, false, true, false, true, false, 0.f, 0.f, 4.f);
            EXPECT(!r.accented);                                                     // accTh=0 -> never
        }
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G2 — GHOST onset: accent is live and comes from subRes[GHOST][ACCENT]");
    TEST("ghost onset accents when subRes[GHOST][ACCENT]=1, not when =0", {
        {   SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
            setMonoSub(eng, /*ratchetAcc=*/0.f, /*ghostAcc=*/1.f);
            stepMono(eng, true, false, false, true, false, 0.f, 0.f, 7.f);           // main onset
            stepMono(eng, false, false, false, false, false, 0.f, 0.f, 7.f);         // silent gap
            StepResult r = stepMono(eng, false, false, true, false, true, 0.f, 0.f, 7.f); // ghost onset
            EXPECT(eng.ghostActive);
            EXPECT(r.decision != D::Rest);
            EXPECT(r.accented);
        }
        {   SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
            setMonoSub(eng, /*ratchetAcc=*/1.f, /*ghostAcc=*/0.f);
            stepMono(eng, true, false, false, true, false, 0.f, 0.f, 7.f);
            stepMono(eng, false, false, false, false, false, 0.f, 0.f, 7.f);
            StepResult r = stepMono(eng, false, false, true, false, true, 0.f, 0.f, 7.f);
            EXPECT(eng.ghostActive);
            EXPECT(!r.accented);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G3 — PER-LEVEL routing: ghost and ratchet read DIFFERENT accent thresholds");
    TEST("one engine, ghost-accent=1 / ratchet-accent=0: ghost accents, ratchet does not", {
        SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
        setMonoSub(eng, /*ratchetAcc=*/0.f, /*ghostAcc=*/1.f);
        // ratchet onset (in-gate) -> ratchet threshold (0) -> NOT accented
        stepMono(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
        StepResult rat = stepMono(eng, false, true, false, true, false, 0.f, 0.f, 4.f);
        EXPECT(!rat.accented);
        // then a gap, then a ghost onset -> ghost threshold (1) -> accented
        stepMono(eng, false, false, false, false, false, 0.f, 0.f, 4.f);
        StepResult gho = stepMono(eng, false, false, true, false, true, 0.f, 0.f, 4.f);
        EXPECT(eng.ghostActive);
        EXPECT(gho.accented);
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G4 — a rested subgate onset is NEVER accented (accent needs a sounding note)");
    TEST("ghost onset with subRes[GHOST][REST]=1 rests -> not accented even with accent=1", {
        SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
        setMonoSub(eng, 0.f, /*ghostAcc=*/1.f);
        eng.voices[SLOT(0)].subRes[SK::GHOST][SK::REST] = 1.f;   // force the ghost to rest
        stepMono(eng, true, false, false, true, false, 0.f, 0.f, 7.f);
        stepMono(eng, false, false, false, false, false, 0.f, 0.f, 7.f);
        StepResult r = stepMono(eng, false, false, true, false, true, 0.f, 0.f, 7.f);
        EXPECT(r.decision == D::Rest);
        EXPECT(!r.accented);                                      // no accent on a rest
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G5 — a tied sub-cell CARRIES the onset's accent (not re-drawn)");
    TEST("accented ratchet onset, then a legato sub-cell: the tie keeps accented=true", {
        SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
        setMonoSub(eng, /*ratchetAcc=*/1.f, 0.f);
        StepResult onset = stepMono(eng, true, true, false, true, false, 0.f, 0.f, 7.f);
        EXPECT(onset.accented);
        // flip the ratchet accent to 0 AND legato to 1 so the next sub-cell ties; the carried
        // accent must survive even though a fresh draw would now be unaccented.
        eng.voices[SLOT(0)].subRes[SK::RATCHET][SK::ACCENT] = 0.f;
        eng.voices[SLOT(0)].subRes[SK::RATCHET][SK::LEGATO] = 1.f;
        StepResult tie = stepMono(eng, false, true, false, true, false, 0.f, 0.f, 7.f);
        // a continued note: Tie / Legato / LegatoMax (legTh=1 -> LegatoMax) — NOT a fresh onset
        EXPECT(tie.decision == D::Tie || tie.decision == D::Legato || tie.decision == D::LegatoMax);
        EXPECT(tie.accented);                                     // carried from the onset, not re-drawn
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G6 — the NON-subgate fallback: accTh is the voice accentProb when !subResLive");
    TEST("subResLive=false: a ratchet onset uses accentProb (1 -> accent, 0 -> not)", {
        {   SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
            eng.voices[SLOT(0)].subResLive = false;
            eng.voices[SLOT(0)].accentProb = 1.f;
            stepMono(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
            StepResult r = stepMono(eng, false, true, false, true, false, 0.f, 0.f, 4.f);
            EXPECT(r.accented);
        }
        {   SequencerEngine eng; eng.numPolyVoices = 0; prime(eng);
            eng.voices[SLOT(0)].subResLive = false;
            eng.voices[SLOT(0)].accentProb = 0.f;
            stepMono(eng, true, true, false, true, false, 0.f, 0.f, 4.f);
            StepResult r = stepMono(eng, false, true, false, true, false, 0.f, 0.f, 4.f);
            EXPECT(!r.accented);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────────
    SUITE("G7 — POLY per voice: each SOUNDING voice's ghost accent reads ITS OWN threshold");
    // The star graph decides WHICH poly voices sound a given ghost; we don't assume. The pin:
    // every voice that DOES sound carries the accent iff ITS OWN subRes[GHOST][ACCENT] is hot,
    // and flipping a voice's threshold flips only that voice's accent (per-voice, not shared).
    auto runGhost = [](float v0ghostAcc, float v1ghostAcc, int& soundedOut,
                       bool& v0acc, bool& v0snd, bool& v1acc, bool& v1snd) {
        SequencerEngine eng; eng.numPolyVoices = 2; injectLegacyStarGraph(eng);
        for (int i = 0; i < 16; ++i) {
            eng.pe.melodyRandom[i]=.3f; eng.pe.octaveRandom[i]=.4f; eng.pe.qmixRandom[i]=.5f;
            eng.pe.rhythmRandom[i]=.6f; eng.pe.accentRandom[i]=.1f; eng.pe.variationRandom[i]=.7f;
            eng.pe.legatoRandom[i]=.1f;
        }
        for (int vi = 0; vi < 2; ++vi) {
            auto& v = eng.voices[dotModular::VoiceResolver::slot(vi + 1)];
            v.restProb = 0.f; v.legatoProb = 0.f; v.variationProb = 0.5f; v.qmixLevel = 0.f;
            v.subResLive = true;
            for (int lvl = 0; lvl < SK::LEVELS; ++lvl) {
                v.subRes[lvl][SK::REST]=0.f; v.subRes[lvl][SK::LEGATO]=0.f;
                v.subRes[lvl][SK::ADVANCE]=0.f; v.subRes[lvl][SK::ACCENT]=0.f;
            }
        }
        eng.voices[dotModular::VoiceResolver::slot(1)].subRes[SK::GHOST][SK::ACCENT] = v0ghostAcc;
        eng.voices[dotModular::VoiceResolver::slot(2)].subRes[SK::GHOST][SK::ACCENT] = v1ghostAcc;
        auto polyStep = [&](bool mR,bool rR,bool gR,bool mH,bool gH){
            PatternInput in = makeInput();
            StepResult r = eng.executeModeBSubdivided(mR, mH, rR, 7.f, in, gR, gH);
            if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
            const bool isRest = (r.decision == MonoDecision::Rest);
            const bool ghostSounding = eng.ghostActive && gH;
            const bool gateOpen = !isRest && (mH || ghostSounding);
            eng.gs.gateHeld = gateOpen; if (!gateOpen) eng.gs.holdRemain = 0.f;
            eng.gsStep.gateHeld = gateOpen;
        };
        polyStep(true, false, false, true, false);    // main onset
        polyStep(false, false, false, false, false);  // silent gap
        polyStep(false, false, true,  false, true);   // ghost onset
        v0snd = eng.voices[dotModular::VoiceResolver::slot(1)].gs.gateHeld;
        v1snd = eng.voices[dotModular::VoiceResolver::slot(2)].gs.gateHeld;
        v0acc = eng.voices[dotModular::VoiceResolver::slot(1)].accented;
        v1acc = eng.voices[dotModular::VoiceResolver::slot(2)].accented;
        soundedOut = (v0snd?1:0) + (v1snd?1:0);
    };
    TEST("every sounding voice follows its own ghost accent; flipping a voice flips only it", {
        int sounded; bool v0a,v0s,v1a,v1s;
        // polarity A: voice0 hot, voice1 cold
        runGhost(1.f, 0.f, sounded, v0a, v0s, v1a, v1s);
        EXPECT(sounded >= 1);                         // the march reaches the ghost (coverage)
        if (v0s) EXPECT(v0a);                          // sounding voice 0 reads its own hot threshold
        if (v1s) EXPECT(!v1a);                         // sounding voice 1 reads its own cold threshold
        bool v0s_A=v0s, v1s_A=v1s;
        // polarity B: voice0 cold, voice1 hot — accent does not change WHICH voices sound
        runGhost(0.f, 1.f, sounded, v0a, v0s, v1a, v1s);
        EXPECT(v0s == v0s_A && v1s == v1s_A);          // same voices sound (accent is not a gate)
        if (v0s) EXPECT(!v0a);                         // flipped: voice 0 now cold
        if (v1s) EXPECT(v1a);                          // flipped: voice 1 now hot
    });

    std::cout << "\n" << std::string(64, '=') << "\n";
    std::cout << (g_fail==0?"\033[32m":"\033[31m") << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail==0 ? 0 : 1;
}

/* ── MUTATION LOG (to run once green) ─────────────────────────────────────────
 * In SequencerEngine.cpp. Each mutant built to its own binary, build verified.
 *   M1  mono accTh forced to accentProb (ignore subRes[subLevel][ACCENT]) → G1/G2/G3 FAIL
 *       (the per-level routing collapses).
 *   M2  mono `result.accented = (r_accent < accTh)` → `= false` on the onset branch → G1/G2 FAIL.
 *   M3  rest branch `result.accented = false` → `= true` → G4 FAIL.
 *   M4  tie branch stops carrying (accented = (r_accent<accTh) instead of lastStepResult) → G5 FAIL.
 *   M5  poly accTh uses voice 0's subRes for every voice (drop the per-voice `v.`) → G7 FAIL.
 * ─────────────────────────────────────────────────────────────────────────── */
