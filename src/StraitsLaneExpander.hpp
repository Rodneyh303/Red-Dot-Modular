#pragma once
#include <rack.hpp>
#include <atomic>   // needsResnap crosses threads: engine (onExpanderChange) writes, UI (the base's step) reads/clears
#include "Monsoon.hpp"
#include "ui/ModuleLookupCore.hpp"   // THE lookup authority: the topology epoch (onExpanderChange bump — CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START)

// ── Straits Lane Expander — generic per-lane panel extension ─────────────────
// SANDS CONSOLIDATION / LANE EXTENSION MODEL: each rhythm lane (REST/ACCENT/QMIX/
// VARIATION/LEGATO) is a physically attachable extension that docks RIGHT of the
// Straits Base. ONE generic LaneExpander class, parameterised by a LaneDescriptor.
// The lane expander carries 16 per-voice knobs (voice 0 = mono mirror, 1..15 = poly).
// The knobs bind the lane's OWN SMALL local namespace (StraitsLaneIds below); the
// base's step() syncs lane-local → base-MonsoonIds so the engine reads them via the
// cached poly-voice-expander pointer (which points to the base).
//
// See docs/design/STRAITS_CAUSEWAY_LANE_EXTENSIONS.md REFINED MODEL + METHOD:REFACTOR.
// This is a REFACTOR of the working Straits widget — the knob binding loop, mod-arc
// overlay, dim/lock logic, and theme following are all COPIED from MonsoonStraitsExpander,
// not reimplemented.

// ── The lane expander's OWN small param namespace (CC_PROMPT_LANE_EXPANDER_RIGHTSIZE) ──
// The lane expander used to be sized to MonsoonIds::NUM_PARAMS (~1128) purely so
// the poly param IDs were valid slots on its params[] — exporting ~1128 to the DAW
// (the NUM_PARAMS_MIGRATION overflow class). It USES 16: the mono mirror + 15
// per-voice. The knobs bind THESE local ids (kept real params — DAW-automatable,
// the fixed-addressing keep-param category); the BASE's step() sync remaps
// lane-local → base-MonsoonIds (via LaneDescriptor's ids), and the engine reads
// the BASE — no MonsoonIds reach-in survives on the lane module itself.
namespace StraitsLaneIds {
    enum ParamIds {
        MONO_MIRROR_PARAM = 0,   // voice-1 mono mirror (display-only, locked)
        POLY_PARAM_1,            // per-voice knobs, voices 2..16
        POLY_PARAM_2,
        POLY_PARAM_3,
        POLY_PARAM_4,
        POLY_PARAM_5,
        POLY_PARAM_6,
        POLY_PARAM_7,
        POLY_PARAM_8,
        POLY_PARAM_9,
        POLY_PARAM_10,
        POLY_PARAM_11,
        POLY_PARAM_12,
        POLY_PARAM_13,
        POLY_PARAM_14,
        POLY_PARAM_15,
        NUM_PARAMS = 16,
    };
}

// ── Lane Descriptor — the ONLY thing that differs between lanes ───────────────
// Read off the existing Straits bank differences (see plans/lane_expander_phase0_recon.md).
struct LaneDescriptor {
    const char* slug;           // e.g. "StraitsLaneQMIX"
    const char* anchorPrefix;   // e.g. "param_qmix_"
    const char* label;          // e.g. "Q-MIX"
    // The BASE-side slots the base's step() sync WRITES (the engine reads the base
    // via cachedPolyVoiceExpander) — NOT the lane's own param ids (those are the
    // StraitsLaneIds locals now).
    int monoParamId;            // e.g. MonsoonIds::QMIX_LEVEL_PARAM (a BASE slot)
    int polyParamIdBase;        // e.g. MonsoonIds::POLY_QMIX_PARAM_1 (BASE slots)
    int arcLane;                // 0=REST, 1=ACCENT, 2=QMIX (mod-arc lane index)
    // Colour keys in the panel theme (matched to gen_straits_lane.py)
    const char* tintKey;        // e.g. "qmix"
};

// ── Lane Expander Module ─────────────────────────────────────────────────────
// Right-sized to StraitsLaneIds::NUM_PARAMS (16 — was MonsoonIds::NUM_PARAMS
// ≈ 1128, the DAW-export overflow; the big size was only ever the interface for
// the MonsoonIds-indexed knob binds). process() is empty — the base syncs the
// lane's params into the base's own via step() (lane-local ids → base-MonsoonIds),
// and the engine reads from the base (cachedPolyVoiceExpander).
struct StraitsLaneExpander : Module {
    const LaneDescriptor* desc = nullptr;   // set by the widget constructor
    bool beingDragged = false;               // set by widget onButton (legacy, unused now)
    std::atomic<bool> needsResnap{false};    // resnap request — writers: the widget's onDragEnd (UI) + onExpanderChange's left-link check (engine); reader/clearer: the base's step (UI). Relaxed — the topologyEpoch house pattern

    StraitsLaneExpander() {
        // The lane's OWN 16-param namespace (the right-size fix).
        config(StraitsLaneIds::NUM_PARAMS, 0, 0, 0);
    }

    void setDescriptor(const LaneDescriptor* d) {
        desc = d;
        // Configure the lane's params on the LOCAL ids (mono mirror + 15 poly).
        // Voice 1 (mono): mirror the parent Monsoon's mono param (display-only, locked).
        configParam(StraitsLaneIds::MONO_MIRROR_PARAM, 0.f, 1.f, 0.f,
                    std::string("Voice 1 (mono) ") + d->label + " - follows Monsoon");
        // Voices 2..16: per-poly-voice knobs.
        for (int i = 0; i < 15; i++) {
            configParam(StraitsLaneIds::POLY_PARAM_1 + i, 0.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " " + d->label);
        }
    }

    void process(const ProcessArgs&) override {}

    // LOOKUP AUTHORITY (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START): the event is THE trigger — bump
    // the global topology epoch so this module's adjacency (the lookup* memos) re-resolves on the
    // next read, engine + widget threads.
    // RESNAP FAST PATH (CC_PROMPT_STRAITS_RESNAP_HARDENING): also flag our own needsResnap
    // when the LEFT link is violated — the only adjacency signal for MID-chain insertions
    // (a module dropped between two lanes never changes the BASE's own expanders, so the
    // base's topologyDirty hook stays silent; it fires HERE instead). Naive model check,
    // deliberately ownership-blind: a lane parked against ANOTHER chain passes here, but
    // the base-side exact chainIntact() — fired by the base's own hook when we left it —
    // heals that. Relaxed atomic (the topologyEpoch house pattern): this can run on the
    // engine thread; the base's step() (UI) reads and clears the flag.
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
        rack::Module* left = leftExpander.module;
        const bool leftIsChain = left
            && (left->model == modelStraitsBase
             || left->model == modelStraitsLaneQMIX
             || left->model == modelStraitsLaneREST
             || left->model == modelStraitsLaneACCENT
             || left->model == modelStraitsLaneVARIATION
             || left->model == modelStraitsLaneLEGATO);
        if (!leftIsChain)
            needsResnap.store(true, std::memory_order_relaxed);
    }
};
