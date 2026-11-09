#pragma once
// ── redDot::PhaseDistChannel + redDot::PhaseDistBlockReader — the STANDALONE
// phase-distortion module's rack-free signal core (PHASE_DISTRIBUTION_SPEC §13,
// the module-only comparison model; docs/design/CC_PROMPT_PHASE_DISTORTION_MODULE.md).
// Built ALONGSIDE the expander model (§14) — do not modify the expander build. ──
//
// THE MODEL (§13): EIGHT phase inputs → EIGHT phase outputs, channel k in → channel
// k out, each channel's own parameter set (the SAME set shape the expander's store
// holds — PhaseDistSet, shared on purpose: one set shape, one codec, one identity
// predicate). The module sits BETWEEN the phase source and the Monsoons; the patch
// rule is a CONVENTION (every Monsoon's phase and subphase patch from the module's
// outputs; a direct source→Monsoon path is one hop and desyncs — the manual states
// it, no code enforces it). Monsoon gets NO transform code and NO index parameters
// in this build.
//
// THE FIXED ONE-SAMPLE HOP (the §13 gate "identity, adjusted"): the module adds
// exactly ONE sample of delay per channel — out(n) = transform(in(n−1)). The
// comparison doc's gate note is explicit: its identity gate is "today's output
// delayed by one sample", NOT "bit-identical to today" — tests compare against the
// source SHIFTED one sample. The hop is FIXED (not compensated, not optional): a
// two-hop path (source → module → Monsoon) is one sample behind a one-hop path, and
// the §13 sync rule is that EVERY Monsoon takes the two-hop path so they all share
// the same delay. The identity case passes the DELAYED sample through RAW — the
// voltage never round-trips through the normalised domain (the same law Monsoon's
// Mode E raw path runs; the Stage-1 bit-identity gate holds because there is no
// float round-trip).
//
// THE TRANSFORM IS THE ONE PRIMITIVE (the §13 prompt's Task 1 "only if not already
// present" — it IS present): dotModular::phaseDistort(u, cell), the extraction the
// expander build landed (SandsModifierResolver.hpp — the exact body the lane draws
// run). This core CALLS it; it does not copy the chain (the §14 gate 6 / §13 gate 4
// single-implementation law). The quantise-to-pulses step is deliberately NOT here
// either: it stays the EXISTING machinery inside the Monsoons that consume the
// module's output voltages.
//
// THE PPQN CELL RULE (CC_FIX_STANDALONE_PHASE_CELL_HOLD, docs tip f1ee29ba): the
// TRANSFORM's evaluation is no longer per-sample — the core routes it through the
// SAME cell-held core the Monsoon tier sites run (dotModular::PhaseDistCellHold,
// ONE implementation, called never copied — the doc's DO-NOT), one hold per
// channel. The transform fires ONCE PER PPQN CELL of the input at the cell's OWN
// grid point (bit-identical to a per-sample evaluation there) and holds between
// cells; an identity set keeps the raw path AND resets the hold (no stale set's
// value rides an identity episode — Monsoon's law, verbatim). The standalone's OWN
// grid is the plugin's default PPQN (24 → 6 pulses/16th) over the ramp's own
// pattern cycle (16 sixteenths — ONE full 0..10 V ramp, the jack convention):
// 96 cells per span — the same grid a default-configured Monsoon tier runs
// (pulsesPer16th(24)·patternLength 16), so the two consumers agree bit-for-bit on
// the same input and set. The module has NO ppqn/length knowledge (§13: no index
// parameters, no cross-module reads) — a consumer running a different grid
// quantises its own tier as its own machinery always has.
//
// THE VOLTAGE CONVENTION is Monsoon's Mode E convention, verbatim: in volts,
// normalised by PhaseEngine::normPhase (volt / phaseInMax, floored into [0,1) — the
// ramp wraps; 17 V reads 0.7), distorted in [0,1), re-scaled by phaseInMax. The
// default phaseInMax is 10 V and nothing in the plugin writes it elsewhere — the
// A/B comparison gate (§13 gate: matching parameters ⟹ the module's output equals
// the expander build's output delayed one sample) holds BIT-EXACTLY because both
// sides normalise through the SAME PhaseEngine member function and transform through
// the SAME primitive.
//
// THE BLOCK READER: §13 says the expander's block-consistency rule is "not needed,
// since parameters are local to the module" — there is ONE reader per channel (this
// module's own process loop) and the knobs are UI-thread writes. The reader still
// snapshots ONCE PER CONTROL BLOCK under the store mutex (the store's own write
// atomicity): a knob drag lands whole at the next block boundary, never mid-block,
// and the per-sample hot path touches no lock. Same snapshot shape as the Monsoon
// reader in the expander build, single-reader edition.
//
// PURE: no Rack — plain C++ over PhaseDistStore (itself rack-free), so the unit rig
// drives the core directly and the module embeds it. Arrays are std::array (the
// array-guard's P-B form).

