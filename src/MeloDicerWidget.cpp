#include <rack.hpp>
#include <algorithm>
#include "MeloDicerWidget.hpp"
#include "MeloDicer.hpp"
#include "Scales.hpp"

using namespace rack;
using namespace MeloDicerIds;
// ── Custom Slider for Scale Locking ──────────────────────────────────────────
template <typename TLightBase = RedLight>
struct MeloDicerLightSlider : VCVLightSlider<TLightBase> {
    void draw(const widget::Widget::DrawArgs& args) override {
        auto* m = dynamic_cast<MeloDicer*>(this->module);
        bool dimmed = false;
        if (m) {
            if (this->paramId >= MeloDicerIds::SEMI0_PARAM && this->paramId < MeloDicerIds::SEMI0_PARAM + 12) {
                int sem = this->paramId - MeloDicerIds::SEMI0_PARAM;
                if (!(m->activeScaleMask & (1 << sem))) dimmed = true;
            }
        }
        if (dimmed) nvgGlobalAlpha(args.vg, 0.4f);
        VCVLightSlider<TLightBase>::draw(args);
        if (dimmed) nvgGlobalAlpha(args.vg, 1.0f);
    }
};

// ── Simple Befaco-inspired knobs ─────────────────────────────────────────────
RDM_KnobLarge::RDM_KnobLarge() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobLarge.svg")));
}
RDM_KnobMedium::RDM_KnobMedium() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobMedium.svg")));
}
RDM_KnobSmall::RDM_KnobSmall() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobSmall.svg")));
}
RDM_KnobDarkLarge::RDM_KnobDarkLarge() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobDark_Large.svg")));
}
RDM_KnobDarkMedium::RDM_KnobDarkMedium() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobDark_Medium.svg")));
}
RDM_KnobCreamLarge::RDM_KnobCreamLarge() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobCream_Large.svg")));
}
RDM_KnobCreamMedium::RDM_KnobCreamMedium() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobCream_Medium.svg")));
}

bool MeloDicerWidget::getLightTheme() const {
    auto* m = dynamic_cast<const MeloDicer*>(module);
    return m ? m->lightTheme : false;
}

void MeloDicerWidget::setLightTheme(bool v) {
    auto* m = dynamic_cast<MeloDicer*>(module);
    if (m) m->lightTheme = v;
}

