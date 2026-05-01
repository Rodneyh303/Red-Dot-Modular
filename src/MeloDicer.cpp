// src/MeloDicer.cpp
// MeloDicer - full implementation with clean Mode A stepping logic
// C++11 compatible
//
// Patched: separate RNGs for melody & rhythm using rack::random::Xoroshiro128Plus,
// SEED CV input/output, RESET input, deferred reseed at phrase boundary, JSON serialization,
// two-part 64-bit seeding, widget ports for RESET/SEED, and preserved behavior.

#include <rack.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>
#include "MeloDicerWidget.hpp"
#include "MeloDicer.hpp"
#include "PatternEngine.hpp"
#include "GateState.hpp"

using namespace rack;

Plugin* pluginInstance = nullptr;

void MeloDicer::reseedXoroshiroFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat) {
    engine.pe.seedRngFromFloat(rng, seedFloat);
}

MeloDicer::MeloDicer() {


        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        // Main controls
        configSwitch(NOTE_VALUE_PARAM, 0.f, 7.f, 2.f, "Note value",
            {"1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/32T", "1/32"});
        configParam(VARIATION_PARAM,   0.f, 1.f, 0.5f, "Variation (longer–shorter)");
        configParam(LEGATO_PARAM,      0.f, 1.f, 0.10f, "Legato probability");
        configParam(REST_PARAM,        0.f, 1.f, 0.10f, "Rest probability");
        configParam(TRANSPOSE_PARAM,  -12.f, 12.f, 0.f, "Transpose (semitones)");
        //Pattern ring controls
        configParam(PATTERN_LENGTH_PARAM, 1.f, 16.f, 16.f, "Pattern length (steps)");
        configParam(PATTERN_OFFSET_PARAM, 0.f, 15.f, 0.f, "Pattern offset");

        // 12 semitone sliders – default to major scale C (C,D,E,F,G,A,B)
        for (int i = 0; i < 12; ++i) {
            float def = 0.f;
            if (i == 0 || i == 2 || i == 4 || i == 5 || i == 7 || i == 9 || i == 11) def = 1.f;
            configParam(SEMI0_PARAM + i, 0.f, 1.f, def, "Semitone weight");
        }

        configParam(OCT_LO_PARAM, 0.f, 8.f, 2.f, "Lowest octave");
        configParam(OCT_HI_PARAM, 0.f, 8.f, 5.f, "Highest octave");

        configParam(BPM_PARAM, 20.f, 300.f, 120.f, "BPM (internal clock)");

        // Buttons (momentary)
        configButton(DICE_R_PARAM, "Dice rhythm");
        configButton(DICE_M_PARAM, "Dice melody");
        configButton(LOCK_PARAM,   "Lock");
        configButton(MUTE_PARAM,   "Mute");
        configButton(MODE_A_PARAM,        "Mode A (Sequencer)");
        configButton(MODE_B_PARAM,        "Mode B (Seq+Gate)");
        configButton(MODE_C_PARAM,        "Mode C (Quantizer 1)");
        configButton(MODE_D_PARAM,        "Mode D (Quantizer 2)");
        configButton(RESET_BUTTON_PARAM,  "Reset");
        configButton(RUN_GATE_PARAM,      "Run/Stop");

        // I/O
        configInput(CLK_INPUT,   "Clock");
        configInput(GATE1_INPUT, "Gate In 1");
        configInput(GATE2_INPUT, "Gate In 2");
        configInput(CV1_INPUT,   "CV In 1");
        configInput(CV2_INPUT,   "CV In 2");

        // --- RNG/SEED ADDITION: new inputs
        configInput(RESET_TRIGGER_INPUT, "Reset (phrase restart)");
        configInput(SEED_INPUT,   "Seed CV (0..10V)");
        configInput(LENGTH_INPUT, "Pattern Length CV (0..10V)");
        configInput(OFFFSET_INPUT,"Pattern Offset CV (0..10V)");
        configInput(RUN_GATE_INPUT, "Run/Stop Gate");

        configOutput(GATE_OUTPUT,           "Gate");
        configOutput(CV_OUTPUT,             "1V/Oct");
        configOutput(SEED_OUTPUT,           "Seed Voltage Out (0..10V)");
        configOutput(RESET_TRIGGER_OUTPUT,  "Reset Trigger Out");
        configOutput(RUN_GATE_OUTPUT,       "Run Gate Out");

        // Seed RNGs with a random value — safe to call here (uses rack::random, not inputs[])
        rhythmSeedFloat = rack::random::uniform() * 10.f;
        melodySeedFloat = rack::random::uniform() * 10.f;
        reseedXoroshiroFromFloat(rhythmRng, rhythmSeedFloat);
        reseedXoroshiroFromFloat(melodyRng, melodySeedFloat);
        rhythmSeedPendingFloat = rhythmSeedFloat;
        melodySeedPendingFloat = melodySeedFloat;

        // Default patterns: all gates on, CV at 0V (C0), semitone 0
        // genPitchV() reads params[] which aren't valid yet, so use safe literals
        for (int i = 0; i < 16; ++i) {
            rhythmPattern[i]  = true;
            melodyPitchV[i]   = 0.f;   // C0 = 0V
            melodySemitone[i] = 0;     // semitone C
        }

        initialize();

        // Apply POWER MUTE: if the loaded/default muteBehavior is 3 (PWR MUTE), start muted
        if (muteBehavior == 3) muted = true;
    }

  void MeloDicer::initialize(){
        cv1Mode = 0;
        cv2Mode = 0;
        gate1Assign = 0;
        gate2Assign = 1;
        muteBehavior = 0;
        lastModeSelect = -1;

        engine.reset();

        bpm = 120.f;
        clock.reset();
        prevExtGate = false;
        currentPitchV = 0.f;
        melodySeedCached = false;
        rhythmSeedCached = false;
  }

    // --- serialization ---
    json_t* MeloDicer::dataToJson() {
        json_t* root = json_object();
        json_object_set_new(root,"cv1Mode", json_integer(cv1Mode));
        json_object_set_new(root,"cv2Mode", json_integer(cv2Mode));
        json_object_set_new(root,"gate1Assign", json_integer(gate1Assign));
        json_object_set_new(root,"gate2Assign", json_integer(gate2Assign));
        json_object_set_new(root,"muteBehavior", json_integer(muteBehavior));
        json_object_set_new(root,"noteVariationMask", json_integer(noteVariationMask));
        json_object_set_new(root,"ppqnSetting", json_integer(ppqnSetting));
        json_object_set_new(root,"modeSelect", json_integer(modeSelect));
        json_object_set_new(root,"lightTheme", json_boolean(lightTheme));

        json_object_set_new(root,"locked", json_boolean(locked));
        json_object_set_new(root,"muted", json_boolean(muted));

        // new fields
        json_object_set_new(root,"rhythmMode", json_integer(rhythmMode));
        json_object_set_new(root,"melodyMode", json_integer(melodyMode));
        json_object_set_new(root,"startStep", json_integer(startStep));
        json_object_set_new(root,"endStep", json_integer(endStep));

        // store seed floats and parts
        json_object_set_new(root,"rhythmSeedFloat", json_real((float)rhythmSeedFloat));
        json_object_set_new(root,"melodySeedFloat", json_real((float)melodySeedFloat));


        // pending
        json_object_set_new(root,"rhythmSeedPending", json_boolean(rhythmSeedPending));
        json_object_set_new(root,"rhythmSeedPendingFloat", json_real((float)rhythmSeedPendingFloat));
        json_object_set_new(root,"melodySeedPending", json_boolean(melodySeedPending));
        json_object_set_new(root,"melodySeedPendingFloat", json_real((float)melodySeedPendingFloat));

        // serialize rhythmPattern as array of ints 0/1
        json_t* rarr = json_array();
        for (int i = 0; i < 16; ++i) json_array_append_new(rarr, json_integer(rhythmPattern[i] ? 1 : 0));
        json_object_set_new(root,"rhythmPattern", rarr);

        // serialize melodyPitchV as array of reals
        json_t* marr = json_array();
        for (int i = 0; i < 16; ++i) json_array_append_new(marr, json_real(melodyPitchV[i]));
        json_object_set_new(root,"melodyPitchV", marr);

        return root;
    }

    void MeloDicer::dataFromJson(json_t* root) {
        if (auto j = json_object_get(root,"cv1Mode")) cv1Mode = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"cv2Mode")) cv2Mode = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"gate1Assign")) gate1Assign = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"gate2Assign")) gate2Assign = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"muteBehavior")) muteBehavior = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"noteVariationMask")) noteVariationMask = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"ppqnSetting")) ppqnSetting = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"modeSelect")) modeSelect = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"lightTheme")) lightTheme = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"locked")) locked = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"muted")) muted = (bool)json_boolean_value(j);

        if (auto j = json_object_get(root,"rhythmMode")) rhythmMode = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"melodyMode")) melodyMode = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"startStep")) startStep = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"endStep")) endStep = (int)json_integer_value(j);

        if (auto j = json_object_get(root,"rhythmSeedFloat")) rhythmSeedFloat = (float)json_real_value(j);
        if (auto j = json_object_get(root,"melodySeedFloat")) melodySeedFloat = (float)json_real_value(j);

        if (auto j = json_object_get(root,"rhythmSeedPending")) rhythmSeedPending = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"rhythmSeedPendingFloat")) rhythmSeedPendingFloat = (float)json_real_value(j);
        if (auto j = json_object_get(root,"melodySeedPending")) melodySeedPending = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"melodySeedPendingFloat")) melodySeedPendingFloat = (float)json_real_value(j);

        if (auto j = json_object_get(root,"rhythmPattern")) {
            if (json_is_array(j)) {
                size_t n = json_array_size(j);
                for (size_t i = 0; i < 16 && i < n; ++i) {
                    json_t* it = json_array_get(j, i);
                    if (it) rhythmPattern[i] = (json_integer_value(it) != 0);
                }
            }
        }
        if (auto j = json_object_get(root,"melodyPitchV")) {
            if (json_is_array(j)) {
                size_t n = json_array_size(j);
                for (size_t i = 0; i < 16 && i < n; ++i) {
                    json_t* it = json_array_get(j, i);
                    if (it) melodyPitchV[i] = (float)json_real_value(it);
                }
            }
        }

        // Always reseed RNGs from saved seeds so patch restore is deterministic
        reseedXoroshiroFromFloat(rhythmRng, rhythmSeedFloat);
        reseedXoroshiroFromFloat(melodyRng, melodySeedFloat);
    }

