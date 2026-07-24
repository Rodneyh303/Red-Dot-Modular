#pragma once
// MonsoonTemasekExpander — the Temasek transform expander for Change Alley.
//
// Design: CHANGE_ALLEY_DESIGN.md §13–§14.
// Temasek is the old Malay name for Singapore (sea town) — the layer beneath Change Alley
// that reshapes the whole board at once, at phrase boundaries, by force.
//
// Layout: intra (within-block) verbs LEFT of a ~40HP panel, inter (across-block) RIGHT,
// mirrored with jacks to the outside. Top-to-bottom: Collapse, Rotate, Reflect, Scatter.
// Per verb × side (intra/inter) × pin type (R/M):
//   domain button+jack, codomain button+jack, grain knob.
//   Collapse additionally: leader knob.
//   Rotate additionally: step knob.
//   Scatter additionally: fwd+back jacks (like dice) instead of a step knob.
//
// Queue: one PendingAction per row (verb × side × type), latched at trigger time (§14a).
// Applied at the phrase boundary by MonsoonExpanderManager, exactly as Change Alley's
// current transforms. Change Alley and Temasek share the same src arrays on the module.
//
// Claiming: Temasek must be attached to a Change Alley, which is attached to Monsoon.
// The chain: Monsoon ↔ Change Alley ↔ Temasek.

#include "Monsoon.hpp"
#include "MonsoonChangeAlleyExpander.hpp"
#include "ui/StoreBound.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
namespace TK = TemasekIds;

struct MonsoonTemasekExpander : Module {
    // Pending actions — one per row, latched at trigger time.
    TK::PendingAction pendingRows[TK::N_ROWS];

    // Trigger detectors
    dsp::SchmittTrigger domainTrig [TK::N_ROWS];
    dsp::SchmittTrigger codomainTrig[TK::N_ROWS];
    dsp::SchmittTrigger scatterFwd [TK::SIDES * TK::TYPES];
    dsp::SchmittTrigger scatterBack[TK::SIDES * TK::TYPES];

    // Scatter Philox counters (one per side per pin type — own key, not the dice counter)
    uint64_t scatterCounter[TK::SIDES * TK::TYPES] = {};

    MonsoonTemasekExpander() {
        config(TK::NUM_PARAMS, TK::NUM_INPUTS, 0, TK::NUM_LIGHTS);

        // Grain knobs — DAW-exposed GENERATION params, latch under lock.
        static const char* verbNames[TK::N_VERBS] = {"Collapse","Rotate","Reflect","Scatter"};
        static const char* sideNames[TK::SIDES]   = {"Intra","Inter"};
        static const char* typeNames[TK::TYPES]   = {"Rhythm","Melody"};
        static const char* grainLabels[] = {"1","2","4","8","16"};
        for (int v = 0; v < TK::N_VERBS; ++v)
            for (int s = 0; s < TK::SIDES; ++s)
                for (int t = 0; t < TK::TYPES; ++t) {
                    int r = TK::rowId(v, s, t);
                    std::string stem = std::string(verbNames[v]) + " " +
                                       sideNames[s] + " " + typeNames[t];
                    configSwitch(TK::GRAIN_START + r, 0.f, 4.f, 2.f,
                        stem + " grain", {grainLabels[0], grainLabels[1],
                        grainLabels[2], grainLabels[3], grainLabels[4]});
                }

        // Leader knobs (Collapse rows only: N_ROWS/2 = 8)
        for (int r = 0; r < TK::N_ROWS / 2; ++r)
            configParam(TK::LEADER_START + r, 0.f, 15.f, 0.f,
                "Leader offset", "", 0.f, 1.f, 0.f)->snapEnabled = true;

        // Step knobs (Rotate rows only: N_ROWS/2 = 8)
        for (int r = 0; r < TK::N_ROWS / 2; ++r)
            configParam(TK::STEP_START + r, -7.f, 7.f, 1.f,
                "Step", "", 0.f, 1.f, 0.f)->snapEnabled = true;

        // Trigger jacks
        for (int r = 0; r < TK::N_ROWS; ++r) {
            configInput(TK::DOMAIN_TRIG_START   + r, "Domain trigger");
            configInput(TK::CODOMAIN_TRIG_START + r, "Codomain trigger");
        }
        // Grain CV + attenuverters (NOT DAW-exposed per §14b — will be de-parammed)
        for (int i = 0; i < TK::N_VERBS * TK::TYPES; ++i) {
            configInput(TK::GRAIN_CV_START    + i, "Grain CV");
            configInput(TK::GRAIN_ATTEN_START + i, "Grain attenuverter");
        }
        // Scatter fwd/back
        for (int i = 0; i < TK::SIDES * TK::TYPES; ++i) {
            configInput(TK::SCATTER_FWD_START  + i, "Scatter forward");
            configInput(TK::SCATTER_BACK_START + i, "Scatter back");
        }
    }

