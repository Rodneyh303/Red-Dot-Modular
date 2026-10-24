// test_voice_conventions.cpp — voice-convention CONTRACT tests (plans/VOICE_CONVENTION_TESTS.md,
// driven by plans/V16_FLICKER_root_smell.md: VoiceResolver::polyBankIndex was a lying abstraction —
// name/comments said 0-based poly bank (0=V2, voiceSlot=polyBankIndex+1) while the code returned
// v-1 = voiceSlot-for-poly. (Renamed polyVoiceSlot in the Phase-1 truth-fix; value unchanged.)
// These tests pin index<->voice mappings by ACTUAL BEHAVIOUR
// (definition's array + writer, never the param name — names lie), so a lying convention or a
// wrong-convention caller fails with "voice N's data appeared under voice M".
//
// The four tests (per VOICE_CONVENTION_TESTS.md):
//   1. ROUND-TRIP IDENTITY — sentinel via the writer path, read back via every accessor,
//      for the boundary voices V1 (slot 0), V2 (slot 1), V16 (slot 15). Catches the whole
//      wrong-voice class no matter where it hides.
//   2. ACCESSOR CONVENTION CONTRACT — pin each per-voice accessor's index->voice mapping.
//   3. BOUNDARY-VOICE COVERAGE — folded into 1+2: slots 0 (V1), 1 (V2), 14 (V15), 15 (V16).
//   4. STABILITY — read V16's display value twice with a refresh between, no input change
//      -> bit-equal (the deterministic flicker catcher; thread-race flicker needs Rack).
//
// RACK-ONLY (out of scope here, flagged per protocol): polySpreadEffective (module member,
// written by MonsoonExpanderManager), Monsoon::getSpread, editor.spread/macroOwn/laneDir stores.
//
// LANE-CONVENTION SPLIT — FOUND by the originally-RED [C] pins, FIXED the same session
// (see plans/convention_unification_INCOMPLETE.md "THE LANE AXIS"). SpreadInterp's switches
// used the OLD spread order (0=REST, 1=MEL, 2=OCT, 3=ACC, 4=QMIX) while unified PL_* callers
// (MEL=0, OCT=1, QMIX=2, REST=3, ACC=4, VAR=5, LEG=6) passed the new order — 5/7 lanes
// crossed wherever spread != 0, the AUDIO poly spread path included, and the V16 melody-bar
// flicker's mechanism (the melody spread target was V1's RHYTHM row, which churns under
// rhythm-mix modulation). Fixed by: SpreadInterp speaks PL_*; MonsoonSandsManager's applyMono
// sites + SpreadManager::getOriginalValue + SandsParameterManager converted to PL_*.
// Phase-2 follow-up (LANDED, same session): the spread-VALUE fetch columns were converted
// to unified PL_* (spreadE/sprForLane/spv — the stores were verified unified: menu
// EDITOR_LONG, East binds, publishGlobal); setMacroSpread deleted as dead code. Rack
// verification of the knob→lane mapping is the remaining behavioural check.
#include "engines/PatternEngine.hpp"
#include "engines/SequencerEngine.hpp"
#include "managers/SpreadManager.hpp"
#include "VoiceResolver.hpp"
#include "SpreadInterp.hpp"
#include <cstdio>
#include <cstdint>

using VR = dotModular::VoiceResolver;   // struct alias — VoiceResolver is a struct, not a namespace

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed;
    else { ++failed; std::printf("  FAIL: %s\n", what); }
}

// Unique sentinel per (slot, step): distinct across slots AND steps, in (0,1).
static float stepSentinel(int slot, int step) { return 0.123f + 0.001f * (slot * 16 + step); }
// Unique sentinel per slot.
static float slotSentinel(int slot) { return 0.05f + 0.01f * slot; }

// ── Shared setup (same recipe as test_eradication_harness) ──
static PatternEngine makeSeededEngine(int nPolyVoices) {
    PatternEngine pe;
    pe.seedRhythmPhilox(3.14f);
    pe.seedMelodyPhilox(2.71f);
    pe.seedQmixPhilox(1.41f);
    pe.seedBridgeRhythmPhilox(3.14f);
    pe.seedBridgeMelodyPhilox(2.71f);
    pe.seedBridgeQmixPhilox(1.41f);
    pe.numPolyVoicesHint = nPolyVoices;
    pe.syncPolyVoiceCount();
    return pe;
}
static void rollDice(PatternEngine& pe, const PatternInput& in) {
    pe.rhythmSeedPending = true;  pe.rhythmSeedPendingFloat = 3.14f;
    pe.melodySeedPending = true;  pe.melodySeedPendingFloat = 2.71f;
    pe.qmixSeedPending   = true;  pe.qmixSeedPendingFloat   = 1.41f;
    pe.applyPendingSeedsAndRedraw(in);
}

