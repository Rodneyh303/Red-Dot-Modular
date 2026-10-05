// StraitsBase — the frame + IO + voice count, NO lane knobs.
// This is a REFACTOR of the working Straits widget (MonsoonStraitsExpander.cpp):
// the IO bindings, voice-count knob, connect mark, and step() logic are COPIED
// from MonsoonStraitsExpanderWidget — only the lane knob bindings are removed.
// The base module IS a StraitsStraitsExpander subclass (same config, same params,
// same IO). Lane expanders are UNREGISTERED — the base spawns them via context menu
// and owns persistence (save/load the lane list).
//
// See docs/design/STRAITS_CAUSEWAY_LANE_EXTENSIONS.md REFINED MODEL + METHOD:REFACTOR.
#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitsExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/Controls.hpp"
#include "StraitsLaneExpander.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitsIds;

// ── StraitsBaseModule — subclass with lane persistence ───────────────────────
// Adds dataToJson/dataFromJson to save/restore the list of attached lane
// expanders. Since lane expanders are unregistered, Rack won't auto-restore
// them — the base re-spawns them on load.
struct StraitsBaseModule : MonsoonStraitsExpander {
    // Lane keys to spawn on next step (set by dataFromJson or context menu).
    // e.g. "qmix", "rest", "accent". Processed in the widget's step().
    std::vector<std::string> pendingLanes;
    std::vector<int64_t> orderedLaneIds_;  // owned lane module IDs, in dock order (for force-follow)
    bool initialized = false;   // false until the widget's first step runs

    json_t* dataToJson() override {
        json_t* rootJ = MonsoonStraitsExpander::dataToJson();
        if (!rootJ) rootJ = json_object();
        // Save the list of attached lane keys (walk rightExpander chain).
        json_t* lanesJ = json_array();
        rack::Module* right = rightExpander.module;
        while (right) {
            auto* lane = dynamic_cast<StraitsLaneExpander*>(right);
            if (!lane || !lane->desc) break;
            json_array_append_new(lanesJ, json_string(lane->desc->slug));
            right = right->rightExpander.module;
        }
        json_object_set_new(rootJ, "straitLanes", lanesJ);
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        MonsoonStraitsExpander::dataFromJson(rootJ);
        json_t* lanesJ = json_object_get(rootJ, "straitLanes");
        if (lanesJ) {
            size_t i; json_t* v;
            json_array_foreach(lanesJ, i, v) {
                const char* s = json_string_value(v);
                if (s) pendingLanes.push_back(s);
            }
        }
    }
};

