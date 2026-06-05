#include <rack.hpp>
#include <algorithm>
#include "MonsoonWidget.hpp"
#include "Monsoon.hpp"
#include "ui/OutputAccent.hpp"
#include "dsp/managers/MonsoonScaleManager.hpp"

using namespace rack;
using namespace MonsoonIds;
// ── Custom Slider for Scale Locking ──────────────────────────────────────────
template <typename TLightBase = RedLight>
struct MonsoonLightSlider : VCVLightSlider<TLightBase> {
    void draw(const widget::Widget::DrawArgs& args) override {
        auto* m = dynamic_cast<Monsoon*>(this->module);
        bool dimmed = false;
        if (m) {
            if (this->paramId >= MonsoonIds::SEMI0_PARAM && this->paramId < MonsoonIds::SEMI0_PARAM + 12) {
                int sem = this->paramId - MonsoonIds::SEMI0_PARAM;
                if (m->scaleManager && !(m->scaleManager->activeScaleMask & (1 << sem))) dimmed = true;
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

bool MonsoonWidget::getLightTheme() const {
    auto* m = dynamic_cast<const Monsoon*>(module);
    return m ? m->lightTheme : false;
}

void MonsoonWidget::setLightTheme(bool v) {
    auto* m = dynamic_cast<Monsoon*>(module);
    if (m) m->lightTheme = v;
}

MonsoonWidget::MonsoonWidget(Monsoon* module) {
        setModule(module);
        applyTheme();
        if (box.size.x == 0) box.size = mm2px(Vec(W_MM, 128.5f));

        // ── 12 semitone sliders: 9mm pitch ───────────────────────────────────
        for (int i = 0; i < 12; ++i)
            addParam(createLightParamCentered<MonsoonLightSlider<GreenRedLight>>(
                mm2px(Vec(7.5f + i*9.f, 59.75f)), module, SEMI0_PARAM+i, SEMI_LED_START+2*i));

        // ── OCT sliders ───────────────────────────────────────────────────────
        addParam(createLightParamCentered<VCVLightSlider<RedLight>>(
            mm2px(Vec(119.f, 59.75f)), module, MonsoonIds::OCT_LO_PARAM, MonsoonIds::OCT_LO_LED));
        addParam(createLightParamCentered<VCVLightSlider<RedLight>>(
            mm2px(Vec(128.f, 59.75f)), module, MonsoonIds::OCT_HI_PARAM, MonsoonIds::OCT_HI_LED));

        // ── 16-step light ring: enlarged, cx=162 cy=30 r=18 ──────────────────
        {
            const float RCX=162.f, RCY=30.f, RLED=14.f;
            for (int i = 0; i < 16; ++i) {
                float ang = float(i)/16.f * 2.f*M_PI - M_PI/2.f;
                float lx  = RCX + RLED*std::cos(ang);
                float ly  = RCY + RLED*std::sin(ang);
                if (i%4==0) addChild(createLightCentered<SmallLight<RedLight>>(  mm2px(Vec(lx,ly)), module, MonsoonIds::STEP_LIGHTS_START+i));
                else        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(lx,ly)), module, MonsoonIds::STEP_LIGHTS_START+i));
            }
        }

        // ── Mode button + lights: far right ──────────────────────────────────
        addParam(createParamCentered<TL1105>(mm2px(Vec(197.f, 12.f)), module, MonsoonIds::MODE_PARAM));
        for (int i = 0; i < 4; ++i)
            addChild(createLightCentered<MediumLight<YellowLight>>(
                mm2px(Vec(192.f, 34.f + i*8.f)), module, MonsoonIds::MODE_A_LIGHT+i));

        // ── 6 action buttons: y=87, 15mm pitch ───────────────────────────────
        const float JY=87.f, JYL=93.f, JX=118.f, JP=15.f;
        // Utility buttons (lock/mute/reset/run) stay small (TL1105), on the right.
        addParam(createParamCentered<TL1105>(mm2px(Vec(JX+2*JP, JY)), module, MonsoonIds::LOCK_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec(JX+2*JP, JYL)), module, MonsoonIds::LOCK_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec(JX+3*JP, JY)), module, MonsoonIds::MUTE_PARAM));
        addChild(createLightCentered<MediumLight<RedLight>>(  mm2px(Vec(JX+3*JP, JYL)), module, MonsoonIds::MUTE_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec(JX+4*JP, JY)), module, MonsoonIds::RESET_BUTTON_PARAM));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec(JX+4*JP, JYL)), module, MonsoonIds::RESET_LIGHT));
        addParam(createParamCentered<TL1105>(mm2px(Vec(JX+5*JP, JY)), module, MonsoonIds::RUN_GATE_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(JX+5*JP, JYL)), module, MonsoonIds::RUN_GATE_LIGHT));

        // ── Dice + Slew + Mix cluster (left of the utility buttons) ──────────
        // All four dice are LARGER (VCVButton) than the utility buttons so they
        // stand out. Layout per column (rhythm | melody):
        //   row A (top):  MAIN dice  (commit, A walks)
        //   row B:        TRIAL dice (audition vs fixed A)
        //   plus a SLEW trim and a MIX knob per column in the cluster.
        const float CX = 6.f;          // cluster origin x (free left/mid strip)
        const float CP = 11.f;         // column pitch (rhythm vs melody)
        const float MAINY = 84.f, TRIALY = 92.f, SLEWY = 76.f, MIXY = 100.f;
        // MAIN dice (big), with dice lights
        addParam(createParamCentered<VCVButton>(mm2px(Vec(CX,    MAINY)), module, MonsoonIds::DICE_R_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(CX+3.2f, MAINY-3.2f)), module, MonsoonIds::RHYTHM_DICE_LIGHT));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(CX+CP, MAINY)), module, MonsoonIds::DICE_M_PARAM));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(CX+CP+3.2f, MAINY-3.2f)), module, MonsoonIds::MELODY_DICE_LIGHT));
        // TRIAL dice (big)
        addParam(createParamCentered<VCVButton>(mm2px(Vec(CX,    TRIALY)), module, MonsoonIds::DICE_TRIAL_R_PARAM));
        addParam(createParamCentered<VCVButton>(mm2px(Vec(CX+CP, TRIALY)), module, MonsoonIds::DICE_TRIAL_M_PARAM));
        // SLEW trims (consumed at roll: limits step size)
        addParam(createParamCentered<Trimpot>(mm2px(Vec(CX,    SLEWY)), module, MonsoonIds::DICE_SLEW_R_PARAM));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(CX+CP, SLEWY)), module, MonsoonIds::DICE_SLEW_M_PARAM));
        // MIX knobs (live A<->B morph, like spread)
        addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(CX,    MIXY)), module, MonsoonIds::RHYTHM_MIX_PARAM));
        addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(CX+CP, MIXY)), module, MonsoonIds::MELODY_MIX_PARAM));

        // ── Inputs: row1 = transport+gates, row2 = clock+CV. 17mm pitch. ─────
        const float IX=15.f, IP=17.f;
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX,      105.f)), module, MonsoonIds::RUN_GATE_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+1*IP, 105.f)), module, MonsoonIds::RESET_TRIGGER_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+2*IP, 105.f)), module, MonsoonIds::SEED_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+3*IP, 105.f)), module, MonsoonIds::GATE1_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+4*IP, 105.f)), module, MonsoonIds::GATE2_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+5*IP, 105.f)), module, MonsoonIds::GATE3_MOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX,      120.f)), module, MonsoonIds::CLK_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+1*IP, 120.f)), module, MonsoonIds::LENGTH_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+2*IP, 120.f)), module, MonsoonIds::OFFSET_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+3*IP, 120.f)), module, MonsoonIds::CV1_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+4*IP, 120.f)), module, MonsoonIds::CV2_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(IX+5*IP, 120.f)), module, MonsoonIds::CV3_MOD_INPUT));

        // ── Outputs: shifted right (114→182, 17mm) to clear the 6-wide input
        //    rows. In/out accent region sits behind the output group. ──────────
        {
            Monsoon* mm = dynamic_cast<Monsoon*>(module);
            redDot::addOutputAccent(this, 106.f, 97.f, 88.f, 31.f,
                [mm]() { return mm ? mm->lightTheme : false; });
        }
        const float OX=114.f, OP=17.f;
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX,      105.f)), module, MonsoonIds::GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX+1*OP, 105.f)), module, MonsoonIds::TIE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX+2*OP, 105.f)), module, MonsoonIds::LEGATO_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX+3*OP, 105.f)), module, MonsoonIds::TIE_OR_LEGATO_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX+4*OP, 105.f)), module, MonsoonIds::ACCENT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX,      120.f)), module, MonsoonIds::CV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX+1*OP, 120.f)), module, MonsoonIds::SEED_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX+2*OP, 120.f)), module, MonsoonIds::RUN_GATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(OX+3*OP, 120.f)), module, MonsoonIds::RESET_TRIGGER_OUTPUT));

        // ── Expander lights ───────────────────────────────────────────────────
        addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(EXP_LIGHT_X,              EXP_LIGHT_Y)), module, MonsoonIds::SCALE_EXPANDER_LIGHT));
        addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(EXP_LIGHT_X+EXP_LIGHT_S,  EXP_LIGHT_Y)), module, MonsoonIds::DNA_EXPANDER_LIGHT));
        addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(EXP_LIGHT_X+2*EXP_LIGHT_S,EXP_LIGHT_Y)), module, MonsoonIds::POLY_EXPANDER_LIGHT));
    }