int main() {
    std::printf("Voice-convention contract tests (VOICE_CONVENTION_TESTS.md)\n");
    const int PL_MEL = SequencerEngine::PL_MELODY;
    const int PL_RST = SequencerEngine::PL_REST;

    // ════════════════════════════════════════════════════════════════════
    // A. RESOLVER STATIC CONTRACT (the abstraction under truth-fix)
    // ════════════════════════════════════════════════════════════════════
    std::printf("[A: resolver statics]\n");
    {
        check(VR::voiceSlot(1) == 0 && VR::voiceSlot(2) == 1 && VR::voiceSlot(16) == 15,
              "voiceSlot(v) = v-1  (V1=0, V2=1, V16=15)");
        // polyVoiceSlot (renamed from polyBankIndex — the name lied; see
        // plans/V16_FLICKER_root_smell.md): SAME VALUE as voiceSlot for poly voices,
        // -1 for mono (the poly guard). This pin made the rename provably value-identical.
        check(VR::polyVoiceSlot(2) == 1 && VR::polyVoiceSlot(16) == 15,
              "polyVoiceSlot(poly v) = v-1  (V2=1, V16=15) — SAME value as voiceSlot");
        check(VR::polyVoiceSlot(1) == -1,
              "polyVoiceSlot(mono V1) = -1  (poly guard, not a slot)");
        bool allEq = true;
        for (int v = 2; v <= 16; ++v) if (VR::voiceSlot(v) != VR::polyVoiceSlot(v)) allEq = false;
        check(allEq, "voiceSlot == polyVoiceSlot across the whole poly range (ONE convention)");
        check(VR::slotToVoice(VR::voiceSlot(16)) == 16 && VR::slotToVoice(VR::voiceSlot(1)) == 1,
              "slotToVoice(voiceSlot(v)) round-trips (V1, V16)");
        check(VR::kVoiceSlots == 16 && VR::kMonoSlot == 0,
              "16 voice slots; mono occupies slot 0");
    }

    // ════════════════════════════════════════════════════════════════════
    // B. ENGINE ACCESSOR CONTRACTS (pin by behaviour: write the store, read the accessor)
    // ════════════════════════════════════════════════════════════════════
    std::printf("[B: engine accessor contracts]\n");
    {
        PatternEngine pe;

        // random_[voiceSlot][editorLane][step]; polyRandom(slot) reads random_[slot].
        for (int slot : {0, 1, 14, 15}) {
            pe.random_[slot][PL_MEL][4] = stepSentinel(slot, 4);
            char label[96];
            std::snprintf(label, sizeof(label),
                          "polyRandom(slot=%d) reads random_[slot] (V%d)", slot, slot + 1);
            check(pe.polyRandom(dotModular::VoiceResolver::slot(slot + 1), dotModular::LaneId{PL_MEL})[4] == stepSentinel(slot, 4), label);
        }
        pe.random_[14][PL_MEL][4] = stepSentinel(14, 4);
        pe.random_[15][PL_MEL][4] = stepSentinel(15, 4);
        check(pe.polyRandom(dotModular::VoiceResolver::slot(14 + 1), dotModular::LaneId{PL_MEL})[4] == stepSentinel(14, 4)
              && pe.polyRandom(dotModular::VoiceResolver::slot(15 + 1), dotModular::LaneId{PL_MEL})[4] == stepSentinel(15, 4),
              "adjacent slots V15/V16 hold their OWN rows (no off-by-one)");

        // pubSlewedPoly* via the publishSlewed* writer: [slot], [0] = V1.
        for (int slot : {0, 1, 15}) {
            pe.slewedMelody[slot][5] = stepSentinel(slot, 5);
        }
        pe.publishSlewedMelody();
        for (int slot : {0, 1, 15}) {
            char label[96];
            std::snprintf(label, sizeof(label),
                          "publishSlewedMelody: pubSlewedPolyMelody[%d] = slewedMelody[%d] (V%d)",
                          slot, slot, slot + 1);
            check(pe.pubSlewedPolyMelody[slot][5] == stepSentinel(slot, 5), label);
        }
        check(pe.pubSlewedPolyMelody[0][5] == stepSentinel(0, 5)
              && pe.pubSlewedPolyMelody[15][5] == stepSentinel(15, 5),
              "pub slot 0 = V1 and slot 15 = V16 are distinct (mono branch reads [0])");

        // V1's final value: finalRandomByStrand reads the mono row random_[0].
        pe.random_[0][PL_MEL][7] = stepSentinel(0, 7);
        check(pe.finalRandomByStrand(PL_MEL, 7) == stepSentinel(0, 7),
              "finalRandomByStrand = random_[0] (V1 IS slot 0 — mono not special)");

        // SequencerEngine LOR store: lorStore_[16-wide slot], accessors by slot.
        {
            SequencerEngine se;
            for (int slot : {0, 1, 14, 15}) {
                se.polyLenERef(dotModular::VoiceResolver::slot(slot + 1), dotModular::LaneId{PL_MEL})  = 3 + slot;
                se.polyOffERef(dotModular::VoiceResolver::slot(slot + 1), dotModular::LaneId{PL_MEL})  = slot;
                se.polyRotERef(dotModular::VoiceResolver::slot(slot + 1), dotModular::LaneId{PL_MEL})  = slot + 1;
            }
            bool okLen = true, okOff = true, okRot = true;
            for (int slot : {0, 1, 14, 15}) {
                if (se.polyLenE(dotModular::VoiceResolver::slot(slot + 1), dotModular::LaneId{PL_MEL}) != 3 + slot) okLen = false;
                if (se.polyOffE(dotModular::VoiceResolver::slot(slot + 1), dotModular::LaneId{PL_MEL}) != slot)     okOff = false;
                if (se.polyRotE(dotModular::VoiceResolver::slot(slot + 1), dotModular::LaneId{PL_MEL}) != slot + 1) okRot = false;
                // Editor-order accessor must agree (lane order unified: identity).
                if (se.polyLOR(slot, SequencerEngine::editorLane(PL_MEL), SequencerEngine::LOR_LEN) != 3 + slot) okLen = false;
            }
            check(okLen, "polyLenE/polyLOR(slot) read lorStore_[slot]  (16-wide, slot 0 = V1)");
            check(okOff, "polyOffE(slot) reads lorStore_[slot]");
            check(okRot, "polyRotE(slot) reads lorStore_[slot]");
            // Adjacency: V16's LOR must not leak into V15's.
            se.polyLenERef(dotModular::VoiceResolver::slot(15 + 1), dotModular::LaneId{PL_MEL}) = 11; se.polyLenERef(dotModular::VoiceResolver::slot(14 + 1), dotModular::LaneId{PL_MEL}) = 4;
            check(se.polyLenE(dotModular::VoiceResolver::slot(15 + 1), dotModular::LaneId{PL_MEL}) == 11 && se.polyLenE(dotModular::VoiceResolver::slot(14 + 1), dotModular::LaneId{PL_MEL}) == 4,
                  "V15/V16 LOR slots independent (no clobber)");
        }

        // laneTickV_[16]/laneSignV_[16] via polyLaneTick/polyLaneSign (bank = voiceSlot).
        {
            SequencerEngine se;
            const int melStrand = SequencerEngine::polyLaneStrand(PL_MEL);
            const int rstStrand = SequencerEngine::polyLaneStrand(PL_RST);
            for (int slot : {0, 1, 15}) {
                se.laneTickV_[slot][melStrand] = 5 + slot;
                se.laneSignV_[slot][rstStrand] = (slot == 15) ? -1 : 1;
            }
            bool okTick = true, okSign = true;
            for (int slot : {0, 1, 15}) {
                if (se.polyLaneTick(slot, PL_MEL) != 5 + slot) okTick = false;
                if (se.polyLaneSign(slot, rstStrand) != ((slot == 15) ? -1 : 1)) okSign = false;
            }
            check(okTick, "polyLaneTick(bank) reads laneTickV_[bank]  (16-wide voiceSlot)");
            check(okSign, "polyLaneSign(bank) reads laneSignV_[bank]  (16-wide voiceSlot)");
            // Out-of-range bank falls back to mono's lane, never reads OOB.
            se.laneTick_[melStrand] = 9;
            check(se.polyLaneTick(-1, PL_MEL) == 9 && se.polyLaneTick(16, PL_MEL) == 9,
                  "polyLaneTick out-of-range bank falls back to mono laneTick_ (no OOB)");
        }

        // SequencerEngine probability chain: polyLaneProbabilityAtStep(lane, voice, step)
        // -> pe.warpRow(dotModular::VoiceResolver::slot(voice + 1), dotModular::LaneId{lane}) -> random_[voice] (bank-less: warpRow IS the raw row).
        {
            SequencerEngine se;
            for (int slot : {0, 1, 15}) se.pe.random_[slot][PL_MEL][9] = stepSentinel(slot, 9);
            bool ok = true;
            for (int slot : {0, 1, 15})
                if (se.polyLaneProbabilityAtStep(PL_MEL, slot, 9) != stepSentinel(slot, 9)) ok = false;
            check(ok, "polyLaneProbabilityAtStep(lane, voice=slot) reads random_[slot] (chain pin)");
            check(se.polyLaneProbabilityAtStep(PL_MEL, 15, 9) != se.polyLaneProbabilityAtStep(PL_MEL, 14, 9)
                  || se.pe.random_[14][PL_MEL][9] == se.pe.random_[15][PL_MEL][9],
                  "V16's chain value is its OWN (adjacent-slot independence)");
        }

        // VoiceResolver read facade: laneProbabilityAtStep dispatches V1->slot 0, V(k)->slot
        // k-1 — through the MACRO-OWN source (CC_PROMPT_macro_output_east_leak: pubSlewed*
        // base + macroOwnSpread via SequencerEngine::macroOwnLaneProbability, the ONE
        // accessor the bars also read — NOT the shared random_ rows East's spread writes
        // on an East-owned lane). macroOwnSpread defaults 0, no warp bank wired ⇒ the
        // accessor returns the pubSlewed base bit-exactly, so the sentinels pin the
        // (voice, lane, step) addressing through the NEW source.
        {
            SequencerEngine se;
            for (int slot = 0; slot < 16; ++slot) se.pe.pubSlewedPolyMelody[slot][2] = stepSentinel(slot, 2);
            dotModular::VoiceResolver vr(se);
            check(vr.laneProbabilityAtStep(1, PL_MEL, 2) == stepSentinel(0, 2),
                  "resolver V1 -> mono row (pubSlewed slot 0)");
            check(vr.laneProbabilityAtStep(2, PL_MEL, 2) == stepSentinel(1, 2),
                  "resolver V2 -> slot 1");
            check(vr.laneProbabilityAtStep(16, PL_MEL, 2) == stepSentinel(15, 2),
                  "resolver V16 -> slot 15");
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // C. SpreadManager CONTRACT (slot bank + the Phase-2 arithmetic pins)
    // ════════════════════════════════════════════════════════════════════
    std::printf("[C: SpreadManager contracts]\n");
    {
        PatternEngine pe;
        redDot::SpreadManager sm(&pe);   // default numVoices=7, startVoiceIdx=0 (East)
        bool okRW = true;
        for (int slot : {0, 1, 14, 15}) {
            sm.setSpread(slot, 0, slotSentinel(slot));
            if (sm.getSpread(slot, 0) != slotSentinel(slot)) okRW = false;
        }
        check(okRW, "SpreadManager set/getSpread(slot) — 16-wide slot bank (V16 = slot 15 storable)");
        // Adjacency non-clobber (the historical [8]-wide bug class).
        sm.setSpread(14, 0, 0.11f);
        sm.setSpread(15, 0, 0.22f);
        check(sm.getSpread(14, 0) == 0.11f && sm.getSpread(15, 0) == 0.22f,
              "slot 15 write does not clobber slot 14 (old avgCache_ overflow class)");
        // LANE-CONVENTION CONTRACT (unified PL_* order — the lane split is FIXED; these
        // pins were the deliberately-RED rot map before the fix, now they guard it):
        pe.slewedMelody[0][6]  = stepSentinel(0, 6);
        pe.slewedMelody[1][6]  = stepSentinel(1, 6);
        pe.slewedRhythm[1][6]  = 0.645f;   // distinct from the melody sentinel
        pe.publishSlewedMelody();
        pe.publishSlewedRhythm();

        check(redDot::SpreadInterp::polySlewedPub(pe, PL_MEL, 1, 6) == pe.pubSlewedPolyMelody[1][6],
              "polySlewedPub(PL_MELODY=0) reads the MELODY array (unified order)");
        check(redDot::SpreadInterp::polySlewedPub(pe, SequencerEngine::PL_REST, 1, 6)
                  == pe.pubSlewedPolyRhythm[1][6],
              "polySlewedPub(PL_REST=3) reads the RHYTHM array (unified order)");
        sm.setSpread(1, PL_MEL, 0.f);   // spread 0 -> interpolate returns original exactly
        float got = sm.getInterpolatedValue(1, PL_MEL, 6);
        char which[160];
        std::snprintf(which, sizeof(which),
                      "getInterpolatedValue(voiceIdx=1, PL_MELODY) reads slot 1 MELODY row %.3f, got %.3f",
                      stepSentinel(1, 6), got);
        check(got == stepSentinel(1, 6), which);
    }

    // ════════════════════════════════════════════════════════════════════
    // D. ROUND-TRIP IDENTITY (the big one) — writer paths, boundary voices V1/V2/V16
    // ════════════════════════════════════════════════════════════════════
    std::printf("[D: round-trip identity V1/V2/V16]\n");
    {
        // Real writer pipeline: seeds -> roll -> promote (slewed* -> random_ per voice) -> publish.
        PatternEngine pe = makeSeededEngine(15);
        pe.sandsActive = false;             // promote writes slewed* directly into random_
        pe.cachedSpreadInitialized = false; // ...without spread (matches eradication harness Test 2)
        PatternInput in;
        rollDice(pe, in);
        pe.publishSlewedMelody();

        bool okPromote = true, okPub = true;
        for (int v = 0; v < 16; ++v) {
            for (int s = 0; s < 16; ++s) {
                if (pe.polyRandom(dotModular::VoiceResolver::slot(v + 1), dotModular::LaneId{PL_MEL})[s] != pe.slewedMelody[v][s]) okPromote = false;
                if (pe.pubSlewedPolyMelody[v][s] != pe.slewedMelody[v][s]) okPub = false;
            }
        }
        check(okPromote, "promote: random_[v] = slewed*[v] for ALL 16 voices (V16 included)");
        check(okPub,     "publish: pubSlewedPoly*[v] = slewed*[v] for ALL 16 voices (V16 included)");

        // Boundary-voice explicit: V1's row is NOT V2's, V16's is NOT V15's (two-pattern flip
        // class = adjacent voices aliasing; any off-by-one anywhere in the pipeline breaks this).
        bool v1v2Distinct = false, v15v16Distinct = false;
        for (int s = 0; s < 16; ++s) {
            if (pe.pubSlewedPolyMelody[0][s]  != pe.pubSlewedPolyMelody[1][s])  v1v2Distinct = true;
            if (pe.pubSlewedPolyMelody[14][s] != pe.pubSlewedPolyMelody[15][s]) v15v16Distinct = true;
        }
        check(v1v2Distinct,  "V1 and V2 rows are DISTINCT data (no V1<->V2 aliasing)");
        check(v15v16Distinct, "V15 and V16 rows are DISTINCT data (no V15<->V16 aliasing — the flicker class)");
    }

    // ════════════════════════════════════════════════════════════════════
    // E. STABILITY (deterministic flicker catcher) — V16, read twice + refresh
    // ════════════════════════════════════════════════════════════════════
    std::printf("[E: stability V16 (refresh between reads)]\n");
    {
        PatternEngine pe = makeSeededEngine(15);
        PatternInput in;
        rollDice(pe, in);
        pe.publishSlewedMelody();

        float row1[16], disp1[16];
        for (int s = 0; s < 16; ++s) {
            row1[s] = pe.pubSlewedPolyMelody[15][s];
            disp1[s] = redDot::SpreadInterp::applyAnchorV1Only(pe, PL_MEL, s, row1[s], 0.5f);
        }
        pe.forceRecomputeSlewed();   // refresh with NO input change
        bool okRow = true, okDisp = true;
        for (int s = 0; s < 16; ++s) {
            if (pe.pubSlewedPolyMelody[15][s] != row1[s]) okRow = false;
            if (redDot::SpreadInterp::applyAnchorV1Only(pe, PL_MEL, s,
                    pe.pubSlewedPolyMelody[15][s], 0.5f) != disp1[s]) okDisp = false;
        }
        check(okRow,  "V16 pub row bit-stable across forceRecomputeSlewed (no second writer)");
        check(okDisp, "V16 display value (applyAnchorV1Only, spread=0.5) bit-stable across refresh");

        // Second refresh — three reads total must be pairwise identical (deterministic pipeline).
        pe.forceRecomputeSlewed();
        bool okThird = true;
        for (int s = 0; s < 16; ++s)
            if (pe.pubSlewedPolyMelody[15][s] != row1[s]) okThird = false;
        check(okThird, "V16 pub row stable across a SECOND refresh (idempotent recompute)");
    }

    std::printf("voice-convention contracts: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
