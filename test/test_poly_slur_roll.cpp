// EAST_EXTRA_LANES Stage 3 — Rule 2 per-voice leading-edge slur ROLL (step 2).
//
// Pins the design claim the roll rests on (EAST_EXTRA_LANES.md §4d): the slur
// commitment is mono's LEAD formula run PER VOICE, and the ONLY per-voice input
// is the reading CELL, selected by delegation. Everything else — the legatoRandom
// array and the legatoProb threshold — stays global/mono, exactly like VARIATION.
//
// Consequences proved here (the real engine's roll at SequencerEngine.cpp mirrors
// this formula line-for-line; this validates the DESIGN, the engine binding is a
// local-build concern):
//   1. A fully-delegated voice (LEG+VAR follow mono) commits the SAME slur as mono
//      at every step — the follow-mono guarantee.
//   2. A LEG-Local-East voice reading a different cell can diverge from mono.
//   3. A resting voice commits NO forward slur.
//   4. A non-leading note (fractional length) can never lead a slur, cell regardless.
//   5. LegatoMax (prob ≥ 0.999) forces the slur for a playing, leading voice.
//
// Build: g++ -std=c++17 -I src/dsp test/test_poly_slur_roll.cpp
#include "NoteValues.hpp"
#include <cstdio>
#include <cmath>
#include <string>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

// Mono's LEAD formula (SequencerEngine::executeStep) and the per-voice roll
// (executePolyVoice) are the SAME expression — copied here from source truth.
// leStarting: the onset actually starts a note this edge. r is the legatoRandom
// value at the reading cell. prob is mono's legatoProb (global threshold).
static bool rollSlur(bool leStarting, int nvIdx, float prob, float r) {
    return leStarting
        && noteCanLeadLegato(nvIdx)
        && (prob >= 0.999f || r < prob);
}

// getLegatoStepForVoice model: delegated (default) → mono's legato cell; Local
// East → the voice's own cell. (Real engine: delegated returns getLegatoStep().)
static int legCell(bool localEast, int monoCell, int ownCell) {
    return localEast ? ownCell : monoCell;
}

int main() {
    std::printf("\033[1mRule 2 per-voice slur roll\033[0m\n");
    std::printf("%s\n", std::string(46, '=').c_str());

    // A shared mono legatoRandom shape (the array is global; only the cell differs).
    float legatoRandom[16];
    for (int i = 0; i < 16; ++i) legatoRandom[i] = (float)((i * 5 + 2) % 16) / 16.f;

    const float prob = 0.5f;   // mono legatoProb (global threshold)
    const int   monoNv = 4;    // 1/8 = 2 steps → whole → CAN lead

    // ── 1. Fully-delegated voice commits the SAME slur as mono, every step ──
    // Delegated ⇒ voice reads mono's legato cell AND mono's variation (so same nvIdx):
    // all three roll inputs are mono's, so the slur is bit-identical.
    {
        int mism = 0;
        for (int step = 0; step < 96; ++step) {
            int monoCell = (step * 7 + 1) & 0x0F;   // whatever mono's LEG window yields
            float rMono  = legatoRandom[monoCell];
            bool monoSlur = rollSlur(true, monoNv, prob, rMono);
            // delegated: cell = monoCell, nv = monoNv (VAR delegated too)
            float rVoice = legatoRandom[legCell(/*localEast=*/false, monoCell, /*own=*/(step*3)&0x0F)];
            bool voiceSlur = rollSlur(true, monoNv, prob, rVoice);
            if (voiceSlur != monoSlur) ++mism;
        }
        check(mism == 0, "delegated voice slur == mono slur at every step");
    }

    // ── 2. LEG-Local-East voice can diverge from mono ──
    // Choose two cells straddling the threshold: mono's r < prob (slur), own r ≥ prob (no slur).
    {
        int monoCell = -1, ownCell = -1;
        for (int i = 0; i < 16; ++i) if (legatoRandom[i] < prob) { monoCell = i; break; }
        for (int i = 0; i < 16; ++i) if (legatoRandom[i] >= prob) { ownCell = i; break; }
        check(monoCell >= 0 && ownCell >= 0, "found straddling cells for divergence");
        bool monoSlur  = rollSlur(true, monoNv, prob, legatoRandom[legCell(false, monoCell, ownCell)]);
        bool localSlur = rollSlur(true, monoNv, prob, legatoRandom[legCell(true,  monoCell, ownCell)]);
        check(monoSlur == true,  "mono (r<prob) leads a slur");
        check(localSlur == false, "Local East voice (own r>=prob) does NOT — diverges from mono");
    }

    // ── 3. A resting voice commits no forward slur ──
    // In the engine a resting voice sets slurForward=false directly (never reaches the
    // roll). Model: leStarting=false for a rester → no slur, any cell/prob.
    {
        bool restSlur = rollSlur(/*leStarting=*/false, monoNv, /*prob=*/1.0f, /*r=*/0.f);
        check(restSlur == false, "resting voice commits no slur");
    }

    // ── 4. A non-leading (fractional) note can never lead a slur ──
    // 1/4T (idx 3) and 1/32 (idx 7) end off-grid → noteCanLeadLegato false.
    {
        check(!noteCanLeadLegato(3) && !noteCanLeadLegato(7), "fractional notes cannot lead (table)");
        bool s = rollSlur(true, /*nvIdx=*/3, /*prob=*/1.0f, /*r=*/0.f);   // even with forced prob
        check(s == false, "non-leading note commits no slur even at prob=1");
    }

    // ── 5. LegatoMax forces the slur for a playing, leading voice ──
    {
        bool s = rollSlur(true, monoNv, /*prob=*/1.0f, /*r=*/0.99f);   // r high, but LegatoMax
        check(s == true, "LegatoMax (prob>=0.999) forces slur regardless of r");
        // …but still respects noteCanLeadLegato:
        bool sFrac = rollSlur(true, /*1/4T*/3, 1.0f, 0.f);
        check(sFrac == false, "LegatoMax still cannot force a fractional note to lead");
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
