#include <rack.hpp>
#include "MonsoonCausewayExpander.hpp"
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"

using namespace rack;

extern Model* modelMonsoon;

// Geometry MUST match panel_src/gen_causeway.py. File-scope to avoid C++11
// constexpr-static-member ODR linkage issues.
static const float CW_CV_COLS[4] = {9.0f, 21.0f, 39.0f, 51.0f};
static const float CW_CV_ROWS[2] = {38.0f, 50.0f};
static const float CW_G_COLS[2]  = {16.0f, 44.0f};
static const float CW_G_ROWS[5]  = {66.0f, 78.0f, 90.0f, 102.0f, 114.0f};

struct MonsoonCausewayExpanderWidget : ModuleWidget {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    int lastThemeLight = -1;

    MonsoonCausewayExpanderWidget(MonsoonCausewayExpander* module) {
        setModule(module);
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Causeway_panel_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Causeway_panel_light.svg"));
        panelWidget = createPanel(asset::plugin(pluginInstance, "res/panels/Causeway_panel_dark.svg"));
        setPanel(panelWidget);
        redDot::addRedScrews(this);

       // using M = MonsoonIds;
        // CV section: jack | atten | atten | jack, rows = slew, mix.
        // Row 0 (slew): R jack/att on cols 0/1, M att/jack on cols 2/3.
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CW_CV_COLS[0], CW_CV_ROWS[0])), module, MonsoonIds::CAUSEWAY_SLEW_R_CV));
        addParam(createParamCentered<Trimpot>(  mm2px(Vec(CW_CV_COLS[1], CW_CV_ROWS[0])), module, MonsoonIds::CAUSEWAY_SLEW_R_ATT));
        addParam(createParamCentered<Trimpot>(  mm2px(Vec(CW_CV_COLS[2], CW_CV_ROWS[0])), module, MonsoonIds::CAUSEWAY_SLEW_M_ATT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CW_CV_COLS[3], CW_CV_ROWS[0])), module, MonsoonIds::CAUSEWAY_SLEW_M_CV));
        // Row 1 (mix)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CW_CV_COLS[0], CW_CV_ROWS[1])), module, MonsoonIds::CAUSEWAY_MIX_R_CV));
        addParam(createParamCentered<Trimpot>(  mm2px(Vec(CW_CV_COLS[1], CW_CV_ROWS[1])), module, MonsoonIds::CAUSEWAY_MIX_R_ATT));
        addParam(createParamCentered<Trimpot>(  mm2px(Vec(CW_CV_COLS[2], CW_CV_ROWS[1])), module, MonsoonIds::CAUSEWAY_MIX_M_ATT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CW_CV_COLS[3], CW_CV_ROWS[1])), module, MonsoonIds::CAUSEWAY_MIX_M_CV));

        // Gate section: rhythm left (col 0), melody right (col 1); 5 rows.
        const int gateL[5] = { MonsoonIds::CAUSEWAY_GATE_TRIAL_R, MonsoonIds::CAUSEWAY_GATE_REDICE_R,
            MonsoonIds::CAUSEWAY_GATE_LIVESRC_R, MonsoonIds::CAUSEWAY_GATE_LIVESTATIC_R, MonsoonIds::CAUSEWAY_GATE_RESEED_ROLL };
        const int gateR[5] = { MonsoonIds::CAUSEWAY_GATE_TRIAL_M, MonsoonIds::CAUSEWAY_GATE_REDICE_M,
            MonsoonIds::CAUSEWAY_GATE_LIVESRC_M, MonsoonIds::CAUSEWAY_GATE_LIVESTATIC_M, MonsoonIds::CAUSEWAY_GATE_RESEED_RESTART };
        for (int r = 0; r < 5; ++r) {
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CW_G_COLS[0], CW_G_ROWS[r])), module, gateL[r]));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CW_G_COLS[1], CW_G_ROWS[r])), module, gateR[r]));
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
        T(30.f, 30.f, 9.f, "CAUSEWAY");
        // CV row labels (left of each pair)
        T((CW_CV_COLS[1]+CW_CV_COLS[2])/2.f, CW_CV_ROWS[0]-6.f, 7.f, "SLEW");
        T((CW_CV_COLS[1]+CW_CV_COLS[2])/2.f, CW_CV_ROWS[1]-6.f, 7.f, "MIX");
        T(CW_CV_COLS[0], CW_CV_ROWS[0]+6.f, 6.f, "R"); T(CW_CV_COLS[3], CW_CV_ROWS[0]+6.f, 6.f, "M");
        // Gate row labels (between the two columns)
        const char* gl[5] = {"TRIAL","REDICE","LIVE SRC","LIVE/STAT","RESEED"};
        for (int r = 0; r < 5; ++r) T(30.f, CW_G_ROWS[r], 6.f, gl[r]);
    }
};

Model* modelMonsoonCausewayExpander =
    createModel<MonsoonCausewayExpander, MonsoonCausewayExpanderWidget>("MonsoonCausewayExpander");
