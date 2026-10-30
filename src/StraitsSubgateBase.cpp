// StraitsSubgateBase — the subgate consumer-group BASE surface (see the .hpp for the
// homing + model law). The wheel here is StraitsBase's VERBATIM, family-
// parameterised to the subgate models: stable owned-ID list, unconditional
// reposition, event fast path + 0.1 s poll, drag guard, restore adoption. The
// knobs are store-backed (the de-param law — StoreKnob onto Monsoon's
// editor.subgate bank; PARAM_CLASSIFICATION.md).
// The per-voice PANELS live in StraitsSubgatePanel.cpp (CC_PROMPT_FIX_SUBGATE_
// PANELS option (a): one 6/6/4 165-wide panel per (level × decision), the
// gen_straits_lane.py layout — the panel widget is that file's concern).
#include <rack.hpp>
#include <algorithm>
#include <cmath>

#include "StraitsSubgateBase.hpp"
#include "ui/ModuleLookup.hpp"       // THE module-lookup authority: lookupMonsoon
#include "ui/SvgPanelKit.hpp"        // Compose/bind*/findNamed/centerOf/kitStep
#include "ui/ConnectMark.hpp"
#include "ui/StoreBound.hpp"         // StoreKnob — the de-param law
#include "ui/StraitsSeamFiller.hpp"
#include "ui/StraitsSubgateArt.hpp"  // the shared lettering + tint helpers
#include "ui/Controls.hpp"           // control-face Tags

using namespace rack;

namespace SKns = dotModular::SubgateKnobs;

// ── the BASE's process: publish the GLOBAL mod deltas + heartbeat slot 0 ─────
// The dist-delta single-writer law: audio-thread-only writer of the delta
// plane, change-gated, publish-overwrites; the heartbeat vouches for liveness
// and MONSOON's watchdog drops the plane if it freezes (publisher gone).
void StraitsSubgateBaseModule::process(const ProcessArgs&) {
    Monsoon* m = redDot::lookupMonsoon(this);
    if (!m) return;   // isolated: heartbeat freezes → the watchdog drops what we last published

    for (int l = 0; l < SKns::LEVELS; ++l)
        for (int d = 0; d < SKns::DECS; ++d) {
            const int id = StraitsSubgateIds::globalModId(l, d);
            const float delta = inputs[id].isConnected() ? inputs[id].getVoltage() * 0.1f : 0.f;
            if (std::fabs(delta - lastGlob_[l][d]) > 1e-6f) {
                m->setSubgateGlobalDelta(l, d, delta);
                lastGlob_[l][d] = delta;
            }
        }
    ++m->editor.subgate.publishSeq[0];
}

// ── persistence: the owned subpanel list (the straitLanes shape) ─────────────
json_t* StraitsSubgateBaseModule::dataToJson() {
    json_t* rootJ = json_object();
    json_t* panelsJ = json_array();
    for (int64_t id : orderedPanelIds_) {
        rack::Module* m = APP->engine->getModule(id);
        if (m) {
            auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
            if (panel)
                json_array_append_new(panelsJ,
                    json_string(StraitsSubgateIds::panelSlug(panel->level, panel->dec)));
        }
    }
    json_object_set_new(rootJ, "subgatePanels", panelsJ);
    return rootJ;
}

void StraitsSubgateBaseModule::dataFromJson(json_t* rootJ) {
    json_t* panelsJ = json_object_get(rootJ, "subgatePanels");
    if (panelsJ) {
        size_t i; json_t* v;
        json_array_foreach(panelsJ, i, v) {
            const char* s = json_string_value(v);
            if (s) pendingPanels_.push_back(s);
        }
    }
}

