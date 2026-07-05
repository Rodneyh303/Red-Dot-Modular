#include <rack.hpp>
#include <algorithm>
#include <cmath>
#include "Monsoon.hpp"
#include "MonsoonShophouseExpander.hpp"
#include "dsp/managers/MonsoonScaleManager.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
using namespace ShophouseIds;

// A front's 12 clickable shutters. Renders the live scale (lit=in-scale teal,
// dim=out-of-scale, root=Singapore red) from that front's (scale,root) params, and
// on click of a shutter sets that front's ROOT to the clicked semitone — the display
// IS the control (no menu). Scale itself is the per-front scale knob.
struct ShutterBank : Widget {
    MonsoonShophouseExpander* module = nullptr;
    int front = 0;
    Vec centres[12];              // shutter click centres (px), filled from panel markers
    Rect rects[12];               // full shutter rectangles (bank-local px) — for whole-shutter fill
    float clickR = 0.f;           // hit radius (px)
    std::shared_ptr<window::Font> font;   // (unused now; kept for ABI simplicity)

    static uint16_t maskFor(int scaleIdx, int root) {
        return ScaleManager::calculateMask(root, scaleIdx);
    }
    static const char* rootName(int r) {
        static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return N[((r % 12) + 12) % 12];
    }

    void onButton(const event::Button& e) override {
        if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT) {
            Widget::onButton(e); return;
        }
        // Nearest shutter within radius → set root.
        int best = -1; float bestD = clickR * clickR;
        for (int s = 0; s < 12; ++s) {
            float dx = e.pos.x - centres[s].x, dy = e.pos.y - centres[s].y;
            float d = dx*dx + dy*dy;
            if (d < bestD) { bestD = d; best = s; }
        }
        if (best >= 0) {
            module->params[ROOT_PARAM_0 + front].setValue((float)best);
            e.consume(this);
            return;
        }
        Widget::onButton(e);
    }

    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        if (!module) return;
        NVGcontext* vg = args.vg;
        int scaleIdx = (int)std::round(module->params[SCALE_PARAM_0 + front].getValue());
        int root     = (int)std::round(module->params[ROOT_PARAM_0 + front].getValue());
        uint16_t mask = maskFor(scaleIdx, root);
        for (int s = 0; s < 12; ++s) {
            const Rect& rc = rects[s];
            if (rc.size.x <= 0.f) continue;
            bool in = (mask >> s) & 1u;
            bool isRoot = (s == root);
            NVGcolor fill, slat;
            if (isRoot && in) {                 // root = Singapore red, "open" shutter
                fill = nvgRGB(0xd4, 0x00, 0x1a); slat = nvgRGBA(0x00, 0x00, 0x00, 0x55);
            } else if (in) {                    // in-scale = teal, open shutter
                fill = nvgRGB(0x26, 0xa6, 0x9a); slat = nvgRGBA(0x00, 0x00, 0x00, 0x4d);
            } else {                            // out-of-scale = dark, closed shutter
                fill = nvgRGB(0x1b, 0x20, 0x26); slat = nvgRGBA(0xff, 0xff, 0xff, 0x14);
            }
            // Whole-shutter fill.
            nvgBeginPath(vg);
            nvgRoundedRect(vg, rc.pos.x, rc.pos.y, rc.size.x, rc.size.y, 1.f);
            nvgFillColor(vg, fill);
            nvgFill(vg);
            // Louvre slats — horizontal lines across the shutter, so it reads as a slatted panel.
            int nslat = std::max(3, (int)(rc.size.y / 3.0f));
            float step = rc.size.y / (nslat + 1);
            nvgStrokeColor(vg, slat);
            nvgStrokeWidth(vg, std::max(0.6f, step * 0.28f));
            float inset = rc.size.x * 0.12f;
            for (int k = 1; k <= nslat; ++k) {
                float y = rc.pos.y + k * step;
                nvgBeginPath(vg);
                nvgMoveTo(vg, rc.pos.x + inset, y);
                nvgLineTo(vg, rc.pos.x + rc.size.x - inset, y);
                nvgStroke(vg);
            }
            // Frame the shutter.
            nvgBeginPath(vg);
            nvgRoundedRect(vg, rc.pos.x, rc.pos.y, rc.size.x, rc.size.y, 1.f);
            nvgStrokeColor(vg, nvgRGBA(0x00, 0x00, 0x00, 0x66));
            nvgStrokeWidth(vg, 0.6f);
            nvgStroke(vg);
        }
    }
};

// Live scale-name readout for one front — draws "<root> <scale name>" on the name band,
// updating as that front's scale dial rotates / a shutter sets the root.
struct ScaleNameLabel : Widget {
    MonsoonShophouseExpander* module = nullptr;
    int front = 0;
    std::shared_ptr<window::Font> font;
    static const char* rootName(int r) {
        static const char* N[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        return N[((r % 12) + 12) % 12];
    }
    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        if (!module) return;
        // Load the font here with a fallback (matches Lantern) — loading in the ctor could return
        // null before the window font cache is ready, leaving the label silently blank.
        std::shared_ptr<window::Font> f =
            APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        if (!f) return;
        int scaleIdx = (int)std::round(module->params[SCALE_PARAM_0 + front].getValue());
        int root     = (int)std::round(module->params[ROOT_PARAM_0 + front].getValue());
        if (scaleIdx < 0 || scaleIdx >= (int)MONSOON_SCALES.size()) return;
        std::string label = std::string(rootName(root)) + "  " + MONSOON_SCALES[scaleIdx].name;
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, 10.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(0xe8, 0xe8, 0xec));
        nvgText(vg, box.size.x * 0.5f, box.size.y * 0.5f, label.c_str(), nullptr);
    }
};

