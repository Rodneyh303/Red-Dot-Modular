#include <rack.hpp>
#include <algorithm>
#include "MeloDicerWidget.hpp"
#include "MeloDicer.hpp"

using namespace rack;

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
            addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(
                mm2px(Vec(cx,59.75f)), module, MeloDicer::SEMI0_PARAM+i, MeloDicer::SEMI_LED_START+2*i));
        }

        addParam(createLightParamCentered<VCVLightSlider<RedLight>>(mm2px(Vec(100.f,59.75f)), module, MeloDicer::OCT_LO_PARAM, MeloDicer::OCT_LO_LED));
        addParam(createLightParamCentered<VCVLightSlider<RedLight>>(mm2px(Vec(108.f,59.75f)), module, MeloDicer::OCT_HI_PARAM, MeloDicer::OCT_HI_LED));

        // 16 step lights: Circular ring arrangement near top right
        {
            const float RCX = 138.f, RCY = 46.f, RLED = 13.25f;
            for (int i = 0; i < 16; ++i) {
                float ang = float(i) / 16.f * 2.f * M_PI - M_PI / 2.f;
                float lx = RCX + RLED * std::cos(ang), ly = RCY + RLED * std::sin(ang);
                if (i % 4 == 0) addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(lx, ly)), module, MeloDicer::STEP_LIGHTS_START + i));
                else            addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(lx, ly)), module, MeloDicer::STEP_LIGHTS_START + i));
            }
        }

        {
            const float MX=168.f,LX=W_MM-5.f;
            const float ys[4]={20.f,33.f,46.f,59.f};
            const int par[4]={MeloDicer::MODE_A_PARAM,MeloDicer::MODE_B_PARAM,MeloDicer::MODE_C_PARAM,MeloDicer::MODE_D_PARAM};
            const int lit[4]={MeloDicer::MODE_A_LIGHT,MeloDicer::MODE_B_LIGHT,MeloDicer::MODE_C_LIGHT,MeloDicer::MODE_D_LIGHT};
            for (int i=0;i<4;++i) {
                addParam(createParamCentered<TL1105>(mm2px(Vec(MX,ys[i])),module,par[i]));
                addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(LX,ys[i])),module,lit[i]));
            }
        }

        addParam(createParamCentered<TL1105>(mm2px(Vec( 14.f,92.f)),module,MeloDicer::DICE_R_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec( 14.f,98.f)),module,MeloDicer::RHYTHM_DICE_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( 30.f,92.f)),module,MeloDicer::DICE_M_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec( 30.f,98.f)),module,MeloDicer::MELODY_DICE_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( 46.f,92.f)),module,MeloDicer::LOCK_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec( 46.f,98.f)),module,MeloDicer::LOCK_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( 62.f,92.f)),module,MeloDicer::MUTE_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(  mm2px(Vec( 62.f,98.f)),module,MeloDicer::MUTE_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( 78.f,92.f)),module,MeloDicer::RESET_BUTTON_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec( 78.f,98.f)),module,MeloDicer::RESET_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec( 94.f,92.f)),module,MeloDicer::RUN_GATE_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec( 94.f,98.f)),module,MeloDicer::RUN_GATE_LIGHT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 88.f,116.f)),module,MeloDicer::RUN_GATE_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec( 98.f,116.f)),module,MeloDicer::RESET_TRIGGER_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(108.f,116.f)),module,MeloDicer::SEED_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(118.f,116.f)),module,MeloDicer::LENGTH_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(128.f,116.f)),module,MeloDicer::OFFFSET_INPUT));

        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 14.f,124.5f)),module,MeloDicer::CLK_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 30.f,124.5f)),module,MeloDicer::GATE1_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 46.f,124.5f)),module,MeloDicer::GATE2_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 62.f,124.5f)),module,MeloDicer::CV1_INPUT));
        addInput(createInputCentered<PJ301MPort>( mm2px(Vec( 78.f,124.5f)),module,MeloDicer::CV2_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec( 95.f,124.5f)),module,MeloDicer::SEED_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(111.f,124.5f)),module,MeloDicer::RUN_GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(127.f,124.5f)),module,MeloDicer::RESET_TRIGGER_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(143.f,124.5f)),module,MeloDicer::GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(159.f,124.5f)),module,MeloDicer::CV_OUTPUT));
    }

