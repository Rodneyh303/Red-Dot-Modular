#pragma once
// MonsoonTemasekExpander — the Temasek transform expander for Change Alley.
//
// Design: CHANGE_ALLEY_DESIGN.md §13–§14.
// Temasek is the old Malay name for Singapore (sea town) — the layer beneath Change Alley
// that reshapes the whole board at once, at phrase boundaries, by force.
//
// Layout: intra (within-block) verbs LEFT of a ~40HP panel, inter (across-block) RIGHT,
// mirrored with jacks to the outside. Top-to-bottom: Collapse, Rotate, Reflect, Scatter.
// Per verb × side (intra/inter) × pin type (R/M):
//   domain button+jack, codomain button+jack, grain knob.
//   Collapse additionally: leader knob.
//   Rotate additionally: step knob.
//   Scatter additionally: fwd+back jacks (like dice) instead of a step knob.
//
// Queue: one PendingAction per row (verb × side × type), latched at trigger time (§14a).
// Applied at the phrase boundary by MonsoonExpanderManager, exactly as Change Alley's
// current transforms. Change Alley and Temasek share the same src arrays on the module.
//
// Claiming: Temasek must be attached to a Change Alley, which is attached to Monsoon.
// The chain: Monsoon ↔ Change Alley ↔ Temasek.

#include "Monsoon.hpp"
// (deliberately does NOT include MonsoonChangeAlleyExpander.hpp -- Change Alley's
//  header includes THIS one for the submatrix highlight, so including it back would
//  be circular. Temasek references nothing from Change Alley; the manager does the
//  cross-module work.)
#include "ui/StoreBound.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
namespace TK = TemasekIds;

struct MonsoonTemasekExpander : Module {
    // Pending actions — one per row, latched at trigger time.
    TK::PendingAction pendingRows[TK::N_ROWS];

    // Trigger detectors
    dsp::SchmittTrigger domainTrig [TK::N_ROWS];
    dsp::SchmittTrigger codomainTrig[TK::N_ROWS];
    dsp::SchmittTrigger scatterFwd [TK::SIDES * TK::TYPES];
    dsp::SchmittTrigger scatterBack[TK::SIDES * TK::TYPES];
    dsp::BooleanTrigger btnTrig[TK::N_ROWS * 2];

    // Scatter Philox counters (one per side per pin type — own key, not the dice counter)
    uint64_t scatterCounter[TK::SIDES * TK::TYPES] = {};

    MonsoonTemasekExpander() {
        config(TK::NUM_PARAMS_TOTAL, TK::NUM_INPUTS, 0, TK::NUM_LIGHTS);
        // Configure momentary button params (NOT DAW-exposed; manual triggers only)
        for (int i = 0; i < TK::N_ROWS * 2; ++i)
            configButton(TK::BTN_START + i, (i % 2 == 0) ? "Domain trigger" : "Codomain trigger");

        // Grain knobs — DAW-exposed GENERATION params, latch under lock.
        static const char* verbNames[TK::N_VERBS] = {"Collapse","Rotate","Reflect","Scatter"};
        static const char* sideNames[TK::SIDES]   = {"Intra","Inter"};
        static const char* typeNames[TK::TYPES]   = {"Rhythm","Melody"};
        static const char* grainLabels[] = {"1","2","4","8","16"};
        for (int v = 0; v < TK::N_VERBS; ++v)
            for (int s = 0; s < TK::SIDES; ++s)
                for (int t = 0; t < TK::TYPES; ++t) {
                    int r = TK::rowId(v, s, t);
                    std::string stem = std::string(verbNames[v]) + " " +
                                       sideNames[s] + " " + typeNames[t];
                    configSwitch(TK::GRAIN_START + r, 0.f, 4.f, 2.f,
                        stem + " grain", {grainLabels[0], grainLabels[1],
                        grainLabels[2], grainLabels[3], grainLabels[4]});
                }

        // Leader knobs (Collapse rows only: N_ROWS/2 = 8)
        for (int r = 0; r < TK::N_ROWS / 2; ++r)
            configParam(TK::LEADER_START + r, 0.f, 15.f, 0.f,
                "Leader offset", "", 0.f, 1.f, 0.f)->snapEnabled = true;

        // Step knobs (Rotate rows only: N_ROWS/2 = 8)
        for (int r = 0; r < TK::N_ROWS / 2; ++r)
            configParam(TK::STEP_START + r, -7.f, 7.f, 1.f,
                "Step", "", 0.f, 1.f, 0.f)->snapEnabled = true;

        // Trigger jacks
        for (int r = 0; r < TK::N_ROWS; ++r) {
            configInput(TK::DOMAIN_TRIG_START   + r, "Domain trigger");
            configInput(TK::CODOMAIN_TRIG_START + r, "Codomain trigger");
        }
        // Grain CV + attenuverters (NOT DAW-exposed per §14b — will be de-parammed)
        for (int i = 0; i < TK::N_VERBS * TK::TYPES; ++i) {
            configInput(TK::GRAIN_CV_START    + i, "Grain CV");
            configInput(TK::GRAIN_ATTEN_START + i, "Grain attenuverter");
        }
        // Scatter fwd/back
        for (int i = 0; i < TK::SIDES * TK::TYPES; ++i) {
            configInput(TK::SCATTER_FWD_START  + i, "Scatter forward");
            configInput(TK::SCATTER_BACK_START + i, "Scatter back");
        }
    }

