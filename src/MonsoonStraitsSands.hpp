#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

/**
 * Straits Sands Expander - Compact Macro DNA Control
 * 
 * Simple global DNA controls for all poly voices (2-16).
 * Single set of controls broadcasts to all voices with global interpolation.
 * 
 * Designed for:
 *  • Live performance quick tweaking
 *  • Simple workflows without per-voice details
 *  • Compact panel footprint (~15mm wide)
 * 
 * Contrast with "Straits Sands Deep" which provides per-voice detailed control.
 */

namespace StraitsSandsIds {
    enum ButtonId {
        SCRAMBLE_ALL = 0,
        RESET_ALL,
        NUM_PARAMS
    };
    
    enum InputId {
        SCRAMBLE_ALL_GATE = 0,
        RESET_ALL_GATE,
        NUM_INPUTS
    };
};

struct MonsoonStraitsSands : Module {
    MonsoonStraitsSands() {
        // Size to full parent namespace to access global macro params
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, 0, 0);

        // ── Rest DNA Controls (Global) ──
        configParam(MonsoonIds::GLOBAL_REST_DNA_LEN, 1.f, 16.f, 16.f, "Rest DNA Length");
        configParam(MonsoonIds::GLOBAL_REST_DNA_OFF, 0.f, 15.f, 0.f, "Rest DNA Offset");
        configParam(MonsoonIds::GLOBAL_REST_DNA_ROT, 0.f, 15.f, 0.f, "Rest DNA Rotation");
        configParam(MonsoonIds::GLOBAL_REST_INTERP, 0.f, 1.f, 0.f, "Rest Interp (Per-Voice ↔ Average)");
        
        // ── Melody DNA Controls (Global) ──
        configParam(MonsoonIds::GLOBAL_MELODY_DNA_LEN, 1.f, 16.f, 16.f, "Melody DNA Length");
        configParam(MonsoonIds::GLOBAL_MELODY_DNA_OFF, 0.f, 15.f, 0.f, "Melody DNA Offset");
        configParam(MonsoonIds::GLOBAL_MELODY_DNA_ROT, 0.f, 15.f, 0.f, "Melody DNA Rotation");
        configParam(MonsoonIds::GLOBAL_MELODY_INTERP, 0.f, 1.f, 0.f, "Melody Interp (Per-Voice ↔ Average)");
        
        // ── Octave DNA Controls (Global) ──
        configParam(MonsoonIds::GLOBAL_OCTAVE_DNA_LEN, 1.f, 16.f, 16.f, "Octave DNA Length");
        configParam(MonsoonIds::GLOBAL_OCTAVE_DNA_OFF, 0.f, 15.f, 0.f, "Octave DNA Offset");
        configParam(MonsoonIds::GLOBAL_OCTAVE_DNA_ROT, 0.f, 15.f, 0.f, "Octave DNA Rotation");
        configParam(MonsoonIds::GLOBAL_OCTAVE_INTERP, 0.f, 1.f, 0.f, "Octave Interp (Per-Voice ↔ Average)");
        
        // ── Buttons & Gates ──
        configButton(StraitsSandsIds::SCRAMBLE_ALL, "Scramble All Voices");
        configButton(StraitsSandsIds::RESET_ALL, "Reset All Voices");
        configInput(StraitsSandsIds::SCRAMBLE_ALL_GATE, "Scramble All Gate");
        configInput(StraitsSandsIds::RESET_ALL_GATE, "Reset All Gate");
    }

    void process(const ProcessArgs& args) override {}
};
