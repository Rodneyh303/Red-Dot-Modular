#pragma once
// src/ui/SweepSigGate.hpp — the Macro prob-out sweep's input-change fingerprint gate.
//
// RESTORED by CC_PROMPT_restore_sweepsig_gate_START (Rodney's bisect: 70dd837 deleted
// this gate for a step-cache keyed on the step NUMBER, and Macro-only CPU went 0.4% →
// ~3%; a /32 throttle ALONE was already tried and reverted — e0981d5 — because
// throttling the frequency still resolves UNCONDITIONALLY every tick ≈ 22.7 µs/tick
// ≈ 0.7 µs/sample ≈ 3.4%). The SKIP-ON-UNCHANGED fingerprint is the mechanism that gave
// the 0.4%: the resolve is a PURE FUNCTION of its inputs, so when they are bit-unchanged
// since the last run, the recomputed outs are bit-identical and the whole resolve is
// skipped — the cached rows (and the ports through the write-elimination walk) hold the
// exact bits the last run wrote. Steady state: stopped+idle ≈ 0 resolves; playing ≈
// step-rate (8-16 Hz × 22.7 µs ≈ 0.02%).
//
// The gate lives in THIS header (not the widget .cpp, where 70dd837 found it) so the
// unit rig drives the REAL decision logic — test_sweepsig_gate.cpp — the claims-vs-code
// discipline: never a reimplementation beside the shipped one.
//
// THE FINGERPRINT = EXACTLY the CURRENT resolve's reads. The sweep resolves through
// dotModular::VoiceResolver::laneProbabilityAtStep(v, l, step) → SequencerEngine::
// effectiveProbability(voice, lane, step, ProbLens::MacroOwn), whose MacroOwn-lens
// reads are, exhaustively:
//   • eng.macroOwnSpread[l]           — the manager-published store (publishGlobal;
//                                       the gate compares the LIVE store, not a shadow,
//                                       so ANY external writer breaks the compare and
//                                       the next tick re-runs — every-tick healing).
//   • pe.pubSlewedPoly{lane}[v][step] — the published base cells, voiceSlots v = 0..nV
//                                       (polySlewedPub), INCLUDING row 0 (the anchor
//                                       target applyAnchorV1Only reads via
//                                       monoSlewedPub — same cell family, slot 0).
//   • the step itself                 — the masked playhead indexes the cells.
//   • pe.warpBank_ +, through PatternEngine::warpDisplayMixMacroOwn (delegated=TRUE →
//     WarpBank::warpedValue → resolveWarpCell's PURE SELECTION — pinned by
//     test_warp_f_bank S4c and defended by test_sweepsig_gate's own-slot perturbation
//     case), ONLY Macro's GLOBAL cells: globalWarpW/F/Tilt/AtomLevel/AtomMass[lane].
//   • nV = eng.numPolyVoices          — 1+nV channels are resolved.
//
// DELIBERATELY NOT FINGERPRINTED (the adaptation — 70dd837's pre-deletion gate carried
// these, the OLD resolve read them; the CURRENT MacroOwn lens does NOT):
//   • the bank's own-slot per-(voice,lane) stores and the warpDelegated_ matrix —
//     East-authorable, read by the OWNER lens, but copied-then-DISCARDED on this path
//     (pure selection with delegated hardcoded TRUE — "Macro-own ALL THE WAY DOWN",
//     the FINISH2 design). Fingerprinting an input the resolve cannot read only re-runs
//     the resolve spuriously (the CPU cost) for zero correctness.
//   • the output scale + the S&H latch state — the scale is the out-write epoch's job
//     (StepProbRow::outScale/outCh re-walk the SAME cached values at the new transform,
//     no resolve needed), and probOutSampleHold is inert on this module's outs.
// The DrawnWarp pool is append-only (putCurve appends; every append accompanies the
// globalWarpF rewrite that points a slot at it), so the variant bits + poolUsed cover
// it. WarpF is trivially copyable, so the byte compare is conservative-complete: it can
// only re-run spuriously (padding / +0-vs−0), never miss a change.
#include <cstring>
#include "../dsp/engines/SequencerEngine.hpp"   // macroOwnSpread + .pe (PatternEngine)
#include "../dsp/WarpF.hpp"                     // WarpBank's GLOBAL cells + WarpF

