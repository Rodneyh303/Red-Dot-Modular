#include <rack.hpp>
#include "Monsoon.hpp"
#include "Intertropical.hpp"
#include "MonsoonChangiT3Expander.hpp"
#include "ui/VisualExpanderHelpers.hpp"     // redDot::findMonsoonEitherSide
#include "ui/IntertropicalPairing.hpp"      // resolveFollowedIT, presentPairIds
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
using namespace ChangiT3Ids;

// ── process(): self-bind to the followed Intertropical and MIRROR its 5 output jacks
//    per channel into T3's 40 by-channel jacks. Intertropical maps STEP->LEGATO_OUT and
//    STEP-LEGATO->SLEG_OUT (see Intertropical::process), so we read those two for STEP/SLEG.
//    CV is already post-transpose + tie-latched inside Intertropical. ──
void MonsoonChangiT3Expander::process(const ProcessArgs& args) {
    Intertropical* it = redDot::resolveFollowedIT(this, followIT);
    if (!it) {
        for (int o = 0; o < ChangiT3Ids::NUM_OUTPUTS; ++o) outputs[o].setVoltage(0.f);
        return;
    }
    using ITo = Intertropical::Ids;
    for (int ch = 0; ch < N_CH; ++ch) {
        outputs[idx(ch, GATE)].setVoltage(       it->outputs[ITo::GATE_OUT].getVoltage(ch));
        outputs[idx(ch, CV)].setVoltage(         it->outputs[ITo::CV_OUT].getVoltage(ch));
        outputs[idx(ch, ACCENT)].setVoltage(     it->outputs[ITo::ACCENT_OUT].getVoltage(ch));
        outputs[idx(ch, STEP)].setVoltage(       it->outputs[ITo::LEGATO_OUT].getVoltage(ch));  // STEP gate
        outputs[idx(ch, STEP_LEGATO)].setVoltage(it->outputs[ITo::SLEG_OUT].getVoltage(ch));     // STEP legato
    }
}

// ============================ WIDGET ============================

struct MonsoonChangiT3ExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonChangiT3ExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonChangiT3ExpanderWidget(MonsoonChangiT3Expander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/ChangiT3_panel_dark.svg";
        const char* lightPath = "res/panels/ChangiT3_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // 8 channels × 5 signals, bound by panel markers output_ch<ch>_<signal>.
        static const char* SIG[PER_CH] = {"gate", "cv", "accent", "step", "stepleg"};
        static const int   SIGID[PER_CH] = {GATE, CV, ACCENT, STEP, STEP_LEGATO};
        for (int ch = 0; ch < N_CH; ++ch) {
            for (int s = 0; s < PER_CH; ++s) {
                std::string marker = "output_ch" + std::to_string(ch) + "_" + SIG[s];
                bindOutput<PJ301MPort>(marker, idx(ch, SIGID[s]));
            }
        }

        // Connect mark: T3 is an OBSERVER (like Intertropical/Lantern), so light on
        // Intertropical REACHABILITY (via the follow setting), not expander-claim.
        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            connectMark->connected = [mod]() {
                return mod && redDot::resolveFollowedIT(mod, mod->followIT) != nullptr;
            };
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

    void appendContextMenu(Menu* menu) override {
        auto* m = dynamic_cast<MonsoonChangiT3Expander*>(module);
        if (!m) return;
        menu->addChild(new MenuSeparator);
        std::vector<int> ids = redDot::presentPairIds();
        menu->addChild(createSubmenuItem("Follow Intertropical",
            (m->followIT == 0) ? "Auto" : rack::string::f("#%d", m->followIT),
            [m, ids](Menu* sub) {
                sub->addChild(createCheckMenuItem("Auto (nearest)", "",
                    [m]() { return m->followIT == 0; },
                    [m]() { m->followIT = 0; }));
                for (int id : ids) {
                    sub->addChild(createCheckMenuItem(rack::string::f("Intertropical #%d", id), "",
                        [m, id]() { return m->followIT == id; },
                        [m, id]() { m->followIT = id; }));
                }
            }));
    }
};

Model* modelMonsoonChangiT3Expander =
    createModel<MonsoonChangiT3Expander, MonsoonChangiT3ExpanderWidget>("ChangiT3");
