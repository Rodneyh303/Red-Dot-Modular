#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// ── Straits — base poly expander (transit family) ────────────────────────────
// The refactor/simplification of the old Straits East/West pair. West retires;
// one Straits module houses all poly voices. It carries the per-poly-voice REST
// and ACCENT probability knobs (each voice's reaction to the probability rolls),
// laid out as a 4-col × 8-row grid, and exposes the poly voices as 16-CHANNEL
// POLY CABLES (gate / accent-gate / CV) — replacing the old 21 individual jacks.
//
// Poly-cable channel convention (matches Causeway CV in and Changi out):
//   ch0 = MONO voice (voice 1) ALWAYS — duplicated from the parent Monsoon.
//   ch1..15 = poly voices 2..16.
// Cables are always 16ch wide; voices beyond engine.numPolyVoices output
// gate-low / 0V. Voice count is set by the context menu on the parent Monsoon
// (engine.numPolyVoices); Straits just reads it.
//
// REST/ACCENT base-level PARAMS reuse the existing MonsoonIds::POLY_REST_PARAM_*
// / POLY_ACCENT_PARAM_* enums (voices 2..16 = 15 poly voices; voice 1/mono's
// rest/accent lives on the parent Monsoon). No new engine plumbing — this is a
// panel + I/O simplification of what already exists.
//
// CV MODULATION of these levels moves OUT to the separate Causeway expander;
// per-voice mono OUT jacks move to the separate Changi expander. Straits itself
// carries only the base knobs + the poly-cable outs.
namespace StraitsIds {
    enum OutputIds {
        POLY_GATE_OUT,     // 16ch: ch0 = mono, ch1..15 = poly voices 2..16 (FUSED gate)
        POLY_STEP_GATE_OUT,// 16ch: the un-fused gate -- legato/tie REMOVED, every sub-note
                           //       articulated (see LEGATO_TIE_MODEL_NOTE.md). ENGINE EMISSION
                           //       PENDING: MonsoonOutputGenerator must compute the pre-fusion
                           //       gate; jack is wired so the panel + binding are ready.
        POLY_STEP_LEGATO_GATE_OUT,// 16ch: STEP GATE masked to SLURRED notes only (silent on
                           //       isolated notes) -- gsStep gated by slurForward||prevSlur,
                           //       per voice. The generative primitive (GATE+this => STEP via
                           //       one OR + latch). ENGINE EMISSION PENDING.
        POLY_CV_OUT,       // 16ch pitch
        POLY_ACCENT_OUT,   // 16ch accent gate
        NUM_OUTPUTS
    };
    // Straits reuses MonsoonIds for the rest/accent params, but the VOICE-COUNT knob is
    // Straits' OWN control (poly is gated on Straits being present, so Straits owns "how
    // many"). It lives ABOVE Monsoon's param namespace so it can't collide.
    enum ParamIds {
        VOICE_COUNT_PARAM = MonsoonIds::NUM_PARAMS,  // stepped 1..16; Monsoon READS this
        NUM_PARAMS
    };
    // Straits' OWN inputs (poly is gated on Straits being present, so the poly quantiser CV
    // in lives here). QUANTISER UNIFICATION Q2: the external note-CV source for the quantiser
    // modes — a 16-channel poly cable, ch0 = mono/voice 1, ch1..15 = poly voices 2..16 (the
    // SAME channel convention as Straits' poly-cable OUTputs). Monsoon READS this per-channel
    // into engine.quantiserCV[v] each quantiser step. Above MonsoonIds' input namespace so it
    // can't collide with the reused host input ids.
    enum InputIds {
        QUANT_CV_INPUT = MonsoonIds::NUM_INPUTS,   // 16ch external CV for the quantiser
        NUM_INPUTS
    };
}

struct MonsoonStraitsExpander : Module {
    MonsoonStraitsExpander() {
        // Sized to the main MonsoonIds param/input namespace (the expander reuses
        // those IDs), PLUS Straits' own params (the voice-count knob) and its own inputs
        // (the poly quantiser CV in), plus the local poly-cable outputs.
        config(StraitsIds::NUM_PARAMS, StraitsIds::NUM_INPUTS, StraitsIds::NUM_OUTPUTS, 0);

        // Poly voice count: stepped 1..16, set-and-forget (Slot widget on panel). Straits
        // OWNS this -- poly mode is gated on Straits being present, so the count lives here
        // and Monsoon READS it from the connected Straits. Default 1 (mono only) so adding
        // Straits doesn't silently change behaviour until the user dials voices.
        configParam(StraitsIds::VOICE_COUNT_PARAM, 1.f, 16.f, 1.f, "Poly voice count");
        getParamQuantity(StraitsIds::VOICE_COUNT_PARAM)->snapEnabled = true;

        // Voice 1 (mono) rest/accent: these knobs MIRROR the parent Monsoon's mono rest/accent
        // (driven each frame in process()). Configured here so the params exist with range; the
        // parent stays authoritative (see getRest/getAccent in the parameter manager).
        configParam(MonsoonIds::REST_PARAM,  0.f, 1.f, 0.1f, "Voice 1 (mono) Rest Probability - follows Monsoon");
        configParam(MonsoonIds::ACCENT_KNOB, 0.f, 1.f, 0.f,  "Voice 1 (mono) Accent Probability - follows Monsoon");

        // Per-poly-voice REST + ACCENT probability knobs (voices 2..16 = 15 knobs each).
        // Voice 1 (mono) rest/accent lives on the parent Monsoon.
        for (int i = 0; i < 15; i++) {
            configParam(MonsoonIds::POLY_REST_PARAM_1 + i, 0.f, 1.f, 0.1f,
                        "Voice " + std::to_string(i + 2) + " Rest Probability");
            configParam(MonsoonIds::POLY_ACCENT_PARAM_1 + i, 0.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " Accent Probability");
        }

        configOutput(StraitsIds::POLY_GATE_OUT,           "Poly gate (16ch: ch1 = mono, ch2.. = poly)");
        configOutput(StraitsIds::POLY_STEP_GATE_OUT,      "Poly STEP gate (16ch: legato removed -- every sub-note articulated)");
        configOutput(StraitsIds::POLY_STEP_LEGATO_GATE_OUT,"Poly STEP LEGATO gate (16ch: sub-note articulations inside slurs only)");
        configOutput(StraitsIds::POLY_CV_OUT,             "Poly CV / pitch (16ch)");
        configOutput(StraitsIds::POLY_ACCENT_OUT,         "Poly accent gate (16ch)");

        // QUANTISER UNIFICATION Q2: the poly note-CV IN for the quantiser modes (C/D). 16ch:
        // ch1 = mono/voice 1, ch2.. = poly voices 2..16 (same layout as the poly cable OUTs).
        // Monsoon reads it per-channel into engine.quantiserCV[v]. A 1-channel cable normals to
        // all voices (getPolyVoltage), so a mono source drives every voice in unison.
        configInput(StraitsIds::QUANT_CV_INPUT, "Quantiser CV in (16ch: ch1 = mono, ch2.. = poly)");
    }

    // The parent Monsoon writes the poly-cable outputs via the cached pointer (see
    // MonsoonOutputGenerator). The voice-1 (mono) knobs MIRROR the parent's mono rest/accent for
    // display — driven from the WIDGET each frame (matching the Sands visual-expander mirror idiom,
    // which does its param mirroring widget-side, not on the audio thread), since the mirror is
    // display-only (the engine reads Monsoon's own knob; see getRest/getAccent). Nothing per-sample.
    void process(const ProcessArgs& args) override {}
};
