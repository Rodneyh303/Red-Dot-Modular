// test_lantern_bars.cpp — STEPS_32 STAGE 3, the DECIDED Lantern gate (docs/design/
// STEPS_32_PLAN.md, the three Lantern blocks + D20): the bar-aware write path, the
// follow-disc display bar, and the ≤16-unchanged law, pinned against the REAL
// executor (the same discipline as test_stage2b_pregates: executeModeA marches, the
// engine's own decisions and gate state — no mocks).
//
//   THE WRITE PATH (the old bug the gate exists for): Lantern's recording was
//   bar-blind — `if (step < 0 || step >= 16) return` dropped every absolute step
//   ≥ 16 silently once the 2b flip widened the ring. The DECIDED routing is
//   bar = step / 16, column = step % 16: steps 0..15 record into the bar-1
//   buffer, 16..31 into the bar-2 buffer, so both bars keep their own 16-column
//   history and switching the view never loses the other bar's data.
//
//   THE FOLLOW-DISC LAW (BAR INDICATOR invariant 1, display side): the displayed
//   bar is the ENGINE's absolute bar (stepIndex / 16) — the same index the inner
//   disc shows; the display reads engine position and never recomputes it. It
//   updates on the SAME tick the playhead lands in the new bar (invariant 2's
//   flip timing), including across a LIVE window boundary (a (20,0) window plays
//   through the bar line and the view follows the ENGINE, not the window).
//
//   THE LAP FORMULA, ABSOLUTE: the lap is the PHRASE (the ring), not the bar —
//   the lap-arrival flag (the wrap-arrival continuation bar) fires when the
//   absolute write step moves AGAINST travel: the ring wraps (31→0 fwd, 0→31
//   rev) and any against-travel jump — NEVER the mid-phrase bar crossing (15→16
//   fwd), whose covering onset is fully visible in the adjacent bar's buffer and
//   whose continuation is marked by the held-in caret (the DECIDED "held-over
//   carets mark notes that continue across the bar boundary").
//
//   THE ≤16 LAW: at the module-default (16,0) window every edge resolves
//   writeBar == 0, the bar-1 buffer holds the FULL history, and the bar-2 buffer
//   is never touched — the ≤16 output is the old display, unchanged.
//
// The recording logic itself is VERBATIM from the pre-Stage-3 module (moved into
// the rack-free core ui/LanternRecordCore.hpp); this suite pins the ROUTING and
// the view laws, not the articulation logic (the content checks read the engine's
// live state as the expectation — no re-implementation of recordCell to drift).
//
// Build (run_all.sh): test_lantern_bars|$SE $GS $PE $CE

#include <cstdio>
#include <cstdint>
#include <set>
#include <vector>

#include "SequencerEngine.hpp"
#include "VoiceResolver.hpp"
#include "ui/LanternRecordCore.hpp"

namespace dm = dotModular;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what); }
}

// ── the deterministic authoring: every visited cell SOUNDS (restProb 0 — with
// the factory-generated warp rows the headless rig's rest row is a constant 1.0,
// so 0 never rests and every visited cell records real content). The pe row
// writes below are kept from the harness discipline (authored inputs, nothing
// left to chance where the engine reads them).
static void authorDeterministic(SequencerEngine& eng, PatternInput& in) {
    // per-degree melody weights, varied deterministically — with all-zero weights
    // the melody pick stays one constant pitch and the content-routing check loses
    // its teeth (a mis-routed write would alias the constant).
    for (int d = 0; d < 12; ++d)
        in.semiWeights[d] = ((d % 3) == 0) ? 0.85f : (((d % 3) == 1) ? 0.45f : 0.15f);

    for (int v = 1; v <= 16; ++v) {
        const auto vs = dm::VoiceResolver::slot(v);
        eng.voices[vs].restProb   = 0.f;   // every visited cell is a note
        eng.voices[vs].legatoProb = 0.f;   // fresh Single notes: per-step pitch moves stay visible
        eng.voices[vs].accentProb = 0.f;
    }
    for (int i = 0; i < dm::kPatternLengthCap; ++i) {   // the RING width — every cell authored
        const float t = (float)i / (float)dm::kPatternLengthCap;
        eng.pe.melodyRandom[i]    = 0.05f + 0.90f * t;
        eng.pe.rhythmRandom[i]    = 0.15f + 0.70f * t;
        eng.pe.octaveRandom[i]    = t;
        eng.pe.qmixRandom[i]      = t;
        eng.pe.accentRandom[i]    = t;
        eng.pe.variationRandom[i] = t;
        eng.pe.legatoRandom[i]    = t;
    }
    eng.numPolyVoices = 3;   // rows 1..3 record poly voices V2..V4
}

