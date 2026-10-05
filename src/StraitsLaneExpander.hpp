#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

// ── Straits Lane Expander — generic per-lane panel extension ─────────────────
// SANDS CONSOLIDATION / LANE EXTENSION MODEL: each rhythm lane (REST/ACCENT/QMIX/
// VARIATION/LEGATO) is a physically attachable extension that docks RIGHT of the
// Straits Base. ONE generic LaneExpander class, parameterised by a LaneDescriptor.
// The lane expander carries 16 per-voice knobs (voice 0 = mono mirror, 1..15 = poly).
// The knobs bind to the lane expander's OWN params (same IDs as MonsoonIds); the
// base's step() syncs them into the base's params so the engine reads them via the
// cached poly-voice-expander pointer (which points to the base).
//
// See docs/design/STRAITS_CAUSEWAY_LANE_EXTENSIONS.md REFINED MODEL + METHOD:REFACTOR.
// This is a REFACTOR of the working Straits widget — the knob binding loop, mod-arc
// overlay, dim/lock logic, and theme following are all COPIED from MonsoonStraitsExpander,
// not reimplemented.

// ── Lane Descriptor — the ONLY thing that differs between lanes ───────────────
// Read off the existing Straits bank differences (see plans/lane_expander_phase0_recon.md).
struct LaneDescriptor {
    const char* slug;           // e.g. "StraitsLaneQMIX"
    const char* anchorPrefix;   // e.g. "param_qmix_"
    const char* label;          // e.g. "Q-MIX"
    int monoParamId;            // e.g. MonsoonIds::QMIX_LEVEL_PARAM
    int polyParamIdBase;        // e.g. MonsoonIds::POLY_QMIX_PARAM_1
    int arcLane;                // 0=REST, 1=ACCENT, 2=QMIX (mod-arc lane index)
    // Colour keys in the panel theme (matched to gen_straits_lane.py)
    const char* tintKey;        // e.g. "qmix"
};

// ── Lane Expander Module ─────────────────────────────────────────────────────
// Sized to MonsoonIds::NUM_PARAMS so the poly param IDs are valid slots. Only the
// lane's own params are configured (the rest stay at default). process() is empty —
// the base syncs params via step(), and the engine reads from the base.
struct StraitsLaneExpander : Module {
    const LaneDescriptor* desc = nullptr;   // set by the widget constructor
    bool beingDragged = false;               // set by widget onButton, used by base force-follow

    StraitsLaneExpander() {
        // Sized to Monsoon's full param namespace so POLY_*_PARAM IDs are valid.
        config(MonsoonIds::NUM_PARAMS, 0, 0, 0);
    }

    void setDescriptor(const LaneDescriptor* d) {
        desc = d;
        // Configure the lane's params (mono mirror + 15 poly knobs).
        // Voice 1 (mono): mirror the parent Monsoon's mono param (display-only, locked).
        configParam(d->monoParamId, 0.f, 1.f, 0.f,
                    std::string("Voice 1 (mono) ") + d->label + " - follows Monsoon");
        // Voices 2..16: per-poly-voice knobs.
        for (int i = 0; i < 15; i++) {
            configParam(d->polyParamIdBase + i, 0.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " " + d->label);
        }
    }

    void process(const ProcessArgs&) override {}
};
