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

#include <rack.hpp>
#include "dsp/VoiceResolver.hpp"
#include "ui/VisualExpanderHelpers.hpp"   // findMonsoonEitherSide

using namespace rack;
namespace redDot { struct Monsoon; }

// ---- Id enum (declared for shape; config() reserves ZERO params  de-parammed) ----
struct IntertropicalIds {
    enum ParamId  {
        // 8 per-output TRANSPOSE knobs (+/-24 semis, detented). Real params: values the user
        // sets, DAW-automatable/CV-able -- unlike the store-backed grid (display state, no param).
        TRANSPOSE_0, TRANSPOSE_1, TRANSPOSE_2, TRANSPOSE_3,
        TRANSPOSE_4, TRANSPOSE_5, TRANSPOSE_6, TRANSPOSE_7,
        NUM_PARAMS
    };
    static constexpr int TRANSPOSE_FIRST = TRANSPOSE_0;   // TRANSPOSE_0..7 == output 0..7
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
    static constexpr int N_VOICES  = 16;       // 16 voices to choose FROM
    static constexpr int MAX_VOICES_PER_SCENE = 8;  // max voices routed per scene (≤8 output channels)
    static constexpr int MAX_REPEAT = 8;
};

struct Intertropical : Module {
    using Ids = IntertropicalIds;

    // ---- STORE (persisted; store-backed, no params) ----
    // sceneMask[scene] : bit v set => voice v (0..15) is IN this scene.
    uint16_t sceneMask[Ids::N_SCENES]   = {0};
    // repeats[scene]   : 1..8 boundary crossings the scene holds before advancing.
    uint8_t  repeats[Ids::N_SCENES]     = {1,1,1,1,1,1,1,1};
    // loopLen: how many scenes to cycle through (1..8). Scene advance wraps at loopLen, not N_SCENES.
    uint8_t  loopLen                    = Ids::N_SCENES;   // default: all 8 scenes
    // sceneOutput[scene][voice]: optional per-voice output channel override.
    // -1 = auto (auto-pack to next available ch0..7), 0..7 = forced to that output channel.
    int8_t   sceneOutput[Ids::N_SCENES][Ids::N_VOICES];
    // Constructor initialises sceneOutput to all -1 (auto-pack by default).
    Intertropical();

    // ---- store accessors (the widget binds these; keeps widget off raw fields) ----
    bool getCell(int scene, int voice) const {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return false;
        return (sceneMask[scene] >> voice) & 1u;
    }
    void setCell(int scene, int voice, bool on) {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return;
        if (on) {
            // Enforce max 8 voices per scene.
            int count = 0;
            for (int b = 0; b < Ids::N_VOICES; ++b) if ((sceneMask[scene] >> b) & 1u) count++;
            if (count >= Ids::MAX_VOICES_PER_SCENE) return;
            sceneMask[scene] |=  (uint16_t)(1u << voice);
        } else {
            sceneMask[scene] &= (uint16_t)~(1u << voice);
            sceneOutput[scene][voice] = -1;  // clear override when voice leaves
        }
    }
    int getOutput(int scene, int voice) const {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return -1;
        return sceneOutput[scene][voice];
    }
    void cycleOutput(int scene, int voice) {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return;
        if (!getCell(scene, voice)) return;
        int cur = sceneOutput[scene][voice];
        cur = (cur + 1) % (Ids::MAX_VOICES_PER_SCENE + 1);
        sceneOutput[scene][voice] = (cur == Ids::MAX_VOICES_PER_SCENE) ? -1 : (int8_t)cur;
    }
    // Compute voice→output mapping for a scene. out[voice] = output ch (0..7) or -1.
    void computeRouting(int scene, int8_t out[Ids::N_VOICES]) const {
        for (int v = 0; v < Ids::N_VOICES; ++v) out[v] = -1;
        if (scene < 0 || scene >= Ids::N_SCENES) return;
        bool used[Ids::MAX_VOICES_PER_SCENE] = {};
        for (int v = 0; v < Ids::N_VOICES; ++v) {
            if ((sceneMask[scene] >> v) & 1u) {
                int8_t o = sceneOutput[scene][v];
                if (o >= 0 && o < Ids::MAX_VOICES_PER_SCENE && !used[o]) {
                    out[v] = o;  used[o] = true;
                }
            }
        }
        int nextCh = 0;
        for (int v = 0; v < Ids::N_VOICES; ++v) {
            if ((sceneMask[scene] >> v) & 1u) {
                if (out[v] >= 0) continue;
                while (nextCh < Ids::MAX_VOICES_PER_SCENE && used[nextCh]) nextCh++;
                if (nextCh < Ids::MAX_VOICES_PER_SCENE) {
                    out[v] = (int8_t)nextCh;  used[nextCh] = true;  nextCh++;
                }
            }
        }
    }
    int  getRepeats(int scene) const {
        return (scene >= 0 && scene < Ids::N_SCENES) ? repeats[scene] : 1;
    }
    void setRepeats(int scene, int n) {
        if (scene >= 0 && scene < Ids::N_SCENES)
            repeats[scene] = (uint8_t)rack::math::clamp(n, 1, Ids::MAX_REPEAT);
    }
    int  getLoopLen() const { return loopLen; }
    void setLoopLen(int n) { loopLen = (uint8_t)rack::math::clamp(n, 1, Ids::N_SCENES); }

