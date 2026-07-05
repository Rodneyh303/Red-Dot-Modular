#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// Junction: the big-5 pattern-knob modulation expander for Monsoon. Passive — its
// CV (x attenuverter) is summed into NOTE VALUE / VARIATION / LEGATO / REST /
// ACCENT by the ParameterManager via the cached pointer.
struct MonsoonJunctionExpander : Module {
    MonsoonJunctionExpander() {
        config(MonsoonIds::NUM_JUNCTION_PARAMS, MonsoonIds::NUM_JUNCTION_INPUTS, 0, 0);
        configParam(MonsoonIds::JUNCTION_NOTEVAL_ATT,   -1.f, 1.f, 0.f, "Note value CV attenuverter");
        configParam(MonsoonIds::JUNCTION_VARIATION_ATT, -1.f, 1.f, 0.f, "Variation CV attenuverter");
        configParam(MonsoonIds::JUNCTION_LEGATO_ATT,    -1.f, 1.f, 0.f, "Legato CV attenuverter");
        configParam(MonsoonIds::JUNCTION_REST_ATT,      -1.f, 1.f, 0.f, "Rest CV attenuverter");
        configParam(MonsoonIds::JUNCTION_ACCENT_ATT,    -1.f, 1.f, 0.f, "Accent CV attenuverter");
        configInput(MonsoonIds::JUNCTION_NOTEVAL_CV,   "Note value CV");
        configInput(MonsoonIds::JUNCTION_VARIATION_CV, "Variation CV");
        configInput(MonsoonIds::JUNCTION_LEGATO_CV,    "Legato CV");
        configInput(MonsoonIds::JUNCTION_REST_CV,      "Rest CV");
        configInput(MonsoonIds::JUNCTION_ACCENT_CV,    "Accent CV");
    }
    void process(const ProcessArgs& args) override {}
};
