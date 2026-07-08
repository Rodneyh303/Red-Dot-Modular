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
// Active-scale indicator: a lit lantern hanging over whichever front is the committed-active
// scale (list.active()). Positioned by the module widget over each front's lantern_<f> anchor;
// only the active front's lantern is lit (Singapore red), the rest hang dark. Resolves both
// house (left/right) and floor (upper/lower) since there's one per window.
struct LanternWidget : Widget {
    MonsoonShophouseExpander* module = nullptr;
    int front = 0;

    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        if (!module) return;
        NVGcontext* vg = args.vg;
        const bool lit = (module->list.active() == front);
        const float cx = box.size.x * 0.5f;
        const float topY = 0.f;
        const float bodyTop = box.size.y * 0.28f;
        const float bodyBot = box.size.y * 0.86f;
        const float halfW = box.size.x * 0.30f;

        // hanging cord from the arch down to the lantern cap
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx, topY);
        nvgLineTo(vg, cx, bodyTop);
        nvgStrokeColor(vg, nvgRGBA(0x30, 0x30, 0x30, 0xcc));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);

        // lantern body colours: lit = Singapore red glow, dark = muted slate
        NVGcolor body = lit ? nvgRGB(0xd4, 0x00, 0x1a) : nvgRGBA(0x3a, 0x3f, 0x46, 0xdd);
        NVGcolor trim = lit ? nvgRGB(0xf0, 0xc0, 0x40) : nvgRGBA(0x5a, 0x60, 0x68, 0xdd);

        // soft glow halo when lit
        if (lit) {
            nvgBeginPath(vg);
            nvgCircle(vg, cx, (bodyTop + bodyBot) * 0.5f, halfW * 2.1f);
            nvgFillColor(vg, nvgRGBA(0xd4, 0x00, 0x1a, 0x33));
            nvgFill(vg);
        }

        // top cap
        nvgBeginPath(vg);
        nvgRect(vg, cx - halfW * 0.7f, bodyTop - box.size.y * 0.05f, halfW * 1.4f, box.size.y * 0.06f);
        nvgFillColor(vg, trim);
        nvgFill(vg);

        // lantern body (rounded lozenge)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cx - halfW, bodyTop, halfW * 2.f, bodyBot - bodyTop, halfW * 0.5f);
        nvgFillColor(vg, body);
        nvgFill(vg);
        nvgStrokeColor(vg, trim);
        nvgStrokeWidth(vg, 0.8f);
        nvgStroke(vg);

        // horizontal ribs
        for (int r = 1; r <= 2; ++r) {
            float ry = bodyTop + (bodyBot - bodyTop) * r / 3.f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx - halfW * 0.9f, ry);
            nvgLineTo(vg, cx + halfW * 0.9f, ry);
            nvgStrokeColor(vg, nvgRGBA(0x00, 0x00, 0x00, lit ? 0x40 : 0x30));
            nvgStrokeWidth(vg, 0.6f);
            nvgStroke(vg);
        }

        // bottom finial
        nvgBeginPath(vg);
        nvgRect(vg, cx - halfW * 0.25f, bodyBot, halfW * 0.5f, box.size.y * 0.08f);
        nvgFillColor(vg, trim);
        nvgFill(vg);
    }
};


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
        // Live indication: the committed-ACTIVE front (the scale currently playing) draws at full
        // brightness; the other (staged) fronts are dimmed, so the live scale reads vividly against
        // the ones queued for modulation. Pairs with the lantern (which marks WHICH front is active).
        const bool active = (module->list.active() == front);
        const float dim = active ? 1.0f : 0.42f;
        auto scaleC = [dim](int r, int g, int b) {
            return nvgRGB((int)(r*dim), (int)(g*dim), (int)(b*dim));
        };
        for (int s = 0; s < 12; ++s) {
            const Rect& rc = rects[s];
            if (rc.size.x <= 0.f) continue;
            bool in = (mask >> s) & 1u;
            bool isRoot = (s == root);
            NVGcolor fill, slat;
            if (isRoot && in) {                 // root = Singapore red, "open" shutter
                fill = scaleC(0xd4, 0x00, 0x1a); slat = nvgRGBA(0x00, 0x00, 0x00, 0x55);
            } else if (in) {                    // in-scale = teal, open shutter
                fill = scaleC(0x26, 0xa6, 0x9a); slat = nvgRGBA(0x00, 0x00, 0x00, 0x4d);
            } else {                            // out-of-scale = dark, closed shutter
                fill = scaleC(0x1b, 0x20, 0x26); slat = nvgRGBA(0xff, 0xff, 0xff, 0x14);
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
            // Read each shutter's full RECT straight from its panel <rect> marker (boundsOf) — panel
            // art is the single source of geometry truth. Centre is the rect centre. No hardcoded
            // panel constants here, so the panel layout (HP, house arrangement) can change freely.
            Rect markRects[12];
            float minX = 0, minY = 0, maxX = 0, maxY = 0; bool init = false;
            for (int s = 0; s < 12; ++s) {
                if (auto* m = findNamed("shutter_" + std::to_string(f) + "_" + std::to_string(s))) {
                    Rect r = boundsOf(m);
                    markRects[s] = r;
                    Vec c = r.pos.plus(r.size.div(2.f));   // rect centre (explicit; avoids API assumptions)
                    bank->centres[s] = c;
                    float x0 = r.pos.x, y0 = r.pos.y, x1 = r.pos.x + r.size.x, y1 = r.pos.y + r.size.y;
                    if (!init) { minX = x0; minY = y0; maxX = x1; maxY = y1; init = true; }
                    else {
                        minX = std::min(minX, x0); maxX = std::max(maxX, x1);
                        minY = std::min(minY, y0); maxY = std::max(maxY, y1);
                    }
                }
            }
            if (init) {
                float pad = mm2px(3.5f);
                bank->box.pos  = Vec(minX - pad, minY - pad);
                bank->box.size = Vec((maxX - minX) + 2*pad, (maxY - minY) + 2*pad);
                Vec bp = bank->box.pos;
                // Re-base centres and rects into bank-local coords.
                for (int s = 0; s < 12; ++s) {
                    bank->centres[s] = bank->centres[s].minus(bp);
                    bank->rects[s]   = Rect(markRects[s].pos.minus(bp), markRects[s].size);
                }
                bank->clickR = mm2px(3.0f);
                addChild(bank);
            }

            // Active-scale lantern over this front's window (positioned on the lantern_<f> anchor).
            if (auto* lm = findNamed("lantern_" + std::to_string(f))) {
                auto* lantern = new LanternWidget();
                lantern->module = mod;
                lantern->front = f;
                lantern->box.size = Vec(mm2px(4.4f), mm2px(7.0f));
                Vec c = centerOf(lm);
                // hang the lantern from the anchor: anchor sits at the top of the lantern box
                lantern->box.pos = Vec(c.x - lantern->box.size.x * 0.5f, c.y - mm2px(0.5f));
                addChild(lantern);
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
