#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// ── Changi T2 — per-voice ARTICULATION output expander (transit family) ──────
// The second Changi terminal. Where Changi T1 breaks out voice IDENTITY signals
// (gate / CV / accent — what each voice IS), T2 breaks out the PHRASING signals
// (how each voice departs): per-voice STEP GATE and STEP LEGATO gate.
//
// 16 voices × 2 signals. Index 0 = MONO (voice 1); indices 1..15 = poly voices
// 2..16. Mirrors Straits' poly-cable channel layout (ch0 = mono). Written by the
// parent Monsoon via the cached pointer (see MonsoonOutputGenerator), same as T1.
//
//   STEP GATE   = the un-fused gate: legato/tie removed, every sub-note articulated
//                 (Straits POLY_STEP_GATE per voice).
//   STEP LEGATO = STEP GATE masked to slurred notes only, silent on isolated notes
//                 (Straits POLY_STEP_LEGATO per voice).
//
// Poly data requires Straits (the poly prerequisite that owns voice count); the
// mono strand at index 0 works standalone. Passive jack-holder like T1 — no
// process(); the host writes from the shared precomputed poly voltages.
namespace ChangiT2Ids {
    enum OutputIds {
        // 16 per group: index 0 = MONO (voice 1), 1..15 = poly voices 2..16.
        STEP_GATE_OUT_0,   STEP_GATE_OUT_END   = STEP_GATE_OUT_0   + 15,
        STEP_LEGATO_OUT_0, STEP_LEGATO_OUT_END = STEP_LEGATO_OUT_0 + 15,
        NUM_OUTPUTS
    };
}

struct MonsoonChangiT2Expander : Module {
    MonsoonChangiT2Expander() {
        config(0, 0, ChangiT2Ids::NUM_OUTPUTS, 0);
        // index 0 = MONO (voice 1); indices 1..15 = poly voices 2..16.
        configOutput(ChangiT2Ids::STEP_GATE_OUT_0,   "Mono (voice 1) step gate");
        configOutput(ChangiT2Ids::STEP_LEGATO_OUT_0, "Mono (voice 1) step legato gate");
        for (int i = 1; i < 16; ++i) {
            configOutput(ChangiT2Ids::STEP_GATE_OUT_0   + i, "Voice " + std::to_string(i + 1) + " step gate");
            configOutput(ChangiT2Ids::STEP_LEGATO_OUT_0 + i, "Voice " + std::to_string(i + 1) + " step legato gate");
        }
    }

    void process(const ProcessArgs& args) override {}
};
