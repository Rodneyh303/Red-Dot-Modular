// StraitsSubgateBase — the subgate consumer-group surfaces (see the .hpp for the
// homing + model law). The wheel here is StraitsBase's VERBATIM, family-
// parameterised to the subgate models: stable owned-ID list, unconditional
// reposition, event fast path + 0.1 s poll, drag guard, restore adoption. The
// knobs are store-backed (the de-param law — StoreKnob onto Monsoon's
// editor.subgate bank; PARAM_CLASSIFICATION.md).
#include <rack.hpp>
#include <algorithm>
#include <cmath>

#include "StraitsSubgateBase.hpp"
#include "ui/ModuleLookup.hpp"       // THE module-lookup authority: lookupMonsoon
#include "ui/SvgPanelKit.hpp"        // Compose/bind*/findNamed/centerOf/kitStep
#include "ui/ConnectMark.hpp"
#include "ui/StoreBound.hpp"         // StoreKnob — the de-param law
#include "ui/StraitsSeamFiller.hpp"
#include "ui/Controls.hpp"           // control-face Tags

using namespace rack;

namespace SKns = dotModular::SubgateKnobs;

namespace {

// The Cantonment lettering idiom: nanosvg drops <text>, so the widget draws the
// captions (the WarpCurvePopup font block verbatim). mm-anchored, theme-aware
// via colourFn.
struct SubgateCaption : rack::widget::Widget {
    std::string text;
    float sizePx = 7.f;
    std::function<NVGcolor()> colourFn;
    void draw(const DrawArgs& args) override {
        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font) font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, colourFn ? colourFn() : nvgRGB(0xc8, 0xcd, 0xd4));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFontSize(args.vg, sizePx);
        nvgText(args.vg, 0, 0, text.c_str(), nullptr);
    }
};
void addCaption(rack::widget::Widget* parent, const std::string& text,
                float xmm, float ymm, float sizePx, std::function<NVGcolor()> colourFn) {
    auto* c = rack::createWidget<SubgateCaption>(rack::Vec(0, 0));
    c->text = text;
    c->sizePx = sizePx;
    c->colourFn = std::move(colourFn);
    c->box.pos = rack::mm2px(rack::Vec(xmm, ymm));
    parent->addChild(c);
}

// The row tints — ONE authority with the generators (gen_straits_subgate_*.py).
NVGcolor subgateTint(int level) {
    return level == dotModular::SubgateKnobs::GHOST ? nvgRGB(0x58, 0x78, 0xb8)
                                                    : nvgRGB(0xc0, 0x58, 0x50);
}

} // namespace

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