//return semitone parameter value with CV input added (if connected)
    float MeloDicer::getSemitoneParam(int sem)  {
        if (sem < 0 || sem > 11) return 0.f;
        float v =  params[SEMI0_PARAM + sem].getValue();
        // Add CV from left expander if present
        if (leftExpander.module && leftExpander.module->model == modelMeloDicerExpander) {
            if(leftExpander.module->inputs[MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + sem].isConnected()) {
                   v += leftExpander.module->inputs[MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + sem].getVoltage()/10.0f;
            }
        }
        v = clampv(v, 0.f, 1.f);
        return v;
    }

    //return octave parameter value with CV input added (if connected)
    float MeloDicer::getOctaveLoParam()  {
        float v = params[OCT_LO_PARAM].getValue();
        if (leftExpander.module && leftExpander.module->model == modelMeloDicerExpander) {
            if(leftExpander.module->inputs[MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT].isConnected()) {
                v += leftExpander.module->inputs[MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT].getVoltage()/10.0f*8.0f;
            }
        }
        v = clampv(v, 0.f, 8.f);
        return v;
    }

    //return octave parameter value with CV input added (if connected)
    float MeloDicer::getOctaveHiParam()  {
        float v = params[OCT_HI_PARAM].getValue();
        if (leftExpander.module && leftExpander.module->model == modelMeloDicerExpander) {
            if(leftExpander.module->inputs[MeloDicerIds::EXPANDER_OCT_HI_CV_INPUT].isConnected()) {
                v += leftExpander.module->inputs[MeloDicerIds::EXPANDER_OCT_HI_CV_INPUT].getVoltage()/10.0f*8.0f;
            }
        }
        v = clampv(v, 0.f, 8.f);
        return v;
    }

