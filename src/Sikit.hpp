#pragma once
// ── Sikit — the Tuning Expander (microtonal Phase 1) ─────────────────────────────────────────
// A lightweight, cents-only tuning expander: 12 per-degree CENTS knobs that RETUNE Monsoon's
// existing 12-degree system without touching the scale mask. Writes cents[] into the engine's
// shared dotModular::TuningTable (Monsoon's scale system keeps weight[]). See
// docs/design/TUNING_EXPANDER_SPEC.md + SIKIT_CLAUDE_CODE_GUIDE.md.
//
// Root (degree 0) is LOCKED at 0 cents (Scalar's rule): UI has no interactive knob for it and
// process() clamps it to 0 (belt-and-braces against preset/.scl writes). Equal-division default
// (cents[i] = i*100) reproduces 12-TET exactly, so a fresh/at-default Sikit is byte-identical.
//
// Sikit.hpp is intentionally free of Monsoon.hpp (no include cycle); Sikit.cpp pulls in Monsoon
// for the claim/publish + discovery wiring.

#include <rack.hpp>
#include <string>

namespace SikitIds {
    static constexpr int N_DEGREES = 12;
    enum ParamIds {
        CENTS_PARAM_0,                                   // degree 0 = C = root, LOCKED to 0
        CENTS_PARAM_END = CENTS_PARAM_0 + N_DEGREES - 1, // degree 11 = B
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS  = 0 };   // no CV inputs in v1
    enum OutputIds { NUM_OUTPUTS = 0 };   // publishes via the shared TuningTable, not jacks
    enum LightIds  { NUM_LIGHTS  = 0 };

    // Standard sharp note names for the 12 degrees (labels only; meaningful because Sikit stays
    // near 12-TET). v1: sharps only (flat variant deferred).
    static inline const char* noteName(int i) {
        static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return N[((i % 12) + 12) % 12];
    }
}

struct Sikit : rack::engine::Module {
    // Equal-division default for degree i: i*100 cents (0,100,…,1100). SIKIT_CLAUDE_CODE_GUIDE §Cents.
    static float defaultCents(int i) { return (float)i * 100.f; }

    // Display-only name of the currently-loaded .scl (its description, else the file stem). Empty =
    // no file loaded (manual/default tuning). Set by the widget's loader; drawn on the name band;
    // persisted so a saved patch shows what was loaded. NOT read by the engine.
    std::string loadedTuningName;

    Sikit() {
        config(SikitIds::NUM_PARAMS, SikitIds::NUM_INPUTS, SikitIds::NUM_OUTPUTS, SikitIds::NUM_LIGHTS);
        for (int i = 0; i < SikitIds::N_DEGREES; ++i) {
            configParam(SikitIds::CENTS_PARAM_0 + i, 0.f, 1200.f, defaultCents(i),
                        std::string("Cents (") + SikitIds::noteName(i) + ")", " cents");
        }
    }

    // Defined in Sikit.cpp (needs Monsoon for claim + publish).
    void process(const ProcessArgs& args) override;

    json_t* dataToJson() override {
        json_t* root = json_object();
        if (!loadedTuningName.empty())
            json_object_set_new(root, "loadedTuningName", json_string(loadedTuningName.c_str()));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "loadedTuningName"))
            loadedTuningName = json_string_value(j);
    }
};
