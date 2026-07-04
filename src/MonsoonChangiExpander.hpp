#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// ── Changi — per-voice output expander (transit family) ──────────────────────
// "Departures" — the poly voices leave as individual mono jacks. Breaks the poly
// cable out into per-voice GATE / CV / ACCENT jacks, for patching each voice to a
// separate destination (the counterpart to Straits' single poly cables).
//
// 15 poly voices (voices 2..16) × 3 signals. Voice 1 (mono) leaves on the parent
// Monsoon's own outs, so it's not duplicated here. Written by the parent Monsoon
// via the cached pointer (see MonsoonOutputGenerator), same as Straits.
namespace ChangiIds {
    enum OutputIds {
        // gate 0..14, cv 0..14, accent 0..14  (poly voices 2..16)
        GATE_OUT_0,   GATE_OUT_END   = GATE_OUT_0   + 14,
        CV_OUT_0,     CV_OUT_END     = CV_OUT_0     + 14,
        ACCENT_OUT_0, ACCENT_OUT_END = ACCENT_OUT_0 + 14,
        NUM_OUTPUTS
    };
}

struct MonsoonChangiExpander : Module {
    MonsoonChangiExpander() {
        config(0, 0, ChangiIds::NUM_OUTPUTS, 0);
        for (int i = 0; i < 15; ++i) {
            configOutput(ChangiIds::GATE_OUT_0   + i, "Voice " + std::to_string(i + 2) + " gate");
            configOutput(ChangiIds::CV_OUT_0     + i, "Voice " + std::to_string(i + 2) + " CV / pitch");
            configOutput(ChangiIds::ACCENT_OUT_0 + i, "Voice " + std::to_string(i + 2) + " accent gate");
        }
    }

    void process(const ProcessArgs& args) override {}
};
