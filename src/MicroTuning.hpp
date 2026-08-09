#pragma once
// ── MicroTuning — shared base for the Colonnades family (Model A tuning+scale AUTHORING expanders) ──
// Option B (Rodney): ONE copy of the Micro logic, parameterised on the DEGREE COUNT. Colonnades
// (N=12) and Colonnades Duo (N=24) are thin subclasses differing only in the degree count, the panel,
// and the cents-display layout. Extracted as a PURE REFACTOR of the Rack-verified Colonnades (guarded
// by test_TuningRoundTrip) — N=12 behaviour is byte-identical.
//
// A Micro CLAIMS Monsoon's tuning source and publishes BOTH cents[] AND weight[] into the shared
// TuningTable (maskAuthored), so Monsoon's own note faders grey out and delegate. Root (degree 0):
// cents LOCKED at 0 (Scalar rule). See MONSOON_MICRO_SPEC.md, MICRO_TUNING_INTEGRATION_PLAN.md,
// plans/colonnades_duo_micro24.md.
//
// Header stays free of Monsoon.hpp (no include cycle); MicroTuning.cpp pulls in Monsoon for the
// claim/publish + discovery wiring.

#include <rack.hpp>
#include <string>
#include <vector>
#include "tuning/TuningTable.hpp"        // dotModular::TuningTable::MAXN
#include "ui/SvgPanelKit.hpp"            // dotModular::Compose

namespace redDot { struct ConnectMark; }

// ── ID layout (contiguous, count-parameterised) ────────────────────────────────────────────────
// WEIGHT params [0 .. n-1], CENTS params [n .. 2n-1]; each degree has 2 sub-lights (green,red).
namespace microTuning {
    inline int   weightParam(int i)          { return i; }
    inline int   centsParam (int n, int i)   { return n + i; }
    inline int   numParams  (int n)          { return 2 * n; }
    inline int   weightLed  (int i)          { return 2 * i; }   // green=2i, red=2i+1
    inline int   numLights  (int n)          { return 2 * n; }

    // Equal-division default for degree i of an n-tone octave: i*(1200/n) cents. At n=12 this is
    // exactly i*100 (1200/12==100 in float) → byte-identical to the legacy Colonnades default, which
    // reproduces 12-TET. Root (i=0) is always 0.
    inline float defaultCents(int i, int n)  { return (float)i * (1200.f / (float)n); }

    // Tooltip label for a degree. 12-tone keeps the familiar note names (faithful to legacy); other N
    // use degree numbers (arbitrary tunings have no note names — MONSOON_MICRO_SPEC §34).
    inline std::string degreeLabel(int i, int n) {
        if (n == 12) {
            static const char* NN[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            return NN[((i % 12) + 12) % 12];
        }
        return std::to_string(i + 1);
    }
}

// ── Module base ────────────────────────────────────────────────────────────────────────────────
struct MicroTuningModule : rack::engine::Module {
    const int nDegrees;                    // 12 (Colonnades) or 24 (Colonnades Duo)
    std::string loadedTuningName;          // display-only name of a loaded .scl/.dmtune; persisted
    // SCALE-MEMBERSHIP mask (ENABLED_MASK_BUILD_BRIEF): per-degree enabled, SEPARATE from the weight
    // faders (which are pure loudness). enabledState[i]=false => degree out-of-scale (published to
    // tt.enabled → zeroed at read, fader dimmed). This is authored by the NOTES knob + the panel enable
    // band. Root (0) is ALWAYS enabled. Persisted. Default all-true for i<nDegrees.
    bool enabledState[dotModular::TuningTable::MAXN];
    // Pairing HUB id (3C-ii): Interchange expanders bind to this Micro by pairId (reuses the shared
    // redDot::*T pairing — same as Intertropical/Lantern/CA). Self-assigned in process() behind
    // pairChecked; persisted. 0 until assigned. pairColour(pairId) tints the ConnectMark.
    int  pairId = 0;
    bool pairChecked = false;              // runtime-only one-shot guard for the assign scan
    // Bound-Interchange cache (3C-ii): the rack-wide getModuleIds() discovery is control-rate (a
    // per-sample scan is the CA CPU pitfall). Cache the bound Interchange module pointers on a divider;
    // read their live CV every sample. Runtime-only.
    std::vector<rack::Module*> boundInterchanges_;
    rack::dsp::ClockDivider    ixScanDiv_;
    // Bound Shophouse Micro (Model Q scene source). Same divider-cached discovery — a per-sample
    // getModuleIds() scan is the CA CPU pitfall. Runtime-only.
    rack::Module*              boundShophouseMicro_ = nullptr;
    // Per-degree MODULATION VIZ (3C-ii): the published weight AFTER Interchange CV, + whether it
    // differs from the fader's set value. The fader widget draws a mod-arc marker from these (like
    // Monsoon's semitone arcs). Runtime-only; written each block in process().
    float modWeight[dotModular::TuningTable::MAXN] = {};
    bool  modActive[dotModular::TuningTable::MAXN] = {};
    // Effective (post-override) enabled mask, mirrored from tt.enabled[] AFTER any Shophouse Micro
    // override is applied, so the fader-dim widget reflects the ACTIVE scene's mask -- not just the
    // Colonnades' own authored enabledState[]. (Bug: faders didn't dim under a Shophouse Micro override
    // because the dim read enabledState[] (base) instead of the effective mask.)
    // Defaults all-true (in-scale): before the first process() mirrors tt.enabled[], a draw must show
    // faders IN-scale, not all-dimmed. Set true rather than the {}-zero (all-false = all-dimmed flash).
    bool  effectiveEnabled[dotModular::TuningTable::MAXN] = {
        true,true,true,true,true,true,true,true,true,true,true,true,
        true,true,true,true,true,true,true,true,true,true,true,true};