void MonsoonWidget::applyTheme() {
 // Panel
    bool lightTheme = getLightTheme();  // read from module
    auto panelPath = asset::plugin(pluginInstance,
        lightTheme ? "res/panels/Monsoon_panel_light_monsoon.svg"
                   : "res/panels/Monsoon_panel_dark_monsoon.svg");

    // Standard SvgPanel for BOTH the browser preview (module==nullptr) and a
    // placed module. Same code path either way so they render identically.
    app::SvgPanel* sp = nullptr;
    for (widget::Widget* child : children) {
        sp = dynamic_cast<app::SvgPanel*>(child);
        if (sp) break;
    }

    if (sp) {
        // theme toggle on an existing widget: just swap the background SVG
        sp->setBackground(APP->window->loadSvg(panelPath));
    } else {
        setPanel(createPanel(panelPath));
    }

    // Remove any existing knob params at the 7 top knob positions
        // so we can re-add with the correct type.
        // Rack allows re-adding after clearing; easiest is to replace
        // the ParamWidget at the known indices. We use the positions directly.
        // Since this is called once at construction (no existing widgets yet),
        // we just add fresh. On theme toggle, widgets are rebuilt via
        // removing children with the affected paramIds first.
        auto* mod = dynamic_cast<Monsoon*>(module);
        // auto removeKnob = [&](int paramId) {
        //     for (auto it = children.begin(); it != children.end(); ) {
        //         auto* pw = dynamic_cast<ParamWidget*>(*it);
        //         if (pw && pw->paramId == paramId) {
        //             it = children.erase(it);
        //         } else { ++it; }
        //     }
        // };
        // Remove the 7 top knobs (they get re-added below with correct type)
        for (int pid : {(int)MonsoonIds::NOTE_VALUE_PARAM,
                        (int)MonsoonIds::VARIATION_PARAM,
                        (int)MonsoonIds::LEGATO_PARAM,
                        (int)MonsoonIds::REST_PARAM,
                        (int)MonsoonIds::ACCENT_KNOB,
                        (int)MonsoonIds::BPM_PARAM,
                        (int)MonsoonIds::PATTERN_LENGTH_PARAM,
                        (int)MonsoonIds::PATTERN_OFFSET_PARAM}) {
            ParamWidget* pw = getParam(pid);
            if (pw) {
                removeChild(pw);
                delete pw;
            }
        }

        if (lightTheme) {
            addParam(createParamCentered<RDM_KnobDarkLarge> (mm2px(Vec(16.f,22.f)), mod, MonsoonIds::NOTE_VALUE_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(42.f,22.f)), mod, MonsoonIds::VARIATION_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(68.f,22.f)), mod, MonsoonIds::LEGATO_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(94.f,22.f)), mod, MonsoonIds::REST_PARAM));
            addParam(createParamCentered<RDM_KnobDarkMedium>(mm2px(Vec(120.f,22.f)), mod, MonsoonIds::ACCENT_KNOB));
            // BPM/LEN/OFFSET below ring
            addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(148.f,58.f)), mod, MonsoonIds::BPM_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(163.f,58.f)), mod, MonsoonIds::PATTERN_LENGTH_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(178.f,58.f)), mod, MonsoonIds::PATTERN_OFFSET_PARAM));
        } else {
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(16.f,22.f)), mod, MonsoonIds::NOTE_VALUE_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(42.f,22.f)), mod, MonsoonIds::VARIATION_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(68.f,22.f)), mod, MonsoonIds::LEGATO_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(94.f,22.f)), mod, MonsoonIds::REST_PARAM));
            addParam(createParamCentered<RDM_KnobCreamMedium>(mm2px(Vec(120.f,22.f)), mod, MonsoonIds::ACCENT_KNOB));
            addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(148.f,58.f)), mod, MonsoonIds::BPM_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(163.f,58.f)), mod, MonsoonIds::PATTERN_LENGTH_PARAM));
            addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(178.f,58.f)), mod, MonsoonIds::PATTERN_OFFSET_PARAM));
        }
    }

