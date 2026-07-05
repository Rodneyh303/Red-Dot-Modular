#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// ── Causeway — poly CV modulation expander (transit family) ──────────────────
// "The link across" — modulation across the poly voices. Two 16-CHANNEL POLY CV
// inputs (ch0=mono, ch1..15=poly voices 2..16), each scaled per voice by a
// per-voice attenuator PLUS a global attenuator. Effective scale for voice v =
// (perVoiceAtt[v] + globalAtt). Monsoon reads these per-channel and adds the
// scaled CV on top of the base REST/ACCENT knobs (on Straits).
//
// Params:
//   POLY_REST_MOD_ATT_1..15    per-voice REST attenuators (voices 2..16)
//   POLY_ACCENT_MOD_ATT_1..15  per-voice ACCENT attenuators
//   POLY_REST_MOD_ATT_GLOBAL   global REST attenuator (summed onto every voice)
//   POLY_ACCENT_MOD_ATT_GLOBAL global ACCENT attenuator
//   POLY_REST_CV_INPUT / POLY_ACCENT_CV_INPUT   16ch modulation CV in
struct MonsoonCausewayPolyExpander : Module {
    MonsoonCausewayPolyExpander() {
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, 0, 0);
        for (int i = 0; i < 15; ++i) {
            configParam(MonsoonIds::POLY_REST_MOD_ATT_1 + i,   -1.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " rest CV attenuator");
            configParam(MonsoonIds::POLY_ACCENT_MOD_ATT_1 + i, -1.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " accent CV attenuator");
        }
        configParam(MonsoonIds::POLY_REST_MOD_ATT_GLOBAL,   -1.f, 1.f, 0.f, "Global rest CV attenuator");
        configParam(MonsoonIds::POLY_ACCENT_MOD_ATT_GLOBAL, -1.f, 1.f, 0.f, "Global accent CV attenuator");
        configInput(MonsoonIds::POLY_REST_CV_INPUT,   "Poly rest modulation CV (16ch: ch1=mono)");
        configInput(MonsoonIds::POLY_ACCENT_CV_INPUT, "Poly accent modulation CV (16ch: ch1=mono)");
    }

    // Monsoon reads these inputs/attenuators via the cached pointer; nothing per-sample here.
    void process(const ProcessArgs& args) override {}
};
