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
// LOCAL id pool — re-homed out of the shared MonsoonIds pool so Causeway can be
// right-sized (was claiming all 152 param + all input slots for 34 params + 2 inputs).
// Names deliberately DIFFER from the MonsoonIds ones (…_ATT vs …_MOD_ATT): this file has
// `using namespace MonsoonIds` at file scope, so identical names would be ambiguous.
namespace CausewayIds {
    enum ParamIds {
        MONO_REST_ATT = 0,                                   // voice 1 (mono) rest
        MONO_ACCENT_ATT,                                     // voice 1 (mono) accent
        POLY_REST_ATT_START,                                 // 2 .. 16  (voices 2..16)
        POLY_ACCENT_ATT_START  = POLY_REST_ATT_START + 15,   // 17 .. 31
        POLY_REST_ATT_GLOBAL   = POLY_ACCENT_ATT_START + 15, // 32
        POLY_ACCENT_ATT_GLOBAL,                              // 33
        // Q-mix lane (full parity with REST/ACC): voice-1 mono attenuator + 15 per-voice poly
        // attenuators + a global. Modulates the q-mix THRESHOLD (POLY_QMIX_PARAM_* / QMIX_LEVEL_PARAM)
        // — the knob the engine compares against the q-mix probability draw from Sands. The CV is the
        // 16ch poly QMIX_CV_INPUT (ch0 = mono), attenuated exactly like rest/accent.
        MONO_QMIX_ATT,                                       // voice 1 (mono) q-mix
        POLY_QMIX_ATT_START  = POLY_ACCENT_ATT_GLOBAL + 1,   // 35 .. 49 (voices 2..16)
        POLY_QMIX_ATT_GLOBAL = POLY_QMIX_ATT_START + 15,     // 50
        NUM_PARAMS                                           // 51
    };
    enum InputIds { REST_CV_INPUT = 0, ACCENT_CV_INPUT, QMIX_CV_INPUT, NUM_INPUTS };
}

struct MonsoonCausewayPolyExpander : Module {
    MonsoonCausewayPolyExpander() {
        config(CausewayIds::NUM_PARAMS, CausewayIds::NUM_INPUTS, 0, 0);
        for (int i = 0; i < 15; ++i) {
            configParam(CausewayIds::POLY_REST_ATT_START + i,   -1.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " rest CV attenuator");
            configParam(CausewayIds::POLY_ACCENT_ATT_START + i, -1.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " accent CV attenuator");
            configParam(CausewayIds::POLY_QMIX_ATT_START + i,   -1.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " q-mix CV attenuator");
        }
        configParam(CausewayIds::POLY_REST_ATT_GLOBAL,   -1.f, 1.f, 0.f, "Global rest CV attenuator");
        configParam(CausewayIds::POLY_ACCENT_ATT_GLOBAL, -1.f, 1.f, 0.f, "Global accent CV attenuator");
        configParam(CausewayIds::POLY_QMIX_ATT_GLOBAL,   -1.f, 1.f, 0.f, "Global q-mix CV attenuator");
        configParam(CausewayIds::MONO_REST_ATT,   -1.f, 1.f, 0.f, "Voice 1 (mono) rest CV attenuator");
        configParam(CausewayIds::MONO_ACCENT_ATT, -1.f, 1.f, 0.f, "Voice 1 (mono) accent CV attenuator");
        configParam(CausewayIds::MONO_QMIX_ATT,   -1.f, 1.f, 0.f, "Voice 1 (mono) q-mix CV attenuator");
        configInput(CausewayIds::REST_CV_INPUT,   "Poly rest modulation CV (16ch: ch1=mono)");
        configInput(CausewayIds::ACCENT_CV_INPUT, "Poly accent modulation CV (16ch: ch1=mono)");
        configInput(CausewayIds::QMIX_CV_INPUT,  "Poly q-mix modulation CV (16ch: ch1=mono)");
    }

    // Monsoon reads these inputs/attenuators via the cached pointer; nothing per-sample here.
    void process(const ProcessArgs& args) override {}
};