void MonsoonWidget::draw(const DrawArgs& args) {
        NVGcontext* vg=args.vg;

        // Force a solid opaque background fill (alpha 255) to prevent transparency
    // Draw solid opaque background to prevent transparency
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, getLightTheme() ? nvgRGBA(0xe6, 0xe6, 0xe6, 255) : nvgRGBA(0x23, 0x23, 0x23, 255));
    nvgFill(vg);

    ModuleWidget::draw(args); // renders the panel SVG + child widgets (knobs/jacks/LEDs)

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

        setNvgFontSize(3.4f); fillNvgColour(200,200,200);
        writeNvgText(16.f,34.f,"NOTE VALUE"); writeNvgText(42.f,34.f,"VARIATION");
        writeNvgText(68.f,34.f,"LEGATO");     writeNvgText(94.f,34.f,"REST");
        writeNvgText(120.f,34.f,"ACCENT");

        auto arcLabel = [&](float cx_mm, float cy_mm, float r_mm, float angle_deg, const char* text, int ri=160, int gi=160, int bi=160) {
            float a=angle_deg*float(M_PI)/180.f, tx=cx_mm+r_mm*std::cos(a), ty=cy_mm+r_mm*std::sin(a);
            nvgSave(vg); nvgTranslate(vg,mm2px(tx),mm2px(ty)); nvgRotate(vg,a+float(M_PI)/2.f);
            if(lt){auto inv=[](int v){return std::max(0,220-v);}; bool isGrey=(std::abs(ri-gi)<20&&std::abs(gi-bi)<20); if(isGrey){ri=inv(ri);gi=inv(gi);bi=inv(bi);}else{ri=ri*7/10;gi=gi*7/10;bi=bi*7/10;}}
            nvgFillColor(vg,nvgRGBA(ri,gi,bi,200)); nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
            nvgText(vg,0,0,text,nullptr); nvgRestore(vg);
        };

        setNvgFontSize(2.5f);
        { const char* n[8]={"1/2","1/4","1/4T","1/8","1/8T","1/16","1/32T","1/32"};
          for(int i=0;i<8;++i) arcLabel(16.f,22.f,13.5f,-225.f+i*(270.f/7.f),n[i],150,150,135); }
        setNvgFontSize(2.8f);
        arcLabel(42.f,22.f,13.f,-225.f,"LONGER",130,130,120); arcLabel(42.f,22.f,13.f,45.f,"SHORTER",130,130,120);
        arcLabel(68.f,22.f,12.f,-225.f,"0%",130,130,120);     arcLabel(68.f,22.f,12.f,45.f,"100%",130,130,120);
        arcLabel(94.f,22.f,12.f,-225.f,"0%",130,130,120);     arcLabel(94.f,22.f,12.f,45.f,"100%",130,130,120);
        arcLabel(120.f,22.f,12.f,-225.f,"0%",130,130,120);     arcLabel(120.f,22.f,12.f,45.f,"100%",130,130,120);

        // Seq knob labels (below ring)
        setNvgFontSize(3.2f); fillNvgColour(170,170,170);
        writeNvgText(148.f,70.f,"BPM"); writeNvgText(163.f,70.f,"LEN"); writeNvgText(178.f,70.f,"OFFSET");

        // Semitone note labels
        setNvgFontSize(3.0f);
        const char* sn[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for(int i=0;i<12;++i){ fillNvgColour(200,200,200); writeNvgText(7.5f+i*9.f,43.f,sn[i]); }
        setNvgFontSize(2.7f); fillNvgColour(85,85,85);
        const char* nums[12]={"1","2","3","4","5","6","7","8","9","10","11","12"};
        for(int i=0;i<12;++i) writeNvgText(7.5f+i*9.f,SL_TOP+SLH+6.f,nums[i]);
        setNvgFontSize(2.9f); fillNvgColour(38,166,154);
        writeNvgText(119.f,43.f,"LO"); writeNvgText(128.f,43.f,"HI");

        // ── Slider tick marks: parallel horizontal lines per slider (meloDICER style) ──
        // 9 tick levels in the slider travel range, alternating long/short
        // Travel: SL_TOP=45mm to SL_TOP+SLH=74.5mm → 29.5mm / 8 intervals = 3.69mm each
        {
            const float SL_BOT = SL_TOP + SLH;
            const int   N_TICKS = 9;
            const float tickXs[14] = {7.5f,16.5f,25.5f,34.5f,43.5f,52.5f,61.5f,70.5f,79.5f,88.5f,97.5f,106.5f,119.f,128.f};
            nvgBeginPath(vg);
            for (int t = 0; t < N_TICKS; ++t) {
                float ty   = mm2px(SL_TOP + t*(SL_BOT-SL_TOP)/(N_TICKS-1));
                bool  major = (t==0 || t==4 || t==8);
                float hw   = mm2px(major ? 2.2f : 1.4f);
                for (int i = 0; i < 14; ++i) {
                    float cx = mm2px(tickXs[i]);
                    nvgMoveTo(vg, cx - hw, ty);
                    nvgLineTo(vg, cx + hw, ty);
                }
            }
            nvgStrokeWidth(vg, 0.6f);
            if (lt) nvgStrokeColor(vg, nvgRGBA(140,140,140,90));
            else    nvgStrokeColor(vg, nvgRGBA(180,180,180,55));
            nvgStroke(vg);
        }

        // Mode
        setNvgFontSize(3.2f); fillNvgColour(210,210,210); writeNvgText(197.f,5.f,"MODE");
        setNvgFontSize(3.8f);
        const float LX=192.f, LY0=34.f, LDY=8.f;
        writeNvgText(LX,LY0,"A"); writeNvgText(LX,LY0+LDY,"B");
        writeNvgText(LX,LY0+2*LDY,"C"); writeNvgText(LX,LY0+3*LDY,"D");

        // Expander lights
        setNvgFontSize(2.0f); fillNvgColour(200,200,200);
        writeNvgText(EXP_LIGHT_X,            EXP_LIGHT_Y+3.f,"S");
        writeNvgText(EXP_LIGHT_X+EXP_LIGHT_S,EXP_LIGHT_Y+3.f,"D");
        writeNvgText(EXP_LIGHT_X+2*EXP_LIGHT_S,EXP_LIGHT_Y+3.f,"P");

        // Buttons
        { const float BX=118.f,BP=15.f,BY=87.f;
          setNvgFontSize(2.7f); fillNvgColour(200,60,60);
          writeNvgText(BX,BY+6.5f,"DICE R"); writeNvgText(BX+BP,BY+6.5f,"DICE M");
          fillNvgColour(190,190,190);
          writeNvgText(BX+2*BP,BY+6.5f,"LOCK"); writeNvgText(BX+3*BP,BY+6.5f,"MUTE");
          writeNvgText(BX+4*BP,BY+6.5f,"RESET"); writeNvgText(BX+5*BP,BY+6.5f,"RUN"); }

        // Jack labels
        const float PR=7.7f/2.f;
        setNvgFontSize(2.7f); fillNvgColour(195,195,195);
        { const char* l[5]={"RUN","RST","SEED","LEN","OFF"}; const float x[5]={12,30,48,66,84};
          for(int i=0;i<5;++i) writeNvgText(x[i],105.f-PR-2.2f,l[i]); }
        { const char* l[5]={"CLK","G1","G2","CV1","CV2"}; const float x[5]={12,30,48,66,84};
          for(int i=0;i<5;++i) writeNvgText(x[i],120.f-PR-2.2f,l[i]); }
        fillNvgColour(180,180,180);
        { const char* l[5]={"GATE","TIE","LEG","T|L","ACC"}; const float x[5]={104,122,140,158,176};
          for(int i=0;i<5;++i) writeNvgText(x[i],105.f-PR-2.2f,l[i]); }
        { const char* l[4]={"CV","SEED","RUN","RST"}; const float x[4]={104,122,140,158};
          for(int i=0;i<4;++i) writeNvgText(x[i],120.f-PR-2.2f,l[i]); }
    }

