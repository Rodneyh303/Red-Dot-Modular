#include <rack.hpp>
#include "MonsoonInterchangeExpander.hpp"
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/VisualExpanderHelpers.hpp"

using namespace rack;

extern Model* modelMonsoon;

struct MonsoonInterchangeExpanderWidget : ModuleWidget {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

MonsoonInterchangeExpanderWidget(MonsoonInterchangeExpander* mod) {
    setModule(mod);
    //box.size = mm2px(Vec(270, 380));
    panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/interchange_gemini_new2.svg"));
    panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/interchange_gemini_light.svg"));
    panelWidget = createPanel(asset::plugin(pluginInstance, "res/panels/interchange_gemini_new2.svg"));
    setPanel(panelWidget);
    redDot::addRedScrews(this);

    // ... (Screws same as before) ...
    
// Semitone Loop (i=0 to 5)
for (int i = 0; i < 6; i++) {
    float rowY = 80.0f + (i * 40.0f); 
    addInput(createInputCentered<DarkPJ301MPort>(Vec(48.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_CV_INPUT_0 + i));
    addParam(createParamCentered<Trimpot>(Vec(102.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_ATTENUVERTER_0 + i));
    addParam(createParamCentered<Trimpot>(Vec(168.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_ATTENUVERTER_0 + 6 + i));
    addInput(createInputCentered<DarkPJ301MPort>(Vec(222.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_CV_INPUT_0 + 6 + i));
}

// Octave Section (Position 7, Y=320)
float octY = 320.0f;
addInput(createInputCentered<PJ301MPort>(Vec(48.0f, octY), module, MonsoonIds::EXPANDER_OCT_LO_CV_INPUT));
addParam(createParamCentered<Trimpot>(Vec(102.0f, octY), module, MonsoonIds::EXPANDER_OCT_LO_ATTENUVERTER));
addParam(createParamCentered<Trimpot>(Vec(168.0f, octY), module, MonsoonIds::EXPANDER_OCT_HI_ATTENUVERTER));
addInput(createInputCentered<PJ301MPort>(Vec(222.0f, octY), module, MonsoonIds::EXPANDER_OCT_HI_CV_INPUT));

    // dot.modular connect mark (brand mark; greyed when no Monsoon attached). This
    // panel is hand-placed in px (270x380), so position directly; footer-centre.
    {
        connectMark = redDot::makeConnectMark(module, rack::math::Vec(135.f, 360.f), 24.f);
        addChild(connectMark);
    }
}
    Monsoon* getMonsoon() {
        return module ? redDot::findMonsoonEitherSide(module) : nullptr;
    }
    bool hostLight() {
        Monsoon* m = getMonsoon();
        return m ? m->lightTheme : false;
    }

    void step() override {
        ModuleWidget::step();
        int wantLight = hostLight() ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            if (panelWidget) panelWidget->setBackground(wantLight ? panelSvgLight : panelSvgDark);
        }
    }

    void draw(const DrawArgs& args) override {
        const bool light = (lastThemeLight == 1);
        // Force a solid opaque background fill to prevent transparency.
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, light ? nvgRGBA(0xe8, 0xe8, 0xea, 255) : nvgRGBA(0x23, 0x23, 0x23, 255));
        nvgFill(args.vg);

        ModuleWidget::draw(args);

        // ── Panel Labels ─────────────────────────────────────────────────────
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, light ? nvgRGBA(0x2a, 0x2a, 0x2e, 0xff) : nvgRGBA(0xe6, 0xe6, 0xe6, 0xff));

        // Header
        nvgFontSize(args.vg, mm2px(3.5f));
        nvgText(args.vg, mm2px(20.0f), mm2px(10.0f), "EXPANDER CV", nullptr);

        // Semitone Labels — placed directly above each CV jack. Coordinates are in
        // pixels to match the createInputCentered positions above: left column
        // x=48, right column x=222, rows y = 80 + row*40.
        nvgFontSize(args.vg, mm2px(2.5f));
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const float semiRow0 = 80.0f;
        const float semiRowStep = 40.0f;
        const float labelOffset = 20.0f;   // px above jack centre
        for (int i = 0; i < 12; i++) {
            int col = i / 6;
            int row = i % 6;
            float jackX = (col == 0) ? 48.0f : 222.0f;
            float jackY = semiRow0 + (row * semiRowStep);
            nvgText(args.vg, jackX, jackY - labelOffset, noteNames[i], nullptr);
        }

        // Octave Labels — above the octave CV jacks (x=48 / x=222, y=320)
        float octY = 320.0f;
        nvgText(args.vg, 48.0f,  octY - labelOffset, "OCT LO", nullptr);
        nvgText(args.vg, 222.0f, octY - labelOffset, "OCT HI", nullptr);

        // Attenuation guides — "-" / "+" flanking each trimpot (x=102 and x=168)
        nvgFontSize(args.vg, mm2px(1.8f));
        nvgFillColor(args.vg, light ? nvgRGBA(0x88, 0x8d, 0x96, 0xff) : nvgRGBA(0x99, 0x99, 0x99, 0xff));
        for (float knobX : {102.0f, 168.0f}) {
            for (int row = 0; row < 6; row++) {
                float y = semiRow0 + (row * semiRowStep);
                nvgText(args.vg, knobX - 9.0f, y, "-", nullptr);
                nvgText(args.vg, knobX + 9.0f, y, "+", nullptr);
            }
            // Octave row guides
            nvgText(args.vg, knobX - 9.0f, octY, "-", nullptr);
            nvgText(args.vg, knobX + 9.0f, octY, "+", nullptr);
        }
    }
};

Model* modelMonsoonInterchangeExpander = createModel<MonsoonInterchangeExpander, MonsoonInterchangeExpanderWidget>("MonsoonInterchangeExpander");