MeloDicerWidget::MeloDicerWidget(MeloDicer* module) {
        setModule(module);
        applyTheme();
        if (box.size.x == 0) box.size = mm2px(Vec(W_MM, 128.5f)); // Fallback if SVG fails

        for (int i=0; i<12; ++i) {
            float cx=7.5f+i*7.0f;
            addParam(createLightParamCentered<MeloDicerLightSlider<GreenRedLight>>(
                mm2px(Vec(cx,59.75f)), module, SEMI0_PARAM+i, SEMI_LED_START+2*i));
        }

        addParam(createLightParamCentered<VCVLightSlider<RedLight>>(mm2px(Vec(100.f,59.75f)), module, MeloDicerIds::OCT_LO_PARAM, MeloDicerIds::OCT_LO_LED));
        addParam(createLightParamCentered<VCVLightSlider<RedLight>>(mm2px(Vec(108.f,59.75f)), module, MeloDicerIds::OCT_HI_PARAM, MeloDicerIds::OCT_HI_LED));

        // 16 step lights: Circular ring arrangement near top right
        {
            const float RCX = 138.f, RCY = 66.f, RLED = 13.25f;
            for (int i = 0; i < 16; ++i) {
                float ang = float(i) / 16.f * 2.f * M_PI - M_PI / 2.f;
                float lx = RCX + RLED * std::cos(ang), ly = RCY + RLED * std::sin(ang);
                if (i % 4 == 0) addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(lx, ly)), module, MeloDicerIds::STEP_LIGHTS_START + i));
                else            addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(lx, ly)), module, MeloDicerIds::STEP_LIGHTS_START + i));
            }
        }

        {
            const float MX = 168.f, LX = 163.f, Y_START = 54.f, v_spacing = 6.5f;
            // Add a single physical button for cycling modes
            addParam(createParamCentered<TL1105>(mm2px(Vec(MX, 44.f)), module, MeloDicerIds::MODE_PARAM));
            for (int i = 0; i < 4; ++i) {
                // Place 4 lights vertically starting below the MODE button
                addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(LX, Y_START + i * v_spacing)), module, MeloDicerIds::MODE_A_LIGHT + i));
            }
        }

        const float JY=96.f, JYL=102.f,JX=100.f, JP=12.0f;
        addParam(createParamCentered<TL1105>(mm2px(Vec( JX,JY)),module,MeloDicerIds::DICE_R_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec( JX,JYL)),module,MeloDicerIds::RHYTHM_DICE_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( JX+JP,JY)),module,MeloDicerIds::DICE_M_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec( JX+JP,JYL)),module,MeloDicerIds::MELODY_DICE_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( JX+2*JP,JY)),module,MeloDicerIds::LOCK_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec( JX+2*JP,JYL)),module,MeloDicerIds::LOCK_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( JX+3*JP,JY)),module,MeloDicerIds::MUTE_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(  mm2px(Vec( JX+3*JP,JYL)),module,MeloDicerIds::MUTE_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( JX+4*JP,JY)),module,MeloDicerIds::RESET_BUTTON_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec( JX+4*JP,JYL)),module,MeloDicerIds::RESET_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( JX+5*JP,JY)),module,MeloDicerIds::RUN_GATE_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec( JX+5*JP,JYL)),module,MeloDicerIds::RUN_GATE_LIGHT));

        // addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 88.f,112.f)),module,MeloDicerIds::RUN_GATE_INPUT));
        // addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 98.f,112.f)),module,MeloDicerIds::RESET_TRIGGER_INPUT));
        // addInput(createInputCentered<PJ301MPort>(mm2px(Vec(108.f,112.f)),module,MeloDicerIds::SEED_INPUT));
        // addInput(createInputCentered<PJ301MPort>(mm2px(Vec(118.f,11.f)),module,MeloDicerIds::LENGTH_INPUT));
        // addInput(createInputCentered<PJ301MPort>(mm2px(Vec(128.f,112.f)),module,MeloDicerIds::OFFFSET_INPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 14.f,105.f)),module,MeloDicerIds::RUN_GATE_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 30.f,105.f)),module,MeloDicerIds::RESET_TRIGGER_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 46.f,105.f)),module,MeloDicerIds::SEED_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(62.f,105.f)),module,MeloDicerIds::LENGTH_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 78.f,105.f)),module,MeloDicerIds::OFFSET_INPUT));

        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 14.f,120.5f)),module,MeloDicerIds::CLK_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 30.f,120.5f)),module,MeloDicerIds::GATE1_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 46.f,120.5f)),module,MeloDicerIds::GATE2_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 62.f,120.5f)),module,MeloDicerIds::CV1_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 78.f,120.5f)),module,MeloDicerIds::CV2_INPUT));


        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec( 95.f,120.5f)),module,MeloDicerIds::SEED_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(111.f,120.5f)),module,MeloDicerIds::RUN_GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(127.f,120.5f)),module,MeloDicerIds::RESET_TRIGGER_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(143.f,120.5f)),module,MeloDicerIds::GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(159.f,120.5f)),module,MeloDicerIds::CV_OUTPUT));
    }