// ── CV2-aware parameter readers ──────────────────────────────────────────────
// These return the knob value + any active CV2 offset, clamped to the valid range.
// Call these instead of params[X].getValue() wherever the CV2 offset should apply.
float MeloDicer::getNoteValueParam()  { return clampv<float>(params[NOTE_VALUE_PARAM].getValue() + cv2Offsets[0], 0.f, 8.f); }
float MeloDicer::getVariationParam()  { return clampv<float>(params[VARIATION_PARAM].getValue()  + cv2Offsets[1], 0.f, 1.f); }
float MeloDicer::getLegatoParam()     { return clampv<float>(params[LEGATO_PARAM].getValue()     + cv2Offsets[2], 0.f, 1.f); }
float MeloDicer::getRestParam()       { return clampv<float>(params[REST_PARAM].getValue()       + cv2Offsets[3], 0.f, 1.f); }

// --- switch melody/rhythm mode (dice/realtime), caching/restoring state as needed ---    
void MeloDicer::switchMelodyMode() { engine.pe.switchMelodyMode(stepIndex, lastStepIndex); }

// --- switch melody/rhythm mode (dice/realtime), caching/restoring state as needed ---
void MeloDicer::switchRhythmMode() { engine.pe.switchRhythmMode(stepIndex, lastStepIndex); }


// pick a semitone (0..11) using weighted random, or -1 if no semitone is selected
// uses melody RNG
// returns -1 if no semitone is selected (all weights zero)
int MeloDicer::pickSemitoneWeighted() {
    float w[12]; for(int i=0;i<12;++i) w[i]=getSemitoneParam(i);
    return engine.pe.pickSemitone(w);
}

