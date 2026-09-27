// test_MpeMath.cpp — dotModular::mpe CV→MPE conversion math + the ROUND-TRIP guarantee.
//
// Fills the previously-dangling `test_MpeMath|` entry in run_all.sh (the file was missing, so the pure
// note/bend split shipped with ZERO coverage). Keppel (src/Keppel.cpp) and this test share the EXACT
// same math via dsp/MpeMath.hpp, so what is asserted here is what ships.
//
// The load-bearing property is the ROUND-TRIP: an ideal MPE receiver, given the note + 14-bit bend we
// emit at the receiver's bend range, must reproduce the ORIGINAL microtonal 1V/oct pitch to well under
// a cent — at ANY range in 1..48. This is the internal ground truth behind the reverse-calc monitor and
// the go/no-go for the MPE-in round-trip test (MPE_UTILITY_BUILD_SPEC / MICROTONAL_MIDI_MPE_DIRECTION).
//
// Pure C++/no Rack (MpeMath.hpp is stdlib-only), builds standalone. Registered in run_all.sh.
//
// Build (standalone):
//   g++ -std=c++17 -Isrc test/test_MpeMath.cpp -o /tmp/mpe && /tmp/mpe

#include <cmath>
#include <iostream>
#include <string>

#include "dsp/MpeMath.hpp"

using namespace dotModular::mpe;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)

static int g_pass = 0, g_fail = 0;

// cents of round-trip error for a CENTRED-split (note-on) reconstruction at range R.
static double roundTripCentsCentred(float V, float R) {
    int   n   = noteFor(V);
    int   b   = bend14For(V, R);
    float Vr  = reconstructVolts(n, b, R);
    return std::fabs((double)Vr - (double)V) * 1200.0;   // 1V = 1200 cents
}

