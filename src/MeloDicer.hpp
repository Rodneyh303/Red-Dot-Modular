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
#include "dsp/engines/ClockEngine.hpp"
#include "dsp/engines/PatternEngine.hpp"
#include "dsp/engines/SequencerEngine.hpp"
#include "dsp/managers/MeloDicerDNAManager.hpp"
#include "dsp/managers/MeloDicerParameterManager.hpp"
#include "dsp/managers/MeloDicerModeController.hpp"
#include "dsp/managers/MeloDicerUIManager.hpp"
#include "dsp/managers/MeloDicerTimingController.hpp"
#include "dsp/managers/MeloDicerCVRouter.hpp"
#include "dsp/managers/MeloDicerOutputGenerator.hpp"
#include "GateState.hpp"
#include "SequencerEngine.hpp"
#include "Scales.hpp"

#define MAX_UNIT64 18446744073709551615ULL

using namespace rack;

extern Plugin* pluginInstance;
struct MeloDicerExpander;
struct MeloDicerPolyVoiceExpander; // Forward declaration for the new expander
struct MeloDicerDNAExpander;
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
        ACCENT_KNOB,         // New: accent gate probability (0..1)
        TRANSPOSE_PARAM,
        PATTERN_LENGTH_PARAM,
        PATTERN_OFFSET_PARAM,

        // DNA Strand Windows (Grouped for easy looping: Len, Off, Rot)
        DNA_R_LEN_PARAM, DNA_R_OFF_PARAM, DNA_R_ROT_PARAM,
        DNA_V_LEN_PARAM, DNA_V_OFF_PARAM, DNA_V_ROT_PARAM,
        DNA_L_LEN_PARAM, DNA_L_OFF_PARAM, DNA_L_ROT_PARAM,
        DNA_A_LEN_PARAM, DNA_A_OFF_PARAM, DNA_A_ROT_PARAM,  // New: accent strand
        DNA_M_LEN_PARAM, DNA_M_OFF_PARAM, DNA_M_ROT_PARAM,
        DNA_O_LEN_PARAM, DNA_O_OFF_PARAM, DNA_O_ROT_PARAM,

        // DNA Buttons (6 Buttons)
        DNA_SCRAMBLE_ALL_PARAM,
        DNA_SCRAMBLE_R_PARAM,
        DNA_SCRAMBLE_M_PARAM,
        DNA_SCRAMBLE_V_PARAM, // Added for completeness
        DNA_SCRAMBLE_L_PARAM,
        DNA_SCRAMBLE_A_PARAM,  // New: scramble accent strand
        DNA_SCRAMBLE_O_PARAM,

        DNA_RESET_ALL_PARAM,
        DNA_RESET_R_PARAM,
        DNA_RESET_M_PARAM,
        DNA_RESET_V_PARAM, // Moved here to keep SEMI range contiguous
        DNA_RESET_L_PARAM,
        DNA_RESET_A_PARAM,  // New: reset accent strand
        DNA_RESET_O_PARAM,

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

        MODE_PARAM,

        RESET_BUTTON_PARAM,
        RUN_GATE_PARAM,

        // Poly Voice Expander Params (7 Knobs)
        POLY_REST_PARAM_1,
        POLY_REST_PARAM_2,
        POLY_REST_PARAM_3,
        POLY_REST_PARAM_4,
        POLY_REST_PARAM_5,
        POLY_REST_PARAM_6,
        POLY_REST_PARAM_7,

        // Poly DNA Window Controls (7 voices x 3 params = 21)
        POLY_DNA_VOICE_1_LEN,
        POLY_DNA_VOICE_1_OFF,
        POLY_DNA_VOICE_1_ROT,
        POLY_DNA_VOICE_2_LEN,
        POLY_DNA_VOICE_2_OFF,
        POLY_DNA_VOICE_2_ROT,
        POLY_DNA_VOICE_3_LEN,
        POLY_DNA_VOICE_3_OFF,
        POLY_DNA_VOICE_3_ROT,
        POLY_DNA_VOICE_4_LEN,
        POLY_DNA_VOICE_4_OFF,
        POLY_DNA_VOICE_4_ROT,
        POLY_DNA_VOICE_5_LEN,
        POLY_DNA_VOICE_5_OFF,
        POLY_DNA_VOICE_5_ROT,
        POLY_DNA_VOICE_6_LEN,
        POLY_DNA_VOICE_6_OFF,
        POLY_DNA_VOICE_6_ROT,
        POLY_DNA_VOICE_7_LEN,
        POLY_DNA_VOICE_7_OFF,
        POLY_DNA_VOICE_7_ROT,

        NUM_PARAMS
    };

    enum InputIds {
        CLK_INPUT = 0,
        GATE1_INPUT,
        GATE2_INPUT,
        CV1_INPUT,
        CV2_INPUT,
        ACCENT_CV_INPUT,      // New: accent probability CV modulation

        RUN_GATE_INPUT,
        RESET_TRIGGER_INPUT,
        SEED_INPUT,
        LENGTH_INPUT,
        OFFSET_INPUT,        // note: matches the typo in MeloDicer.cpp

        SEMI_CV_INPUT_0,  SEMI_CV_INPUT_1,  SEMI_CV_INPUT_2,  SEMI_CV_INPUT_3,
        SEMI_CV_INPUT_4,  SEMI_CV_INPUT_5,  SEMI_CV_INPUT_6,  SEMI_CV_INPUT_7,
        SEMI_CV_INPUT_8,  SEMI_CV_INPUT_9,  SEMI_CV_INPUT_10, SEMI_CV_INPUT_11,

        // DNA CV Modulation (10 Inputs)
        DNA_R_LEN_INPUT, DNA_R_OFF_INPUT,
        DNA_V_LEN_INPUT, DNA_V_OFF_INPUT,
        DNA_L_LEN_INPUT, DNA_L_OFF_INPUT,
        DNA_A_LEN_INPUT, DNA_A_OFF_INPUT,  // New: accent strand CV
        DNA_M_LEN_INPUT, DNA_M_OFF_INPUT,
        DNA_O_LEN_INPUT, DNA_O_OFF_INPUT,

        // DNA Gate Triggers (6 Inputs)
        // Corrected to include all scramble/reset inputs
        DNA_SCRAMBLE_ALL_INPUT,
        DNA_SCRAMBLE_R_INPUT,
        DNA_SCRAMBLE_M_INPUT,
        DNA_SCRAMBLE_V_INPUT,
        DNA_SCRAMBLE_L_INPUT,
        DNA_SCRAMBLE_A_INPUT,  // New: scramble accent strand
        DNA_SCRAMBLE_O_INPUT,
        DNA_RESET_ALL_INPUT,
        DNA_RESET_R_INPUT,
        DNA_RESET_M_INPUT,
        DNA_RESET_V_INPUT,
        DNA_RESET_L_INPUT,
        DNA_RESET_A_INPUT,  // New: reset accent strand
        DNA_RESET_O_INPUT,

        // Poly Voice Expander Inputs
        POLY_REST_CV_INPUT,

        NUM_INPUTS
    };

    enum ExpanderInputIds {
        // These are for the expander's own config() calls, not for MeloDicer's.
        // They must be distinct from MeloDicer's InputIds.
        EXPANDER_SEMI_CV_INPUT_0 = 100,
        EXPANDER_SEMI_CV_INPUT_1,
        EXPANDER_SEMI_CV_INPUT_2,
        EXPANDER_SEMI_CV_INPUT_3,
        EXPANDER_SEMI_CV_INPUT_4,
        EXPANDER_SEMI_CV_INPUT_5,
        EXPANDER_SEMI_CV_INPUT_6,
        EXPANDER_SEMI_CV_INPUT_7,
        EXPANDER_SEMI_CV_INPUT_8,
        EXPANDER_SEMI_CV_INPUT_9,
        EXPANDER_SEMI_CV_INPUT_10,
        EXPANDER_SEMI_CV_INPUT_11,

        EXPANDER_OCT_LO_CV_INPUT,
        EXPANDER_OCT_HI_CV_INPUT,

        NUM_EXPANDER_INPUTS
    };

    enum ExpanderParamIds {
        EXPANDER_SEMI_ATTENUVERTER_0 = 0,
        EXPANDER_OCT_LO_ATTENUVERTER = 12,
        EXPANDER_OCT_HI_ATTENUVERTER = 13,
        NUM_EXPANDER_PARAMS
    };

    enum OutputIds {
        GATE_OUTPUT = 0,
        CV_OUTPUT,
        SEED_OUTPUT,
        RESET_TRIGGER_OUTPUT,
        RUN_GATE_OUTPUT,
        TIE_OUTPUT,          // New: high on Tie decision
        LEGATO_OUTPUT,       // New: high on Legato/LegatoMax (not on Rest, Tie)
        ACCENT_OUTPUT,       // New: high on accented notes
        
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

        SCALE_EXPANDER_LIGHT,
        DNA_EXPANDER_LIGHT = SCALE_EXPANDER_LIGHT + 2,
        POLY_EXPANDER_LIGHT = DNA_EXPANDER_LIGHT + 2,
        RESET_LIGHT = POLY_EXPANDER_LIGHT + 2,
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
    ClockEngine clock;

    int cv1Mode = 0;
    int cv2Mode = 0;
    int gate1Assign = 0;
    int gate2Assign = 1;
    bool invertMuteLogic = false;
    bool restartOnUnmute = false;
    int lastModeSelect = -1;
    int scaleRoot = 0; 
    int lastSelectedScale = -1;
    bool lockScaleNotes = false;
    uint16_t activeScaleMask = 0xFFF;
    bool lightTheme = false;
    MeloDicerExpander* cachedExpander = nullptr; // Cache expander pointer for performance
    MeloDicerPolyVoiceExpander* cachedPolyVoiceExpander = nullptr;
    MeloDicerDNAExpander* cachedDnaExpander = nullptr;
    dsp::ClockDivider lightDivider;
    dsp::ClockDivider controlDivider; // For DNA modulation at "Control Rate"

    SequencerEngine engine;
    DNAStrandManager dnaManager{engine.pe};
    std::unique_ptr<ParameterManager> paramManager;  // Initialized in constructor
    std::unique_ptr<ModeController> modeController;  // Initialized in constructor
    std::unique_ptr<UIManager> uiManager;  // Initialized in constructor
    std::unique_ptr<TimingController> timingController;  // Initialized in constructor
    std::unique_ptr<CVRouter> cvRouter;  // Initialized in constructor
    std::unique_ptr<OutputGenerator> outputGenerator;  // Initialized in constructor

    // Convenience accessors
    rack::random::Xoroshiro128Plus& rhythmRng = engine.pe.rhythmRng;
    rack::random::Xoroshiro128Plus& melodyRng = engine.pe.melodyRng;
    //rack::random::Xoroshiro128Plus& stochasticRng = engine.pe.stochasticRng;
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
    //float& stochasticSeedFloat = engine.pe.stochasticSeedFloat;

    int scaleExpanderCount = 0;
    int dnaExpanderCount = 0;
    int polyExpanderCount = 0;
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
    float (&rhythmRandom)[16] = engine.pe.rhythmRandom;
    float (&variationRandom)[16] = engine.pe.variationRandom;
    float (&legatoRandom)[16] = engine.pe.legatoRandom;
    float (&melodyRandom)[16] = engine.pe.melodyRandom;
    float (&octaveRandom)[16] = engine.pe.octaveRandom;

    dsp::PulseGenerator& gatePulse = engine.gs.gatePulse;
    bool& prevExtGate = engine.prevGate1High;

    dsp::BooleanTrigger diceRTrig, diceMTrig, resetBtn, runGateBtn, modeTrig;
    // DNA Action Triggers (SchmittTriggers for gate inputs)
    dsp::SchmittTrigger DNA_SCRAMBLE_ALL_INPUTTrig, DNA_SCRAMBLE_R_INPUTTrig, DNA_SCRAMBLE_M_INPUTTrig, DNA_SCRAMBLE_V_INPUTTrig, DNA_SCRAMBLE_L_INPUTTrig, DNA_SCRAMBLE_A_INPUTTrig, DNA_SCRAMBLE_O_INPUTTrig;
    dsp::SchmittTrigger DNA_RESET_ALL_INPUTTrig, DNA_RESET_R_INPUTTrig, DNA_RESET_M_INPUTTrig, DNA_RESET_V_INPUTTrig, DNA_RESET_L_INPUTTrig, DNA_RESET_A_INPUTTrig, DNA_RESET_O_INPUTTrig;

    // BooleanTriggers for momentary buttons (params)
    dsp::BooleanTrigger DNA_SCRAMBLE_ALL_PARAMTrig, DNA_SCRAMBLE_R_PARAMTrig, DNA_SCRAMBLE_M_PARAMTrig, DNA_SCRAMBLE_V_PARAMTrig, DNA_SCRAMBLE_L_PARAMTrig, DNA_SCRAMBLE_A_PARAMTrig, DNA_SCRAMBLE_O_PARAMTrig;
    dsp::BooleanTrigger DNA_RESET_ALL_PARAMTrig, DNA_RESET_R_PARAMTrig, DNA_RESET_M_PARAMTrig, DNA_RESET_V_PARAMTrig, DNA_RESET_L_PARAMTrig, DNA_RESET_A_PARAMTrig, DNA_RESET_O_PARAMTrig;
    // SchmittTriggers for rotation knobs to detect "snap" logic if needed, 
    // though we use them as continuous offsets now.
    
    dsp::SchmittTrigger modeTrig;  // For mode cycling (now in UIManager, but kept for compatibility)
    dsp::PulseGenerator clockPulse;  // For clock output

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
    // CV routing state now in CVRouter
    // Transient octave offsets from CV1 modes 2 and 3.
    // Applied in getOctaveLoParam()/getOctaveHiParam() and cleared when CV1 is disconnected.
    // These are never written to params so they leave no persistent state.
    // CV routing offsets now in CVRouter

    MeloDicer();

    void updateExpanderPointers();
    void initialize();
    void updateScaleMask();

    json_t* dataToJson() override;
    void dataFromJson(json_t* root) override;

    float getSemitoneParam(int sem);  // DEPRECATED: use paramManager->getSemitone()
    
    // All other parameter getters now delegated to paramManager:
    // - getNoteValueParam()       → paramManager->getNoteValue()
    // - getVariationParam()       → paramManager->getVariation()
    // - getLegatoParam()          → paramManager->getLegato()
    // - getRestParam()            → paramManager->getRest()
    // - getAccentParam()          → paramManager->getAccent()
    // - getOctaveLoParam()        → paramManager->getOctaveLo()
    // - getOctaveHiParam()        → paramManager->getOctaveHi()
    // - getPolyRestParam(int)     → paramManager->getPolyRest(int)

    void switchMelodyMode();
    void switchRhythmMode();
    int pickSemitoneWeighted();
    //inline float unitRandomStochastic() { return engine.pe.unitStochastic(); }
    float genPitchV(int& outSemitone);
    int varyNoteIndex(int baseIdx);
    float semitoneToVolts(int semitone);
    PatternInput makePatternInput();
    void redrawRhythmPattern();
    void redrawMelodyPattern();

    void rotateRhythmPattern(int steps);
    void rotateMelodyPattern(int steps);
    void rotateRhythm(int steps);
    void rotateMelody(int steps);
    void rotateVariation(int steps);
    void rotateLegato(int steps);
    void rotateOctave(int steps);
    void rotatePattern(bool isMelody, int steps);
    void rotatePattern(int steps);

    void rebuildSemiCache_();
    float quantizeToScale(float vIn);
    void handleRestart(bool manual = true, bool resetImmediate = false);
    float sampleSeedFromSource();
    void onPhraseBoundary_();
    void onReset() override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    int getNoteLenIdx_();
    void onExpanderChange(const ExpanderChangeEvent& e) override; // Declare override
    int computeNoteLengthIdx(int requestedIdx, int ppqnMask);
    // UI/LED updates now delegated to UIManager:
    // - updateStepLEDs_()  → uiManager->updateSemitoneFlashLights()
    float quantizePitch(int semitoneIndex, int octaveOffset);

    void process(const ProcessArgs& args) override;

    void reseedXoroshiroFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat);

    // Mode handlers now delegated to ModeController:
    // - executeModeA()  → modeController->executeModeA()
    // - executeModeB()  → modeController->executeModeB()
    // - executeModeC()  → modeController->executeModeC()
    // - executeModeD()  → modeController->executeModeD()
};

extern Model* modelMeloDicer;
extern Model* modelMeloDicerExpander;
extern Model* modelMeloDicerDNAExpander;
extern Model* modelMeloDicerPolyVoiceExpander; // Declare new expander model