// generate a pitch V in 1V/oct format, return semitone via out parameter
//applies octave range and transpose
// uses melody RNG
// returns 0V and -1 semitone if no semitone is selected
// (should be rare, only if all semitone weights are zero)
    float MeloDicer::genPitchV(int& outSemitone) {
        return engine.pe.genPitch(outSemitone, makePatternInput());
    }
    // uses rhythm RNG
    //  biases shorter/longer values depending on VARIATION_PARAM (0..1)
    // 0.5 = no bias, <0.5 = longer notes, >0.5 = shorter notes
    // only varies within ±2 indices, and only to allowed note lengths
    // if no allowed variation, returns baseIdx unchanged
    // not sure of purpose of randomly picking from allowed set, but keeping it for now
    // (this is different from original MeloDicer behavior so probably needs removing)
    // if baseIdx is out of range, it is clamped to 0..8
    // if no allowed note lengths in range, returns baseIdx unchanged
    // allowed note lengths depend on noteVariationMask - note we defined NoteLength enum and allowednoteLengths function
    //so this could be changed to use that
    // (bit0: 1/8T, bit1: 1/16T, bit2: 1/32 & 1/32T)
    // e.g. mask=0b101 allows 1/8T, 1/4, 1/2, 1, 2, 4, 1/32, 1/32T
    //      mask=0b010 allows 1/16T, 1/4, 1/2, 1, 2, 4
    //      mask=0b000 allows only 1/4, 1/2, 1, 2, 4
    //      mask=0b111 allows all note lengths
    // if baseIdx is 0 or 8, variation is only towards inside (1 or 7)
    // if baseIdx is 1 or 7, variation is towards inside (0 or 2) or outside (2 or 6)
    // etc.// (this is handled by the lo/hi clamping)
    // if baseIdx is out of range, it is clamped to 0..8
    // if no allowed note lengths in range, returns baseIdx unchanged
    // uses rhythm RNG

// Convert semitone (0..11) to volts (1V/oct)
// 12 semitones per octave
float MeloDicer::semitoneToVolts(int semitone) {
    return semitone / 12.f;
}

    // regenerate rhythm pattern (uses rhythmRng) — skipped when locked
    // Build a PatternInput snapshot from current params/CV state
    PatternInput MeloDicer::makePatternInput() {
        PatternInput in;
        for (int i=0;i<12;++i) in.semiWeights[i]=getSemitoneParam(i);
        in.restProb          = getRestParam();
        in.variationAmount   = getVariationParam();
        in.octaveLo          = getOctaveLoParam();
        in.octaveHi          = getOctaveHiParam();
        in.transpose         = params[TRANSPOSE_PARAM].getValue();
        in.noteVariationMask = noteVariationMask;
        in.locked            = locked;
        return in;
    }

    void MeloDicer::redrawRhythmPattern() { engine.pe.redrawRhythm(makePatternInput()); }
    void  MeloDicer::redrawMelodyPattern() { engine.pe.redrawMelody(makePatternInput()); }

    void MeloDicer::rebuildSemiCache_() {
        float weights[12];
        for (int i = 0; i < 12; ++i) weights[i] = params[SEMI0_PARAM + i].getValue();
        engine.rebuildScaleCache(weights);
    }

    float MeloDicer::quantizeToScale(float vIn) { return engine.quantize(vIn); }

    // handle manual restart: place index so next increment lands on startStep, optionally redraw realtime patterns
    // If there are pending seeds and resetImmediate==true, apply them immediately (for RESET triggered reseed)
    void MeloDicer::handleRestart(bool manual, bool resetImmediate) {
        stepIndex = (startStep - 1 + 16) % 16;
        engine.gs.reset();          // clears gate, hold, pitch, pulse, semiPlayRemain
        prevExtGate = false;

        if (!locked) {
            if (resetImmediate) {
                engine.pe.applyPendingSeedsAndRedraw(makePatternInput());
            }
        }
        resetArmed = false;
    }

    // Helper to sample a seed value (float 0..10) from either SEED input (if connected) or internal RNG.
    // Helper to sample a seed value (float 0..10) from either SEED input (if connected) or internal RNG.
    // The action of sampling is performed when user presses DICE (or dice is triggered via gate assignment).
    float MeloDicer::sampleSeedFromSource() {
        // If SEED_INPUT is connected, use it (clamped) as sample-and-hold.
        if (inputs[SEED_INPUT].isConnected()) {
            float v = inputs[SEED_INPUT].getVoltage();
            v = clampv<float>(v, 0.f, 10.f);
            return v;
        }
        // Otherwise fall back to internal RNG. 
        float u = rack::random::uniform(); //  default internal RNG
        // scale to 0..10
        return (float)(u * 10.0);
    }



