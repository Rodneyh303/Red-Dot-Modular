// ─────────────────────────────────────────────────────────────────────────────
// Lantern — note-output visualiser (read-only observer expander)
//
// "Lights up what's happening." A 16-lane bar view of the notes Monsoon is
// generating: one narrow lane per voice (row N = voice N), bars over the 16-step
// grid, colour = note TYPE (single/tie/legato/accent), sub-step-resolution ends,
// tie cross-lines, and held-over carets for notes spanning the bar boundary.
//
// PURE OBSERVER: reads Monsoon's engine state (via findMonsoonEitherSide) and
// keeps its OWN per-voice 16-step display ring buffer. It writes NO engine state.
// Independent of the Sands topology work. See docs/design/LANTERN_SPEC.md.
// ─────────────────────────────────────────────────────────────────────────────
#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"

using namespace rack;

// ── Note-name helper (self-contained; no engine change) ──────────────────────
// 1V/oct → "C4" style label. Rack convention: 0 V = C4.
namespace lantern {
    inline std::string noteName(float volts) {
        static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        int semis = (int)std::lround(volts * 12.0f);     // semitones from C4
        int idx   = ((semis % 12) + 12) % 12;
        int oct   = 4 + (int)std::floor((semis + (semis < 0 ? -0 : 0)) / 12.0);
        // floor division for octave (handles negatives)
        oct = 4 + (int)std::floor(semis / 12.0);
        return std::string(N[idx]) + std::to_string(oct);
    }

    // Note-type for the colour axis, mapped from the engine's MonoDecision.
    enum class NoteType : uint8_t { Inactive, Single, Tie, Legato, Accent };
}

// ── Module: display-only params, reads Monsoon read-only ─────────────────────
struct Lantern : Module {
    enum ParamIds {
        VIEW_PARAM,       // 0=Notes 1=Velocity 2=Prob (view-only)
        ZOOM_PARAM,       // 0=x1 1=x2 2=x4
        FOLLOW_PARAM,     // 0/1 keep phase in view
        NUM_PARAMS
    };
    enum InputIds  { NUM_INPUTS };
    enum OutputIds { NUM_OUTPUTS };
    enum LightIds  { NUM_LIGHTS };

    // Per-voice display ring buffer — Lantern's OWN accumulated history (the
    // engine only keeps lastStepResult, so we record what we observe each step).
    // [voice][step] for the 16 voices × 16 steps.
    struct Cell {
        lantern::NoteType type = lantern::NoteType::Inactive;
        float  pitchV   = 0.f;   // for the label
        float  lengthSteps = 0.f;// note length in steps (sub-step ok)
        bool   heldIn   = false; // continues from previous bar
        bool   heldOut  = false; // continues past step 16
        bool   accented = false;
    };
    Cell cells[16][16];
    int  lastObservedStep = -1;

    Lantern() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configSwitch(VIEW_PARAM,   0.f, 2.f, 0.f, "View", {"Notes", "Velocity", "Prob"});
        configSwitch(ZOOM_PARAM,   0.f, 2.f, 0.f, "Zoom", {"x1", "x2", "x4"});
        configSwitch(FOLLOW_PARAM, 0.f, 1.f, 1.f, "Follow", {"Off", "On"});
    }

    // Read-only sampler: called each process(); when a new step edge is seen on
    // the engine, records the current per-voice state into the display buffer.
    // NOTE: this only READS monsoon->engine; it never writes it.
    void process(const ProcessArgs& args) override {
        Monsoon* mon = redDot::findMonsoonEitherSide(this);
        if (!mon) return;
        SequencerEngine& eng = mon->engine;

        int step = eng.stepIndex;
        if (step == lastObservedStep) return;   // only sample on step edges
        lastObservedStep = step;
        if (step < 0 || step >= 16) return;

        // TODO(step): populate cells[voice][step] from:
        //   mono  → eng.gs, eng.lastStepResult
        //   poly  → eng.voices[v].gs, eng.voices[v].accented, eng.lastStepResult.decision
        // deriving NoteType from MonoDecision, lengthSteps from nvIdx via
        // gs_noteSteps(), heldIn/heldOut from holdRemain vs step position, pitchV
        // from GateState.currentPitchV. (Fleshed out next; scaffold records nothing
        // yet so it's a safe no-op observer.)
        (void)eng;
    }
};

