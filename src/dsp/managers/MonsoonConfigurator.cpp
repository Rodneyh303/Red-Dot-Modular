#include "MonsoonConfigurator.hpp"
#include "../../Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

void MonsoonConfigurator::setup(Monsoon* m) {
    m->config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    // Main controls
    m->configSwitch(NOTE_VALUE_PARAM, 0.f, 7.f, 2.f, "Note value",
        {"1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/32"});
    m->configParam(VARIATION_PARAM,   0.f, 1.f, 0.5f, "Variation (longer–shorter)");
    m->configParam(LEGATO_PARAM,      0.f, 1.f, 0.10f, "Legato probability");
    m->configParam(REST_PARAM,        0.f, 1.f, 0.10f, "Rest probability");
    m->configParam(ACCENT_KNOB,       0.f, 1.f, 0.25f, "Accent gate probability");
    m->configParam(TRANSPOSE_PARAM,  -12.f, 12.f, 0.f, "Transpose (semitones)");
    
    // Main window controls (Always present)
    m->configParam(PATTERN_LENGTH_PARAM, 1.f, 16.f, 16.f, "Pattern length");
    m->configParam(PATTERN_OFFSET_PARAM, 0.f, 15.f, 0.f, "Pattern offset");

    // DNA Action Buttons (Main module only configures these if no expander)
    m->configButton(DNA_SCRAMBLE_ALL_PARAM, "Scramble ALL DNA");
    m->configButton(DNA_SCRAMBLE_R_PARAM,   "Scramble Rhythm");
    m->configButton(DNA_SCRAMBLE_V_PARAM,   "Scramble Variation");
    m->configButton(DNA_SCRAMBLE_L_PARAM,   "Scramble Legato");
    m->configButton(DNA_SCRAMBLE_M_PARAM,   "Scramble Melody");
    m->configButton(DNA_SCRAMBLE_O_PARAM,   "Scramble Octave");

    m->configButton(DNA_RESET_ALL_PARAM,    "Reset ALL DNA");
    m->configButton(DNA_RESET_R_PARAM,      "Reset Rhythm");
    m->configButton(DNA_RESET_V_PARAM,      "Reset Variation");
    m->configButton(DNA_RESET_L_PARAM,      "Reset Legato");
    m->configButton(DNA_RESET_M_PARAM,      "Reset Melody");
    m->configButton(DNA_RESET_O_PARAM,      "Reset Octave");

    // 12 semitone sliders – default to major scale C (C,D,E,F,G,A,B)
    for (int i = 0; i < 12; ++i) {
        float def = 0.f;
        if (i == 0 || i == 2 || i == 4 || i == 5 || i == 7 || i == 9 || i == 11) def = 1.f;
        m->configParam(SEMI0_PARAM + i, 0.f, 1.f, def, "Semitone weight");
    }

    m->configParam(OCT_LO_PARAM, 0.f, 8.f, 2.f, "Lowest octave");
    m->configParam(OCT_HI_PARAM, 0.f, 8.f, 5.f, "Highest octave");
    m->configParam(BPM_PARAM, 20.f, 300.f, 120.f, "BPM (internal clock)");

    // Buttons (momentary)
    m->configButton(DICE_R_PARAM, "Dice rhythm");
    m->configParam(DICE_SLEW_R_PARAM, 0.f, 1.f, 1.f, "Rhythm dice slew", "%", 0.f, 100.f);
    m->configButton(DICE_M_PARAM, "Dice melody");
    m->configParam(DICE_SLEW_M_PARAM, 0.f, 1.f, 1.f, "Melody dice slew", "%", 0.f, 100.f);
    m->configButton(DICE_TRIAL_R_PARAM, "Trial rhythm (audition vs fixed A)");
    m->configButton(DICE_TRIAL_M_PARAM, "Trial melody (audition vs fixed A)");
    m->configParam(RHYTHM_MIX_PARAM, 0.f, 1.f, 0.f, "Rhythm A>B mix", "%", 0.f, 100.f);
    m->configParam(MELODY_MIX_PARAM, 0.f, 1.f, 0.f, "Melody A>B mix", "%", 0.f, 100.f);
    m->configButton(LOCK_PARAM,   "Lock");
    m->configButton(MUTE_PARAM,   "Mute");
    m->configButton(MODE_PARAM,   "Mode (Cycle A-B-C-D)");
    m->configButton(RESET_BUTTON_PARAM,  "Reset");
    m->configButton(RUN_GATE_PARAM,      "Run/Stop");

    // I/O
    m->configInput(CLK_INPUT,   "Clock");
    m->configInput(GATE1_INPUT, "Gate In 1");
    m->configInput(GATE2_INPUT, "Gate In 2");
    m->configInput(CV1_INPUT,   "CV In 1");
    m->configInput(CV2_INPUT,   "CV In 2");
    m->configInput(CV3_MOD_INPUT,   "CV3 assignable mod (slew/mix)");
    m->configInput(GATE3_MOD_INPUT, "Gate3 assignable mod (trial die / reseed toggles)");
    m->configInput(ACCENT_CV_INPUT, "Accent Probability CV");

    m->configInput(RESET_TRIGGER_INPUT, "Reset (phrase restart)");
    m->configInput(SEED_INPUT,   "Seed CV (0..10V)");
    m->configInput(LENGTH_INPUT, "Pattern Length CV (0..10V)");
    m->configInput(OFFSET_INPUT, "Pattern Offset CV (0..10V)");
    m->configInput(RUN_GATE_INPUT, "Run/Stop Gate");

    // DNA Gate Inputs
    m->configInput(DNA_SCRAMBLE_ALL_INPUT, "Scramble ALL DNA Gate");
    m->configInput(DNA_SCRAMBLE_R_INPUT,   "Scramble Rhythm Gate");
    m->configInput(DNA_SCRAMBLE_V_INPUT,   "Scramble Variation Gate");
    m->configInput(DNA_SCRAMBLE_L_INPUT,   "Scramble Legato Gate");
    m->configInput(DNA_SCRAMBLE_A_INPUT,   "Scramble Accent Gate");
    m->configInput(DNA_SCRAMBLE_M_INPUT,   "Scramble Melody Gate");
    m->configInput(DNA_SCRAMBLE_O_INPUT,   "Scramble Octave Gate");

    m->configInput(DNA_RESET_ALL_INPUT,    "Reset ALL DNA Gate");
    m->configInput(DNA_RESET_R_INPUT,      "Reset Rhythm Gate");
    m->configInput(DNA_RESET_V_INPUT,      "Reset Variation Gate");
    m->configInput(DNA_RESET_L_INPUT,      "Reset Legato Gate");
    m->configInput(DNA_RESET_A_INPUT,      "Reset Accent Gate");
    m->configInput(DNA_RESET_M_INPUT,      "Reset Melody Gate");
    m->configInput(DNA_RESET_O_INPUT,      "Reset Octave Gate");

    m->configOutput(GATE_OUTPUT,           "Gate");
    m->configOutput(CV_OUTPUT,             "1V/Oct");
    m->configOutput(SEED_OUTPUT,           "Seed Voltage Out (0..10V)");
    m->configOutput(RESET_TRIGGER_OUTPUT,  "Reset Trigger Out");
    m->configOutput(RUN_GATE_OUTPUT,       "Run Gate Out");
    m->configOutput(TIE_OUTPUT,            "Tie Gate (high on Tie)");
    m->configOutput(LEGATO_OUTPUT,         "Legato Gate (high on Legato/Max)");
    m->configOutput(TIE_OR_LEGATO_OUTPUT,  "Tie or Legato Gate (high on either)");
    m->configOutput(ACCENT_OUTPUT,         "Accent Gate (high when accented)");
}