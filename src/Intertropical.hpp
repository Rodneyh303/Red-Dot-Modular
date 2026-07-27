#pragma once
// Intertropical  scene sequencer (arrangement layer over Monsoon's generation).
//
// SCAFFOLD. See docs/design/INTERTROPICAL_SPEC.md. A graphical sequential switch scoped to
// VOICES: 8 scenes x 16-voice opt-in masks + per-scene repeat count; routes the active scene's
// voices to poly GATE/CV/ACCENT/LEGATO/SLEG outs. Scene advances on phrase-boundary CROSSINGS
// (one phase cycle = 16 steps; scene boundary at the cycle edge). De-parammed from the start:
// the scene grid + repeat counts live in the STORE, not host params. LIVE under lock mode
// (output/arrangement layer, downstream of generation). Scene membership READ at the boundary.
//
// This header defines the STORE SCHEMA + module skeleton. Routing/advance logic is stubbed.

#include "plugin.hpp"
#include "dsp/VoiceResolver.hpp"

namespace redDot { struct Monsoon; }

// ---- Id enum (declared for shape; config() reserves ZERO params  de-parammed) ----
struct IntertropicalIds {
    enum ParamId  { NUM_PARAMS };                 // none: store-backed grid, no host params
    enum InputId  {
        PHASE_IN,                                 // optional external phase (else host bus)
        NUM_INPUTS
    };
    enum OutputId {
        GATE_OUT, CV_OUT, ACCENT_OUT, LEGATO_OUT, SLEG_OUT,   // poly (16ch)
        NUM_OUTPUTS
    };
    enum LightId  { NUM_LIGHTS };

    static constexpr int N_SCENES  = 8;
    static constexpr int N_VOICES  = 16;
    static constexpr int MAX_REPEAT = 8;
};

struct Intertropical : Module {
    using Ids = IntertropicalIds;

    // ---- STORE (persisted; store-backed, no params) ----
    // sceneMask[scene] : bit v set => voice v (0..15) is IN this scene.
    uint16_t sceneMask[Ids::N_SCENES]   = {0};
    // repeats[scene]   : 1..8 boundary crossings the scene holds before advancing.
    uint8_t  repeats[Ids::N_SCENES]     = {1,1,1,1,1,1,1,1};

    // ---- store accessors (the widget binds these; keeps widget off raw fields) ----
    bool getCell(int scene, int voice) const {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return false;
        return (sceneMask[scene] >> voice) & 1u;
    }
    void setCell(int scene, int voice, bool on) {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return;
        if (on) sceneMask[scene] |=  (uint16_t)(1u << voice);
        else    sceneMask[scene] &= (uint16_t)~(1u << voice);
    }
    int  getRepeats(int scene) const {
        return (scene >= 0 && scene < Ids::N_SCENES) ? repeats[scene] : 1;
    }
    void setRepeats(int scene, int n) {
        if (scene >= 0 && scene < Ids::N_SCENES)
            repeats[scene] = (uint8_t)rack::math::clamp(n, 1, Ids::MAX_REPEAT);
    }

    // ---- live playback state (NOT persisted  derived from host phase) ----
    int   activeScene   = 0;     // which scene is currently sounding
    int   repeatPos     = 0;     // 0..repeats[activeScene]-1  which repeat we're on
    int   lastBoundary  = -1;    // last phrase-boundary index seen (for crossing detection)
    uint16_t liveMask   = 0;     // membership sampled AT the boundary (read-at-boundary rule)

    Intertropical() {
        // De-parammed: ZERO host params. Store-backed grid + repeats.
        config(Ids::NUM_PARAMS, Ids::NUM_INPUTS, Ids::NUM_OUTPUTS, Ids::NUM_LIGHTS);
        configInput(Ids::PHASE_IN, "Phase (optional; else reads host)");
        configOutput(Ids::GATE_OUT,   "Gate (poly, active scene)");
        configOutput(Ids::CV_OUT,     "CV (poly, active scene)");
        configOutput(Ids::ACCENT_OUT, "Accent (poly, active scene)");
        configOutput(Ids::LEGATO_OUT, "Legato (poly, active scene)");
        configOutput(Ids::SLEG_OUT,   "SLEG (poly, active scene)");
    }

    // ---- persistence (store-backed  serialise the schema) ----
    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* m = json_array();
        for (int s = 0; s < Ids::N_SCENES; ++s) json_array_append_new(m, json_integer(sceneMask[s]));
        json_object_set_new(root, "sceneMask", m);
        json_t* r = json_array();
        for (int s = 0; s < Ids::N_SCENES; ++s) json_array_append_new(r, json_integer(repeats[s]));
        json_object_set_new(root, "repeats", r);
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* m = json_object_get(root, "sceneMask"))
            for (int s = 0; s < Ids::N_SCENES; ++s)
                if (json_t* v = json_array_get(m, s)) sceneMask[s] = (uint16_t)json_integer_value(v);
        if (json_t* r = json_object_get(root, "repeats"))
            for (int s = 0; s < Ids::N_SCENES; ++s)
                if (json_t* v = json_array_get(r, s)) repeats[s] = (uint8_t)json_integer_value(v);
    }

    // ---- STUBBED: advance on boundary crossing, route active scene to poly outs ----
    void process(const ProcessArgs& args) override;
};