// Lane → that lane's pubSlewed twin, flattened ([voiceSlot*16 + step]). The same
// lane dispatch the published-snapshot accessor switches on, hoisted so the gate
// can walk the exact cells the resolve reads without a switch per cell.
// (Restored verbatim from the pre-70dd837 StraitsSandsMacroVisual.cpp.)
inline const float* pubSlewedLane_(const PatternEngine& pe, int lane) {
    switch (lane) {
        case PatternEngine::PL_MELODY:    return &pe.pubSlewedPolyMelody[0][0];
        case PatternEngine::PL_OCTAVE:    return &pe.pubSlewedPolyOctave[0][0];
        case PatternEngine::PL_QMIX:      return &pe.pubSlewedPolyQmix[0][0];
        case PatternEngine::PL_REST:      return &pe.pubSlewedPolyRhythm[0][0];
        case PatternEngine::PL_ACCENT:    return &pe.pubSlewedPolyAccent[0][0];
        case PatternEngine::PL_VARIATION: return &pe.pubSlewedPolyVariation[0][0];
        case PatternEngine::PL_LEGATO:    return &pe.pubSlewedPolyLegato[0][0];
        default:                          return &pe.pubSlewedPolyRhythm[0][0];   // invalid lane → rhythm fallback (polySlewedPub parity)
    }
}

// The shadow signature — every input the resolve's output depends on (see the header
// comment for the exhaustive read enumeration + the deliberate drops). Field shapes
// restored from the pre-70dd837 struct; the 7/16 literals are that struct's own —
// tied to the engine's lane count below.
struct SweepSig {
    bool valid = false;
    int  nV = -1;                          // eng.numPolyVoices — 1+nV channels resolved
    int  step[7] = {};                     // per-lane masked playheads (the resolve's index)
    float pub[7][16] = {};                 // pubSlewed cells the resolve reads: [lane][voiceSlot 0..nV]
    float macroOwnSpread[7] = {};          // eng.macroOwnSpread — the MacroOwn lens's amount (manager-published)
    const void* bank = nullptr;            // pe.warpBank_ (null ⇒ warpDisplayMixMacroOwn is passthrough)
    float globalWarpW[7] = {};             // Macro's delegated-default depth   ── the ONLY warp
    dotModular::WarpF globalWarpF[7] = {}; // Macro's delegated-default curve   │  cells the MacroOwn
    float globalWarpTilt[7] = {};          // Macro's delegated-default tilt    │  lens can read
    float globalWarpAtomLevel[7] = {};     // Macro's delegated-default atom loc│  (pure selection,
    float globalWarpAtomMass[7] = {};      // Macro's delegated-default atom mass┘  delegated=TRUE)
    uint16_t poolUsed = 0;                 // drawn-curve pool high-water mark (append-only pool)
};
static_assert(PatternEngine::PL_LANES == 7,
              "SweepSig's literal 7s are the pre-70dd837 struct's — the engine lane count moved");

// The gate decision, against the LIVE stores (the same expressions the resolve is
// about to consume): true ⇒ the resolve MUST re-run; false ⇒ the cached rows already
// hold the exact bits a re-run would produce (pure function, bit-unchanged inputs).
inline bool sweepSigChanged(const SweepSig& sig, const SequencerEngine& eng,
                            const int stepArr[7], int nV) {
    if (!sig.valid) return true;
    if (nV != sig.nV) return true;
    if (0 != std::memcmp(stepArr, sig.step, sizeof sig.step)) return true;
    if (0 != std::memcmp(eng.macroOwnSpread, sig.macroOwnSpread, sizeof sig.macroOwnSpread)) return true;
    const PatternEngine& pe = eng.pe;
    for (int l = 0; l < 7; ++l) {
        const float* p = pubSlewedLane_(pe, l);   // flat [v*16 + step]
        for (int v = 0; v <= nV; ++v)
            if (0 != std::memcmp(&p[v * 16 + stepArr[l]], &sig.pub[l][v], sizeof(float)))
                return true;
    }
    if ((const void*)pe.warpBank_ != sig.bank) return true;
    if (pe.warpBank_) {
        const dotModular::WarpBank& b = *pe.warpBank_;
        if (0 != std::memcmp(b.globalWarpW, sig.globalWarpW, sizeof b.globalWarpW)) return true;
        if (0 != std::memcmp(b.globalWarpF, sig.globalWarpF, sizeof b.globalWarpF)) return true;
        if (0 != std::memcmp(b.globalWarpTilt, sig.globalWarpTilt, sizeof b.globalWarpTilt)) return true;
        if (0 != std::memcmp(b.globalWarpAtomLevel, sig.globalWarpAtomLevel, sizeof b.globalWarpAtomLevel)) return true;
        if (0 != std::memcmp(b.globalWarpAtomMass, sig.globalWarpAtomMass, sizeof b.globalWarpAtomMass)) return true;
        if (b.poolUsed != sig.poolUsed) return true;
    }
    return false;
}

