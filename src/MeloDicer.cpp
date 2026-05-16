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

#include "MeloDicerDNAExpander.hpp"
#include "MeloDicerExpander.hpp"
#include "MeloDicerPolyVoiceExpander.hpp"
#include "MeloDicerWidget.hpp"
#include "MeloDicer.hpp"
#include "PatternEngine.hpp"
#include "GateState.hpp"

using namespace rack;
using namespace MeloDicerIds;

Plugin* pluginInstance = nullptr;

void MeloDicer::reseedXoroshiroFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat) {
    engine.pe.seedRngFromFloat(rng, seedFloat);
}

void MeloDicer::onSampleRateChange(const SampleRateChangeEvent& e) {
    lightDivider.setDivision(std::max(1, (int)std::round(e.sampleRate / 90.f)));
    controlDivider.setDivision(std::max(1, (int)std::round(e.sampleRate / 1500.f)));
}

MeloDicer::MeloDicer() {
        using namespace MeloDicerIds;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        // Main controls
        configSwitch(NOTE_VALUE_PARAM, 0.f, 7.f, 2.f, "Note value",
            {"1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32"});
        configParam(VARIATION_PARAM,   0.f, 1.f, 0.5f, "Variation (longer–shorter)");
        configParam(LEGATO_PARAM,      0.f, 1.f, 0.10f, "Legato probability");
        configParam(REST_PARAM,        0.f, 1.f, 0.10f, "Rest probability");
        configParam(ACCENT_KNOB,       0.f, 1.f, 0.25f, "Accent gate probability");  // New
        configParam(TRANSPOSE_PARAM,  -12.f, 12.f, 0.f, "Transpose (semitones)");
        //Pattern ring controls
        // Main window controls (Always present)
        configParam(PATTERN_LENGTH_PARAM, 1.f, 16.f, 16.f, "Pattern length");
        configParam(PATTERN_OFFSET_PARAM, 0.f, 15.f, 0.f, "Pattern offset");

        // DNA Action Buttons (Main module only configures these if no expander)
        // If an expander is present, these are configured on the expander.
        // We configure them here as a fallback for standalone operation.
        configButton(DNA_SCRAMBLE_ALL_PARAM, "Scramble ALL DNA");
        configButton(DNA_SCRAMBLE_R_PARAM,   "Scramble Rhythm");
        configButton(DNA_SCRAMBLE_V_PARAM,   "Scramble Variation");
        configButton(DNA_SCRAMBLE_L_PARAM,   "Scramble Legato");
        configButton(DNA_SCRAMBLE_M_PARAM,   "Scramble Melody");
        configButton(DNA_SCRAMBLE_O_PARAM,   "Scramble Octave");

        configButton(DNA_RESET_ALL_PARAM,    "Reset ALL DNA");
        configButton(DNA_RESET_R_PARAM,      "Reset Rhythm");
        configButton(DNA_RESET_V_PARAM,      "Reset Variation");
        configButton(DNA_RESET_L_PARAM,      "Reset Legato");
        configButton(DNA_RESET_M_PARAM,      "Reset Melody");
        configButton(DNA_RESET_O_PARAM,      "Reset Octave");

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
        configButton(MODE_PARAM,   "Mode (Cycle A-B-C-D)");
        configButton(RESET_BUTTON_PARAM,  "Reset");
        configButton(RUN_GATE_PARAM,      "Run/Stop");

        // I/O
        configInput(CLK_INPUT,   "Clock");
        configInput(GATE1_INPUT, "Gate In 1");
        configInput(GATE2_INPUT, "Gate In 2");
        configInput(CV1_INPUT,   "CV In 1");
        configInput(CV2_INPUT,   "CV In 2");
        configInput(ACCENT_CV_INPUT, "Accent Probability CV");  // New

        // --- RNG/SEED ADDITION: new inputs
        configInput(RESET_TRIGGER_INPUT, "Reset (phrase restart)");
        configInput(SEED_INPUT,   "Seed CV (0..10V)");
        configInput(LENGTH_INPUT, "Pattern Length CV (0..10V)");
        configInput(OFFSET_INPUT, "Pattern Offset CV (0..10V)");
        configInput(RUN_GATE_INPUT, "Run/Stop Gate");

        // DNA Gate Inputs (Main module only configures these if no expander)
        configInput(DNA_SCRAMBLE_ALL_INPUT, "Scramble ALL DNA Gate");
        configInput(DNA_SCRAMBLE_R_INPUT,   "Scramble Rhythm Gate");
        configInput(DNA_SCRAMBLE_V_INPUT,   "Scramble Variation Gate");
        configInput(DNA_SCRAMBLE_L_INPUT,   "Scramble Legato Gate");
        configInput(DNA_SCRAMBLE_A_INPUT,   "Scramble Accent Gate");  // New
        configInput(DNA_SCRAMBLE_M_INPUT,   "Scramble Melody Gate");
        configInput(DNA_SCRAMBLE_O_INPUT,   "Scramble Octave Gate");

        configInput(DNA_RESET_ALL_INPUT,    "Reset ALL DNA Gate");
        configInput(DNA_RESET_R_INPUT,      "Reset Rhythm Gate");
        configInput(DNA_RESET_V_INPUT,      "Reset Variation Gate");
        configInput(DNA_RESET_L_INPUT,      "Reset Legato Gate");
        configInput(DNA_RESET_A_INPUT,      "Reset Accent Gate");  // New
        configInput(DNA_RESET_M_INPUT,      "Reset Melody Gate");
        configInput(DNA_RESET_O_INPUT,      "Reset Octave Gate");

        configOutput(GATE_OUTPUT,           "Gate");
        configOutput(CV_OUTPUT,             "1V/Oct");
        configOutput(SEED_OUTPUT,           "Seed Voltage Out (0..10V)");
        configOutput(RESET_TRIGGER_OUTPUT,  "Reset Trigger Out");
        configOutput(RUN_GATE_OUTPUT,       "Run Gate Out");
        configOutput(TIE_OUTPUT,            "Tie Gate (high on Tie)");          // New
        configOutput(LEGATO_OUTPUT,         "Legato Gate (high on Legato/Max)"); // New
        configOutput(ACCENT_OUTPUT,         "Accent Gate (high when accented)");  // New

        // Seed RNGs with a random value — safe to call here (uses rack::random, not inputs[])
        rhythmSeedFloat = rack::random::uniform() * 10.f;
        melodySeedFloat = rack::random::uniform() * 10.f;
        reseedXoroshiroFromFloat(rhythmRng, rhythmSeedFloat);
        //stochasticSeedFloat = rack::random::uniform() * 10.f;
        //reseedXoroshiroFromFloat(stochasticRng, stochasticSeedFloat);
        reseedXoroshiroFromFloat(melodyRng, melodySeedFloat);
        rhythmSeedPendingFloat = rhythmSeedFloat;
        melodySeedPendingFloat = melodySeedFloat;

        // Initialize dividers based on current engine sample rate
        //onSampleRateChange({APP->engine->getSampleRate(), APP->engine->getSampleRate()});

        // Default patterns: all gates on, CV at 0V (C0), semitone 0
        // genPitchV() reads params[] which aren't valid yet, so use safe literals
        for (int i = 0; i < 16; ++i) {
            rhythmPattern[i]  = true;
            engine.pe.rhythmRandom[i] = 1.0f;
            engine.pe.variationRandom[i] = 0.5f;
            engine.pe.legatoRandom[i] = 0.0f;
            engine.pe.melodyRandom[i] = 0.5f;
            engine.pe.octaveRandom[i] = 0.5f;
            for (int v = 0; v < 7; v++) {
                engine.pe.polyRhythmRandom[v][i] = (float)rack::random::uniform(); // Seed with random for immediate DNA feedback
                engine.pe.polyMelodyRandom[v][i] = 0.5f;
                engine.pe.polyOctaveRandom[v][i] = 0.5f;
            }
            melodyPitchV[i]   = 0.f;   // C0 = 0V
            melodySemitone[i] = 0;     // semitone C
        }

        initialize();
    }

