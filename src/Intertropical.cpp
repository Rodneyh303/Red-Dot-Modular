// Intertropical  scene sequencer implementation.
// See INTERTROPICAL_SPEC.md. Process = boundary-crossing advance + route active scene to poly
// outs. Widget = continuous-grid display, store-backed cells, widget-drawn live state.

#include "Intertropical.hpp"
#include "Monsoon.hpp"
#include "MonsoonStraitsExpander.hpp"   // complete type for cachedPolyVoiceExpander->outputs
#include "ui/SvgPanelKit.hpp"

using namespace rack;

// ============================ MODULE ============================

Intertropical::Intertropical() {
    for (int s = 0; s < Ids::N_SCENES; ++s)
        for (int v = 0; v < Ids::N_VOICES; ++v)
            sceneOutput[s][v] = -1;  // all auto-pack by default
    config(Ids::NUM_PARAMS, Ids::NUM_INPUTS, Ids::NUM_OUTPUTS, Ids::NUM_LIGHTS);
    configInput(Ids::PHASE_IN, "Phase (optional; else reads host)");
    configOutput(Ids::GATE_OUT,   "Gate (poly, active scene, <=8ch)");
    configOutput(Ids::CV_OUT,     "CV (poly, active scene, <=8ch)");
    configOutput(Ids::ACCENT_OUT, "Accent (poly, active scene, <=8ch)");
    configOutput(Ids::LEGATO_OUT, "Legato (poly, active scene, <=8ch)");
    configOutput(Ids::SLEG_OUT,   "SLEG (poly, active scene, <=8ch)");
}

void Intertropical::process(const ProcessArgs& args) {
    Monsoon* host = redDot::findMonsoonEitherSide(this);
    if (!host) {
        for (int o = 0; o < Ids::NUM_OUTPUTS; ++o) outputs[o].setChannels(0);
        return;
    }
    auto& eng = host->engine;

    // ---- boundary-crossing advance (phase-aware, direction-agnostic) ----
    // Use the engine's own phrase-boundary detection: lastStepResult.wrapped is true on the
    // step where the pattern wraps (accounts for modulated start/endStep correctly).
    // We track the stepIndex to detect when a new step result is available.
    const int si = eng.stepIndex;
    if (lastStepIndex >= 0 && si != lastStepIndex) {
        // A new step was processed. Check if the engine reported a phrase boundary wrap.
        if (eng.lastStepResult.wrapped) {
            repeatPos++;
            if (repeatPos >= getRepeats(activeScene)) {
                repeatPos = 0;
                activeScene = (activeScene + 1) % getLoopLen();
            }
            // Read-at-boundary rule: sample the active scene's membership NOW.
            liveMask = sceneMask[activeScene];
        }
    }
    lastStepIndex = si;

    // ---- route: hybrid auto-pack + override routing (<=8 output channels) ----
    // ---- route: read the Straits expander's 16-channel poly outputs (the poly output source) ----
    // The Straits expander (cachedPolyVoiceExpander) has the 16-channel poly outputs computed by
    // the OutputGenerator (gs.process — authoritative gate voltage). The host Monsoon's own outputs
    // are mono-only (1 channel). Straits is required for poly mode.
    // computeRouting() maps each active voice to an output channel (0..7): forced overrides
    // first, then auto-pack. Output channel count = number of routed voices (<=8).
    auto* straits = host->expanderManager.cachedPolyVoiceExpander;
    int8_t routing[Ids::N_VOICES];
    computeRouting(activeScene, routing);
    int nOut = 0;
    for (int v = 0; v < Ids::N_VOICES; ++v)
        if (routing[v] >= 0) nOut = std::max(nOut, routing[v] + 1);
    for (int o = 0; o < Ids::NUM_OUTPUTS; ++o) outputs[o].setChannels(nOut);
    for (int o = 0; o < Ids::NUM_OUTPUTS; ++o)
        for (int ch = 0; ch < nOut; ++ch)
            outputs[o].setVoltage(0.f, ch);
    if (!straits) return;  // no Straits = no poly outputs = silence
    // Straits output IDs: POLY_GATE_OUT=0, POLY_STEP_GATE_OUT=1, POLY_STEP_LEGATO_GATE_OUTPUT=2,
    // POLY_CV_OUT=3, POLY_ACCENT_OUT=4 (from StraitsIds::OutputIds).
    for (int v = 0; v < Ids::N_VOICES; ++v) {
        int ch = routing[v];
        if (ch < 0) continue;
        outputs[Ids::GATE_OUT].setVoltage(straits->outputs[0].getVoltage(v), ch);
        outputs[Ids::CV_OUT].setVoltage(straits->outputs[3].getVoltage(v), ch);
        outputs[Ids::ACCENT_OUT].setVoltage(straits->outputs[4].getVoltage(v), ch);
        outputs[Ids::LEGATO_OUT].setVoltage(straits->outputs[1].getVoltage(v), ch);
        outputs[Ids::SLEG_OUT].setVoltage(straits->outputs[2].getVoltage(v), ch);
    }
}

