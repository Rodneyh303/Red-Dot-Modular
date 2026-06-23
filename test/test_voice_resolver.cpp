// test_voice_resolver.cpp ─ Step 2 of the Voice Resolver plan: the PARALLEL-RUN ASSERTION.
//
// Proves the read-only resolver shadow faithfully mirrors reality BEFORE any caller depends
// on it: for every voice V (1..16) and lane (REST/MELODY/OCTAVE), the resolver's
// laneProbability / laneStep / laneProbabilityAtStep must return BIT-EXACTLY what the direct
// engine accessors return —
//     V1  → eng.masterLane* (the mono master strand)
//     V≥2 → eng.polyLane*(.., V-2)
// plus the metadata dispatch (isMono/isPoly/polyBankIndex/baseEditableHere/spreadMode).
//
// Strategy: construct a REAL SequencerEngine (now possible post-Xoroshiro-deprecation — the
// test rack shim constructs it), seed every poly bank [v][step] and the master strand arrays
// with UNIQUE sentinel values so any mis-dispatch (wrong voice, wrong lane, off-by-one in the
// -2 map) produces a mismatch. Then compare resolver vs direct for all voices × lanes.
//
// Build (needs the engine TUs for getStrandIdx / GateState / ClockEngine):
//   g++ -std=c++17 -I. -I../src/dsp -I../src/dsp/engines -I../src/dsp/gates \
//       test_voice_resolver.cpp \
//       ../src/dsp/engines/SequencerEngine.cpp ../src/dsp/engines/PatternEngine.cpp \
//       ../src/dsp/gates/GateState.cpp ../src/dsp/engines/ClockEngine.cpp \
//       -o /tmp/tvr && /tmp/tvr

#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)
#define EXPECT_EQ(a,b) do { if((a)!=(b)) { std::ostringstream _s; \
    _s << "EXPECT_EQ(" #a "," #b ") : " << (a) << " != " << (b); \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_BITEQ(a,b) do { float _x=(a),_y=(b); if(std::memcmp(&_x,&_y,sizeof(float))!=0) { \
    std::ostringstream _s; _s << "bit mismatch: " << _x << " vs " << _y; \
    throw std::runtime_error(_s.str()); } } while(0)

static int g_pass = 0, g_fail = 0;

using dotModular::VoiceResolver;
using E = SequencerEngine;

// Distinct sentinel per (voice, lane, step) so any mis-dispatch is visible.
static float sentinel(int voice, int lane, int step) {
    return (float)(voice * 10000 + lane * 100 + step) * 0.0001f + 0.013f;
}

static void seedEngine(SequencerEngine& eng) {
    eng.numPolyVoices = 15;               // all poly voices active
    eng.totalStepsElapsed = 0;            // deterministic step → strand idx
    // Master strand arrays (voice 1 source). Use voice index 1 for sentinels.
    for (int s = 0; s < 16; ++s) {
        eng.pe.rhythmRandom[s] = sentinel(1, E::PL_REST,   s);
        eng.pe.melodyRandom[s] = sentinel(1, E::PL_MELODY, s);
        eng.pe.octaveRandom[s] = sentinel(1, E::PL_OCTAVE, s);
        eng.pe.accentRandom[s] = sentinel(1, E::PL_ACCENT, s);
    }
    // Poly banks (voices 2..16 → bank 0..14).
    for (int v = 0; v < 15; ++v)
        for (int s = 0; s < 16; ++s) {
            eng.pe.polyRhythmRandom[v][s] = sentinel(v + 2, E::PL_REST,   s);
            eng.pe.polyMelodyRandom[v][s] = sentinel(v + 2, E::PL_MELODY, s);
            eng.pe.polyOctaveRandom[v][s] = sentinel(v + 2, E::PL_OCTAVE, s);
            eng.pe.polyAccentRandom[v][s] = sentinel(v + 2, E::PL_ACCENT, s);
            // identity LOR so step index is predictable (= totalStepsElapsed & 0x0F = 0)
            eng.polyLen[v][s % 3] = 16; // harmless; lanes use [v][lane]
        }
    // identity LOR per (voice, lane)
    for (int v = 0; v < 15; ++v)
        for (int l = 0; l < 4; ++l) { eng.polyLen[v][l] = 16; eng.polyOff[v][l] = 0; eng.polyRot[v][l] = 0; }
}

