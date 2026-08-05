#pragma once
#include <rack.hpp>

using namespace rack;

// Forward-decl: the module caches an Intertropical* (pointer only; full type in the .cpp).
struct Intertropical;

// ── Changi T3 — INTERTROPICAL arranged-output breakout (transit family) ──────
// The third Changi terminal. T1/T2 break out Monsoon's RAW 16-voice engine. T3
// breaks out Intertropical's ARRANGED 8-channel output — the Lantern parallel:
// Intertropical arranges (voice->slot->output, transpose), Lantern visualises it,
// Changi T3 jacks it out.
//
// 8 CHANNELS × 5 signals = 40 jacks, organised BY CHANNEL (each channel's
// gate/CV/accent/step/step-leg adjacent) — because an arranged output goes to one
// synth voice, so its signals want to be together. Layout per channel ch (0..7):
//   base = ch*5:  +0 GATE  +1 CV  +2 ACCENT  +3 STEP-GATE  +4 STEP-LEGATO
//
// DATA SOURCE (settled, CHANGI_TERMINAL_SPLIT.md "REVISED DECISION"): T3 is a
// self-binding OBSERVER like Lantern — it finds its Intertropical (via the shared
// pairing number) and in its OWN process() MIRRORS that Intertropical's output
// jacks: t3->out = it->outputs[GATE/CV/ACCENT/LEGATO/SLEG].getVoltage(ch). CV is
// already POST-TRANSPOSE + tie-latched inside Intertropical (effectiveTranspose),
// so T3 gets the sounding pitch for free. No Intertropical/Monsoon changes; no
// push code; not a Monsoon expander.
//
// (Intertropical's LEGATO_OUT carries the STEP gate and SLEG_OUT the STEP-LEGATO —
//  see Intertropical::process — so T3's STEP/STEP-LEG map to those two jacks.)
namespace ChangiT3Ids {
    static constexpr int N_CH    = 8;   // Intertropical's arranged output channels
    static constexpr int PER_CH  = 5;   // GATE, CV, ACCENT, STEP, STEP-LEGATO
    enum OutputIds {
        // Grouped by channel: output id = ch*PER_CH + signalOffset.
        OUT_FIRST = 0,
        NUM_OUTPUTS = N_CH * PER_CH   // 40
    };
    // Per-channel signal offsets (added to ch*PER_CH).
    enum Signal { GATE = 0, CV = 1, ACCENT = 2, STEP = 3, STEP_LEGATO = 4 };
    static inline int idx(int ch, int sig) { return ch * PER_CH + sig; }
}

struct MonsoonChangiT3Expander : Module {
    // Pairing: which Intertropical this T3 breaks out. 0 = Auto (nearest either-side);
    // >0 = the Intertropical whose pairId matches, anywhere in the rack. Persisted.
    // See ui/IntertropicalPairing.hpp.
    int followIT = 0;

    // Rate discipline: resolveFollowedIT() does a rack-wide getModuleIds() scan — topology is
    // control-rate, so cache the resolved pointer on a divider (mirrors the sibling-expander idiom
    // in PROCESS_RATE_AUDIT). The 40-jack MIRROR still runs per-sample from the cache, so signal
    // stays continuous; only the lookup is throttled. Runtime-only (not persisted).
    Intertropical* cachedIT_ = nullptr;
    rack::dsp::ClockDivider itLookupDiv_;

    MonsoonChangiT3Expander() {
        itLookupDiv_.setDivision(512);   // ~94 Hz @ 48k — control rate for topology
        config(0, 0, ChangiT3Ids::NUM_OUTPUTS, 0);
        using namespace ChangiT3Ids;
        for (int ch = 0; ch < N_CH; ++ch) {
            const std::string c = std::to_string(ch + 1);
            configOutput(idx(ch, GATE),        "Ch " + c + " gate");
            configOutput(idx(ch, CV),          "Ch " + c + " CV / pitch (post-transpose)");
            configOutput(idx(ch, ACCENT),      "Ch " + c + " accent gate");
            configOutput(idx(ch, STEP),        "Ch " + c + " step gate");
            configOutput(idx(ch, STEP_LEGATO), "Ch " + c + " step legato gate");
        }
    }

    // process() lives in the .cpp (needs the complete Intertropical type + pairing).
    void process(const ProcessArgs& args) override;

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "followIT", json_integer(followIT));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "followIT")) followIT = (int)json_integer_value(j);
    }
};