void MeloDicerWidget::applyTheme() {
 // Panel
        bool lightTheme = getLightTheme();  // read from module
        auto panelPath = asset::plugin(pluginInstance,
            lightTheme ? "res/panels/MeloDicer_panel_light.svg"
                       : "res/panels/MeloDicer_panel_v2.svg");
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
        for (int pid : {(int)MeloDicer::NOTE_VALUE_PARAM,
                        (int)MeloDicer::VARIATION_PARAM,
                        (int)MeloDicer::LEGATO_PARAM,
                        (int)MeloDicer::REST_PARAM,
                        (int)MeloDicer::BPM_PARAM,
                        (int)MeloDicer::PATTERN_LENGTH_PARAM,
                        (int)MeloDicer::PATTERN_OFFSET_PARAM}) {
            ParamWidget* pw = getParam(pid);
            if (pw) {
                removeChild(pw);
                delete pw;
            }
        }

        if (lightTheme) {
            // Light theme: Use dark knobs
            addParam(createParamCentered<RDM_KnobDarkLarge> (mm2px(Vec(14.f,24.f)),mod,MeloDicer::NOTE_VALUE_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(37.f,24.f)),mod,MeloDicer::VARIATION_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(60.f,24.f)),mod,MeloDicer::LEGATO_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(83.f,24.f)),mod,MeloDicer::REST_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>     (mm2px(Vec(110.f,24.f)),mod,MeloDicer::BPM_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>     (mm2px(Vec(133.f,24.f)),mod,MeloDicer::PATTERN_LENGTH_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>     (mm2px(Vec(156.f,24.f)),mod,MeloDicer::PATTERN_OFFSET_PARAM));
        } else {
            // Dark theme: Use cream/light knobs
            addParam(createParamCentered<RDM_KnobCreamLarge> (mm2px(Vec(14.f,24.f)),mod,MeloDicer::NOTE_VALUE_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(37.f,24.f)),mod,MeloDicer::VARIATION_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(60.f,24.f)),mod,MeloDicer::LEGATO_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(83.f,24.f)),mod,MeloDicer::REST_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>      (mm2px(Vec(110.f,24.f)),mod,MeloDicer::BPM_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>      (mm2px(Vec(133.f,24.f)),mod,MeloDicer::PATTERN_LENGTH_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>      (mm2px(Vec(156.f,24.f)),mod,MeloDicer::PATTERN_OFFSET_PARAM));
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
        auto c=[&](int r,int g,int b,int a=255){
            if (lt) {
                auto inv=[](int v){ return std::max(0, 220-v); };
                bool isGrey = (std::abs(r-g)<20 && std::abs(g-b)<20);
                if (isGrey) { r=inv(r); g=inv(g); b=inv(b); }
                else { r=std::max(0,r*7/10); g=std::max(0,g*7/10); b=std::max(0,b*7/10); }
            }
            nvgFillColor(vg,nvgRGBA(r,g,b,a));
        };
        auto L=[&](float x,float y,const char* t){ nvgText(vg,mm2px(x),mm2px(y),t,nullptr); };
        auto sz=[&](float mm){ nvgFontSize(vg,mm2px(mm)); };

        sz(4.2f); c(200,200,200); L(W_MM/2.f,5.5f,"Red Dot Modular  -  MeloDicer");
        sz(3.4f); c(200,200,200); L(14,37.5f,"NOTE VALUE"); L(37,37.5f,"VARIATION"); L(60,37.5f,"LEGATO"); L(83,37.5f,"REST");

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

        sz(2.5f);
        {
            const char* noteLabels[8] = {"1/2","1/4","1/4T","1/8","1/8T","1/16","1/32T","1/32"};
            for (int i=0;i<8;++i) {
                float deg = -225.f + i * (270.f/7.f);
                arcLabel(14.f, 26.f, 12.5f, deg, noteLabels[i], 150,150,135);
            }
        }
        sz(2.8f);
        arcLabel(37.f, 26.f, 12.5f, -225.f, "LONGER",  130,130,120); arcLabel(37.f, 26.f, 12.5f,  +45.f, "SHORTER", 130,130,120);
        arcLabel(60.f, 26.f, 12.0f, -225.f, "0%",   130,130,120); arcLabel(60.f, 26.f, 12.0f,  +45.f, "100%", 130,130,120);
        arcLabel(83.f, 26.f, 12.0f, -225.f, "0%",   130,130,120); arcLabel(83.f, 26.f, 12.0f,  +45.f, "100%", 130,130,120);
        sz(3.2f); c(170,170,170); L(110,37.5f,"BPM"); L(133,37.5f,"LEN"); L(156,37.5f,"OFFSET");

        sz(2.5f); c(80,80,80); nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
        nvgText(vg,mm2px(4.5f),mm2px(43.f),"SEMITONE PROBABILITY",nullptr); nvgText(vg,mm2px(94.f),mm2px(43.f),"OCT",nullptr);
        nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);

        sz(3.0f); const char* sn[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for (int i=0;i<12;++i){ c(200,200,200); L(7.5f+i*7.f,43.f,sn[i]); }
        sz(2.7f); c(85,85,85); const char* nums[12]={"1","2","3","4","5","6","7","8","9","10","11","12"};
        for (int i=0;i<12;++i) L(7.5f+i*7.f,SL_TOP+SLH+6.f,nums[i]);
        sz(2.9f); c(38,166,154); L(100,43.f,"LO"); L(108,43.f,"HI");
        sz(4.0f); c(210,210,210); L(168,20.f,"A"); L(168,33.f,"B"); L(168,46.f,"C"); L(168,59.f,"D");

        sz(2.9f); c(200,60,60);  L(14,103.f,"DICE R"); L(30,103.f,"DICE M");
        c(190,190,190); L(46,103.f,"LOCK"); L(62,103.f,"MUTE"); L(78,103.f,"RESET"); L(94,103.f,"RUN");
        L(120,103.f,"RESET"); L(142,103.f,"RUN");

        const float JY3=124.5f,PR=7.7f/2.f; sz(3.0f); c(195,195,195);
        const char* il[5]={"CLK","G1","G2","CV1","CV2"}; const float ix[5]={14,30,46,62,78};
        for(int i=0;i<5;++i) L(ix[i],JY3-PR-2.2f,il[i]);
        sz(2.7f); c(180,180,180); const char* ol[5]={"SEED","RUN","RST","GATE","CV"};
        const float ox[5]={95,111,127,143,159};
        for(int i=0;i<5;++i) L(ox[i],JY3-PR-2.2f,ol[i]);
        sz(2.3f); c(80,80,80); nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
        const char* sl[5] = {"RUN", "RST", "SEED", "LEN", "OFF"};
        const float sx[5] = {88.f, 98.f, 108.f, 118.f, 128.f};
        for(int i=0;i<5;++i) L(sx[i],116.f-PR-1.8f,sl[i]);
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
        { auto* l = new ui::MenuLabel; l->text = "Mode"; menu->addChild(l);
          const char* n[] = {"A: Sequencer","B: Seq + Gate","C: Quantizer 1","D: Quantizer 2"};
          for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->modeSelect;it->value=v;menu->addChild(it);} }
        { auto* l = new ui::MenuLabel; l->text = "CV IN 1"; menu->addChild(l);
          const char* n[] = {"Add Seq","Transpose Seq","Mod Range LO","Mod Range HI"};
          for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv1Mode;it->value=v;menu->addChild(it);} }
        { auto* l = new ui::MenuLabel; l->text = "CV IN 2"; menu->addChild(l);
          const char* n[] = {"Note value","Variation","Legato","Rest"};
          for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv2Mode;it->value=v;menu->addChild(it);} }
        { auto* l = new ui::MenuLabel; l->text = "Gate 1 Assignment"; menu->addChild(l);
          const char* n1[] = {"Toggle Dice R","Re-dice R","Re-dice M","Restart"};
          for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n1[v]);it->module=m;it->target=&m->gate1Assign;it->value=v;menu->addChild(it);}
          auto* l2 = new ui::MenuLabel; l2->text = "Gate 2 Assignment"; menu->addChild(l2);
          const char* n2[] = {"Toggle Dice M","Re-dice M","Mute","Restart"};
          for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n2[v]);it->module=m;it->target=&m->gate2Assign;it->value=v;menu->addChild(it);} }
        { auto* l = new ui::MenuLabel; l->text = "Mute Behavior"; menu->addChild(l);
          const char* n[] = {"No restart","Restart on unmute","Invert gate (INV GATE)","Start muted (Power Mute)"};
          for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->muteBehavior;it->value=v;menu->addChild(it);} }
        { auto* l = new ui::MenuLabel; l->text = "Note Variation"; menu->addChild(l);
          struct MaskItem : ui::MenuItem { MeloDicer* module=nullptr; int bit=0;
            void onAction(const event::Action&) override { if(module) module->noteVariationMask ^= (1<<bit); }
            void step() override { rightText=(module&&(module->noteVariationMask&(1<<bit)))?"✔":""; ui::MenuItem::step(); } };
          auto add=[&](const char* t,int b){auto* it=createMenuItem<MaskItem>(t);it->module=m;it->bit=b;menu->addChild(it);};
          add("Allow 1/8T",0); add("Allow 1/16T",1); add("Allow 1/32 & 1/32T",2); }
        { auto* l = new ui::MenuLabel; l->text = "PPQN"; menu->addChild(l);
          struct PItem : ui::MenuItem { MeloDicer* module=nullptr; int value=4;
            void onAction(const event::Action&) override { if(module) module->ppqnSetting=value; }
            void step() override { if(module) rightText=(module->ppqnSetting==value)?"✔":""; ui::MenuItem::step(); } };
          for (int v : {1,4,24}){auto* it=createMenuItem<PItem>(string::f("%d",v).c_str());it->module=m;it->value=v;menu->addChild(it);} }
        { auto* l = new ui::MenuLabel; l->text = "Rhythm Mode"; menu->addChild(l);
          struct RMI : ui::MenuItem { MeloDicer* module=nullptr; int value=0;
            void onAction(const event::Action&) override { if(module) module->switchRhythmMode(); }
            void step() override { if(module) rightText=(module->rhythmMode==value)?"✔":""; ui::MenuItem::step(); } };
          auto* it0=createMenuItem<RMI>("Dice (static)"); it0->module=m; it0->value=0; menu->addChild(it0);
          auto* it1=createMenuItem<RMI>("Realtime");      it1->module=m; it1->value=1; menu->addChild(it1);
          auto* l2 = new ui::MenuLabel; l2->text = "Melody Mode"; menu->addChild(l2);
          struct MMI : ui::MenuItem { MeloDicer* module=nullptr; int value=0;
            void onAction(const event::Action&) override { if(module) module->switchMelodyMode(); }
            void step() override { if(module) rightText=(module->melodyMode==value)?"✔":""; ui::MenuItem::step(); } };
          auto* it2=createMenuItem<MMI>("Dice (static)"); it2->module=m; it2->value=0; menu->addChild(it2);
          auto* it3=createMenuItem<MMI>("Realtime");      it3->module=m; it3->value=1; menu->addChild(it3); }
    }