#include <rack.hpp>
#include "MonsoonSurgeExpander.hpp"
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"

using namespace rack;

// Label-row geometry for draw() (control placement now comes from the SVG kit).
static const float SG_ROWS[5]  = {60.0f, 74.0f, 88.0f, 102.0f, 116.0f};
static const float SG_JACK_X   = 14.0f;
static const float SG_ATT_X    = 30.0f;

struct MonsoonSurgeExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonSurgeExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;
    redDot::ConnectMark* connectMark = nullptr;

    MonsoonSurgeExpanderWidget(MonsoonSurgeExpander* module) {
        setModule(module);
        const char* darkPath  = "res/panels/Junction_panel_dark.svg";
        const char* lightPath = "res/panels/Junction_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));
        redDot::addRedScrews(this);

        // Controls bound by id from the kit (#components). gen_junction.py emits
        // input_SURGE_<NAME>_CV / param_SURGE_<NAME>_ATT for the 5 rows.
        const char* nm[5] = {"NOTEVAL","VARIATION","LEGATO","REST","ACCENT"};
        const int cvId[5]  = { MonsoonIds::SURGE_NOTEVAL_CV, MonsoonIds::SURGE_VARIATION_CV,
            MonsoonIds::SURGE_LEGATO_CV, MonsoonIds::SURGE_REST_CV, MonsoonIds::SURGE_ACCENT_CV };
        const int attId[5] = { MonsoonIds::SURGE_NOTEVAL_ATT, MonsoonIds::SURGE_VARIATION_ATT,
            MonsoonIds::SURGE_LEGATO_ATT, MonsoonIds::SURGE_REST_ATT, MonsoonIds::SURGE_ACCENT_ATT };
        for (int r = 0; r < 5; ++r) {
            bindInput<PJ301MPort>(std::string("input_SURGE_") + nm[r] + "_CV",  cvId[r]);
            bindParam<Trimpot>   (std::string("param_SURGE_") + nm[r] + "_ATT", attId[r]);
        }

        // dot.modular connect mark — full brand colour when attached to a Monsoon
        // core, greyed when not. Placed at the SVG marker id="light_connect"
        // (repositionable in the generator) via the kit's bindChild.
        connectMark = bindChild<redDot::ConnectMark>("light_connect",
            std::function<void(redDot::ConnectMark*)>([this](redDot::ConnectMark* w){
                w->box.size = mm2px(rack::math::Vec(8.f, 8.f));
                w->box.pos  = w->box.pos.minus(w->box.size.div(2));  // re-centre after sizing
                w->connected  = [this]() { return module && redDot::findMonsoonEitherSide(module) != nullptr; };
                w->lightTheme = [this]() { Monsoon* mm = module ? redDot::findMonsoonEitherSide(module) : nullptr; return mm && mm->lightTheme; };
            }));
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
        NVGcontext* vg = args.vg;
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(vg, font->handle);
        nvgFillColor(vg, nvgRGB(0xc8, 0xcc, 0xd4));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        auto T = [&](float xmm, float ymm, float sz, const char* s){
            nvgFontSize(vg, sz); nvgText(vg, mm2px(xmm), mm2px(ymm), s, nullptr);
        };
        T(20.f, 50.f, 8.f, "JUNCTION");
        const char* lbl[5] = {"NOTE","VAR","LEG","REST","ACC"};
        for (int r = 0; r < 5; ++r) T((SG_JACK_X+SG_ATT_X)/2.f, SG_ROWS[r]-6.f, 6.f, lbl[r]);
    }
};

Model* modelMonsoonSurgeExpander =
    createModel<MonsoonSurgeExpander, MonsoonSurgeExpanderWidget>("Junction");
