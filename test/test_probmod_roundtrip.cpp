// test_probmod_roundtrip.cpp ─ Step 0 of the probability-modifier unification
// (docs/design/PROBABILITY_MODIFIER_MODEL.md).
//
// PURPOSE: the REGRESSION ORACLE. Pins the CURRENT observable behaviour of the LOR
// (Length/Offset/Rotation) storage — through the CURRENT accessors — so that when the storage
// is later migrated to the unified len/off/rot[16][6] model (indexed by VoiceResolver::voiceSlot,
// slot 0 = V1 = mono), every set→get round-trip below must STILL hold bit-for-bit. If a later
// step breaks any of these, the migration changed behaviour and must be fixed.
//
// It deliberately tests through the ACTUAL current API surface — the two divergent conventions
// this refactor unifies:
//   MONO LOR : strandLenRef/OffRef/RotRef(strand)  +  strandLen/Off/Rot(strand)   [6 strands]
//   POLY LOR : direct arrays polyLen/Off/Rot[bank][lane]                          [15 banks × 4 lanes]
// and pins the VoiceResolver slot mapping (voiceSlot(V1)==0; poly bank b ↔ slot b+1) that the
// unified storage will index by.
//
// Sentinel strategy: every (which, index-space) cell gets a UNIQUE value, so any cross-strand,
// cross-voice, cross-lane, or mono↔poly leakage produces a mismatch on read-back.
//
// Build (mirrors test_voice_resolver.cpp):
//   g++ -std=c++17 -I. -I../src/dsp -I../src/dsp/engines -I../src/dsp/gates \
//       test_probmod_roundtrip.cpp \
//       ../src/dsp/engines/SequencerEngine.cpp ../src/dsp/engines/PatternEngine.cpp \
//       ../src/dsp/gates/GateState.cpp ../src/dsp/engines/ClockEngine.cpp \
//       -o /tmp/tpm && /tmp/tpm

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

static int g_pass = 0, g_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT_EQ(a,b) do { if((a)!=(b)) { std::ostringstream _s; \
    _s << "EXPECT_EQ(" #a "," #b ") : " << (a) << " != " << (b); \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_NEAR(a,b) do { double _d=(double)(a)-(double)(b); if(_d<0)_d=-_d; if(_d>1e-6){ \
    std::ostringstream _s; _s << "EXPECT_NEAR(" #a "," #b ") : " << (a) << " != " << (b); \
    throw std::runtime_error(_s.str()); } } while(0)

namespace SR = dotModular;   // STRAND_* enum lives in dotModular

// The 6 mono strands in enum order (MEL=0,OCT=1,RHYTHM=2,ACC=3,VAR=4,LEG=5).
static const int kStrands[6] = {
    SR::STRAND_MELODY, SR::STRAND_OCTAVE, SR::STRAND_RHYTHM,
    SR::STRAND_ACCENT, SR::STRAND_VARIATION, SR::STRAND_LEGATO
};
static const char* kStrandName[6] = { "MELODY","OCTAVE","RHYTHM","ACCENT","VARIATION","LEGATO" };

// Unique sentinels per (item, strand) so any cross-strand or cross-item leak is caught.
// item: 0=len 1=off 2=rot. Kept in the legal-ish range but distinct.
static int monoSentinel(int item, int strandEnum) { return 100 + item * 10 + strandEnum; }
// Poly: unique per (item, bank, lane).
static int polySentinel(int item, int bank, int lane) { return 1000 + item*300 + bank*4 + lane; }