// ── the SUBPANEL's process: publish the PER-VOICE deltas + heartbeat slot 1 ──
// One 16-channel jack per decision column, ch v = voice storage slot v. An
// unpatched column publishes zeros for the cells it last owned (the
// publish-overwrite law — nothing latches after an unplug).
void StraitsSubgatePanelModule::process(const ProcessArgs&) {
    Monsoon* m = redDot::lookupMonsoon(this);
    if (!m) return;

    for (int d = 0; d < SKns::DECS; ++d) {
        rack::engine::Input& in = inputs[StraitsSubgateIds::MOD_PANEL_0 + d];
        if (in.isConnected()) {
            const int chans = std::max(1, in.getChannels());
            for (int v = 0; v < SKns::VOICES; ++v) {
                // Beyond the cable's channels the mod is simply absent (no wrap) —
                // the delta publishes 0 there, which is a no-op on a quiet cell.
                const float delta = (v < chans) ? in.getPolyVoltage(v) * 0.1f : 0.f;
                if (std::fabs(delta - lastOwn_[v][d]) > 1e-6f) {
                    m->setSubgateOwnDelta(v, level, d, delta);
                    lastOwn_[v][d] = delta;
                }
            }
        } else {
            for (int v = 0; v < SKns::VOICES; ++v) {
                if (lastOwn_[v][d] != 0.f) {
                    m->setSubgateOwnDelta(v, level, d, 0.f);
                    lastOwn_[v][d] = 0.f;
                }
            }
        }
    }
    ++m->editor.subgate.publishSeq[1];
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
                json_array_append_new(panelsJ, json_string(StraitsSubgateIds::panelSlug(panel->level)));
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
        addCaption(this, "SUBGATE", Wmm * 0.5f, 16.5f, 9.f, ink);
        for (int d = 0; d < SKns::DECS; ++d)
            addCaption(this, SKns::DEC_CAPTIONS[d], MARGIN_L + COL_PITCH * (d + 0.5f), 21.0f, 6.f, ink);
        for (int l = 0; l < SKns::LEVELS; ++l)
            addCaption(this, SKns::LEVEL_CAPTIONS[l], 8.8f, 30.f + 18.f * l, 7.f,
                       [this, l]() { return subgateTint(l); });
        addCaption(this, "MOD", Wmm * 0.5f, 85.0f, 5.5f, ink);
    }

    Monsoon* mon() { return module ? redDot::lookupMonsoon(module) : nullptr; }
    bool themeLight_ = false;   // shared with the captions' colourFn

    // ── the SUBPANEL WHEEL (StraitsBase verbatim, family-parameterised) ──────
    Model* panelModel(int level) {
        return level == SKns::GHOST ? modelStraitsSubgatePanelGhost
                                    : modelStraitsSubgatePanelRatchet;
    }
    bool hasPanel(int level) {
        auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module);
        if (!baseMod) return false;
        for (int64_t id : baseMod->orderedPanelIds_) {
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
                if (panel && panel->level == level) return true;
            }
        }
        return false;
    }
    void spawnPanel(int level) {
        if (!module) return;
        Model* model = panelModel(level);
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
    void removePanel(int level) {
        auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module);
        if (!baseMod) return;
        for (size_t i = 0; i < baseMod->orderedPanelIds_.size(); ++i) {
            int64_t id = baseMod->orderedPanelIds_[i];
            rack::Module* m = APP->engine->getModule(id);
            if (m) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(m);
                if (panel && panel->level == level) {
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
    void togglePanel(int level) {
        if (hasPanel(level)) removePanel(level);
        else spawnPanel(level);
    }

    // ── Re-snap: sort by canonical order (RATCHET then GHOST), position via
    // Rack's placement API — the UNCONDITIONAL reclaim (task-84 invariant 2):
    // every owned ID setModulePosForce'd to its seat REGARDLESS of where it
    // sits, from any distance; an intruder seated in the run is SHOVED OUT.
    void resnapPanels() {
        auto* baseMod = dynamic_cast<StraitsSubgateBaseModule*>(module);
        if (!baseMod) return;
        std::sort(baseMod->orderedPanelIds_.begin(), baseMod->orderedPanelIds_.end(),
            [&](int64_t a, int64_t b) {
                auto* ma = dynamic_cast<StraitsSubgatePanelModule*>(APP->engine->getModule(a));
                auto* mb = dynamic_cast<StraitsSubgatePanelModule*>(APP->engine->getModule(b));
                int la = ma ? ma->level : 99;
                int lb = mb ? mb->level : 99;
                return la < lb;
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
                return a->level < b->level;
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

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Per-voice panels"));
        menu->addChild(createCheckMenuItem("RATCHET panel", "",
            [this]() { return hasPanel(SKns::RATCHET); },
            [this]() { togglePanel(SKns::RATCHET); }));
        menu->addChild(createCheckMenuItem("GHOST panel", "",
            [this]() { return hasPanel(SKns::GHOST); },
            [this]() { togglePanel(SKns::GHOST); }));

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
        // fallback) with no live panel is spawned docked. No auto-spawn: the
        // globals work standalone; the per-voice tier is OPT-IN (tier 1).
        if (!panelsSpawned_) {
            rack::Module* right = module->rightExpander.module;
            while (right) {
                auto* panel = dynamic_cast<StraitsSubgatePanelModule*>(right);
                if (!panel) break;
                baseMod->orderedPanelIds_.push_back(right->id);
                right = right->rightExpander.module;
            }
            for (const std::string& slug : baseMod->pendingPanels_) {
                const int level = (slug == "GHOST") ? SKns::GHOST : SKns::RATCHET;
                if (!hasPanel(level)) spawnPanel(level);
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

// ══ the SUBPANEL widget — 16 voice rows × 4 decision columns, store-backed ═══
struct StraitsSubgatePanelWidget : ModuleWidget,
    dotModular::Compose<StraitsSubgatePanelWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight_ = -1;
    redDot::StraitsSeamFiller* leftFiller = nullptr;
    redDot::StraitsSeamFiller* rightFiller = nullptr;

    StraitsSubgatePanelWidget(StraitsSubgatePanelModule* mod) {
        setModule(mod);
        const int level = mod ? mod->level : 0;
        const char* key = (level == SKns::GHOST) ? "ghost" : "ratchet";
        const std::string darkPath  = std::string("res/panels/StraitsSubgatePanel_") + key + "_dark.svg";
        const std::string lightPath = std::string("res/panels/StraitsSubgatePanel_") + key + "_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

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

        // ── the 64 per-voice store knobs (touch = claim — the tier-1 opt-in) ──
        // Unclaimed cells DISPLAY the resolved three-way value (what the voice
        // plays now — inherited main/global/deltas); the first drag gesture
        // starts from that shown value and the write CLAIMS the cell. The
        // double-click default claims 0.5 (mid); the menu's "Clear per-voice
        // overrides" (on the base) is the un-claim path.
        auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
        for (int d = 0; d < SKns::DECS; ++d) {
            for (int v = 0; v < SKns::VOICES; ++v) {
                const std::string cap = std::string("V") + std::to_string(v + 1) + " "
                                      + SKns::LEVEL_CAPTIONS[level] + " " + SKns::DEC_CAPTIONS[d];
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Small_Cog>(
                    this, "param_sbp_" + std::to_string(d) + "_" + std::to_string(v),
                    resolveMon, 0.f, 1.f, SKns::kDefaultThreshold, cap,
                    [level, d, v](Monsoon& m) { return m.subgateDisplayThreshold(v, level, d); },
                    [level, d, v](Monsoon& m, float x) { m.setSubgateOwn(v, level, d, x); });
            }
            bindInput<PJ301MPort>("mod_sbp_" + std::to_string(d),
                                  StraitsSubgateIds::MOD_PANEL_0 + d);
        }

        if (auto* s = findNamed("light_connect")) {
            addChild(redDot::makeConnectMark(module, centerOf(s), mm2px(8.f)));
        }

        // ── Lettering (the Cantonment idiom — the SVG carries no text) ──
        // Coordinates match gen_straits_subgate_panel.py: W=60.96, MARGIN=4,
        // COL_PITCH=13.24, GRID_TOP=17, ROW_H=5.625.
        constexpr float Wmm = 60.96f, MARGIN = 4.0f, COL_PITCH = 52.96f / 4.f;
        constexpr float GRID_TOP = 17.0f, ROW_H = 5.625f;
        auto ink = [this]() { return themeLight_ ? nvgRGB(0x30, 0x34, 0x3a) : nvgRGB(0xc8, 0xcd, 0xd4); };
        addCaption(this, SKns::LEVEL_CAPTIONS[level], Wmm * 0.5f, 7.5f, 9.f,
                   [level]() { return subgateTint(level); });
        for (int d = 0; d < SKns::DECS; ++d)
            addCaption(this, SKns::DEC_CAPTIONS[d], MARGIN + COL_PITCH * (d + 0.5f), 11.6f, 5.5f, ink);
        for (int v = 0; v < SKns::VOICES; v += 4)
            addCaption(this, "V" + std::to_string(v + 1), 2.9f,
                       GRID_TOP + ROW_H * (v + 0.5f), 3.5f, ink);
    }

    bool themeLight_ = false;   // shared with the captions' colourFn

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* mm = redDot::lookupMonsoon(module);
        themeLight_ = (mm && mm->lightTheme);
        NVGcolor bg = themeLight_ ? nvgRGB(0xdc, 0xdc, 0xdc) : nvgRGB(0x14, 0x17, 0x1b);
        if (leftFiller)  { leftFiller->active  = isStraitsSubgateChainModel(module->leftExpander.module  ? module->leftExpander.module->model  : nullptr); leftFiller->bgColour  = bg; }
        if (rightFiller) { rightFiller->active = isStraitsSubgateChainModel(module->rightExpander.module ? module->rightExpander.module->model : nullptr); rightFiller->bgColour = bg; }
        if (themeLight_ != lastThemeLight_) {
            lastThemeLight_ = themeLight_;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(themeLight_ ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }
    }

    void onDragEnd(const event::DragEnd& e) override {
        ModuleWidget::onDragEnd(e);
        // The panel flags its own drag; the BASE's step consumes it (the lane hook).
        if (auto* panelMod = dynamic_cast<StraitsSubgatePanelModule*>(module))
            panelMod->needsResnap.store(true, std::memory_order_relaxed);
    }
};

Model* modelStraitsSubgateBase =
    createModel<StraitsSubgateBaseModule, StraitsSubgateBaseWidget>("StraitsSubgateBase");
Model* modelStraitsSubgatePanelRatchet =
    createModel<StraitsSubgateRatchetModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelRatchet");
Model* modelStraitsSubgatePanelGhost =
    createModel<StraitsSubgateGhostModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelGhost");
