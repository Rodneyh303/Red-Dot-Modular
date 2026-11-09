// PhaseDistortion — the STANDALONE phase-distortion module (the §13 comparison
// build). Spec: PHASE_DISTRIBUTION_SPEC §13 + docs/design/CC_PROMPT_PHASE_DISTORTION_MODULE.md.
// Built on feat/phase-duals-gaps ALONGSIDE the expander model (§14, the
// SandsDistributionExpander store) — do not delete or alter the expander build.
//
// THE MODEL (§13, Rodney-decided):
//   · EIGHT phase inputs, EIGHT phase outputs. Channel k in → channel k out.
//   · Each channel has its own parameter set (warp, tilt, gap, atom, delegation) —
//     the SAME PhaseDistSet shape the expander's store holds, in a PhaseDistStore
//     embedded here (one set shape, one codec, one identity predicate — shared
//     infrastructure, not the expander build).
//   · IDENTITY BY DEFAULT: an unpatched or zeroed channel passes phase through
//     unchanged PLUS the module's fixed hop.
//   · The transform is the ONE shared primitive (dotModular::phaseDistort — Task 2
//     of the expander prompt, already landed). Called, never copied. It evaluates
//     ONCE PER PPQN CELL of the input and holds between cells, through the SAME
//     cell-held core the Monsoon tier sites run (PhaseDistCellHold, one per
//     channel — CC_FIX_STANDALONE_PHASE_CELL_HOLD; the §13 per-sample model is
//     superseded, the gate now being the held contract "as for Monsoon").
//   · EVERY Monsoon's phase and subphase come from the module's OUTPUTS (the
//     channel IS the jack: a Monsoon patched to output k uses parameter set k;
//     several Monsoons may share an output). The patch rule is a PATCHING
//     CONVENTION — stated in the tooltips/manual text, NOT enforced by code.
//   · Monsoon gets NO transform code and NO index parameters in this build.
//   · DELAY: the module adds ONE hop (one sample, fixed). Documented in the manual
//     text (plugin.json + the jack tooltips); NOT compensated in software (the
//     §13 prompt's own DO-NOT).
//
// DE-PARAM LAW (the distribution-module convention): every control is store-backed
// (StoreKnob onto THIS module's PhaseDistStore) — the module exposes NO Rack params:
// config(0, 8, 8, 0). Host automation cannot fight the per-channel sets (the
// PARAM_CLASSIFICATION rule the visual/editor expanders run), and the sets persist
// through the store's codec (the additive key "phaseDistSets", the house
// missing-key-⟹-identity-defaults contract).
#pragma once
#include <rack.hpp>

#include "dsp/PhaseDistCore.hpp"   // the channel cores + the block reader (rack-free)

using namespace rack;

// ── the module ids ──────────────────────────────────────────────────────────────
namespace PhaseDistortionIds {
    static constexpr int kChannels = redDot::PhaseDistStore::kSets;   // 8 sets, 8 jacks
    // The param-AXIS is the distribution expander's OWN set — p1, p2 (the warp
    // member's per-kind params), tilt, level(ℓ), mass(m), warp(w), gaploc(a),
    // gapsize(b−a) — the §13 set maps onto it one-to-one (the member's params live
    // INSIDE f). The VISUAL order is the expander's verbatim: P1 P2 TILT LEVEL MASS
    // | GAPLOC GAPSIZE | WARP — "the gap sits immediately beside the atom so the
    // two read as a pair", WARP trails (the bidirectional depth, the sign IS the
    // reading).
    static constexpr int N_PARAMS = 8;
    static constexpr int VISUAL_ORDER[N_PARAMS] = {0, 1, 2, 3, 4, 6, 7, 5};
    static const char* const PARAM_CAPTIONS[N_PARAMS] = {
        "P1", "P2", "TILT", "LEVEL", "MASS", "WARP", "GAPLOC", "GAPSIZE",
    };
    // the SVG anchor prefixes (panel_src/gen_sands_phase_distortion.py emits them;
    // test/audit_anchor_bind.py gates the binds)
    static const char* const KNOB_ANCHOR  = "param_pd_";
    static const char* const FOCUS_ANCHOR = "param_pdfocus_";
    static const char* const IN_ANCHOR    = "input_pdin_";
    static const char* const OUT_ANCHOR   = "output_pdout_";
} // namespace PhaseDistortionIds

