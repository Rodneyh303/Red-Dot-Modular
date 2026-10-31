// StraitsBase — the ONE base surface: frame + IO + voice count, the lane run's
// owner, AND (CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE) the subgate consumer
// group's GLOBAL tier — the 8 global knobs + their mod jacks, the source
// switches, and the subgate-panel wheel. The retired StraitsSubgateBase's
// module logic, menu, and wheel are folded HERE (one base owns poly/outputs/
// snapping/voice-select; the subgate tier joins it); the per-voice panels
// (StraitsSubgatePanel.cpp) attach to THIS base, docking after the lane run.
// Lane expanders are base-spawned; the base owns persistence for both lists.
//
// See docs/design/STRAITS_CAUSEWAY_LANE_EXTENSIONS.md REFINED MODEL + METHOD:REFACTOR.
#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitsExpander.hpp"
#include "ui/ModuleLookup.hpp"   // THE module-lookup authority: lookupMonsoon — memoized, event-driven (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START)
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/Controls.hpp"
#include <algorithm>   // std::sort
#include <atomic>      // std::atomic — the resnap fast-path flags cross engine → UI threads
#include <cmath>       // std::fabs — the subgate global-delta publish
#include "StraitsLaneExpander.hpp"
#include "StraitsSubgatePanel.hpp"  // SUBGATE MERGE: the subgate panel module + ids (the base module is retired)
#include "ui/StraitsSeamFiller.hpp"
#include "ui/StoreBound.hpp"        // bindStoreKnob — the de-param law (the subgate globals)
#include "ui/StraitsSubgateArt.hpp" // addSubgateCaption + subgateTint — the merged panel's lettering

using namespace rack;