#include <array>

#include "PhaseDistStore.hpp"        // the set shape + the store (shared with A)
#include "PhaseDistCellHold.hpp"     // the PPQN cell hold — the ONE hold implementation
#include "engines/PhaseEngine.hpp"   // normPhase + phaseInMax — the ONE normalisation

namespace redDot {

// ONE channel's signal path: a one-sample delay line + the shared transform,
// evaluated through the shared PPQN cell hold (one hold per channel).
struct PhaseDistChannel {
    // The delayed sample. Reset to 0 (the pre-patch source level): out(0) = 0 =
    // in(−1), so the identity stream equals the source shifted one sample from the
    // very first sample of a clean run.
    float prev_ = 0.f;

    // The normalisation AUTHORITY — the SAME normPhase()/phaseInMax Monsoon's Mode E
    // lines call (the A/B gate's shared convention; one implementation, not a
    // re-stated formula). The engine is never stepped: default config only, and
    // nothing in the plugin writes phaseInMax away from its 10 V default.
    PhaseEngine pe_{};

    // THE CELL HOLD (CC_FIX_STANDALONE_PHASE_CELL_HOLD): the SAME type the Monsoon
    // tier sites run — the fire-once-per-cell + hold lives THERE, not here (the
    // doc's DO-NOT: no second cell or hold path). Public: the unit rig reads the
    // hold's cell state for the fire-count pins.
    dotModular::PhaseDistCellHold hold_{};

    // The standalone's OWN grid (the module's evaluation rate, not the consumer's
    // pulse walk): pulsesPer16th(24)·16 — the plugin's default PPQN over ONE full
    // 0..10 V ramp (the pattern cycle the jack convention defines). 96 cells.
    static constexpr long kSpanPulses =
        (long)PhaseEngine::pulsesPer16th(24) * 16;

    // ONE sample. `inVolt` is THIS sample's input; the output is the PREVIOUS
    // sample's input through the channel's set. Identity set ⟹ the delayed sample
    // returned RAW (bit-exact — the fixed hop, nothing else) and the hold RESET
    // (Monsoon's step-3 law: the re-entry re-seeds — the first cell IS a new
    // cell). Non-identity ⟹ the held-cell core evaluates the transform ONCE PER
    // CELL of the delayed input at the cell's own grid point and holds between
    // cells. Order matters: read the delay line first, latch second (a one-line
    // pipeline, not a feedback loop).
    float process(float inVolt, const PhaseDistSet& s) {
        const float delayed = prev_;
        prev_ = inVolt;
        if (s.isIdentity()) { hold_.reset(); return delayed; }   // the raw path — the §13 identity gate
        return hold_.processTransport(pe_.normPhase(delayed), kSpanPulses,
                                      s.cell(), pe_.phaseInMax);
    }

    void reset() { prev_ = 0.f; hold_.reset(); }
};

// The per-block snapshot reader: ONE reader (this module), all eight sets, once per
// control block. Between blocks the module reads its private copies — the hot path
// is lock-free (the §14 mechanism's shape, single-reader edition; §13 notes the
// cross-reader consistency rule is not needed because the sets are module-local).
struct PhaseDistBlockReader {
    static constexpr int kSets = PhaseDistStore::kSets;

    std::array<PhaseDistSet, kSets> snap_{};   // the private working copies
    std::array<bool, kSets>        identity_{}; // the raw-path flags (frozen per block)

    void beginBlock(const PhaseDistStore& store) {
        for (int i = 0; i < kSets; ++i) {
            store.snapshotSet(i, snap_[i]);       // under the store mutex, atomic per set
            identity_[i] = snap_[i].isIdentity(); // the Stage-1 predicate, frozen for the block
        }
    }
};

} // namespace redDot