struct PhaseDistortion : rack::Module {
    // ── THE EIGHT PARAMETER SETS (§13 "each channel has its own parameter set") ──
    // The store is the expander model's container class, embedded here as B's OWN
    // local state (§13: "parameters are local to the module" — no cross-module
    // reads, no Monsoon store-pulls, no index parameters). Default construction is
    // EIGHT IDENTITY SETS. Knob writes go through writeSet (atomic per set); the
    // engine snapshots per control block (PhaseDistBlockReader).
    redDot::PhaseDistStore store;

    // ── the signal cores + the per-block working copies (rack-free, unit-tested) ─
    // Each channel core carries its OWN dotModular::PhaseDistCellHold (the SAME
    // type Monsoon's tier sites run — CC_FIX_STANDALONE_PHASE_CELL_HOLD): ch_ IS
    // the held-cell core, one hold per channel, and the process() call site keeps
    // its §13 shape verbatim. The fixed one-sample hop and the identity raw path
    // remain the channel's own (the §13 model's function).
    std::array<redDot::PhaseDistChannel, PhaseDistortionIds::kChannels> ch_{};
    redDot::PhaseDistBlockReader reader_;

    // The control-block cadence: snapshots are taken every kCtrlEvery frames
    // (~0.7 ms at 44.1 kHz — a knob drag lands at the next boundary, never
    // mid-block, and the hot path never locks). Frame 0 snapshots immediately so a
    // fresh module runs on real (identity) copies from the first sample.
    static constexpr int kCtrlEvery = 32;
    int ctrlCount_ = 0;

    PhaseDistortion() {
        // DE-PARAM LAW: no params, 8 phase inputs, 8 phase outputs.
        config(0, PhaseDistortionIds::kChannels, PhaseDistortionIds::kChannels, 0);
        for (int k = 0; k < PhaseDistortionIds::kChannels; ++k) {
            // THE MANUAL TEXT (the §13 prompt's Task 2: "Document it in the module's
            // manual text. Do not compensate in software.") — the convention + the
            // fixed hop live on the jacks' tooltips:
            configInput(k, "Phase in " + std::to_string(k + 1)
                + " (0..10 V phase ramp, wraps — patch EVERY Monsoon's phase and"
                  " subphase from the outputs, never direct from the source: a"
                  " direct path is one hop and desyncs by one sample)");
            configOutput(k, "Phase out " + std::to_string(k + 1)
                + " (= in, distorted by parameter set " + std::to_string(k + 1)
                + "; ONE-SAMPLE HOP — every path through this module is delayed"
                  " exactly one sample, fixed by design, not compensated)");
        }
        reader_.beginBlock(store);   // frame-0 copies (identity on a fresh module)
    }

    void process(const ProcessArgs& args) override;

    // ── persistence: the sets ride THIS module's patch state under the SAME
    // additive key the expander build uses ("phaseDistSets") — the store's codec
    // verbatim (missing/malformed ⟹ the identity defaults, the house contract).
    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "phaseDistSets", store.toJson());
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (!root) return;
        json_t* j = json_object_get(root, "phaseDistSets");
        if (j) store.fromJson(j);   // absent/malformed ⟹ the identity defaults survive
        reader_.beginBlock(store);  // a load lands at the next boundary — make it now
    }

    void onReset() override {
        // fromJson(nullptr) IS the store's reset path (resets-then-no-overwrite —
        // the WarpBank::parse convention): eight identity sets, no stale state.
        store.fromJson(nullptr);
        for (auto& c : ch_) c.reset();
        reader_.beginBlock(store);
    }
};