// ---------------- Helper: phrase boundary hook -------------------------------
// Called at phrase boundary (stepIndex wraps from endStep back to startStep).
// Seeds are applied FIRST so the subsequent redraw uses the new RNG state.
void MeloDicer::onPhraseBoundary_() {
    engine.pe.applyPendingSeedsAndRedraw(makePatternInput());
}

// ---------------- Helper: reset hook -----------------------------------------
void MeloDicer::onReset() {
    Module::onReset();
    initialize();   // resets all module state to defaults
    clock.reset();  // resets ClockEngine timing
}


// Returns note length index: NOTE VALUE sets the base, VARIATION randomly biases it.
// NOTEVALS.allowedPPQN bitmask: 1=PPQN1, 2=PPQN4, 4=PPQN24
// ppqnSetting raw values: 1, 4, 24 — must be converted to bitmask before use.
int MeloDicer::getNoteLenIdx_() { 
    // This function is no longer used directly by executeModeA/B as the random value
    // is now consumed inside those functions. It's kept for potential external use
    // but should be called with a random float argument if used.
    // For now, it's safe to remove its declaration and definition if not used elsewhere.
    // However, to resolve the compilation error, we'll remove its definition and declaration.
    return 0; // Should not be reached
}
int MeloDicer::computeNoteLengthIdx(int requestedIdx, int ppqnMask) { return engine.computeNoteLengthIdx(requestedIdx, ppqnMask); }


// quantize pitch to semitone index (0..11) and octave offset (integer)
float MeloDicer::quantizePitch(int semitoneIndex, int octaveOffset) {
    return octaveOffset + semitoneIndex / 12.f;
}

// ---------------- Helper: update step LEDs -------------------------------
// activeSemitone: 0..11 for currently playing semitone, or -1 if
void MeloDicer::updateStepLEDs_(float sampleTime)
{
    // Green channel (ch0) is managed by VCVLightSlider widget automatically.
    // We only drive the red channel (ch1) = "note playing" flash.
    // Always update red — semiPlayRemain decays each step so values always change.
    for (int i = 0; i < 12; i++) {
        float redLevel = engine.gs.semiLedBrightness(i);
        lights[SEMI_LED_START + 2*i + 1].setBrightness(redLevel);
    }
}