void MeloDicer::updateExpanderPointers() {
    cachedExpander = nullptr;
    cachedDnaExpander = nullptr;
    cachedPolyVoiceExpander = nullptr;

    scaleExpanderCount = 0;
    dnaExpanderCount = 0;
    polyExpanderCount = 0;

    // Walk expander chains in both directions to find any connected Melodicer expanders
    auto scan = [&](Module* start, bool left) {
        Module* curr = start;
        int depth = 0;
        while (curr && depth < 8) { // 8 is a safe depth limit for Rack chains
            if (curr->model == modelMeloDicerExpander) {
                if (!cachedExpander) cachedExpander = dynamic_cast<MeloDicerExpander*>(curr);
                scaleExpanderCount++;
            }
            else if (curr->model == modelMeloDicerDNAExpander) {
                if (!cachedDnaExpander) cachedDnaExpander = dynamic_cast<MeloDicerDNAExpander*>(curr);
                dnaExpanderCount++;
            }
            else if (curr->model == modelMeloDicerPolyVoiceExpander) {
                if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = dynamic_cast<MeloDicerPolyVoiceExpander*>(curr);
                polyExpanderCount++;
            }
            else {
                break; // Stop if we hit a module that isn't part of this system
            }
            curr = left ? curr->leftExpander.module : curr->rightExpander.module;
            depth++;
        }
    };

    scan(leftExpander.module, true);
    scan(rightExpander.module, false);
    
    // Initialize parameter manager with main module and expander pointers
    paramManager = std::make_unique<ParameterManager>(this, &cachedExpander, &cachedPolyVoiceExpander);
}

  void MeloDicer::updateScaleMask() {
        uint16_t newMask = 0;
        if (lastSelectedScale >= 0 && lastSelectedScale < (int)BITWIG_SCALES.size()) {
            for (int interval : BITWIG_SCALES[lastSelectedScale].intervals) {
                newMask |= (1 << ((scaleRoot + interval) % 12));
            }
        } else {
            newMask = 0xFFF;
        }

        if (lockScaleNotes) {
            // Capture current fader weights before redistribution
            float currentWeights[12];
            for (int i = 0; i < 12; i++) {
                currentWeights[i] = params[SEMI0_PARAM + i].getValue();
            }

            // Redistribute weight from out-of-scale notes to the nearest in-scale notes
            float redistributedWeights[12];
            for (int i = 0; i < 12; i++) redistributedWeights[i] = 0.0f;

            for (int i = 0; i < 12; i++) {
                if (newMask & (1 << i)) {
                    // Already in scale, keep the weight
                    redistributedWeights[i] += currentWeights[i];
                } else if (currentWeights[i] > 0.0001f) {
                    float val = currentWeights[i];
                    // Search for nearest neighbors in the circular 12-semitone space
                    int distUp = 13, distDown = 13;
                    for (int d = 1; d <= 6; d++) {
                        if (newMask & (1 << ((i + d) % 12))) { distUp = d; break; }
                    }
                    for (int d = 1; d <= 6; d++) {
                        if (newMask & (1 << ((i - d + 12) % 12))) { distDown = d; break; }
                    }

                    if (distUp < distDown) {
                        redistributedWeights[(i + distUp) % 12] += val;
                    } else if (distDown < distUp) {
                        redistributedWeights[(i - distDown + 12) % 12] += val;
                    } else if (distUp <= 6) {
                        // Equidistant neighbors (e.g. C# moving to C and D)
                        redistributedWeights[(i + distUp) % 12] += val * 0.5f;
                        redistributedWeights[(i - distDown + 12) % 12] += val * 0.5f;
                    }
                }
            }

            // Apply the redistributed values and lock the faders
            for (int i = 0; i < 12; i++) {
                bool inScale = (newMask & (1 << i));
                ParamQuantity* pq = getParamQuantity(SEMI0_PARAM + i);
                if (pq) {
                    if (!inScale) {
                        params[SEMI0_PARAM + i].setValue(0.f);
                        pq->minValue = 0.f;
                        pq->maxValue = 0.f;
                    } else {
                        params[SEMI0_PARAM + i].setValue(clampv(redistributedWeights[i], 0.f, 1.f));
                        pq->minValue = 0.f;
                        pq->maxValue = 1.f;
                    }
                }
            }
        } else {
            // Unlock logic: allow faders to move freely within 0..1 range
            for (int i = 0; i < 12; i++) {
                ParamQuantity* pq = getParamQuantity(SEMI0_PARAM + i);
                if (pq) {
                    pq->minValue = 0.f;
                    pq->maxValue = 1.f;
                }
            }
        }
  }

  void MeloDicer::initialize(){
        cv1Mode = 0;
        cv2Mode = 0;
        gate1Assign = 0;
        gate2Assign = 1;
        invertMuteLogic = false;
        restartOnUnmute = false;
        lastModeSelect = -1;
        scaleRoot = 0;
        lastSelectedScale = -1;
        lockScaleNotes = false;
        updateScaleMask();

        engine.reset();

        bpm = 120.f;
        clock.reset();
        prevExtGate = false;
        currentPitchV = 0.f;
        melodySeedCached = false;
        cachedExpander = nullptr; // Initialize cached expander
        rhythmSeedCached = false;
        updateExpanderPointers();
  }

    // --- serialization ---
    json_t* MeloDicer::dataToJson() {
        json_t* root = json_object();
        json_object_set_new(root,"cv1Mode", json_integer(cv1Mode));
        json_object_set_new(root,"cv2Mode", json_integer(cv2Mode));
        json_object_set_new(root,"gate1Assign", json_integer(gate1Assign));
        json_object_set_new(root,"gate2Assign", json_integer(gate2Assign));
        json_object_set_new(root,"invertMuteLogic", json_boolean(invertMuteLogic));
        json_object_set_new(root,"restartOnUnmute", json_boolean(restartOnUnmute));
        json_object_set_new(root,"noteVariationMask", json_integer(noteVariationMask));
        json_object_set_new(root,"ppqnSetting", json_integer(ppqnSetting));
        json_object_set_new(root,"modeSelect", json_integer(modeSelect));
        json_object_set_new(root,"lightTheme", json_boolean(lightTheme));
        json_object_set_new(root,"scaleRoot", json_integer(scaleRoot));
        json_object_set_new(root,"lastSelectedScale", json_integer(lastSelectedScale));
        json_object_set_new(root,"lockScaleNotes", json_boolean(lockScaleNotes));

        json_object_set_new(root,"locked", json_boolean(locked));
        json_object_set_new(root,"muted", json_boolean(muted));

        // new fields
        json_object_set_new(root,"rhythmMode", json_integer(rhythmMode));
        json_object_set_new(root,"melodyMode", json_integer(melodyMode));
        json_object_set_new(root,"startStep", json_integer(startStep));
        json_object_set_new(root,"endStep", json_integer(endStep));
        json_object_set_new(root,"dnaLength", json_integer(engine.dnaLength));
        json_object_set_new(root,"dnaOffset", json_integer(engine.dnaOffset));

        // store seed floats and parts
        json_object_set_new(root,"rhythmSeedFloat", json_real((float)rhythmSeedFloat));
        json_object_set_new(root,"melodySeedFloat", json_real((float)melodySeedFloat));


        // pending
        json_object_set_new(root,"rhythmSeedPending", json_boolean(rhythmSeedPending));
        json_object_set_new(root,"rhythmSeedPendingFloat", json_real((float)rhythmSeedPendingFloat));
        json_object_set_new(root,"melodySeedPending", json_boolean(melodySeedPending));
        json_object_set_new(root,"melodySeedPendingFloat", json_real((float)melodySeedPendingFloat));
        json_object_set_new(root,"numPolyVoices", json_integer(engine.numPolyVoices));

        // serialize rhythmPattern as array of ints 0/1
        json_t* rarr = json_array();
        for (int i = 0; i < 16; ++i) json_array_append_new(rarr, json_integer(rhythmPattern[i] ? 1 : 0));
        json_object_set_new(root,"rhythmPattern", rarr);

        // serialize random buffers for pattern stability
        json_t* rrarr = json_array();
        json_t* vrarr = json_array();
        json_t* lrarr = json_array();
        json_t* mrarr = json_array();
        json_t* orarr = json_array();
        for (int i = 0; i < 16; ++i) {
            json_array_append_new(rrarr, json_real(engine.pe.rhythmRandom[i]));
            json_array_append_new(vrarr, json_real(engine.pe.variationRandom[i]));
            json_array_append_new(lrarr, json_real(engine.pe.legatoRandom[i]));
            json_array_append_new(mrarr, json_real(engine.pe.melodyRandom[i]));
            json_array_append_new(orarr, json_real(engine.pe.octaveRandom[i]));
        }
        json_object_set_new(root,"rhythmRandom", rrarr);
        json_object_set_new(root,"variationRandom", vrarr);
        json_object_set_new(root,"legatoRandom", lrarr);
        json_object_set_new(root,"melodyRandom", mrarr);
        json_object_set_new(root,"octaveRandom", orarr);

        // // serialize poly random buffers for identity stability
        // json_t* prarr = json_array();
        // json_t* pmarr = json_array();
        // json_t* poarr = json_array();
        // for (int v = 0; v < 7; v++) {
        //     for (int i = 0; i < 16; i++) {
        //         json_array_append_new(prarr, json_real(engine.pe.polyRhythmRandom[v][i]));
        //         json_array_append_new(pmarr, json_real(engine.pe.polyMelodyRandom[v][i]));
        //         json_array_append_new(poarr, json_real(engine.pe.polyOctaveRandom[v][i]));
        //     }
        // }
        // json_object_set_new(root, "polyRhythmRandom", prarr);
        // json_object_set_new(root, "polyMelodyRandom", pmarr);
        // json_object_set_new(root, "polyOctaveRandom", poarr);

        // serialize poly random buffers for identity stability
        json_t* prarr = json_array();
        json_t* pmarr = json_array();
        json_t* poarr = json_array();
        for (int v = 0; v < 7; v++) {
            for (int i = 0; i < 16; i++) {
                json_array_append_new(prarr, json_real(engine.pe.polyRhythmRandom[v][i]));
                json_array_append_new(pmarr, json_real(engine.pe.polyMelodyRandom[v][i]));
                json_array_append_new(poarr, json_real(engine.pe.polyOctaveRandom[v][i]));
            }
        }
        json_object_set_new(root, "polyRhythmRandom", prarr);
        json_object_set_new(root, "polyMelodyRandom", pmarr);
        json_object_set_new(root, "polyOctaveRandom", poarr);

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
        if (auto j = json_object_get(root,"invertMuteLogic")) invertMuteLogic = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"restartOnUnmute")) restartOnUnmute = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"noteVariationMask")) noteVariationMask = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"ppqnSetting")) ppqnSetting = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"modeSelect")) modeSelect = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"lightTheme")) lightTheme = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"scaleRoot")) scaleRoot = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"lastSelectedScale")) lastSelectedScale = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"lockScaleNotes")) lockScaleNotes = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"locked")) locked = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"muted")) muted = (bool)json_boolean_value(j);

        if (auto j = json_object_get(root,"rhythmMode")) rhythmMode = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"melodyMode")) melodyMode = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"startStep")) startStep = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"endStep")) endStep = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"dnaLength")) engine.dnaLength = (int)json_integer_value(j);
        if (auto j = json_object_get(root,"dnaOffset")) engine.dnaOffset = (int)json_integer_value(j);

        if (auto j = json_object_get(root,"rhythmSeedFloat")) rhythmSeedFloat = (float)json_real_value(j);
        if (auto j = json_object_get(root,"melodySeedFloat")) melodySeedFloat = (float)json_real_value(j);

        if (auto j = json_object_get(root,"rhythmSeedPending")) rhythmSeedPending = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"rhythmSeedPendingFloat")) rhythmSeedPendingFloat = (float)json_real_value(j);
        if (auto j = json_object_get(root,"melodySeedPending")) melodySeedPending = (bool)json_boolean_value(j);
        if (auto j = json_object_get(root,"melodySeedPendingFloat")) melodySeedPendingFloat = (float)json_real_value(j);
        if (auto j = json_object_get(root,"numPolyVoices")) engine.numPolyVoices = pe_clamp((int)json_integer_value(j), 0, 7);

        if (auto j = json_object_get(root,"rhythmPattern")) {
            if (json_is_array(j)) {
                size_t n = json_array_size(j);
                for (size_t i = 0; i < 16 && i < n; ++i) {
                    json_t* it = json_array_get(j, i);
                    if (it) rhythmPattern[i] = (json_integer_value(it) != 0);
                }
            }
        }
        if (auto j = json_object_get(root,"rhythmRandom")) {
            if (json_is_array(j)) { for (size_t i=0; i<16 && i<json_array_size(j); ++i) engine.pe.rhythmRandom[i] = (float)json_real_value(json_array_get(j,i)); }
        }
        if (auto j = json_object_get(root,"variationRandom")) {
            if (json_is_array(j)) { for (size_t i=0; i<16 && i<json_array_size(j); ++i) engine.pe.variationRandom[i] = (float)json_real_value(json_array_get(j,i)); }
        }
        if (auto j = json_object_get(root,"legatoRandom")) {
            if (json_is_array(j)) { for (size_t i=0; i<16 && i<json_array_size(j); ++i) engine.pe.legatoRandom[i] = (float)json_real_value(json_array_get(j,i)); }
        }

        if (auto j = json_object_get(root, "polyRhythmRandom")) {
            if (json_is_array(j)) {
                for (int v = 0; v < 7; v++) {
                    for (int i = 0; i < 16; i++) {
                        engine.pe.polyRhythmRandom[v][i] = (float)json_real_value(json_array_get(j, v * 16 + i));
                        engine.pe.polyRhythmSource[v][i] = engine.pe.polyRhythmRandom[v][i];
                    }
                }
            }
        }
        if (auto j = json_object_get(root, "polyMelodyRandom")) {
            if (json_is_array(j)) {
                for (int v = 0; v < 7; v++) {
                    for (int i = 0; i < 16; i++) {
                        engine.pe.polyMelodyRandom[v][i] = (float)json_real_value(json_array_get(j, v * 16 + i));
                        engine.pe.polyMelodySource[v][i] = engine.pe.polyMelodyRandom[v][i];
                    }
                }
            }
        }
        if (auto j = json_object_get(root, "polyOctaveRandom")) {
            if (json_is_array(j)) {
                for (int v = 0; v < 7; v++) {
                    for (int i = 0; i < 16; i++) {
                        engine.pe.polyOctaveRandom[v][i] = (float)json_real_value(json_array_get(j, v * 16 + i));
                        engine.pe.polyOctaveSource[v][i] = engine.pe.polyOctaveRandom[v][i];
                    }
                }
            }
        }

        if (auto j = json_object_get(root, "polyRhythmRandom")) {
            if (json_is_array(j)) {
                for (int v = 0; v < 7; v++) {
                    for (int i = 0; i < 16; i++) {
                        engine.pe.polyRhythmRandom[v][i] = (float)json_real_value(json_array_get(j, v * 16 + i));
                        engine.pe.polyRhythmSource[v][i] = engine.pe.polyRhythmRandom[v][i]; // Copy to source
                    }
                }
            }
        }
        if (auto j = json_object_get(root, "polyMelodyRandom")) {
            if (json_is_array(j)) {
                for (int v = 0; v < 7; v++) {
                    for (int i = 0; i < 16; i++) {
                        engine.pe.polyMelodyRandom[v][i] = (float)json_real_value(json_array_get(j, v * 16 + i));
                        engine.pe.polyMelodySource[v][i] = engine.pe.polyMelodyRandom[v][i]; // Copy to source
                    }
                }
            }
        }
        if (auto j = json_object_get(root, "polyOctaveRandom")) {
            if (json_is_array(j)) {
                for (int v = 0; v < 7; v++) {
                    for (int i = 0; i < 16; i++) {
                        engine.pe.polyOctaveRandom[v][i] = (float)json_real_value(json_array_get(j, v * 16 + i));
                        engine.pe.polyOctaveSource[v][i] = engine.pe.polyOctaveRandom[v][i]; // Copy to source
                    }
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
        updateScaleMask();
    }

//return semitone parameter value with CV input added (if connected)
    float MeloDicer::getSemitoneParam(int sem)  {
        if (sem < 0 || sem > 11) return 0.f;
        // Enforce lock: if note is not in scale mask, probability is zero regardless of CV
        if (lockScaleNotes && !(activeScaleMask & (1 << sem))) return 0.f;

        float v =  params[SEMI0_PARAM + sem].getValue();
        // Add CV from cached expander if present
    if (cachedExpander && cachedExpander->inputs[MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + sem].isConnected()) {
        float att = cachedExpander->params[MeloDicerIds::EXPANDER_SEMI_ATTENUVERTER_0 + sem].getValue();
        v += (cachedExpander->inputs[MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + sem].getVoltage() * att) / 10.0f;
    }

        v = clampv(v, 0.f, 1.f);
        return v;
    }

    //return octave parameter value with CV input added (if connected)
    // Now delegated to ParameterManager - see wrapper functions below

// ── Parameter getters (delegated to ParameterManager) ──────────────────────
// These wrapper functions maintain backward compatibility while delegating
// to the centralized ParameterManager.

float MeloDicer::getNoteValueParam()  { return paramManager ? paramManager->getNoteValue() : params[NOTE_VALUE_PARAM].getValue(); }
float MeloDicer::getVariationParam()  { return paramManager ? paramManager->getVariation() : 0.5f; }
float MeloDicer::getLegatoParam()     { return paramManager ? paramManager->getLegato() : 0.1f; }
float MeloDicer::getRestParam()       { return paramManager ? paramManager->getRest() : 0.1f; }
float MeloDicer::getAccentParam()     { return paramManager ? paramManager->getAccent() : 0.25f; }

float MeloDicer::getOctaveLoParam()   { return paramManager ? paramManager->getOctaveLo() : 2.f; }
float MeloDicer::getOctaveHiParam()   { return paramManager ? paramManager->getOctaveHi() : 5.f; }

float MeloDicer::getPolyRestParam(int voiceIdx) {
    return paramManager ? paramManager->getPolyRest(voiceIdx) : 0.1f;
}

// --- switch melody/rhythm mode (dice/realtime), caching/restoring state as needed ---    
void MeloDicer::switchMelodyMode() { engine.pe.switchMelodyMode(stepIndex, lastStepIndex); }

// --- switch melody/rhythm mode (dice/realtime), caching/restoring state as needed ---
void MeloDicer::switchRhythmMode() { engine.pe.switchRhythmMode(stepIndex, lastStepIndex); }


// pick a semitone (0..11) using weighted random, or -1 if no semitone is selected
// uses melody RNG
// returns -1 if no semitone is selected (all weights zero)
int MeloDicer::pickSemitoneWeighted() {
    float w[12];
    for(int i=0; i<12; ++i) w[i] = getSemitoneParam(i);
    int idx = engine.getMelodyStep();
    return engine.pe.pickSemitone(w, melodyRandom[idx]);
}

// generate a pitch V in 1V/oct format, returnh semitone via out parameter
//applies octave range and transpose
// uses melody RNG
// returns 0V and -1 semitone if no semitone is selected
// (should be rare, only if all semitone weights are zero)
float MeloDicer::genPitchV(int& outSemitone) {
    PatternInput in = makePatternInput();
    return engine.pe.genPitchLive(outSemitone, in, melodyRandom[engine.getMelodyStep()], octaveRandom[engine.getOctaveStep()]);
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

    void MeloDicer::rotateRhythm(int steps) {
        engine.pe.rotateRhythm(steps);
        engine.pe.refreshVisualCache(makePatternInput());
    }

    void MeloDicer::rotateRhythmPattern(int steps) {
        engine.pe.rotateRhythm(steps);
        engine.pe.rotateVariation(steps);
        engine.pe.rotateLegato(steps);
        engine.pe.refreshVisualCache(makePatternInput());
    }
    void MeloDicer::rotateVariation(int steps) {
        engine.pe.rotateVariation(steps);
        engine.pe.refreshVisualCache(makePatternInput());
    }

    void MeloDicer::rotateLegato(int steps) {
        engine.pe.rotateLegato(steps);
        engine.pe.refreshVisualCache(makePatternInput());
    }

    void MeloDicer::rotateMelody(int steps) {
        engine.pe.rotateMelody(steps);
        engine.pe.refreshVisualCache(makePatternInput());
    }

    void MeloDicer::rotateMelodyPattern(int steps) {
        engine.pe.rotateMelody(steps);
        engine.pe.rotateOctave(steps);
        engine.pe.refreshVisualCache(makePatternInput());
    }
    void MeloDicer::rotateOctave(int steps) {
        engine.pe.rotateOctave(steps);
        engine.pe.refreshVisualCache(makePatternInput());
    }

    void MeloDicer::rebuildSemiCache_() {
        float weights[12];
        for (int i = 0; i < 12; ++i) weights[i] = getSemitoneParam(i);
        engine.rebuildScaleCache(weights);
    }

    float MeloDicer::quantizeToScale(float vIn) { return engine.quantize(vIn); }

    // handle manual restart: place index so next increment lands on startStep, optionally redraw realtime patterns
    // If there are pending seeds and resetImmediate==true, apply them immediately (for RESET triggered reseed)
    void MeloDicer::handleRestart(bool manual, bool resetImmediate) {
        stepIndex = (startStep - 1 + 16) % 16;
        engine.totalStepsElapsed = 0; // Sync polymeters to "Beat 1" on hard reset
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
    engine.pe.applyPendingSeedsAndRedraw(makePatternInput()); // This will also redraw poly patterns
    // No longer need to reseed stochasticRng as it's been replaced by DNA strands.
}

// ---------------- Helper: expander change hook -------------------------------
//
// Expander topology:
//   [ScaleExpander] — [MeloDicer] — [DNAExpander] — [PolyVoiceExpander]
//
// The left expander is always the scale/CV expander.
// The right side is a chain: MeloDicer checks its immediate right for DNA or
// PolyVoice.  If DNA is present, PolyVoice is expected to the right of DNA
// and is reached by walking one step further.  If no DNA is present,
// PolyVoice may attach directly to MeloDicer's right.
void MeloDicer::onExpanderChange(const ExpanderChangeEvent& e) {
    updateExpanderPointers();
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

    // Pre-process asynchronous triggers
    bool gate1Rise = g1Trig.process(inputs[GATE1_INPUT].getVoltage());
    bool gate2Rise = g2Trig.process(inputs[GATE2_INPUT].getVoltage());

    if (inputs[RUN_GATE_INPUT].isConnected()) 
    {
      bool runGateTrigHigh=runGateTrig.process(inputs[RUN_GATE_INPUT].getVoltage(), 0.1f, 2.f);
      bool runGateBtnHigh = runGateBtn.process(params[RUN_GATE_PARAM].getValue());
      if (runGateTrigHigh || runGateBtnHigh){
        runGateActive = !runGateActive;
        runPulse.trigger(1e-3f);
      }
    }
    else 
    {
      if(runGateBtn.process(params[RUN_GATE_PARAM].getValue()))
      {
        runGateActive = !runGateActive;
        runPulse.trigger(1e-3f);
      }
    };

    //if(schmittTrigger(resetTrigHigh,inputs[RESET_TRIGGER_INPUT].getVoltage()) && trigGenerator.remaining <= 0) resetArmed = true;
    //if(buttonTrigger(resetBtnHigh,params[RESET_BUTTON_PARAM].getValue()) && trigGenerator.remaining <= 0) resetArmed = true;
    if (resetTrig.process(inputs[RESET_TRIGGER_INPUT].getVoltage(),0.1f, 2.f))  resetArmed = true;
    if (resetBtn.process(params[RESET_BUTTON_PARAM].getValue()))  resetArmed = true;

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
                {
                    bool shouldMute = invertMuteLogic ? !g2High : g2High;
                    if (shouldMute != muted) {
                        muted = shouldMute;
                        if (!muted && restartOnUnmute)
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
            case 1: {                                                                   // Transpose quantized
                if (std::abs(v - lastCv1V) > 1e-5f) {
                    lastCv1V = v;
                    lastCv1Off = std::round(v * 12.f) * (1.f / 12.f);
                }
                outputs[CV_OUTPUT].setVoltage(currentPitchV + lastCv1Off);
            } break;
            case 2: {                                                                    // Mod LO (transient offset — never writes to param)
                cv1LoOffset = clampv<float>(v * 0.25f, -8.f, 8.f);
                outputs[CV_OUTPUT].setVoltage(currentPitchV);
            } break;
            case 3: {                                                                    // Mod HI (transient offset — never writes to param)
                cv1HiOffset = clampv<float>(v * 0.25f, -8.f, 8.f);
                outputs[CV_OUTPUT].setVoltage(currentPitchV);
            } break;
        }
    } else {
        cv1LoOffset = 0.f;
        cv1HiOffset = 0.f;
        outputs[CV_OUTPUT].setVoltage(currentPitchV);
    }

    
    
    // Gate output: engine.gs.process() ticks the pulse and returns raw gate voltage
    float gateV = engine.gs.process(args.sampleTime);
    if (muted)             gateV = 0.f;
    outputs[GATE_OUTPUT].setVoltage(gateV);
    
    // Derived gate outputs based on mono note decision
    float tieGateV = (engine.lastStepResult.decision == MonoDecision::Tie) ? 10.f : 0.f;
    float legatoGateV = (engine.lastStepResult.decision == MonoDecision::Legato || 
                         engine.lastStepResult.decision == MonoDecision::LegatoMax) ? 10.f : 0.f;
    float accentGateV = (engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f;
    
    if (muted) {
        tieGateV = 0.f;
        legatoGateV = 0.f;
        accentGateV = 0.f;
    }
    
    outputs[TIE_OUTPUT].setVoltage(tieGateV);
    outputs[LEGATO_OUTPUT].setVoltage(legatoGateV);
    outputs[ACCENT_OUTPUT].setVoltage(accentGateV);

    // Poly voice gate and CV outputs — written every sample so pulse timing is accurate.
    // MeloDicer writes directly into the expander's output ports.
    if (cachedPolyVoiceExpander && engine.numPolyVoices > 0) {
        using namespace PolyVoiceExpanderIds;
        for (int i = 0; i < engine.numPolyVoices; ++i) {
            float vg = engine.voices[i].gs.process(args.sampleTime);
            if (muted) vg = 0.f;
            cachedPolyVoiceExpander->outputs[POLY_GATE_OUT_1 + i].setVoltage(vg);
            cachedPolyVoiceExpander->outputs[POLY_CV_OUT_1 + i].setVoltage(engine.voices[i].gs.currentPitchV);
            
            // Poly accent: fires when mono is accented AND poly voice is sounding
            float polyAccent = (engine.lastStepResult.accented && vg > 5.f) ? 10.f : 0.f;
            if (muted) polyAccent = 0.f;
            cachedPolyVoiceExpander->outputs[POLY_ACCENT_OUT_1 + i].setVoltage(polyAccent);
        }
        // Zero unused voice outputs so they don't emit stale voltages.
        for (int i = engine.numPolyVoices; i < 7; ++i) {
            cachedPolyVoiceExpander->outputs[POLY_GATE_OUT_1 + i].setVoltage(0.f);
            cachedPolyVoiceExpander->outputs[POLY_CV_OUT_1 + i].setVoltage(0.f);
            cachedPolyVoiceExpander->outputs[POLY_ACCENT_OUT_1 + i].setVoltage(0.f);
        }
    }


    // DICE light policy: bright when enabled, also bright when a seed is armed
    // float rDiceLight = diceR ? 1.f : 0.1f;
    // float mDiceLight = diceM ? 1.f : 0.1f;
    // if (rhythmSeedPending) rDiceLight = 1.f;
    // if (melodySeedPending) mDiceLight = 1.f;
    lights[RHYTHM_DICE_LIGHT].setBrightness(rhythmSeedPending ? 1.f : 0.1f);
    lights[MELODY_DICE_LIGHT].setBrightness(melodySeedPending ? 1.f : 0.1f);
    lights[LOCK_LIGHT].setBrightness(locked ? 1.f : 0.f);

    // Expander indicators: Green for valid connection, Red for warning (multiple)
    auto setExpLight = [&](int lightId, int count) {
        lights[lightId + 0].setBrightness(count == 1 ? 1.f : 0.f); // Green channel
        lights[lightId + 1].setBrightness(count > 1 ? 1.f : 0.f);  // Red channel
    };
    setExpLight(SCALE_EXPANDER_LIGHT, scaleExpanderCount);
    setExpLight(DNA_EXPANDER_LIGHT, dnaExpanderCount);
    setExpLight(POLY_EXPANDER_LIGHT, polyExpanderCount);

    lights[MUTE_LIGHT].setBrightness(muted ? 1.f : 0.f);

    // ── Throttle UI and Light processing ──
    if (lightDivider.process()) {
        // Throttled Visuals/Outputs
        lights[RUN_GATE_LIGHT].setBrightness(runGateActive ? 1.f : 0.f);
        lights[RESET_LIGHT].setBrightnessSmooth(resetArmed ? 1.f : 0.f, args.sampleTime * 512.f);
        outputs[RUN_GATE_OUTPUT].setVoltage(runGateActive ? 10.f : 0.f);

        // Seed monitor out — outputs the most recently armed or committed seed.
        // Prefers a pending (armed) value if present; reflects both rhythm and
        // melody seeds so the output is useful regardless of which DICE is active.
        // Rhythm and melody seeds share the same output (last-armed wins).
        float outSeed = rhythmSeedFloat;
        if (rhythmSeedPending)  outSeed = rhythmSeedPendingFloat;
        if (melodySeedPending)  outSeed = melodySeedPendingFloat;   // melody armed overrides rhythm
        outputs[SEED_OUTPUT].setVoltage(clampv<float>(outSeed, 0.f, 10.f));

        // --- UI button toggles ---
        if (diceRTrig.process(params[DICE_R_PARAM].getValue())) {
            rhythmMode = 0;
            rhythmSeedPendingFloat = sampleSeedFromSource();
            rhythmSeedPending = true;
        }
        if (diceMTrig.process(params[DICE_M_PARAM].getValue())) {
            melodyMode = 0;
            melodySeedPendingFloat = sampleSeedFromSource();
            melodySeedPending = true;
        }
        if (lockTrig.process(params[LOCK_PARAM].getValue()))     locked = !locked;
        if (muteTrig.process(params[MUTE_PARAM].getValue())) {
            muted = !muted;
            if (!muted && restartOnUnmute) {
                handleRestart(/*manual=*/true, /*resetImmediate=*/true);
            }
        }

        if (modeTrig.process(params[MODE_PARAM].getValue())) {
            modeSelect = (modeSelect + 1) % 4;
        }

        // Mode lamps
        if (modeSelect != lastModeSelect) {
            lights[MODE_A_LIGHT].setBrightness(modeSelect == 0 ? 1.f : 0.f);
            lights[MODE_B_LIGHT].setBrightness(modeSelect == 1 ? 1.f : 0.f);
            lights[MODE_C_LIGHT].setBrightness(modeSelect == 2 ? 1.f : 0.f);
            lights[MODE_D_LIGHT].setBrightness(modeSelect == 3 ? 1.f : 0.f);
            lastModeSelect = modeSelect;
        }

        // --- Ring LEDs (Steps) ---
        for (int i = 0; i < 16; ++i) {
            lights[STEP_LIGHTS_START + i].setBrightness(engine.getStepLightBrightness(i));
        }

        // ── Semitone fader cache ──
        {
            bool faderDirty = false;
            for (int i = 0; i < 12; ++i) {
                if (lockScaleNotes && !(activeScaleMask & (1 << i))) {
                    if (params[SEMI0_PARAM + i].getValue() != 0.f)
                        params[SEMI0_PARAM + i].setValue(0.f);
                }
                float w = getSemitoneParam(i);
                if (!faderDirty && std::fabs(w - faderCache[i]) > 1e-5f) faderDirty = true;
            }
            if (faderDirty || activeSemiCount == 0) rebuildSemiCache_();
        }

        // Refresh visual cache so LEDs react to knob changes (at ~90Hz instead of 44kHz)
        engine.pe.refreshVisualCache(makePatternInput());
        // updateStepLEDs_ handles red flash channel
        updateStepLEDs_(args.sampleTime * 512.f);
    }

    // ── Control-Rate DNA and Window Updates (Optimized CPU) ──
    if (controlDivider.process()) {
        updateExpanderPointers();
        if (cachedDnaExpander) {
            auto processStrand = [&](int pLen, int iLen, int pOff, int iOff, int pRot, int& tLen, int& tOff, int& tRot) {
                float lCV = cachedDnaExpander->inputs[iLen].getNormalVoltage(0.f) * 1.6f;
                float oCV = cachedDnaExpander->inputs[iOff].getNormalVoltage(0.f) * 1.5f;
                tLen = clampv<int>((int)std::round(cachedDnaExpander->params[pLen].getValue() + lCV), 1, 16);
                // Use positive-safe modulo: C++ % can return negative for negative operands.
                int rawOff = (int)std::round(cachedDnaExpander->params[pOff].getValue() + oCV);
                tOff = ((rawOff % 16) + 16) % 16;
                int rawRot = (int)std::round(cachedDnaExpander->params[pRot].getValue());
                tRot = ((rawRot % 16) + 16) % 16;
            };

            using namespace MeloDicerIds;
            processStrand(DNA_R_LEN_PARAM, DNA_R_LEN_INPUT, DNA_R_OFF_PARAM, DNA_R_OFF_INPUT, DNA_R_ROT_PARAM, 
                            engine.rhythmLen, engine.rhythmOff, engine.rhythmRot);
            processStrand(DNA_V_LEN_PARAM, DNA_V_LEN_INPUT, DNA_V_OFF_PARAM, DNA_V_OFF_INPUT, DNA_V_ROT_PARAM, 
                            engine.variationLen, engine.variationOff, engine.variationRot);
            processStrand(DNA_L_LEN_PARAM, DNA_L_LEN_INPUT, DNA_L_OFF_PARAM, DNA_L_OFF_INPUT, DNA_L_ROT_PARAM, 
                            engine.legatoLen, engine.legatoOff, engine.legatoRot);
            processStrand(DNA_A_LEN_PARAM, DNA_A_LEN_INPUT, DNA_A_OFF_PARAM, DNA_A_OFF_INPUT, DNA_A_ROT_PARAM, 
                            engine.accentLen, engine.accentOff, engine.accentRot);
            processStrand(DNA_M_LEN_PARAM, DNA_M_LEN_INPUT, DNA_M_OFF_PARAM, DNA_M_OFF_INPUT, DNA_M_ROT_PARAM, 
                            engine.melodyLen, engine.melodyOff, engine.melodyRot);
            processStrand(DNA_O_LEN_PARAM, DNA_O_LEN_INPUT, DNA_O_OFF_PARAM, DNA_O_OFF_INPUT, DNA_O_ROT_PARAM, 
                            engine.octaveLen, engine.octaveOff, engine.octaveRot);

            #define DNA_ACT_PARAM(p, func) if (p##Trig.process(cachedDnaExpander->params[MeloDicerIds::p].getValue())) dnaManager.func()
            #define DNA_ACT_INPUT(i, func) if (i##Trig.process(cachedDnaExpander->inputs[MeloDicerIds::i].getVoltage())) dnaManager.func()
            
            DNA_ACT_PARAM(DNA_SCRAMBLE_ALL_PARAM, scrambleAll);
            DNA_ACT_INPUT(DNA_SCRAMBLE_ALL_INPUT, scrambleAll);
            DNA_ACT_PARAM(DNA_SCRAMBLE_R_PARAM, scrambleRhythmGroup);
            DNA_ACT_INPUT(DNA_SCRAMBLE_R_INPUT, scrambleRhythmGroup);
            DNA_ACT_PARAM(DNA_SCRAMBLE_V_PARAM, scrambleVariation);
            DNA_ACT_INPUT(DNA_SCRAMBLE_V_INPUT, scrambleVariation);
            DNA_ACT_PARAM(DNA_SCRAMBLE_L_PARAM, scrambleLegato);
            DNA_ACT_INPUT(DNA_SCRAMBLE_L_INPUT, scrambleLegato);
            DNA_ACT_PARAM(DNA_SCRAMBLE_A_PARAM, scrambleAccent);
            DNA_ACT_INPUT(DNA_SCRAMBLE_A_INPUT, scrambleAccent);
            DNA_ACT_PARAM(DNA_SCRAMBLE_M_PARAM, scrambleMelodyGroup);
            DNA_ACT_INPUT(DNA_SCRAMBLE_M_INPUT, scrambleMelodyGroup);
            DNA_ACT_PARAM(DNA_SCRAMBLE_O_PARAM, scrambleOctave);
            DNA_ACT_INPUT(DNA_SCRAMBLE_O_INPUT, scrambleOctave);

            DNA_ACT_PARAM(DNA_RESET_ALL_PARAM, resetAll);
            DNA_ACT_INPUT(DNA_RESET_ALL_INPUT, resetAll);
            DNA_ACT_PARAM(DNA_RESET_R_PARAM, resetRhythmGroup);
            DNA_ACT_INPUT(DNA_RESET_R_INPUT, resetRhythmGroup);
            DNA_ACT_PARAM(DNA_RESET_V_PARAM, resetVariation);
            DNA_ACT_INPUT(DNA_RESET_V_INPUT, resetVariation);
            DNA_ACT_PARAM(DNA_RESET_L_PARAM, resetLegato);
            DNA_ACT_INPUT(DNA_RESET_L_INPUT, resetLegato);
            DNA_ACT_PARAM(DNA_RESET_A_PARAM, resetAccent);
            DNA_ACT_INPUT(DNA_RESET_A_INPUT, resetAccent);
            DNA_ACT_PARAM(DNA_RESET_M_PARAM, resetMelodyGroup);
            DNA_ACT_INPUT(DNA_RESET_M_INPUT, resetMelodyGroup);
            DNA_ACT_PARAM(DNA_RESET_O_PARAM, resetOctave);
            DNA_ACT_INPUT(DNA_RESET_O_INPUT, resetOctave);
        } else {
            // Fallback defaults if expander is disconnected (only set once to save CPU)
            if (engine.rhythmLen != 16) {
                engine.rhythmLen = engine.variationLen = engine.legatoLen = engine.accentLen = engine.melodyLen = engine.octaveLen = 16;
                engine.rhythmOff = engine.variationOff = engine.legatoOff = engine.accentOff = engine.melodyOff = engine.octaveOff = 0;
                engine.rhythmRot = engine.variationRot = engine.legatoRot = engine.accentRot = engine.melodyRot = engine.octaveRot = 0;
            }
        }
        if (cachedPolyVoiceExpander) {
            for (int i = 0; i < 7; i++) {
                engine.polyLen[i] = clampv<int>((int)std::round(cachedPolyVoiceExpander->params[POLY_DNA_VOICE_1_LEN + i * 3].getValue()), 1, 16);
                int rawOff = (int)std::round(cachedPolyVoiceExpander->params[POLY_DNA_VOICE_1_OFF + i * 3].getValue());
                engine.polyOff[i] = ((rawOff % 16) + 16) % 16;
                int rawRot = (int)std::round(cachedPolyVoiceExpander->params[POLY_DNA_VOICE_1_ROT + i * 3].getValue());
                engine.polyRot[i] = ((rawRot % 16) + 16) % 16;
            }
        } else {
            for (int i = 0; i < 7; i++) {
                engine.polyLen[i] = 16;
                engine.polyOff[i] = engine.polyRot[i] = 0;
            }
        }

        engine.updateWindow(
            params[PATTERN_LENGTH_PARAM].getValue(), inputs[LENGTH_INPUT].getVoltage(), inputs[LENGTH_INPUT].isConnected(),
            params[PATTERN_OFFSET_PARAM].getValue(), inputs[OFFSET_INPUT].getVoltage(), inputs[OFFSET_INPUT].isConnected()
        );

        for (int i = 0; i < 4; ++i) cv2Offsets[i] = 0.f;
        if (inputs[CV2_INPUT].isConnected()) {
            float v    = clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f);
            float norm = v / 5.f; 
            if (cv2Mode == 0) cv2Offsets[0] = norm * 8.f;
            if (cv2Mode == 1) cv2Offsets[1] = norm;
            if (cv2Mode == 2) cv2Offsets[2] = norm;
            if (cv2Mode == 3) cv2Offsets[3] = norm;
        }
    }
}

// ---------------- Mode A: internal clock with offset -------------------
// stepEdge: true on 1/16 tick (internal or CLK input)
//  Also handles LED updates and semitone LEDs
// Uses shared triggerStepEvent_() helper.
//  Also handles phrase boundary reseeding and redraw.
//  Also handles first note logic when starting from reset.
//  Also handles mute logic (no output, but still advances and updates LEDs)
void MeloDicer::handleModeA_(const ProcessArgs& args) {
    if (clock.sixteenthEdge && !muted) {
        engine.accentProb = getAccentParam();  // Update accent probability from knob + CV
        StepResult result = engine.executeModeA(clock, getRestParam(), getLegatoParam(), getNoteValueParam(), makePatternInput());
        if (result.wrapped) onPhraseBoundary_();
        if (result.stepped && engine.numPolyVoices > 0) {
            for (int i = 0; i < engine.numPolyVoices; ++i)
                engine.voices[i].restProb = getPolyRestParam(i);
            engine.executePolyVoices(makePatternInput());
        }
        lastStepIndex = stepIndex;
    }
}

// ---------------- Mode B: external gate with offset -------------------
// gate1Edge: true on rising edge of GATE1 input
//  Also handles LED updates and semitone LEDs  
// Uses shared triggerStepEvent_() helper.
//  Also handles phrase boundary reseeding and redraw.
//  Also handles first note logic when gate held high continuously.
void MeloDicer::handleModeB_(const ProcessArgs& args, bool gate1Rise) {
    bool gate1High = inputs[GATE1_INPUT].getVoltage() >= 1.f;
    if (!muted && (gate1Rise || (gate1High && stepIndex == -1))) {
        engine.accentProb = getAccentParam();  // Update accent probability from knob + CV
        StepResult result = engine.executeModeB(gate1Rise, gate1High, getRestParam(), getLegatoParam(), getNoteValueParam(), makePatternInput());
        if (result.wrapped) onPhraseBoundary_();
        if (result.stepped && engine.numPolyVoices > 0) {
            for (int i = 0; i < engine.numPolyVoices; ++i)
                engine.voices[i].restProb = getPolyRestParam(i);
            engine.executePolyVoices(makePatternInput());
        }
        lastStepIndex = stepIndex;
    }
}


// ---------------- Mode C: Quantizer 1 ---------------------------------------
// quarterEdge: true on one-sample quarter-note pulse from ClockEngine
// Latches and quantizes CV2 on each quarter-note edge; steps through pattern window.
void MeloDicer::handleModeC_(const ProcessArgs& args) {
    if (clock.quarterEdge) {
        float inCV = inputs[CV2_INPUT].isConnected() ? clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f) : 0.f;
        engine.executeModeC(clock, inCV);
        lastStepIndex = stepIndex;
    }
}


// ---------------- Mode D: Quantizer 2 (second lane or alt flavor) -----------
//  
void MeloDicer::handleModeD_(const ProcessArgs& args) {
    bool gateHigh = inputs[GATE2_INPUT].isConnected() && inputs[GATE2_INPUT].getVoltage() > 1.f;
    float inCV = inputs[CV2_INPUT].isConnected() ? clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f) : 0.f;
    
    engine.executeModeD(gateHigh, inCV);
}


Model* modelMeloDicer = createModel<MeloDicer, MeloDicerWidget>("MeloDicer");

void init(rack::Plugin* p) {
	pluginInstance = p;
	p->addModel(modelMeloDicer);
	p->addModel(modelMeloDicerExpander);
	p->addModel(modelMeloDicerDNAExpander);
	p->addModel(modelMeloDicerPolyVoiceExpander);
}