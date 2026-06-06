#include <rack.hpp>
#include "MonsoonSurgeExpander.hpp"
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"

using namespace rack;

// Geometry MUST match panel_src/gen_surge.py. File-scope (C++11 ODR-safe).
static const float SG_ROWS[5]  = {60.0f, 74.0f, 88.0f, 102.0f, 116.0f};
static const float SG_JACK_X   = 14.0f;
static const float SG_ATT_X    = 30.0f;

struct MonsoonSurgeExpanderWidget : ModuleWidget {
    MonsoonSurgeExpanderWidget(MonsoonSurgeExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Surge_panel_dark.svg")));
        redDot::addRedScrews(this);

        const int cvId[5]  = { MonsoonIds::SURGE_NOTEVAL_CV, MonsoonIds::SURGE_VARIATION_CV,
            MonsoonIds::SURGE_LEGATO_CV, MonsoonIds::SURGE_REST_CV, MonsoonIds::SURGE_ACCENT_CV };
        const int attId[5] = { MonsoonIds::SURGE_NOTEVAL_ATT, MonsoonIds::SURGE_VARIATION_ATT,
            MonsoonIds::SURGE_LEGATO_ATT, MonsoonIds::SURGE_REST_ATT, MonsoonIds::SURGE_ACCENT_ATT };
        for (int r = 0; r < 5; ++r) {
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(SG_JACK_X, SG_ROWS[r])), module, cvId[r]));
            addParam(createParamCentered<Trimpot>(   mm2px(Vec(SG_ATT_X,  SG_ROWS[r])), module, attId[r]));
        }
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        NVGcontext* vg = args.vg;
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(vg, font->handle);
        nvgFillColor(vg, nvgRGB(0xc8, 0xcc, 0xd4));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        auto T = [&](float xmm, float ymm, float sz, const char* s){
            nvgFontSize(vg, sz); nvgText(vg, mm2px(xmm), mm2px(ymm), s, nullptr);
        };
        T(20.f, 50.f, 8.f, "SURGE");
        const char* lbl[5] = {"NOTE","VAR","LEG","REST","ACC"};
        for (int r = 0; r < 5; ++r) T((SG_JACK_X+SG_ATT_X)/2.f, SG_ROWS[r]-6.f, 6.f, lbl[r]);
    }
};

Model* modelMonsoonSurgeExpander =
    createModel<MonsoonSurgeExpander, MonsoonSurgeExpanderWidget>("MonsoonSurgeExpander");
