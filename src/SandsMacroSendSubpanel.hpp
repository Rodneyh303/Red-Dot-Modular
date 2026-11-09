// SandsMacroSendSubpanel — the Macro send-knob subpanel (the spec's §"MACRO
// PER-VOICE SENDS → their own subpanel" + §"The MACRO SEND-KNOB subpanel
// ENCAPSULATES ALL Macro→East sends"). Spec:
// docs/design/CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md.
//
// ONE snapped subpanel owns the WHOLE Macro→East send bus:
//   - the EXISTING per-voice mix-in sends (7 lanes × LEN/OFF/ROT/SPR — the
//     editor.macroSend store the Macro visual's right strip used to host; the
//     strip MOVES here) + the PRE/POST taps (7 lanes × LOR/SPR — editor.macroTap),
//   - the NEW distribution-param send amounts (7 lanes × P1/P2/TILT/LEVEL/MASS/
//     WARP — editor.warp.distSend, the per-voice scalers of the Macro-sent
//     global deltas at the fold; WarpBank::distSend is THIS panel's store).
// The division of labour on Macro (the spec's ruling): the DISTRIBUTION expander
// SHAPES the (cross-voice) distribution; the SEND subpanel ROUTES everything —
// including the distribution-param mods — to the voices. Both snap RIGHT of the
// main Macro visual, in any order (the right chain IS the canonical order; the
// resnap preserves it, never re-sorts).
//
// DE-PARAM LAW: the module exposes NOTHING — config(0, 0, 0, 0); every control
// is a store-backed StoreKnob onto Monsoon's editor stores (the Macro strip's
// own convention, moved verbatim). The sends' addressing is the SEND VOICE
// (PatternEngine::sendVoice — CC_FIX_SEND_VOICE_FROM_MACRO; the MACRO strip's
// tab publishes it, Monsoon::macroViewVoice persists it; East's displayedVoice
// is unrelated): the knobs author the send voice's row, resolved LIVE per read
// — switching the Macro tab re-binds the view, every voice keeps its own
// persisted row.
//
// SNAPPING (the hard requirement): the SandsSnap cluster (ui/SandsSnap.hpp — the
// StraitsBase resnap family, shared with the Distribution expander). The Macro
// visual's snap family seats BOTH subpanels; this module joins as a guest
// (SandsSnapGuest: its own drag-end + a broken left link flag the host's
// consumer). East's family deliberately EXCLUDES this module — the send subpanel
// is a MACRO subpanel; parked beside East it stays inert (no forced seating, no
// resnap war), its host walk treating the East visual as a boundary (the segment
// rule: never walk PAST a host looking for a farther one).
//
// NO AUDIO-THREAD WORK: there is nothing to publish — the knobs write Monsoon's
// stores directly (the dist-send setter pairs its write with
// requestWarpRebuildVoiceLane so the changed fold lands in the baked mirror).
#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/SandsSnap.hpp"               // the guest mixin (the host's harvest reads it)
#include "ui/ModuleLookupCore.hpp"        // the topology epoch + lookupMemoizedT
#include "ui/ModuleLookup.hpp"            // lookupMonsoon (the store resolution for the knobs)

using namespace rack;

namespace SandsMacroSendIds {
    static constexpr int LANES       = 7;   // dotModular::SandsGrid::POLY_LANES
    static constexpr int SEND_ITEMS  = 4;   // LEN/OFF/ROT/SPR (editor.macroSend items)
    static constexpr int TAP_KINDS   = 2;   // LOR / SPR (editor.macroTap kinds)
    // PHASE_DISTRIBUTION_SPEC: the gap pair APPENDED to the fold axis (0..5 keep
    // their pre-gap meaning; the gap sends are 6/7) — = dotModular::WarpBank::DPARS.
    static constexpr int DIST_PARAMS = 8;
    static constexpr int NUM_KNOBS   = LANES * (SEND_ITEMS + TAP_KINDS + DIST_PARAMS);  // 98
    // the SVG anchor prefixes (panel_src/gen_sands_macro_send.py emits them)
    static const char* const SEND_ANCHOR = "param_msend_";   // + lane "_" item
    static const char* const TAP_ANCHOR  = "param_mtap_";    // + lane "_" kind
    static const char* const DIST_ANCHOR = "param_mdist_";   // + lane "_" param
    // column captions (widget-drawn lettering — nanosvg drops <text>)
    static const char* const SEND_CAPTIONS[SEND_ITEMS]  = {"LEN", "OFF", "ROT", "SPR"};
    static const char* const TAP_CAPTIONS[TAP_KINDS]    = {"LOR", "SPR"};
    static const char* const DIST_CAPTIONS[DIST_PARAMS] = {
        "P1", "P2", "TILT", "LEVEL", "MASS", "WARP", "GAPLOC", "GAPSIZE",
    };
    // the VISUAL column order (the gap pair beside the atom pair, WARP trailing) —
    // the anchors are (lane, param)-named, the SVG places the columns, the caption
    // draw reads this map (kept in sync with gen_sands_macro_send.py)
    static constexpr int DIST_VISUAL_ORDER[DIST_PARAMS] = {0, 1, 2, 3, 4, 6, 7, 5};
} // namespace SandsMacroSendIds

struct SandsMacroSendSubpanel : rack::Module, redDot::SandsSnapGuest {

    // ── host badge (UI-only — the knobs need only a reachable Monsoon; the badge
    // reports whether the LEFT chain actually claims the Macro visual). Plain
    // bool: the widget's step() is the only writer and the only reader's thread.
    bool macroHost = false;

    SandsMacroSendSubpanel() {
        // the de-param law: no params, no jacks — the whole panel is store-backed
        // knobs (the Macro strip's convention, moved here wholesale).
        config(0, 0, 0, 0);
    }

    // ── the host walk (the SandsDistributionRole template, send-subpanel
    // predicates): walks LEFT from this module; the first MACRO VISUAL reached
    // claims it. The EAST visual TERMINATES the walk (a host that cannot host
    // this panel — the segment rule: never walk past a host). Memoized by the
    // topology epoch (the L5 law — no raw walks per frame).
    bool resolveMacroHost();

    // ── the Sands subpanel family (seam continuity + the left-link check + the
    // connect mark): the two host visuals + both subpanels. NOTE this is the
    // SEAM/LOOSE family, not any host's seating family: the Macro visual seats
    // {Distribution expander, this}; East seats {Distribution expander} only.
    static bool isFamilyModel(const rack::Model* m);

    // ── topology events: bump the epoch (the ONLY legal bump site) + the
    // left-link hardening check (the StraitsLane idiom, family swapped).
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
        rack::Module* left = leftExpander.module;
        if (!(left && isFamilyModel(left->model)))
            needsResnap.store(true, std::memory_order_relaxed);
    }
};