// ── Display widget: the 16-lane bar render ───────────────────────────────────
struct LanternDisplay : widget::Widget {
    Lantern* module = nullptr;

    // Geometry (mm→px handled by the panel; these are the display-box internals).
    static constexpr int   N_VOICES = 16;
    static constexpr int   N_STEPS  = 16;
    // Layout inside the display box (matches gen_lantern.py): a left gutter for note
    // labels, a right strip for velocity/accent dots, the step grid in between.
    static constexpr float GUTTER_FRAC = 16.f / (208.28f - 12.f);  // ~16mm / DISP_W
    static constexpr float DOTS_FRAC   = 10.f / (208.28f - 12.f);  // ~10mm / DISP_W

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) { Widget::drawLayer(args, layer); return; }
        if (!module) return;
        const float W = box.size.x, H = box.size.y;
        const float laneH = H / N_VOICES;

        // Step grid + bar-boundary lines (1..4 bars of 4 steps) — TODO draw grid.
        // Per voice row:
        for (int v = 0; v < N_VOICES; ++v) {
            float y = v * laneH;
            // TODO: gutter label = lantern::noteName(cell.pitchV) or "-"
            for (int s = 0; s < N_STEPS; ++s) {
                const auto& c = module->cells[v][s];
                if (c.type == lantern::NoteType::Inactive) continue;
                // TODO: bar from step s to s + c.lengthSteps (sub-step), coloured by
                //   c.type; tie cross-lines; c.heldIn → left caret; c.heldOut → right.
                (void)c; (void)y;
            }
        }
        // TODO: current-phase red vertical line at module engine stepIndex.
    }

    // Colour per note type (brand-aligned; accent = brand red emphasis).
    static NVGcolor typeColour(lantern::NoteType t) {
        switch (t) {
            case lantern::NoteType::Single: return nvgRGB(0x6c, 0x8c, 0xd4);
            case lantern::NoteType::Tie:    return nvgRGB(0x8a, 0x6c, 0xd4);
            case lantern::NoteType::Legato: return nvgRGB(0x26, 0xa6, 0x9a);
            case lantern::NoteType::Accent: return nvgRGB(0xd4, 0x00, 0x1a);  // Singapore red
            default:                        return nvgRGB(0x3a, 0x40, 0x48);
        }
    }
};

// ── Module widget ────────────────────────────────────────────────────────────
struct LanternWidget : ModuleWidget {
    LanternWidget(Lantern* module) {
        setModule(module);
        // Panel SVG generated by panel_src/gen_lantern.py (dark; light via theme).
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Lantern_12HP.svg")));

        // Screws (C++ RedScrew, matching the family).
        addChild(createWidget<redDot::RedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<redDot::RedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<redDot::RedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<redDot::RedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // The display box mirrors gen_lantern.py: DISP_X=6, DISP_Y=16, DISP_W=W-12,
        // DISP_H=96 mm on a 41HP (208.28mm) panel. The widget reserves a left gutter
        // (~16mm) for note-name labels and a right strip (~10mm) for velocity/accent
        // dots; the 16 lanes fill the height (LANE_H = 96/16 = 6mm).
        auto* disp = new LanternDisplay();
        disp->module = module;
        disp->box.pos  = mm2px(Vec(6.f, 16.f));
        disp->box.size = mm2px(Vec(208.28f - 12.f, 96.f));
        addChild(disp);

        // TODO: view buttons (Notes/Velocity/Prob), Zoom knob, Follow button —
        //   wired to VIEW_PARAM / ZOOM_PARAM / FOLLOW_PARAM (display-only).
    }
};

Model* modelLantern = createModel<Lantern, LanternWidget>("Lantern");