// ---------------- Replacement for process() ---------------------------------
// Full logic for all modes inline here
// Calls helper functions as needed
void MeloDicer::process(const ProcessArgs& args) {
    // --- UI button toggles ---
    // Dice button: pressing generates new random values regardless of current mode
    // (manual: "With each press, meloDICER generates new random values")
    // It also switches you back INTO dice-mode if you were in realtime-mode.
    if (diceRTrig.process(params[DICE_R_PARAM].getValue())) {
        rhythmMode = 0; // enter dice-mode
        rhythmSeedPendingFloat = sampleSeedFromSource();
        rhythmSeedPending = true;
    }
    if (diceMTrig.process(params[DICE_M_PARAM].getValue())) {
        melodyMode = 0; // enter dice-mode
        melodySeedPendingFloat = sampleSeedFromSource();
        melodySeedPending = true;
    }
    if (lockTrig.process(params[LOCK_PARAM].getValue()))     locked = !locked;
    if (muteTrig.process(params[MUTE_PARAM].getValue()))     muted  = !muted;

    if (modeATrig.process(params[MODE_A_PARAM].getValue())) modeSelect = 0;
    if (modeBTrig.process(params[MODE_B_PARAM].getValue())) modeSelect = 1;
    if (modeCTrig.process(params[MODE_C_PARAM].getValue())) modeSelect = 2;
    if (modeDTrig.process(params[MODE_D_PARAM].getValue())) modeSelect = 3;

    // Mode lamps: only write when modeSelect changes (~never per sample)
    if (modeSelect != lastModeSelect) {
        lights[MODE_A_LIGHT].setBrightness(modeSelect == 0 ? 1.f : 0.f);
        lights[MODE_B_LIGHT].setBrightness(modeSelect == 1 ? 1.f : 0.f);
        lights[MODE_C_LIGHT].setBrightness(modeSelect == 2 ? 1.f : 0.f);
        lights[MODE_D_LIGHT].setBrightness(modeSelect == 3 ? 1.f : 0.f);
        lastModeSelect = modeSelect;
    }

    // ── Centralised clock tick (processes CLK IN once, before all mode handlers) ──
    // Derives bpm from external clock period or BPM knob, emits sixteenthEdge + quarterEdge.
    clock.process(
        inputs[CLK_INPUT].getVoltage(),
        inputs[CLK_INPUT].isConnected(),
        params[BPM_PARAM].getValue(),
        ppqnSetting,
        args.sampleTime
    );
    bpm = clock.bpm;  // keep module-level bpm in sync for note duration calculations

    // ── Semitone fader cache: rebuild active list when any fader changes ──
    // O(12) compares per sample — far cheaper than the quantiser search.
    {
        bool faderDirty = false;
        for (int i = 0; i < 12 && !faderDirty; ++i) {
            float w = clampv<float>(params[SEMI0_PARAM + i].getValue(), 0.f, 1.f);
            if (std::fabs(w - faderCache[i]) > 1e-5f) faderDirty = true;
        }
        if (faderDirty || activeSemiCount == 0) rebuildSemiCache_();
    }

    engine.updateWindow(
        params[PATTERN_LENGTH_PARAM].getValue(), inputs[LENGTH_INPUT].getVoltage(), inputs[LENGTH_INPUT].isConnected(),
        params[PATTERN_OFFSET_PARAM].getValue(), inputs[OFFFSET_INPUT].getVoltage(), inputs[OFFFSET_INPUT].isConnected()
    );

    // Pre-process asynchronous triggers
    bool gate1Rise = g1Trig.process(inputs[GATE1_INPUT].getVoltage());
    bool gate2Rise = g2Trig.process(inputs[GATE2_INPUT].getVoltage());

    bool runStart = false;
    if (inputs[RUN_GATE_INPUT].isConnected()) 
    {
      bool runGateTrigHigh=runGateTrig.process(inputs[RUN_GATE_INPUT].getVoltage(), 0.1f, 2.f);
      bool runGateBtnHigh = runGateBtn.process(params[RUN_GATE_PARAM].getValue());
      if (runGateTrigHigh || runGateBtnHigh){
        runGateActive = !runGateActive;
        runStart = runGateActive;
        runPulse.trigger(1e-3f);
      }
    }
    else 
    {
      if(runGateBtn.process(params[RUN_GATE_PARAM].getValue()))
      {
        runGateActive = !runGateActive;
        runStart = runGateActive;
        runPulse.trigger(1e-3f);
      }
    };
   // bool runGate = runPulse.process(args.sampleTime);
    if(runStart) {
    //   newBar = true;
    //   newPhrase = true;
      //resetArmed = true;
    }
   // lights[RUN_GATE_LIGHT].setBrightnessSmooth(runGateActive ? 1.f : LIGHT_OFF, args.sampleTime);
    lights[RUN_GATE_LIGHT].setBrightness(runGateActive? 1.f : 0.f);
    outputs[RUN_GATE_OUTPUT].setVoltage(runGateActive ? 10.f : 0.f);

    //if(schmittTrigger(resetTrigHigh,inputs[RESET_TRIGGER_INPUT].getVoltage()) && trigGenerator.remaining <= 0) resetArmed = true;
    //if(buttonTrigger(resetBtnHigh,params[RESET_BUTTON_PARAM].getValue()) && trigGenerator.remaining <= 0) resetArmed = true;
    if (resetTrig.process(inputs[RESET_TRIGGER_INPUT].getVoltage(),0.1f, 2.f))  resetArmed = true;
    if (resetBtn.process(params[RESET_BUTTON_PARAM].getValue()))  resetArmed = true;
    lights[RESET_LIGHT].setBrightnessSmooth(resetArmed ? 1.f : 0.f, args.sampleTime);

    // --- RESET input: immediate phrase restart and apply pending seeds ---
    if (resetArmed && runGateActive)
    {
        handleRestart(/*manual=*/true, /*resetImmediate=*/true);
        resetPulse.trigger(1e-3f);  // fire reset output trigger
    }
    else if (!runGateActive) {
        resetArmed = false;
    }

    // Drive RESET_TRIGGER_OUTPUT as a 1ms pulse on reset
    outputs[RESET_TRIGGER_OUTPUT].setVoltage(resetPulse.process(args.sampleTime) ? 10.f : 0.f);

    // --- Gate input assignments ---
    if (gate1Rise) {
        switch (gate1Assign) {
            case 0: // Toggle Dice R MODE
                rhythmMode = (1-rhythmMode ); // toggle 0/1
                break;
            case 1: // Re-dice R (arm) — only if currently in dice-mode (manual: "only works if in dice-mode")
                if (rhythmMode == 0) {
                    rhythmSeedPendingFloat = sampleSeedFromSource();
                    rhythmSeedPending = true;
                }
                break;
            case 2: // Re-dice M (arm) — only if currently in dice-mode
                if (melodyMode == 0) {
                    melodySeedPendingFloat = sampleSeedFromSource();
                    melodySeedPending = true;
                }
                break;
            case 3: // Restart now
                handleRestart(/*manual=*/true, /*resetImmediate=*/true);
                break;
        }
    }
    {
        bool g2High = inputs[GATE2_INPUT].getVoltage() >= 1.f;
        switch (gate2Assign) {
            case 0: // TGL DICE M — toggle on rising edge
                if (gate2Rise) melodyMode = (1 - melodyMode);
                break;
            case 1: // Re-dice M — rising edge, only in dice-mode
                if (gate2Rise && melodyMode == 0) {
                    melodySeedPendingFloat = sampleSeedFromSource();
                    melodySeedPending = true;
                }
                break;
            case 2: // MUTE — manual: positive edge=activate, negative edge=deactivate
                // With INV GATE (muteBehavior==2): invert the polarity
                {
                    bool shouldMute = (muteBehavior == 2) ? !g2High : g2High;
                    if (shouldMute != muted) {
                        muted = shouldMute;
                        if (!muted && muteBehavior == 1)
                            handleRestart(/*manual=*/true, /*resetImmediate=*/true);
                    }
                }
                break;
            case 3: // RESTART — rising edge
                if (gate2Rise) handleRestart(/*manual=*/true, /*resetImmediate=*/true);
                break;
        }
    }

    // --- Mode dispatch (only if running) ---
    if (runGateActive) {
        switch (modeSelect) {
            case 0: handleModeA_(args);                 break;
            case 1: handleModeB_(args, gate1Rise);      break;
            case 2: handleModeC_(args);                 break;
            case 3: handleModeD_(args);                 break;
            default: break;
        }
    }



    // --- CV IN routing (post-mode, keeps behavior from your code) ---
    if (inputs[CV1_INPUT].isConnected()) {
        float v = inputs[CV1_INPUT].getVoltage();
        switch (cv1Mode) {
            case 0: outputs[CV_OUTPUT].setVoltage(currentPitchV + v); break;            // Add Seq
            case 1: {                                                                     // Transpose quantized
                float off = std::round(v * 12.f) / 12.f;
                outputs[CV_OUTPUT].setVoltage(currentPitchV + off);
            } break;
            case 2: {                                                                     // Mod LO
                float lo = clampv<float>(params[OCT_LO_PARAM].getValue() + v * 0.25f, 0.f, 8.f);
                params[OCT_LO_PARAM].setValue(lo);
                outputs[CV_OUTPUT].setVoltage(currentPitchV);
            } break;
            case 3: {                                                                     // Mod HI
                float hi = clampv<float>(params[OCT_HI_PARAM].getValue() + v * 0.25f, 0.f, 8.f);
                params[OCT_HI_PARAM].setValue(hi);
                outputs[CV_OUTPUT].setVoltage(currentPitchV);
            } break;
        }
} else {
    outputs[CV_OUTPUT].setVoltage(currentPitchV);
}

    // CV IN 2: 0..5V per manual, ADDED to knob as a temporary offset.
    // We store the offset separately so the knob value is never permanently drifted.
    // cv2Offsets[] is consumed at point-of-use (getCV2NoteValue etc).
    for (int i = 0; i < 4; ++i) cv2Offsets[i] = 0.f;  // clear each sample
    if (inputs[CV2_INPUT].isConnected()) {
        float v    = clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f);
        float norm = v / 5.f; // 0..1
        // Only one mode active at a time; set that offset slot
        if (cv2Mode == 0) cv2Offsets[0] = norm * 8.f;   // NOTE_VALUE range 0..8
        if (cv2Mode == 1) cv2Offsets[1] = norm;           // VARIATION  range 0..1
        if (cv2Mode == 2) cv2Offsets[2] = norm;           // LEGATO     range 0..1
        if (cv2Mode == 3) cv2Offsets[3] = norm;           // REST       range 0..1
    }
    
    
    // Gate output: engine.gs.process() ticks the pulse and returns raw gate voltage
    float gateV = engine.gs.process(args.sampleTime);
    if (muteBehavior == 2) gateV = (gateV > 1.f) ? 0.f : 10.f;
    if (muted)             gateV = (muteBehavior == 2) ? 10.f : 0.f;
    outputs[GATE_OUTPUT].setVoltage(gateV);


    // DICE light policy: bright when enabled, also bright when a seed is armed
    // float rDiceLight = diceR ? 1.f : 0.1f;
    // float mDiceLight = diceM ? 1.f : 0.1f;
    // if (rhythmSeedPending) rDiceLight = 1.f;
    // if (melodySeedPending) mDiceLight = 1.f;
    lights[RHYTHM_DICE_LIGHT].setBrightness(rhythmSeedPending ? 1.f : 0.1f);
    lights[MELODY_DICE_LIGHT].setBrightness(melodySeedPending ? 1.f : 0.1f);
    lights[LOCK_LIGHT].setBrightness(locked ? 1.f : 0.f);
    lights[MUTE_LIGHT].setBrightness(muted ? 1.f : 0.f);

    // Seed monitor out (prefers armed value if present)
    float outSeed = rhythmSeedPending ? rhythmSeedPendingFloat : rhythmSeedFloat;
    outSeed = clampv<float>(outSeed, 0.f, 10.f);
    outputs[SEED_OUTPUT].setVoltage(outSeed);
}

