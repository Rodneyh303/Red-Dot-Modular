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
        INDEX_CV_ATT_PARAM,                                                 // attenuverter for INDEX_CV_INPUT
        NUM_PARAMS
    };
    enum InputIds {
        INDEX_CV_INPUT,   // CV → active front index (sampled at phrase boundary)
        NUM_INPUTS
    };
}

struct MonsoonShophouseExpander : Module {
    ScaleList list{ShophouseIds::NUM_FRONTS};
    // MONSOON_SCALE_AUTHORING: per-front display name for a slot loaded with a user .dmtune (empty =
    // factory slot). The mask itself lives in list.entry(f).customMask; this is just the label.
    std::string slotName[ShophouseIds::NUM_FRONTS];

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
        configParam(INDEX_CV_ATT_PARAM, -1.f, 1.f, 1.f, "Index CV attenuverter");
        configInput(INDEX_CV_INPUT, "Scale-list index CV (sampled at phrase boundary)");
    }

    // Sync the per-front params into the ScaleList entries (called from Monsoon each frame,
    // or here). Kept simple: the widget writes params; Monsoon reads list + drives commit.
    void syncEntriesFromParams() {
        using namespace ShophouseIds;
        for (int f = 0; f < NUM_FRONTS; ++f) {
            // MONSOON_SCALE_AUTHORING: a slot loaded with a user .dmtune is CUSTOM — its scale/root
            // knobs don't apply, so don't clobber it from params (setEntry would clear isCustom every
            // frame). Only factory slots are param-driven. "Clear slot" reverts it to factory.
            if (list.entry(f).isCustom) continue;
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
        // MONSOON_SCALE_AUTHORING: persist per-front CUSTOM slots (loaded user .dmtune scale masks).
        // Factory slots are re-derived from params, so only custom slots need saving.
        json_t* slots = json_array();
        for (int f = 0; f < ShophouseIds::NUM_FRONTS; ++f) {
            json_t* js = json_object();
            const auto& e = list.entry(f);
            json_object_set_new(js, "custom", json_boolean(e.isCustom));
            if (e.isCustom) {
                json_object_set_new(js, "mask", json_integer(e.customMask));
                json_object_set_new(js, "transposable", json_boolean(e.customTransposable));
                json_object_set_new(js, "root", json_integer(e.root));
                if (!slotName[f].empty()) json_object_set_new(js, "name", json_string(slotName[f].c_str()));
            }
            json_array_append_new(slots, js);
        }
        json_object_set_new(root, "slots", slots);
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* p = json_object_get(root, "pending")) list.setPending((int)json_integer_value(p));
        if (json_t* slots = json_object_get(root, "slots"); json_is_array(slots)) {
            for (int f = 0; f < ShophouseIds::NUM_FRONTS && f < (int)json_array_size(slots); ++f) {
                json_t* js = json_array_get(slots, f);
                if (!json_is_object(js)) continue;
                json_t* jc = json_object_get(js, "custom");
                if (jc && json_boolean_value(jc)) {
                    uint16_t mask = 0x0FFF;
                    if (json_t* jm = json_object_get(js, "mask")) mask = (uint16_t)json_integer_value(jm);
                    bool tr = false;
                    if (json_t* jt = json_object_get(js, "transposable")) tr = json_boolean_value(jt);
                    list.setEntryCustom(f, mask, tr);
                    if (json_t* jr = json_object_get(js, "root"))          // restore the transposed root
                        params[ShophouseIds::ROOT_PARAM_0 + f].setValue((float)((json_integer_value(jr) % 12 + 12) % 12));
                    if (json_t* jn = json_object_get(js, "name"); json_is_string(jn)) slotName[f] = json_string_value(jn);
                }
            }
        }
    }
};
