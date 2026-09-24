#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"
#include "MicroTuning.hpp"                   // MicroTuningModule (for the control-rate bound-Micro resolve)
#include "ui/IntertropicalPairing.hpp"       // redDot::resolveFollowedT

using namespace rack;

// Left expander for Monsoon.
// Monsoon reads this module's inputs and params directly via the
// cachedExpander pointer — no message-passing protocol is used.
// The messages[2] double-buffer has been removed as it was dead code.
struct MonsoonInterchangeExpander : Module {
    // 3C-ii: this Interchange can also CV-modulate a Colonnades/Duo MICRO's scale-mask faders (not just
    // Monsoon's own SEMI faders). It binds to a Micro by pairId (reuse of the shared redDot pairing):
    //   followTarget == 0  -> AUTO: the nearest Micro hub either side (findPairHubEitherSide).
    //   followTarget >  0  -> the Micro whose pairId == followTarget, anywhere in the rack.
    // targetHalf says WHICH 12 of a 24-degree Micro these 12 CV inputs drive:
    //   1 -> degrees 0..11,  2 -> degrees 12..23.  For a 12-degree Micro, half 2 addresses degrees
    //   12..23 which don't exist → inert (satisfies "second Interchange on a Micro-12 is inert").
    // The Micro READS these (its own process() is the single writer of weight[]); this stays passive.
    int followTarget = 0;   // 0 = adjacency; >0 = Micro pairId to follow (persisted)
    int targetHalf   = 1;   // 1 or 2 (persisted)

    // ── Phase 4 / §16 item 1: TARGET SELECTOR (Rodney: "instead of Monsoon") ──────────────────────
    // Which sink these 12 SEMI + 2 OCT CV inputs modulate:
    //   0 = AUTO  → if a Colonnades/Duo Micro is bound, drive the MICRO's weight faders ONLY and
    //               SUPPRESS the Monsoon semi/octave-fader read; else drive Monsoon.
    //   1 = Monsoon only  → always Monsoon's faders, even when a Micro is bound.
    //   2 = Micro only    → always the Micro weights (Monsoon read suppressed) even if none is bound
    //                       (then it simply drives nothing — an explicit "not Monsoon" park).
    // Persisted. Default 0 (AUTO). Historic patches had no field → they load as AUTO; the only
    // behaviour change is the intended one (a bound Micro stops double-driving Monsoon).
    int targetMode = 0;

    // Runtime cache (NOT persisted): does a Micro currently resolve for THIS Interchange? Refreshed at
    // control rate in process() so drivesMonsoon() is self-contained on the Monsoon-fader read side
    // without that side repeating the rack-wide resolve. MicroTuning's own bound-scan is the authority
    // for the MICRO read; this mirror only gates the Monsoon read under AUTO.
    bool  microBoundCached_ = false;
    rack::dsp::ClockDivider targetScanDiv_;

    // Effective routing predicates. `microBound` = is a Micro reachable for this Interchange (AUTO
    // needs it; the fixed modes ignore it). Kept as small pure helpers so both read sites agree.
    bool drivesMonsoon(bool microBound) const {
        if (targetMode == 1) return true;      // Monsoon only
        if (targetMode == 2) return false;     // Micro only
        return !microBound;                    // AUTO: Monsoon only when no Micro is bound
    }
    bool drivesMicro(bool microBound) const {
        if (targetMode == 1) return false;     // Monsoon only
        if (targetMode == 2) return true;      // Micro only
        return microBound;                     // AUTO: Micro when one is bound
    }
    // Convenience for the Monsoon-fader read side, which doesn't recompute microBound itself.
    bool drivesMonsoonCached() const { return drivesMonsoon(microBoundCached_); }

    MonsoonInterchangeExpander() {
        config(MonsoonIds::NUM_EXPANDER_PARAMS, MonsoonIds::NUM_EXPANDER_INPUTS, 0, 0);

        for (int i = 0; i < 12; i++) {
            configInput(MonsoonIds::EXPANDER_SEMI_CV_INPUT_0 + i,
                        string::f("Semitone %d CV", i + 1));
            configParam(MonsoonIds::EXPANDER_SEMI_ATTENUVERTER_0 + i,
                        -1.f, 1.f, 0.f, string::f("Semitone %d CV Attenuverter", i + 1));
        }

        configInput(MonsoonIds::EXPANDER_OCT_LO_CV_INPUT, "Octave Low CV");
        configParam(MonsoonIds::EXPANDER_OCT_LO_ATTENUVERTER, -1.f, 1.f, 0.f, "Octave Low CV Attenuverter");

        configInput(MonsoonIds::EXPANDER_OCT_HI_CV_INPUT, "Octave High CV");
        configParam(MonsoonIds::EXPANDER_OCT_HI_ATTENUVERTER, -1.f, 1.f, 0.f, "Octave High CV Attenuverter");
    }

    void process(const ProcessArgs& args) override {
        // Refresh microBoundCached_ at control rate (rack-wide resolve is too costly per-sample). Only
        // AUTO actually needs it, but the resolve is cheap on a divider and keeps the flag honest for
        // the display too. resolveFollowedT handles both followTarget>0 (by id) and 0 (nearest).
        if (targetScanDiv_.getDivision() == 0) targetScanDiv_.setDivision(64);
        if (targetScanDiv_.process()) {
            MicroTuningModule* hub = redDot::resolveFollowedT<MicroTuningModule>(this, followTarget);
            microBoundCached_ = (hub && hub->pairId > 0);
        }
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "followTarget", json_integer(followTarget));
        json_object_set_new(root, "targetHalf",   json_integer(targetHalf));
        json_object_set_new(root, "targetMode",   json_integer(targetMode));   // §16 item 1 (0=AUTO)
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "followTarget")) followTarget = (int)json_integer_value(j);
        if (json_t* j = json_object_get(root, "targetHalf"))   targetHalf   = (int)json_integer_value(j);
        if (json_t* j = json_object_get(root, "targetMode"))   targetMode   = (int)json_integer_value(j);  // missing => 0 => AUTO
    }
};
