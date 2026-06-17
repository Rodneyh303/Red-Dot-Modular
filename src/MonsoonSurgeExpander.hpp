#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// Surge: the big-5 pattern-knob modulation expander for Monsoon. Passive — its
// CV (x attenuverter) is summed into NOTE VALUE / VARIATION / LEGATO / REST /
// ACCENT by the ParameterManager via the cached pointer.
struct MonsoonSurgeExpander : Module {
    MonsoonSurgeExpander() {
        config(MonsoonIds::NUM_SURGE_PARAMS, MonsoonIds::NUM_SURGE_INPUTS, 0, 0);
        configParam(MonsoonIds::SURGE_NOTEVAL_ATT,   -1.f, 1.f, 0.f, "Note value CV attenuverter");
        configParam(MonsoonIds::SURGE_VARIATION_ATT, -1.f, 1.f, 0.f, "Variation CV attenuverter");
        configParam(MonsoonIds::SURGE_LEGATO_ATT,    -1.f, 1.f, 0.f, "Legato CV attenuverter");
        configParam(MonsoonIds::SURGE_REST_ATT,      -1.f, 1.f, 0.f, "Rest CV attenuverter");
        configParam(MonsoonIds::SURGE_ACCENT_ATT,    -1.f, 1.f, 0.f, "Accent CV attenuverter");
        configInput(MonsoonIds::SURGE_NOTEVAL_CV,   "Note value CV");
        configInput(MonsoonIds::SURGE_VARIATION_CV, "Variation CV");
        configInput(MonsoonIds::SURGE_LEGATO_CV,    "Legato CV");
        configInput(MonsoonIds::SURGE_REST_CV,      "Rest CV");
        configInput(MonsoonIds::SURGE_ACCENT_CV,    "Accent CV");
    }
    void process(const ProcessArgs& args) override {}
};
