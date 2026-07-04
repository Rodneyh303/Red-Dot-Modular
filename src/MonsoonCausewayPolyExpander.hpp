#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// ── Causeway — poly CV modulation expander (transit family) ──────────────────
// "The link across" — modulation across the poly voices. Carries the per-voice
// REST and ACCENT CV modulation that used to sit on the old Straits East/West
// surfaces, now consolidated as two 16-CHANNEL POLY CV inputs (ch0=mono,
// ch1..15=poly voices 2..16 — same convention as Straits' poly-cable outs),
// each with a single attenuverter. Monsoon reads these per-channel and adds the
// scaled CV on top of the base REST/ACCENT knobs (on Straits).
//
// Reuses existing MonsoonIds enums so no host param-space churn:
//   POLY_REST_CV_INPUT     → 16ch REST modulation CV in
//   POLY_ACCENT_CV_INPUT   → 16ch ACCENT modulation CV in
//   POLY_REST_MOD_ATT_1    → REST modulation attenuverter (one, global to the cable)
//   POLY_ACCENT_MOD_ATT_1  → ACCENT modulation attenuverter
//
// Simplification vs the old design: the old East/West carried 15 individual mod
// CV jacks + 15 attenuverters per lane; a poly CV input already addresses voices
// per-channel, so a single poly jack + one attenuverter per lane replaces them.
struct MonsoonCausewayPolyExpander : Module {
    MonsoonCausewayPolyExpander() {
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, 0, 0);
        configParam(MonsoonIds::POLY_REST_MOD_ATT_1,   -1.f, 1.f, 0.f, "Rest CV attenuverter");
        configParam(MonsoonIds::POLY_ACCENT_MOD_ATT_1, -1.f, 1.f, 0.f, "Accent CV attenuverter");
        configInput(MonsoonIds::POLY_REST_CV_INPUT,   "Poly rest modulation CV (16ch: ch1=mono)");
        configInput(MonsoonIds::POLY_ACCENT_CV_INPUT, "Poly accent modulation CV (16ch: ch1=mono)");
    }

    // Monsoon reads these inputs/attenuverters via the cached pointer; nothing per-sample here.
    void process(const ProcessArgs& args) override {}
};