// ============================ WIDGET ============================

// Panel geometry constants (mm). Panel is ~22HP (330px × 379.43px at 75 DPI = ~112mm × 128.5mm).
// Derived from the panel SVG's rect/circle coordinates (converted px→mm at 75/25.4).
// Repeat row rect: px(35.4, 47.2, 276.9, 26.6) → mm(12.0, 16.0, 93.7, 9.0)
// Main grid rect:  px(35.4, 82.7, 276.9, 240.7) → mm(12.0, 28.0, 93.7, 81.5)
// Jack wells (5):  px(63.1, 118.5, 173.9, 229.2, 284.6) × cy=346.9 → mm y=117.5
static constexpr float IT_GRID_X   = 12.0f;  // grid left edge (matches panel rect)
static constexpr float IT_GRID_Y   = 28.0f;  // grid top (matches panel rect y=82.7px)
static constexpr float IT_GRID_W   = 93.7f;  // grid width (8 cols × ~11.7mm)
static constexpr float IT_GRID_H   = 81.5f;  // grid height (16 rows × ~5.1mm)
static constexpr float IT_REP_Y    = 16.0f;  // repeat row top (matches panel rect y=47.2px)
static constexpr float IT_REP_H    = 9.0f;   // repeat row height (matches panel rect h=26.6px)
static constexpr float IT_JACK_Y   = 117.5f; // output jack row (matches panel cy=346.9px)
static constexpr float IT_JACK_X[5] = { 21.4f, 40.1f, 58.9f, 77.6f, 96.4f };  // 5 jacks from panel SVG