// ---------------- Mode A: internal clock with offset -------------------
// stepEdge: true on 1/16 tick (internal or CLK input)
//  Also handles LED updates and semitone LEDs
// Uses shared triggerStepEvent_() helper.
//  Also handles phrase boundary reseeding and redraw.
//  Also handles first note logic when starting from reset.
//  Also handles mute logic (no output, but still advances and updates LEDs)
void MeloDicer::handleModeA_(const ProcessArgs& args) {
    if (engine.executeModeA(clock, getRestParam(), getLegatoParam(), getNoteValueParam(), makePatternInput())) {
        // Sample new seeds for realtime modes, then let onPhraseBoundary_()
        // apply any pending seeds (Dice-armed or realtime) before redrawing.
        if (melodyMode == 1) { melodySeedPendingFloat = sampleSeedFromSource(); melodySeedPending = true; }
        if (rhythmMode == 1) { rhythmSeedPendingFloat = sampleSeedFromSource(); rhythmSeedPending = true; }
        onPhraseBoundary_();
    }

    for (int i = 0; i < 16; ++i) {
        lights[STEP_LIGHTS_START + i].setBrightness(engine.getStepLightBrightness(i));
    }
    lastStepIndex = stepIndex;

    // updateStepLEDs_ handles red flash channel; slider widget handles green.
    updateStepLEDs_(args.sampleTime);
}