void MeloDicerWidget::applyTheme() {
 // Panel
        bool lightTheme = getLightTheme();  // read from module
        auto panelPath = asset::plugin(pluginInstance,
            lightTheme ? "res/panels/MeloDicer_panel_light.svg"
                       : "res/panels/MeloDicer_panel_v6.svg");
        setPanel(createPanel(panelPath));

        // Remove any existing knob params at the 7 top knob positions
        // so we can re-add with the correct type.
        // Rack allows re-adding after clearing; easiest is to replace
        // the ParamWidget at the known indices. We use the positions directly.
        // Since this is called once at construction (no existing widgets yet),
        // we just add fresh. On theme toggle, widgets are rebuilt via
        // removing children with the affected paramIds first.
        auto* mod = dynamic_cast<MeloDicer*>(module);
        // auto removeKnob = [&](int paramId) {
        //     for (auto it = children.begin(); it != children.end(); ) {
        //         auto* pw = dynamic_cast<ParamWidget*>(*it);
        //         if (pw && pw->paramId == paramId) {
        //             it = children.erase(it);
        //         } else { ++it; }
        //     }
        // };
        // Remove the 7 top knobs (they get re-added below with correct type)
        for (int pid : {(int)MeloDicerIds::NOTE_VALUE_PARAM,
                        (int)MeloDicerIds::VARIATION_PARAM,
                        (int)MeloDicerIds::LEGATO_PARAM,
                        (int)MeloDicerIds::REST_PARAM,
                        (int)MeloDicerIds::BPM_PARAM,
                        (int)MeloDicerIds::PATTERN_LENGTH_PARAM,
                        (int)MeloDicerIds::PATTERN_OFFSET_PARAM}) {
            ParamWidget* pw = getParam(pid);
            if (pw) {
                removeChild(pw);
                delete pw;
            }
        }

        if (lightTheme) {
            // Light theme: Use dark knobs
            addParam(createParamCentered<RDM_KnobDarkLarge> (mm2px(Vec(14.f,24.f)),mod,MeloDicerIds::NOTE_VALUE_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(37.f,24.f)),mod,MeloDicerIds::VARIATION_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(60.f,24.f)),mod,MeloDicerIds::LEGATO_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(83.f,24.f)),mod,MeloDicerIds::REST_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>     (mm2px(Vec(110.f,24.f)),mod,MeloDicerIds::BPM_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>     (mm2px(Vec(133.f,24.f)),mod,MeloDicerIds::PATTERN_LENGTH_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>     (mm2px(Vec(156.f,24.f)),mod,MeloDicerIds::PATTERN_OFFSET_PARAM));
        } else {
            // Dark theme: Use cream/light knobs
            addParam(createParamCentered<RDM_KnobCreamMedium> (mm2px(Vec(14.f,24.f)),mod,MeloDicerIds::NOTE_VALUE_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(37.f,24.f)),mod,MeloDicerIds::VARIATION_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(60.f,24.f)),mod,MeloDicerIds::LEGATO_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(83.f,24.f)),mod,MeloDicerIds::REST_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>      (mm2px(Vec(110.f,24.f)),mod,MeloDicerIds::BPM_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>      (mm2px(Vec(133.f,24.f)),mod,MeloDicerIds::PATTERN_LENGTH_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>      (mm2px(Vec(156.f,24.f)),mod,MeloDicerIds::PATTERN_OFFSET_PARAM));
        }
    }

