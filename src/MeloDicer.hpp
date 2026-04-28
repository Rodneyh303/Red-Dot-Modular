#pragma once

/**
 * MeloDicer.hpp
 * Public interface for MeloDicer and its expanders.
 *
 * Include this in:
 *   - plugin.cpp         (for modelMeloDicer)
 *   - any expander .cpp  (for ParamIds, InputIds, OutputIds, LightIds,
 *                          NoteVal, NOTEVALS, ExpanderMessage)
 *
 * MeloDicer.cpp itself also includes this so the enums are defined once.
 * PatternEngine.hpp and GateState.hpp are separate standalone headers
 * (no Rack dependency) used by the engine unit tests.
 */

#include <rack.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>
#include "ClockEngine.hpp"
#include "PatternEngine.hpp"
#include "GateState.hpp"
#include "SequencerEngine.hpp"

#define MAX_UNIT64 18446744073709551615ULL

using namespace rack;

extern Plugin* pluginInstance;

// Minimal clamp helper for C++11 (no std::clamp)
template <typename T>
static inline T clampv(T v, T lo, T hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

#include "MeloDicerWidget.hpp"

// ── Parameter IDs ─────────────────────────────────────────────────────────────
// Stable integer values expanders can use to address params[] / inputs[] etc.
// Keep in sync with the enums inside struct MeloDicer.
namespace MeloDicerIds {

    enum ParamIds {
        NOTE_VALUE_PARAM = 0,
        VARIATION_PARAM,
        LEGATO_PARAM,
        REST_PARAM,
        TRANSPOSE_PARAM,
        PATTERN_LENGTH_PARAM,
        PATTERN_OFFSET_PARAM,

        SEMI0_PARAM,  SEMI1_PARAM,  SEMI2_PARAM,  SEMI3_PARAM,
        SEMI4_PARAM,  SEMI5_PARAM,  SEMI6_PARAM,  SEMI7_PARAM,
        SEMI8_PARAM,  SEMI9_PARAM,  SEMI10_PARAM, SEMI11_PARAM,

        OCT_LO_PARAM,
        OCT_HI_PARAM,
        BPM_PARAM,

        DICE_R_PARAM,
        DICE_M_PARAM,
        LOCK_PARAM,
        MUTE_PARAM,

        MODE_A_PARAM,
        MODE_B_PARAM,
        MODE_C_PARAM,
        MODE_D_PARAM,

        RESET_BUTTON_PARAM,
        RUN_GATE_PARAM,

        NUM_PARAMS
    };

    enum InputIds {
        CLK_INPUT = 0,
        GATE1_INPUT,
        GATE2_INPUT,
        CV1_INPUT,
        CV2_INPUT,

        RUN_GATE_INPUT,
        RESET_TRIGGER_INPUT,
        SEED_INPUT,
        LENGTH_INPUT,
        OFFFSET_INPUT,        // note: matches the typo in MeloDicer.cpp

        SEMI_CV_INPUT_0,  SEMI_CV_INPUT_1,  SEMI_CV_INPUT_2,  SEMI_CV_INPUT_3,
        SEMI_CV_INPUT_4,  SEMI_CV_INPUT_5,  SEMI_CV_INPUT_6,  SEMI_CV_INPUT_7,
        SEMI_CV_INPUT_8,  SEMI_CV_INPUT_9,  SEMI_CV_INPUT_10, SEMI_CV_INPUT_11,

        OCT_LO_CV_INPUT,
        OCT_HI_CV_INPUT,

        NUM_INPUTS
    };

    enum OutputIds {
        GATE_OUTPUT = 0,
        CV_OUTPUT,
        SEED_OUTPUT,
        RESET_TRIGGER_OUTPUT,
        RUN_GATE_OUTPUT,

        NUM_OUTPUTS
    };

    enum LightIds {
        STEP_LIGHTS_START = 0,
        STEP_LIGHTS_END   = STEP_LIGHTS_START + 16,

        // STEP_LIGHTS_END consumes slot 16; RHYTHM_DICE_LIGHT auto-follows at 17
        RHYTHM_DICE_LIGHT,
        MELODY_DICE_LIGHT,
        LOCK_LIGHT,
        MUTE_LIGHT,

        MODE_A_LIGHT,
        MODE_B_LIGHT,
        MODE_C_LIGHT,
        MODE_D_LIGHT,

        SEMI_LED_START,
        SEMI_LED_END = SEMI_LED_START + 24,  // 2 channels × 12 semitones

        // SEMI_LED_END consumes its slot; OCT_LO_LED auto-follows
        OCT_LO_LED,
        OCT_HI_LED,

        RESET_LIGHT,
        RUN_GATE_LIGHT,

        NUM_LIGHTS
    };

} // namespace MeloDicerIds

// ── Expander message structs ───────────────────────────────────────────────────
// Place data here that MeloDicer needs to send to / receive from expanders.
// Rack passes these via Module::leftExpander.producerMessage /
// Module::rightExpander.producerMessage each process() call.
//
// Convention:
//   Right expander (e.g. extra CV outputs): MeloDicer writes → expander reads
//   Left expander  (e.g. extra inputs):     expander writes  → MeloDicer reads

// Sent rightward each sample — expander reads this
struct MeloDicerRightMessage {
    // Current playback state (useful for display or CV generation expanders)
    float currentPitchV    = 0.f;  // 1V/oct output voltage
    int   currentSemitone  = -1;   // 0..11, -1 = none
    bool  gateHeld         = false;
    bool  running          = false;
    float bpm              = 120.f;
    int   stepIndex        = -1;   // 0..15, -1 = not started
    int   modeSelect       = 0;    // 0=A 1=B 2=C 3=D

    // Current pattern snapshot (useful for a pattern display expander)
    bool  rhythmPattern[16]  = {};
    int   melodySemitone[16] = {};
    float melodyPitchV[16]   = {};

    // Seed voltages
    float rhythmSeedV  = 0.f;  // 0..10V
    float melodySeedV  = 0.f;
};

// Sent leftward each sample — MeloDicer reads this from a left expander
struct MeloDicerLeftMessage {
    // Extra CV inputs an expander might provide
    bool  hasExtraGate   = false;
    float extraGateV     = 0.f;
    bool  hasExtraClock  = false;
    float extraClockV    = 0.f;

    // CV inputs from expander
    float semiCv[12]     = {};
    float octLoCv        = 0.f;
    float octHiCv        = 0.f;

    // Remote control flags (set by expander, consumed by MeloDicer)
    bool  requestDiceR   = false;  // pulse: arm new rhythm seed
    bool  requestDiceM   = false;  // pulse: arm new melody seed
    bool  requestReset   = false;  // pulse: trigger restart
    bool  requestLock    = false;  // level: override lock state
    bool  lockOverride   = false;  // value when requestLock=true
};


// ------------------------------- Module --------------------------------------
struct MeloDicer : Module {
    enum ParamIds {
        NOTE_VALUE_PARAM, VARIATION_PARAM, LEGATO_PARAM, REST_PARAM,
        TRANSPOSE_PARAM, PATTERN_LENGTH_PARAM, PATTERN_OFFSET_PARAM,
        SEMI0_PARAM, SEMI1_PARAM, SEMI2_PARAM, SEMI3_PARAM, SEMI4_PARAM, SEMI5_PARAM,
        SEMI6_PARAM, SEMI7_PARAM, SEMI8_PARAM, SEMI9_PARAM, SEMI10_PARAM, SEMI11_PARAM,
        OCT_LO_PARAM, OCT_HI_PARAM, BPM_PARAM,
        DICE_R_PARAM, DICE_M_PARAM, LOCK_PARAM, MUTE_PARAM,
        MODE_A_PARAM, MODE_B_PARAM, MODE_C_PARAM, MODE_D_PARAM,
        RESET_BUTTON_PARAM, RUN_GATE_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        CLK_INPUT, GATE1_INPUT, GATE2_INPUT, CV1_INPUT, CV2_INPUT,
        RUN_GATE_INPUT, RESET_TRIGGER_INPUT, SEED_INPUT, LENGTH_INPUT, OFFFSET_INPUT,
        SEMI_CV_INPUT_0, SEMI_CV_INPUT_1, SEMI_CV_INPUT_2, SEMI_CV_INPUT_3,
        SEMI_CV_INPUT_4, SEMI_CV_INPUT_5, SEMI_CV_INPUT_6, SEMI_CV_INPUT_7,
        SEMI_CV_INPUT_8, SEMI_CV_INPUT_9, SEMI_CV_INPUT_10, SEMI_CV_INPUT_11,
        OCT_LO_CV_INPUT, OCT_HI_CV_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        GATE_OUTPUT, CV_OUTPUT, SEED_OUTPUT, RESET_TRIGGER_OUTPUT, RUN_GATE_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        STEP_LIGHTS_START, STEP_LIGHTS_END = STEP_LIGHTS_START + 16,
        RHYTHM_DICE_LIGHT, MELODY_DICE_LIGHT, LOCK_LIGHT, MUTE_LIGHT,
        MODE_A_LIGHT, MODE_B_LIGHT, MODE_C_LIGHT, MODE_D_LIGHT,
        SEMI_LED_START, SEMI_LED_END = SEMI_LED_START + 24,
        OCT_LO_LED, OCT_HI_LED, RESET_LIGHT, RUN_GATE_LIGHT,
        NUM_LIGHTS
    };

    ClockEngine clock;

    int cv1Mode = 0;
    int cv2Mode = 0;
    int gate1Assign = 0;
    int gate2Assign = 1;
    int muteBehavior = 0;
    int lastModeSelect = -1;
    bool lightTheme = false;

    SequencerEngine engine;

    // Convenience accessors
    rack::random::Xoroshiro128Plus& rhythmRng = engine.pe.rhythmRng;
    rack::random::Xoroshiro128Plus& melodyRng = engine.pe.melodyRng;
    float& holdRemain = engine.gs.holdRemain;
    bool& gateHeld = engine.gs.gateHeld;
    float& currentPitchV = engine.gs.currentPitchV;
    int& lastSemitone = engine.gs.lastSemitone;
    float (&semiPlayRemain)[12] = engine.gs.semiPlayRemain;

    float& rhythmSeedFloat = engine.pe.rhythmSeedFloat;
    float& melodySeedFloat = engine.pe.melodySeedFloat;
    bool& rhythmSeedPending = engine.pe.rhythmSeedPending;
    float& rhythmSeedPendingFloat = engine.pe.rhythmSeedPendingFloat;
    bool& melodySeedPending = engine.pe.melodySeedPending;
    float& melodySeedPendingFloat = engine.pe.melodySeedPendingFloat;

    inline float unitRandomRhythm() { return engine.pe.unitRhythm(); }
    inline float unitRandomMelody() { return engine.pe.unitMelody(); }

    bool& locked = engine.locked;
    bool& muted = engine.muted;
    bool& runGateActive = engine.runGateActive;
    bool& resetArmed = engine.resetArmed;

    int& rhythmMode = engine.pe.rhythmMode;
    int& melodyMode = engine.pe.melodyMode;
    int& startStep = engine.startStep;
    int& endStep = engine.endStep;
    int& stepIndex = engine.stepIndex;
    int& lastStepIndex = engine.lastStepIndex;
    int& modeSelect = engine.modeSelect;
    int& ppqnSetting = engine.ppqnSetting;
    int& noteVariationMask = engine.noteVariationMask;
    int& cachedLength = engine.cachedLength;
    int& cachedOffset = engine.cachedOffset;
    float bpm = 120.f;

    bool (&rhythmPattern)[16] = engine.pe.rhythmPattern;
    float (&melodyPitchV)[16] = engine.pe.melodyPitchV;
    int (&melodySemitone)[16] = engine.pe.melodySemitone;

    dsp::PulseGenerator& gatePulse = engine.gs.gatePulse;
    bool& prevExtGate = engine.prevGate1High;

    dsp::BooleanTrigger diceRTrig, diceMTrig, resetBtn, runGateBtn;
    dsp::SchmittTrigger g1Trig, g2Trig, lockTrig, muteTrig, modeATrig, modeBTrig, modeCTrig, modeDTrig, resetTrig, runGateTrig;
    dsp::PulseGenerator runPulse, clockPulse, resetPulse;

    float& cachedMelodySeedFloat = engine.pe.cachedMelodySeedFloat;
    float& cachedRhythmSeedFloat = engine.pe.cachedRhythmSeedFloat;
    bool& melodySeedCached = engine.pe.melodySeedCached;
    bool& rhythmSeedCached = engine.pe.rhythmSeedCached;
    int& cachedMelodyStepIndex = engine.pe.cachedMelodyStepIndex;
    int& cachedMelodyLastStepIndex = engine.pe.cachedMelodyLastStepIndex;
    float (&cachedMelodyPitchV)[16] = engine.pe.cachedMelodyPitchV;
    int& cachedRhythmStepIndex = engine.pe.cachedRhythmStepIndex;
    int& cachedRhythmLastStepIndex = engine.pe.cachedRhythmLastStepIndex;
    bool (&cachedRhythmPattern)[16] = engine.pe.cachedRhythmPattern;

    int& activeSemiCount = engine.activeSemiCount;
    float (&faderCache)[12] = engine.faderCache;
    float cv2Offsets[4] = {0.f, 0.f, 0.f, 0.f};

    MeloDicer();

    void initialize();

    json_t* dataToJson() override;
    void dataFromJson(json_t* root) override;

    float getSemitoneParam(int sem);
    float getOctaveLoParam();
    float getOctaveHiParam();

    float getNoteValueParam();
    float getVariationParam();
    float getLegatoParam();
    float getRestParam();

    void switchMelodyMode();
    void switchRhythmMode();
    int pickSemitoneWeighted();
    float genPitchV(int& outSemitone);
    int varyNoteIndex(int baseIdx);
    float semitoneToVolts(int semitone);
    PatternInput makePatternInput();
    void redrawRhythmPattern();
    void redrawMelodyPattern();
    void rebuildSemiCache_();
    float quantizeToScale(float vIn);
    void handleRestart(bool manual = true, bool resetImmediate = false);
    float sampleSeedFromSource();
    void onPhraseBoundary_();
    void onReset() override;
    int getNoteLenIdx_();
    int computeNoteLengthIdx(int requestedIdx, int ppqnMask);
    void updateStepLEDs_(float sampleTime);
    float quantizePitch(int semitoneIndex, int octaveOffset);

    void process(const ProcessArgs& args) override;

    void reseedXoroshiroFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat);

    void handleModeA_(const ProcessArgs& args);
    void handleModeB_(const ProcessArgs& args, bool gate1Rise);
    void handleModeC_(const ProcessArgs& args);
    void handleModeD_(const ProcessArgs& args);
};

extern Model* modelMeloDicer;
extern Model* modelMeloDicerExpander;