// ---------------- Mode B: external gate with offset -------------------
// gate1Edge: true on rising edge of GATE1 input
//  Also handles LED updates and semitone LEDs  
// Uses shared triggerStepEvent_() helper.
//  Also handles phrase boundary reseeding and redraw.
//  Also handles first note logic when gate held high continuously.
void MeloDicer::handleModeB_(const ProcessArgs& args, bool gate1Rise) {
    bool gate1High = inputs[GATE1_INPUT].getVoltage() >= 1.f;
    if (engine.executeModeB(gate1Rise, gate1High, getRestParam(), getLegatoParam(), getNoteValueParam(), makePatternInput())) {
        if (melodyMode == 1) { melodySeedPendingFloat = sampleSeedFromSource(); melodySeedPending = true; }
        if (rhythmMode == 1) { rhythmSeedPendingFloat = sampleSeedFromSource(); rhythmSeedPending = true; }
        onPhraseBoundary_();
    }

    for (int i = 0; i < 16; ++i) {
        lights[STEP_LIGHTS_START + i].setBrightness(engine.getStepLightBrightness(i));
    }
    lastStepIndex = stepIndex;

    updateStepLEDs_(args.sampleTime);
}


// ---------------- Mode C: Quantizer 1 ---------------------------------------
// quarterEdge: true on one-sample quarter-note pulse from ClockEngine
// Latches and quantizes CV2 on each quarter-note edge; steps through pattern window.
void MeloDicer::handleModeC_(const ProcessArgs& args) {
    float inCV = inputs[CV2_INPUT].isConnected() ? clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f) : 0.f;
    engine.executeModeC(clock, inCV);

    // Update Ring LEDs
    for (int i = 0; i < 16; ++i) {
        lights[STEP_LIGHTS_START + i].setBrightness(engine.getStepLightBrightness(i));
    }

    lastStepIndex = stepIndex;
    updateStepLEDs_(args.sampleTime);
}


// ---------------- Mode D: Quantizer 2 (second lane or alt flavor) -----------
//  
void MeloDicer::handleModeD_(const ProcessArgs& args) {
    bool gateHigh = inputs[GATE2_INPUT].isConnected() && inputs[GATE2_INPUT].getVoltage() > 1.f;
    float inCV = inputs[CV2_INPUT].isConnected() ? clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f) : 0.f;
    
    engine.executeModeD(gateHigh, inCV);

    for (int i = 0; i < 16; ++i) {
        lights[STEP_LIGHTS_START + i].setBrightness(engine.getStepLightBrightness(i));
    }
    updateStepLEDs_(args.sampleTime);
}


Model* modelMeloDicer = createModel<MeloDicer, MeloDicerWidget>("MeloDicer");

void init(rack::Plugin* p) {
	pluginInstance = p;
	p->addModel(modelMeloDicer);
	p->addModel(modelMeloDicerExpander);
}