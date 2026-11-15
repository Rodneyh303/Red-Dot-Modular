// test/test_variation_centre_hold.cpp — CC_PROMPT_VARIATION_CENTRE: knob 0.5 ==
// repeat-in-place (no forward creep).
//
// THE BUG (verified in code): at the neutral centre the gate-mode HOLD is correct, but
// every held rise calls scanHold_() (the freeze fix) which grows scanOfsV_[0]
// unconditionally — and with |p| == 0 the ESCAPE PROBABILITY is zero (the threshold is
// 0, so no scanned cell is ever an escape and the hunt can never terminate — horizon
// 1/escape-prob = inf), so the VARIATION lane / chevron creeps forward indefinitely at
// the centre (the arbitrary-forward Rodney observed).
//
// THE FIX: the scan is gated on |p| > eps (eps = 1e-3f, the choice stated in the
// commit): knob 0.5 maps to exactly 0 (the clean case), CV2/Junction noise up to
// ~10mV (1e-3 in p units) holds cleanly instead of creeping over a ~1000-rise
// horizon, and an intentional small p (1% knob = 0.01, escape ~100 rises) still
// scans. p > 0 and p < 0 are EXACTLY as today (the freeze-fix signed scan); only the
// centre changes, from forward-creep to clean repeat. Variation stays COUPLED to
// direction (the sign of p drives it) — this is purely the centre.
//
// Reproduce-first: GATE 1's scan/delta checks FAIL on the pre-fix engine (the scan
// creeps); the p != 0 gates pin the unchanged freeze-fix law.
#include <cstdio>
#include "engines/SequencerEngine.hpp"
#include "VoiceResolver.hpp"

using dotModular::VoiceResolver;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

static PatternInput makeInput() {
    PatternInput in;
    for (int i = 0; i < 12; ++i) in.semiWeights[i] = 1.f;
    in.noteVariationMask = 0b111;
    in.variationAmount   = 0.5f;
    in.octaveLo = 0; in.octaveHi = 0;
    return in;
}

// One Gate-1 RISE (a fall before it so every call is a true rise event). p lands on
// the mono slot exactly as the mode controller writes it per block.
static StepResult rise(SequencerEngine& eng, float p) {
    PatternInput in = makeInput();
    auto& mono = eng.voices[VoiceResolver::mono()];
    mono.restProb = 0.f; mono.legatoProb = 0.f; mono.accentProb = 0.25f; mono.qmixLevel = 0.f;
    mono.variationProb = p;
    eng.executeModeB(/*gate1Rise=*/false, /*gate1High=*/false, 2.f, in);   // the fall
    return eng.executeModeB(/*gate1Rise=*/true,  /*gate1High=*/true,  2.f, in);
}

