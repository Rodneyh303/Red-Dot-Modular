#include <rack.hpp>
#include "MonsoonRafflesExpander.hpp"
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "gen/RafflesLayout.gen.hpp"
#include "ui/SvgPanelKit.hpp"

using namespace rack;

extern Model* modelMonsoon;

// Control positions live in the SVG kit (#components markers, generated from the
// same panel_src/layouts/raffles.json). The RafflesLayout header is still used
// for the draw() label coordinates.
struct MonsoonRafflesExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonRafflesExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonRafflesExpanderWidget(MonsoonRafflesExpander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Raffles_panel_dark.svg";
        const char* lightPath = "res/panels/Raffles_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));
        redDot::addRedScrews(this);

        // All controls bound by id from the kit (#components). Marker ids are
        // "<kind>_<CONTROL_ID>" emitted by gen_raffles.py from raffles.json.
        bindInput<PJ301MPort>("input_RAFFLES_SLEW_R_CV",  MonsoonIds::RAFFLES_SLEW_R_CV);
        bindParam<Trimpot>   ("param_RAFFLES_SLEW_R_ATT", MonsoonIds::RAFFLES_SLEW_R_ATT);
        bindParam<Trimpot>   ("param_RAFFLES_SLEW_M_ATT", MonsoonIds::RAFFLES_SLEW_M_ATT);
        bindParam<Trimpot>   ("param_RAFFLES_SLEW_Q_ATT", MonsoonIds::RAFFLES_SLEW_Q_ATT);
        bindInput<PJ301MPort>("input_RAFFLES_SLEW_M_CV",  MonsoonIds::RAFFLES_SLEW_M_CV);
        bindInput<PJ301MPort>("input_RAFFLES_SLEW_Q_CV",  MonsoonIds::RAFFLES_SLEW_Q_CV);
        bindInput<PJ301MPort>("input_RAFFLES_MIX_R_CV",   MonsoonIds::RAFFLES_MIX_R_CV);
        bindParam<Trimpot>   ("param_RAFFLES_MIX_R_ATT",  MonsoonIds::RAFFLES_MIX_R_ATT);
        bindParam<Trimpot>   ("param_RAFFLES_MIX_M_ATT",  MonsoonIds::RAFFLES_MIX_M_ATT);
        bindParam<Trimpot>   ("param_RAFFLES_MIX_Q_ATT",  MonsoonIds::RAFFLES_MIX_Q_ATT);
        bindInput<PJ301MPort>("input_RAFFLES_MIX_M_CV",   MonsoonIds::RAFFLES_MIX_M_CV);
        bindInput<PJ301MPort>("input_RAFFLES_MIX_Q_CV",   MonsoonIds::RAFFLES_MIX_Q_CV);

        bindInput<PJ301MPort>("input_RAFFLES_GATE_REDICE_R",     MonsoonIds::RAFFLES_GATE_REDICE_R);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_REDICE_M",     MonsoonIds::RAFFLES_GATE_REDICE_M);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_REDICE_Q",     MonsoonIds::RAFFLES_GATE_REDICE_Q);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_LIVESTATIC_R", MonsoonIds::RAFFLES_GATE_LIVESTATIC_R);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_LIVESTATIC_M", MonsoonIds::RAFFLES_GATE_LIVESTATIC_M);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_LIVESTATIC_Q", MonsoonIds::RAFFLES_GATE_LIVESTATIC_Q);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_RESEED_ROLL",    MonsoonIds::RAFFLES_GATE_RESEED_ROLL);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_RESEED_RESTART", MonsoonIds::RAFFLES_GATE_RESEED_RESTART);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_LASTDICE_R",   MonsoonIds::RAFFLES_GATE_LASTDICE_R);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_LASTDICE_M",   MonsoonIds::RAFFLES_GATE_LASTDICE_M);
        bindInput<PJ301MPort>("input_RAFFLES_GATE_LASTDICE_Q",   MonsoonIds::RAFFLES_GATE_LASTDICE_Q);

        // dot.modular connect mark (brand mark; greyed when no Monsoon attached).
        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(mod, centerOf(s), mm2px(8.f));
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

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        // Runtime labels (nanosvg can't render SVG <text>).
        NVGcontext* vg = args.vg;
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(vg, font->handle);
        nvgFillColor(vg, nvgRGB(0xc8, 0xcc, 0xd4));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        auto T = [&](float xmm, float ymm, float sz, const char* s){
            nvgFontSize(vg, sz); nvgText(vg, mm2px(xmm), mm2px(ymm), s, nullptr);
        };
        // Section headers
        T(30.f, 30.f, 9.f, "RAFFLES");
        // CV row labels — derived from the generated control coords.
        const float cvMid = (RafflesLayout::RAFFLES_SLEW_R_ATT.x + RafflesLayout::RAFFLES_SLEW_M_ATT.x) / 2.f;
        T(cvMid, RafflesLayout::RAFFLES_SLEW_R_CV.y - 6.f, 7.f, "SLEW");
        T(cvMid, RafflesLayout::RAFFLES_MIX_R_CV.y - 6.f, 7.f, "MIX");
        T(RafflesLayout::RAFFLES_SLEW_R_CV.x, RafflesLayout::RAFFLES_SLEW_R_CV.y + 6.f, 6.f, "R");
        T(RafflesLayout::RAFFLES_SLEW_M_CV.x, RafflesLayout::RAFFLES_SLEW_M_CV.y + 6.f, 6.f, "M");
        // Gate row labels (centred): 3 rows — REDICE, LIVE/STAT, RESEED. Trial/LiveSrc rows removed;
        // each REDICE/LIVESTATIC row now spans R/M/Q (3 cols); RESEED is global (2 gates).
        const char* gl[3] = {"REDICE","LIVE/STAT","RESEED"};
        const float gateY[3] = {
            RafflesLayout::RAFFLES_GATE_REDICE_R.y, RafflesLayout::RAFFLES_GATE_LIVESTATIC_R.y,
            RafflesLayout::RAFFLES_GATE_RESEED_ROLL.y };
        for (int r = 0; r < 3; ++r) T(30.f, gateY[r], 6.f, gl[r]);
        // Paired Last-gate "L" markers (LastDice R/M/Q — LastTrial removed with Trial).
        T(RafflesLayout::RAFFLES_GATE_LASTDICE_R.x,  RafflesLayout::RAFFLES_GATE_LASTDICE_R.y  - 6.f, 4.0f, "L");
        T(RafflesLayout::RAFFLES_GATE_LASTDICE_M.x,  RafflesLayout::RAFFLES_GATE_LASTDICE_M.y  - 6.f, 4.0f, "L");
    }
};

Model* modelMonsoonRafflesExpander =
    createModel<MonsoonRafflesExpander, MonsoonRafflesExpanderWidget>("Raffles");
