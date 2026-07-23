#pragma once

/**
 * Monsoon.hpp
 * Public interface for Monsoon and its expanders.
 *
 * Include this in:
 *   - plugin.cpp         (for modelMonsoon)
 *   - any expander .cpp  (for ParamIds, InputIds, OutputIds, LightIds,
 *                          NoteVal, NOTEVALS, ExpanderMessage)
 *
 * Monsoon.cpp itself also includes this so the enums are defined once.
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
#include "dsp/engines/PhaseEngine.hpp"
#include "dsp/engines/PatternEngine.hpp"
#include "dsp/engines/SequencerEngine.hpp"
#include "dsp/managers/MonsoonSandsManager.hpp"
#include "dsp/managers/MonsoonParameterManager.hpp"
#include "dsp/managers/MonsoonPersistenceManager.hpp"
#include "dsp/managers/MonsoonScaleManager.hpp"
#include "dsp/managers/MonsoonExpanderManager.hpp"
#include "dsp/managers/MonsoonModeController.hpp"
#include "dsp/managers/MonsoonUIManager.hpp"
#include "dsp/managers/MonsoonTimingController.hpp"
#include "dsp/managers/MonsoonCVRouter.hpp"
#include "dsp/managers/MonsoonOutputGenerator.hpp"
#include "dsp/gates/GateState.hpp"
#include "dsp/engines/SequencerEngine.hpp"

#define MAX_UNIT64 18446744073709551615ULL

using namespace rack;

extern Plugin* pluginInstance;
struct MonsoonInterchangeExpander;
struct MonsoonStraitsExpander; // base poly expander (Straits redesign)
struct MonsoonSandsExpander;

// Minimal clamp helper for C++11 (no std::clamp)
template <typename T>
static inline T clampv(T v, T lo, T hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

#include "MonsoonWidget.hpp"

/**
 * InputState captures all audio-rate input signals in a single struct.
 */
struct InputState {
    float clk, gate1, gate2, gate3;
    float run, reset, cv1, cv2;
    bool  gate1Rise, gate2Rise;
};

// ── Parameter IDs ─────────────────────────────────────────────────────────────
// Stable integer values expanders can use to address params[] / inputs[] etc.
// Keep in sync with the enums inside struct Monsoon.
namespace MonsoonIds {

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
        POLY_REST_PARAM_8,   // NEW (Phase 4): voices 9-16
        POLY_REST_PARAM_9,
        POLY_REST_PARAM_10,
        POLY_REST_PARAM_11,
        POLY_REST_PARAM_12,
        POLY_REST_PARAM_13,
        POLY_REST_PARAM_14,
        POLY_REST_PARAM_15,

        // Accent Probability (15 voices) — accent as a poly lane, parallel to rest - NEW
        POLY_ACCENT_PARAM_1,
        POLY_ACCENT_PARAM_2,
        POLY_ACCENT_PARAM_3,
        POLY_ACCENT_PARAM_4,
        POLY_ACCENT_PARAM_5,
        POLY_ACCENT_PARAM_6,
        POLY_ACCENT_PARAM_7,
        POLY_ACCENT_PARAM_8,
        POLY_ACCENT_PARAM_9,
        POLY_ACCENT_PARAM_10,
        POLY_ACCENT_PARAM_11,
        POLY_ACCENT_PARAM_12,
        POLY_ACCENT_PARAM_13,
        POLY_ACCENT_PARAM_14,
        POLY_ACCENT_PARAM_15,
        
        // Rest Probability Modulation Attenuverters (15 voices) - NEW

        // Accent Probability Modulation Attenuverters (15 voices) - NEW

        // Causeway GLOBAL poly-mod attenuators — summed with the per-voice ones above.
        // Effective scale for voice v = perVoiceAtt[v] + globalAtt.

        // Causeway MONO (voice-1) mod attenuators — voice-1's counterpart to the poly per-voice
        // attenuators above. Applies to CV channel 0 (the mono channel) of the Causeway CV inputs,
        // added onto the mono base rest/accent. Panel marker param_restatt_0 / param_accatt_0.


        
        // Global Macro DNA Controls (for simple Straits Sands) - NEW
        // Single set of controls for all poly voices
        GLOBAL_REST_DNA_LEN,
        GLOBAL_REST_DNA_OFF,
        GLOBAL_REST_DNA_ROT,
        GLOBAL_REST_INTERP,
        
        GLOBAL_MELODY_DNA_LEN,
        GLOBAL_MELODY_DNA_OFF,
        GLOBAL_MELODY_DNA_ROT,
        GLOBAL_MELODY_INTERP,
        
        GLOBAL_OCTAVE_DNA_LEN,
        GLOBAL_OCTAVE_DNA_OFF,
        GLOBAL_OCTAVE_DNA_ROT,
        // Macro global accent L/O/R (Stage 6 — panel relayout complete)
        GLOBAL_ACCENT_DNA_LEN,
        GLOBAL_ACCENT_DNA_OFF,
        GLOBAL_ACCENT_DNA_ROT,
        GLOBAL_OCTAVE_INTERP,

