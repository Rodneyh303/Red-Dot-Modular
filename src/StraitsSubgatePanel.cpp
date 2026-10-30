// StraitsSubgatePanel — the subgate per-voice PANEL surfaces (see the .hpp for the
// homing + model law). CC_PROMPT_FIX_SUBGATE_PANELS, Rodney DECIDED option (a):
// ONE panel per (subgate level × decision) — {RATCHET,GHOST} × {REST,LEGATO,
// ACCENT,ADVANCE} = eight attachable modules, each 16 voices × ONE decision,
// the settled lane-panel form (gen_straits_lane.py: 6/6/4, 165-wide, col-major
// v0=mono top-left, the wave field, the seam rails — artwork-identical to the
// lane subpanels except tint; NO mod jack: CC_PROMPT_SUBGATE_MERGE_ONTO_
// STRAITS_BASE — modulation is Causeway). The old per-LEVEL cram (16 voices ×
// 4 decision columns on one 180-wide panel) is RETIRED.
// The wheel side (spawn/adopt/resnap) lives in StraitsSubgateBase.cpp.
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

// ══ the SUBPANEL widget — 16 voice knobs for ONE decision, store-backed ══════
// The lane-expander layout: the SVG carries the grid (param_sgv_0..15, tint-
// neutral so one widget class binds all eight panels), the wave field and the
// rails; the widget draws the LEVEL + DECISION captions (the Cantonment idiom)
// and binds the 16 StoreKnobs.
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
        const int dec   = mod ? mod->dec   : 0;
        const std::string key = StraitsSubgateIds::panelSvgKey(level, dec);
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

        // ── the 16 per-voice store knobs (touch = claim — the tier-1 opt-in) ──
        // Unclaimed cells DISPLAY the resolved three-way value (what the voice
        // plays now — inherited main/global/deltas); the first drag gesture
        // starts from that shown value and the write CLAIMS the cell. The
        // double-click default claims 0.5 (mid); the menu's "Clear per-voice
        // overrides" (on the base) is the un-claim path. The anchors are the
        // TINT-NEUTRAL param_sgv_ family (one widget class binds all eight
        // panels); the lane grid is col-major v0 = mono top-left.
        auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
        for (int v = 0; v < SKns::VOICES; ++v) {
            const std::string cap = std::string("V") + std::to_string(v + 1) + " "
                                  + SKns::LEVEL_CAPTIONS[level] + " " + SKns::DEC_CAPTIONS[dec];
            redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Small_Cog>(
                this, "param_sgv_" + std::to_string(v),
                resolveMon, 0.f, 1.f, SKns::kDefaultThreshold, cap,
                [level, dec, v](Monsoon& m) { return m.subgateDisplayThreshold(v, level, dec); },
                [level, dec, v](Monsoon& m, float x) { m.setSubgateOwn(v, level, dec, x); });
        }
        // (No mod jack — CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE WRONG 2:
        // per-panel modulation was not asked for; modulation is Causeway.)

        if (auto* s = findNamed("light_connect")) {
            addChild(redDot::makeConnectMark(module, centerOf(s), mm2px(8.f)));
        }

        // ── Lettering (the Cantonment idiom — the SVG carries no text) ──
        // The lane grid's top band: TOP=16, the tint band starts at TOP-4; the
        // identifying dot sits at TOP-6. LEVEL over DECISION above it.
        constexpr float Wmm = 55.88f;   // 11 HP — the lane-panel width
        auto ink = [this]() { return themeLight_ ? nvgRGB(0x30, 0x34, 0x3a) : nvgRGB(0xc8, 0xcd, 0xd4); };
        redDot::addSubgateCaption(this, SKns::LEVEL_CAPTIONS[level], Wmm * 0.5f, 5.6f, 8.5f,
                          [level]() { return redDot::subgateTint(level); });
        redDot::addSubgateCaption(this, SKns::DEC_CAPTIONS[dec], Wmm * 0.5f, 10.6f, 6.5f, ink);
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

// (No panel process — CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE: with the mod
// jack gone there is no per-voice delta writer, and heartbeat slot 1 is
// frozen-by-default already when no panels are attached (the opt-in tier) —
// the watchdog drops the per-voice delta plane in its default state, which is
// exactly all-zero deltas. The module is a pure attach-surface: StoreKnob
// authoring goes through Monsoon's store via the widget.)

// ── the eight per-(level × decision) models — one slug each ──────────────────
Model* modelStraitsSubgatePanelRatchetRest =
    createModel<StraitsSubgateRatchetRestModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelRatchetRest");
Model* modelStraitsSubgatePanelRatchetLegato =
    createModel<StraitsSubgateRatchetLegatoModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelRatchetLegato");
Model* modelStraitsSubgatePanelRatchetAccent =
    createModel<StraitsSubgateRatchetAccentModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelRatchetAccent");
Model* modelStraitsSubgatePanelRatchetAdvance =
    createModel<StraitsSubgateRatchetAdvanceModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelRatchetAdvance");
Model* modelStraitsSubgatePanelGhostRest =
    createModel<StraitsSubgateGhostRestModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelGhostRest");
Model* modelStraitsSubgatePanelGhostLegato =
    createModel<StraitsSubgateGhostLegatoModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelGhostLegato");
Model* modelStraitsSubgatePanelGhostAccent =
    createModel<StraitsSubgateGhostAccentModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelGhostAccent");
Model* modelStraitsSubgatePanelGhostAdvance =
    createModel<StraitsSubgateGhostAdvanceModule, StraitsSubgatePanelWidget>("StraitsSubgatePanelGhostAdvance");