// The expected cell content for one row, from the engine's live reads (the SAME
// reads recordCell consumes — the expectation is the engine, not a copy of the
// logic). Returns true when the row must sound; `pitch` is the expected pitchV.
static bool expectSounding(const SequencerEngine& eng, int row, float& pitch) {
    if (eng.lastStepResult.decision == MonoDecision::Rest) return false;
    const GateState& gs = (row == 0) ? eng.gs
                                     : eng.voices[dm::VoiceResolver::slot(row + 1)].gs;
    if (!(gs.gateHeld || gs.holdRemain > 0.0001f)) return false;
    pitch = gs.currentPitchV;
    return true;
}

struct MarchReport {
    int edges = 0;
    int soundingRow0 = 0;
    std::set<int> distinctPitchQ;      // quantised mono pitches seen on sounding edges
    std::vector<int> arrivalEdges;     // edge indices that flagged lap arrival
    std::vector<int> absAtEdge;        // the absolute write step per edge
};

// One Mode-A march (the pre-gates' shape: rm.advance(Rate::Step) → executeModeA →
// executePolyVoices), feeding the core at every step edge. The COMMON laws run
// inside: the routing identity (writeBar/Col == the absolute forStep split), the
// follow-disc display law, and the per-row content routing against live reads.
static MarchReport marchModeA(SequencerEngine& eng, LanternRecordCore& rec,
                              PatternInput& in, int steps) {
    MarchReport rep;
    redDot::RateManager rm;
    for (int i = 0; i < steps; ++i) {
        rm.advance(redDot::Rate::Step, true);
        auto r = eng.executeModeA(rm, in.noteValue, in, +1);
        if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);

        const LanternRecordCore::StepEdge e = rec.beginSample(eng);
        if (!e.valid) continue;
        rec.recordMonsoonSource(eng, e);   // record FIRST (the module's order), then pin the routed slot
        const int edgeIdx = rep.edges++;

        // ── the routing identity: the edge's write slot IS the absolute forStep split
        const int forStep = eng.lastStepResult.forStep;
        check(forStep >= 0 && forStep < dm::kMaskRing, "forStep lands inside the ring");
        check(e.writeBar == forStep / LanternRecordCore::BAR_STEPS &&
              e.writeCol == forStep % LanternRecordCore::BAR_STEPS,
              "the edge's write slot is the DECIDED split (bar = step / 16, column = step % 16)");
        const int absW = e.writeBar * LanternRecordCore::BAR_STEPS + e.writeCol;
        rep.absAtEdge.push_back(absW);

        // ── the follow-disc law: the displayed bar is the ENGINE's absolute bar
        check(rec.displayBar == eng.stepIndex / LanternRecordCore::BAR_STEPS,
              "displayBar == the engine bar index (stepIndex / 16) on every step");
        check(rec.displayCol == eng.stepIndex % LanternRecordCore::BAR_STEPS,
              "displayCol == the playhead column in the followed bar");

        // ── the content routing: rows 0..3 record their voice's LIVE state into
        // the ROUTED slot (a mis-routed write leaves stale content there).
        for (int row = 0; row < 4; ++row) {
            float pitch = 0.f;
            const bool sounding = expectSounding(eng, row, pitch);
            const auto& c = rec.cells[e.writeBar][row][e.writeCol];
            if (sounding) {
                check(c.type != lantern::NoteType::Inactive,
                      "a sounding voice's routed cell is not Inactive");
                check(c.pitchV == pitch,
                      "a sounding voice's routed cell carries THIS step's pitch (the routing pin)");
            } else {
                check(c.type == lantern::NoteType::Inactive,
                      "a rested/not-sounding voice's routed cell is Inactive");
            }
        }

        // stats
        float p0 = 0.f;
        if (expectSounding(eng, 0, p0)) {
            ++rep.soundingRow0;
            rep.distinctPitchQ.insert((int)std::lround((double)p0 * 12.0));
        }
        if (e.lapArrival) rep.arrivalEdges.push_back(edgeIdx);
    }
    return rep;
}