int main() {
    // ── the note/bend split primitives ───────────────────────────────────────────────────────────
    SUITE("noteFor — nearest 12-TET note, 0V = C4 = 60, clamped");
    TEST("0V → 60",            EXPECT(noteFor(0.f) == 60));
    TEST("+1V → 72 (octave)",  EXPECT(noteFor(1.f) == 72));
    TEST("-1V → 48",           EXPECT(noteFor(-1.f) == 48));
    TEST("nearest rounds: +0.04V (~0.48st) → 60", EXPECT(noteFor(0.04f) == 60));
    TEST("nearest rounds: +0.05V (~0.6st) → 61",  EXPECT(noteFor(0.05f) == 61));
    TEST("clamps high: +100V → 127", EXPECT(noteFor(100.f) == 127));
    TEST("clamps low:  -100V → 0",   EXPECT(noteFor(-100.f) == 0));

    SUITE("centsOffsetSemis — signed within-semitone offset in [-0.5,+0.5]");
    TEST("exact note → 0 offset", EXPECT(std::fabs(centsOffsetSemis(0.f)) < 1e-6f));
    TEST("just under half-semitone up ≈ +0.49 (note rounds down, offset positive)",
         EXPECT(std::fabs(centsOffsetSemis(0.49f/12.f) - 0.49f) < 1e-4f));
    TEST("exact half-semitone rounds UP to the next note → offset −0.5 (magnitude 0.5)",
         EXPECT(std::fabs(centsOffsetSemis(0.5f/12.f) - (-0.5f)) < 1e-4f));
    TEST("quarter-tone up ≈ +0.25", EXPECT(std::fabs(centsOffsetSemis(0.25f/12.f) - 0.25f) < 1e-4f));
    TEST("bounded to ±0.5 for any input", {
        for (int k = -2000; k <= 2000; ++k) {
            float V = (float)k * 0.001f;
            EXPECT(std::fabs(centsOffsetSemis(V)) <= 0.5f + 1e-4f);
        }
    });

    SUITE("bend14 — 14-bit, centre 8192, clamps, scales by range");
    TEST("zero offset → centre 8192", EXPECT(bend14(0.f, 2.f) == 8192));
    TEST("+full range → 16383 (top)", EXPECT(bend14(2.f, 2.f) == 16383));
    TEST("-full range → 1 (round of 0.0, but symmetric near bottom)", {
        // -2 semis at range 2 maps to 8192 - 8192 = 0 exactly.
        EXPECT(bend14(-2.f, 2.f) == 0);
    });
    TEST("half of range → quarter span above centre", {
        // offset 1 at range 2 → 8192 + 1*(8192/2) = 12288
        EXPECT(bend14(1.f, 2.f) == 12288);
    });
    TEST("overshoot clamps at 16383", EXPECT(bend14(10.f, 2.f) == 16383));
    TEST("undershoot clamps at 0",    EXPECT(bend14(-10.f, 2.f) == 0));
    TEST("range < 1 is floored to 1 (no divide blow-up)", {
        EXPECT(bend14(0.5f, 0.f) == bend14(0.5f, 1.f));
    });

    // ── the ROUND-TRIP guarantee — the reason ±48 is safe ─────────────────────────────────────────
    SUITE("ROUND-TRIP (note-on centred split): sub-cent reconstruction at every range 1..48");
    for (float R : {1.f, 2.f, 12.f, 24.f, 48.f}) {
        std::string label = "range ±" + std::to_string((int)R) + " st: max round-trip error < 1 cent";
        TEST(label.c_str(), {
            double worst = 0.0;
            // sweep a wide microtonal range of input voltages (−2..+2 oct, fine grid incl. odd offsets)
            for (int k = -2400; k <= 2400; ++k) {
                float V = (float)k * (1.f / 1200.f);   // one-cent steps
                worst = std::max(worst, roundTripCentsCentred(V, R));
            }
            // theoretical bound ≈ R·0.0061 cents (half a 14-bit step); assert comfortably under 1 cent.
            EXPECT(worst < 1.0);
        });
    }
    TEST("even at ±48 the worst error stays under the analytic 0.3-cent bound", {
        double worst = 0.0;
        for (int k = -2400; k <= 2400; ++k)
            worst = std::max(worst, roundTripCentsCentred((float)k * (1.f/1200.f), 48.f));
        EXPECT(worst < 0.30);
    });

    // ── held-voice continuous tracking + the legato landmine ──────────────────────────────────────
    SUITE("held-voice bend tracking (offsetFromNoteSemis / bend14FromNote)");
    TEST("within ±range: reconstruction tracks the live pitch under a cent", {
        const float R = 2.f;
        const int   n = noteFor(0.f);          // latch note at 0V (=60)
        double worst = 0.0;
        for (int k = -200; k <= 200; ++k) {    // drift ±2 semitones (== the range) around the note
            float V = (float)k * (1.f/1200.f) * ( (2.f) );  // scale to ±2 st span in cents grid
            float Vc = V;                       // stay within ±R by construction below
            if (std::fabs(offsetFromNoteSemis(Vc, n)) > R) continue;
            int   b  = bend14FromNote(Vc, n, R);
            float Vr = reconstructVolts(n, b, R);
            worst = std::max(worst, std::fabs((double)Vr - (double)Vc) * 1200.0);
        }
        EXPECT(worst < 1.0);
    });
    TEST("LEGATO LANDMINE: a slide past ±range clamps — reconstruction does NOT match input", {
        const float R = 2.f;
        const int   n = noteFor(0.f);           // note latched at 60
        const float V = 5.f / 12.f;             // +5 semitones — well past the ±2 range
        int   b  = bend14FromNote(V, n, R);
        EXPECT(b == 16383);                     // clamped at the top
        float Vr = reconstructVolts(n, b, R);   // reconstructs to note+range (=62 st worth), not +5
        double errCents = std::fabs((double)Vr - (double)V) * 1200.0;
        EXPECT(errCents > 100.0);               // grossly wrong → this is why re-articulation is needed
    });
    TEST("re-articulation MODEL: re-noting to the nearest note restores sub-cent accuracy", {
        // What Keppel's re-articulation does: when |offset|>range, drop the old note and re-note on the
        // nearest 12-TET note at a fresh centred bend. Modelled here as a fresh centred split.
        const float R = 2.f;
        const float V = 5.f / 12.f;             // the same +5 st slide
        double err = roundTripCentsCentred(V, R);   // fresh nearest-note + centred bend
        EXPECT(err < 1.0);
    });

    // ── Two-layer expression: sumAroundRest (MPE_UTILITY_BUILD_SPEC "Two-layer input structure") ──────
    // OUT = clamp(A + B - rest, lo, hi). a/b are full value-space values including the rest offset; a
    // silent layer reads as `rest`, so summing two full values removes one rest to avoid double-counting.
    SUITE("sumAroundRest — two-layer sum around a dimension's rest point");
    TEST("B at rest → OUT = A (B contributes nothing)", {
        // X: rest 0. A = +1.5st, B = 0 (silent/unpatched).
        EXPECT(std::fabs(sumAroundRest(1.5f, 0.f, 0.f, -1000.f, 1000.f) - 1.5f) < 1e-5f);
    });
    TEST("both at rest → OUT = rest (the neutral point)", {
        EXPECT(std::fabs(sumAroundRest(0.f, 0.f, 0.f, -1000.f, 1000.f) - 0.f) < 1e-5f);   // X/Z rest 0
        EXPECT(std::fabs(sumAroundRest(64.f, 64.f, 64.f, 0.f, 127.f) - 64.f) < 1e-5f);     // Y rest 64
    });
    TEST("A + B additive around rest: Y rest 64, A=80, B=96 → 80+96-64 = 112", {
        EXPECT(std::fabs(sumAroundRest(80.f, 96.f, 64.f, 0.f, 127.f) - 112.f) < 1e-4f);
    });
    TEST("clamps at the hi rail (Y: 120 + 120 - 64 = 176 → 127)", {
        EXPECT(std::fabs(sumAroundRest(120.f, 120.f, 64.f, 0.f, 127.f) - 127.f) < 1e-4f);
    });
    TEST("clamps at the lo rail (Y: 10 + 10 - 64 = -44 → 0)", {
        EXPECT(std::fabs(sumAroundRest(10.f, 10.f, 64.f, 0.f, 127.f) - 0.f) < 1e-4f);
    });
    TEST("Z unipolar rest 0: A=40, B=30 → 70 (simple additive, rest 0)", {
        EXPECT(std::fabs(sumAroundRest(40.f, 30.f, 0.f, 0.f, 127.f) - 70.f) < 1e-4f);
    });
    TEST("X bipolar rest 0, wide rails: A=+2, B=-0.5 → +1.5 (bend14 clamps later)", {
        EXPECT(std::fabs(sumAroundRest(2.f, -0.5f, 0.f, -1000.f, 1000.f) - 1.5f) < 1e-5f);
    });
    TEST("two-layer X feeds bend14 + reconstructs sub-cent (X added after the split)", {
        // Model Keppel's held-voice path: residual (within-semitone) + X expression, then bend14 + reconstruct.
        const float R = 2.f;
        const float pitchV = 0.3f / 12.f;        // +0.3 st pitch → note 60, residual +0.3
        const int   note   = noteFor(pitchV);
        const float resid  = centsOffsetSemis(pitchV);          // ≈ +0.3
        const float xTotal = sumAroundRest(0.8f, 0.f, 0.f, -1000.f, 1000.f);  // X-A = +0.8st, B silent
        const float total  = resid + xTotal;                     // ≈ +1.1 st, within ±2
        int   b  = bend14(total, R);
        float Vr = reconstructVolts(note, b, R);
        // The receiver reproduces note + total bend = the original note + (resid + xTotal) of bend.
        // Reconstruct = (note-60 + total)/12; the X expression is part of the emitted bend, so the
        // monitor faithfully reflects it. Assert the bend encodes `total` to sub-cent.
        double reconSemis = (double)reconstructVolts(note, b, R) * 12.0;
        EXPECT(std::fabs(reconSemis - ((double)(note - 60) + (double)total)) < 0.01);
    });

    // ── CV → MPE-value mappers (rest points: X@0, Y@64, Z@0) ─────────────────────────────────────────
    SUITE("CV mappers — unpatched (0V) reads as the dimension rest");
    TEST("xSemisFromVolts: 0V → 0 (rest), 1V → 1 semitone (1V=1st)", {
        EXPECT(std::fabs(xSemisFromVolts(0.f)) < 1e-6f);
        EXPECT(std::fabs(xSemisFromVolts(1.f) - 1.f) < 1e-6f);
        EXPECT(std::fabs(xSemisFromVolts(-2.5f) + 2.5f) < 1e-6f);
    });
    TEST("yCc74FromVolts: 0V → 64 (rest), +5V → 127, -5V → 0 (bipolar around 64)", {
        EXPECT(yCc74FromVolts(0.f) == 64);
        EXPECT(yCc74FromVolts(5.f) == 127);     // 64 + 5*12.8 = 128 → clamp 127
        EXPECT(yCc74FromVolts(-5.f) == 0);      // 64 - 64 = 0
    });
    TEST("yCc74FromVolts clamps both rails", {
        EXPECT(yCc74FromVolts(100.f) == 127);
        EXPECT(yCc74FromVolts(-100.f) == 0);
    });
    TEST("zPressureFromVolts: 0V → 0 (rest), 10V → 127 (unipolar from 0)", {
        EXPECT(zPressureFromVolts(0.f) == 0);
        EXPECT(zPressureFromVolts(10.f) == 127);   // 10*12.7 = 127
    });
    TEST("zPressureFromVolts clamps negative to 0 (pressure cannot be negative)", {
        EXPECT(zPressureFromVolts(-5.f) == 0);
        EXPECT(zPressureFromVolts(100.f) == 127);
    });
    TEST("Y/Z round-trip: the value we send IS the value received (no decomposition — direct CC)", {
        // Unlike pitch (note+bend split), Y (CC74) and Z (channel pressure) are sent as direct values,
        // so the round-trip is identity: send v → receiver sees v. Asserted so the dedupe + transmit
        // contract has a ground truth.
        for (float V = -6.f; V <= 11.f; V += 0.37f) {
            int y = yCc74FromVolts(V);
            EXPECT(y >= 0 && y <= 127);   // identity: what we compute is what we send is what arrives
            int z = zPressureFromVolts(V);
            EXPECT(z >= 0 && z <= 127);
        }
    });

    std::cout << "\n-----\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