int main() {
    const auto M = VoiceResolver::mono();

    // ── GATE 1: p == 0 (knob 0.5) — repeat-in-place, NO creep ── (FAILS pre-fix)
    {
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        rise(eng, 0.f);                                  // the priming rise: the transport starts
        const int start = eng.playedColumn();
        bool holdOK = true, scanOK = true, deltaOK = true;
        for (int k = 0; k < 48; ++k) {
            rise(eng, 0.f);
            holdOK  = holdOK  && eng.playedColumn() == start;                    // every decision HOLD
            scanOK  = scanOK  && eng.scanOfsV_[0] == 0;                     // NO creep
            deltaOK = deltaOK && eng.stepAdvanceScanDelta(M) == 0;          // the chevron at rest
        }
        check(holdOK,  "p==0: every rise HOLDS (the playhead repeats in place)");
        check(scanOK,  "p==0: scanOfsV_[0] stays 0 (no scan accumulation at the centre)");
        check(deltaOK, "p==0: stepAdvanceScanDelta == 0 (the chevron sits still)");
    }

    // ── GATE 2: p > 0 — the held-rise scan still accumulates (the freeze fix, unchanged) ──
    {
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        rise(eng, 0.5f);                                 // the priming rise
        bool lawOK = true, signOK = true;
        int seenHolds = 0;
        for (int k = 0; k < 64; ++k) {
            const int scanBefore = eng.scanOfsV_[0];
            const int colBefore  = eng.playedColumn();
            rise(eng, 0.5f);
            if (eng.playedColumn() == colBefore) {            // a HOLD: the scan must have grown +1 (mod varLen)
                ++seenHolds;
                const int varLen = eng.strandLen(dotModular::STRAND_VARIATION);
                lawOK  = lawOK  && eng.scanOfsV_[0] == (scanBefore + 1) % (varLen > 0 ? varLen : 1);
                signOK = signOK && eng.stepAdvanceScanDelta(M) == eng.scanOfsV_[0];   // forward-signed
            } else {
                lawOK = lawOK && eng.scanOfsV_[0] == 0;  // any motion re-anchors the scan
            }
        }
        check(seenHolds >= 1, "p>0: held rises occur in the run (the pin is not vacuous)");
        check(lawOK,  "p>0: every held rise grows the scan +1 (mod varLen); motion re-anchors (the freeze fix, unchanged)");
        check(signOK, "p>0: stepAdvanceScanDelta == +scanOfsV_[0] (the forward-signed display)");
    }

    // ── GATE 3: p < 0 — the signed backward scan (unchanged) ──
    {
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        rise(eng, -0.5f);                                // the priming rise
        bool lawOK = true, signOK = true;
        int seenHolds = 0;
        for (int k = 0; k < 64; ++k) {
            const int scanBefore = eng.scanOfsV_[0];
            const int colBefore  = eng.playedColumn();
            rise(eng, -0.5f);
            if (eng.playedColumn() == colBefore) {
                ++seenHolds;
                const int varLen = eng.strandLen(dotModular::STRAND_VARIATION);
                lawOK  = lawOK  && eng.scanOfsV_[0] == (scanBefore + 1) % (varLen > 0 ? varLen : 1);
                signOK = signOK && eng.stepAdvanceScanDelta(M) == -eng.scanOfsV_[0];  // backward-signed
            } else {
                lawOK = lawOK && eng.scanOfsV_[0] == 0;
            }
        }
        check(seenHolds >= 1, "p<0: held rises occur in the run (the pin is not vacuous)");
        check(lawOK,  "p<0: every held rise grows the scan +1 (mod varLen); motion re-anchors (unchanged)");
        check(signOK, "p<0: stepAdvanceScanDelta == -scanOfsV_[0] (the backward-signed display)");
    }

    // ── GATE 4: the eps boundary — just below holds cleanly; a clear intentional p scans ──
    {
        SequencerEngine eng; eng.reset();
        eng.numPolyVoices = 0;
        rise(eng, 5e-4f);                                // the priming rise
        bool ok = true;
        for (int k = 0; k < 24; ++k) { rise(eng, 5e-4f); ok = ok && eng.scanOfsV_[0] == 0; }
        check(ok, "eps: |p| = 5e-4 (below eps = 1e-3) holds cleanly — no creep from CV noise");

        SequencerEngine eng2; eng2.reset();
        eng2.numPolyVoices = 0;
        rise(eng2, 0.01f);                               // the priming rise
        bool sawHold = false, grewOnHold = true;
        const int varLen2 = eng2.strandLen(dotModular::STRAND_VARIATION);
        for (int k = 0; k < 96; ++k) {
            const int col = eng2.playedColumn();
            const int scanBefore = eng2.scanOfsV_[0];
            rise(eng2, 0.01f);                           // an intentional small p (1% knob)
            if (eng2.playedColumn() == col) {
                sawHold = true;
                grewOnHold = grewOnHold
                    && eng2.scanOfsV_[0] == (scanBefore + 1) % (varLen2 > 0 ? varLen2 : 1);
            }
        }
        check(sawHold && grewOnHold, "eps: an intentional p = 0.01 still scans on holds (the +1-mod growth law)");
    }

    // ── GATE 5: the POLY twin — a per-voice centre must not creep ──
    // The reported regression: CC gated mono's scanHold_ but NOT the per-voice scanHoldV_,
    // so a poly voice with its OWN variationProb at the centre (poly knob 0.5 → p_v == 0)
    // still crept. The twins are now ONE scanHoldFor_ (slice X, the ODR consolidation) —
    // it self-gates on the VOICE's own p with the varLen selected BY BANK (mono's strand
    // bound, the voice's own polyLOR bound). Reproduce-first: on the pre-fix scanHoldV_
    // (no gate) the p==0 call accumulates; the gate holds it at 0.
    {
        SequencerEngine eng; eng.reset();
        const auto V = VoiceResolver::slot(3);           // a poly voice (bank.v == 2)

        eng.voices[V].variationProb = 0.f;               // p_v == 0 (poly knob 0.5)
        for (int k = 0; k < 48; ++k) eng.scanHoldFor_(V);
        check(eng.scanOfsV_[V.v] == 0,
              "poly p==0: scanHoldFor_ does not accumulate (the voice repeats in place, no creep)");

        eng.voices[V].variationProb = 0.5f;              // a clear intentional p
        const int before = eng.scanOfsV_[V.v];
        eng.scanHoldFor_(V);
        check(eng.scanOfsV_[V.v] != before,
              "poly p>0: scanHoldFor_ still accumulates (the per-voice freeze-fix scan, unchanged)");

        eng.voices[V].variationProb = -5e-4f;            // below eps: holds cleanly (CV noise)
        const int scanAtEps = eng.scanOfsV_[V.v];
        for (int k = 0; k < 24; ++k) eng.scanHoldFor_(V);
        check(eng.scanOfsV_[V.v] == scanAtEps,
              "poly |p|<eps: scanHoldFor_ holds cleanly (no creep from CV noise, matching mono's gate)");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