        // Playable dice slew (0..1): live morph between locked (A) and candidate
        // (B) draws, latched at the bar boundary. Appended at END (IDs stable).
        DICE_SLEW_R_PARAM,
        DICE_SLEW_M_PARAM,

        // Live A<->B blend ("MIX", Model 1): continuously interpolates committed
        // pattern A with candidate B, like spread but global. SLEW (above) is
        // consumed at roll (Model 2, limits the step); MIX is the live morph.
        // Rhythm/melody independent so one can morph while the other holds.
        RHYTHM_MIX_PARAM,
        MELODY_MIX_PARAM,

        // Trial/audition dice (rhythm, melody): roll a fresh candidate B with A
        // ANCHORED (no promote), so the user auditions candidates against a fixed
        // A. The regular dice (DICE_R/M_PARAM) commits B→A (main mode).
        DICE_TRIAL_R_PARAM,
        DICE_TRIAL_M_PARAM,
        // LastDice / LastTrial: step the draw index opposite to dice/trial (Philox
        // addressability). Normal-mode only — blocked on reversible streams. Grouped
        // with their dice/trial siblings.
        LAST_DICE_R_PARAM,
        LAST_DICE_M_PARAM,
        LAST_TRIAL_R_PARAM,
        LAST_TRIAL_M_PARAM,

        // ── MACRO ranges (OWN 64, SEND 256, ATTEN 256, TAP 8 = 584) MIGRATED OUT of params[]
        //    to Monsoon::editor.macroOwn/macroSend/macroAtten (accessors getMacroOwn/…); tap re-homed to a Macro param;
        //    the DISP proxies (OWN_DISP 4 -> StraitsEastVisualIds, SEND_DISP 16 ->
        //    StraitsMacroVisualIds) re-homed to expander namespaces since they drive visible
        //    cells. NUM_PARAMS_MIGRATION.md. No shared-pool slots consumed here now. ──

        // Mode E manual phase knob. Full range = one upward ramp cycle (phase 0->1 = one bar).
        // Only used when CV1 (the phase input) is UNPATCHED -- the DAW-automatable fallback.
        PHASE_PARAM,

