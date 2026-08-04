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
// (no Monsoon forward-decl: the real Monsoon is a GLOBAL type from Monsoon.hpp,
//  included by the .cpp before use. A "namespace redDot { struct Monsoon; }" here
//  would declare a PHANTOM redDot::Monsoon that mismatches the global one.)

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
    static constexpr int MAX_REPEAT = 4;   // max repeats per scene (panel has 4 sub-segs)
};

struct Intertropical : Module {
    using Ids = IntertropicalIds;

    // ---- STORE (persisted; store-backed, no params) ----
    // sceneMask[scene] : bit v set => voice v (0..15) is IN this scene.
    uint16_t sceneMask[Ids::N_SCENES]   = {0};
    // repeats[scene]   : 1..4 boundary crossings the scene holds before advancing.
    uint8_t  repeats[Ids::N_SCENES]     = {1,1,1,1,1,1,1,1};
    // loopLen: how many scenes to cycle through (1..8). Scene advance wraps at loopLen, not N_SCENES.
    uint8_t  loopLen                    = Ids::N_SCENES;   // default: all 8 scenes
    // --- Routing model (SETTLED, INTERTROPICAL_SPEC "Routing model + fan-out") ---
    // Three spaces (voices 16 -> slots <=8 -> outputs 8), two mappings:
    //  voice -> slot : PER-SCENE seating (this array). -1 = auto-pack, 0..7 = seated in that slot.
    //  slot  -> output : GLOBAL bitmask (slotOutput). A slot drives every output whose bit is set;
    //                    fan-out = >1 bit. Set once for the instance (the parts).
    // This replaces the old flattened sceneOutput[scene][voice]=output (which tangled the two
    // mappings and couldn't express fan-out). sceneSlots is the SAME array re-meaned: voice->SLOT.
    int8_t   sceneSlots[Ids::N_SCENES][Ids::N_VOICES];   // per-scene voice->slot (-1 auto, 0..7 seat)
    uint8_t  slotOutput[Ids::MAX_VOICES_PER_SCENE];      // global slot->output 8-bit mask (fan-out)
    // Tie-latched per-output transpose (semitones). What actually reaches CV_OUT: held constant across
    // a TRUE TIE (so a tied note's pitch can't slide when transpose is edited/modulated), re-captured
    // from the live knob on any non-tie. Lantern reads this so its piano-roll shows the sounding pitch.
    float    effectiveTranspose[Ids::MAX_VOICES_PER_SCENE];
    float    transposeForOutput(int ch) const {
        return (ch >= 0 && ch < Ids::MAX_VOICES_PER_SCENE) ? effectiveTranspose[ch] : 0.f;
    }
    // Constructor: sceneSlots all -1 (auto-pack), slotOutput identity permutation (slot i -> out i).
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
            sceneSlots[scene][voice] = -1;  // clear slot seating when voice leaves
        }
    }
    // voice -> SLOT seating (per scene). -1 = auto-pack, 0..7 = explicitly seated in that slot.
    int getSlot(int scene, int voice) const {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return -1;
        return sceneSlots[scene][voice];
    }
    void cycleSlot(int scene, int voice) {
        if (scene < 0 || scene >= Ids::N_SCENES || voice < 0 || voice >= Ids::N_VOICES) return;
        if (!getCell(scene, voice)) return;
        int cur = sceneSlots[scene][voice];
        cur = (cur + 1) % (Ids::MAX_VOICES_PER_SCENE + 1);   // 0..7 then wrap to -1 (auto)
        sceneSlots[scene][voice] = (cur == Ids::MAX_VOICES_PER_SCENE) ? -1 : (int8_t)cur;
    }
    // slot -> output GLOBAL bitmask (fan-out capable). Toggle one output bit for a slot.
    void toggleSlotOutput(int slot, int output) {
        if (slot < 0 || slot >= Ids::MAX_VOICES_PER_SCENE || output < 0 || output >= Ids::MAX_VOICES_PER_SCENE) return;
        const bool wasSet = (slotOutput[slot] >> output) & 1u;
        if (wasSet) {
            slotOutput[slot] &= (uint8_t)~(1u << output);   // simple clear
        } else {
            // SETTING: enforce single-slot-per-output (no fan-IN). Each output is driven by at most
            // one slot, so the output->slot inverse map is a true function (Lantern traces it
            // unambiguously). Fan-OUT is still allowed (a slot may light several outputs = several
            // bits in ITS row); only the COLUMN is exclusive. Clear this output's bit from every
            // other slot, then set it here.
            for (int s = 0; s < Ids::MAX_VOICES_PER_SCENE; ++s)
                slotOutput[s] &= (uint8_t)~(1u << output);
            slotOutput[slot] |= (uint8_t)(1u << output);
        }
    }
    bool getSlotOutput(int slot, int output) const {
        if (slot < 0 || slot >= Ids::MAX_VOICES_PER_SCENE || output < 0 || output >= Ids::MAX_VOICES_PER_SCENE) return false;
        return (slotOutput[slot] >> output) & 1u;
    }
    // Compute voice -> SLOT seating for a scene: slotOf[voice] = slot (0..7) or -1 (not seated).
    // Explicit seatings first (honoured where free), then auto-pack the rest in voice order.
    void computeSlots(int scene, int8_t slotOf[Ids::N_VOICES]) const {
        for (int v = 0; v < Ids::N_VOICES; ++v) slotOf[v] = -1;
        if (scene < 0 || scene >= Ids::N_SCENES) return;
        bool used[Ids::MAX_VOICES_PER_SCENE] = {};
        for (int v = 0; v < Ids::N_VOICES; ++v) {
            if ((sceneMask[scene] >> v) & 1u) {
                int8_t s = sceneSlots[scene][v];
                if (s >= 0 && s < Ids::MAX_VOICES_PER_SCENE && !used[s]) { slotOf[v] = s; used[s] = true; }
            }
        }
        int nextSlot = 0;
        for (int v = 0; v < Ids::N_VOICES; ++v) {
            if ((sceneMask[scene] >> v) & 1u) {
                if (slotOf[v] >= 0) continue;
                while (nextSlot < Ids::MAX_VOICES_PER_SCENE && used[nextSlot]) nextSlot++;
                if (nextSlot < Ids::MAX_VOICES_PER_SCENE) { slotOf[v] = (int8_t)nextSlot; used[nextSlot] = true; nextSlot++; }
            }
        }
    }
    // Compute voice→output mapping for a scene. out[voice] = output ch (0..7) or -1.
    // Voice -> primary output channel, for a scene (backward-compatible single-output view used by
    // the process path's channel-count sizing and by simple 1:1 setups). Composes the two mappings:
    // voice -> slot (computeSlots) then slot -> output (slotOutput mask, LOWEST set bit = primary).
    // Fan-out (a slot driving >1 output) is expanded separately by routedOutputsForVoice().
    void computeRouting(int scene, int8_t out[Ids::N_VOICES]) const {
        int8_t slotOf[Ids::N_VOICES];
        computeSlots(scene, slotOf);
        for (int v = 0; v < Ids::N_VOICES; ++v) {
            out[v] = -1;
            const int s = slotOf[v];
            if (s < 0) continue;
            const uint8_t mask = slotOutput[s];
            for (int o = 0; o < Ids::MAX_VOICES_PER_SCENE; ++o)
                if ((mask >> o) & 1u) { out[v] = (int8_t)o; break; }   // lowest set bit = primary
        }
    }
    // Full fan-out expansion: the output-channel bitmask a voice drives this scene (voice->slot->mask).
    uint8_t routedOutputsForVoice(int scene, int voice) const {
        int8_t slotOf[Ids::N_VOICES];
        computeSlots(scene, slotOf);
        const int s = (voice >= 0 && voice < Ids::N_VOICES) ? slotOf[voice] : -1;
        return (s >= 0) ? slotOutput[s] : 0;
    }
    // INVERSE map for Lantern (output -> slot -> global voice), per scene. Single-slot-per-output is
    // enforced (toggleSlotOutput), so each output is driven by <=1 slot -> unambiguous. Returns the
    // global voice index feeding this output channel, or -1 if the output is unrouted this scene.
    int voiceForOutput(int scene, int output) const {
        if (output < 0 || output >= Ids::MAX_VOICES_PER_SCENE) return -1;
        int slot = -1;
        for (int s = 0; s < Ids::MAX_VOICES_PER_SCENE; ++s)
            if ((slotOutput[s] >> output) & 1u) { slot = s; break; }   // <=1 by enforcement
        if (slot < 0) return -1;
        int8_t slotOf[Ids::N_VOICES];
        computeSlots(scene, slotOf);
        for (int v = 0; v < Ids::N_VOICES; ++v) if (slotOf[v] == slot) return v;
        return -1;   // slot empty this scene
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
        json_t* ss = json_array();
        for (int s = 0; s < Ids::N_SCENES; ++s)
            for (int v = 0; v < Ids::N_VOICES; ++v)
                json_array_append_new(ss, json_integer(sceneSlots[s][v]));
        json_object_set_new(root, "sceneSlots", ss);
        json_t* so = json_array();
        for (int s = 0; s < Ids::MAX_VOICES_PER_SCENE; ++s)
            json_array_append_new(so, json_integer(slotOutput[s]));
        json_object_set_new(root, "slotOutput", so);
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
        // New split model.
        if (json_t* ss = json_object_get(root, "sceneSlots")) {
            int idx = 0;
            for (int s = 0; s < Ids::N_SCENES; ++s)
                for (int v = 0; v < Ids::N_VOICES; ++v)
                    if (json_t* jv = json_array_get(ss, idx++)) sceneSlots[s][v] = (int8_t)json_integer_value(jv);
        } else if (json_t* o = json_object_get(root, "sceneOutput")) {
            // MIGRATION: old flattened sceneOutput[scene][voice]=output. Its per-scene overrides map
            // most closely to voice->SLOT seatings (the identity slotOutput default then sends slot i
            // -> output i, preserving the old effective routing for the common 1:1 case).
            int idx = 0;
            for (int s = 0; s < Ids::N_SCENES; ++s)
                for (int v = 0; v < Ids::N_VOICES; ++v)
                    if (json_t* jv = json_array_get(o, idx++)) sceneSlots[s][v] = (int8_t)json_integer_value(jv);
        }
        if (json_t* so = json_object_get(root, "slotOutput"))
            for (int s = 0; s < Ids::MAX_VOICES_PER_SCENE; ++s)
                if (json_t* v = json_array_get(so, s)) slotOutput[s] = (uint8_t)json_integer_value(v);
    }

    // ---- STUBBED: advance on boundary crossing, route active scene to poly outs ----
    void process(const ProcessArgs& args) override;
};