struct StraitsBaseWidget : ModuleWidget,
    dotModular::Compose<StraitsBaseWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    bool themeLight_ = false;
    int activeVoices_ = -1;
    bool voiceCountSynced_ = false;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;
    bool lanesSpawned_ = false;   // false until auto-spawn/pending-spawn runs

    StraitsBaseWidget(StraitsBaseModule* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/StraitsBase_panel_dark.svg";
        const char* lightPath = "res/panels/StraitsBase_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── IO bindings (COPIED from MonsoonStraitsExpanderWidget, unchanged) ──
        bindOutput<PJ301MPort>("output_polygate",     POLY_GATE_OUT);
        bindOutput<PJ301MPort>("output_polystepgate", POLY_STEP_GATE_OUT);
        bindOutput<PJ301MPort>("output_polyslegato",  POLY_STEP_LEGATO_GATE_OUT);
        bindOutput<PJ301MPort>("output_polycv",       POLY_CV_OUT);
        bindOutput<PJ301MPort>("output_polyaccent",   POLY_ACCENT_OUT);
        bindInput<PJ301MPort>("input_quantcv", StraitsIds::QUANT_CV_INPUT);

        // Voice-count knob (COPIED from MonsoonStraitsExpanderWidget)
        bindParam<redDot::Themed_Trim_Slot>("param_voicecount", StraitsIds::VOICE_COUNT_PARAM,
            std::function<void(redDot::Themed_Trim_Slot*)>([this](redDot::Themed_Trim_Slot* k){
                k->lightWhen = [this](){ return themeLight_; };
            }));

        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            addChild(connectMark);
        }
    }

    // ── Context menu: "Add Lane >" ───────────────────────────────────────────
    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Lanes"));
        menu->addChild(createMenuItem("Add Q-MIX Lane", "",
            [this]() { spawnLane("StraitsLaneQMIX"); }));
    }

    // ── Spawn a lane expander module and dock it right ───────────────────────
    void spawnLane(const std::string& slug) {
        if (!module) return;
        // Find the Model for this slug
        Model* model = nullptr;
        if (slug == "StraitsLaneQMIX") model = modelStraitsLaneQMIX;
        if (!model) return;

        // Create the module
        engine::Module* laneMod = model->createModule();
        APP->engine->addModule(laneMod);

        // Record the lane's module ID for force-follow (walk by ID, not live chain)
        if (auto* baseMod = dynamic_cast<StraitsBaseModule*>(module))
            baseMod->orderedLaneIds_.push_back(laneMod->id);

        // Create the widget
        ModuleWidget* laneW = model->createModuleWidget(laneMod);

        // Position: right of the rightmost owned lane (or base if none)
        float rightX = box.getTopRight().x;
        if (auto* baseMod = dynamic_cast<StraitsBaseModule*>(module)) {
            for (int64_t id : baseMod->orderedLaneIds_) {
                if (id == laneMod->id) continue;  // skip self (just added)
                ModuleWidget* lw = APP->scene->rack->getModule(id);
                if (lw) rightX = std::max(rightX, lw->box.getTopRight().x);
            }
        }
        laneW->box.pos = Vec(rightX, box.pos.y);

        // Add to rack
        APP->scene->rack->addModule(laneW);
    }

    void step() override {
        // ── step() logic COPIED from MonsoonStraitsExpanderWidget, plus lane sync ──
        if (module) {
            Monsoon* mm = redDot::findMonsoonEitherSide(module);
            themeLight_ = (mm && mm->lightTheme);
            if (mm) {
                if (!voiceCountSynced_) {
                    float knob = (float)(mm->engine.numPolyVoices + 1);
                    module->params[StraitsIds::VOICE_COUNT_PARAM].setValue(knob);
                    voiceCountSynced_ = true;
                }
                int want = (int)module->params[StraitsIds::VOICE_COUNT_PARAM].getValue() - 1;
                if (want < 0) want = 0; else if (want > 15) want = 15;
                mm->engine.numPolyVoices = want;
                activeVoices_ = want;
            } else {
                activeVoices_ = -1;
                voiceCountSynced_ = false;
            }

            // ── Lane expander param sync ────────────────────────────────────────
            // Walk the right-neighbour chain and sync each lane expander's params
            // into the base's params. The engine reads from the base (as
            // cachedPolyVoiceExpander), so lane knob edits reach the engine here.
            rack::Module* right = module->rightExpander.module;
            while (right) {
                auto* lane = dynamic_cast<StraitsLaneExpander*>(right);
                if (!lane || !lane->desc) break;
                module->params[lane->desc->monoParamId].setValue(
                    right->params[lane->desc->monoParamId].getValue());
                for (int i = 0; i < 15; i++) {
                    module->params[lane->desc->polyParamIdBase + i].setValue(
                        right->params[lane->desc->polyParamIdBase + i].getValue());
                }
                right = right->rightExpander.module;
            }

            // ── Auto-spawn / restore lanes ──────────────────────────────────────
            if (!lanesSpawned_) {
                auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
                if (baseMod) {
                    // Restore saved lanes (from dataFromJson)
                    for (const auto& slug : baseMod->pendingLanes)
                        spawnLane(slug);
                    baseMod->pendingLanes.clear();

                    // Auto-spawn QMIX if no lanes attached (default-attached)
                    if (!module->rightExpander.module ||
                        !dynamic_cast<StraitsLaneExpander*>(module->rightExpander.module)) {
                        spawnLane("StraitsLaneQMIX");
                    }
                }
                lanesSpawned_ = true;
            }

            // ── Force-follow: keep lane expanders welded to base's right ──────────
            // Walk the OWNED lane ID list (not the live rightExpander chain — a lane
            // dragged away is no longer adjacent, so the chain would miss it). Each owned
            // lane is snapped to its computed slot, regardless of current position.
            // Reposition only when out of place (>0.5px) to avoid jitter.
            {
                auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
                if (baseMod) {
                    float expectedX = box.getTopRight().x;
                    for (int64_t laneId : baseMod->orderedLaneIds_) {
                        ModuleWidget* lw = APP->scene->rack->getModule(laneId);
                        if (!lw) continue;
                        Vec target(expectedX, box.pos.y);
                        if (lw->box.pos.minus(target).norm() > 0.5f)
                            lw->box.pos = target;
                        expectedX = lw->box.getTopRight().x;
                    }
                }
            }
        }
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
};

Model* modelStraitsBase =
    createModel<StraitsBaseModule, StraitsBaseWidget>("StraitsBase");