void MeloDicerWidget::draw(const DrawArgs& args) {
        NVGcontext* vg=args.vg;

        // Force a solid opaque background fill (alpha 255) to prevent transparency
    // Draw solid opaque background to prevent transparency
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, getLightTheme() ? nvgRGBA(0xe6, 0xe6, 0xe6, 255) : nvgRGBA(0x23, 0x23, 0x23, 255));
    nvgFill(vg);

        ModuleWidget::draw(args);

        nvgFontFaceId(vg,APP->window->uiFont->handle);
        nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
        const bool lt = getLightTheme();
        auto fillNvgColour=[&](int r,int g,int b,int a=255){
            if (lt) {
                auto inv=[](int v){ return std::max(0, 220-v); };
                bool isGrey = (std::abs(r-g)<20 && std::abs(g-b)<20);
                if (isGrey) { r=inv(r); g=inv(g); b=inv(b); }
                else { r=std::max(0,r*7/10); g=std::max(0,g*7/10); b=std::max(0,b*7/10); }
            }
            nvgFillColor(vg,nvgRGBA(r,g,b,a));
        };
        auto writeNvgText=[&](float x,float y,const char* t){ nvgText(vg,mm2px(x),mm2px(y),t,nullptr); };
        auto setNvgFontSize=[&](float mm){ nvgFontSize(vg,mm2px(mm)); };

        setNvgFontSize(4.2f); fillNvgColour(200,200,200); writeNvgText(W_MM/2.f,5.5f,"Red Dot Modular  -  MeloDicer");
        setNvgFontSize(3.4f); fillNvgColour(200,200,200); writeNvgText(14,37.5f,"NOTE VALUE"); writeNvgText(37,37.5f,"VARIATION"); writeNvgText(60,37.5f,"LEGATO"); writeNvgText(83,37.5f,"REST");

        auto arcLabel = [&](float cx_mm, float cy_mm, float r_mm, float angle_deg, const char* text, int ri=160, int gi=160, int bi=160) {
            float a  = angle_deg * float(M_PI) / 180.f;
            float tx = cx_mm + r_mm * std::cos(a);
            float ty = cy_mm + r_mm * std::sin(a);
            nvgSave(vg);
            nvgTranslate(vg, mm2px(tx), mm2px(ty));
            nvgRotate(vg, a + float(M_PI)/2.f);
            if (lt) {
                auto inv=[](int v){ return std::max(0, 220-v); };
                bool isGrey=(std::abs(ri-gi)<20 && std::abs(gi-bi)<20);
                if(isGrey){ri=inv(ri);gi=inv(gi);bi=inv(bi);}
                else{ri=ri*7/10;gi=gi*7/10;bi=bi*7/10;}
            }
            nvgFillColor(vg, nvgRGBA(ri,gi,bi,200));
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, 0, 0, text, nullptr);
            nvgRestore(vg);
        };

        setNvgFontSize(2.5f);
        {
            const char* noteLabels[8] = {"1/2","1/4","1/4T","1/8","1/8T","1/16","1/32T","1/32"};
            for (int i=0;i<8;++i) {
                float deg = -225.f + i * (270.f/7.f);
                arcLabel(14.f, 26.f, 12.5f, deg, noteLabels[i], 150,150,135);
            }
        }
        setNvgFontSize(2.8f);
        arcLabel(37.f, 26.f, 12.5f, -225.f, "LONGER",  130,130,120); arcLabel(37.f, 26.f, 12.5f,  +45.f, "SHORTER", 130,130,120);
        arcLabel(60.f, 26.f, 12.0f, -225.f, "0%",   130,130,120); arcLabel(60.f, 26.f, 12.0f,  +45.f, "100%", 130,130,120);
        arcLabel(83.f, 26.f, 12.0f, -225.f, "0%",   130,130,120); arcLabel(83.f, 26.f, 12.0f,  +45.f, "100%", 130,130,120);
        setNvgFontSize(3.2f); fillNvgColour(170,170,170); writeNvgText(110,37.5f,"BPM"); writeNvgText(133,37.5f,"LEN"); writeNvgText(156,37.5f,"OFFSET");

        setNvgFontSize(2.5f); fillNvgColour(80,80,80); nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
        //nvgText(vg,mm2px(4.5f),mm2px(43.f),"SEMITONE PROBABILITY",nullptr); nvgText(vg,mm2px(94.f),mm2px(43.f),"OCT",nullptr);
        nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);

        setNvgFontSize(3.0f); const char* sn[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for (int i=0;i<12;++i){ fillNvgColour(200,200,200); writeNvgText(7.5f+i*7.f,43.f,sn[i]); }
        setNvgFontSize(2.7f); fillNvgColour(85,85,85); const char* nums[12]={"1","2","3","4","5","6","7","8","9","10","11","12"};
        for (int i=0;i<12;++i) writeNvgText(7.5f+i*7.f,SL_TOP+SLH+6.f,nums[i]);
        setNvgFontSize(2.9f); fillNvgColour(38,166,154); writeNvgText(100,43.f,"LO"); writeNvgText(108,43.f,"HI");

        // Mode Section: Label on top, Button below (at 34), then vertical A-D stack
        setNvgFontSize(3.2f); fillNvgColour(210,210,210); writeNvgText(168.f, 30.f, "MODE");
        setNvgFontSize(3.8f); 
        const float TX = 168.f, Y_START_VAL = 54.f, v_spacing_val = 6.5f;
        writeNvgText(TX, Y_START_VAL, "A"); 
        writeNvgText(TX, Y_START_VAL + v_spacing_val, "B"); 
        writeNvgText(TX, Y_START_VAL + 2 * v_spacing_val, "C"); 
        writeNvgText(TX, Y_START_VAL + 3 * v_spacing_val, "D");

        const float JY=109.f,JX=100.f, JP=12.0f;
        setNvgFontSize(2.9f); fillNvgColour(200,60,60);  writeNvgText(JX,JY,"DICE R"); writeNvgText(JX+JP,JY,"DICE M");
        fillNvgColour(190,190,190); writeNvgText(JX+2*JP,JY,"LOCK"); writeNvgText(JX+3*JP,JY,"MUTE"); writeNvgText(JX+4*JP,JY,"RESET"); writeNvgText(JX+5*JP,JY,"RUN");
        //L(JX+2*JP,JY,"RESET"); L(JX+5*JP,JY,"RUN");

        const float JY3=120.5f,PR=7.7f/2.f; setNvgFontSize(3.0f); fillNvgColour(195,195,195);
        const char* il[5]={"CLK","G1","G2","CV1","CV2"}; const float ix[5]={14,30,46,62,78};
        for(int i=0;i<5;++i) writeNvgText(ix[i],JY3-PR-2.2f,il[i]);
        const char* sl[5] = {"RUN", "RST", "SEED", "LEN", "OFF"};
        //const float sx[5] = {88.f, 98.f, 108.f, 118.f, 128.f};
        for(int i=0;i<5;++i) writeNvgText(ix[i],102.f-PR-1.8f,sl[i]);


        setNvgFontSize(2.7f); fillNvgColour(180,180,180); const char* ol[5]={"SEED","RUN","RST","GATE","CV"};
        const float ox[5]={95,111,127,143,159};
        for(int i=0;i<5;++i) writeNvgText(ox[i],JY3-PR-2.2f,ol[i]);
        setNvgFontSize(2.3f); fillNvgColour(80,80,80); nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
        // const char* sl[5] = {"RUN", "RST", "SEED", "LEN", "OFF"};
        // const float sx[5] = {88.f, 98.f, 108.f, 118.f, 128.f};
        // for(int i=0;i<5;++i) L(sx[i],116.f-PR-1.8f,sl[i]);
    }

