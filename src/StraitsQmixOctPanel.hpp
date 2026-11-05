#pragma once
// ── StraitsQmixOctPanel — the qmix-octave per-voice panel (the QMIX_OCTAVE
// tier-1 surface). CC_PROMPT_QMIX_OCTAVE + QMIX_OCTAVE_PLAN.md Stage 3: the
// most-flexible end state (Rodney: "per voice separate qmix probability for
// pitch within octave and octave") — ONE attachable panel of 16 per-voice
// QMIX-OCTAVE knobs, the StraitsSubgatePanel build VERBATIM (the lane-panel
// form: 6/6/4, 165-wide, col-major v0 = mono top-left, the wave field, the
// seam rails; the qmix violet family hue — ONE panel, no lightness stepping;
// NO mod jack — modulation is Causeway).
//
// The dock: the run's LAST tier — lanes → subgate panels → THIS panel (the
// base's dock order law). Registered + HIDDEN — base-spawned only (the
// StraitsLane law; Rack still restores it natively and the base adopts the
// run). The bank lives in MONSOON (editor.qmixOctave — the QMIX_OCTAVE Stage
// 2 bank); this surface addresses it store-backed through the Monsoon
// accessors via redDot::lookupMonsoon. Touch = claim (the tier-1 opt-in);
// the base's "Clear per-voice overrides" is the un-claim path (both banks).
//
// Defaults are the byte-identity pin: with no cell claimed, the octave
// threshold follows the voice's degree qmix level (or the global knob when
// the base's source switch flips) — until a knob here is TOUCHED, nothing
// this panel does changes a single note (the QmixOctave::resolve contract).
#include <rack.hpp>
#include <atomic>

#include "Monsoon.hpp"               // the extern Model* block (modelStraitsQmixOctPanel lives there)
#include "StraitsSubgatePanel.hpp"   // isStraitsSubgateChainModel — the family predicate this module's hook answers to
#include "ui/ModuleLookupCore.hpp"   // THE lookup authority: the topology epoch (onExpanderChange bump)

namespace StraitsQmixOctIds {
    // Panel slug (persistence + spawn dispatch — the StraitsBase lane-slug
    // shape). ONE panel type; the base's json key "qmixOctPanels" carries an
    // ARRAY of these (the "subgatePanels" shape verbatim — robust to any
    // future count), values all this slug.
    inline const char* panelSlug() { return "QMIX_OCT"; }
    inline bool parsePanelSlug(const std::string& s) { return s == "QMIX_OCT"; }
}

// ── the SUBPANEL module — the pure attach surface (the subgate panel module
// verbatim, family-parameterised): no params, no inputs, no process — the
// StoreKnob authoring goes through Monsoon's store via the widget; no delta
// planes exist for this bank (no mod jack in scope), so no heartbeat either.
struct StraitsQmixOctPanelModule : rack::engine::Module {
    std::atomic<bool> needsResnap{false};   // writers: widget drag-end (UI) + the left-link check (engine); reader/clearer: the base's step

    StraitsQmixOctPanelModule() {
        config(0, 0, 0, 0);
    }

    // The lane's left-link fast path: a mid-chain insertion never changes the
    // BASE's own expanders, so it fires HERE (the StraitsLaneExpander hook
    // verbatim, family-parameterised — the predicate includes this panel).
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
        rack::Module* left = leftExpander.module;
        if (!left || !isStraitsSubgateChainModel(left->model))
            needsResnap.store(true, std::memory_order_relaxed);
    }
};

// The family predicate (seam detection + the resnap chain checks) is
// isStraitsSubgateChainModel (StraitsSubgatePanel.hpp) and grew THIS model —
// the one predicate both panel families answer to (the QOCT panel is part of
// the same Straits chain for seam/resnap purposes). Included above.