    void process(const ProcessArgs&) override {
        // Detect triggers and latch into pendingRows.
        // Application happens in MonsoonExpanderManager at phrase boundary, exactly as the
        // current Change Alley transforms. (Not yet wired — expanderManager needs a
        // cachedTemasekExpander pointer and a applyTemasekPending() call.)

        for (int v = 0; v < TK::N_VERBS; ++v) {
            for (int s = 0; s < TK::SIDES; ++s) {
                for (int t = 0; t < TK::TYPES; ++t) {
                    const int r = TK::rowId(v, s, t);

                    auto latchRow = [&](bool domain) {
                        auto& p     = pendingRows[r];
                        p.armed     = true;
                        p.isDomain  = domain;
                        p.isInter   = (s == 1);
                        // grain from param (latched here, not re-read at boundary)
                        const int gIdx = TK::GRAIN_START + r;
                        p.grain = 1 << (int)std::round(
                            clamp(params[gIdx].getValue(), 0.f, 4.f));
                        // leader / step
                        if (v == TK::V_COLLAPSE) {
                            // leader offset: which Collapse row (0..N_ROWS/2-1)?
                            // Collapse rows are verb=0, so leader row = s*2+t
                            const int lIdx = TK::LEADER_START + s * TK::TYPES + t;
                            p.leaderOrStep = (int)std::round(params[lIdx].getValue());
                        } else if (v == TK::V_ROTATE) {
                            const int sIdx = TK::STEP_START + s * TK::TYPES + t;
                            p.leaderOrStep = (int)std::round(params[sIdx].getValue());
                        } else {
                            p.leaderOrStep = 0;
                        }
                        p.scatterDelta = 1;   // fwd/back handled separately
                        lights[TK::PENDING_LIGHT_START + r].setBrightness(1.f);
                    };

                    if (domainTrig[r].process(
                            inputs[TK::DOMAIN_TRIG_START + r].getVoltage()))
                        latchRow(true);
                    if (codomainTrig[r].process(
                            inputs[TK::CODOMAIN_TRIG_START + r].getVoltage()))
                        latchRow(false);
                }
            }
        }

        // Scatter fwd/back — latch the scatter rows with appropriate delta
        for (int s = 0; s < TK::SIDES; ++s) {
            for (int t = 0; t < TK::TYPES; ++t) {
                const int i = s * TK::TYPES + t;
                const int r = TK::rowId(TK::V_SCATTER, s, t);
                for (int delta : {1, -1}) {
                    const int jIdx = (delta == 1 ? TK::SCATTER_FWD_START
                                                 : TK::SCATTER_BACK_START) + i;
                    auto& trig = (delta == 1 ? scatterFwd : scatterBack)[i];
                    if (trig.process(inputs[jIdx].getVoltage())) {
                        auto& p        = pendingRows[r];
                        p.armed        = true;
                        p.isInter      = (s == 1);
                        p.grain        = 1 << (int)std::round(
                            clamp(params[TK::GRAIN_START + r].getValue(), 0.f, 4.f));
                        p.scatterDelta = delta;
                        lights[TK::PENDING_LIGHT_START + r].setBrightness(1.f);
                    }
                }
            }
        }
    }
};