void MeloDicerWidget::appendContextMenu(ui::Menu* menu) {
        auto* m = dynamic_cast<MeloDicer*>(module);
        if (!m) return;
        menu->addChild(new ui::MenuSeparator);
        struct ThemeItem : ui::MenuItem {
            MeloDicerWidget* widget = nullptr;
            void onAction(const event::Action&) override { widget->setLightTheme(!widget->getLightTheme()); widget->applyTheme(); }
            void step() override { rightText = widget->getLightTheme() ? "Light ✔" : "Dark ✔"; ui::MenuItem::step(); }
        };
        auto* themeItem = createMenuItem<ThemeItem>("Theme"); themeItem->widget = this; menu->addChild(themeItem);
        menu->addChild(new ui::MenuSeparator);
        struct IntItem : ui::MenuItem {
            MeloDicer* module; int* target; int value;
            void onAction(const event::Action&) override { if (module && target) *target = value; }
            void step() override { rightText = (target && *target == value) ? "✔" : ""; ui::MenuItem::step(); }
        };

        menu->addChild(createSubmenuItem("Sequencer Modes", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "Operating Mode"; sub->addChild(l);
              const char* n[] = {"A: Sequencer","B: Seq + Gate","C: Quantizer 1","D: Quantizer 2"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->modeSelect;it->value=v;sub->addChild(it);} }

            sub->addChild(new ui::MenuSeparator);

            struct RMI : ui::MenuItem { MeloDicer* module=nullptr; int value=0;
              void onAction(const event::Action&) override { if(module) module->switchRhythmMode(); }
              void step() override { if(module) rightText=(module->rhythmMode==value)?"✔":""; ui::MenuItem::step(); } };
            { auto* l = new ui::MenuLabel; l->text = "Rhythm Mode"; sub->addChild(l);
              auto* it0=createMenuItem<RMI>("Dice (static)"); it0->module=m; it0->value=0; sub->addChild(it0);
              auto* it1=createMenuItem<RMI>("Realtime");      it1->module=m; it1->value=1; sub->addChild(it1); }

            sub->addChild(new ui::MenuSeparator);

            struct MMI : ui::MenuItem { MeloDicer* module=nullptr; int value=0;
              void onAction(const event::Action&) override { if(module) module->switchMelodyMode(); }
              void step() override { if(module) rightText=(module->melodyMode==value)?"✔":""; ui::MenuItem::step(); } };
            { auto* l = new ui::MenuLabel; l->text = "Melody Mode"; sub->addChild(l);
              auto* it2=createMenuItem<MMI>("Dice (static)"); it2->module=m; it2->value=0; sub->addChild(it2);
              auto* it3=createMenuItem<MMI>("Realtime");      it3->module=m; it3->value=1; sub->addChild(it3); }

            sub->addChild(new ui::MenuSeparator);

            { 
                auto* l = new ui::MenuLabel; l->text = "Mute Behavior"; sub->addChild(l);
                sub->addChild(createBoolPtrMenuItem("Restart on unmute", "", &m->restartOnUnmute));
                sub->addChild(createBoolPtrMenuItem("Inverted Mute logic (GATE 2)", "", &m->invertMuteLogic));
            }
        }));

        menu->addChild(createSubmenuItem("DNA Rotation", "", [=](ui::Menu* sub) {
            auto addRot = [=](ui::Menu* m, const char* label, std::function<void(int)> func) {
                m->addChild(createSubmenuItem(label, "", [=](ui::Menu* s) {
                    s->addChild(createMenuItem("Rotate Forward (+1)", "", [=]() { func(1); }));
                    s->addChild(createMenuItem("Rotate Backward (-1)", "", [=]() { func(-1); }));
                }));
            };

            addRot(sub, "Rhythm (Gates/Rests)", [=](int s) { m->rotateRhythm(s); }); // Individual
            addRot(sub, "Variation (Lengths)", [=](int s) { m->rotateVariation(s); });
            addRot(sub, "Legato/Tie", [=](int s) { m->rotateLegato(s); });
            sub->addChild(new ui::MenuSeparator);
            addRot(sub, "Melody (Pitch)", [=](int s) { m->rotateMelody(s); }); // Individual
            addRot(sub, "Octave", [=](int s) { m->rotateOctave(s); });
            sub->addChild(new ui::MenuSeparator);
            addRot(sub, "Rotate Rhythm Pattern", [=](int s) { m->rotateRhythmPattern(s); }); // Combined
            addRot(sub, "Rotate Melody Pattern", [=](int s) { m->rotateMelodyPattern(s); }); // Combined
            sub->addChild(new ui::MenuSeparator);
            sub->addChild(createMenuItem("Rotate EVERYTHING (+1)", "", [=]() { 
                m->rotateRhythmPattern(1); m->rotateMelodyPattern(1);
            }));
            sub->addChild(new ui::MenuSeparator);
            sub->addChild(createMenuItem("Scramble Rhythm DNA", "", [=]() { m->scrambleRhythmRotation(); }));
            sub->addChild(createMenuItem("Scramble Melody DNA", "", [=]() { m->scrambleMelodyRotation(); }));
            sub->addChild(createMenuItem("Scramble ALL DNA (Remix)", "DNA Remix", [=]() { m->scrambleDnaRotation(); }));
            sub->addChild(new ui::MenuSeparator);
            sub->addChild(createMenuItem("Reset DNA Alignment", "Original Draw", [=]() { m->resetDnaRotation(); }));
        }));

        menu->addChild(createSubmenuItem("CV Assign", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "CV IN 1"; sub->addChild(l);
              const char* n[] = {"Add Seq","Transpose Seq","Mod Range LO","Mod Range HI"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv1Mode;it->value=v;sub->addChild(it);} }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "CV IN 2"; sub->addChild(l);
              const char* n[] = {"Note value","Variation","Legato","Rest"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv2Mode;it->value=v;sub->addChild(it);} }
        }));

        menu->addChild(createSubmenuItem("Gate Assign", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "Gate 1 Assignment"; sub->addChild(l);
              const char* n1[] = {"Toggle Dice R","Re-dice R","Re-dice M","Restart"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n1[v]);it->module=m;it->target=&m->gate1Assign;it->value=v;sub->addChild(it);} }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "Gate 2 Assignment"; sub->addChild(l);
              const char* n2[] = {"Toggle Dice M","Re-dice M","Mute","Restart"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n2[v]);it->module=m;it->target=&m->gate2Assign;it->value=v;sub->addChild(it);} }
        }));

        menu->addChild(createSubmenuItem("Scales", "", [=](ui::Menu* sub) {
            sub->addChild(createBoolMenuItem("Lock Scale Notes", "",
                [=]() { return m->lockScaleNotes; },
                [=](bool v) { m->lockScaleNotes = v; m->updateScaleMask(); }
            ));
            sub->addChild(new ui::MenuSeparator);

            sub->addChild(createSubmenuItem("Root Note", "Set the scale root", [=](ui::Menu* rootMenu) {
                const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                for (int i = 0; i < 12; i++) {
                    struct RootItem : ui::MenuItem {
                        MeloDicer* module; int value;
                        void onAction(const event::Action&) override { 
                            module->scaleRoot = value; 
                            module->updateScaleMask();
                        }
                        void step() override { rightText = (module->scaleRoot == value) ? "✔" : ""; ui::MenuItem::step(); }
                    };
                    auto* it = createMenuItem<RootItem>(noteNames[i]);
                    it->module = m; it->value = i;
                    rootMenu->addChild(it);
                }
            }));

            sub->addChild(createSubmenuItem("Scale Type", "Choose scale", [=](ui::Menu* typeMenu) {
                int scaleIdx = 0;
                for (const auto& scale : BITWIG_SCALES) {
                    struct ScaleItem : ui::MenuItem {
                        MeloDicer* module; ScaleType scale; int index;
                        void onAction(const event::Action&) override {
                            module->lastSelectedScale = index;
                            module->updateScaleMask();
                        }
                        void step() override {
                            rightText = (module && module->lastSelectedScale == index) ? "✔" : "";
                            ui::MenuItem::step();
                        }
                    };
                    auto* it = createMenuItem<ScaleItem>(scale.name);
                    it->module = m; it->scale = scale; it->index = scaleIdx++;
                    typeMenu->addChild(it);
                }
            }));
        }));

        menu->addChild(createSubmenuItem("Note Division and Clock", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "Note Variation"; sub->addChild(l);
              struct MaskItem : ui::MenuItem { MeloDicer* module=nullptr; int bit=0;
                void onAction(const event::Action&) override { if(module) module->noteVariationMask ^= (1<<bit); }
                void step() override { rightText=(module&&(module->noteVariationMask&(1<<bit)))?"✔":""; ui::MenuItem::step(); } };
              auto add=[&](const char* t,int b){auto* it=createMenuItem<MaskItem>(t);it->module=m;it->bit=b;sub->addChild(it);};
              add("Allow 1/8T",0); add("Allow 1/16T",1); add("Allow 1/32 & 1/32T",2); }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "PPQN"; sub->addChild(l);
              struct PItem : ui::MenuItem { MeloDicer* module=nullptr; int value=4;
                void onAction(const event::Action&) override { if(module) module->ppqnSetting=value; }
                void step() override { if(module) rightText=(module->ppqnSetting==value)?"✔":""; ui::MenuItem::step(); } };
              for (int v : {1,4,24}){auto* it=createMenuItem<PItem>(string::f("%d",v).c_str());it->module=m;it->value=v;sub->addChild(it);} }
        }));
    }