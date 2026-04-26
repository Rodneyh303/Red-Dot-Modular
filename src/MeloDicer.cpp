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
    pe.seedRngFromFloat(rng, seedFloat);
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

            // initialize basic edit state
        cv1Mode = 0;
        cv2Mode = 0;
        gate1Assign = 0;
        gate2Assign = 1;
        muteBehavior = 0;
        // POWER MUTE (5-D): if muteBehavior==3, start muted after power-on/reset
        // Applied after the rest of initialize() completes (see end of constructor)
        noteVariationMask = 0b111;
        ppqnSetting = 4;
        modeSelect = 0;
        lastModeSelect = -1;
        cachedLength = 16;
        cachedOffset = 0;



        // PatternEngine owns all seed/RNG/mode state — reset it here
        pe.reset();

        // dice state
        locked = false;
        muted = false;

        startStep = 0; endStep = 15;
        stepIndex = -1;
        bpm = 120.f;

        // Reset centralised clock engine
        clock.reset();

        gs.reset();
        prevExtGate = false;

        currentPitchV = 0.f;

        lastSemitone  = -1;
        lastStepIndex = -1;

        melodySeedCached = false;
        rhythmSeedCached = false;

        // semiPlayRemain cleared by gs.reset() above

        //reset and run/gate state
        resetArmed = false;
        runGateActive = false;



        // resetTrigHigh = false;
        // resetBtnHigh = false;

        // runGateBtnHigh = false;
        // runGateTrigHigh = false;

    //reseedRng();
    //updateLights();
        rebuildSemiCache_();  // populate active semitone list on init
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
            MeloDicerLeftMessage *msg = (MeloDicerLeftMessage *)leftExpander.consumerMessage;
            v += msg->semiCv[sem] / 10.f;
        }
        v = clampv(v, 0.f, 1.f);
        return v;
    }

    //return octave parameter value with CV input added (if connected)
    float MeloDicer::getOctaveLoParam()  {
        float v = params[OCT_LO_PARAM].getValue();
        if (leftExpander.module && leftExpander.module->model == modelMeloDicerExpander) {
            MeloDicerLeftMessage *msg = (MeloDicerLeftMessage *)leftExpander.consumerMessage;
            v += msg->octLoCv / 10.f * 8.f;
        }
        v = clampv(v, 0.f, 8.f);
        return v;
    }

    //return octave parameter value with CV input added (if connected)
    float MeloDicer::getOctaveHiParam()  {
        float v = params[OCT_HI_PARAM].getValue();
        if (leftExpander.module && leftExpander.module->model == modelMeloDicerExpander) {
            MeloDicerLeftMessage *msg = (MeloDicerLeftMessage *)leftExpander.consumerMessage;
            v += msg->octHiCv / 10.f * 8.f;
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
void MeloDicer::switchMelodyMode() { pe.switchMelodyMode(stepIndex, lastStepIndex); }

// --- switch melody/rhythm mode (dice/realtime), caching/restoring state as needed ---
void MeloDicer::switchRhythmMode() { pe.switchRhythmMode(stepIndex, lastStepIndex); }


// pick a semitone (0..11) using weighted random, or -1 if no semitone is selected
// uses melody RNG
// returns -1 if no semitone is selected (all weights zero)
int MeloDicer::pickSemitoneWeighted() {
    float w[12]; for(int i=0;i<12;++i) w[i]=getSemitoneParam(i);
    return pe.pickSemitone(w);
}

// generate a pitch V in 1V/oct format, return semitone via out parameter
//applies octave range and transpose
// uses melody RNG
// returns 0V and -1 semitone if no semitone is selected
// (should be rare, only if all semitone weights are zero)
    float MeloDicer::genPitchV(int& outSemitone) {
        return pe.genPitch(outSemitone, makePatternInput());
    }

    // apply variation to a base note length index (0..8)
    // returns a possibly different index, respecting noteVariationMask
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

    int MeloDicer::varyNoteIndex(int baseIdx) { return pe.varyNoteIndex(baseIdx, makePatternInput()); }

// --- trigger a note of given note length index (0..8) ---
// - nvIdx: index into NOTEVALS[] for length
// - tie: whether 'tie' was drawn (extend if same pitch / tieAcrossSteps behavior)
// NOTE: Caller must set currentPitchV and lastSemitone BEFORE calling this.
//       This helper only manages gateHeld / holdRemain / gatePulse / semiPlayRemain.
void MeloDicer::triggerNoteLenIdx(int nvIdx, bool tie) {
    // compute length in seconds for the chosen note value
    float dur = noteLenSteps_(nvIdx);

    // --- LED duration for the currently active semitone ---
    int sem = lastSemitone; // caller is responsible for having set this
    if (sem >= 0 && sem < 12)
        semiPlayRemain[sem] = std::max(semiPlayRemain[sem], dur);

    if (tie) {
        // extend note, keep gate high (no retrigger)
        gateHeld = true;
        holdRemain += dur;
    } else {
        // retrigger: short pulse kicks envelopes, holdRemain controls gate duration
        gateHeld = false;
        holdRemain = dur;
        gatePulse.trigger(1e-3f);   // 1ms retrigger pulse — not dur
        gateHeld = true;
    }
}





// Shared helper: handle a note OR a rest for a step
// - pitchV: 1V/oct pitch value to use for CV when note is played
// - semitone: 0..11 semitone index (or -1 if none)
// - nvIdx: index into NOTEVALS[] for length
// - doRest: true => rest for this step
// - tie: whether 'tie' was drawn (extend if same pitch / tieAcrossSteps behavior)
// - legato: when true -> force gate held (LEGATO MAX behaviour)
// NOTE: This helper updates gateHeld / holdRemain / gatePulse / semiPlayRemain / currentPitchV.
void MeloDicer::triggerNoteOrRest_(float pitchV, int semitone, int nvIdx, bool doRest, bool tie, bool legato) {
    // Compute actual duration in seconds for this note length
    float dur = noteLenSteps_(nvIdx);

    // If legato() is "force gate always on" (e.g. knob max),
    // behave as sustained: set gateHeld and update pitch
    if (legato) {
        // Always hold gate and change pitch (slide) to new pitch
        currentPitchV = pitchV;
        gateHeld = true;
        // show LED duration too (use dur to visualize)
        if (semitone >= 0 && semitone < 12)
            semiPlayRemain[semitone] = std::max(semiPlayRemain[semitone], dur);
        // do not trigger gatePulse (no retrigger)
        return;
    }

    // Rest: let current note ring to natural end. Tie always extends (tieAcrossSteps removed).
    if (doRest) {
        if (tie && gateHeld)
            holdRemain += dur;
        return;
    }

    // Note case (not a rest)
    // Update pitch: if first note or not tying to previous pitch, update currentPitchV
    // We keep the behaviour: pitch changes even if gateHeld (legato/tie decide gate retrigger vs keep)
    bool sameAsLast = (semitone == lastSemitone);

    // If tie requested and gate is already held:
    // - If tieAcrossSteps == true: extend holdRemain by dur (no gate retrigger)
    // - If tieAcrossSteps == false: only tie if the note length aligns with step boundaries;
    //   that check is done by caller using nvIdx logic (we assume caller computed 'tie' correctly).
    if (tie && gateHeld) {
        // If tying to same pitch, don't retrigger; just extend
        if (sameAsLast) {
            // keep currentPitchV unchanged (tie between same pitch)
            holdRemain += dur;
            // light LED duration for visibility
            if (semitone >= 0 && semitone < 12)
                semiPlayRemain[semitone] = std::max(semiPlayRemain[semitone], dur);
            return;
        } else {
            // Different semitone: legato slide — pitch changes, no retrigger
            currentPitchV = pitchV;
            holdRemain += dur;
            if (semitone >= 0 && semitone < 12)
                semiPlayRemain[semitone] = std::max(semiPlayRemain[semitone], dur);
            return;
        }
    }

    // Normal retrigger (no tie/legato preventing retrigger)
    currentPitchV = pitchV;
    // Trigger gatePulse for a short envelope retrigger and set holdRemain to full dur
    // Use a very short gatePulse to ensure envelope re-triggers (enough for most VCV modules)
    const float trigPulse = 0.001f;
    gatePulse.trigger(trigPulse);
    gateHeld = true;
    holdRemain = dur;

    // Light semitone ring for duration
    if (semitone >= 0 && semitone < 12) {
        semiPlayRemain[semitone] = std::max(semiPlayRemain[semitone], dur);
    }
}

// Convenience wrapper to handle rest-only based on nvIdx and tie flag
// Updates gateHeld and holdRemain accordingly
// - nvIdx: index into NOTEVALS[] for length
// - tie: whether 'tie' was drawn (extend if tieAcrossSteps behavior)
// NOTE: This helper assumes the caller has already handled muting/locking logic
void MeloDicer::triggerRestLenIdx_(int nvIdx, bool tie) {
    float dur = noteLenSteps_(nvIdx);
    if (tie && gateHeld)
        holdRemain += dur;
    else { gateHeld = false; holdRemain = 0.f; }
}



//=====================================================
// Compute note length in seconds
// Duration in clock-steps for a note length index.
// One step = one 1/16 note = holdRemain unit.
// A whole note = 16 steps, quarter = 4, eighth = 2, thirty-second = 0.5.
// BPM-independent: no clock measurement needed.
float MeloDicer::noteLenSteps_(int nvIdx) {
    return std::max(0.5f, MeloDicer::NOTEVALS[nvIdx].fraction * 16.f);
}



// Convert semitone (0..11) to volts (1V/oct)
// 12 semitones per octave
float MeloDicer::semitoneToVolts(int semitone) {
    return semitone / 12.f;
}

//handle a step event: note/rest decision, legato, tie, etc
// updates gateHeld, holdRemain, gatePulse, currentPitchV
// - semitone: 0..11 pitch index for note to play
// - noteLenIdx: index into NOTEVALS[] for length
// - restProb: probability of rest for this step (0..1)
// - legatoProb: probability of legato for this step (0..1)
// - allowTieAcrossSteps: if true, allow tie to extend across steps (if tie drawn)
// - bpm: current BPM for note length calculation
// NOTE: This helper assumes the caller has already handled muting/locking logic
// and that semitone is valid (0..11)
// It also assumes the caller has handled the logic of whether a tie is requested
// and whether it is legal (same pitch or tieAcrossSteps).
// This helper only decides between note/rest/legato and updates the state accordingly.
// It does NOT handle the logic of whether a tie is legal or not; that is up to the caller.
// It does NOT handle the logic of whether a tie is requested or not; that is up to the caller.
// It does NOT handle the logic of whether the step is in the active window; that is up to the caller.
void MeloDicer::triggerStepEvent_(
    int semitone,
    int noteLenIdx,
    float restProb,
    float legatoProb,
    bool allowTieAcrossSteps,
    float bpm)
{
    // Draw rest first; only draw legato if the step is NOT a rest.
    // This prevents the legato RNG call from shifting when rest prob is high.
    bool doRest = unitRandomRhythm() < restProb;
    if (doRest) {
        // Let current note ring to natural end rather than abruptly cutting gate
        // (holdRemain counts down on its own — no need to zero it here)
        return;
    }

    bool doLegato = unitRandomRhythm() < legatoProb;

    if (doLegato && allowTieAcrossSteps) {
        // Legato: pitch changes, gate stays high (no retrigger), duration extends
        currentPitchV = semitoneToVolts(semitone);
        if (gateHeld) {
            holdRemain += noteLenSteps_(noteLenIdx);
        } else {
            // First note in a legato run — still need to open the gate
            gatePulse.trigger(1e-3f);
            holdRemain = noteLenSteps_(noteLenIdx);
            gateHeld = true;
        }
        return;
    }

    // if (doTie && allowTieAcrossSteps) {
    //     if (gateHeld) {
    //         holdRemain += noteLenSec_(noteLenIdx, bpm);
    //         return;
    //     }
    // }

    // Normal note
    // Retrigger gate
    gatePulse.trigger(1e-3f);
    holdRemain = noteLenSteps_(noteLenIdx);
    gateHeld = true;
    currentPitchV = semitoneToVolts(semitone);
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

    void MeloDicer::redrawRhythmPattern() { pe.redrawRhythm(makePatternInput()); }
    void MeloDicer::redrawMelodyPattern() { pe.redrawMelody(makePatternInput()); }

     // quantizer helper (modes C/D) – snap vIn to nearest active semitone, preserving octave
     //ignore CV/octave transposition, just work in 0..1 range
     //ignores semitone CV inputs
    // Quantize vIn to the nearest active semitone, weighted by fader height.
    // Manual: "The higher a fader is raised, the wider its quantization range."
    // Implementation: each active semitone has an attraction radius proportional to its fader value.
    // The input snaps to whichever active semitone has the highest score (value / distance).
    // Rebuild active semitone cache when faders change.
    // Called from process() only when a fader value differs from faderCache.
    void MeloDicer::rebuildSemiCache_() {
        activeSemiCount = 0;
        for (int s = 0; s < 12; ++s) {
            float w = clampv<float>(params[SEMI0_PARAM + s].getValue(), 0.f, 1.f);
            faderCache[s] = w;
            if (w > 0.f) {
                activeSemiList[activeSemiCount]   = s;
                activeSemiWeight[activeSemiCount] = w;
                ++activeSemiCount;
            }
        }
    }

    float MeloDicer::quantizeToScale(float vIn) {
        // Fast path: search only active semitones (cached list, typically 5-7).
        // Searches ±1 octave around the input octave — same logic as before,
        // just iterating activeSemiCount candidates instead of always 36.
        int   octave    = (int)std::floor(vIn);
        float bestScore = -1.f;
        float bestV     = vIn;

        for (int ai = 0; ai < activeSemiCount; ++ai) {
            int   s       = activeSemiList[ai];
            float w       = activeSemiWeight[ai];
            float radius  = w * (1.f / 12.f);
            for (int o = octave - 1; o <= octave + 1; ++o) {
                float cand = o + s / 12.f;
                float dist = std::fabs(vIn - cand);
                if (dist <= radius + 1e-4f) {
                    float score = w / (dist + 1e-4f);
                    if (score > bestScore) { bestScore = score; bestV = cand; }
                }
            }
        }
        // Fallback: nearest active semitone in ±1 octave
        if (bestScore < 0.f) {
            float minDist = 1e9f;
            for (int ai = 0; ai < activeSemiCount; ++ai) {
                int s = activeSemiList[ai];
                for (int o = octave - 1; o <= octave + 1; ++o) {
                    float cand = o + s / 12.f;
                    float dist = std::fabs(vIn - cand);
                    if (dist < minDist) { minDist = dist; bestV = cand; }
                }
            }
        }
        return clampv<float>(bestV, 0.f, 5.f);
    }

    // helper: return true if step idx is in active window (handles wrap if start > end)
    bool MeloDicer::stepInWindow(int idx) {
        if (startStep <= endStep) return (idx >= startStep && idx <= endStep);
        // wrapped window: e.g., start=12, end=3
        return (idx >= startStep || idx <= endStep);
    }

    // handle manual restart: place index so next increment lands on startStep, optionally redraw realtime patterns
    // If there are pending seeds and resetImmediate==true, apply them immediately (for RESET triggered reseed)
    void MeloDicer::handleRestart(bool manual, bool resetImmediate) {
        // Step position and gate always reset — RESET is a transport control,
        // not a pattern control, so it fires regardless of lock state.
        stepIndex = (startStep - 1 + 16) % 16;
        gs.reset();          // clears gate, hold, pitch, pulse, semiPlayRemain
        prevExtGate = false;

        // Pattern redraw and seed application only happen when unlocked.
        // While locked: patterns stay frozen, pending seeds stay queued.
        if (!locked) {
            redrawRhythmPattern();
            redrawMelodyPattern();

            if (resetImmediate) {
                if (rhythmSeedPending) {
                    rhythmSeedFloat = rhythmSeedPendingFloat;
                    reseedXoroshiroFromFloat(rhythmRng, rhythmSeedFloat);
                    rhythmSeedPending = false;
                }
                if (melodySeedPending) {
                    melodySeedFloat = melodySeedPendingFloat;
                    reseedXoroshiroFromFloat(melodyRng, melodySeedFloat);
                    melodySeedPending = false;
                }
            }
        }
        resetArmed = false;
    }

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

// MeloDicer.cpp — refactor: split process() into handleModeA/B/C/D() with full logic inline


// ---------------- Helper: phrase boundary hook -------------------------------
// Called at phrase boundary (stepIndex wraps from endStep back to startStep).
// Seeds are applied FIRST so the subsequent redraw uses the new RNG state.
void MeloDicer::onPhraseBoundary_() {
    pe.applyPendingSeedsAndRedraw(makePatternInput());
}

// ---------------- Helper: reset hook -----------------------------------------
void MeloDicer::onReset() {
    Module::onReset();
    initialize();   // resets all module state to defaults
    clock.reset();  // resets ClockEngine timing
}


// ---------------- Helper: compute note length idx with variation -------------
// int getNoteLenIdx_() {
//     int baseIdx = (int) std::round(params[NOTE_VALUE_PARAM].getValue());
//     baseIdx = clampv<int>(baseIdx, 0, 8);
//     //if (!diceR) return baseIdx;
//     return varyNoteIndex(baseIdx);
// }

// int getNoteLenIdx_() {
//     int maxIdx = sizeof(NOTEVALS) / sizeof(NOTEVALS[0]);

//     while (true) {
//         int idx = random::u32() % maxIdx;
//         const NoteVal& nv = NOTEVALS[idx];

//         int mask = (ppqnSetting == 1 ? 1 : ppqnSetting == 4 ? 2 : 4);
//         if (nv.allowedPPQN & mask) {
//             return idx;
//         }
//     }
// }


// Compute note length index (0..8) based on VARIATION_PARAM biasing
// 0.0 = bias to longer notes, 1.0 = bias to shorter notes, 0.5 = no bias
// without ties, forbid notes longer than 1/4 note  (=index 3)
// returns index into NOTEVALS[]
// uses rhythm RNG
// (this is a replacement for getNoteLenIdx_0 and varyNoteIndex combined)
// Note: this function does not consider PPQN legality; that is handled elsewhere
// (e.g. computeNoteLengthIdx)
// Note: this function does not consider the NOTE_VALUE_PARAM; it only uses VARIATION_PARAM
// to bias the choice of note length.   This is to match the original MeloDicer behavior    
// where the NOTE_VALUE_PARAM was not used in Mode A/B.
// If you want to use NOTE_VALUE_PARAM as a base, you can modify this function accordingly
// (e.g. call varyNoteIndex with baseIdx from NOTE_VALUE_PARAM).

// Returns note length index: NOTE VALUE sets the base, VARIATION randomly biases it.
// NOTEVALS.allowedPPQN bitmask: 1=PPQN1, 2=PPQN4, 4=PPQN24
// ppqnSetting raw values: 1, 4, 24 — must be converted to bitmask before use.
int MeloDicer::getNoteLenIdx_() {
    int baseIdx = clampv<int>((int)std::round(getNoteValueParam()), 0, 7);
    // Convert ppqnSetting (1/4/24) to allowedPPQN bitmask (1/2/4)
    int ppqnMask = (ppqnSetting == 1) ? 1 : (ppqnSetting == 4) ? 2 : 4;
    baseIdx = computeNoteLengthIdx(baseIdx, ppqnMask);
    return varyNoteIndex(baseIdx);
}

// Clamp requestedIdx to the highest NOTEVALS index valid for ppqnMask.
// ppqnMask: bitmask — 1=PPQN1, 2=PPQN4, 4=PPQN24 (NOT the raw ppqnSetting value).
int MeloDicer::computeNoteLengthIdx(int requestedIdx, int ppqnMask) {
    int idx = requestedIdx;
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;

    // mask check: if note not valid for this ppqn, walk downward
    while (idx > 0 && !(NOTEVALS[idx].allowedPPQN & ppqnMask)) {
        idx--;
    }
    return idx;
}



// quantize pitch to semitone index (0..11) and octave offset (integer)
float MeloDicer::quantizePitch(int semitoneIndex, int octaveOffset) {
    return octaveOffset + semitoneIndex / 12.f;
}

// ---------------- Helper: update step LEDs -------------------------------
// activeSemitone: 0..11 for currently playing semitone, or -1 if
void MeloDicer::updateStepLEDs_(int activeSemitone, float sampleTime)
{
    // Green channel (ch0) is managed by VCVLightSlider widget automatically.
    // We only drive the red channel (ch1) = "note playing" flash.
    // Always update red — semiPlayRemain decays each step so values always change.
    for (int i = 0; i < 12; i++) {
        float redLevel = gs.semiLedBrightness(i);
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

    // --- Edge detection / timing ---
    bool stepEdge    = false;  // 1/16 edge consumed by Mode A
    bool gate1Edge   = false;  // rising edge at GATE1 (Mode B + gate assignments)
    bool quarterEdge = false;  // quarter-note edge consumed by Mode C

    // --- Pattern length & offset knobs: compute active window (1..16 steps, offset 0..15)
    {
        // Read knobs (round to integer). Length must be 1..16
        int length = clampv<int>((int)std::round(params[PATTERN_LENGTH_PARAM].getValue()), 1, 16);
        int offset = (int)std::round(params[PATTERN_OFFSET_PARAM].getValue()) & 0x0F;

        // CV inputs modulate length and offset (0..10V → full range)
        if (inputs[LENGTH_INPUT].isConnected()) {
            float cv = clampv<float>(inputs[LENGTH_INPUT].getVoltage(), 0.f, 10.f);
            length = clampv<int>((int)std::round(cv / 10.f * 15.f) + 1, 1, 16);
        }
        if (inputs[OFFFSET_INPUT].isConnected()) {
            float cv = clampv<float>(inputs[OFFFSET_INPUT].getVoltage(), 0.f, 10.f);
            offset = ((int)std::round(cv / 10.f * 15.f)) & 0x0F;
        }

        // Map knobs to start/end step indices (0..15). End wraps correctly.
        startStep = offset;
        endStep = (offset + length - 1) & 0x0F;


        // If the current playhead is outside the new window, move it so that the next
        // increment will land on the window start. This matches handleRestart() behavior.
        // (If you want immediate jump to the first step, set stepIndex = startStep instead.)
        if (!stepInWindow(stepIndex)) {
            stepIndex = (startStep - 1 + 16) % 16;
        }
    }


    // ── Distribute clock edges to each mode ──────────────────────────────────
    // ClockEngine has already processed CLK IN — just read its outputs.
    if (modeSelect == 0) {
        stepEdge = clock.sixteenthEdge;   // Mode A: steps on every 1/16
    }
    if (modeSelect == 1) {
        gate1Edge = g1Trig.process(inputs[GATE1_INPUT].getVoltage()); // Mode B: driven by Gate1
    }
    if (modeSelect == 2) {
        quarterEdge = clock.quarterEdge;  // Mode C: latches on quarter note
    }
    // Mode D: no clock edge needed — driven entirely by GATE2_INPUT level

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

    if (!runGateActive) {
        // If not running, discard any reset request and hold outputs quiet
        resetArmed = false;
        return;
    }

    // --- RESET input: immediate phrase restart and apply pending seeds ---
    if (resetArmed)
    {
        handleRestart(/*manual=*/true, /*resetImmediate=*/true);
        resetPulse.trigger(1e-3f);  // fire reset output trigger
    }

    // Drive RESET_TRIGGER_OUTPUT as a 1ms pulse on reset
    outputs[RESET_TRIGGER_OUTPUT].setVoltage(resetPulse.process(args.sampleTime) ? 10.f : 0.f);

    // --- Gate input assignments ---
    if (g1Trig.process(inputs[GATE1_INPUT].getVoltage())) {
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
        bool g2Rise = g2Trig.process(inputs[GATE2_INPUT].getVoltage());
        switch (gate2Assign) {
            case 0: // TGL DICE M — toggle on rising edge
                if (g2Rise) melodyMode = (1 - melodyMode);
                break;
            case 1: // Re-dice M — rising edge, only in dice-mode
                if (g2Rise && melodyMode == 0) {
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
                if (g2Rise) handleRestart(/*manual=*/true, /*resetImmediate=*/true);
                break;
        }
    }

    // --- Mode dispatch ---
    switch (modeSelect) {
        case 0: handleModeA_(args, stepEdge);       break;
        case 1: handleModeB_(args, gate1Edge);      break;
        case 2: handleModeC_(args, quarterEdge);    break;
        case 3: handleModeD_(args);                 break;
        default: break;
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
    } else if (modeSelect <= 1) {
        // In A/B we continuously drive CV out with currentPitchV when no CV1 override
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
    
    
    // Decay semitone LED timers every sample so LEDs extinguish after the note ends
    // semiPlayRemain decay now handled by gs.tick() on each step edge

    // --- Gate output shaping (Modes A and B only) ---
    // Modes C and D write GATE_OUTPUT directly inside their own handlers.
    if (modeSelect <= 1) {
        // GateState handles hold countdown + semiPlayRemain decay
        if (stepEdge || gate1Edge) gs.tick();

        // Gate output: gs.process() ticks the pulse and returns raw gate voltage
        float gateV = gs.process(args.sampleTime);
        if (muteBehavior == 2) gateV = (gateV > 1.f) ? 0.f : 10.f;
        if (muted)             gateV = (muteBehavior == 2) ? 10.f : 0.f;
        outputs[GATE_OUTPUT].setVoltage(gateV);
    } else {
        gs.tickPulseOnly(args.sampleTime);  // keep pulse timer consistent in C/D
    }


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

// ---------------- Helper: PPQN-aware step trigger ----------------
// Returns true if the current stepIndex should trigger a note event,
// considering the PPQN setting and the pattern offset.
// stepIndex: current step index (0..15)
// startStep: start of active window (0..15)
// offset: PATTERN_OFFSET_PARAM value (0..15)
// ppqn: current PPQN setting (1, 4, or 24)
// For PPQN=1, only trigger on main 4 beats relative to startStep + offset
// For PPQN>1, trigger every tick (caller can use ppqnCounter if needed
//  to further filter)
// This matches the original MeloDicer behavior.
inline bool shouldTriggerStep(int stepIndex, int startStep, int offset, int ppqn) {
    if (ppqn <= 1) {
        // Only trigger on main 4 beats relative to startStep + offset
        int relative = (stepIndex - startStep + offset + 16) % 16;
        return (relative & 0x03) == 0;
    }
    return true; // Higher PPQN triggers every tick (can use ppqnCounter if needed)
}

// --- Helper: advance the playhead (increment-first, wrap to window) ----
// If playhead hasn't started (stepIndex == -1), initialize to startStep-1 so that
// the first increment lands on startStep.
// After incrementing, if the new stepIndex is outside the active window, wrap to startStep.
// If the window is very small (e.g. length=1), this ensures we stay within the window.
// This matches the behavior of handleRestart() and ensures consistent stepping.
void MeloDicer::advanceStepIndexOnEdge_() {
    if (stepIndex == -1) stepIndex = (startStep - 1 + 16) % 16;
    stepIndex = (stepIndex + 1) & 0x0F;
    if (!stepInWindow(stepIndex)) stepIndex = startStep;
    for (int s = 0; s < 16 && !stepInWindow(stepIndex); ++s)
        stepIndex = (stepIndex + 1) & 0x0F;
}

// --- Helper: compute offsetStep from stepIndex + cachedOffset ---------------
int MeloDicer::computeOffsetStep_() {
    if (stepIndex == -1) return 0;
    // cachedOffset is kept current each sample — avoids redundant getValue()+round()
    return (stepIndex - startStep + cachedOffset + 16) % 16;
}

// Thin translator: offsetStep -> semitone + nvIdx + probabilites + tieAcrossSteps + bpm
// Uses melodySemitone[] array for semitone lookup
// Uses getNoteLenIdx_() for note length index lookup   
// Uses params[REST_PARAM] and params[LEGATO_PARAM] for probabilities
// Uses tieAcrossSteps and bpm from global state
// Calls shared triggerStepEvent_() helper
// Assumes caller has handled mute logic (no output, but still advances and updates LEDs)
// Assumes caller has handled first note logic when starting from reset.
// Assumes caller has handled step windowing (i.e. does not call if step not in window)
// Assumes caller has handled mode logic (i.e. only called in Mode A/B)
// Assumes caller has handled any other global logic (e.g. reset, run gate, etc.)
// Assumes caller has handled any other global state (e.g. locked, firstNoteB, etc.)
// Assumes caller has handled any other global parameters (e.g. ppqnSetting, pattern length/offset, etc.)
// Assumes caller has handled any other global state (e.g. rhythmMode, melodyMode, etc.)
// Assumes caller has handled any other global parameters (e.g. rhythmMode, melodyMode, etc.)
// Assumes caller has handled any other global state (e.g. rhythmSeedPending, melodySeedPending, etc.)
// Assumes caller has handled any other global parameters (e.g. rhythmSeedPending, melodySeedPending, etc.)

void MeloDicer::triggerStepEventForOffsetStep_(int offsetStep) {
    // ── The secret sauce ──────────────────────────────────────────────────────
    // MeloDicer works in a fixed 16-step phrase but note durations create
    // sub-phrases. A quarter note (4 steps) should HOLD across steps 2,3,4 —
    // not fire a new note decision on each step. This is what makes it feel
    // like a human playing phrases rather than a step sequencer toggling gates.
    //
    // State at entry:
    //   holdRemain > 1.0  →  we are INSIDE a multi-step note or rest.
    //                        Only probabilistic extension is allowed — no new
    //                        attacks, no new rests. This preserves the note body.
    //   holdRemain ≤ 1.0  →  note/rest has expired (or was a 1/16).
    //                        Fire the full new note/rest/legato decision.
    // ─────────────────────────────────────────────────────────────────────────

    int nvIdx        = getNoteLenIdx_();
    int sem          = melodySemitone[offsetStep];
    float pitchV     = melodyPitchV[offsetStep];
    float restProb   = getRestParam();
    float legatoProb = getLegatoParam();
    float dur        = noteLenSteps_(nvIdx);

    // ── Max-legato shortcut: gate always on ──────────────────────────────────
    if (legatoProb >= 0.999f) {
        currentPitchV = pitchV;
        lastSemitone  = sem;
        gateHeld      = true;
        holdRemain    = dur;
        if (sem >= 0 && sem < 12)
            semiPlayRemain[sem] = std::max(semiPlayRemain[sem], dur);
        return;
    }

    // ── Mid-note: inside a multi-step note ───────────────────────────────────
    // holdRemain > 1 means this step is the 2nd, 3rd, 4th... step of a note.
    // Probabilistically extend via legato; otherwise let it ring naturally.
    // Rests are also multi-step: holdRemain > 1 with gateHeld=false = held rest.
    if (holdRemain >= 1.f) {
        if (gateHeld) {
            // Mid-note: maybe extend the tie (legato probability reused)
            if (unitRandomRhythm() < legatoProb) {
                holdRemain += dur;
                if (sem >= 0 && sem < 12)
                    semiPlayRemain[sem] = std::max(semiPlayRemain[sem], holdRemain);
            }
            // else: let holdRemain count down — note ends at its nominal boundary
        } else {
            // Mid-rest: rest spans multiple steps — just let holdRemain count down.
            // No extension: rest duration is fixed at the boundary decision.
            // (Extension via restProb caused infinite lock-in at restProb=1.0)
        }
        return;  // never fire new attack or retrigger while mid-note/mid-rest
    }

    // ── Note boundary: holdRemain ≤ 1 — full new decision ───────────────────

    // Draw rest first (preserves RNG independence between rest and legato draws)
    bool doRest = unitRandomRhythm() < restProb;
    if (doRest) {
        // Begin a new rest spanning `dur` steps.
        // Gate closes naturally — holdRemain will count down through the rest.
        // We set holdRemain to dur here so mid-rest logic above fires correctly.
        gateHeld   = false;
        holdRemain = dur;
        return;
    }

    // Legato: pitch slides, gate extends, no retrigger
    bool doLegato = unitRandomRhythm() < legatoProb;

    // Tie: same semitone only, gate extends, no pitch change
    bool doTie = (!doLegato) && (sem == lastSemitone) && gateHeld
                 && (unitRandomRhythm() < legatoProb);

    if (doLegato) {
        currentPitchV = pitchV;
        lastSemitone  = sem;
        if (gateHeld) {
            holdRemain += dur;
        } else {
            gatePulse.trigger(1e-3f);
            holdRemain = dur;
            gateHeld   = true;
        }
        if (sem >= 0 && sem < 12)
            semiPlayRemain[sem] = std::max(semiPlayRemain[sem], dur);
        return;
    }

    if (doTie) {
        holdRemain += dur;
        if (sem >= 0 && sem < 12)
            semiPlayRemain[sem] = std::max(semiPlayRemain[sem], dur);
        return;
    }

    // Normal note: retrigger gate, set new pitch and duration
    currentPitchV = pitchV;
    lastSemitone  = sem;
    gatePulse.trigger(1e-3f);
    gateHeld      = true;
    holdRemain    = dur;
    if (sem >= 0 && sem < 12)
        semiPlayRemain[sem] = std::max(semiPlayRemain[sem], dur);
}

// ---------------- Mode A: internal clock with offset -------------------
// stepEdge: true on 1/16 tick (internal or CLK input)
//  Also handles LED updates and semitone LEDs
// Uses shared triggerStepEvent_() helper.
//  Also handles phrase boundary reseeding and redraw.
//  Also handles first note logic when starting from reset.
//  Also handles mute logic (no output, but still advances and updates LEDs)
void MeloDicer::handleModeA_(const ProcessArgs& args, bool stepEdge) {
    if (!stepEdge || muted)
        return;

    int prevStep = stepIndex;

    // advance & windowing
    advanceStepIndexOnEdge_();

    // Detect wrap to startStep (phrase boundary)
    bool wrappedRestart = (prevStep != -1 && stepIndex == startStep);
    if (wrappedRestart) {
        // Sample new seeds for realtime modes, then let onPhraseBoundary_()
        // apply any pending seeds (Dice-armed or realtime) before redrawing.
        if (melodyMode == 1) { melodySeedPendingFloat = sampleSeedFromSource(); melodySeedPending = true; }
        if (rhythmMode == 1) { rhythmSeedPendingFloat = sampleSeedFromSource(); rhythmSeedPending = true; }
        onPhraseBoundary_();
    }

    // compute offset and fire the shared step event
    int offsetStep = computeOffsetStep_();
    triggerStepEventForOffsetStep_(offsetStep);

    // update LEDs
    //updateStepLEDs_(offsetStep);
        // ---------------- Step LEDs ----------------
    if (stepIndex != lastStepIndex) {
        for (int i = 0; i < 16; ++i) {
            float baseActive = stepInWindow(i) ? 0.25f : 0.0f;
            float current = (i == offsetStep) ? 1.f : 0.f;
            lights[STEP_LIGHTS_START + i].setBrightness(std::max(baseActive, current));
        }
        lastStepIndex = stepIndex;
    }

    // updateStepLEDs_ handles red flash channel; slider widget handles green.
    updateStepLEDs_(offsetStep, args.sampleTime);
    lastSemitone = melodySemitone[offsetStep];
}

// ---------------- Mode B: external gate with offset -------------------
// gate1Edge: true on rising edge of GATE1 input
//  Also handles LED updates and semitone LEDs  
// Uses shared triggerStepEvent_() helper.
//  Also handles phrase boundary reseeding and redraw.
//  Also handles first note logic when gate held high continuously.
void MeloDicer::handleModeB_(const ProcessArgs& args, bool gate1Edge//, bool diceTrig
) {
    if (muted)
        return;

    bool extGateHigh = inputs[GATE1_INPUT].getVoltage() >= 1.f;
    int prevStep = stepIndex;

   // if (gate1Edge || diceTrig)
        if (gate1Edge)
     {
        // Advance step like Mode A
        advanceStepIndexOnEdge_();

        bool wrappedRestart = (prevStep != -1 && stepIndex == startStep);
        if (wrappedRestart) {
            // Sample new seeds for realtime modes, then let onPhraseBoundary_()
            // apply any pending seeds (Dice-armed or realtime) before redrawing.
            if (melodyMode == 1) { melodySeedPendingFloat = sampleSeedFromSource(); melodySeedPending = true; }
            if (rhythmMode == 1) { rhythmSeedPendingFloat = sampleSeedFromSource(); rhythmSeedPending = true; }
            onPhraseBoundary_();
        }

        int offsetStep = computeOffsetStep_();
        triggerStepEventForOffsetStep_(offsetStep);
        updateStepLEDs_(offsetStep, args.sampleTime);
    }
    else if (extGateHigh && !prevExtGate) {
        // If no step has been selected yet, advance to startStep
        if (stepIndex == -1) {
            stepIndex = (startStep - 1 + 16) % 16;
            advanceStepIndexOnEdge_();
        }
        int offsetStep = computeOffsetStep_();
        triggerStepEventForOffsetStep_(offsetStep);
        updateStepLEDs_(offsetStep, args.sampleTime);
    }

    // Visual semitone LEDs
    int offsetStep = (stepIndex == -1) ? 0 : computeOffsetStep_();
    int sem = melodySemitone[offsetStep];
    // for (int i = 0; i < 12; ++i) {
    //     float prob = clampv<float>(params[SEMI0_PARAM + i].getValue(), 0.f, 1.f);
    //     bool isLast = (i == sem) && gateHeld;
    //     lights[SEMI_LED_START + i * 2 + 0].setBrightness(isLast ? 0.f : prob);
    //     lights[SEMI_LED_START + i * 2 + 1].setBrightness(semiPlayRemain[i]);
    // }

    lastSemitone = sem;
    prevExtGate = extGateHigh;
}


// ---------------- Mode A: internal sequencer with offset -------------------





// ---------------- Mode C: Quantizer 1 ---------------------------------------
// quarterEdge: true on one-sample quarter-note pulse from ClockEngine
// Latches and quantizes CV2 on each quarter-note edge; steps through pattern window.
void MeloDicer::handleModeC_(const ProcessArgs& args, bool quarterEdge) {

    // Step index advances on each quarter-note edge
    if (quarterEdge) {
        stepIndex = (stepIndex + 1) % 16;
        if (!stepInWindow(stepIndex)) {
            stepIndex = startStep;
        }
        for (int s = 0; s < 16 && !stepInWindow(stepIndex); ++s) {
            stepIndex = (stepIndex + 1) % 16;
        }
    }

    // Latch quantized CV from CV2 input on each quarter-note edge
    float inCV = inputs[CV2_INPUT].isConnected() ? clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f) : 0.f;
    if (quarterEdge) {
        currentPitchV = quantizeToScale(inCV);
    }

    // Output voltage (holds last quantized value between edges)
    outputs[CV_OUTPUT].setVoltage(currentPitchV);

    // Gate output: brief pulse on each quarter-note edge
    outputs[GATE_OUTPUT].setVoltage(quarterEdge ? 10.f : 0.f);

    // 6. LEDs
    for (int i = 0; i < 16; ++i) {
        float baseActive = stepInWindow(i) ? 0.25f : 0.0f;
        float current = (i == ((stepIndex + 16) % 16)) ? 1.f : 0.f;
        lights[STEP_LIGHTS_START + i].setBrightness(std::max(baseActive, current));
    }

    // Semitone LEDs (show quantized note)
    for (int i = 0; i < 12; ++i) {
        int sem = int(std::round((currentPitchV - std::floor(currentPitchV)) * 12.f)) % 12;
        float levelB = 0.08f + clampv<float>(params[SEMI0_PARAM + i].getValue(), 0.f, 1.f) * 0.92f;
        float playB  = (i == sem) ? 1.f : 0.f;
        int base = SEMI_LED_START + i * 2;
        lights[base + 0].setBrightnessSmooth(levelB, args.sampleTime);
        lights[base + 1].setBrightnessSmooth(playB,  args.sampleTime * 4.f);
    }
}


// ---------------- Mode D: Quantizer 2 (second lane or alt flavor) -----------
//  
void MeloDicer::handleModeD_(const ProcessArgs& args) {
    // Check gate high on GATE2_INPUT
    bool gateHigh = inputs[GATE2_INPUT].isConnected() && inputs[GATE2_INPUT].getVoltage() > 1.f;

    if (gateHigh) {
        // 1. Read CV2 input
        float inCV = inputs[CV2_INPUT].isConnected() ? clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f) : 0.f;
        currentPitchV = quantizeToScale(inCV); // quantize according to faders

        // 2. Output voltage
        outputs[CV_OUTPUT].setVoltage(currentPitchV);

        // 3. Gate output – high as long as gate is held
        outputs[GATE_OUTPUT].setVoltage(10.f);

        // 4. Step LEDs
        for (int i = 0; i < 16; ++i) {
            float baseActive = stepInWindow(i) ? 0.25f : 0.0f;
            lights[STEP_LIGHTS_START + i].setBrightness(baseActive);
        }

        // Semitone LEDs (show quantized note)
        for (int i = 0; i < 12; ++i) {
            int sem = int(std::round((currentPitchV - std::floor(currentPitchV)) * 12.f)) % 12;
            float levelB = 0.08f + clampv<float>(params[SEMI0_PARAM + i].getValue(), 0.f, 1.f) * 0.92f;
            float playB  = (i == sem) ? 1.f : 0.f;
            int base = SEMI_LED_START + i * 2;
            lights[base + 0].setBrightnessSmooth(levelB, args.sampleTime);
            lights[base + 1].setBrightnessSmooth(playB,  args.sampleTime * 4.f);
        }
    } else {
        // Gate low – output zero
        outputs[CV_OUTPUT].setVoltage(0.f);
        outputs[GATE_OUTPUT].setVoltage(0.f);
    }
}

typedef MeloDicer::NoteVal RDM_NoteVal;
const RDM_NoteVal MeloDicer::NOTEVALS[8] = {
    {1.0f, 1|2|4}, {0.5f, 1|2|4}, {0.25f, 1|2|4}, {0.125f, 2|4},
    {0.0625f, 2|4}, {1.0f/6.0f, 4}, {1.0f/12.0f, 4}, {0.03125f, 4},
};

Model* modelMeloDicer = createModel<MeloDicer, MeloDicerWidget>("MeloDicer");

void init(rack::Plugin* p) {
	pluginInstance = p;
	p->addModel(modelMeloDicer);
	p->addModel(modelMeloDicerExpander);
}