struct MonsoonShophouseExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonShophouseExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonShophouseExpanderWidget(MonsoonShophouseExpander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Shophouse_panel_dark.svg";
        const char* lightPath = "res/panels/Shophouse_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Per-front scale knobs + shutter banks (click to set root + live display).
        auto uiFont = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        for (int f = 0; f < NUM_FRONTS; ++f) {
            bindParam<Trimpot>("param_scale_" + std::to_string(f), SCALE_PARAM_0 + f);

            // Live scale-name readout on the name band.
            if (auto* nb = findNamed("name_band_" + std::to_string(f))) {
                auto* lbl = new ScaleNameLabel();
                lbl->module = mod; lbl->front = f; lbl->font = uiFont;
                Vec c = centerOf(nb);
                lbl->box.size = Vec(mm2px(38.f), mm2px(4.f));
                lbl->box.pos  = c.minus(lbl->box.size.div(2.f));
                addChild(lbl);
            }

            auto* bank = new ShutterBank();
            bank->module = mod;
            bank->front = f;
            // Gather this front's 12 shutter marker centres; size the bank to span them.
            // Track the bounding box manually (min/max corners) — avoids relying on a specific
            // Rect helper signature.
            float minX = 0, minY = 0, maxX = 0, maxY = 0; bool init = false;
            for (int s = 0; s < 12; ++s) {
                if (auto* m = findNamed("shutter_" + std::to_string(f) + "_" + std::to_string(s))) {
                    Vec c = centerOf(m);
                    bank->centres[s] = c;
                    if (!init) { minX = maxX = c.x; minY = maxY = c.y; init = true; }
                    else {
                        minX = std::min(minX, c.x); maxX = std::max(maxX, c.x);
                        minY = std::min(minY, c.y); maxY = std::max(maxY, c.y);
                    }
                }
            }
            if (init) {
                // Inflate the bank box a bit so clicks near shutters register.
                float pad = mm2px(3.5f);
                bank->box.pos  = Vec(minX - pad, minY - pad);
                bank->box.size = Vec((maxX - minX) + 2*pad, (maxY - minY) + 2*pad);
                // Re-base shutter centres into bank-local coords.
                for (int s = 0; s < 12; ++s) bank->centres[s] = bank->centres[s].minus(bank->box.pos);
                bank->clickR = mm2px(3.0f);
                // Compute each shutter's full RECT (bank-local) mirroring the panel geometry, so the
                // widget can fill the WHOLE shutter (not a dot). White keys: full height; black keys:
                // ~0.62 width, ~0.55 height, top-aligned. Derived from the front band rect.
                {
                    static const int WHITE_ORDER[7] = {0,2,4,5,7,9,11};
                    static const int BLACK_AFTER_K[5] = {1,3,6,8,10};
                    static const int BLACK_AFTER_V[5] = {0,1,3,4,5};
                    const float W_MM = 20.0f * 5.08f;   // 20HP panel width in mm
                    float fy = mm2px(22.0f + f*(20.0f+2.2f));
                    float fx = mm2px(6.0f);
                    float fw = mm2px(W_MM - 12.0f);
                    float fh = mm2px(20.0f);
                    float padg = mm2px(2.0f);
                    float bx = fx + padg, by = fy + padg + mm2px(4.0f);
                    float bw = fw - 2*padg;
                    float bh = fh - padg - mm2px(6.0f);
                    float wgap = mm2px(0.6f);
                    float ww = (bw - 6*wgap) / 7.0f;
                    float wh = bh;
                    Vec bp = bank->box.pos;
                    for (int i = 0; i < 7; ++i) {
                        int semi = WHITE_ORDER[i];
                        float sx = bx + i*(ww+wgap);
                        bank->rects[semi] = Rect(Vec(sx, by).minus(bp), Vec(ww, wh));
                    }
                    float bwd = ww * 0.62f, bhh = wh * 0.55f;
                    for (int j = 0; j < 5; ++j) {
                        int semi = BLACK_AFTER_K[j], after = BLACK_AFTER_V[j];
                        float sx = bx + (after+1)*(ww+wgap) - wgap - bwd/2.f;
                        bank->rects[semi] = Rect(Vec(sx, by).minus(bp), Vec(bwd, bhh));
                    }
                }
                addChild(bank);
            }
        }

        bindParam<CKSS>("param_conservation", CONSERVATION_PARAM);
        bindInput<PJ301MPort>("input_indexcv", INDEX_CV_INPUT);
        bindParam<Trimpot>("param_indexcvatt", INDEX_CV_ATT_PARAM);

        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            addChild(connectMark);
        }
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* m = redDot::findMonsoonEitherSide(module);
        int wantLight = (m && m->lightTheme) ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }
    }
};

Model* modelMonsoonShophouseExpander =
    createModel<MonsoonShophouseExpander, MonsoonShophouseExpanderWidget>("Shophouse");