// Continuous grid display: reads cell geometry from the panel constants and draws live state
// (membership fill via voiceColour, active-scene highlight, repeat count + progress, voice
// numbers) over the static screen. Cells are store-backed toggles.
struct IntertropicalGrid : Widget {
    Intertropical* module = nullptr;
    Rect gridBox;     // main 8x16 membership grid (px)
    Rect repBox;      // repeat row (px)

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
        if (!module || e.action != GLFW_PRESS) { Widget::onButton(e); return; }
        // Right-click on a filled cell: cycle output override (Auto -> 0 -> 1 -> ... -> 7 -> Auto).
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT && gridBox.contains(e.pos)) {
            const float cw = gridBox.size.x / Intertropical::Ids::N_SCENES;
            const float ch = gridBox.size.y / Intertropical::Ids::N_VOICES;
            int scene = (int)((e.pos.x - gridBox.pos.x) / cw);
            int voice = (int)((e.pos.y - gridBox.pos.y) / ch);
            if (module->getCell(scene, voice)) module->cycleOutput(scene, voice);
            e.consume(this); return;
        }
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) { Widget::onButton(e); return; }
        // Left-click: toggle cell membership (store-backed).
        if (gridBox.contains(e.pos)) {
            const float cw = gridBox.size.x / Intertropical::Ids::N_SCENES;
            const float ch = gridBox.size.y / Intertropical::Ids::N_VOICES;
            int scene = (int)((e.pos.x - gridBox.pos.x) / cw);
            int voice = (int)((e.pos.y - gridBox.pos.y) / ch);
            module->setCell(scene, voice, !module->getCell(scene, voice));
            e.consume(this); return;
        }
        // Hit-test the repeat row: set repeats N by horizontal position within the scene cell.
        if (repBox.contains(e.pos)) {
            const float cw = repBox.size.x / Intertropical::Ids::N_SCENES;
            int scene = (int)((e.pos.x - repBox.pos.x) / cw);
            float frac = ((e.pos.x - repBox.pos.x) - scene*cw) / cw;
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

        // --- grid lines (faint) ---
        const float cw = gridBox.size.x / NS;
        const float ch = gridBox.size.y / NV;
        nvgStrokeColor(vg, nvgRGBA(0x40,0x40,0x40,0x60));
        nvgStrokeWidth(vg, 0.5f);
        for (int s = 0; s <= NS; ++s) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, gridBox.pos.x + s*cw, gridBox.pos.y);
            nvgLineTo(vg, gridBox.pos.x + s*cw, gridBox.pos.y + gridBox.size.y);
            nvgStroke(vg);
        }
        for (int v = 0; v <= NV; ++v) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, gridBox.pos.x, gridBox.pos.y + v*ch);
            nvgLineTo(vg, gridBox.pos.x + gridBox.size.x, gridBox.pos.y + v*ch);
            nvgStroke(vg);
        }

        // --- membership cells: filled = in (voiceColour), hollow = out ---
        for (int s = 0; s < NS; ++s) {
            for (int v = 0; v < NV; ++v) {
                float cx = gridBox.pos.x + s*cw + 1;
                float cy = gridBox.pos.y + v*ch + 1;
                float cwid = cw - 2;
                float cht = ch - 2;
                if (module->getCell(s, v)) {
                    NVGcolor col = voiceColour(v);
                    nvgBeginPath(vg);
                    nvgRect(vg, cx, cy, cwid, cht);
                    nvgFillColor(vg, col);
                    nvgFill(vg);
                    // Output override: draw the output channel number (0-7) in the corner.
                    int outCh = module->getOutput(s, v);
                    if (outCh >= 0) {
                        nvgFontSize(vg, 6.f);
                        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                        nvgFillColor(vg, nvgRGBA(0xff,0xff,0xff,0xe0));
                        char buf[4]; snprintf(buf, sizeof(buf), "%d", outCh);
                        nvgText(vg, cx + 1.f, cy + 0.5f, buf, nullptr);
                    }
                } else {
                    // Hollow outline in faint voiceColour
                    NVGcolor col = voiceColour(v);
                    col.a = 0.25f;
                    nvgBeginPath(vg);
                    nvgRect(vg, cx, cy, cwid, cht);
                    nvgStrokeColor(vg, col);
                    nvgStrokeWidth(vg, 0.8f);
                    nvgStroke(vg);
                }
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

        // --- repeat row: N lit sub-segments per scene + current-repeat emphasis ---
        {
            const float rcw = repBox.size.x / NS;
            const float rsw = rcw / Intertropical::Ids::MAX_REPEAT;  // sub-segment width
            for (int s = 0; s < NS; ++s) {
                int rep = module->getRepeats(s);
                bool isActive = (s == module->activeScene);
                for (int r = 0; r < Intertropical::Ids::MAX_REPEAT; ++r) {
                    float sx = repBox.pos.x + s*rcw + r*rsw + 0.5f;
                    float sy = repBox.pos.y + 0.5f;
                    float sw = rsw - 1.f;
                    float sh = repBox.size.y - 1.f;
                    if (r < rep) {
                        // Lit segment
                        NVGcolor col = voiceColour(s % 8);
                        if (isActive && r == module->repeatPos) {
                            // Current repeat in active scene: brighter
                            col.a = 1.0f;
                        } else if (isActive) {
                            col.a = 0.7f;  // done repeats in active scene
                        } else {
                            col.a = 0.4f;  // non-active scene repeats
                        }
                        nvgBeginPath(vg);
                        nvgRect(vg, sx, sy, sw, sh);
                        nvgFillColor(vg, col);
                        nvgFill(vg);
                    } else {
                        // Unlit segment
                        nvgBeginPath(vg);
                        nvgRect(vg, sx, sy, sw, sh);
                        nvgFillColor(vg, nvgRGBA(0x30,0x30,0x30,0x50));
                        nvgFill(vg);
                    }
                }
                // Scene boundary line
                nvgBeginPath(vg);
                nvgMoveTo(vg, repBox.pos.x + (s+1)*rcw, repBox.pos.y);
                nvgLineTo(vg, repBox.pos.x + (s+1)*rcw, repBox.pos.y + repBox.size.y);
                nvgStrokeColor(vg, nvgRGBA(0x50,0x50,0x50,0x80));
                nvgStrokeWidth(vg, 0.8f);
                nvgStroke(vg);
            }
        }

        // --- voice numbers 1..16 in the left gutter ---
        nvgFillColor(vg, nvgRGBA(0x80,0x80,0x80,0xff));
        nvgFontSize(vg, 7.f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgTextLetterSpacing(vg, 0.f);
        for (int v = 0; v < NV; ++v) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", v + 1);
            nvgText(vg, gridBox.pos.x - 2.f, gridBox.pos.y + v*ch + ch/2, buf, nullptr);
        }
    }
};

