#pragma once

/**
 * StraitsEastSands.hpp
 * 
 * Straits East Sands Expander - Per-Voice DNA Control (Voices 2-8)
 * 
 * Connected to the right of Straits East outputs.
 * Provides per-voice DNA control for the 7 voices (2-8) output by Straits East.
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

struct StraitsEastSands : Module {
    // Module uses MonsoonIds parameters from parent Monsoon module
    // No local parameter IDs needed - just a UI expander for voices 2-8
    
    StraitsEastSands() {
        // Dummy constructor - actual parameters managed by Monsoon parent
        config(0, 0, 0, 0);
    }
    
    void process(const ProcessArgs& args) override {
        // No processing needed - this is just a UI expander
        // Monsoon module handles all DNA logic
    }
};