int main() {
    SequencerEngine eng;
    seedEngine(eng);
    VoiceResolver r(eng);
    const int lanes[4] = { E::PL_REST, E::PL_MELODY, E::PL_OCTAVE, E::PL_ACCENT };

    SUITE("VoiceResolver — metadata dispatch");
    TEST("voice identity + bank index across all 16 voices", {
        EXPECT(VoiceResolver::isMono(1));
        EXPECT(!VoiceResolver::isPoly(1));
        EXPECT_EQ(VoiceResolver::polyBankIndex(1), -1);
        for (int v = 2; v <= 16; ++v) {
            EXPECT(VoiceResolver::isPoly(v));
            EXPECT(!VoiceResolver::isMono(v));
            EXPECT_EQ(VoiceResolver::polyBankIndex(v), v - 2);
        }
    });
    TEST("baseEditableHere: false for mono, true for poly", {
        EXPECT(!VoiceResolver::baseEditableHere(1));
        for (int v = 2; v <= 16; ++v) EXPECT(VoiceResolver::baseEditableHere(v));
    });
    TEST("spreadMode: MONO_DRAW for V1, AVERAGE_POLY for V2..V16", {
        EXPECT_EQ((int)VoiceResolver::spreadMode(1), (int)redDot::SpreadInterp::MONO_DRAW);
        for (int v = 2; v <= 16; ++v)
            EXPECT_EQ((int)VoiceResolver::spreadMode(v), (int)redDot::SpreadInterp::AVERAGE_POLY);
    });
    TEST("activeVoiceCount = numPolyVoices + 1", {
        EXPECT_EQ(r.activeVoiceCount(), eng.numPolyVoices + 1);
    });

    SUITE("VoiceResolver — laneProbability == direct accessors (bit-exact, all voices/lanes)");
    TEST("V1 laneProbability == masterLaneProbability", {
        for (int li = 0; li < 4; ++li)
            EXPECT_BITEQ(r.laneProbability(1, lanes[li]), eng.masterLaneProbability(lanes[li]));
    });
    TEST("V2..V16 laneProbability == polyLaneProbability(lane, V-2)", {
        for (int v = 2; v <= 16; ++v)
            for (int li = 0; li < 4; ++li)
                EXPECT_BITEQ(r.laneProbability(v, lanes[li]),
                             eng.polyLaneProbability(lanes[li], v - 2));
    });

    SUITE("VoiceResolver — laneStep == direct accessors");
    TEST("V1 laneStep == masterLaneStep", {
        for (int li = 0; li < 4; ++li)
            EXPECT_EQ(r.laneStep(1, lanes[li]), eng.masterLaneStep(lanes[li]));
    });
    TEST("V2..V16 laneStep == polyLaneStep(lane, V-2)", {
        for (int v = 2; v <= 16; ++v)
            for (int li = 0; li < 4; ++li)
                EXPECT_EQ(r.laneStep(v, lanes[li]), eng.polyLaneStep(lanes[li], v - 2));
    });

    SUITE("VoiceResolver — laneProbabilityAtStep == direct accessors (every step)");
    TEST("V2..V16 laneProbabilityAtStep matches at all 16 steps", {
        for (int v = 2; v <= 16; ++v)
            for (int li = 0; li < 4; ++li)
                for (int s = 0; s < 16; ++s)
                    EXPECT_BITEQ(r.laneProbabilityAtStep(v, lanes[li], s),
                                 eng.polyLaneProbabilityAtStep(lanes[li], v - 2, s));
    });
    TEST("V1 laneProbabilityAtStep == masterLaneProbability (mono ignores explicit step)", {
        for (int li = 0; li < 4; ++li)
            for (int s = 0; s < 16; ++s)
                EXPECT_BITEQ(r.laneProbabilityAtStep(1, lanes[li], s),
                             eng.masterLaneProbability(lanes[li]));
    });

    SUITE("VoiceResolver — sentinels prove no cross-voice / cross-lane leakage");
    TEST("each poly voice/lane reads its OWN sentinel (no -2 off-by-one, no lane swap)", {
        for (int v = 2; v <= 16; ++v)
            for (int li = 0; li < 4; ++li) {
                float got = r.laneProbabilityAtStep(v, lanes[li], 0);
                float want = sentinel(v, lanes[li], 0);
                EXPECT_BITEQ(got, want);
            }
    });

    std::cout << "\n\033[1m" << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