struct IntertropicalWidget : ModuleWidget {
    IntertropicalWidget(Intertropical* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/Intertropical_panel_dark.svg"),
            asset::plugin(pluginInstance, "res/panels/Intertropical_panel_light.svg")));

        // Grid widget — covers the repeat row + main grid area
        auto* grid = new IntertropicalGrid;
        grid->module = module;
        grid->gridBox = Rect(mm2px(Vec(IT_GRID_X, IT_GRID_Y)),
                             mm2px(Vec(IT_GRID_W, IT_GRID_H)));
        grid->repBox  = Rect(mm2px(Vec(IT_GRID_X, IT_REP_Y)),
                             mm2px(Vec(IT_GRID_W, IT_REP_H)));
        // The grid widget's own box covers both areas
        grid->box = Rect(mm2px(Vec(IT_GRID_X - 6, IT_REP_Y - 2)),
                         mm2px(Vec(IT_GRID_W + 8, IT_GRID_H + IT_REP_H + 6)));
        addChild(grid);

        // 5 poly output jacks
        const char* jackLabels[5] = {"GATE", "CV", "ACC", "LEG", "SLEG"};
        for (int o = 0; o < Intertropical::Ids::NUM_OUTPUTS; ++o) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(IT_JACK_X[o], IT_JACK_Y)), module, o));
        }

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    }

    void appendContextMenu(Menu* menu) override {
        auto* m = dynamic_cast<Intertropical*>(module);
        if (!m) return;
        menu->addChild(new MenuSeparator);
        auto* label = new MenuLabel;
        label->text = "Scenes (loop length)";
        menu->addChild(label);
        for (int n = 1; n <= Intertropical::Ids::N_SCENES; ++n) {
            auto* item = new MenuItem;
            item->text = std::to_string(n) + (n == 1 ? " scene" : " scenes");
            item->rightText = (m->getLoopLen() == n) ? "\xe2\x9c\x93" : "";
            struct SetLoop : MenuItem {
                Intertropical* m; int n;
                void onAction(const event::Action& e) override { m->setLoopLen(n); }
            };
            auto* sl = new SetLoop;
            sl->text = item->text;
            sl->rightText = item->rightText;
            sl->m = m; sl->n = n;
            menu->addChild(sl);
            delete item;  // replaced by sl
        }
    }
};

Model* modelIntertropical = createModel<Intertropical, IntertropicalWidget>("Intertropical");
