#pragma once
// ── dotModular::PhaseDistCellHold — the ENGINE-side PPQN-cell hold for the
// phase-distortion transform (docs/design/CC_FIX_PHASE_DISTORT_AT_PPQN.md, docs tip
// 9b900695, building on the phase PPQN gate 4ed0f10). ────────────────────────────
//
// THE RULE (Rodney): calculate the distortion at PPQN rate for the ENGINE, and at
// VIDEO rate for DISPLAY. No change-tolerance heuristic. The measured cause: with a
// non-uniform set the transform (an inverse CDF plus density) ran EVERY SAMPLE
// (Monsoon.cpp's two tier sites) — 0.5% → 1.2% CPU in phase mode, identity vs
// non-uniform, expander attached or not.
//
// THE ENGINE CONTRACT: phaseDistort is evaluated ONCE PER PPQN CELL of the input
// and held until the next cell. The cell is the input's pulse index on the PPQN
// grid, FROM THE EXISTING QUANTISATION — per tier, the engine's own running
// expression (the CC_FIX_PHASE_RUN_AT_PPQN adjudication: the engine's actual
// expression wins over any literal re-derivation):
//   · TRANSPORT — the plain path's OWN probe llround(phase·PPB) (PhaseEngine's
//     targetPulse); nearest-pulse, the same convention the engine's pulse walk
//     lands on. The span ring is cyclic, so index PPB IS index 0 (the engine's own
//     law: "its edge leak IS the wrap") — the hold wraps the index and evaluates at
//     the wrapped grid point, staying inside the transform's [0,1) domain (u = 1.0
//     is unreachable as a normPhase output and never evaluated).
//   · SUBPHASE — the fine term's OWN quantisation floor(s·regionPulses) inside
//     tierPulseFor, on the REGION-RELATIVE grid. Floor keeps the subphase STRICTLY
//     inside its region at every s < 1 (PhaseEngine's own within-region law:
//     llround would leak one pulse across the region boundary at s → 1⁻); the last
//     cell holds until the sweep wrap fires cell 0. regionPulses = rF·PPB =
//     regionSixteenths·pulsesPer16th — exactly the engine's fine-term scale.
//
// THE FIRE: when the cell changes (the ONLY re-fire trigger — the DO-NOT: no
// change-tolerance or threshold heuristic; the rule IS the PPQN cell), the
// transform is called ONCE, AT THE CELL'S OWN GRID POINT (cell/grid in [0,1) — the
// point the existing quantisation names for that cell), so the held value is the
// value the engine would have produced with the input sitting exactly there:
// bit-identical at the grid point, not an approximation of it. Between cells the
// engine reads the held value; no other per-sample transform call exists. A
// feature narrower than one pulse cell (a steep stretch, an atom or gap edge
// inside a cell) lands on the next evaluation point — the spec's own rule that
// phase jumps land on PPQN positions, now applied to the transform.
//
// THE CELL IDENTITY IS (GRID, INDEX): a pattern-length or PPQN change rebuilds the
// grid, so a grid change IS a cell change (comparing bare indices across grids
// would silently reuse a value computed for a different grid point).
//
// THE RAW PATH (step 3): an identity set — or an unpatched subphase CV3 — needs no
// transform and no held value; the caller resets the hold there, so no stale set's
// value can ride an identity episode or an unpatched gap into the next non-identity
// sample (the re-entry re-seeds: the first cell IS a new cell).
//
// THE DISPLAY PATH (step 4) IS NOT HERE: the DistributionCurveWidget and the phase
// markers keep their own continuous video-rate evaluation; neither reads this held
// value, and the engine never reads the display. The standalone module
// (PhaseDistChannel) now runs THIS hold too — one per channel, its own default grid
// (CC_FIX_STANDALONE_PHASE_CELL_HOLD, docs tip f1ee29ba: the PPQN rule reaches the
// standalone; one hold implementation, called never copied).
//
// PURE HEADER: no Rack — plain C++ over SandsModifierResolver.hpp (the ONE
// primitive, dotModular::phaseDistort — the §14 single-implementation gate; the
// hold CALLS it, never copies it). Unit rig: test/test_phase_distortion.cpp.

#include <cmath>   // std::llround, std::floor

#include "SandsModifierResolver.hpp"   // dotModular::phaseDistort, redDot::WarpCell

namespace dotModular {

struct PhaseDistCellHold {
    // The last fired cell: index + the grid it lives on. (-1, -1) = none yet — the
    // first call IS a new cell (the seed fire).
    long  lastCell = -1;
    long  lastGrid = -1;
    // The held output voltage: the transform at the current cell's grid point,
    // rescaled by phaseInMax — the exact value the engine reads between cells.
    float heldVolt = 0.f;

    void reset() { lastCell = -1; lastGrid = -1; heldVolt = 0.f; }

    // THE TRANSPORT TIER: the input cell from the plain probe's quantisation
    // (llround — the engine's own pulse index of a phase on the span grid).
    float processTransport(double u, long ppb, const redDot::WarpCell& set,
                           float phaseInMax) {
        long cell = (long)std::llround(u * (double)ppb);
        if (cell >= ppb) cell -= ppb;   // the span ring's edge leak IS the wrap
        return eval(cell, ppb, set, phaseInMax);
    }

    // THE SUBPHASE TIER: the input cell from the fine term's quantisation
    // (floor — the engine's own within-region law) on the region-relative grid.
    // floor(u·rp) stays in [0, rp−1] for u ∈ [0,1) (normPhase guarantees u < 1,
    // so u·rp < rp) — no wrap guard exists to write.
    float processSubphase(double u, long regionPulses, const redDot::WarpCell& set,
                          float phaseInMax) {
        long cell = (long)std::floor(u * (double)regionPulses);
        return eval(cell, regionPulses, set, phaseInMax);
    }

private:
    // THE FIRE + THE HOLD (steps 1-2): a cell change — index or grid — is the only
    // re-fire; the fire evaluates the transform ONCE at the cell's own grid point
    // (the SAME expression shape the per-sample path ran: cast to float, THEN the
    // phaseInMax multiply — bit-identical arithmetic at the grid point); the
    // unchanged-cell path reuses the held value untouched.
    float eval(long cell, long grid, const redDot::WarpCell& set, float phaseInMax) {
        if (cell != lastCell || grid != lastGrid) {
            const double uGrid = (double)cell / (double)grid;
            heldVolt = (float)phaseDistort(uGrid, set) * phaseInMax;
            lastCell = cell;
            lastGrid = grid;
        }
        return heldVolt;
    }
};

} // namespace dotModular
