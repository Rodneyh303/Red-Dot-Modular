#pragma once
#include <rack.hpp>
#include "ui/ModuleLookupCore.hpp"   // THE lookup authority: the topology epoch (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START)
#include "Monsoon.hpp"

using namespace rack;

// ── Changi T1 — per-voice output expander (transit family) ───────────────────
// "Departures" — every voice leaves as an individual mono jack. Breaks the poly
// cable out into per-voice GATE / CV / ACCENT jacks, for patching each voice to a
// separate destination (the counterpart to Straits' single poly cables).
//
// 16 voices × 3 signals. Index 0 = MONO (voice 1); indices 1..15 = poly voices
// 2..16. Mirrors Straits' poly-cable channel layout (ch0 = mono). Written by the
// parent Monsoon via the cached pointer (see MonsoonOutputGenerator), same as
// Straits.
namespace ChangiIds {
    enum OutputIds {
        // 16 per group: index 0 = MONO (voice 1), 1..15 = poly voices 2..16.
        GATE_OUT_0,   GATE_OUT_END   = GATE_OUT_0   + 15,
        CV_OUT_0,     CV_OUT_END     = CV_OUT_0     + 15,
        ACCENT_OUT_0, ACCENT_OUT_END = ACCENT_OUT_0 + 15,
        NUM_OUTPUTS
    };
}

struct MonsoonChangiExpander : Module {
    // THE LOOKUP AUTHORITY: the event bumps the global topology epoch — adjacency
    // (the lookup* memos) re-resolves on the next read.
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
    }

    MonsoonChangiExpander() {
        config(0, 0, ChangiIds::NUM_OUTPUTS, 0);
        // index 0 = MONO (voice 1); indices 1..15 = poly voices 2..16.
        configOutput(ChangiIds::GATE_OUT_0,   "Mono (voice 1) gate");
        configOutput(ChangiIds::CV_OUT_0,     "Mono (voice 1) CV / pitch");
        configOutput(ChangiIds::ACCENT_OUT_0, "Mono (voice 1) accent gate");
        for (int i = 1; i < 16; ++i) {
            configOutput(ChangiIds::GATE_OUT_0   + i, "Voice " + std::to_string(i + 1) + " gate");
            configOutput(ChangiIds::CV_OUT_0     + i, "Voice " + std::to_string(i + 1) + " CV / pitch");
            configOutput(ChangiIds::ACCENT_OUT_0 + i, "Voice " + std::to_string(i + 1) + " accent gate");
        }
    }

    void process(const ProcessArgs& args) override {}
};
