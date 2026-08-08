#pragma once
// ── Monsoon Micro-12 — tuning + scale AUTHORING expander (microtonal Phase 2, Model A) ──────────
// The first AUTHORING tuning expander: 12 per-degree strips, each = a WEIGHT fader (the scale mask;
// 0 = degree disabled) + a CENTS knob (the tuning; equal-division default, drag to detune). When
// attached to Monsoon it CLAIMS the tuning source and publishes BOTH cents[] AND weight[] into the
// engine's shared TuningTable — so Monsoon's own note faders grey out and delegate (Model A, Rodney).
// Distinct from Sikit (which is cents-only, Model B, and never writes weight[]).
//
// Root (degree 0): cents LOCKED at 0 (Scalar rule) — no cents knob, engine clamps to 0. Its WEIGHT
// fader is still live (the root is a normal scale degree you can enable/disable/weight).
//
// See MONSOON_MICRO_SPEC.md, MONSOON_MICRO_CLAUDE_CODE_GUIDE.md. Header stays free of Monsoon.hpp
// (no include cycle); MonsoonMicro12.cpp pulls in Monsoon for the claim/publish + discovery wiring.

#include <rack.hpp>
#include <string>

namespace Micro12Ids {
    static constexpr int N_DEGREES = 12;
    enum ParamIds {
        WEIGHT_PARAM_0,                                    // degree 0..11 scale-weight faders (0=disabled)
        WEIGHT_PARAM_END = WEIGHT_PARAM_0 + N_DEGREES - 1,
        CENTS_PARAM_0,                                     // degree 0 = root, LOCKED to 0 (no knob)
        CENTS_PARAM_END = CENTS_PARAM_0 + N_DEGREES - 1,
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS  = 0 };   // no CV inputs in v1 (Interchange modulates via pairing, Phase 3)
    enum OutputIds { NUM_OUTPUTS = 0 };   // publishes via the shared TuningTable, not jacks
    // Per-degree fader FLASH light — 2 sub-lights each (GreenRedLight: green + red), so the fader
    // lights when its degree PLAYS, exactly like Monsoon's SEMI_LED bank (2*i green, 2*i+1 red).
    enum LightIds  {
        WEIGHT_LED_START,
        WEIGHT_LED_END = WEIGHT_LED_START + 2 * N_DEGREES - 1,
        NUM_LIGHTS
    };

    static inline const char* noteName(int i) {
        static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return N[((i % 12) + 12) % 12];
    }
}

struct MonsoonMicro12 : rack::engine::Module {
    // Equal-division default for degree i: i*100 cents (0,100,…,1100) — reproduces 12-TET exactly.
    static float defaultCents(int i) { return (float)i * 100.f; }

    // Display-only name of a loaded .scl (description or file stem). Persisted; drawn on menu.
    std::string loadedTuningName;

    MonsoonMicro12() {
        config(Micro12Ids::NUM_PARAMS, Micro12Ids::NUM_INPUTS, Micro12Ids::NUM_OUTPUTS, Micro12Ids::NUM_LIGHTS);
        for (int i = 0; i < Micro12Ids::N_DEGREES; ++i) {
            // WEIGHT fader: 0..1, default 1 (all degrees enabled at full weight = chromatic, which
            // reproduces Monsoon's default all-faders-up state → byte-identical at equal-division cents).
            configParam(Micro12Ids::WEIGHT_PARAM_0 + i, 0.f, 1.f, 1.f,
                        std::string("Weight (") + Micro12Ids::noteName(i) + ")");
            // CENTS knob: 0..1200, equal-division default.
            configParam(Micro12Ids::CENTS_PARAM_0 + i, 0.f, 1200.f, defaultCents(i),
                        std::string("Cents (") + Micro12Ids::noteName(i) + ")", " cents");
        }
    }

    // Defined in MonsoonMicro12.cpp (needs Monsoon for claim + publish).
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