// Helper: is this module a compatible StraitsR panel (for seam detection)?
// SUBGATE MERGE: the subgate per-voice panels joined the chain family — they
// dock after the lane run, so the base's right neighbour can be one.
static bool isStraitsRPanel(rack::Module* m) {
    if (!m) return false;
    return m->model == modelStraitsBase
        || m->model == modelStraitsLaneQMIX
        || m->model == modelStraitsLaneREST
        || m->model == modelStraitsLaneACCENT
        || m->model == modelStraitsLaneVARIATION
        || m->model == modelStraitsLaneLEGATO
        || m->model == modelStraitsSubgatePanelRatchetRest
        || m->model == modelStraitsSubgatePanelRatchetLegato
        || m->model == modelStraitsSubgatePanelRatchetAccent
        || m->model == modelStraitsSubgatePanelRatchetAdvance
        || m->model == modelStraitsSubgatePanelGhostRest
        || m->model == modelStraitsSubgatePanelGhostLegato
        || m->model == modelStraitsSubgatePanelGhostAccent
        || m->model == modelStraitsSubgatePanelGhostAdvance;
}
using namespace MonsoonIds;
using namespace StraitsIds;
namespace SKns = dotModular::SubgateKnobs;   // SUBGATE MERGE — the globals' tier constants

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
    // SUBGATE MERGE (CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE): the subgate
    // panel list (the straitLanes shape) + the global-delta publish shadow.
    std::vector<std::string> pendingSubgates_;   // panel slugs to spawn on the widget's first step (restore)
    std::vector<int64_t> orderedSubgateIds_;     // owned subgate panel IDs, in dock order
    float lastGlob_[SKns::LEVELS][SKns::DECS] = {};   // change-gated publishing; audio thread only
    // RESNAP FAST PATH (CC_PROMPT_STRAITS_RESNAP_HARDENING): set by onExpanderChange —
    // Rack's own adjacency-change signal (the head-adjacent insertion event no drag-end
    // covers). Only marks DIRTY; the exact chainIntact() evaluation runs UI-side in the
    // widget's step (orderedLaneIds_ is UI-thread data). Relaxed atomic (the
    // topologyEpoch house pattern) — the writer may be the engine thread.
    std::atomic<bool> topologyDirty{false};

    // SUBGATE MERGE: the 8 global mod inputs live in the SHARED StraitsIds
    // namespace (StraitsIds::SG_MOD_0) — Rack's config() may be called ONCE per
    // module (the re-call from the first attempt ASSERTED — Rodney's Rack run
    // caught it). The parent's single config() already sized them; this ctor
    // only adds the tooltips (the Causeway mod path's labels — the captions are
    // the one authority shared with the retired base's strips).
    StraitsBaseModule() {
        for (int l = 0; l < SKns::LEVELS; ++l)
            for (int d = 0; d < SKns::DECS; ++d)
                configInput(StraitsIds::SG_MOD_0 + l * SKns::DECS + d,
                    std::string(SKns::LEVEL_CAPTIONS[l]) + " " + SKns::DEC_CAPTIONS[d]
                    + " mod (±10V → ±1 threshold)");
    }

    // THE one process (the merge's poly/output clause): the parent's is empty —
    // Monsoon writes the poly-cable outputs via the cached pointer — so this
    // override carries the subgate GLOBAL-delta publish + heartbeat slot 0 (the
    // retired base's process, verbatim: change-gated, publish-overwrites; the
    // heartbeat vouches for liveness and MONSOON's watchdog drops the plane if
    // it freezes — publisher gone).
    void process(const ProcessArgs& args) override {
        MonsoonStraitsExpander::process(args);
        Monsoon* m = redDot::lookupMonsoon(this);
        if (!m) return;   // isolated: heartbeat freezes → the watchdog drops what we last published
        for (int l = 0; l < SKns::LEVELS; ++l)
            for (int d = 0; d < SKns::DECS; ++d) {
                const int id = SG_MOD_0 + l * SKns::DECS + d;
                const float delta = inputs[id].isConnected() ? inputs[id].getVoltage() * 0.1f : 0.f;
                if (std::fabs(delta - lastGlob_[l][d]) > 1e-6f) {
                    m->setSubgateGlobalDelta(l, d, delta);
                    lastGlob_[l][d] = delta;
                }
            }
        ++m->editor.subgate.publishSeq[0];
    }

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
        // SUBGATE MERGE: the owned subgate panel list (the same shape; the
        // retired base's "subgatePanels" key, so its saves restore here).
        json_t* sgJ = json_array();
        for (int64_t id : orderedSubgateIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
                if (panel)
                    json_array_append_new(sgJ,
                        json_string(StraitsSubgateIds::panelSlug(panel->level, panel->dec)));
            }
        }
        json_object_set_new(rootJ, "subgatePanels", sgJ);
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
        // SUBGATE MERGE: the subgate panel slugs (spawned on the widget's first
        // step; legacy per-LEVEL slugs parse false and skip gracefully).
        json_t* sgJ = json_object_get(rootJ, "subgatePanels");
        if (sgJ) {
            size_t i; json_t* v;
            json_array_foreach(sgJ, i, v) {
                const char* s = json_string_value(v);
                if (s) pendingSubgates_.push_back(s);
            }
        }
    }

    // RESNAP FAST PATH: Rack fires this after an expander is added/removed/changed on
    // either side of us. Chains the base-class epoch bump (the lookup authority).
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        MonsoonStraitsExpander::onExpanderChange(e);
        topologyDirty.store(true, std::memory_order_relaxed);
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
    double lastAdjacencyCheck_ = -1.0;   // periodic chain-poll cadence (system::getTime)
    redDot::StraitsSeamFiller* leftFiller = nullptr;
    redDot::StraitsSeamFiller* rightFiller = nullptr;

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

        // ── Seam fillers (runtime draw-over for seamless abutment) ──
        float rulePx = 1.2f * 75.f / 25.4f;
        leftFiller = new redDot::StraitsSeamFiller();
        leftFiller->isLeft = true;
        leftFiller->box.pos = Vec(-2, 0);
        leftFiller->box.size.y = box.size.y;
        leftFiller->ruleHeightPx = rulePx;
        addChild(leftFiller);
        rightFiller = new redDot::StraitsSeamFiller();
        rightFiller->isLeft = false;
        rightFiller->box.pos = Vec(box.size.x - 2, 0);
        rightFiller->box.size.y = box.size.y;
        rightFiller->ruleHeightPx = rulePx;
        addChild(rightFiller);

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

        // ── SUBGATE MERGE: the 8 GLOBAL store knobs + their mod jacks ──────────
        // (the retired StraitsSubgateBaseWidget's binds — the Causeway mod coverage
        // law: each knob ships with its jack, born together). BIPOLAR SCOPE (the
        // CC_PROMPT_STEP_ADVANCE_FREEZE_AND_BIPOLAR update): the ADVANCE columns are
        // ALWAYS bipolar — single-use (step-advance only; no clock/phase meaning to
        // protect): [-1,+1] centered at 0 (hold; + = forward advance prob, -
        // = backward). REST/LEGATO/ACCENT stay [0,1].
        {
            auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
            for (int l = 0; l < SKns::LEVELS; ++l)
                for (int d = 0; d < SKns::DECS; ++d) {
                    const bool isAdv = (d == SKns::ADVANCE);
                    const std::string cap = std::string(SKns::LEVEL_CAPTIONS[l]) + " " + SKns::DEC_CAPTIONS[d];
                    redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(
                        this, "param_sbg_" + std::to_string(l) + "_" + std::to_string(d),
                        resolveMon, isAdv ? -1.f : 0.f, isAdv ? +1.f : 1.f,
                        isAdv ? 0.f : SKns::kDefaultThreshold,
                        "Subgate " + cap + (isAdv ? " (global, bipolar)" : " (global)"),
                        [l, d](Monsoon& m) { return m.getSubgateGlobal(l, d); },
                        [l, d](Monsoon& m, float x) { m.setSubgateGlobal(l, d, x); });
                    bindInput<PJ301MPort>("mod_sbg_" + std::to_string(l) + "_" + std::to_string(d),
                                          StraitsIds::SG_MOD_0 + l * SKns::DECS + d);
                }
        }

        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            addChild(connectMark);
        }

        // ── SUBGATE MERGE lettering (the Cantonment idiom; coordinates match
        // gen_straits_base.py's merged layout: W=81.28, MARGIN_L=17, COL_PITCH
        // 14.82, knob rows 30/48, mod rows 63/77, voice 91, IO 111.1). The
        // "MOD" label moves to the GUTTER (the centre band now carries the
        // voice knob); "VOICES" labels it from below.
        {
            constexpr float Wmm = 81.28f, MARGIN_L = 17.0f, COL_PITCH = 59.28f / 4.f;
            auto ink = [this]() { return themeLight_ ? nvgRGB(0x30, 0x34, 0x3a) : nvgRGB(0xc8, 0xcd, 0xd4); };
            redDot::addSubgateCaption(this, "SUBGATE", Wmm * 0.5f, 16.5f, 9.f, ink);
            for (int d = 0; d < SKns::DECS; ++d)
                redDot::addSubgateCaption(this, SKns::DEC_CAPTIONS[d], MARGIN_L + COL_PITCH * (d + 0.5f), 21.0f, 6.f, ink);
            for (int l = 0; l < SKns::LEVELS; ++l)
                redDot::addSubgateCaption(this, SKns::LEVEL_CAPTIONS[l], 8.8f, 30.f + 18.f * l, 7.f,
                                  [l]() { return redDot::subgateTint(l); });
            redDot::addSubgateCaption(this, "MOD", 8.8f, 70.f, 6.f, ink);
            redDot::addSubgateCaption(this, "VOICES", Wmm * 0.5f, 101.f, 6.f, ink);
        }
    }

    Monsoon* mon() { return module ? redDot::lookupMonsoon(module) : nullptr; }

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
                    resnapRun();   // close the gap — reposition the remaining run
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

        // ── SUBGATE MERGE: the source switches + the per-voice panels + clear ──
        // (the retired StraitsSubgateBaseWidget's menu, verbatim.)
        Monsoon* mm = mon();
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Subgate source (when no own subpanel)"));
        if (mm) {
            // The per-decision SWITCH (the spec's set-and-forget routing choice):
            // tier 2a global vs tier 2b follow-main. Default FOLLOW MAIN (the
            // least-surprising default — the subgate inherits the main lane until
            // the user opts to a global subgate value or attaches a subpanel).
            // Attached always wins (tier 1) — no switch needed.
            for (int l = 0; l < SKns::LEVELS; ++l) {
                menu->addChild(createMenuLabel(SKns::LEVEL_CAPTIONS[l]));
                for (int d = 0; d < SKns::DECS; ++d) {
                    const std::string cap = std::string(SKns::DEC_CAPTIONS[d]);
                    menu->addChild(createCheckMenuItem(cap + " — follow main", "",
                        [this, l, d]() { Monsoon* m2 = mon(); return m2 ? m2->isSubgateFollowMain(l, d) : true; },
                        [this, l, d]() { if (auto* m2 = mon()) m2->setSubgateFollowMain(l, d, true); }));
                    menu->addChild(createCheckMenuItem(cap + " — global knob", "",
                        [this, l, d]() { Monsoon* m2 = mon(); return m2 ? !m2->isSubgateFollowMain(l, d) : false; },
                        [this, l, d]() { if (auto* m2 = mon()) m2->setSubgateFollowMain(l, d, false); }));
                }
            }
        } else {
            menu->addChild(createMenuLabel("(no Monsoon attached)"));
        }
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Subgate per-voice panels"));
        for (int l = 0; l < SKns::LEVELS; ++l) {
            menu->addChild(createMenuLabel(SKns::LEVEL_CAPTIONS[l]));
            for (int d = 0; d < SKns::DECS; ++d) {
                menu->addChild(createCheckMenuItem(
                    std::string("   ") + SKns::DEC_CAPTIONS[d] + " panel", "",
                    [this, l, d]() { return hasSubgatePanel(l, d); },
                    [this, l, d]() { toggleSubgatePanel(l, d); }));
            }
        }
        menu->addChild(new MenuSeparator);
        // The menu's un-claim path (the touch-is-claim law's complement).
        menu->addChild(createMenuItem("Clear per-voice overrides", "",
            [this]() { if (auto* m2 = mon()) m2->clearSubgateOwnAll(); }));
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

    // ── SUBGATE MERGE: the subgate-panel wheel (the retired StraitsSubgateBase's,
    // family-parameterised to the merged base — the StraitsLane law) ────────────
    static Model* subgatePanelModel(int level, int dec) {
        switch (level * SKns::DECS + dec) {
            case SKns::RATCHET * SKns::DECS + 0: return modelStraitsSubgatePanelRatchetRest;
            case SKns::RATCHET * SKns::DECS + 1: return modelStraitsSubgatePanelRatchetLegato;
            case SKns::RATCHET * SKns::DECS + 2: return modelStraitsSubgatePanelRatchetAdvance;
            case SKns::RATCHET * SKns::DECS + 3: return modelStraitsSubgatePanelRatchetAccent;
            case SKns::GHOST   * SKns::DECS + 0: return modelStraitsSubgatePanelGhostRest;
            case SKns::GHOST   * SKns::DECS + 1: return modelStraitsSubgatePanelGhostLegato;
            case SKns::GHOST   * SKns::DECS + 2: return modelStraitsSubgatePanelGhostAdvance;
            case SKns::GHOST   * SKns::DECS + 3: return modelStraitsSubgatePanelGhostAccent;
        }
        return nullptr;
    }
    static bool sameSubgatePanel(StraitsSubgatePanelModule* p, int level, int dec) {
        return p && p->level == level && p->dec == dec;
    }
    bool hasSubgatePanel(int level, int dec) {
        auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
        if (!baseMod) return false;
        for (int64_t id : baseMod->orderedSubgateIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
                if (sameSubgatePanel(panel, level, dec)) return true;
            }
        }
        return false;
    }
    void spawnSubgatePanel(int level, int dec) {
        if (!module) return;
        Model* model = subgatePanelModel(level, dec);
        if (!model) return;
        // spawnLane's verbatim order: engine module FIRST, ID pushed, widget
        // created, box.pos set BEFORE the rack addModule → addModule preserves
        // pos + runs updateExpanders → LINKED FROM BIRTH.
        engine::Module* panelMod = model->createModule();
        APP->engine->addModule(panelMod);
        if (auto* baseMod = dynamic_cast<StraitsBaseModule*>(module))
            baseMod->orderedSubgateIds_.push_back(panelMod->id);
        ModuleWidget* panelW = model->createModuleWidget(panelMod);
        // Position: the END of the whole run — right of the rightmost owned lane
        // OR panel (the dock order: lanes first, then subgate panels).
        float rightX = box.getTopRight().x;
        if (auto* baseMod = dynamic_cast<StraitsBaseModule*>(module)) {
            for (int64_t id : baseMod->orderedLaneIds_) {
                ModuleWidget* w = APP->scene->rack->getModule(id);
                if (w) rightX = std::max(rightX, w->box.getTopRight().x);
            }
            for (int64_t id : baseMod->orderedSubgateIds_) {
                if (id == panelMod->id) continue;   // skip self (just added)
                ModuleWidget* w = APP->scene->rack->getModule(id);
                if (w) rightX = std::max(rightX, w->box.getTopRight().x);
            }
        }
        panelW->box.pos = Vec(rightX, box.pos.y);
        APP->scene->rack->addModule(panelW);
    }
    void removeSubgatePanel(int level, int dec) {
        auto* baseMod = dynamic_cast<StraitsBaseModule*>(module);
        if (!baseMod) return;
        for (size_t i = 0; i < baseMod->orderedSubgateIds_.size(); ++i) {
            int64_t id = baseMod->orderedSubgateIds_[i];
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
                if (sameSubgatePanel(panel, level, dec)) {
                    ModuleWidget* pw = APP->scene->rack->getModule(id);
                    if (pw) APP->scene->rack->removeModule(pw);
                    APP->engine->removeModule(m);
                    baseMod->orderedSubgateIds_.erase(baseMod->orderedSubgateIds_.begin() + i);
                    resnapRun();   // close the gap
                    return;
                }
            }
        }
    }
    void toggleSubgatePanel(int level, int dec) {
        if (hasSubgatePanel(level, dec)) removeSubgatePanel(level, dec);
        else spawnSubgatePanel(level, dec);
    }

    void step() override {
        // ── step() logic COPIED from MonsoonStraitsExpanderWidget, plus lane sync ──
        if (module) {
            Monsoon* mm = redDot::lookupMonsoon(module);
            themeLight_ = (mm && mm->lightTheme);
            // ── Seam filler: toggle based on neighbour detection ──
            NVGcolor bg = themeLight_ ? nvgRGB(0xdc, 0xdc, 0xdc) : nvgRGB(0x14, 0x17, 0x1b);
            if (leftFiller)  { leftFiller->active = isStraitsRPanel(module->leftExpander.module);  leftFiller->bgColour = bg; }
            if (rightFiller) { rightFiller->active = isStraitsRPanel(module->rightExpander.module); rightFiller->bgColour = bg; }
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
                // RIGHT-SIZE remap (CC_PROMPT_LANE_EXPANDER_RIGHTSIZE): READ the
                // lane's LOCAL ids, WRITE the base's MonsoonIds slots — the engine's
                // read target is the base (unchanged), so the values arrive
                // bit-identically; only the lane-side addressing moved.
                module->params[lane->desc->monoParamId].setValue(
                    right->params[StraitsLaneIds::MONO_MIRROR_PARAM].getValue());
                for (int i = 0; i < 15; i++) {
                    module->params[lane->desc->polyParamIdBase + i].setValue(
                        right->params[StraitsLaneIds::POLY_PARAM_1 + i].getValue());
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
                    // SUBGATE MERGE: adopt the subgate panels (the run continues
                    // right of the lanes — the SAME walk cursor), then spawn any
                    // pending restore slugs (legacy per-LEVEL slugs parse false
                    // and skip). No auto-spawn: the per-voice tier is OPT-IN.
                    while (right) {
                        auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(right);
                        if (!panel) break;
                        baseMod->orderedSubgateIds_.push_back(right->id);
                        right = right->rightExpander.module;
                    }
                    for (const std::string& slug : baseMod->pendingSubgates_) {
                        int l, d;
                        if (!StraitsSubgateIds::parsePanelSlug(slug, l, d)) continue;
                        if (!hasSubgatePanel(l, d)) spawnSubgatePanel(l, d);
                    }
                    baseMod->pendingSubgates_.clear();
                }
                lanesSpawned_ = true;
            }

            // ── Resnap triggers (CC_PROMPT_STRAITS_RESNAP_HARDENING) ──────────────
            // FAST PATH — event-driven (~1 frame): the base's onExpanderChange sets
            // topologyDirty on ANY adjacency change around the base (head-adjacent
            // insertions/removals/moves), and the LANES' hooks flag their own
            // left-link violations (mid-chain insertions the base cannot see).
            // Consumed here, UI-side, where orderedLaneIds_ lives: run the EXACT
            // chainIntact() and set the existing needsResnap on a real break.
            // BACKSTOP — a 0.1s poll (Rodney's ruling): heals anything the events
            // might miss within ~100ms; ten ≤5-pointer walks per second — negligible.
            // BOTH defer while ANY drag is in progress (getDraggedWidget covers
            // module drags — the only position changes; knob drags set it too,
            // harmlessly): mid-drag topology flips stay flagged, consumption lands
            // the frame after release — never fight the drag. Both skip until
            // lanesSpawned_ (never race Rack's restore placement / first-frame
            // adoption). They only SET the existing flag; the one-shot block below
            // consumes it.
            if (lanesSpawned_) {
                auto* trigBase = dynamic_cast<StraitsBaseModule*>(module);
                if (trigBase) {
                    const bool eventDirty = trigBase->topologyDirty.load(std::memory_order_relaxed);
                    const double now = system::getTime();
                    const bool pollDue = lastAdjacencyCheck_ < 0.0 || now - lastAdjacencyCheck_ >= 0.1;
                    if ((eventDirty || pollDue) && !APP->event->getDraggedWidget()) {
                        if (eventDirty) trigBase->topologyDirty.store(false, std::memory_order_relaxed);
                        if (pollDue) lastAdjacencyCheck_ = now;
                        // The exact chain check (BOTH runs) + the panels' own
                        // left-link flags (mid-chain insertions the base cannot see).
                        bool broken = !chainIntact(trigBase);
                        if (!broken) {
                            for (int64_t pid : trigBase->orderedSubgateIds_) {
                                rack::Module* pm = APP->engine->getModule(pid);
                                if (pm) {
                                    auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(pm);
                                    if (panel && panel->needsResnap.load(std::memory_order_relaxed)) { broken = true; break; }
                                }
                            }
                        }
                        if (broken)
                            trigBase->needsResnap = true;
                    }
                }
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
                // Drag guard (the fast path's safety): topology events fire MID-DRAG
                // too — a module dragged through the chain flips expanders
                // continuously — so defer consumption to the frame after release;
                // consuming here would fight the user's drag.
                if (needSnap && !APP->event->getDraggedWidget()) {
                    if (baseMod) baseMod->needsResnap = false;
                    // Clear lane + subgate-panel flags
                    for (int64_t laneId : baseMod->orderedLaneIds_) {
                        rack::Module* m = APP->engine->getModule(laneId);
                        if (m) {
                            auto* lane = dynamic_cast<StraitsLaneExpander*>(m);
                            if (lane) lane->needsResnap = false;
                        }
                    }
                    for (int64_t pid : baseMod->orderedSubgateIds_) {
                        rack::Module* pm = APP->engine->getModule(pid);
                        if (pm) {
                            auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(pm);
                            if (panel) panel->needsResnap.store(false, std::memory_order_relaxed);
                        }
                    }
                    resnapRun();
                }
            }
        }
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* m = redDot::lookupMonsoon(module);
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

    // ── Re-snap helper: the ONE run — lanes then subgate panels ────────────────
    // Uses setModulePosForce (Rack's placement-with-shoving) instead of raw
    // box.pos — pushes other modules aside, no overlap. SUBGATE MERGE: lanes in
    // arcLane order, THEN the subgate panels in level-major/decision-major order
    // (RATCHET REST/LEGATO/ADVANCE/ACCENT, then GHOST), positioned sequentially
    // from the base's right edge — the dock order: base → lanes → panels.
    void resnapRun() {
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
        std::sort(baseMod->orderedSubgateIds_.begin(), baseMod->orderedSubgateIds_.end(),
            [&](int64_t a, int64_t b) {
                auto* ma = dynamic_cast<StraitsSubgatePanelModule*>(APP->engine->getModule(a));
                auto* mb = dynamic_cast<StraitsSubgatePanelModule*>(APP->engine->getModule(b));
                int oa = ma ? ma->level * SKns::DECS + ma->dec : 99 * SKns::DECS;
                int ob = mb ? mb->level * SKns::DECS + mb->dec : 99 * SKns::DECS;
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
        for (int64_t panelId : baseMod->orderedSubgateIds_) {
            ModuleWidget* pw = APP->scene->rack->getModule(panelId);
            if (!pw) continue;
            APP->scene->rack->setModulePosForce(pw, Vec(expectedX, box.pos.y));
            expectedX = pw->box.getTopRight().x;
        }
    }

    // ── Periodic-poll predicate: is the lane chain intact? ────────────────────
    // Exact + ownership-aware: the lanes come from OUR orderedLaneIds_ and the
    // links are checked by POINTER identity (leftExpander.module), so a foreign
    // Straits chain parked adjacent to ours can never confuse it. One pass checks
    // contiguity (a gap or an inserted module breaks a left-link) AND canonical
    // order (the same comparator as resnapLanes). Dead IDs (deleted modules) are
    // skipped, matching the flag-poll loop in step().
    bool chainIntact(StraitsBaseModule* baseMod) {
        std::vector<StraitsLaneExpander*> lanes;
        lanes.reserve(baseMod->orderedLaneIds_.size());
        for (int64_t id : baseMod->orderedLaneIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (!m) continue;
            auto* lane = dynamic_cast<StraitsLaneExpander*>(m);
            if (lane) lanes.push_back(lane);
        }
        std::vector<StraitsSubgatePanelModule*> panels;
        panels.reserve(baseMod->orderedSubgateIds_.size());
        for (int64_t id : baseMod->orderedSubgateIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (!m) continue;
            auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
            if (panel) panels.push_back(panel);
        }
        if (lanes.empty() && panels.empty()) return true;
        std::sort(lanes.begin(), lanes.end(),
            [](const StraitsLaneExpander* a, const StraitsLaneExpander* b) {
                int oa = (a->desc) ? a->desc->arcLane : 99;
                int ob = (b->desc) ? b->desc->arcLane : 99;
                return oa < ob;
            });
        std::sort(panels.begin(), panels.end(),
            [](const StraitsSubgatePanelModule* a, const StraitsSubgatePanelModule* b) {
                return a->level * SKns::DECS + a->dec < b->level * SKns::DECS + b->dec;
            });
        rack::Module* prev = module;   // lane[0]'s left neighbour must be the base (us)
        for (StraitsLaneExpander* lane : lanes) {
            if (lane->leftExpander.module != prev) return false;
            prev = lane;
        }
        for (StraitsSubgatePanelModule* panel : panels) {
            if (panel->leftExpander.module != prev) return false;
            prev = panel;
        }
        return true;
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
