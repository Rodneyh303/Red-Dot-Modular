#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"
#include "dsp/ScaleList.hpp"

using namespace rack;

// ── Shophouse — scale expander (the 12th module) ─────────────────────────────
// Breaks scale-setting OUT of Monsoon's context menu into a direct, menu-free
// panel: the whole point is that setting a scale is faster than the 3-right-click
// menu dance (scale / root / lock).
//
// FOUR "shophouse fronts" (the scale-list street), each = one (scale, root) slot:
//   - SCALE knob per front → steps Major / Dorian / Minor / ... (name shown on facade).
//   - ROOT set by CLICKING a shutter on that front's piano-octave facade (the display
//     IS the control; the clicked key becomes the root, accented red). Stored per front.
// A CV input, SAMPLED AT THE PHRASE BOUNDARY, selects the active front (list index).
// One CONSERVATION toggle (guide vs enforce) replaces the context-menu lock.
//
// The scale/root the engine reads only changes at the phrase boundary (ScaleList
// commitAtBoundary), so mode changes land on the loop edge — never a mid-phrase jolt.
// Shophouse WRITES the active (scale, root) into Monsoon's existing ScaleManager slot;
// no new engine path (getSemitoneWeight already gates out-of-scale to zero when enforced).
namespace ShophouseIds {
    static constexpr int NUM_FRONTS = 4;
    enum ParamIds {
        SCALE_PARAM_0, SCALE_PARAM_END = SCALE_PARAM_0 + NUM_FRONTS - 1,   // scale index per front
        ROOT_PARAM_0,  ROOT_PARAM_END  = ROOT_PARAM_0  + NUM_FRONTS - 1,   // root 0..11 per front (set by shutter click)
        CONSERVATION_PARAM,                                                 // 0=guide, 1=enforce
        NUM_PARAMS
    };
    enum InputIds {
        INDEX_CV_INPUT,   // CV → active front index (sampled at phrase boundary)
        NUM_INPUTS
    };
}

struct MonsoonShophouseExpander : Module {
    ScaleList list{ShophouseIds::NUM_FRONTS};

    MonsoonShophouseExpander() {
        using namespace ShophouseIds;
        config(ShophouseIds::NUM_PARAMS, ShophouseIds::NUM_INPUTS, 0, 0);
        int nScales = 1;
        // MONSOON_SCALES size is known at runtime; configure a generous max, snapped.
        for (int f = 0; f < NUM_FRONTS; ++f) {
            // 0 = Chromatic (no restriction) .. 23 = last scale in MONSOON_SCALES.
            configParam(SCALE_PARAM_0 + f, 0.f, 23.f, (f == 0 ? 1.f : 0.f),
                        "Front " + std::to_string(f + 1) + " scale");
            paramQuantities[SCALE_PARAM_0 + f]->snapEnabled = true;
            configParam(ROOT_PARAM_0 + f, 0.f, 11.f, 0.f,
                        "Front " + std::to_string(f + 1) + " root");
            paramQuantities[ROOT_PARAM_0 + f]->snapEnabled = true;
        }
        (void)nScales;
        configSwitch(CONSERVATION_PARAM, 0.f, 1.f, 0.f, "Conservation", {"Guide", "Enforce"});
        configInput(INDEX_CV_INPUT, "Scale-list index CV (sampled at phrase boundary)");
    }

    // Sync the per-front params into the ScaleList entries (called from Monsoon each frame,
    // or here). Kept simple: the widget writes params; Monsoon reads list + drives commit.
    void syncEntriesFromParams() {
        using namespace ShophouseIds;
        for (int f = 0; f < NUM_FRONTS; ++f) {
            int sc = (int)std::round(params[SCALE_PARAM_0 + f].getValue());
            int rt = (int)std::round(params[ROOT_PARAM_0 + f].getValue());
            list.setEntry(f, sc, rt);
        }
    }

    void process(const ProcessArgs& args) override {}

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "pending", json_integer(list.pending()));
        json_object_set_new(root, "active",  json_integer(list.active()));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* p = json_object_get(root, "pending")) list.setPending((int)json_integer_value(p));
    }
};
