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
    float clickR = 0.f;           // hit radius (px)

    static uint16_t maskFor(int scaleIdx, int root) {
        return ScaleManager::calculateMask(root, scaleIdx);
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
        float r = std::max(2.f, clickR * 0.7f);
        for (int s = 0; s < 12; ++s) {
            bool in = (mask >> s) & 1u;
            bool isRoot = (s == root);
            NVGcolor col;
            if (isRoot && in)      col = nvgRGB(0xd4, 0x00, 0x1a);   // root = Singapore red
            else if (in)           col = nvgRGB(0x26, 0xa6, 0x9a);   // in-scale = teal (open)
            else                   col = nvgRGBA(0x3a, 0x3f, 0x46, 0x80); // closed = dim
            nvgBeginPath(vg);
            nvgCircle(vg, centres[s].x, centres[s].y, r);
            nvgFillColor(vg, col);
            nvgFill(vg);
        }
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
        for (int f = 0; f < NUM_FRONTS; ++f) {
            bindParam<Trimpot>("param_scale_" + std::to_string(f), SCALE_PARAM_0 + f);

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
                addChild(bank);
            }
        }

        bindParam<CKSS>("param_conservation", CONSERVATION_PARAM);
        bindInput<PJ301MPort>("input_indexcv", INDEX_CV_INPUT);

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
