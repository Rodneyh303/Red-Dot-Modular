#pragma once
// MonsoonContainment — the articulation-envelope routing module (the "containment" sibling of
// Change Alley). Authority: CC_PROMPT_CONTAINMENT_MODULE_SPEC.md + ARTICULATION_ENVELOPE_VS_
// CORRELATION.md, amended by Rodney's rulings 2026-10-26 (multi-parent acyclic DAG with
// intersection semantics; disconnected vertices valid; EXPAND = trajectory memory). Build plan:
// docs/design/CC_PROMPT_CONTAINMENT_BUILD_PLAN.md.
//
// WHAT THIS MODULE IS: an EDITOR + DRIVER for the containment graph — per-voice gate-support
// nesting (gate(v) = clamp(own, ∩ resolved parents)), which is cycle-FATAL and therefore owns
// its own routing, structurally acyclic, CA-untouched. The graph state is ENGINE-SIDE
// authoritative (mirroring the CA seam: PatternEngine owns caRhythmSrc, the CA module edits and
// drives it); this module edits pins (cycle-rejecting) and drives the DAG-closed gesture verbs
// (INVERT / ROTATE ROLES / REFLECT ROLES / COLLAPSE-EXPAND) armed as pending and applied at the
// PHRASE BOUNDARY. The gate-clamp recursion that CONSUMES the graph is the decision manager's
// job (separate work — MONO_SPECIAL_IN_LOGIC_AUDIT.md); this module only PRODUCES the graph.
//
// ZERO PARAM SLOTS BY DESIGN (DAW_PARAM_AUDIT.md, the ChangeAlleyV2 rule): transforms are
// triggered verbs (buttons/CV), config lives in context menus — nothing here is
// automation-visible. Lands with the step-3/4 UI work.
//
// STEP 0 SCAFFOLD: registration + panel + the pure dsp headers (ContainmentGraph.hpp /
// ContainmentTransforms.hpp). Pending/boundary machinery = step 3; pin-matrix UI + store-backed
// edits = step 4; trajectory ring = follow-up. Kept honest: process() is a no-op until the
// machinery lands — no silent fake work.

#include <rack.hpp>
#include "Monsoon.hpp"                        // extern Plugin* pluginInstance
#include "dsp/ContainmentGraph.hpp"           // the state (pure, rack-free)
#include "dsp/ContainmentTransforms.hpp"      // the DAG-closed verbs (pure, rack-free)

using namespace rack;
// Qualified ids on purpose: Monsoon.hpp exposes MonsoonIds/ChangeAlleyIds with the same
// NUM_PARAMS/... names (the MonsoonChangeAlleyV2.hpp rule).

struct MonsoonContainment : Module {
    enum ParamIds { NUM_PARAMS };
    enum InputIds { NUM_INPUTS };
    enum OutputIds { NUM_OUTPUTS };
    enum LightIds { NUM_LIGHTS };

    MonsoonContainment() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    }

    void process(const ProcessArgs& args) override {
        // Step 3 lands the pending machinery here: armed verbs (CAPendingOrder reuse, stamped
        // chronologically) applied at the Rate::Phrase tier fed from Monsoon::process (the CA
        // pattern — the module never owns a clock; the R7 grep-gate discipline). Until then:
        // nothing. The graph is pure state; silence is the correct scaffold behaviour.
    }

    // Serialization lands in step 1 with the store wiring (raw uint16 array round-trip via
    // ContainmentGraph::serializeTo/deserializeFrom, the WARPDIAG round-trip lesson).
};

struct MonsoonContainmentWidget : ModuleWidget {
    MonsoonContainmentWidget(MonsoonContainment* module) {
        setModule(module);
        // Placeholder scaffold panel (house dark palette); the real panel art + the one-plane
        // multi-pin matrix UI arrive with step 4. Light-theme variant follows the house
        // dual-panel convention when the art lands.
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Containment_panel_dark.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    }
};
