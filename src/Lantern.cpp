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
#include <string>
#include <algorithm>
#include <cmath>

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
    // Accent is NOT here — it is an orthogonal overlay flag on the Cell.
    enum class NoteType : uint8_t { Inactive, Single, Tie, Legato };
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

    // Read-only sampler: records the observed per-step state into Lantern's own
    // display buffer on each step edge. Per-step, as-it-happens, NO look-back — a
    // note reads as Single when it fires and only changes to Tie/Legato at the step
    // where the engine's decision actually becomes a continuation (the 2nd note on).
    // This mirrors what Monsoon really does (probabilistic, not pre-planned), so the
    // colour change AT the join is the truthful tie/legato signal. Trusts MonoDecision.
    // Reads engine state only; writes nothing back.
    void process(const ProcessArgs& args) override {
        Monsoon* mon = redDot::findMonsoonEitherSide(this);
        if (!mon) return;
        SequencerEngine& eng = mon->engine;

        int step = eng.stepIndex;
        if (step == lastObservedStep) return;   // only sample on step edges
        lastObservedStep = step;
        if (step < 0 || step >= 16) return;

        const MonoDecision dec = eng.lastStepResult.decision;
        const bool accentedMono = eng.lastStepResult.accented;
        const float lenSteps = gs_noteSteps(eng.lastStepResult.nvIdx);

        // Row 0 = mono / V1.
        recordCell(0, step, eng.gs, dec, accentedMono, lenSteps);

        // Rows 1..numPolyVoices = poly voices 2.. . A poly voice follows mono's
        // gate TYPE (retrigger/tie/legato) but can independently REST (then it's
        // inactive this gate) and draws its OWN accent.
        for (int v = 0; v < eng.numPolyVoices && v < 15; ++v) {
            const PolyVoice& pv = eng.voices[v];
            recordCell(v + 1, step, pv.gs, dec, pv.accented, lenSteps);
        }
        // Voices beyond numPolyVoices → mark inactive.
        for (int v = eng.numPolyVoices; v < 15; ++v)
            cells[v + 1][step].type = lantern::NoteType::Inactive;
    }

    // Map an observed voice state at a step into a display Cell. NoteType priority:
    // inactive (gate down & not held) → else Accent if accented → else the join type
    // (Tie/Legato) from the decision → else Single (a fresh/normal note).
    void recordCell(int voice, int step, const GateState& gs, MonoDecision dec,
                    bool accented, float lenSteps) {
        Cell& c = cells[voice][step];
        const bool sounding = gs.gateHeld || gs.holdRemain > 0.0001f;
        if (!sounding) { c.type = lantern::NoteType::Inactive; return; }

        c.pitchV      = gs.currentPitchV;
        c.lengthSteps = lenSteps;
        c.accented    = accented;   // orthogonal overlay (render brightens/marks), NOT a type
        c.heldIn      = (dec == MonoDecision::Tie || dec == MonoDecision::MidNote);
        c.heldOut     = gs.holdRemain > 1.0001f;   // still ringing past this whole step

        // Note-TYPE is single/tie/legato only (accent is a separate overlay so an
        // accented tie still reads as a tie). As-it-happens, no look-back:
        if (dec == MonoDecision::Tie)                  c.type = lantern::NoteType::Tie;
        else if (dec == MonoDecision::Legato ||
                 dec == MonoDecision::LegatoMax)       c.type = lantern::NoteType::Legato;
        else                                           c.type = lantern::NoteType::Single;
        // (NewNote and MidNote both read as Single here — a MidNote is the tail of a
        //  note that already showed its type on the step it fired; drawing it as the
        //  same base colour keeps the bar continuous without claiming a new event.)
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
        NVGcontext* vg = args.vg;
        const float W = box.size.x, H = box.size.y;
        const float laneH = H / N_VOICES;
        const float gutter = W * GUTTER_FRAC;
        const float dots   = W * DOTS_FRAC;
        const float gridX  = gutter;
        const float gridW  = W - gutter - dots;
        const float stepW  = gridW / N_STEPS;

        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));

        for (int v = 0; v < N_VOICES; ++v) {
            const float y0 = v * laneH;
            const float yc = y0 + laneH * 0.5f;
            const float barH = laneH * 0.62f;

            // Label pitch = most recent sounding cell's pitch for this voice.
            bool anyActive = false; float labelPitch = 0.f;
            for (int s = N_STEPS - 1; s >= 0; --s) {
                if (module->cells[v][s].type != lantern::NoteType::Inactive) {
                    labelPitch = module->cells[v][s].pitchV; anyActive = true; break;
                }
            }

            // gutter label: "N  A3" (row number = voice number)
            if (font) {
                nvgFontFaceId(vg, font->handle);
                nvgFontSize(vg, std::min(11.f, laneH * 0.7f));
                nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, nvgRGB(0x8a, 0x8f, 0x98));
                std::string num = std::to_string(v + 1);
                nvgText(vg, 2.f, yc, num.c_str(), nullptr);
                if (anyActive) {
                    nvgFillColor(vg, nvgRGB(0xd8, 0xd8, 0xdc));
                    nvgText(vg, gutter * 0.42f, yc, lantern::noteName(labelPitch).c_str(), nullptr);
                } else {
                    nvgFillColor(vg, nvgRGB(0x4a, 0x50, 0x58));
                    nvgText(vg, gutter * 0.42f, yc, "-", nullptr);
                }
            }

            // per-step bars
            for (int s = 0; s < N_STEPS; ++s) {
                const auto& c = module->cells[v][s];
                if (c.type == lantern::NoteType::Inactive) continue;
                const float bx = gridX + s * stepW;
                float span = std::max(0.15f, c.lengthSteps <= 0.f ? 1.f : c.lengthSteps);
                float bw = std::min(span * stepW, (gridX + gridW) - bx);

                NVGcolor col = typeColour(c.type);
                // accent = shade: brighter when accented, slightly dimmed when not
                if (c.accented) col = nvgLerpRGBA(col, nvgRGB(0xff, 0xff, 0xff), 0.28f);
                else            col = nvgLerpRGBA(col, nvgRGB(0x00, 0x00, 0x00), 0.10f);

                nvgBeginPath(vg);
                nvgRoundedRect(vg, bx + 0.5f, yc - barH * 0.5f, std::max(1.5f, bw - 1.f), barH, 1.5f);
                nvgFillColor(vg, col);
                nvgFill(vg);

                // accented notes get a thin brand-red top edge for extra legibility
                if (c.accented) {
                    nvgBeginPath(vg);
                    nvgRect(vg, bx + 0.5f, yc - barH * 0.5f, std::max(1.5f, bw - 1.f), 1.2f);
                    nvgFillColor(vg, accentColour());
                    nvgFill(vg);
                }
                // held-in caret: note continued from previous bar (left of step 0)
                if (s == 0 && c.heldIn) {
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, gridX - 3.f, yc);
                    nvgLineTo(vg, gridX,        yc - barH * 0.4f);
                    nvgLineTo(vg, gridX,        yc + barH * 0.4f);
                    nvgClosePath(vg);
                    nvgFillColor(vg, nvgRGB(0xc8, 0x96, 0x0c));
                    nvgFill(vg);
                }
            }
        }

        // current-phase red line
        int ph = module->lastObservedStep;
        if (ph >= 0 && ph < N_STEPS) {
            float pxp = gridX + (ph + 0.5f) * stepW;
            nvgBeginPath(vg);
            nvgStrokeColor(vg, nvgRGBA(0xd4, 0x00, 0x1a, 0xcc));
            nvgStrokeWidth(vg, 1.2f);
            nvgMoveTo(vg, pxp, 0.f);
            nvgLineTo(vg, pxp, H);
            nvgStroke(vg);
        }
    }

    // Base colour per note type. Accent is drawn as a separate overlay (brighter
    // outline / brand-red marker) on top of these, so an accented note keeps its
    // tie/legato/single identity.
    static NVGcolor typeColour(lantern::NoteType t) {
        switch (t) {
            case lantern::NoteType::Single: return nvgRGB(0x6c, 0x8c, 0xd4);  // calm blue
            case lantern::NoteType::Tie:    return nvgRGB(0x8a, 0x6c, 0xd4);  // violet — held/joined
            case lantern::NoteType::Legato: return nvgRGB(0x26, 0xa6, 0x9a);  // teal — slid
            default:                        return nvgRGB(0x3a, 0x40, 0x48);  // inactive
        }
    }
    static NVGcolor accentColour() { return nvgRGB(0xd4, 0x00, 0x1a); }       // Singapore red overlay
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