    // ---- live playback state (NOT persisted  derived from host phase) ----
    int   activeScene   = 0;     // which scene is currently sounding
    int   repeatPos     = 0;     // 0..repeats[activeScene]-1  which repeat we're on
    int   lastStepIndex = -1;    // last stepIndex seen (for step-change detection)
    int   stepCounter   = 0;     // steps counted toward the current pattern cycle
    uint16_t liveMask   = 0;     // membership sampled AT the boundary (read-at-boundary rule)

    // (Constructor moved to .cpp — initialises sceneOutput to -1 + config)

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* m = json_array();
        for (int s = 0; s < Ids::N_SCENES; ++s) json_array_append_new(m, json_integer(sceneMask[s]));
        json_object_set_new(root, "sceneMask", m);
        json_t* r = json_array();
        for (int s = 0; s < Ids::N_SCENES; ++s) json_array_append_new(r, json_integer(repeats[s]));
        json_object_set_new(root, "repeats", r);
        json_object_set_new(root, "loopLen", json_integer(loopLen));
        json_t* o = json_array();
        for (int s = 0; s < Ids::N_SCENES; ++s)
            for (int v = 0; v < Ids::N_VOICES; ++v)
                json_array_append_new(o, json_integer(sceneOutput[s][v]));
        json_object_set_new(root, "sceneOutput", o);
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* m = json_object_get(root, "sceneMask"))
            for (int s = 0; s < Ids::N_SCENES; ++s)
                if (json_t* v = json_array_get(m, s)) sceneMask[s] = (uint16_t)json_integer_value(v);
        if (json_t* r = json_object_get(root, "repeats"))
            for (int s = 0; s < Ids::N_SCENES; ++s)
                if (json_t* v = json_array_get(r, s)) repeats[s] = (uint8_t)json_integer_value(v);
        if (json_t* ll = json_object_get(root, "loopLen")) loopLen = (uint8_t)json_integer_value(ll);
        if (json_t* o = json_object_get(root, "sceneOutput")) {
            int idx = 0;
            for (int s = 0; s < Ids::N_SCENES; ++s)
                for (int v = 0; v < Ids::N_VOICES; ++v)
                    if (json_t* jv = json_array_get(o, idx++)) sceneOutput[s][v] = (int8_t)json_integer_value(jv);
        }
    }

    // ---- STUBBED: advance on boundary crossing, route active scene to poly outs ----
    void process(const ProcessArgs& args) override;
};
