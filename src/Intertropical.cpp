// Intertropical  scene sequencer implementation (SCAFFOLD).
// See INTERTROPICAL_SPEC.md. Process = boundary-crossing advance + route active scene to poly
// outs. Widget = continuous-grid display, store-backed cells, widget-drawn live state.

#include "Intertropical.hpp"
#include "Monsoon.hpp"
#include "ui/SvgPanelKit.hpp"

using namespace rack;

// ============================ MODULE ============================

void Intertropical::process(const ProcessArgs& args) {
    Monsoon* host = redDot::findMonsoonEitherSide(this);
    if (!host) {
        // No host: nothing to route. Clear outputs.
        for (int o = 0; o < Ids::NUM_OUTPUTS; ++o) outputs[o].setChannels(0);
        return;
    }
    auto& eng = host->engine;

    // ---- boundary-crossing advance (phase-aware, direction-agnostic) ----
    // One phase cycle = 16 steps. The phrase boundary is a cycle completion, so the cycle
    // index is totalStepsElapsed / 16; a CHANGE in it (either direction) is a crossing.
    const int cycle = eng.totalStepsElapsed / 16;
    if (cycle != lastBoundary) {
        if (lastBoundary >= 0) {
            // A boundary was crossed. Advance the repeat, and the scene when repeats exhaust.
            // TODO: honour play direction for backward crossings (spec open item).
            repeatPos++;
            if (repeatPos >= getRepeats(activeScene)) {
                repeatPos = 0;
                activeScene = (activeScene + 1) % Ids::N_SCENES;
            }
        }
        lastBoundary = cycle;
        // Read-at-boundary rule: sample the active scene's membership NOW, act on it this cycle.
        liveMask = sceneMask[activeScene];
    }

    // ---- route: only the active scene's voices reach the poly outs ----
    // STUB: wire the host's per-voice gate/cv/accent/legato/sleg through liveMask.
    // For each of the 16 voices, if (liveMask >> v) & 1, pass that voice's value; else 0.
    const int nCh = Ids::N_VOICES;
    for (int o = 0; o < Ids::NUM_OUTPUTS; ++o) outputs[o].setChannels(nCh);
    for (int v = 0; v < nCh; ++v) {
        const bool in = (liveMask >> v) & 1u;
        // TODO: replace 0.f with the host engine's per-voice value for each output kind.
        //   GATE_OUT   : voice gate (sounding) -> 10V/0V
        //   CV_OUT     : voice pitch CV
        //   ACCENT_OUT : voice accent -> 10V/0V
        //   LEGATO_OUT : voice legato/tie state
        //   SLEG_OUT   : voice slew-legato state
        // Pulled from eng.voices[v-1] (V2..V16) / mono chain for V1, gated by `in`.
        float gate = in ? 0.f : 0.f;   // placeholder
        outputs[Ids::GATE_OUT].setVoltage(gate, v);
        outputs[Ids::CV_OUT].setVoltage(0.f, v);
        outputs[Ids::ACCENT_OUT].setVoltage(0.f, v);
        outputs[Ids::LEGATO_OUT].setVoltage(0.f, v);
        outputs[Ids::SLEG_OUT].setVoltage(0.f, v);
    }
}

// ============================ WIDGET ============================

// Continuous grid display: reads cell geometry from the panel (single-source-geometry) and
// draws live state (membership fill via voiceColour, active-scene highlight, repeat count +
// progress, voice numbers) over the static screen. Cells are store-backed toggles.
struct IntertropicalGrid : Widget {
    Intertropical* module = nullptr;
    // Grid rect in px (set from panel marker geometry at construction  TODO wire to panel).
    Rect gridBox;     // main 8x16 membership grid
    Rect repBox;      // repeat row (8 scenes, each 8-subdivided)

