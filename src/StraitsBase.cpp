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
#include <algorithm>   // std::sort
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
    std::vector<int64_t> orderedLaneIds_;  // owned lane module IDs, in dock order
    bool needsResnap = false;   // set by onDragEnd, cleared by step() (one-shot)
    bool initialized = false;   // false until the widget's first step runs

    json_t* dataToJson() override {
        json_t* rootJ = MonsoonStraitsExpander::dataToJson();
        if (!rootJ) rootJ = json_object();
        // Save the list of attached lane keys (walk rightExpander chain).
        json_t* lanesJ = json_array();
        // Save by owned ID list (not live chain) — robust to transient detachment
        for (int64_t id : orderedLaneIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* lane = dynamic_cast<StraitsLaneExpander*>(m);
                if (lane && lane->desc)
                    json_array_append_new(lanesJ, json_string(lane->desc->slug));
            }
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
    Vec lastBasePos_;             // detect base movement for one-shot re-snap

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

    // ── Context menu: lane presence checkboxes ───────────────────────────────
    // Lanes are UNIQUE (at most one each). Checkboxes toggle presence.
    // VARIATION/LEGATO shown but disabled (no engine params yet).
    bool hasLane(const std::string& slug) {
        auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
        if (!baseMod) return false;
        for (int64_t id : baseMod->orderedLaneIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* lane = dynamic_cast<StraitsLaneExpander*>(m);
                if (lane && lane->desc && lane->desc->slug == slug) return true;
            }
        }
        return false;
    }
    void removeLane(const std::string& slug) {
        auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
        if (!baseMod) return;
        for (size_t i = 0; i < baseMod->orderedLaneIds_.size(); ++i) {
            int64_t id = baseMod->orderedLaneIds_[i];
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* lane = dynamic_cast<StraitsLaneExpander*>(m);
                if (lane && lane->desc && lane->desc->slug == slug) {
                    ModuleWidget* lw = APP->scene->rack->getModule(id);
                    if (lw) APP->scene->rack->removeModule(lw);
                    APP->engine->removeModule(m);
                    baseMod->orderedLaneIds_.erase(baseMod->orderedLaneIds_.begin() + i);
                    resnapLanes();   // close the gap — reposition remaining lanes
                    return;
                }
            }
        }
    }
    void toggleLane(const std::string& slug) {
        if (hasLane(slug)) removeLane(slug);
        else spawnLane(slug);
    }
    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Lanes"));
        menu->addChild(createCheckMenuItem("REST", "",
            [this]() { return hasLane("StraitsLaneREST"); },
            [this]() { toggleLane("StraitsLaneREST"); }));
        menu->addChild(createCheckMenuItem("ACCENT", "",
            [this]() { return hasLane("StraitsLaneACCENT"); },
            [this]() { toggleLane("StraitsLaneACCENT"); }));
        menu->addChild(createCheckMenuItem("Q-MIX", "",
            [this]() { return hasLane("StraitsLaneQMIX"); },
            [this]() { toggleLane("StraitsLaneQMIX"); }));
        menu->addChild(createCheckMenuItem("VARIATION", "",
            [this]() { return hasLane("StraitsLaneVARIATION"); },
            [this]() { toggleLane("StraitsLaneVARIATION"); }));
        menu->addChild(createCheckMenuItem("LEGATO", "",
            [this]() { return hasLane("StraitsLaneLEGATO"); },
            [this]() { toggleLane("StraitsLaneLEGATO"); }));
    }

    // ── Spawn a lane expander module and dock it right ───────────────────────
    void spawnLane(const std::string& slug) {
        if (!module) return;
        // Find the Model for this slug
        Model* model = nullptr;
        if (slug == "StraitsLaneQMIX") model = modelStraitsLaneQMIX;
        else if (slug == "StraitsLaneREST") model = modelStraitsLaneREST;
        else if (slug == "StraitsLaneACCENT") model = modelStraitsLaneACCENT;
        else if (slug == "StraitsLaneVARIATION") model = modelStraitsLaneVARIATION;
        else if (slug == "StraitsLaneLEGATO") model = modelStraitsLaneLEGATO;
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

            // ── Adopt restored lanes + auto-spawn QMIX ───────────────────────────
            // Rack restores lane expanders natively (registered + hidden). The base
            // adopts them by walking the rightExpander chain and recording their IDs.
            // If no lanes are found (first creation), auto-spawn QMIX (default-attached).
            if (!lanesSpawned_) {
                auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
                if (baseMod) {
                    // Adopt lanes restored by Rack (walk rightExpander chain)
                    rack::Module* right = module->rightExpander.module;
                    while (right) {
                        auto* lane = dynamic_cast<StraitsLaneExpander*>(right);
                        if (!lane || !lane->desc) break;
                        baseMod->orderedLaneIds_.push_back(right->id);
                        right = right->rightExpander.module;
                    }
                    // Auto-spawn QMIX if no lanes attached (first creation, no save)
                    if (baseMod->orderedLaneIds_.empty())
                        spawnLane("StraitsLaneQMIX");
                }
                lanesSpawned_ = true;
            }

            // ── One-shot re-snap: fires ONLY when a needsResnap flag is set
            // by onDragEnd (base's own or a lane's). NOT per-frame — zero
            // box.pos writes in step() during drags → no flicker, no fighting.
            {
                auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
                bool needSnap = baseMod && baseMod->needsResnap;
                // Also check each owned lane's needsResnap flag
                if (baseMod && !needSnap) {
                    for (int64_t laneId : baseMod->orderedLaneIds_) {
                        rack::Module* m = APP->engine->getModule(laneId);
                        if (m) {
                            auto* lane = dynamic_cast<StraitsLaneExpander*>(m);
                            if (lane && lane->needsResnap) { needSnap = true; break; }
                        }
                    }
                }
                if (needSnap) {
                    if (baseMod) baseMod->needsResnap = false;
                    // Clear lane flags
                    for (int64_t laneId : baseMod->orderedLaneIds_) {
                        rack::Module* m = APP->engine->getModule(laneId);
                        if (m) {
                            auto* lane = dynamic_cast<StraitsLaneExpander*>(m);
                            if (lane) lane->needsResnap = false;
                        }
                    }
                    resnapLanes();
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

    // ── Re-snap helper: sort by canonical order, position via Rack's API ──────
    // Uses setModulePosForce (Rack's placement-with-shoving) instead of raw
    // box.pos — pushes other modules aside, no overlap.
    void resnapLanes() {
        auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
        if (!baseMod) return;
        std::sort(baseMod->orderedLaneIds_.begin(), baseMod->orderedLaneIds_.end(),
            [&](int64_t a, int64_t b) {
                auto* ma = dynamic_cast<StraitsLaneExpander*>(APP->engine->getModule(a));
                auto* mb = dynamic_cast<StraitsLaneExpander*>(APP->engine->getModule(b));
                int oa = (ma && ma->desc) ? ma->desc->arcLane : 99;
                int ob = (mb && mb->desc) ? mb->desc->arcLane : 99;
                return oa < ob;
            });
        float expectedX = box.getTopRight().x;
        for (int64_t laneId : baseMod->orderedLaneIds_) {
            ModuleWidget* lw = APP->scene->rack->getModule(laneId);
            if (!lw) continue;
            // Use Rack's placement API (shoves neighbours, no overlap)
            APP->scene->rack->setModulePosForce(lw, Vec(expectedX, box.pos.y));
            expectedX = lw->box.getTopRight().x;
        }
    }

    // ── onDragEnd: set the resnap flag (fires once on drag completion) ──────────
    void onDragEnd(const event::DragEnd& e) override {
        ModuleWidget::onDragEnd(e);
        if (auto* baseMod = dynamic_cast<StraitsBaseModule*>(module))
            baseMod->needsResnap = true;
    }
};

Model* modelStraitsBase =
    createModel<StraitsBaseModule, StraitsBaseWidget>("StraitsBase");