void MonsoonWidget::appendContextMenu(ui::Menu* menu) {
        auto* m = dynamic_cast<Monsoon*>(module);
        if (!m) return;
        menu->addChild(new ui::MenuSeparator);
        struct ThemeItem : ui::MenuItem {
            MonsoonWidget* widget = nullptr;
            void onAction(const event::Action&) override { widget->setLightTheme(!widget->getLightTheme()); widget->applyTheme(); }
            void step() override { rightText = widget->getLightTheme() ? "Light ✔" : "Dark ✔"; ui::MenuItem::step(); }
        };
        auto* themeItem = createMenuItem<ThemeItem>("Theme"); themeItem->widget = this; menu->addChild(themeItem);
        menu->addChild(new ui::MenuSeparator);
        struct IntItem : ui::MenuItem {
            Monsoon* module; int* target; int value;
            void onAction(const event::Action&) override { if (module && target) *target = value; }
            void step() override { rightText = (target && *target == value) ? "✔" : ""; ui::MenuItem::step(); }
        };

        menu->addChild(createSubmenuItem("Poly Voices", "", [=](ui::Menu* sub) {
            sub->addChild(new ui::MenuLabel);
            auto* l = new ui::MenuLabel; l->text = "Active Voices (1 = mono only)"; sub->addChild(l);
            const char* labels[] = {"1 (mono)", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
            for (int v = 0; v <= 15; ++v) {
                auto* it = createMenuItem<IntItem>(labels[v]);
                it->module = m;
                it->target = &m->engine.numPolyVoices;
                it->value  = v;
                sub->addChild(it);
            }
            sub->addChild(new ui::MenuSeparator);
            auto* note = new ui::MenuLabel;
            note->text = "Requires PolyVoice expander(s) for outputs";
            sub->addChild(note);
        }));

        menu->addChild(createSubmenuItem("Sequencer Modes", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "Operating Mode"; sub->addChild(l);
              const char* n[] = {"A: Sequencer","B: Seq + Gate","C: Quantizer 1","D: Quantizer 2"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->modeSelect;it->value=v;sub->addChild(it);} }

            sub->addChild(new ui::MenuSeparator);

            struct RMI : ui::MenuItem { Monsoon* module=nullptr; int value=0;
              void onAction(const event::Action&) override { if(module) module->switchRhythmMode(); }
              void step() override { if(module) rightText=(module->rhythmMode==value)?"✔":""; ui::MenuItem::step(); } };
            { auto* l = new ui::MenuLabel; l->text = "Rhythm Mode"; sub->addChild(l);
              auto* it0=createMenuItem<RMI>("Dice (static)"); it0->module=m; it0->value=0; sub->addChild(it0);
              auto* it1=createMenuItem<RMI>("Realtime");      it1->module=m; it1->value=1; sub->addChild(it1); }

            sub->addChild(new ui::MenuSeparator);

            struct MMI : ui::MenuItem { Monsoon* module=nullptr; int value=0;
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

            sub->addChild(new ui::MenuSeparator);

            {
                auto* l = new ui::MenuLabel; l->text = "Reseed Policy"; sub->addChild(l);
                sub->addChild(createBoolPtrMenuItem("Reseed on roll (main dice)", "", &m->reseedOnRoll));
                sub->addChild(createBoolPtrMenuItem("Reseed on restart", "", &m->reseedOnRestart));
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
            sub->addChild(createMenuItem("Scramble Rhythm DNA", "", [=]() { m->dnaManager.scrambleRhythmGroup(); }));
            sub->addChild(createMenuItem("Scramble Melody DNA", "", [=]() { m->dnaManager.scrambleMelodyGroup(); }));
            sub->addChild(createMenuItem("Scramble ALL DNA (Remix)", "DNA Remix", [=]() { m->dnaManager.scrambleAll(); }));
            sub->addChild(new ui::MenuSeparator);
            sub->addChild(createMenuItem("Reset DNA Alignment", "Original Draw", [=]() { m->dnaManager.resetAll(); }));
        }));

        menu->addChild(createSubmenuItem("CV Assign", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "CV IN 1"; sub->addChild(l);
              const char* n[] = {"Add Seq","Transpose Seq","Mod Range LO","Mod Range HI"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv1Mode;it->value=v;sub->addChild(it);} }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "CV IN 2"; sub->addChild(l);
              const char* n[] = {"Note value","Variation","Legato","Rest"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv2Mode;it->value=v;sub->addChild(it);} }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "CV IN 3 (assignable mod)"; sub->addChild(l);
              const char* n[] = {"Rhythm slew","Melody slew","Rhythm A>B mix","Melody A>B mix"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv3Target;it->value=v;sub->addChild(it);} }
        }));

        menu->addChild(createSubmenuItem("Gate Assign", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "Gate 1 Assignment"; sub->addChild(l);
              const char* n1[] = {"Toggle Dice R","Re-dice R","Re-dice M","Restart"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n1[v]);it->module=m;it->target=&m->gate1Assign;it->value=v;sub->addChild(it);} }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "Gate 2 Assignment"; sub->addChild(l);
              const char* n2[] = {"Toggle Dice M","Re-dice M","Mute","Restart"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n2[v]);it->module=m;it->target=&m->gate2Assign;it->value=v;sub->addChild(it);} }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "Gate 3 (assignable mod)"; sub->addChild(l);
              const char* n3[] = {"Trial rhythm die","Trial melody die","Toggle reseed-on-roll","Toggle reseed-on-restart"};
              for (int v=0;v<4;++v){auto* it=createMenuItem<IntItem>(n3[v]);it->module=m;it->target=&m->gate3Target;it->value=v;sub->addChild(it);} }
        }));

        menu->addChild(createSubmenuItem("Scales", "", [=](ui::Menu* sub) {
            sub->addChild(createBoolMenuItem("Lock Scale Notes", "",
                [=]() { return m->scaleManager ? m->scaleManager->lockScaleNotes : false; },
                [=](bool v) { if (m->scaleManager) { m->scaleManager->lockScaleNotes = v; m->scaleManager->updateScaleMask(); } }
            ));
            sub->addChild(new ui::MenuSeparator);

            sub->addChild(createSubmenuItem("Root Note", "Set the scale root", [=](ui::Menu* rootMenu) {
                const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                for (int i = 0; i < 12; i++) {
                    struct RootItem : ui::MenuItem {
                        Monsoon* module; int value;
                        void onAction(const event::Action&) override { 
                            if (module->scaleManager) { module->scaleManager->scaleRoot = value; module->scaleManager->updateScaleMask(); }
                        }
                        void step() override { rightText = (module->scaleManager && module->scaleManager->scaleRoot == value) ? "✔" : ""; ui::MenuItem::step(); }
                    };
                    auto* it = createMenuItem<RootItem>(noteNames[i]);
                    it->module = m; it->value = i;
                    rootMenu->addChild(it);
                }
            }));

            sub->addChild(createSubmenuItem("Scale Type", "Choose scale", [=](ui::Menu* typeMenu) {
                int scaleIdx = 0;
                for (const auto& scale : MONSOON_SCALES) {
                    struct ScaleItem : ui::MenuItem {
                        Monsoon* module; ScaleType scale; int index;
                        void onAction(const event::Action&) override {
                            if (module->scaleManager) { module->scaleManager->lastSelectedScale = index; module->scaleManager->updateScaleMask(); }
                        }
                        void step() override {
                            rightText = (module->scaleManager && module->scaleManager->lastSelectedScale == index) ? "✔" : "";
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
              struct MaskItem : ui::MenuItem { Monsoon* module=nullptr; int bit=0;
                void onAction(const event::Action&) override { if(module) module->noteVariationMask ^= (1<<bit); }
                void step() override { rightText=(module&&(module->noteVariationMask&(1<<bit)))?"✔":""; ui::MenuItem::step(); } };
              auto add=[&](const char* t,int b){auto* it=createMenuItem<MaskItem>(t);it->module=m;it->bit=b;sub->addChild(it);};
              add("Allow 1/8T",0); add("Allow 1/16T",1); add("Allow 1/32 & 1/32T",2); }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "PPQN"; sub->addChild(l);
              struct PItem : ui::MenuItem { Monsoon* module=nullptr; int value=4;
                void onAction(const event::Action&) override { if(module) module->ppqnSetting=value; }
                void step() override { if(module) rightText=(module->ppqnSetting==value)?"✔":""; ui::MenuItem::step(); } };
              for (int v : {1,4,24}){auto* it=createMenuItem<PItem>(string::f("%d",v).c_str());it->module=m;it->value=v;sub->addChild(it);} }
        }));
    }