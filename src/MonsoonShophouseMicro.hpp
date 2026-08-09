#pragma once
// ── Shophouse Micro — tuning+scale SCENE modulation for the Colonnades family (SHOPHOUSE_MICRO_SPEC) ─
// The microtonal generalisation of Shophouse: instead of (scale, root) slots within 12-TET, it holds
// FOUR (12-mode) / TWO (24-mode) full-.dmtune tuning+scale FRONTS; a CV input sampled AT THE PHRASE
// BOUNDARY selects the active front, committed on the loop edge (never mid-phrase). Makes TUNING
// ITSELF a boundary-quantised modulation target.
//
// WRITER MODEL = Q (spec §45-52): the Colonnades/Duo remains the tuning AUTHORITY (owns tt.N, claims
// the tuning source). Shophouse Micro is a BOUND MODULATOR: it only drives its TuningList (sample CV →
// pending, commit at boundary). The actual TuningTable overwrite is FOLDED INTO the Colonnades/Duo
// publish (single-writer discipline) — that module asks "is a Shophouse Micro on my Monsoon with an
// active loaded front? then publish ITS cents[]+weight[] as the base instead of my faders" (Interchange
// CV then adds on top). So Shophouse Micro writes NO engine state itself; it maintains scene selection.
//
// 12/24 is an EXPLICIT MODULE MODE (spec §66): all fronts one N; mismatched .dmtune loads rejected.
// Connection informs the mode (host tt.N) but never silently wipes loaded slots.

#include <rack.hpp>
#include "Monsoon.hpp"
#include "dsp/TuningList.hpp"

using namespace rack;

namespace ShophouseMicroIds {
    static constexpr int MAX_FRONTS = 4;                 // 12-mode uses 4, 24-mode uses 2 (front count derived)
    enum ParamIds {
        CONSERVATION_PARAM,                              // 0 = guide, 1 = enforce (weight mask meaning)
        INDEX_CV_ATT_PARAM,                              // attenuverter for INDEX_CV
        NUM_PARAMS
    };
    enum InputIds {
        INDEX_CV_INPUT,                                  // CV → active-front index (sampled at phrase boundary)
        NUM_INPUTS
    };
}

struct MonsoonShophouseMicro : Module {
    // The tuning-slot street. degrees() = 12/24 mode; front count = 4 at 12, 2 at 24.
    TuningList list{4, 12};
    bool  modeConflict = false;    // host tt.N != loaded-slot N (flagged, never auto-wiped). Runtime-only.
    int   lastActive_  = 0;        // for the widget's active-front lantern

    MonsoonShophouseMicro() {
        using namespace ShophouseMicroIds;
        config(NUM_PARAMS, NUM_INPUTS, 0, 0);
        configSwitch(CONSERVATION_PARAM, 0.f, 1.f, 0.f, "Conservation", {"Guide", "Enforce"});
        configParam(INDEX_CV_ATT_PARAM, -1.f, 1.f, 1.f, "Index CV attenuverter");
        configInput(INDEX_CV_INPUT, "Scene index CV (sampled at phrase boundary)");
    }

    int  frontCount() const { return (list.degrees() == 24) ? 2 : 4; }
    bool enforce()          { return params[ShophouseMicroIds::CONSERVATION_PARAM].getValue() > 0.5f; }

    // Set 12/24 mode: only when no slot is loaded (spec: mode change with loaded slots needs explicit
    // clear at the UI). Resizes the front count accordingly. Returns true if applied.
    bool setMode(int n) {
        if (list.anyLoaded() && n != list.degrees()) return false;
        list.setDegrees(n);
        list.resize((n == 24) ? 2 : 4);
        return true;
    }

    // Exposure for the Colonnades/Duo publish FOLD (Model Q single-writer).
    bool             hasActiveFront() const { return list.activeSlot().loaded; }
    const TuningSlot& activeFront()   const { return list.activeSlot(); }

    // Driver only (maintains scene selection; writes no engine state). Defined in the .cpp (needs
    // Monsoon engine for the phrase-boundary edge + host N).
    void process(const ProcessArgs& args) override;

    json_t* dataToJson() override;
    void dataFromJson(json_t* root) override;
};