    explicit MicroTuningModule(int n) : nDegrees(n) {
        config(microTuning::numParams(n), 0, 0, microTuning::numLights(n));
        for (int i = 0; i < n; ++i) {
            // WEIGHT fader: 0..1, default 1 (all degrees enabled = chromatic; at equal-division cents
            // this reproduces Monsoon's default all-faders-up state → byte-identical at N=12).
            configParam(microTuning::weightParam(i), 0.f, 1.f, 1.f,
                        std::string("Weight (") + microTuning::degreeLabel(i, n) + ")");
            configParam(microTuning::centsParam(n, i), 0.f, 1200.f, microTuning::defaultCents(i, n),
                        std::string("Cents (") + microTuning::degreeLabel(i, n) + ")", " cents");
        }
        for (int i = 0; i < dotModular::TuningTable::MAXN; ++i)
            enabledState[i] = true;   // all degrees in-scale by default (chromatic); NOTES/band edit it
    }

    // Root cents-lock + (when claimed) publish cents[]+weight[]+maskAuthored into the shared
    // TuningTable, and drive the per-degree play-flash LEDs. Defined in MicroTuning.cpp (needs Monsoon).
    void process(const ProcessArgs& args) override;

    json_t* dataToJson() override {
        json_t* root = json_object();
        if (!loadedTuningName.empty())
            json_object_set_new(root, "loadedTuningName", json_string(loadedTuningName.c_str()));
        json_object_set_new(root, "pairId", json_integer(pairId));
        json_t* je = json_array();          // scale-membership mask (enabled), separate from weight faders
        for (int i = 0; i < nDegrees; ++i) json_array_append_new(je, json_boolean(enabledState[i]));
        json_object_set_new(root, "enabled", je);
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "loadedTuningName"))
            loadedTuningName = json_string_value(j);
        if (json_t* p = json_object_get(root, "pairId")) { pairId = (int)json_integer_value(p); pairChecked = true; }
        if (json_t* je = json_object_get(root, "enabled"); json_is_array(je))
            for (int i = 0; i < nDegrees && i < (int)json_array_size(je); ++i)
                enabledState[i] = json_boolean_value(json_array_get(je, i));
        enabledState[0] = true;             // root always in-scale (invariant)
    }
};

// ── Widget base ────────────────────────────────────────────────────────────────────────────────
// Concrete Compose-based ModuleWidget carrying ALL shared UI + the four file ops + context menu.
// Subclasses supply only the three differences via virtual hooks, then call build() from their ctor
// (virtuals are resolvable by then). Compose is CRTP-typed on THIS base; subclasses add no Compose.
struct MicroTuningWidget : rack::app::ModuleWidget,
    dotModular::Compose<MicroTuningWidget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {

    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    rack::widget::Widget* labels = nullptr;
    int lastThemeLight = -1;

    // ── Subclass hooks (the ONLY differences between Colonnades and Colonnades Duo) ──
    virtual int         mtDegrees()      const = 0;   // 12 / 24
    virtual const char* mtPanelDark()    const = 0;
    virtual const char* mtPanelLight()   const = 0;
    virtual const char* mtWordmark()     const = 0;   // "Colonnades" / "Colonnades Duo"
    // Cents readout: false = two staggered rows offset 50% (Colonnades); true = single row (Duo-24).
    virtual bool        mtCentsSingleRow() const = 0;

    // Shared ctor body — call from the subclass ctor AFTER setModule().
    void build();

    // File ops + menu (all degree-count-aware via mtDegrees()).
    void openScalaFilePicker();
    void openScalaSavePicker();
    void openDmtuneLoadPicker();
    void openDmtuneSavePicker();
    void appendContextMenu(rack::ui::Menu* menu) override;

    void step() override;
};
