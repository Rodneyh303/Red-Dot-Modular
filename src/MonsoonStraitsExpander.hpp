#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// ── Straits — base poly expander (transit family) ────────────────────────────
// The refactor/simplification of the old Straits East/West pair. West retires;
// one Straits module houses all poly voices. It carries the per-poly-voice REST
// and ACCENT probability knobs (each voice's reaction to the probability rolls),
// laid out as a 4-col × 8-row grid, and exposes the poly voices as 16-CHANNEL
// POLY CABLES (gate / accent-gate / CV) — replacing the old 21 individual jacks.
//
// Poly-cable channel convention (matches Causeway CV in and Changi out):
//   ch0 = MONO voice (voice 1) ALWAYS — duplicated from the parent Monsoon.
//   ch1..15 = poly voices 2..16.
// Cables are always 16ch wide; voices beyond engine.numPolyVoices output
// gate-low / 0V. Voice count is set by the context menu on the parent Monsoon
// (engine.numPolyVoices); Straits just reads it.
//
// REST/ACCENT base-level PARAMS reuse the existing MonsoonIds::POLY_REST_PARAM_*
// / POLY_ACCENT_PARAM_* enums (voices 2..16 = 15 poly voices; voice 1/mono's
// rest/accent lives on the parent Monsoon). No new engine plumbing — this is a
// panel + I/O simplification of what already exists.
//
// CV MODULATION of these levels moves OUT to the separate Causeway expander;
// per-voice mono OUT jacks move to the separate Changi expander. Straits itself
// carries only the base knobs + the poly-cable outs.
namespace StraitsIds {
    enum OutputIds {
        POLY_GATE_OUT,     // 16ch: ch0 = mono, ch1..15 = poly voices 2..16
        POLY_CV_OUT,       // 16ch pitch
        POLY_ACCENT_OUT,   // 16ch accent gate
        NUM_OUTPUTS
    };
}

struct MonsoonStraitsExpander : Module {
    MonsoonStraitsExpander() {
        // Sized to the main MonsoonIds param/input namespace (the expander reuses
        // those IDs), plus the local poly-cable outputs.
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, StraitsIds::NUM_OUTPUTS, 0);

        // Per-poly-voice REST + ACCENT probability knobs (voices 2..16 = 15 knobs each).
        // Voice 1 (mono) rest/accent lives on the parent Monsoon.
        for (int i = 0; i < 15; i++) {
            configParam(MonsoonIds::POLY_REST_PARAM_1 + i, 0.f, 1.f, 0.1f,
                        "Voice " + std::to_string(i + 2) + " Rest Probability");
            configParam(MonsoonIds::POLY_ACCENT_PARAM_1 + i, 0.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " Accent Probability");
        }

        configOutput(StraitsIds::POLY_GATE_OUT,   "Poly gate (16ch: ch1 = mono, ch2.. = poly)");
        configOutput(StraitsIds::POLY_CV_OUT,     "Poly CV / pitch (16ch)");
        configOutput(StraitsIds::POLY_ACCENT_OUT, "Poly accent gate (16ch)");
    }

    // The parent Monsoon writes the poly-cable outputs via the cached pointer
    // (see MonsoonOutputGenerator). Nothing to do per-sample here.
    void process(const ProcessArgs& args) override {}
};