    // voiceColour: reuse Lantern's palette so voice identity is consistent across modules.
    static NVGcolor voiceColour(int v) {
        static const NVGcolor P[8] = {
            nvgRGB(0x6c,0x8c,0xd4), nvgRGB(0x26,0xa6,0x9a), nvgRGB(0xd4,0x8a,0x3c),
            nvgRGB(0xb0,0x6c,0xd4), nvgRGB(0x5c,0xb8,0x5c), nvgRGB(0xd4,0x6c,0x8c),
            nvgRGB(0x4c,0xb0,0xc8), nvgRGB(0xc8,0xb0,0x4c),
        };
        return P[((v % 8) + 8) % 8];
    }

    void onButton(const event::Button& e) override {
        if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) {
            Widget::onButton(e); return;
        }
        // Hit-test the main grid: toggle cell membership (store-backed).
        if (gridBox.contains(e.pos)) {
            const float cw = gridBox.size.x / Intertropical::Ids::N_SCENES;
            const float ch = gridBox.size.y / Intertropical::Ids::N_VOICES;
            int scene = (int)((e.pos.x - gridBox.pos.x) / cw);
            int voice = (int)((e.pos.y - gridBox.pos.y) / ch);
            module->setCell(scene, voice, !module->getCell(scene, voice));
            e.consume(this); return;
        }
        // Hit-test the repeat row: set repeats N by horizontal position within the scene cell.
        // TODO (spec lean): click-DRAG like a mini fader instead of pinpoint click.
        if (repBox.contains(e.pos)) {
            const float cw = repBox.size.x / Intertropical::Ids::N_SCENES;
            int scene = (int)((e.pos.x - repBox.pos.x) / cw);
            float frac = ((e.pos.x - repBox.pos.x) - scene*cw) / cw;   // 0..1 across the cell
            int n = 1 + (int)(frac * Intertropical::Ids::MAX_REPEAT);
            module->setRepeats(scene, n);
            e.consume(this); return;
        }
        Widget::onButton(e);
    }

    void draw(const DrawArgs& args) override {
        if (!module) return;
        NVGcontext* vg = args.vg;
        const int NS = Intertropical::Ids::N_SCENES;
        const int NV = Intertropical::Ids::N_VOICES;

        // --- membership cells: filled = in (voiceColour), hollow = out ---
        const float cw = gridBox.size.x / NS;
        const float ch = gridBox.size.y / NV;
        for (int s = 0; s < NS; ++s) {
            for (int v = 0; v < NV; ++v) {
                if (!module->getCell(s, v)) continue;
                NVGcolor col = voiceColour(v);
                nvgBeginPath(vg);
                nvgRect(vg, gridBox.pos.x + s*cw + 1, gridBox.pos.y + v*ch + 1, cw-2, ch-2);
                nvgFillColor(vg, col);
                nvgFill(vg);
            }
        }

        // --- active-scene column highlight ---
        {
            const int as = module->activeScene;
            nvgBeginPath(vg);
            nvgRect(vg, gridBox.pos.x + as*cw, gridBox.pos.y, cw, gridBox.size.y);
            nvgStrokeColor(vg, nvgRGBA(0xd4,0x00,0x1a,0xd0));   // Singapore red
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }

        // --- repeat row: count-lit + current-repeat emphasis ---
        // TODO: draw N lit sub-segments per scene (repeats[s]), brighter cell at repeatPos for
        //       the active scene. Colour-depth = count + progress.

        // --- voice numbers 1..16 in the left gutter (widget-drawn text) ---
        // TODO: nvgText row labels aligned to gridBox rows.
    }
};

struct IntertropicalWidget : ModuleWidget {
    IntertropicalWidget(Intertropical* module) {
        setModule(module);
        // TODO: setPanel with res/panels/Intertropical_panel_{dark,light}.svg via the theme kit.
        // TODO: place IntertropicalGrid over the panel screen, poly output jacks from markers.
        // Poly outs: GATE/CV/ACCENT/LEGATO/SLEG at the marker positions.
    }
};

Model* modelIntertropical = createModel<Intertropical, IntertropicalWidget>("Intertropical");
