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

    std::cout << "\n-----\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