        NUM_PARAMS
    };

    enum InputIds {
        CLK_INPUT = 0,
        GATE1_INPUT,
        GATE2_INPUT,
        GATE3_MOD_INPUT,      // assignable gate mod (trial die / reseed toggles)
        CV1_INPUT,
        CV2_INPUT,
        CV3_MOD_INPUT,        // assignable CV mod (rhythm/melody slew or mix)
        ACCENT_CV_INPUT,      // New: accent probability CV modulation

        RUN_GATE_INPUT,
        RESET_TRIGGER_INPUT,
        SEED_INPUT,
        LENGTH_INPUT,
        OFFSET_INPUT,

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

        // Poly Voice Expander Inputs
        POLY_REST_CV_INPUT,
        
        // Poly Rest Probability Modulation (15 voices × 2 = 30 inputs) - NEW
        POLY_REST_MOD_CV_INPUT_1,
        POLY_REST_MOD_CV_INPUT_2,
        POLY_REST_MOD_CV_INPUT_3,
        POLY_REST_MOD_CV_INPUT_4,
        POLY_REST_MOD_CV_INPUT_5,
        POLY_REST_MOD_CV_INPUT_6,
        POLY_REST_MOD_CV_INPUT_7,
        POLY_REST_MOD_CV_INPUT_8,
        POLY_REST_MOD_CV_INPUT_9,
        POLY_REST_MOD_CV_INPUT_10,
        POLY_REST_MOD_CV_INPUT_11,
        POLY_REST_MOD_CV_INPUT_12,
        POLY_REST_MOD_CV_INPUT_13,
        POLY_REST_MOD_CV_INPUT_14,
        POLY_REST_MOD_CV_INPUT_15,

        // Accent Probability CV (shared + 15 per-voice mod) - NEW
        POLY_ACCENT_CV_INPUT,
        POLY_ACCENT_MOD_CV_INPUT_1,
        POLY_ACCENT_MOD_CV_INPUT_2,
        POLY_ACCENT_MOD_CV_INPUT_3,
        POLY_ACCENT_MOD_CV_INPUT_4,
        POLY_ACCENT_MOD_CV_INPUT_5,
        POLY_ACCENT_MOD_CV_INPUT_6,
        POLY_ACCENT_MOD_CV_INPUT_7,
        POLY_ACCENT_MOD_CV_INPUT_8,
        POLY_ACCENT_MOD_CV_INPUT_9,
        POLY_ACCENT_MOD_CV_INPUT_10,
        POLY_ACCENT_MOD_CV_INPUT_11,
        POLY_ACCENT_MOD_CV_INPUT_12,
        POLY_ACCENT_MOD_CV_INPUT_13,
        POLY_ACCENT_MOD_CV_INPUT_14,
        POLY_ACCENT_MOD_CV_INPUT_15,
        
        // Poly DNA CV Modulation (8 voices × 2 = 16 inputs) - MOVED TO SANDS
        POLY_DNA_VOICE_8_LEN_INPUT,
        POLY_DNA_VOICE_8_OFF_INPUT,
        POLY_DNA_VOICE_9_LEN_INPUT,
        POLY_DNA_VOICE_9_OFF_INPUT,
        POLY_DNA_VOICE_10_LEN_INPUT,
        POLY_DNA_VOICE_10_OFF_INPUT,
        POLY_DNA_VOICE_11_LEN_INPUT,
        POLY_DNA_VOICE_11_OFF_INPUT,
        POLY_DNA_VOICE_12_LEN_INPUT,
        POLY_DNA_VOICE_12_OFF_INPUT,
        POLY_DNA_VOICE_13_LEN_INPUT,
        POLY_DNA_VOICE_13_OFF_INPUT,
        POLY_DNA_VOICE_14_LEN_INPUT,
        POLY_DNA_VOICE_14_OFF_INPUT,
        POLY_DNA_VOICE_15_LEN_INPUT,
        POLY_DNA_VOICE_15_OFF_INPUT,

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

    // ── Raffles expander (dice/draw-generation modulation) ──────────────────
    // Its own param/input enums (distinct from Interchange's EXPANDER_*). 4 CV
    // attenuverters (slew R/M, mix R/M) + 10 dedicated die-action gate inputs.
    enum RafflesParamIds {
        RAFFLES_SLEW_R_ATT = 0,
        RAFFLES_SLEW_M_ATT,
        RAFFLES_MIX_R_ATT,
        RAFFLES_MIX_M_ATT,
        NUM_RAFFLES_PARAMS
    };
    enum RafflesInputIds {
        RAFFLES_SLEW_R_CV = 0,
        RAFFLES_SLEW_M_CV,
        RAFFLES_MIX_R_CV,
        RAFFLES_MIX_M_CV,
        // 10 die-action gates (order = display order on the panel)
        RAFFLES_GATE_TRIAL_R,
        RAFFLES_GATE_TRIAL_M,
        RAFFLES_GATE_REDICE_R,
        RAFFLES_GATE_REDICE_M,
        RAFFLES_GATE_LIVESRC_R,
        RAFFLES_GATE_LIVESRC_M,
        RAFFLES_GATE_LIVESTATIC_R,
        RAFFLES_GATE_LIVESTATIC_M,
        RAFFLES_GATE_RESEED_ROLL,
        RAFFLES_GATE_RESEED_RESTART,
        // LastDice / LastTrial gates (step draw index opposite to dice/trial).
        RAFFLES_GATE_LASTDICE_R,
        RAFFLES_GATE_LASTDICE_M,
        RAFFLES_GATE_LASTTRIAL_R,
        RAFFLES_GATE_LASTTRIAL_M,
        NUM_RAFFLES_INPUTS
    };

    // ── Junction expander (the big-5 pattern-knob modulation) ───────────────────
    // 5 CV + attenuverters summing into NOTE VALUE / VARIATION / LEGATO / REST /
    // ACCENT. Own port enums (0-based).
    enum JunctionParamIds {
        JUNCTION_NOTEVAL_ATT = 0,
        JUNCTION_VARIATION_ATT,
        JUNCTION_LEGATO_ATT,
        JUNCTION_REST_ATT,
        JUNCTION_ACCENT_ATT,
        NUM_JUNCTION_PARAMS
    };
    enum JunctionInputIds {
        JUNCTION_NOTEVAL_CV = 0,
        JUNCTION_VARIATION_CV,
        JUNCTION_LEGATO_CV,
        JUNCTION_REST_CV,
        JUNCTION_ACCENT_CV,
        NUM_JUNCTION_INPUTS
    };

    enum OutputIds {
        GATE_OUTPUT = 0,
        CV_OUTPUT,
        SEED_OUTPUT,
        RESET_TRIGGER_OUTPUT,
        RUN_GATE_OUTPUT,
        STEP_GATE_OUTPUT,         // The UN-FUSED gate: legato/tie REMOVED, every sub-note
                                  // articulated (see LEGATO_TIE_MODEL_NOTE.md). Replaces the
                                  // three old TIE/LEGATO/TIE_OR_LEGATO outputs -- those emitted
                                  // a join-CLASSIFICATION (a distinction the leading-edge model
                                  // can't make at onset); STEP GATE emits the pre-legato-drop
                                  // gate, which is the only truthful second gate. ENGINE
                                  // EMISSION PENDING: OutputGenerator must track the gate BEFORE
                                  // legato drops edges (a second gate-state / pre-drop flag),
                                  // not lastStepResult.decision.
        STEP_LEGATO_GATE_OUTPUT,  // STEP GATE masked to SLURRED notes only (silent on isolated
                                  // notes): the articulations that occur INSIDE a legato/tie.
                                  // = gsStep gated by (slurForward OR prevSlur). The generative
                                  // primitive: GATE + this derive STEP with one OR + a slur
                                  // latch; deriving this from GATE+STEP needs a 3-module latch
                                  // (the lead vs isolated instant is the onset-knowledge wall).
                                  // See STEP_GATE_IMPLEMENTATION.md. ENGINE EMISSION PENDING.
        ACCENT_OUTPUT,            // High when accented
        
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
        MODE_E_LIGHT,   // Mode E (phase-driven) was reachable but had no light: the cycle is
                        // (modeSelect+1)%5 and Monsoon.cpp handles modeSelect==4 throughout,
                        // so selecting it simply turned every mode light off.

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

} // namespace MonsoonIds

// ── Expander message structs ───────────────────────────────────────────────────
// Place data here that Monsoon needs to send to / receive from expanders.
// Rack passes these via Module::leftExpander.producerMessage /
// Module::rightExpander.producerMessage each process() call.
//
// Convention:
//   Right expander (e.g. extra CV outputs): Monsoon writes → expander reads
//   Left expander  (e.g. extra inputs):     expander writes  → Monsoon reads

// Sent rightward each sample — expander reads this
struct MonsoonRightMessage {
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

// Sent leftward each sample — Monsoon reads this from a left expander
struct MonsoonLeftMessage {
    // Extra CV inputs an expander might provide
    bool  hasExtraGate   = false;
    float extraGateV     = 0.f;
    bool  hasExtraClock  = false;
    float extraClockV    = 0.f;

    // CV inputs from expander
    float semiCv[12]     = {};
    float octLoCv        = 0.f;
    float octHiCv        = 0.f;

    // Remote control flags (set by expander, consumed by Monsoon)
    bool  requestDiceR   = false;  // pulse: arm new rhythm seed
    bool  requestDiceM   = false;  // pulse: arm new melody seed
    bool  requestReset   = false;  // pulse: trigger restart
    bool  requestLock    = false;  // level: override lock state
    bool  lockOverride   = false;  // value when requestLock=true
};


// ------------------------------- Module --------------------------------------
struct Monsoon : Module {
    ClockEngine clock;
    redDot::PhaseEngine phase;   // Mode E: CV1 phase ramp → pulse grid (forward+reverse)

    int cv1Mode = 0;
    int rhythmReversibleMode = 0;     // 0=Normal, 1=Reversible (per-stream, Mode E)
    int melodyReversibleMode = 0;
    int reseedOnModeChange = 1;       // global: reseed (+zero index) on entering reversible
    int resetIndexOnModeChange = 1;   // global: zero index on entry when NOT reseeding (greyed if reseed on)
    // Global probability-CV-out config (read by the Sands visual expanders; they have
    // no menus of their own). Scale 0=0..1V,1=0..5V,2=0..10V; S&H vs continuous.
    int  probOutScale = 2;
    bool probOutSampleHold = true;
    int cv2Mode = 0;

    // Assignable mod routing for the main-panel CV3 / GATE3 jacks (persisted).
    // CV3 adds to the selected continuous target; GATE3 rising edge fires the
    // selected action. Same target sets are offered (in full, attenuverted) on
    // the Raffles expander, and the contributions SUM.
    enum Cv3Target  { CV3_RHYTHM_SLEW=0, CV3_MELODY_SLEW, CV3_RHYTHM_MIX, CV3_MELODY_MIX, CV3_NUM_TARGETS };
    enum Gate3Target{ G3_TRIAL_RHYTHM=0, G3_TRIAL_MELODY, G3_TOGGLE_RESEED_ROLL, G3_TOGGLE_RESEED_RESTART,
                      G3_TOGGLE_RHYTHM_LIVESRC, G3_TOGGLE_MELODY_LIVESRC, G3_NUM_TARGETS };
    int  cv3Target   = CV3_RHYTHM_SLEW;
    int  gate3Target = G3_TRIAL_RHYTHM;
    dsp::SchmittTrigger gate3Trig;   // rising-edge detect for GATE3 actions
    dsp::SchmittTrigger rafflesGateTrig[14];  // Raffles's 14 die-action gates (incl Last*)
    // Which dice the LIVE mode drives, per lane: false=main (promote, A walks),
    // true=trial (anchored A, endless variations on a theme). Persisted.
    bool rhythmLiveTrial = false;
    bool melodyLiveTrial = false;

    // ── Shared die-action vocabulary ─────────────────────────────────────────
    // One definition of "what each die-action does", fired by G3 (menu-routed)
    // AND by Raffles's dedicated gates (and any future source). DRY: add an
    // action here and every gate source can use it.
    enum DieAction {
        DA_TRIAL_R = 0, DA_TRIAL_M,
        DA_REDICE_R, DA_REDICE_M,
        DA_LIVESRC_R, DA_LIVESRC_M,          // toggle live source main<->trial
        DA_LIVESTATIC_R, DA_LIVESTATIC_M,    // toggle live<->static (rhythmMode)
        DA_RESEED_ROLL, DA_RESEED_RESTART,
        DA_LASTDICE_R, DA_LASTDICE_M,        // step index opposite to dice
        DA_LASTTRIAL_R, DA_LASTTRIAL_M,      // audition previous candidate
        DA_NUM
    };
    void fireDieAction(int a);   // defined in Monsoon.cpp
    int gate1Assign = 0;
    int gate2Assign = 1;
    bool invertMuteLogic = false;
    bool restartOnUnmute = false;
    // Reseed policy (entropy housekeeping; context-menu, not panel — reseeding is
    // not a performance gesture). reseedOnRoll: a MAIN dice roll also reseeds
    // (fresh entropy / SEED CV) while keeping the A/B morph. TRIAL rolls never
    // reseed (auditioning stays controlled). reseedOnRestart: a restart reseeds
    // for a fresh pattern instead of replaying the held one.
    bool reseedOnRoll    = false;
    bool pendingRegenB   = false;   // set by load: regenerate candidate B post-seed (Option 3)
    bool reseedOnRestart = false;

    int lastModeSelect = -1;
    int lightTheme = 0; // 0 = Dark, 1 = Light. Using int to match PeranakanLatticePanel expectations.
    // Single source of truth for the spread interpolation target mode (context
    // menu lives on Monsoon). false = Average Poly (mono + active poly average);
    // true = Mono Draw (target the raw mono draw; mono strand becomes a fixed
    // anchor). Replaces the old per-visual interpUseMono flags on East/Macro.

    // Modulation-visualisation (mod arc) enables, grouped by surface. All default
    // on; toggled via the Monsoon "Modulation arcs" context submenu. Each arc's
    // isActive is gated by the relevant flag (read directly here, or via
    // findMonsoonEitherSide from the expander widgets).
    bool modVizMonsoonMelody = true;  // the 12 semitone + 2 octave pitch sliders (Interchange)
    bool modVizMonsoonOther  = true;  // big-5 knobs + slew/mix (Junction/CV2/CV3/Raffles)
    bool modVizEast          = true;  // Straits East per-voice rest + spread arcs
    bool modVizWest          = true;  // Straits West per-voice rest arcs
    bool modVizMacro         = true;  // Macro spread arcs
    bool modVizMono          = true;  // Mono Sands spread arcs
    MonsoonExpanderManager expanderManager;

    // Modulation-visualisation snapshot. Published once per process() from the
    // ParameterManager effective-value getters; read by the knob/slider widgets
    // on the UI thread to draw a live "set → modulated" indicator (only when the
    // control is actually being modulated). Values are NORMALISED 0..1 so a widget
    // can compare directly to its knob's normalised position. Stale-by-one-frame
    // is harmless for a visual. Extend this struct as more controls are covered
    // (slew/mix/bpm/global len-off/pitch/per-voice rest).
    struct ModViz {
        // Big-5 effective values, normalised 0..1 (NOTE_VALUE is /8).
        float noteValue = 0.f, variation = 0.f, legato = 0.f, rest = 0.f, accent = 0.f;
        bool  active = false;   // any big-5 modulation source present this frame
        // Slew + mix effective values, normalised 0..1 (all native 0..1).
        // Modulated via CV3 (cv3Offsets). activeCv3 gates their arcs.
        float rhythmSlew = 0.f, melodySlew = 0.f, rhythmMix = 0.f, melodyMix = 0.f;
        bool  activeCv3 = false;
        // Pitch sliders: 12 semitone (0..1) + octave lo/hi (normalised /8).
        // Modulated via the Interchange expander CV (+ CV1 for octaves).
        float semitone[12] = {0.f};
        float octaveLo = 0.f, octaveHi = 0.f;
        bool  activePitch = false;
        // Per-lane modulation flags (0=note,1=variation,2=legato,3=rest,4=accent);
        // and per-lane CV3 flags (0=rhythmSlew,1=melodySlew,2=rhythmMix,3=melodyMix).
        // Each arc gates on ITS OWN lane so an unmodulated knob never draws even when
        // a sibling lane is modulated. Kept at struct END to avoid shifting the
        // offsets of the fields above (ABI hygiene for incremental builds).
        bool  big5Lane[5] = {false,false,false,false,false};
        bool  cv3Lane[4]  = {false,false,false,false};
        // Per-lane pitch flags: 0..11 = semitones, 12 = octaveLo, 13 = octaveHi.
        bool  pitchLane[14] = {false};
    } modViz;
    dsp::ClockDivider lightDivider;
    dsp::ClockDivider controlDivider; // For DNA modulation at "Control Rate"

    SequencerEngine engine;

    // ── EditorState: per-voice-per-lane editor storage formerly held in MonsoonIds param
    //    ranges (see NUM_PARAMS_MIGRATION.md). Moved OUT of params[] because every configParam
    //    is exported to the DAW host, and ~828 of them overflowed the host pool so NO Monsoon-
    //    family param appeared in Bitwig. These are bus-addressable: expanders reach them via
    //    the Monsoon* they already hold, through the accessors below. Persisted in
    //    dataToJson/dataFromJson (params[] auto-saved; plain fields do not).
    //    PILOT: LANE_DIR only (96). Remaining ranges (MACRO*, VARLEG*) migrate next.
    struct EditorState {
        // laneDir: per lane (0..5). INDEXING IS POLY-BANK, NOT voiceSlot:
        //   v = 0..14 -> V2..V16 (poly bank index),  v = 15 -> V1/mono.
        // index = v*6 + lane  (identical to the old LANE_DIR_START + v*6 + lane).
        // Differs from lorBase/spread/varlegAtten/macroSend/macroAtten (voiceSlot, mono = 0).
        // Scheduled for reconciliation — docs/design/MVC_UNIFICATION.md step 1.
        float laneDir[96] = {0};
        // varlegDeleg: delegation local/delegated per lane (0=VAR,1=LEG). POLY-ONLY:
        //   v = 0..14 -> V2..V16. There is NO mono slot (mono cannot delegate to itself).
        // index = v*2 + lane  (== old VARLEG_DELEG_START + v*2 + lane).
        // The only array with no mono slice — see MVC_UNIFICATION.md step 2.
        float varlegDeleg[30] = {0};
        // varlegAtten: VAR/LEG attenuation per (v = 0..15 voice-slot) × lane(0=VAR,1=LEG) × col(0..2).
        // CONVENTION: VoiceResolver::voiceSlot (mono = slot 0). Verified against call sites.
        // index = v*6 + lane*3 + col  (== old VARLEG_ATTEN_START + v*6 + lane*3 + col).
        float varlegAtten[96] = {0};
        // macroOwn: owner per lane(0..3). INDEXING IS POLY-BANK, NOT voiceSlot:
        //   v = 0..14  -> V2..V16 (poly bank index)
        //   v = 15     -> V1/mono   (see getMonoMacroOwn/setMonoMacroOwn)
        // index = v*4 + lane. NOTE this differs from lorBase/spread/varlegAtten/macroSend/
        // macroAtten, which all use VoiceResolver::voiceSlot (mono = slot 0). Scheduled for
        // reconciliation — see docs/design/MVC_UNIFICATION.md step 1. (The previous comment
        // claimed "v=0..15 voice-slot, 15=mono", which is self-contradictory: voiceSlot(V1)
        // is 0, so this array is NOT on the voiceSlot convention.)
        float macroOwn[64] = {0};
        // macroSend: Macro-CV blend send per (v=0..15)×lane(0..3)×item(0..3). index=(v*4+lane)*4+item.
        // CONVENTION: VoiceResolver::voiceSlot (mono = slot 0). Verified against call sites.
        float macroSend[256] = {0};
        // macroAtten: atten depth per (v=0..15)×(lane*4+col, 16 wide). index = v*16 + lane*4+col.
        // CONVENTION: VoiceResolver::voiceSlot (mono = slot 0). Verified against call sites.
        float macroAtten[256] = {0};
        // Unified per-voice LOR base store (Stage 1 of the MVC unification). Indexed by
        // voiceSlot (0 = V1/mono, 1..15 = V2..V16) × bank (0=REST/DNA 1=MEL 2=OCT 3=ACC
        // 4=VAR 5=LEG) × (0=len 1=off 2=rot). This is the SINGLE home for every voice's
        // editable LOR base, replacing the per-voice East params (POLY_*_VOICE_1_LEN) and the
        // old V1-only eastV1Lor. Identity-initialised (len=16) in the Monsoon constructor.
        float lorBase[288] = {0};     // 16 slots × 6 banks × 3
        // Unified per-voice SPREAD store (Stage 1b). Same slot convention as lorBase
        // (0 = V1/mono, 1..15 = V2..V16) × lane 0..3 (REST/MEL/OCT/ACC — spread does not
        // apply to VAR/LEG). Zero-init = no spread, matching the old configParam default.
        // Replaces the per-voice interp params AND the old V1-only eastV1Spread. V1 is slot 0.
        float spread[64] = {0};       // 16 slots × 4 lanes
    } editor;

    // Unified LOR base accessors. slot = voiceSlot (0 = V1/mono), bank 0..5, c 0..2.
    float getLorBase(int slot, int bank, int c) const { return editor.lorBase[slot*18 + bank*3 + c]; }
    void  setLorBase(int slot, int bank, int c, float x) { editor.lorBase[slot*18 + bank*3 + c] = x; }
    // Unified spread accessors. slot = voiceSlot (0 = V1/mono), lane 0..3.
    float getSpread(int slot, int lane) const { return editor.spread[slot*4 + lane]; }
    void  setSpread(int slot, int lane, float x) { editor.spread[slot*4 + lane] = x; }

    // MACRO accessors (mirror old ownerId/sendId/attenId/tapId index math).
    float getMacroOwn(int v, int lane) const { return editor.macroOwn[v*4 + lane]; }
    void  setMacroOwn(int v, int lane, float x) { editor.macroOwn[v*4 + lane] = x; }
    float getMonoMacroOwn(int lane) const { return editor.macroOwn[15*4 + lane]; }
    void  setMonoMacroOwn(int lane, float x) { editor.macroOwn[15*4 + lane] = x; }
    float getMacroSend(int v, int lane, int item) const { return editor.macroSend[(v*4 + lane)*4 + item]; }
    void  setMacroSend(int v, int lane, int item, float x) { editor.macroSend[(v*4 + lane)*4 + item] = x; }
    float getMacroAtten(int v, int laneCol) const { return editor.macroAtten[v*16 + laneCol]; }
    void  setMacroAtten(int v, int laneCol, float x) { editor.macroAtten[v*16 + laneCol] = x; }

    // LANE_DIR accessors — the ONE place the index math lives (mirrors old dirId/monoDirId).
    float getLaneDir(int v, int lane) const { return editor.laneDir[v*6 + lane]; }
    void  setLaneDir(int v, int lane, float x) { editor.laneDir[v*6 + lane] = x; }
    float getMonoLaneDir(int lane) const { return editor.laneDir[15*6 + lane]; }
    void  setMonoLaneDir(int lane, float x) { editor.laneDir[15*6 + lane] = x; }

    // VARLEG accessors (mirror old varlegDelegId / varlegAttId index math).
    float getVarlegDeleg(int v, int lane) const { return editor.varlegDeleg[v*2 + lane]; }
    void  setVarlegDeleg(int v, int lane, float x) { editor.varlegDeleg[v*2 + lane] = x; }
    float getVarlegAtten(int v, int lane, int col) const { return editor.varlegAtten[v*6 + lane*3 + col]; }
    void  setVarlegAtten(int v, int lane, int col, float x) { editor.varlegAtten[v*6 + lane*3 + col] = x; }

    MonsoonSandsManager dnaManager{engine}; // Always valid
    std::unique_ptr<ParameterManager> paramManager;
    std::unique_ptr<ScaleManager> scaleManager;
    std::unique_ptr<ModeController> modeController; // Corrected name
    std::unique_ptr<UIManager> uiManager;
    std::unique_ptr<TimingController> timingController;
    std::unique_ptr<CVRouter> cvRouter;
    std::unique_ptr<OutputGenerator> outputGenerator;

    // Convenience accessors
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

    dsp::BooleanTrigger diceRTrig, diceMTrig, resetBtn, runGateBtn;
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

    // --- Audio-rate parameter caches (updated in controlDivider) ---
    float cachedBpmParam = 120.f;
    bool  cachedClkConnected = false;
    bool  cachedCv1Connected = false;
    bool  cachedCv2Connected = false;
    bool  cachedCv3Connected = false;
    bool  cachedGate1Connected = false;
    bool  cachedGate2Connected = false;
    bool  cachedGate3Connected = false;
    bool  cachedRunConnected = false;
    bool  cachedResetConnected = false;
    float cachedRunBtn = 0.f;
    float cachedResetBtn = 0.f;
    // (Per-voice effective rest/accent is resolved on demand via getEffectivePolyRest/Accent —
    //  no cached-effective arrays. The old cachedPolyRest[Effective]/cachedPolyAccent[Effective]
    //  push-copies were removed when the single-resolver pattern replaced them.)

    Monsoon();

    void updateExpanderPointers();
    void initialize();

    json_t* dataToJson() override;
    void dataFromJson(json_t* root) override;

    // Parameter getters
    float getSemitoneParam(int sem);
    float getNoteValueParam();
    float getVariationParam();
    float getLegatoParam();
    float getMonoRestBase();
    float getMonoAccentBase();
    float getRestParam();
    float getAccentParam();
    float getOctaveLoParam();
    float getOctaveHiParam();
    float getPolyRestParam(int voiceIdx);
    // Effective per-voice poly rest/accent — the SINGLE source of truth every consumer pulls from
    // (engine decision, Straits mod arcs). base = knob; effective = base + Causeway CV × att, clamped.
    float getBasePolyRest(int voiceIdx);
    float getBasePolyAccent(int voiceIdx);
    float getEffectivePolyRest(int voiceIdx);
    float getEffectivePolyAccent(int voiceIdx);
    float getEffectiveMonoRest(float base);
    float getEffectiveMonoAccent(float base);

    // All other parameter getters now delegated to paramManager:

    void switchMelodyMode();
    void switchRhythmMode();
    int pickSemitoneWeighted();
    //inline float unitRandomStochastic() { return engine.pe.unitStochastic(); }
    float genPitchV(int& outSemitone);
    int varyNoteIndex(int baseIdx);
    float semitoneToVolts(int semitone);
    void redrawRhythmPattern();
    void redrawMelodyPattern();

    void rotatePattern(bool isMelody, int steps);
    void rotatePattern(int steps);

    void rebuildSemiCache_();
    float quantizeToScale(float vIn);
    void handleRestart(bool manual = true, bool resetImmediate = false);
    float sampleSeedFromSource();
    // Dice gesture (shared by the panel buttons and gate-assigned re-dice): ROLL
    // (advance RNG, A/B morph) unless the SEED input is patched, in which case
    // sample-and-hold a reproducible seed. Keeps all dice triggers consistent.
    void diceRhythm();
    void diceMelody();
    void onPhraseBoundary_();
    void applyReversibleModeChange_();
    bool rhythmReversiblePrev_ = false, melodyReversiblePrev_ = false;
    void onReset() override;
    void onSampleRateChange(const SampleRateChangeEvent& e) override;
    int getNoteLenIdx_();
    void onExpanderChange(const ExpanderChangeEvent& e) override;
    int computeNoteLengthIdx(int requestedIdx, int ppqnMask);
    void updateStepLEDs_(float sampleTime);
    float quantizePitch(int semitoneIndex, int octaveOffset);

    void process(const ProcessArgs& args) override;

    // Mode handlers now delegated to ModeController:
    // - executeModeA()  → modeController->executeModeA()
    // - executeModeB()  → modeController->executeModeB()
    // - executeModeC()  → modeController->executeModeC()
    // - executeModeD()  → modeController->executeModeD()
};

extern Model* modelMonsoon;
extern Model* modelMonsoonInterchangeExpander;
extern Model* modelMonsoonRafflesExpander;
extern Model* modelMonsoonJunctionExpander;
extern Model* modelMonsoonChangeAlleyExpander;
//extern Model* modelMonsoonSandsExpander;
extern Model* modelMonsoonStraitsExpander; // base poly expander
extern Model* modelMonsoonCausewayPolyExpander; // poly CV modulation expander
extern Model* modelMonsoonChangiExpander; // per-voice output expander
extern Model* modelMonsoonShophouseExpander; // scale expander (12th module)
extern Model* modelLantern;                    // Lantern note-output visualiser
// West retired (Straits redesign)
//extern Model* modelMonsoonStraitsSands;        // NEW (Macro): global DNA controls (compact)
//extern Model* modelMonsoonDeepStraitsSandsEast; // NEW (Deep): per-voice DNA voices 2-8
//extern Model* modelMonsoonDeepStraitsSandsWest; // NEW (Deep): per-voice DNA voices 9-16
// Visual editor expanders
extern Model* modelMonsoonSandsVisualExpander;  // Mono visual DNA editor (voice 1)
extern Model* modelStraitsEastSandsVisual;      // East visual DNA editor (voices 2-8)
extern Model* modelStraitsSandsMacroVisual;     // Macro visual DNA editor (global)

// ─── Change Alley pin matrix expander ────────────────────────────────────────
namespace ChangeAlleyIds {
    // Restructure transforms (CHANGE_ALLEY_DESIGN.md §10/§11): Collapse/Rotate/Scatter/Reflect
    // × (Rhythm, Melody). Row order: C-R, C-M, Ro-R, Ro-M, Sc-R, Sc-M, Rf-R, Rf-M.
    enum ParamIds {
        BLOCK_KNOB_START = 0,          // 8 stepped block-size knobs (grain 1/2/4/8/16)
        TRIG_BTN_START   = 8,          // 8 manual trigger buttons (momentary)
        NUM_PARAMS       = 16
    };
    enum InputIds  { TRIG_IN_START = 0, NUM_INPUTS = 8 };   // 8 gate jacks
    enum OutputIds { NUM_OUTPUTS = 0 };
    enum LightIds  { PENDING_LIGHT_START = 0, NUM_LIGHTS = 8 };
    static constexpr int N_VOICES = 16;
    static constexpr int N_POOLS  = 2;   // 0=rhythm 1=melody
    enum Transform { T_COLLAPSE = 0, T_ROTATE = 1, T_SCATTER = 2, T_REFLECT = 3, N_TRANSFORMS = 4 };
    // Row index for (transform, isRhythm): t*2 + (isRhythm?0:1)
    inline int ctrlRow(int t, bool isRhythm) { return t * 2 + (isRhythm ? 0 : 1); }
}
