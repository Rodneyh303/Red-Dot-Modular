// StraitsQmixOctPanel — the qmix-octave per-voice PANEL surface (see the .hpp
// for the homing + model law). QMIX_OCTAVE_PLAN.md Stage 3: the most-flexible
// end state — ONE panel, 16 voices x ONE threshold (the octave axis's own
// qmix probability), the settled lane-panel form (gen_straits_lane.py: 6/6/4,
// 165-wide, col-major v0 = mono top-left, the wave field, the seam rails;
// the qoct tint — the qmix violet family hue; NO mod jack: modulation is
// Causeway). The StraitsSubgatePanel widget build VERBATIM, family-
// parameterised: the banks differ (editor.qmixOctave vs editor.subgate), the
// lettering is "Q-MIX" over "OCTAVE", the anchors are the same TINT-NEUTRAL
// param_sgv_ family.
#include <rack.hpp>
#include <algorithm>
#include <cmath>

#include "StraitsQmixOctPanel.hpp"
#include "ui/ModuleLookup.hpp"       // THE module-lookup authority: lookupMonsoon
#include "ui/SvgPanelKit.hpp"        // Compose/bind*/findNamed/centerOf/kitStep
#include "ui/ConnectMark.hpp"
#include "ui/StoreBound.hpp"         // StoreKnob — the de-param law
#include "ui/StraitsSeamFiller.hpp"
#include "ui/StraitsSubgateArt.hpp"  // the shared lettering + tint helpers
#include "ui/Controls.hpp"           // control-face Tags

using namespace rack;

// ══ the SUBPANEL widget — 16 per-voice QMIX-OCTAVE knobs, store-backed ══════
// The lane-expander layout: the SVG carries the grid (param_sgv_0..15, tint-
// neutral — the SAME anchor family the subgate panels use, one binding idiom),
// the wave field and the rails; the widget draws the "Q-MIX"/"OCTAVE" captions
// (the Cantonment idiom) and binds the 16 StoreKnobs.
struct StraitsQmixOctPanelWidget : ModuleWidget,
    dotModular::Compose<StraitsQmixOctPanelWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight_ = -1;
    redDot::StraitsSeamFiller* leftFiller = nullptr;
    redDot::StraitsSeamFiller* rightFiller = nullptr;

    StraitsQmixOctPanelWidget(StraitsQmixOctPanelModule* mod) {
        setModule(mod);
        const std::string darkPath  = "res/panels/StraitsQmixOctPanel_dark.svg";
        const std::string lightPath = "res/panels/StraitsQmixOctPanel_light.svg";
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
        // Unclaimed cells DISPLAY the resolved three-way value (what the voice's
        // octave axis plays now — own claim / follow-degree / global); the first
        // drag gesture starts from that shown value and the write CLAIMS the
        // cell. The double-click default claims 0.5 (mid); the base's "Clear
        // per-voice overrides" is the un-claim path. LOOK + DIM (the lane law,
        // CC_PROMPT_SUBGATE_PANELS_DE_SPECIAL_CASE): the Themed_Compact_Cog_Dim
        // pair on StoreKnob; v0 = V1's mono row (never dims, stays EDITABLE —
        // it authors V1's own octave cell); v1..15 dim + lock beyond the active
        // poly count. Range [0,1] — a threshold is a probability (the qmixLevel
        // class; NO bipolar axis on this decision).
        auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
        for (int v = 0; v < dotModular::QmixOctave::VOICES; ++v) {
            const std::string cap = std::string("V") + std::to_string(v + 1)
                                  + " Q-MIX OCTAVE";
            auto* k = redDot::bindStoreThemedKnob<Monsoon,
                                                  redDot::Tag_OffWhite_Compact_Cog,
                                                  redDot::Tag_Dark_Compact_Cog>(
                this, "param_sgv_" + std::to_string(v),
                resolveMon, 0.f, 1.f, dotModular::QmixOctave::kDefaultThreshold, cap,
                [v](Monsoon& m) { return m.qmixOctaveDisplayThreshold(v); },
                [v](Monsoon& m, float x) { m.setQmixOctOwn(v, x); });
            k->lightWhen = [this]() { return themeLight_; };
            auto dimIfInactive = [this, v]() {
                return activeVoices_ >= 0 && v > activeVoices_;
            };
            k->dimWhen  = dimIfInactive;
            k->lockWhen = dimIfInactive;
        }
        // (No mod jack — the subgate panels' law carried over: modulation is
        // Causeway; no delta planes exist for this bank, hence no heartbeat.)

        if (auto* s = findNamed("light_connect")) {
            addChild(redDot::makeConnectMark(module, centerOf(s), mm2px(8.f)));
        }

        // ── Lettering (the Cantonment idiom — the SVG carries no text) ──
        // The lane grid's top band: the identifying dot sits in the LEFT gutter
        // (the subgate idiom — the centre carries the captions). "Q-MIX" (the
        // qmix violet) over "OCTAVE" (ink) — the axis names, not level/decision:
        // this panel IS one decision (the octave axis's threshold).
        constexpr float Wmm = 55.88f;   // 11 HP — the lane-panel width
        auto ink = [this]() { return themeLight_ ? nvgRGB(0x30, 0x34, 0x3a) : nvgRGB(0xc8, 0xcd, 0xd4); };
        auto qmixViolet = []() { return nvgRGB(0x80, 0x60, 0xc0); };
        redDot::addSubgateCaption(this, "Q-MIX", Wmm * 0.5f, 5.6f, 8.5f, qmixViolet);
        redDot::addSubgateCaption(this, "OCTAVE", Wmm * 0.5f, 10.6f, 6.5f, ink);
    }

    bool themeLight_ = false;   // shared with the captions' colourFn + the knobs' lightWhen
    int activeVoices_ = -1;     // the poly-dim source (the lane law; -1 = no Monsoon attached)

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* mm = redDot::lookupMonsoon(module);
        themeLight_ = (mm && mm->lightTheme);
        activeVoices_ = mm ? mm->engine.numPolyVoices : -1;
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
        if (auto* panelMod = dynamic_cast<StraitsQmixOctPanelModule*>(module))
            panelMod->needsResnap.store(true, std::memory_order_relaxed);
    }
};

// ── the model — ONE slug, registered + hidden (base-spawned only) ────────────
Model* modelStraitsQmixOctPanel =
    createModel<StraitsQmixOctPanelModule, StraitsQmixOctPanelWidget>("StraitsQmixOctPanel");