    // Latch a pending action from a manual button press (§14a: parameters captured NOW,
    // not re-read at the phrase boundary). Shared by the gate path in process().
    void latchFromButton(int row, bool isDomain) {
        if (row < 0 || row >= TK::N_ROWS) return;
        const int verb = row / 4;
        const int side = (row % 4) / 2;
        const int type = row % 2;
        auto& p    = pendingRows[row];
        p.armed    = true;
        p.isDomain = isDomain;
        p.isInter  = (side == 1);
        p.grain    = 1 << (int)std::round(
                         clamp(params[TK::GRAIN_START + row].getValue(), 0.f, 4.f));
        if (verb == TK::V_COLLAPSE)
            p.leaderOrStep = (int)std::round(
                params[TK::LEADER_START + side * TK::TYPES + type].getValue());
        else if (verb == TK::V_ROTATE)
            p.leaderOrStep = (int)std::round(
                params[TK::STEP_START + side * TK::TYPES + type].getValue());
        else
            p.leaderOrStep = 0;
        lights[TK::PENDING_LIGHT_START + row].setBrightness(1.f);
    }

    void process(const ProcessArgs&) override {
        // Detect triggers and latch into pendingRows.
        // Application happens in MonsoonExpanderManager at phrase boundary, exactly as the
        // current Change Alley transforms. (Not yet wired — expanderManager needs a
        // cachedTemasekExpander pointer and a applyTemasekPending() call.)

        for (int v = 0; v < TK::N_VERBS; ++v) {
            for (int s = 0; s < TK::SIDES; ++s) {
                for (int t = 0; t < TK::TYPES; ++t) {
                    const int r = TK::rowId(v, s, t);

                    if (domainTrig[r].process(
                            inputs[TK::DOMAIN_TRIG_START + r].getVoltage()))
                        latchFromButton(r, true);
                    if (codomainTrig[r].process(
                            inputs[TK::CODOMAIN_TRIG_START + r].getVoltage()))
                        latchFromButton(r, false);
                    // Buttons (momentary params, manual only)
                    if (btnTrig[r*2].process(params[TK::BTN_START + r*2].getValue()))
                        latchFromButton(r, true);
                    if (btnTrig[r*2+1].process(params[TK::BTN_START + r*2+1].getValue()))
                        latchFromButton(r, false);
                }
            }
        }

        // Scatter fwd/back — latch the scatter rows with appropriate delta
        for (int s = 0; s < TK::SIDES; ++s) {
            for (int t = 0; t < TK::TYPES; ++t) {
                const int i = s * TK::TYPES + t;
                const int r = TK::rowId(TK::V_SCATTER, s, t);
                for (int delta : {1, -1}) {
                    const int jIdx = (delta == 1 ? TK::SCATTER_FWD_START
                                                 : TK::SCATTER_BACK_START) + i;
                    auto& trig = (delta == 1 ? scatterFwd : scatterBack)[i];
                    if (trig.process(inputs[jIdx].getVoltage())) {
                        auto& p        = pendingRows[r];
                        p.armed        = true;
                        p.isInter      = (s == 1);
                        p.grain        = 1 << (int)std::round(
                            clamp(params[TK::GRAIN_START + r].getValue(), 0.f, 4.f));
                        p.scatterDelta = delta;
                        lights[TK::PENDING_LIGHT_START + r].setBrightness(1.f);
                    }
                }
            }
        }
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────
struct MonsoonTemasekExpanderWidget : ModuleWidget {

    static constexpr float PW_MM = 40.f * 5.08f;
    static constexpr float PH_MM = 128.5f;

    // Mirror geometry -- must match gen_temasek.py exactly
    static constexpr float MARGIN   =  5.0f;
    static constexpr float CX       = PW_MM / 2.f;
    static constexpr float N_ROWS_F = 8.f;
    static constexpr float ROW_H    = (PH_MM - 16.f) / N_ROWS_F;
    static constexpr float ROW_TOP  =  8.0f;
    static constexpr float J_OUTER  = MARGIN;
    static constexpr float BTN_D    = MARGIN +  8.5f;
    static constexpr float KNOB1    = MARGIN + 17.5f;
    static constexpr float KNOB2    = MARGIN + 27.0f;
    static constexpr float BTN_C    = MARGIN + 36.0f;
    static constexpr float J_INNER  = MARGIN + 44.5f;

    static float rowY(int verb, int sub) {
        return ROW_TOP + (verb * 2 + sub + 0.5f) * ROW_H;
    }
    static float lx(float x_mm, bool flip) {
        return flip ? PW_MM - x_mm : x_mm;
    }

    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;

    MonsoonTemasekExpanderWidget(MonsoonTemasekExpander* module) {
        setModule(module);
        std::string dark  = asset::plugin(pluginInstance, "res/panels/Temasek_panel_dark.svg");
        std::string light = asset::plugin(pluginInstance, "res/panels/Temasek_panel_light.svg");
        panelSvgDark  = APP->window->loadSvg(dark);
        panelSvgLight = APP->window->loadSvg(light);
        setPanel(Svg::load(dark));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5f,         1.5f))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 1.5f, 1.5f))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5f,         PH_MM - 1.5f))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 1.5f, PH_MM - 1.5f))));

        static const char* verbLabel[TK::N_VERBS] = {"C","Ro","Rf","Sc"};
        (void)verbLabel;

        for (int verb = 0; verb < TK::N_VERBS; ++verb) {
            for (int sub = 0; sub < 2; ++sub) {      // 0=R 1=M
                const float ry = rowY(verb, sub);
                for (int side = 0; side < 2; ++side) { // 0=intra 1=inter
                    const bool flip = (side == 1);

                    // Domain jack (outermost)
                    addInput(createInputCentered<PJ301MPort>(
                        mm2px(Vec(lx(J_OUTER, flip), ry)), module,
                        TK::DOMAIN_TRIG_START + TK::rowId(verb, side, sub)));

                    // Codomain jack
                    addInput(createInputCentered<PJ301MPort>(
                        mm2px(Vec(lx(J_INNER, flip), ry)), module,
                        TK::CODOMAIN_TRIG_START + TK::rowId(verb, side, sub)));

                    // Domain button (momentary -- NOT a param, sets the pending flag)
                    // Using TL1105 mapped to a param slot so Rack handles the press event;
                    // the param is never read for value, only for the trigger in process().
                    // (A proper non-param button widget is a future refinement.)
                    addParam(createParamCentered<TL1105>(
                        mm2px(Vec(lx(BTN_D, flip), ry)), module,
                        TK::BTN_START + TK::rowId(verb, side, sub) * 2));     // NOTE: overflow -- see below

                    // Codomain button
                    addParam(createParamCentered<TL1105>(
                        mm2px(Vec(lx(BTN_C, flip), ry)), module,
                        TK::BTN_START + TK::rowId(verb, side, sub) * 2 + 1));

                    // Grain knob (DAW-exposed)
                    addParam(createParamCentered<Trimpot>(
                        mm2px(Vec(lx(KNOB1, flip), ry)), module,
                        TK::GRAIN_START + TK::rowId(verb, side, sub)));

                    // Leader (Collapse) or step (Rotate) knob
                    if (verb == TK::V_COLLAPSE) {
                        addParam(createParamCentered<Trimpot>(
                            mm2px(Vec(lx(KNOB2, flip), ry)), module,
                            TK::LEADER_START + side * TK::TYPES + sub));
                    } else if (verb == TK::V_ROTATE) {
                        addParam(createParamCentered<Trimpot>(
                            mm2px(Vec(lx(KNOB2, flip), ry)), module,
                            TK::STEP_START + side * TK::TYPES + sub));
                    }

                    // Pending light
                    addChild(createLightCentered<SmallLight<RedLight>>(
                        mm2px(Vec(lx(CX - 3.0f, flip), ry)), module,
                        TK::PENDING_LIGHT_START + TK::rowId(verb, side, sub)));

                    // Scatter fwd/back jacks
                    if (verb == TK::V_SCATTER) {
                        const int i = side * TK::TYPES + sub;
                        const float fwd_y = ry - ROW_H * 0.22f;
                        const float bk_y  = ry + ROW_H * 0.22f;
                        addInput(createInputCentered<PJ301MPort>(
                            mm2px(Vec(lx(J_OUTER, flip), fwd_y)), module,
                            TK::SCATTER_FWD_START  + i));
                        addInput(createInputCentered<PJ301MPort>(
                            mm2px(Vec(lx(J_OUTER, flip), bk_y)),  module,
                            TK::SCATTER_BACK_START + i));
                    }
                }
            }
        }
    }

    void step() override {
        ModuleWidget::step();
        if (!module) return;
        Monsoon* m = redDot::findMonsoonEitherSide(module);
        const int wantLight = (m && m->lightTheme) ? 1 : 0;
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

// ── Momentary trigger button: NOT a param (§ buttons are manual-only) ───────
// A trigger has no value, no persistence and no undo -- it just arms a pending action.
// So none of StoreKnob's seven re-supplied services apply; this is a click handler with
// two draw states.
struct TemasekTrigButton : widget::Widget {
    MonsoonTemasekExpander* mod = nullptr;
    int  row      = 0;
    bool isDomain = true;
    bool pressed  = false;

    TemasekTrigButton() { box.size = mm2px(Vec(5.6f, 5.6f)); }

    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float r  = std::min(box.size.x, box.size.y) * 0.42f;
        // body
        nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
        nvgFillColor(vg, pressed ? nvgRGB(0x50,0x50,0x58) : nvgRGB(0x2e,0x2e,0x33));
        nvgFill(vg);
        // ring: domain vs codomain distinguished by ring style
        nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
        nvgStrokeColor(vg, isDomain ? nvgRGB(0xc8,0x96,0x0c) : nvgRGB(0x88,0x8d,0x96));
        nvgStrokeWidth(vg, isDomain ? 1.4f : 1.0f);
        nvgStroke(vg);
        Widget::draw(args);
    }

    void onButton(const event::Button& e) override {
        if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
            pressed = true;
            if (mod) mod->latchFromButton(row, isDomain);
            e.consume(this);
        }
        if (e.action == GLFW_RELEASE) pressed = false;
        Widget::onButton(e);
    }
    void onDragEnd(const event::DragEnd&) override { pressed = false; }
};