int main() {
    using VR = dotModular::VoiceResolver;

    // ── 0. Pin the addressing the unified storage will use ────────────────────
    SUITE("VoiceResolver slot mapping (the unified-storage index)");
    TEST("voiceSlot(V1) == 0  (mono is slot 0, not a bolted-on special case)", {
        EXPECT_EQ(VR::voiceSlot(VR::kMonoVoice), 0);
        EXPECT_EQ(VR::kMonoSlot, 0);
    });
    TEST("poly bank b  ↔  voiceSlot == b + 1  (V2->slot1 … V16->slot15)", {
        for (int v = VR::kFirstPoly; v <= VR::kLastVoice; ++v)
            EXPECT_EQ(VR::voiceSlot(v), VR::polyBankIndex(v) + 1);
    });
    TEST("16 slots total, slotToVoice inverts voiceSlot", {
        EXPECT_EQ(VR::kVoiceSlots, 16);
        for (int v = 1; v <= 16; ++v) EXPECT_EQ(VR::slotToVoice(VR::voiceSlot(v)), v);
    });

    SequencerEngine eng;

    // ── 1. MONO LOR round-trips through strand*Ref / strand* ───────────────────
    SUITE("Mono LOR: set via strand*Ref, read via strand* — all 6 strands, no leak");
    // Write every strand/item to a unique sentinel.
    for (int i = 0; i < 6; ++i) {
        eng.strandLenRef(kStrands[i]) = monoSentinel(0, kStrands[i]);
        eng.strandOffRef(kStrands[i]) = monoSentinel(1, kStrands[i]);
        eng.strandRotRef(kStrands[i]) = monoSentinel(2, kStrands[i]);
    }
    for (int i = 0; i < 6; ++i) {
        TEST(std::string("mono LEN round-trips for ") + kStrandName[i], {
            EXPECT_EQ(eng.strandLen(kStrands[i]), monoSentinel(0, kStrands[i]));
        });
        TEST(std::string("mono OFF round-trips for ") + kStrandName[i], {
            EXPECT_EQ(eng.strandOff(kStrands[i]), monoSentinel(1, kStrands[i]));
        });
        TEST(std::string("mono ROT round-trips for ") + kStrandName[i], {
            EXPECT_EQ(eng.strandRot(kStrands[i]), monoSentinel(2, kStrands[i]));
        });
    }
    TEST("mono strands do not alias each other (all 18 cells distinct on read-back)", {
        for (int i = 0; i < 6; ++i) {
            EXPECT_EQ(eng.strandLen(kStrands[i]), monoSentinel(0, kStrands[i]));
            EXPECT_EQ(eng.strandOff(kStrands[i]), monoSentinel(1, kStrands[i]));
            EXPECT_EQ(eng.strandRot(kStrands[i]), monoSentinel(2, kStrands[i]));
        }
    });

    // ── 2. POLY LOR round-trips through the direct arrays ──────────────────────
    SUITE("Poly LOR: set/read polyLen/Off/Rot[bank][lane] — 15 banks × 4 lanes, no leak");
    for (int b = 0; b < 15; ++b)
        for (int l = 0; l < SequencerEngine::PL_LANES; ++l) {
            eng.polyLenERef(b, l) = polySentinel(0, b, l);
            eng.polyOffERef(b, l) = polySentinel(1, b, l);
            eng.polyRotERef(b, l) = polySentinel(2, b, l);
        }
    TEST("every poly (bank,lane) reads its OWN sentinel (no cross-bank / cross-lane leak)", {
        for (int b = 0; b < 15; ++b)
            for (int l = 0; l < SequencerEngine::PL_LANES; ++l) {
                EXPECT_EQ(eng.polyLenE(b, l), polySentinel(0, b, l));
                EXPECT_EQ(eng.polyOffE(b, l), polySentinel(1, b, l));
                EXPECT_EQ(eng.polyRotE(b, l), polySentinel(2, b, l));
            }
    });

    // ── 2a. Editor-order poly accessor: pins the editor→engine lane conversion ──
    // The unified [16][6] storage is EDITOR-ordered. polyLOR/polyLORRef present the (engine-ordered)
    // poly storage in editor order. Assert the conversion matches EDITOR_TO_ENGINE_LANE exactly, so a
    // lane-swap in the Step-2b physical merge is caught here.
    SUITE("Poly LOR editor-order accessor: editorLane→PL_ engine lane conversion is correct");
    TEST("polyLOR(bank, editorLane, item) reads the EDITOR_TO_ENGINE_LANE-mapped engine cell", {
        for (int b = 0; b < 15; ++b)
            for (int ed = 0; ed < 4; ++ed) {
                int engLane = dotModular::EDITOR_TO_ENGINE_LANE[ed];
                // reading via editor lane `ed` must return the sentinel stored at engine lane `engLane`
                EXPECT_EQ(eng.polyLOR(b, ed, SequencerEngine::LOR_LEN), polySentinel(0, b, engLane));
                EXPECT_EQ(eng.polyLOR(b, ed, SequencerEngine::LOR_OFF), polySentinel(1, b, engLane));
                EXPECT_EQ(eng.polyLOR(b, ed, SequencerEngine::LOR_ROT), polySentinel(2, b, engLane));
            }
    });
    TEST("polyLORRef(editorLane) and polyLenE(engineLane) address the SAME unified cell", {
        // write via the editor-order ref at editor lane ed; the engine-order accessor at the
        // corresponding engine lane (EDITOR_TO_ENGINE_LANE[ed]) must read the same value, and the
        // editor-order reader must round-trip. Proves the two accessor families agree post-merge.
        for (int b = 0; b < 15; ++b)
            for (int ed = 0; ed < 4; ++ed) {
                int engLane = dotModular::EDITOR_TO_ENGINE_LANE[ed];
                int val = 7000 + b*4 + ed;
                eng.polyLORRef(b, ed, SequencerEngine::LOR_LEN) = val;
                EXPECT_EQ(eng.polyLOR(b, ed, SequencerEngine::LOR_LEN), val);   // editor round-trip
                EXPECT_EQ(eng.polyLenE(b, engLane), val);                       // engine accessor agrees
            }
        // restore the sentinels (engine-order) for later suites
        for (int b = 0; b < 15; ++b)
            for (int l = 0; l < SequencerEngine::PL_LANES; ++l)
                eng.polyLenERef(b, l) = polySentinel(0, b, l);
    });

    // ── 3. Mono ↔ poly independence (separate storage today; must still hold as
    //       distinct SLOTS after unification: slot 0 vs slots 1..15) ────────────
    SUITE("Mono and poly storage are independent (writing one never disturbs the other)");
    TEST("mono values intact after all poly writes", {
        for (int i = 0; i < 6; ++i) {
            EXPECT_EQ(eng.strandLen(kStrands[i]), monoSentinel(0, kStrands[i]));
            EXPECT_EQ(eng.strandOff(kStrands[i]), monoSentinel(1, kStrands[i]));
            EXPECT_EQ(eng.strandRot(kStrands[i]), monoSentinel(2, kStrands[i]));
        }
    });
    TEST("re-writing mono does not perturb any poly cell", {
        for (int i = 0; i < 6; ++i) eng.strandLenRef(kStrands[i]) = monoSentinel(0, kStrands[i]) + 1;
        for (int b = 0; b < 15; ++b)
            for (int l = 0; l < SequencerEngine::PL_LANES; ++l)
                EXPECT_EQ(eng.polyLenE(b, l), polySentinel(0, b, l));
    });

    // ── 4. Spread storage on the engine (B-corrected): spread[16][6] editor-ordered, accessed
    //       engine-order via spreadE/spreadERef. Pins the engine→editor conversion so the migration
    //       of mono spread from the visual onto the engine is provably permutation-correct. ───────
    SUITE("Spread engine storage: spreadERef(slot,engLane) round-trips + editor↔engine agree");
    TEST("spreadERef writes the ENGINE_LANE_TO_EDITOR-mapped cell; spreadE reads it back", {
        for (int slot = 0; slot < SequencerEngine::kVoiceSlots; ++slot)
            for (int engLane = 0; engLane < 4; ++engLane) {
                float val = 0.01f * slot + 0.001f * engLane - 0.5f;   // distinct bipolar sentinel
                eng.spreadERef(slot, engLane) = val;
                // engine accessor round-trips
                EXPECT_NEAR(eng.spreadE(slot, engLane), val);
                // and it landed at the editor-lane cell ENGINE_LANE_TO_EDITOR[engLane]
                EXPECT_NEAR(eng.spread[slot][dotModular::ENGINE_LANE_TO_EDITOR[engLane]], val);
            }
    });
    TEST("distinct slots/lanes never alias (no cross-voice / cross-lane spread leak)", {
        for (int slot = 0; slot < SequencerEngine::kVoiceSlots; ++slot)
            for (int engLane = 0; engLane < 4; ++engLane)
                EXPECT_NEAR(eng.spreadE(slot, engLane), 0.01f * slot + 0.001f * engLane - 0.5f);
    });
    TEST("spread storage is independent of lorStore_ (writing spread leaves LOR intact)", {
        // LOR mono slot-0 sentinels from suite 1 must survive spread writes above.
        for (int i = 0; i < 6; ++i)
            EXPECT_EQ(eng.strandLen(kStrands[i]), monoSentinel(0, kStrands[i]) + 1); // +1 from suite-3 rewrite
    });

    // ── 5. Probability *Random storage (item 4 prep): pin mono + poly probability arrays through
    //       their current access so the [16][6][16] unification is provably behaviour-preserving. ──
    SUITE("Probability *Random storage round-trips (regression net for the [16][6][16] unification)");
    {
        PatternEngine pe;
        // Seed each mono strand array with a distinct per-(strand,step) value.
        auto seedMono = [&](float* arr, float base) { for (int s = 0; s < 16; ++s) arr[s] = base + 0.001f * s; };
        seedMono(pe.melodyRandom,    0.10f);
        seedMono(pe.octaveRandom,    0.20f);
        seedMono(pe.rhythmRandom,    0.30f);
        seedMono(pe.accentRandom,    0.40f);
        seedMono(pe.variationRandom, 0.50f);
        seedMono(pe.legatoRandom,    0.60f);
        // finalRandomByStrand (the mono accessor) must map each strand to its own array.
        struct { int strand; float base; } mc[] = {
            {dotModular::STRAND_MELODY,0.10f},{dotModular::STRAND_OCTAVE,0.20f},
            {dotModular::STRAND_RHYTHM,0.30f},{dotModular::STRAND_ACCENT,0.40f},
            {dotModular::STRAND_VARIATION,0.50f},{dotModular::STRAND_LEGATO,0.60f},
        };
        TEST("mono finalRandomByStrand maps each strand to its own *Random array (all steps)", {
            for (auto& c : mc)
                for (int s = 0; s < 16; ++s)
                    EXPECT_NEAR(pe.finalRandomByStrand(c.strand, s), c.base + 0.001f * s);
        });
        // Poly arrays: distinct per (voice, lane, step).
        for (int v = 0; v < 15; ++v)
            for (int s = 0; s < 16; ++s) {
                pe.polyRhythmRandom[v][s] = 1.0f + v * 0.1f + s * 0.001f;
                pe.polyMelodyRandom[v][s] = 2.0f + v * 0.1f + s * 0.001f;
                pe.polyOctaveRandom[v][s] = 3.0f + v * 0.1f + s * 0.001f;
                pe.polyAccentRandom[v][s] = 4.0f + v * 0.1f + s * 0.001f;
            }
        TEST("poly *Random arrays hold distinct per-(voice,lane,step) values, no aliasing", {
            for (int v = 0; v < 15; ++v)
                for (int s = 0; s < 16; ++s) {
                    EXPECT_NEAR(pe.polyRhythmRandom[v][s], 1.0f + v * 0.1f + s * 0.001f);
                    EXPECT_NEAR(pe.polyMelodyRandom[v][s], 2.0f + v * 0.1f + s * 0.001f);
                    EXPECT_NEAR(pe.polyOctaveRandom[v][s], 3.0f + v * 0.1f + s * 0.001f);
                    EXPECT_NEAR(pe.polyAccentRandom[v][s], 4.0f + v * 0.1f + s * 0.001f);
                }
        });
        TEST("mono and poly probability storage are independent (no cross-write)", {
            // mono still intact after all poly writes
            for (auto& c : mc)
                EXPECT_NEAR(pe.finalRandomByStrand(c.strand, 7), c.base + 0.001f * 7);
        });
    }

    std::cout << "\n\033[1m" << g_pass << " passed, " << g_fail << " failed\033[0m\n";
    return g_fail ? 1 : 0;
}