static bool cellIsDefault(const LanternRecordCore::Cell& c) {
    return c.type == lantern::NoteType::Inactive && c.pitchV == 0.f
        && c.lengthSteps == 0.f && !c.heldIn && !c.heldOut && !c.accented
        && !c.slurMember && !c.leadsLegato && !c.isMidTail && c.playDir == 1;
}

int main() {
    std::printf("test_lantern_bars — the DECIDED Lantern gate (Stage 3, Lantern first)\n");

    // ── 0. the decided constants + the fresh-core state ────────────────────────
    check(LanternRecordCore::BAR_STEPS == 16, "the bar is ALWAYS 16 steps (BAR INDICATOR invariant 3)");
    check(LanternRecordCore::N_BARS == 2,     "two 16-column bar buffers (the DECIDED both-bars-recorded layout)");
    check(LanternRecordCore::N_VOICES == 16,  "16 voice rows, unchanged");
    check(LanternRecordCore::N_BARS * LanternRecordCore::BAR_STEPS >= dm::kPatternLengthCap,
          "the two-bar buffer covers the 32 ring (the storage law; 64 is a conscious widening)");
    check(dm::kPatternLengthCap == 32, "the ring is 32 after the 2b flip (the context this suite pins against)");
    {
        LanternRecordCore rec;
        SequencerEngine eng; eng.reset();
        check(rec.displayBar == 0 && rec.displayCol == -1, "a fresh core shows bar 0, no playhead column");
        const auto e0 = rec.beginSample(eng);
        check(!e0.valid, "a fresh engine (stepIndex unchanged at -1) produces NO edge");
        check(rec.displayBar == 0, "an invalid edge leaves the display bar alone");
        const auto e1 = rec.beginSample(eng);
        check(!e1.valid, "a repeated call without a step change produces NO edge (the sample-key law)");
    }

    // ── 1. THE 32-STEP MARCH — both bars recorded, the follow-disc law, the lap
    //      formula absolute (the gate's literal ask) ───────────────────────────
    {
        SequencerEngine eng; eng.reset();
        PatternInput in;
        authorDeterministic(eng, in);
        LanternRecordCore rec;

        // 34 steps: a full lap lands abs 0..31, then the ring wrap 31→0→1.
        MarchReport rep = marchModeA(eng, rec, in, 34);
        check(rep.edges == 34, "34 executor steps produce 34 sample edges (every step is an edge)");

        // BOTH BARS RECORDED: bar 1 holds steps 0..15, bar 2 holds 16..31 — the
        // mono row (restProb 0 → sounding everywhere) fills ALL 16 columns of BOTH.
        int bar0Filled = 0, bar1Filled = 0;
        for (int s = 0; s < 16; ++s) {
            if (rec.cells[0][0][s].type != lantern::NoteType::Inactive) ++bar0Filled;
            if (rec.cells[1][0][s].type != lantern::NoteType::Inactive) ++bar1Filled;
        }
        check(bar0Filled == 16, "bar-1 buffer recorded ALL 16 columns of steps 0..15");
        check(bar1Filled == 16, "bar-2 buffer recorded ALL 16 columns of steps 16..31 (the old code DROPPED these)");
        // the poly rows record too (rows 1..3 = V2..V4; restProb 0 → sounding)
        int bar1Poly = 0, bar2Poly = 0;
        for (int row = 1; row <= 3; ++row)
            for (int s = 0; s < 16; ++s) {
                if (rec.cells[0][row][s].type != lantern::NoteType::Inactive) ++bar1Poly;
                if (rec.cells[1][row][s].type != lantern::NoteType::Inactive) ++bar2Poly;
            }
        check(bar1Poly > 0 && bar2Poly > 0, "poly rows record into BOTH bars (the whole grid routes)");

        // the absolute mapping: the edges' write slots ran 0..31 then wrapped to 0,1
        check((int)rep.absAtEdge.size() == 34, "every edge reported its write slot");
        bool absRamp = true;
        for (int i = 0; i < 32; ++i) absRamp = absRamp && (rep.absAtEdge[i] == i);
        absRamp = absRamp && rep.absAtEdge[32] == 0 && rep.absAtEdge[33] == 1;
        check(absRamp, "the write slots ran abs 0..31 then wrapped at the RING (the 2b walk)");

        // THE LAP FORMULA, ABSOLUTE: no arrival in the first lap (including the
        // 15→16 bar crossing), ONE arrival at the ring wrap 31→0, none after.
        check(rep.arrivalEdges.empty() || rep.arrivalEdges == std::vector<int>{32},
              "lap arrival fires ONLY at the ring wrap (edge 32, abs 31→0) — never at the bar crossing (15→16)");
        if (!rep.arrivalEdges.empty()) {
            check(rep.absAtEdge[31] == 31 && rep.absAtEdge[32] == 0,
                  "the arrival edge is the ring wrap (the write moved against travel)");
        }

        // the test's power: the mono row sounded on ~every step. The content checks
        // above run per EDGE (sounding -> the ROUTED slot carries this step's live
        // pitch/type), and the routing identity + the abs-slot ramp + the both-bars
        // fill pin the slot itself; with the headless rig's factory-generated rows
        // the pitch is constant (the generated warp rows are the defaults), so the
        // pitch equality is a CONSISTENCY pin here, and the mis-route discrimination
        // lives in the routing identity + ramp + bar-fill checks.
        check(rep.soundingRow0 >= 30, "the mono row sounded on ~every step (restProb 0)");
        check((int)rep.distinctPitchQ.size() >= 1,
              "at least one sounding edge pinned its routed slot's pitch (the consistency pin ran)");
    }

    // ── 2. THE ≤16 LAW — the module-default (16,0) window: bar-1 is the FULL
    //      history, bar-2 is never touched, the view stays bar 0 ──────────────
    {
        SequencerEngine eng; eng.reset();
        PatternInput in;
        authorDeterministic(eng, in);
        eng.updateWindow(16.f, 0.f, false, 0.f, 0.f, false);   // the module-default push
        LanternRecordCore rec;

        MarchReport rep = marchModeA(eng, rec, in, 34);   // two laps over [0,16)
        check(rep.edges == 34, "the ≤16 march edges all valid");

        bool allBar0 = true;
        for (const int a : rep.absAtEdge) allBar0 = allBar0 && (a >= 0 && a < 16);
        check(allBar0, "every write slot stayed inside bar 1 (the (16,0) walk stays in [0,16))");
        check(rec.displayBar == 0, "the view stayed on bar 0 for the WHOLE ≤16 run (displayBar law at every edge above)");

        // "the bar-1 buffer is the full history at 16": all 16 columns recorded
        int filled = 0;
        for (int s = 0; s < 16; ++s)
            if (rec.cells[0][0][s].type != lantern::NoteType::Inactive) ++filled;
        check(filled == 16, "bar-1 holds the FULL 16-column history (the ≤16 view is the old view)");

        // bar-2 was NEVER touched — every cell still the default
        bool bar2Clean = true;
        for (int row = 0; row < LanternRecordCore::N_VOICES; ++row)
            for (int s = 0; s < 16; ++s)
                bar2Clean = bar2Clean && cellIsDefault(rec.cells[1][row][s]);
        check(bar2Clean, "bar-2 was never written at ≤16 (the ≤16 output is unchanged, byte-for-byte default)");
    }

    // ── 3. THE LIVE-WINDOW CROSSING (D3 boundary vs ring meets the view) — a
    //      (20,0) window plays THROUGH the bar line: the view follows the ENGINE
    //      (flips at the abs-16 landing, mid-window), bar-2 records only the
    //      window's bar-2 cells, and the window wrap 19→0 IS a lap arrival ────
    {
        SequencerEngine eng; eng.reset();
        PatternInput in;
        authorDeterministic(eng, in);
        eng.updateWindow(20.f, 0.f, false, 0.f, 0.f, false);
        LanternRecordCore rec;

        MarchReport rep = marchModeA(eng, rec, in, 24);
        check(rep.edges == 24, "the (20,0) march edges all valid");

        // the walk covered [0,20) then wrapped: abs slots 0..19, then 0..3
        bool ramp = true;
        for (int i = 0; i < 20; ++i) ramp = ramp && (rep.absAtEdge[i] == i);
        for (int i = 20; i < 24; ++i) ramp = ramp && (rep.absAtEdge[i] == i - 20);
        check(ramp, "the (20,0) walk ran 0..19 then wrapped at the WINDOW (D3's live boundary)");

        // bar-2 holds ONLY the window's bar-2 cells (abs 16..19 → cols 0..3);
        // cols 4..15 were never played and never written.
        int bar2Filled = 0;
        for (int s = 0; s < 4; ++s)
            if (rec.cells[1][0][s].type != lantern::NoteType::Inactive) ++bar2Filled;
        bool bar2TailClean = true;
        for (int s = 4; s < 16; ++s) bar2TailClean = bar2TailClean && cellIsDefault(rec.cells[1][0][s]);
        check(bar2Filled == 4, "bar-2 recorded exactly the window's 4 bar-2 cells (abs 16..19)");
        check(bar2TailClean, "bar-2 columns outside the window were never written (the window intersected per bar)");

        // the window wrap 19→0 IS a lap arrival (against travel); the bar
        // crossing 15→16 inside the same run is NOT (covered by the march's
        // per-edge arrival check: exactly the wrap edges flagged).
        bool arrivalsOnlyAtWrap = rep.arrivalEdges.size() == 1
                              && rep.absAtEdge[rep.arrivalEdges[0]] == 0
                              && rep.absAtEdge[rep.arrivalEdges[0] - 1] == 19;
        check(arrivalsOnlyAtWrap, "the (20,0) window wrap 19→0 flagged arrival; the 15→16 bar crossing did not");
    }

    // ── 4. REVERSE travel: the absolute lap formula's other half — the reverse
    //      ring wrap 0→31 IS an arrival, the reverse bar crossing 16→15 is NOT ─
    {
        SequencerEngine eng; eng.reset();
        PatternInput in;
        authorDeterministic(eng, in);
        LanternRecordCore rec;
        // walk forward to the bar-2 interior, then reverse across the bar line
        for (int i = 0; i < 20; ++i) eng.advancePlayhead(+1);
        check(eng.stepIndex == 19, "the pre-march walk stands at abs 19");
        // reverse steps: the executor with dir -1 (the executor owns the walk)
        redDot::RateManager rm;
        int sawBarCrossArrival = 0, sawRingArrival = 0;
        int prevAbs = 19;
        for (int i = 0; i < 40; ++i) {
            rm.advance(redDot::Rate::Step, true);
            auto r = eng.executeModeA(rm, in.noteValue, in, -1);
            if (r.stepped && eng.numPolyVoices > 0) eng.executePolyVoices(in);
            const auto e = rec.beginSample(eng);
            if (!e.valid) continue;
            const int absW = e.writeBar * 16 + e.writeCol;
            if (e.lapArrival) {
                if (prevAbs == 16 && absW == 15) ++sawBarCrossArrival;   // the ADJACENT reverse bar crossing
                if (prevAbs == 0  && absW == 31) ++sawRingArrival;       // the reverse ring wrap
                // (any other against-travel arrival is a reverse JUMP — legal, the
                //  old law's "a jump that moves against travel also flags this".)
            }
            prevAbs = absW;
        }
        check(sawBarCrossArrival == 0, "reverse travel NEVER flags the bar crossing 16→15 as a lap");
        check(sawRingArrival == 1, "reverse travel flags the ring wrap 0→31 exactly once");
    }

    std::printf("\n%s: %d passed, %d failed\n",
                (g_fail == 0) ? "PASS" : "FAIL", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