// Capture the signature exactly as the resolve is about to consume it (call BEFORE the
// resolve — it is read-only on every fingerprinted input, so the placement is free).
inline void sweepSigCapture(SweepSig& sig, const SequencerEngine& eng,
                            const int stepArr[7], int nV) {
    sig.valid = true;
    sig.nV = nV;
    std::memcpy(sig.step, stepArr, sizeof sig.step);
    std::memcpy(sig.macroOwnSpread, eng.macroOwnSpread, sizeof sig.macroOwnSpread);
    const PatternEngine& pe = eng.pe;
    for (int l = 0; l < 7; ++l) {
        const float* p = pubSlewedLane_(pe, l);
        for (int v = 0; v <= nV; ++v)
            sig.pub[l][v] = p[v * 16 + stepArr[l]];
    }
    sig.bank = pe.warpBank_;
    if (pe.warpBank_) {
        const dotModular::WarpBank& b = *pe.warpBank_;
        std::memcpy(sig.globalWarpW, b.globalWarpW, sizeof sig.globalWarpW);
        std::memcpy(sig.globalWarpF, b.globalWarpF, sizeof sig.globalWarpF);
        std::memcpy(sig.globalWarpTilt, b.globalWarpTilt, sizeof sig.globalWarpTilt);
        std::memcpy(sig.globalWarpAtomLevel, b.globalWarpAtomLevel, sizeof sig.globalWarpAtomLevel);
        std::memcpy(sig.globalWarpAtomMass, b.globalWarpAtomMass, sizeof sig.globalWarpAtomMass);
        sig.poolUsed = b.poolUsed;
    }
}

#ifdef MONSOON_PROFILE
// IDLE DIAGNOSTIC (instrumented builds only — never ships): the first-match twin of
// sweepSigChanged, returning WHICH field fired so the 50k-sample log can name the
// input that moved if idle ever shows resolves (CC_PROMPT_restore_sweepsig_gate_START
// acceptance: idle ⇒ ≈0). Order mirrors sweepSigChanged.
enum SweepSigWhy {
    kWhyInit = 0,   // first run (or classification fallback)
    kWhyNV,         // eng.numPolyVoices changed
    kWhyStep,       // a lane's masked playhead changed
    kWhySpread,     // eng.macroOwnSpread (the store) changed
    kWhyPub,        // a pubSlewed cell at the current step changed
    kWhyBank,       // pe.warpBank_ pointer changed
    kWhyWarp,       // a global warp cell / poolUsed changed
    kWhyCount
};
inline int sweepSigWhy(const SweepSig& sig, const SequencerEngine& eng,
                       const int stepArr[7], int nV) {
    if (!sig.valid) return kWhyInit;
    if (nV != sig.nV) return kWhyNV;
    if (0 != std::memcmp(stepArr, sig.step, sizeof sig.step)) return kWhyStep;
    if (0 != std::memcmp(eng.macroOwnSpread, sig.macroOwnSpread, sizeof sig.macroOwnSpread)) return kWhySpread;
    const PatternEngine& pe = eng.pe;
    for (int l = 0; l < 7; ++l) {
        const float* p = pubSlewedLane_(pe, l);
        for (int v = 0; v <= nV; ++v)
            if (0 != std::memcmp(&p[v * 16 + stepArr[l]], &sig.pub[l][v], sizeof(float)))
                return kWhyPub;
    }
    if ((const void*)pe.warpBank_ != sig.bank) return kWhyBank;
    if (pe.warpBank_) {
        const dotModular::WarpBank& b = *pe.warpBank_;
        if (0 != std::memcmp(b.globalWarpW, sig.globalWarpW, sizeof b.globalWarpW)) return kWhyWarp;
        if (0 != std::memcmp(b.globalWarpF, sig.globalWarpF, sizeof b.globalWarpF)) return kWhyWarp;
        if (0 != std::memcmp(b.globalWarpTilt, sig.globalWarpTilt, sizeof b.globalWarpTilt)) return kWhyWarp;
        if (0 != std::memcmp(b.globalWarpAtomLevel, sig.globalWarpAtomLevel, sizeof b.globalWarpAtomLevel)) return kWhyWarp;
        if (0 != std::memcmp(b.globalWarpAtomMass, sig.globalWarpAtomMass, sizeof b.globalWarpAtomMass)) return kWhyWarp;
        if (b.poolUsed != sig.poolUsed) return kWhyWarp;
    }
    return kWhyInit;   // unreachable when sweepSigChanged just returned true
}
#endif   // MONSOON_PROFILE
