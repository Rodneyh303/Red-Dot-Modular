#include <rack.hpp>
#include "MonsoonCausewayExpander.hpp"
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "gen/CausewayLayout.gen.hpp"

using namespace rack;

extern Model* modelMonsoon;

// Control positions now come from the generated header (single source of truth:
// panel_src/layouts/causeway.json). Wrap each constexpr mm coord with mm2px here.
static inline Vec L(const CausewayLayout::P& p) { return mm2px(Vec(p.x, p.y)); }

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
        addInput(createInputCentered<PJ301MPort>(L(CausewayLayout::CAUSEWAY_SLEW_R_CV),  module, MonsoonIds::CAUSEWAY_SLEW_R_CV));
        addParam(createParamCentered<Trimpot>(  L(CausewayLayout::CAUSEWAY_SLEW_R_ATT), module, MonsoonIds::CAUSEWAY_SLEW_R_ATT));
        addParam(createParamCentered<Trimpot>(  L(CausewayLayout::CAUSEWAY_SLEW_M_ATT), module, MonsoonIds::CAUSEWAY_SLEW_M_ATT));
        addInput(createInputCentered<PJ301MPort>(L(CausewayLayout::CAUSEWAY_SLEW_M_CV),  module, MonsoonIds::CAUSEWAY_SLEW_M_CV));
        // Row 1 (mix)
        addInput(createInputCentered<PJ301MPort>(L(CausewayLayout::CAUSEWAY_MIX_R_CV),  module, MonsoonIds::CAUSEWAY_MIX_R_CV));
        addParam(createParamCentered<Trimpot>(  L(CausewayLayout::CAUSEWAY_MIX_R_ATT), module, MonsoonIds::CAUSEWAY_MIX_R_ATT));
        addParam(createParamCentered<Trimpot>(  L(CausewayLayout::CAUSEWAY_MIX_M_ATT), module, MonsoonIds::CAUSEWAY_MIX_M_ATT));
        addInput(createInputCentered<PJ301MPort>(L(CausewayLayout::CAUSEWAY_MIX_M_CV),  module, MonsoonIds::CAUSEWAY_MIX_M_CV));

        // Gate section: rhythm left, melody right; 5 rows. Positions from header.
        const CausewayLayout::P gpos[5][2] = {
            { CausewayLayout::CAUSEWAY_GATE_TRIAL_R,      CausewayLayout::CAUSEWAY_GATE_TRIAL_M },
            { CausewayLayout::CAUSEWAY_GATE_REDICE_R,     CausewayLayout::CAUSEWAY_GATE_REDICE_M },
            { CausewayLayout::CAUSEWAY_GATE_LIVESRC_R,    CausewayLayout::CAUSEWAY_GATE_LIVESRC_M },
            { CausewayLayout::CAUSEWAY_GATE_LIVESTATIC_R, CausewayLayout::CAUSEWAY_GATE_LIVESTATIC_M },
            { CausewayLayout::CAUSEWAY_GATE_RESEED_ROLL,  CausewayLayout::CAUSEWAY_GATE_RESEED_RESTART },
        };
        const int gateL[5] = { MonsoonIds::CAUSEWAY_GATE_TRIAL_R, MonsoonIds::CAUSEWAY_GATE_REDICE_R,
            MonsoonIds::CAUSEWAY_GATE_LIVESRC_R, MonsoonIds::CAUSEWAY_GATE_LIVESTATIC_R, MonsoonIds::CAUSEWAY_GATE_RESEED_ROLL };
        const int gateR[5] = { MonsoonIds::CAUSEWAY_GATE_TRIAL_M, MonsoonIds::CAUSEWAY_GATE_REDICE_M,
            MonsoonIds::CAUSEWAY_GATE_LIVESRC_M, MonsoonIds::CAUSEWAY_GATE_LIVESTATIC_M, MonsoonIds::CAUSEWAY_GATE_RESEED_RESTART };
        for (int r = 0; r < 5; ++r) {
            addInput(createInputCentered<PJ301MPort>(L(gpos[r][0]), module, gateL[r]));
            addInput(createInputCentered<PJ301MPort>(L(gpos[r][1]), module, gateR[r]));
        }
    }

    void step() override {
        ModuleWidget::step();
        if (!module) return;
        Monsoon* m = redDot::findMonsoonEitherSide(module);
        int wantLight = (m && m->lightTheme) ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            if (panelWidget) panelWidget->setBackground(wantLight ? panelSvgLight : panelSvgDark);
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
        // CV row labels — derived from the generated control coords.
        const float cvMid = (CausewayLayout::CAUSEWAY_SLEW_R_ATT.x + CausewayLayout::CAUSEWAY_SLEW_M_ATT.x) / 2.f;
        T(cvMid, CausewayLayout::CAUSEWAY_SLEW_R_CV.y - 6.f, 7.f, "SLEW");
        T(cvMid, CausewayLayout::CAUSEWAY_MIX_R_CV.y - 6.f, 7.f, "MIX");
        T(CausewayLayout::CAUSEWAY_SLEW_R_CV.x, CausewayLayout::CAUSEWAY_SLEW_R_CV.y + 6.f, 6.f, "R");
        T(CausewayLayout::CAUSEWAY_SLEW_M_CV.x, CausewayLayout::CAUSEWAY_SLEW_M_CV.y + 6.f, 6.f, "M");
        // Gate row labels (centred between the two gate columns)
        const char* gl[5] = {"TRIAL","REDICE","LIVE SRC","LIVE/STAT","RESEED"};
        const float gateY[5] = {
            CausewayLayout::CAUSEWAY_GATE_TRIAL_R.y, CausewayLayout::CAUSEWAY_GATE_REDICE_R.y,
            CausewayLayout::CAUSEWAY_GATE_LIVESRC_R.y, CausewayLayout::CAUSEWAY_GATE_LIVESTATIC_R.y,
            CausewayLayout::CAUSEWAY_GATE_RESEED_ROLL.y };
        for (int r = 0; r < 5; ++r) T(30.f, gateY[r], 6.f, gl[r]);
    }
};

Model* modelMonsoonCausewayExpander =
    createModel<MonsoonCausewayExpander, MonsoonCausewayExpanderWidget>("MonsoonCausewayExpander");