// ══ the BASE widget ═══════════════════════════════════════════════════════════
struct StraitsSubgateBaseWidget : ModuleWidget,
    dotModular::Compose<StraitsSubgateBaseWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;
    bool panelsSpawned_ = false;   // false until the one-time adopt/pending run
    double lastAdjacencyCheck_ = -1.0;
    redDot::StraitsSeamFiller* leftFiller = nullptr;
    redDot::StraitsSeamFiller* rightFiller = nullptr;

    StraitsSubgateBaseWidget(StraitsSubgateBaseModule* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/StraitsSubgateBase_panel_dark.svg";
        const char* lightPath = "res/panels/StraitsSubgateBase_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Seam fillers (the Straits double-rail abutment) ──
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

        // ── the 8 GLOBAL store knobs (cross-voice tier) + the 8 mod jacks ──
        // THE CAUSEWAY MOD COVERAGE LAW: each knob ships with its jack, born
        // together (a Straits knob without its mod path is a modulation seam).
        auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
        for (int l = 0; l < SKns::LEVELS; ++l)
            for (int d = 0; d < SKns::DECS; ++d) {
                const std::string cap = std::string(SKns::LEVEL_CAPTIONS[l]) + " " + SKns::DEC_CAPTIONS[d];
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(
                    this, "param_sbg_" + std::to_string(l) + "_" + std::to_string(d),
                    resolveMon, 0.f, 1.f, SKns::kDefaultThreshold,
                    "Subgate " + cap + " (global)",
                    [l, d](Monsoon& m) { return m.getSubgateGlobal(l, d); },
                    [l, d](Monsoon& m, float x) { m.setSubgateGlobal(l, d, x); });
                bindInput<PJ301MPort>("mod_sbg_" + std::to_string(l) + "_" + std::to_string(d),
                                      StraitsSubgateIds::globalModId(l, d));
            }

        if (auto* s = findNamed("light_connect")) {
            addChild(redDot::makeConnectMark(module, centerOf(s), mm2px(8.f)));
        }

        // ── Lettering (the Cantonment idiom — the SVG carries no text) ──
        // Coordinates match gen_straits_subgate_base.py: W=81.28, MARGIN_L=17,
        // COL_PITCH=14.82, KNOB_Y={30,48}, JACK_Y={63,77}.
        constexpr float Wmm = 81.28f, MARGIN_L = 17.0f, COL_PITCH = 59.28f / 4.f;
        auto ink = [this]() { return themeLight_ ? nvgRGB(0x30, 0x34, 0x3a) : nvgRGB(0xc8, 0xcd, 0xd4); };
        redDot::addSubgateCaption(this, "SUBGATE", Wmm * 0.5f, 16.5f, 9.f, ink);
        for (int d = 0; d < SKns::DECS; ++d)
            redDot::addSubgateCaption(this, SKns::DEC_CAPTIONS[d], MARGIN_L + COL_PITCH * (d + 0.5f), 21.0f, 6.f, ink);
        for (int l = 0; l < SKns::LEVELS; ++l)
            redDot::addSubgateCaption(this, SKns::LEVEL_CAPTIONS[l], 8.8f, 30.f + 18.f * l, 7.f,
                              [l]() { return redDot::subgateTint(l); });
        redDot::addSubgateCaption(this, "MOD", Wmm * 0.5f, 85.0f, 5.5f, ink);
    }

    Monsoon* mon() { return module ? redDot::lookupMonsoon(module) : nullptr; }
    bool themeLight_ = false;   // shared with the captions' colourFn

    // ── the SUBPANEL WHEEL (StraitsBase verbatim, family-parameterised) ──────
    // CC_PROMPT_FIX_SUBGATE_PANELS option (a): the wheel keys on (level, dec) —
    // one panel per (subgate level × decision), spawned/removed/adopted
    // individually (the StraitsLane law: each is its own attachable module).
    Model* panelModel(int level, int dec) {
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
    static bool samePanel(StraitsSubgatePanelModule* p, int level, int dec) {
        return p && p->level == level && p->dec == dec;
    }
    bool hasPanel(int level, int dec) {
        auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module);
        if (!baseMod) return false;
        for (int64_t id : baseMod->orderedPanelIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
                if (samePanel(panel, level, dec)) return true;
            }
        }
        return false;
    }
    void spawnPanel(int level, int dec) {
        if (!module) return;
        Model* model = panelModel(level, dec);
        if (!model) return;
        // spawnLane's verbatim order: engine module FIRST, ID pushed, widget
        // created, box.pos set BEFORE the rack addModule → addModule preserves
        // pos + runs updateExpanders → LINKED FROM BIRTH.
        engine::Module* panelMod = model->createModule();
        APP->engine->addModule(panelMod);
        if (auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module))
            baseMod->orderedPanelIds_.push_back(panelMod->id);
        ModuleWidget* panelW = model->createModuleWidget(panelMod);
        float rightX = box.getTopRight().x;
        if (auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module)) {
            for (int64_t id : baseMod->orderedPanelIds_) {
                if (id == panelMod->id) continue;   // skip self (just added)
                ModuleWidget* pw = APP->scene->rack->getModule(id);
                if (pw) rightX = std::max(rightX, pw->box.getTopRight().x);
            }
        }
        panelW->box.pos = Vec(rightX, box.pos.y);
        APP->scene->rack->addModule(panelW);
    }
    void removePanel(int level, int dec) {
        auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module);
        if (!baseMod) return;
        for (size_t i = 0; i < baseMod->orderedPanelIds_.size(); ++i) {
            int64_t id = baseMod->orderedPanelIds_[i];
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
                if (samePanel(panel, level, dec)) {
                    ModuleWidget* pw = APP->scene->rack->getModule(id);
                    if (pw) APP->scene->rack->removeModule(pw);
                    APP->engine->removeModule(m);
                    baseMod->orderedPanelIds_.erase(baseMod->orderedPanelIds_.begin() + i);
                    resnapPanels();   // close the gap
                    return;
                }
            }
        }
    }
    void togglePanel(int level, int dec) {
        if (hasPanel(level, dec)) removePanel(level, dec);
        else spawnPanel(level, dec);
    }

    // ── Re-snap: sort by canonical order — level-major then decision-major
    // (RATCHET-REST, RATCHET-LEGATO, RATCHET-ADVANCE, RATCHET-ACCENT, then the
    // GHOST row) — position via Rack's placement API, the UNCONDITIONAL reclaim
    // (task-84 invariant 2): every owned ID setModulePosForce'd to its seat
    // REGARDLESS of where it sits, from any distance; an intruder seated in the
    // run is SHOVED OUT.
    static int panelOrder(StraitsSubgatePanelModule* p) {
        return p ? p->level * SKns::DECS + p->dec : 99 * SKns::DECS;
    }
    void resnapPanels() {
        auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module);
        if (!baseMod) return;
        std::sort(baseMod->orderedPanelIds_.begin(), baseMod->orderedPanelIds_.end(),
            [&](int64_t a, int64_t b) {
                auto* ma = dynamic_cast<StraitsSubgatePanelModule*>(APP->engine->getModule(a));
                auto* mb = dynamic_cast<StraitsSubgatePanelModule*>(APP->engine->getModule(b));
                return panelOrder(ma) < panelOrder(mb);
            });
        float expectedX = box.getTopRight().x;
        for (int64_t panelId : baseMod->orderedPanelIds_) {
            ModuleWidget* pw = APP->scene->rack->getModule(panelId);
            if (!pw) continue;
            APP->scene->rack->setModulePosForce(pw, Vec(expectedX, box.pos.y));
            expectedX = pw->box.getTopRight().x;
        }
    }

    // ── Periodic-poll predicate: is the panel chain intact? (ownership-aware —
    // the panels come from OUR orderedPanelIds_, links by POINTER identity; dead
    // IDs skipped; empty set intact.) ──
    bool chainIntact(StraitsSubgateBaseModule* baseMod) {
        std::vector<StraitsSubgatePanelModule*> panels;
        panels.reserve(baseMod->orderedPanelIds_.size());
        for (int64_t id : baseMod->orderedPanelIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (!m) continue;
            auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
            if (panel) panels.push_back(panel);
        }
        if (panels.empty()) return true;
        std::sort(panels.begin(), panels.end(),
            [](const StraitsSubgatePanelModule* a, const StraitsSubgatePanelModule* b) {
                return a->level * SKns::DECS + a->dec < b->level * SKns::DECS + b->dec;
            });
        rack::Module* prev = module;   // panel[0]'s left neighbour must be the base (us)
        for (StraitsSubgatePanelModule* panel : panels) {
            if (panel->leftExpander.module != prev) return false;
            prev = panel;
        }
        return true;
    }

    // ── Context menu: the source SWITCHES + the per-voice panels ─────────────
    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Subgate source (when no own subpanel)"));
        Monsoon* m = mon();
        if (m) {
            // The per-decision SWITCH (the spec's set-and-forget routing choice):
            // tier 2a global vs tier 2b follow-main. Default FOLLOW MAIN (the
            // spec's least-surprising default — the subgate inherits the main
            // lane until the user opts to a global subgate value or attaches a
            // subpanel). Attached always wins (tier 1) — no switch needed.
            for (int l = 0; l < SKns::LEVELS; ++l) {
                menu->addChild(createMenuLabel(SKns::LEVEL_CAPTIONS[l]));
                for (int d = 0; d < SKns::DECS; ++d) {
                    const std::string cap = std::string(SKns::DEC_CAPTIONS[d]);
                    menu->addChild(createCheckMenuItem(cap + " — follow main", "",
                        [this, l, d]() { Monsoon* mm = mon(); return mm ? mm->isSubgateFollowMain(l, d) : true; },
                        [this, l, d]() { if (auto* mm = mon()) mm->setSubgateFollowMain(l, d, true); }));
                    menu->addChild(createCheckMenuItem(cap + " — global knob", "",
                        [this, l, d]() { Monsoon* mm = mon(); return mm ? !mm->isSubgateFollowMain(l, d) : false; },
                        [this, l, d]() { if (auto* mm = mon()) mm->setSubgateFollowMain(l, d, false); }));
                }
            }
        } else {
            menu->addChild(createMenuLabel("(no Monsoon attached)"));
        }

        // ── the per-voice panels — ONE per (level × decision), option (a) ──
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Per-voice panels"));
        for (int l = 0; l < SKns::LEVELS; ++l) {
            menu->addChild(createMenuLabel(SKns::LEVEL_CAPTIONS[l]));
            for (int d = 0; d < SKns::DECS; ++d) {
                menu->addChild(createCheckMenuItem(
                    std::string("   ") + SKns::DEC_CAPTIONS[d] + " panel", "",
                    [this, l, d]() { return hasPanel(l, d); },
                    [this, l, d]() { togglePanel(l, d); }));
            }
        }

        menu->addChild(new MenuSeparator);
        // The menu's un-claim path (the touch-is-claim law's complement).
        menu->addChild(createMenuItem("Clear per-voice overrides", "",
            [this]() { if (auto* mm = mon()) mm->clearSubgateOwnAll(); }));
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* mm = redDot::lookupMonsoon(module);
        themeLight_ = (mm && mm->lightTheme);
        // ── Seam filler: toggle based on neighbour detection ──
        NVGcolor bg = themeLight_ ? nvgRGB(0xdc, 0xdc, 0xdc) : nvgRGB(0x14, 0x17, 0x1b);
        if (leftFiller)  { leftFiller->active  = isStraitsSubgateChainModel(module->leftExpander.module  ? module->leftExpander.module->model  : nullptr); leftFiller->bgColour  = bg; }
        if (rightFiller) { rightFiller->active = isStraitsSubgateChainModel(module->rightExpander.module ? module->rightExpander.module->model : nullptr); rightFiller->bgColour = bg; }
        if (!lastThemeCheckValid_ || themeLight_ != lastThemeLight_) {
            lastThemeLight_ = themeLight_;
            lastThemeCheckValid_ = true;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(themeLight_ ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }

        auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module);
        if (!baseMod) return;

        // ── One-time adopt + pending spawn ───────────────────────────────────
        // Rack restores the registered+hidden panels natively; the base ADOPTS
        // them by walking the rightExpander chain (the ONLY live-chain-defined
        // membership — the restore adoption). Any pending slug (the dataFromJson
        // fallback) with no live panel is spawned docked; LEGACY slugs (the old
        // per-LEVEL "RATCHET"/"GHOST" — the retired crammed panels) parse false
        // and are skipped. No auto-spawn: the globals work standalone; the
        // per-voice tier is OPT-IN (tier 1).
        if (!panelsSpawned_) {
            rack::Module* right = module->rightExpander.module;
            while (right) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(right);
                if (!panel) break;
                baseMod->orderedPanelIds_.push_back(right->id);
                right = right->rightExpander.module;
            }
            for (const std::string& slug : baseMod->pendingPanels_) {
                int l, d;
                if (!StraitsSubgateIds::parsePanelSlug(slug, l, d)) continue;   // legacy/corrupt slug
                if (!hasPanel(l, d)) spawnPanel(l, d);
            }
            baseMod->pendingPanels_.clear();
            panelsSpawned_ = true;
        }

        // ── Resnap triggers (the StraitsBase fast path + backstop poll) ──────
        // FAST PATH: the base's onExpanderChange sets topologyDirty on any
        // adjacency change around the base, and the PANELS' own hooks flag
        // left-link violations (mid-chain insertions the base cannot see).
        // BACKSTOP: the 0.1 s poll. Both defer while ANY drag is in progress;
        // both only SET needsResnap — the one-shot block consumes it.
        if (panelsSpawned_) {
            const bool eventDirty = baseMod->topologyDirty.load(std::memory_order_relaxed);
            const double now = system::getTime();
            const bool pollDue = lastAdjacencyCheck_ < 0.0 || now - lastAdjacencyCheck_ >= 0.1;
            if ((eventDirty || pollDue) && !APP->event->getDraggedWidget()) {
                if (eventDirty) baseMod->topologyDirty.store(false, std::memory_order_relaxed);
                if (pollDue) lastAdjacencyCheck_ = now;
                bool broken = !chainIntact(baseMod);
                if (!broken) {
                    for (int64_t panelId : baseMod->orderedPanelIds_) {
                        rack::Module* pm = APP->engine->getModule(panelId);
                        if (pm) {
                            auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(pm);
                            if (panel && panel->needsResnap.load(std::memory_order_relaxed)) { broken = true; break; }
                        }
                    }
                }
                if (broken) baseMod->needsResnap = true;
            }
        }

        // ── One-shot re-snap: fires ONLY when a needsResnap flag is set; zero
        // box.pos writes during drags (the drag guard), consumed post-release.
        {
            bool needSnap = baseMod->needsResnap;
            if (needSnap && !APP->event->getDraggedWidget()) {
                baseMod->needsResnap = false;
                for (int64_t panelId : baseMod->orderedPanelIds_) {
                    rack::Module* pm = APP->engine->getModule(panelId);
                    if (pm) {
                        auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(pm);
                        if (panel) panel->needsResnap.store(false, std::memory_order_relaxed);
                    }
                }
                resnapPanels();
            }
        }
    }

    void onDragEnd(const event::DragEnd& e) override {
        ModuleWidget::onDragEnd(e);
        if (auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module))
            baseMod->needsResnap = true;
    }

    bool lastThemeLight_ = false;
    bool lastThemeCheckValid_ = false;
};

Model* modelStraitsSubgateBase =
    createModel<StraitsSubgateBaseModule, StraitsSubgateBaseWidget>("StraitsSubgateBase");
