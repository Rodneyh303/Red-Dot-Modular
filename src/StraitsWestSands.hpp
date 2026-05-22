#pragma once

/**
 * StraitsWestSands.hpp
 * 
 * Straits West Sands Expander - Per-Voice DNA Control (Voices 9-16)
 * 
 * Connected to the right of Straits West outputs.
 * Provides per-voice DNA control for the 8 voices (9-16) output by Straits West.
 * 
 * Each voice row has:
 * - Rest DNA: Length, Offset, Rotation
 * - Melody DNA: Length, Offset, Rotation
 * - Octave DNA: Length, Offset, Rotation
 * - Per-dimension interpolation (blend toward average or mono)
 * 
 * 24 HP module (replaces portion of MonsoonDeepStraitsSands)
 */

#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

struct StraitsWestSands : Module {
    // Module uses MonsoonIds parameters from parent Monsoon module
    // No local parameter IDs needed - just a UI expander for voices 9-16
    
    StraitsWestSands() {
        // Dummy constructor - actual parameters managed by Monsoon parent
        config(0, 0, 0, 0);
    }
    
    void process(const ProcessArgs& args) override {
        // No processing needed - this is just a UI expander
        // Monsoon module handles all DNA logic
    }
};
