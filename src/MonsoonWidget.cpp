#include <rack.hpp>
#include <algorithm>
#include "MonsoonWidget.hpp"
#include "Monsoon.hpp"
#include "ui/OutputAccent.hpp"
#include "ui/ModArcOverlay.hpp"
#include "dsp/managers/MonsoonScaleManager.hpp"

using namespace rack;
using namespace MonsoonIds;

// ── Custom Slider for Scale (non-destructive display) ────────────────────────
// Display layer of the non-destructive scale change (SHOPHOUSE_SPEC.md). Enforcement is
// read-time in getSemitoneWeight (out-of-scale-when-locked → 0 probability → note never
// plays). This widget only DISPLAYS; it never writes the store. Three concerns kept
// separate + swappable so the open UX choices are one-line changes later:
//   (1) dim-on-out-of-scale  — dims the fader without changing its position (the stored
//       value stays visible, greyed). Flip DISPLAY_OUT_OF_SCALE_AS_ZERO to instead render
//       the handle at zero (both are display-only; the store is untouched either way).
//   (2) never-bright invariant — the handle FLASH light is play-driven (semiLedBrightness ←
//       semiPlayRemain, set only when a note actually plays). An out-of-scale locked note
//       reads weight 0 → never plays → never flashes. So the user never sees an out-of-scale
//       fader light up: guaranteed by enforcement, not by this widget. (The alpha dim below
//       also attenuates any residual, belt-and-suspenders.)
//   (3) modulation display — Conservation-GATED (fader-specific, automatic): show the modulated
//       marker EXCEPT on an out-of-scale fader when Conservation is ON (that note is silenced, so
//       its modulation is misleading). Conservation OFF (guide mode) shows everything freely.
//       Layered on top of the existing global modVizMonsoonMelody context-menu choice. Conservation
//       mode is the differentiator. (Isolated in drawModMarker.)
template <typename TLightBase = RedLight>
struct MonsoonLightSlider : VCVLightSlider<TLightBase> {
    // Flip to true to render out-of-scale faders at ZERO position instead of dim-in-place.
    // Display-only in both cases (store never written). Default false = dim-in-place (chosen).
    static constexpr bool DISPLAY_OUT_OF_SCALE_AS_ZERO = false;

    bool semiOutOfScale(Monsoon* m) const {
        if (!m || !m->scaleManager) return false;
        if (this->paramId < MonsoonIds::SEMI0_PARAM || this->paramId >= MonsoonIds::SEMI0_PARAM + 12)
            return false;
        int sem = this->paramId - MonsoonIds::SEMI0_PARAM;
        return !(m->scaleManager->activeScaleMask & (1 << sem));
    }

    void draw(const widget::Widget::DrawArgs& args) override {
        auto* m = dynamic_cast<Monsoon*>(this->module);
        const bool dimmed = semiOutOfScale(m);

        // (Option, flexible) render at zero instead of dim-in-place. Display-only: we transiently
        // present 0 to the handle draw then restore, never writing the store — same present-then-
        // restore idea as the spread knob's displayValueFn.
        float restore = 0.f; bool didZero = false;
        if (dimmed && DISPLAY_OUT_OF_SCALE_AS_ZERO) {
            if (auto* pq = this->getParamQuantity()) { restore = pq->getValue(); pq->setValue(0.f); didZero = true; }  // semitone faders are 0..1; min is 0
        }

        if (dimmed) nvgGlobalAlpha(args.vg, 0.4f);   // dim, position unchanged (chosen default)
        VCVLightSlider<TLightBase>::draw(args);
        if (dimmed) nvgGlobalAlpha(args.vg, 1.0f);

        if (didZero) { if (auto* pq = this->getParamQuantity()) pq->setValue(restore); }

        drawModMarker(args, m);
    }

    // ── Modulation display (SWAPPABLE — final method TODO, see SHOPHOUSE_SPEC.md) ─────────
    // Superimposed contrasting-colour marker at the MODULATED value's height (the handle
    // light shows the SET value). Shown when pitch modulation is active and differs from set.
    // Modulation floored at 0 (slider min is 0). Kept for now even on out-of-scale faders;
    // isolate here so the final gated-fader mod-display choice is a localized change.
    void drawModMarker(const widget::Widget::DrawArgs& args, Monsoon* m) {
        if (!(m && m->modVizMonsoonMelody &&
              this->paramId >= MonsoonIds::SEMI0_PARAM && this->paramId < MonsoonIds::SEMI0_PARAM + 12 &&
              m->modViz.pitchLane[this->paramId - MonsoonIds::SEMI0_PARAM]))
            return;
        // Conservation-gated, fader-specific mod suppression (automatic — on top of the existing
        // global modVizMonsoonMelody context-menu choice). When Conservation is ON, an out-of-
        // scale note is silenced (read as 0), so showing its modulation is misleading (movement
        // on a note that can't sound) — HIDE it. Conservation OFF = guide mode, nothing silenced,
        // show everything freely. So Conservation mode is the differentiator; in-scale faders and
        // all faders when Conservation is off are unaffected. (SHOPHOUSE_SPEC.md.)
        if (m->scaleManager && m->scaleManager->lockScaleNotes && semiOutOfScale(m))
            return;
        int sem = this->paramId - MonsoonIds::SEMI0_PARAM;
        float modN = rack::math::clamp(m->modViz.semitone[sem], 0.f, 1.f);  // floored at 0
        float setN = 0.f;
        if (auto* pq = m->paramQuantities[this->paramId]) setN = (float)pq->getScaledValue();
        if (std::fabs(modN - setN) <= 0.01f) return;
        float top = this->box.size.y * 0.10f;
        float bot = this->box.size.y * 0.90f;
        float y = top + (1.f - modN) * (bot - top);
        float cx = this->box.size.x * 0.5f;
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, y, 4.2f);
        nvgFillColor(args.vg, nvgRGBAf(0.1f, 0.8f, 0.95f, 0.30f));   // halo
        nvgFill(args.vg);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, y, 2.2f);
        nvgFillColor(args.vg, nvgRGBAf(0.4f, 0.95f, 1.0f, 0.95f));   // core
        nvgFill(args.vg);
    }
};

// Trial dice button that disables itself (visually dimmed + press swallowed) when its
// stream is in Reversible mode, where auditioning is blocked. isMelody picks which
// stream's reversible flag to read.
struct TrialButton : VCVButton {
    bool isMelody = false;
    bool inert() const {
        auto* m = dynamic_cast<Monsoon*>(this->module);
        if (!m) return false;
        return (isMelody ? m->melodyReversibleMode : m->rhythmReversibleMode) != 0;
    }
    void onDragStart(const event::DragStart& e) override {
        if (inert()) return;                 // swallow — no actuation, no value change
        VCVButton::onDragStart(e);
    }
    void onButton(const event::Button& e) override {
        if (inert()) { e.consume(this); return; }   // block click before it actuates
        VCVButton::onButton(e);
    }
    void draw(const widget::Widget::DrawArgs& args) override {
        bool dim = inert();
        if (dim) nvgGlobalAlpha(args.vg, 0.4f);
        VCVButton::draw(args);
        if (dim) nvgGlobalAlpha(args.vg, 1.0f);
    }
};

// ── Simple Befaco-inspired knobs ─────────────────────────────────────────────
// ── Knob shadows ─────────────────────────────────────────────────────────────
// Rack's SvgKnob always builds a CircularShadow and, in setSvg(), sizes it to the
// knob and offsets it DOWN by 10% of the knob height. CircularShadow defaults to
// blurRadius = 0, so its radial gradient runs r -> r+0: not a soft shadow at all,
// but a HARD-EDGED black disc at 15% opacity peeking out below every knob. On the
// light panel that reads as a grey smudge, and our knob SVGs already carry their
// own bevel, so it is redundant as well as ugly. Kill it on every RDM knob.
// (Restore by raising opacity AND giving blurRadius a real value — never opacity alone.)
RDM_KnobLarge::RDM_KnobLarge() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobLarge.svg")));
    shadow->opacity = 0.f;   // see note above
}
RDM_KnobMedium::RDM_KnobMedium() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobMedium.svg")));
    shadow->opacity = 0.f;   // see note above
}
RDM_KnobSmall::RDM_KnobSmall() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobSmall.svg")));
    shadow->opacity = 0.f;   // see note above
}
RDM_KnobDarkLarge::RDM_KnobDarkLarge() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobDark_Large.svg")));
    shadow->opacity = 0.f;   // see note above
}
RDM_KnobDarkMedium::RDM_KnobDarkMedium() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobDark_Medium.svg")));
    shadow->opacity = 0.f;   // see note above
}
RDM_KnobCreamLarge::RDM_KnobCreamLarge() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobCream_Large.svg")));
    shadow->opacity = 0.f;   // see note above
}
RDM_KnobCreamMedium::RDM_KnobCreamMedium() {
    minAngle = -0.83f * M_PI;
    maxAngle =  0.83f * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RDM_KnobCream_Medium.svg")));
    shadow->opacity = 0.f;   // see note above
}

bool MonsoonWidget::getLightTheme() const {
    auto* m = dynamic_cast<const Monsoon*>(module);
    return m ? m->lightTheme : false;
}

void MonsoonWidget::setLightTheme(bool v) {
    auto* m = dynamic_cast<Monsoon*>(module);
    if (m) m->lightTheme = v;
}

// Attach a ModArcOverlay on top of a just-bound knob, wired to read the knob's
// SET value (its param, normalised) and the module's published MODULATED value
// (a ModViz field) + the active flag. Decoupled: the overlay only sees floats.
// MUST be called AFTER the knob is added to the widget (z-order: overlay on top).
// Queue a mod-arc for a just-bound knob (created in attachModArc's caller). The
// arc itself is built later in flushModArcs(), AFTER all knobs are added, so it
// draws on top. field() reads the module's published normalised MODULATED value.
// Queue a LINEAR (vertical-slider) mod indicator. Same deferred-attach scheme as
// queueModArc; flushModArcs builds it (mode flag carried via PendingModArc).
static void queueModArcLinear(MonsoonWidget* mw, Monsoon* module, rack::ParamWidget* slider,
                              std::function<float(const Monsoon::ModViz&)> field,
                              std::function<bool(const Monsoon::ModViz&)> active,
                              std::function<bool(const Monsoon&)> enabled = nullptr) {
    if (!slider || !mw || !module) return;
    MonsoonWidget::PendingModArc p;
    p.knob = slider;
    p.linear = true;
    p.getModNorm = [module, field]()  -> float { return module ? field(module->modViz)  : 0.f; };
    p.isActive   = [module, active, enabled]() -> bool  {
        if (!module) return false;
        if (enabled && !enabled(*module)) return false;   // modviz toggle (by surface)
        return active(module->modViz);
    };
    mw->pendingModArcs.push_back(p);
}

static void queueModArc(MonsoonWidget* mw, Monsoon* module, rack::ParamWidget* knob,
                        std::function<float(const Monsoon::ModViz&)> field,
                        std::function<bool(const Monsoon::ModViz&)> active,
                        float radiusFrac = 0.5f,
                        std::function<bool(const Monsoon&)> enabled = nullptr) {
    if (!knob || !mw || !module) return;
    MonsoonWidget::PendingModArc p;
    p.knob = knob;
    p.radiusFrac = radiusFrac;
    p.getModNorm = [module, field]()  -> float { return module ? field(module->modViz)  : 0.f; };
    p.isActive   = [module, active, enabled]() -> bool  {
        if (!module) return false;
        if (enabled && !enabled(*module)) return false;   // modviz toggle (by surface)
        return active(module->modViz);
    };
    mw->pendingModArcs.push_back(p);
}

// Build all queued mod-arc overlays as children of the MAIN widget (panel-space
// coords, like the Sands editor — not framebuffered knob children). Called after
// the knob bind block so the arcs draw on top of the knobs.
static void flushModArcs(MonsoonWidget* mw, Monsoon* module) {
    for (auto& p : mw->pendingModArcs) {
        if (!p.knob) continue;
        auto* arc = new redDot::ModArcOverlay();
        // attachOverKnob (below) pads the overlay box beyond the knob so the arc
        // (drawn at radius + stroke + end-dot, past the knob's bounds) stays fully
        // inside the box — otherwise stale pixels linger as a "mouse-trail" smear
        // when the knob is turned (bug #2).
        if (p.linear) {
            arc->mode = redDot::ModArcOverlay::LINEAR_V;
            arc->travelTopPx = p.knob->box.size.y * 0.10f;
            arc->travelBotPx = p.knob->box.size.y * 0.10f;
        } else {
            arc->radius = std::min(p.knob->box.size.x, p.knob->box.size.y) * p.radiusFrac + mm2px(0.6f);
        }
        // Pad the overlay box so the arc/caps never draw outside it (prevents the
        // stale "trail" pixels on manual knob turns). Sets box pos/size + LINEAR
        // travel insets in one place.
        arc->attachOverKnob(p.knob, mm2px(2.5f));
        int pid = p.knob->paramId;
        arc->getSetNorm = [module, pid]() -> float {
            if (!module) return 0.f;
            auto* pq = module->paramQuantities[pid];
            return pq ? (float)pq->getScaledValue() : 0.f;
        };
        arc->getModNorm = p.getModNorm;
        arc->isActive   = p.isActive;
        mw->addChild(arc);
    }
    mw->pendingModArcs.clear();
}

MonsoonWidget::MonsoonWidget(Monsoon* module) {
        setModule(module);
        applyTheme();
        if (box.size.x == 0) box.size = mm2px(Vec(W_MM, 128.5f));

        // ── 12 semitone sliders: 9mm pitch ───────────────────────────────────
        // EXPERIMENT branch: the semitone modulation is shown by a superimposed
        // light marker inside MonsoonLightSlider::draw (not the linear tick), so
        // no queueModArcLinear here — this lets us compare the two approaches.
        // Bound by ANCHOR, not coordinates: panel_src/fader_level_markers.py emits each
        // param_SEMIn_PARAM anchor AND that fader's level ticks from one loop, so the ticks
        // cannot drift from the slider (cleanup doc A3). Do not reintroduce mm here.
        for (int i = 0; i < 12; ++i) {
            bindLightParam<MonsoonLightSlider<GreenRedLight>>(
                "param_SEMI" + std::to_string(i) + "_PARAM", SEMI0_PARAM + i, SEMI_LED_START + 2*i);
        }

        // ── OCT sliders ───────────────────────────────────────────────────────
        {
            VCVLightSlider<RedLight>* lo = nullptr;
            bindLightParam<VCVLightSlider<RedLight>>("param_OCT_LO_PARAM",
                MonsoonIds::OCT_LO_PARAM, MonsoonIds::OCT_LO_LED,
                std::function<void(VCVLightSlider<RedLight>*)>([&lo](VCVLightSlider<RedLight>* w){ lo = w; }));
            // Guard: bindLightParam only WARNs on a missing anchor, so the slider may be null.
            if (lo) queueModArcLinear(this, module, lo,
                [](const Monsoon::ModViz& m){ return m.octaveLo; },
                [](const Monsoon::ModViz& m){ return m.pitchLane[12]; },
                [](const Monsoon& mm){ return mm.modVizMonsoonMelody; });
            VCVLightSlider<RedLight>* hi = nullptr;
            bindLightParam<VCVLightSlider<RedLight>>("param_OCT_HI_PARAM",
                MonsoonIds::OCT_HI_PARAM, MonsoonIds::OCT_HI_LED,
                std::function<void(VCVLightSlider<RedLight>*)>([&hi](VCVLightSlider<RedLight>* w){ hi = w; }));
            // Guard: bindLightParam only WARNs on a missing anchor, so the slider may be null.
            if (hi) queueModArcLinear(this, module, hi,
                [](const Monsoon::ModViz& m){ return m.octaveHi; },
                [](const Monsoon::ModViz& m){ return m.pitchLane[13]; },
                [](const Monsoon& mm){ return mm.modVizMonsoonMelody; });
        }

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

        // ── Mode button + lights: right strip, bound by ANCHOR ────────────────
        // Anchors come from panel_src/mode_column.py, which also asserts each light row
        // clears the step ring. FIVE lights: Mode E was always selectable (the cycle is
        // (modeSelect+1)%5 and the engine handles modeSelect==4) but had no light, so
        // choosing it turned them all off and looked broken.
        bindParam<TL1105>("param_MODE_PARAM", MonsoonIds::MODE_PARAM);
        for (int i = 0; i < 5; ++i)
            bindLight<MediumLight<YellowLight>>("light_MODE_" + std::string(1, char('A'+i)) + "_LIGHT",
                                                MonsoonIds::MODE_A_LIGHT + i);

        // ── Single control row at y=87: all dice/slew/mix + utility aligned ──
        // ── EXPERIMENT: bottom 3 rows bound by NAME from the panel SVG ───────
        // Discrete controls bind by name via the variadic Compose<> SvgPanelKit.
        // The control-row LIGHTS stay C++-computed (a cheap formula row at ROWYL),
        // same principle as the step ring — wrong job for name-binding.
        const float ROWYL = 93.f;
        const float RX0 = 12.f, RXP = 16.7f;
        auto rx = [&](int i){ return RX0 + i * RXP; };

        // Control row params (positions from SVG; widget types preserved)
        bindParam<Trimpot>  ("param_DICE_SLEW_R_PARAM",   MonsoonIds::DICE_SLEW_R_PARAM,
            std::function<void(Trimpot*)>([this, module](Trimpot* k){ queueModArc(this, module, k, [](const Monsoon::ModViz& m){return m.rhythmSlew;}, [](const Monsoon::ModViz& m){return m.cv3Lane[0];},0.30f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
        bindParam<Trimpot>  ("param_DICE_SLEW_M_PARAM",   MonsoonIds::DICE_SLEW_M_PARAM,
            std::function<void(Trimpot*)>([this, module](Trimpot* k){ queueModArc(this, module, k, [](const Monsoon::ModViz& m){return m.melodySlew;}, [](const Monsoon::ModViz& m){return m.cv3Lane[1];}, 0.30f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
        bindParam<VCVButton>("param_DICE_R_PARAM",        MonsoonIds::DICE_R_PARAM);
        bindParam<VCVButton>("param_DICE_M_PARAM",        MonsoonIds::DICE_M_PARAM);
        bindParam<TrialButton>("param_DICE_TRIAL_R_PARAM",  MonsoonIds::DICE_TRIAL_R_PARAM,
            std::function<void(TrialButton*)>([](TrialButton* b){ b->isMelody = false; }));
        bindParam<TrialButton>("param_DICE_TRIAL_M_PARAM",  MonsoonIds::DICE_TRIAL_M_PARAM,
            std::function<void(TrialButton*)>([](TrialButton* b){ b->isMelody = true; }));
        // Last dice / last trial — same TrialButton (dims + inert on reversible streams,
        // which is correct since Last* is Normal-mode only). These warn-and-skip until
        // the panel SVG gains param_LAST_* markers (panels phase); harmless until then.
        bindParam<TrialButton>("param_LAST_DICE_R_PARAM",   MonsoonIds::LAST_DICE_R_PARAM,
            std::function<void(TrialButton*)>([](TrialButton* b){ b->isMelody = false; }));
        bindParam<TrialButton>("param_LAST_DICE_M_PARAM",   MonsoonIds::LAST_DICE_M_PARAM,
            std::function<void(TrialButton*)>([](TrialButton* b){ b->isMelody = true; }));
        bindParam<TrialButton>("param_LAST_TRIAL_R_PARAM",  MonsoonIds::LAST_TRIAL_R_PARAM,
            std::function<void(TrialButton*)>([](TrialButton* b){ b->isMelody = false; }));
        bindParam<TrialButton>("param_LAST_TRIAL_M_PARAM",  MonsoonIds::LAST_TRIAL_M_PARAM,
            std::function<void(TrialButton*)>([](TrialButton* b){ b->isMelody = true; }));
         bindParam<RDM_KnobSmall>("param_RHYTHM_MIX_PARAM", MonsoonIds::RHYTHM_MIX_PARAM,
            std::function<void(RDM_KnobSmall*)>([this, module](RDM_KnobSmall* k){ queueModArc(this, module, k, [](const Monsoon::ModViz& m){return m.rhythmMix;}, [](const Monsoon::ModViz& m){return m.cv3Lane[2];}, 0.30f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
        bindParam<RDM_KnobSmall>("param_MELODY_MIX_PARAM", MonsoonIds::MELODY_MIX_PARAM,
            std::function<void(RDM_KnobSmall*)>([this, module](RDM_KnobSmall* k){ queueModArc(this, module, k, [](const Monsoon::ModViz& m){return m.melodyMix;}, [](const Monsoon::ModViz& m){return m.cv3Lane[3];}, 0.30f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
        bindParam<TL1105>("param_LOCK_PARAM",             MonsoonIds::LOCK_PARAM);
        bindParam<TL1105>("param_MUTE_PARAM",             MonsoonIds::MUTE_PARAM);
        bindParam<TL1105>("param_RESET_BUTTON_PARAM",     MonsoonIds::RESET_BUTTON_PARAM);
        bindParam<TL1105>("param_RUN_GATE_PARAM",         MonsoonIds::RUN_GATE_PARAM);
        // Control-row lights (computed formula row)
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(rx(2), ROWYL)), module, MonsoonIds::RHYTHM_DICE_LIGHT));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(rx(3), ROWYL)), module, MonsoonIds::MELODY_DICE_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec(rx(8),  ROWYL)), module, MonsoonIds::LOCK_LIGHT));
        addChild(createLightCentered<MediumLight<RedLight>>(  mm2px(Vec(rx(9),  ROWYL)), module, MonsoonIds::MUTE_LIGHT));
        addChild(createLightCentered<MediumLight<BlueLight>>( mm2px(Vec(rx(10), ROWYL)), module, MonsoonIds::RESET_LIGHT));
        addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(rx(11), ROWYL)), module, MonsoonIds::RUN_GATE_LIGHT));

        // Inputs (two rows) — bound by name
        bindInput<PJ301MPort>("input_RUN_GATE_INPUT",      MonsoonIds::RUN_GATE_INPUT);
        bindInput<PJ301MPort>("input_RESET_TRIGGER_INPUT", MonsoonIds::RESET_TRIGGER_INPUT);
        bindInput<PJ301MPort>("input_SEED_INPUT",          MonsoonIds::SEED_INPUT);
        bindInput<PJ301MPort>("input_GATE1_INPUT",         MonsoonIds::GATE1_INPUT);
        bindInput<PJ301MPort>("input_GATE2_INPUT",         MonsoonIds::GATE2_INPUT);
        bindInput<PJ301MPort>("input_GATE3_MOD_INPUT",     MonsoonIds::GATE3_MOD_INPUT);
        bindInput<PJ301MPort>("input_CLK_INPUT",           MonsoonIds::CLK_INPUT);
        bindInput<PJ301MPort>("input_LENGTH_INPUT",        MonsoonIds::LENGTH_INPUT);
        bindInput<PJ301MPort>("input_OFFSET_INPUT",        MonsoonIds::OFFSET_INPUT);
        bindInput<PJ301MPort>("input_CV1_INPUT",           MonsoonIds::CV1_INPUT);
        bindInput<PJ301MPort>("input_CV2_INPUT",           MonsoonIds::CV2_INPUT);
        bindInput<PJ301MPort>("input_CV3_MOD_INPUT",       MonsoonIds::CV3_MOD_INPUT);

        // Output-group accent region: REMOVED (see below).
        {
            // Monsoon* mm = dynamic_cast<Monsoon*>(module);   // only fed the accent's theme fn
            // Output-accent region REMOVED. It implemented a real Rack convention (outputs
            // should read distinctly from inputs), but as a slab at hardcoded mm — 106,97
            // 88x31 — sized by eye rather than derived from the output jacks it covers, so
            // it neither fitted them nor matched the two SVG group boxes it sat beside (all
            // three now gone). Flat fill + hairline outline reads as taped-on paper, not a
            // recess. If in/out differentiation is wanted back, derive the region from the
            // output jack ANCHORS in the generator (cleanup step 5) so it cannot drift:
            // redDot::addOutputAccent still exists and takes a rect.
            //             redDot::addOutputAccent(this, 106.f, 97.f, 88.f, 31.f,
            //                 [mm]() { return mm ? mm->lightTheme : false; });
        }
        // Outputs (two rows) — bound by name
        bindOutput<PJ301MPort>("output_GATE_OUTPUT",          MonsoonIds::GATE_OUTPUT);
        bindOutput<PJ301MPort>("output_TIE_OUTPUT",           MonsoonIds::TIE_OUTPUT);
        bindOutput<PJ301MPort>("output_LEGATO_OUTPUT",        MonsoonIds::LEGATO_OUTPUT);
        bindOutput<PJ301MPort>("output_TIE_OR_LEGATO_OUTPUT", MonsoonIds::TIE_OR_LEGATO_OUTPUT);
        bindOutput<PJ301MPort>("output_ACCENT_OUTPUT",        MonsoonIds::ACCENT_OUTPUT);
        bindOutput<PJ301MPort>("output_CV_OUTPUT",            MonsoonIds::CV_OUTPUT);
        bindOutput<PJ301MPort>("output_SEED_OUTPUT",          MonsoonIds::SEED_OUTPUT);
        bindOutput<PJ301MPort>("output_RUN_GATE_OUTPUT",      MonsoonIds::RUN_GATE_OUTPUT);
        bindOutput<PJ301MPort>("output_RESET_TRIGGER_OUTPUT", MonsoonIds::RESET_TRIGGER_OUTPUT);

        // ── Expander lights ───────────────────────────────────────────────────
        // (Legacy Monsoon-side expander indicator lights removed — each expander
        // now shows its own dot.modular red-dot "connected" light instead.)

        // The big-5 arcs are queued + flushed inside applyTheme() (called above).
        // The slew/mix arcs (and any slider arcs) are queued in THIS constructor
        // AFTER that applyTheme() flush, so they need their own flush here or they
        // never get built. flushModArcs clears the pending list, so this only
        // builds the as-yet-unflushed (slew/mix/slider) overlays.
        flushModArcs(this, dynamic_cast<Monsoon*>(module));
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
        // First load: route through Compose loadPanel so the panel pointer is set and
        // bindParam() can resolve named shapes. (Experiment.)
        loadPanel(panelPath);
    }

    // Remove any existing knob params at the 7 top knob positions
        // so we can re-add with the correct type.
        // Rack allows re-adding after clearing; easiest is to replace
        // the ParamWidget at the known indices. We use the positions directly.
        // Since this is called once at construction (no existing widgets yet),
        // we just add fresh. On theme toggle, widgets are rebuilt via
        // removing children with the affected paramIds first.
        //auto* mod = dynamic_cast<Monsoon*>(module);
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

        // Resolve the specific module type for the helper function
        Monsoon* m = dynamic_cast<Monsoon*>(module);

        // EXPERIMENT: bind the 8 discrete top knobs by NAME from the panel SVG
        // (positions come from named shapes param_*; only the knob graphic varies
        // by theme). Ring + sliders stay C++-computed elsewhere. The named shapes
        // live in res/panels/Monsoon_panel_*_monsoon.svg (components layer).
        if (lightTheme) {
            bindParam<RDM_KnobDarkMedium> ("param_NOTE_VALUE_PARAM",     MonsoonIds::NOTE_VALUE_PARAM,
                std::function<void(RDM_KnobDarkMedium*)>([this, m](RDM_KnobDarkMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.noteValue;}, [](const Monsoon::ModViz& v){return v.big5Lane[0];}, 0.50f,[](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobDarkMedium>("param_VARIATION_PARAM",      MonsoonIds::VARIATION_PARAM,
                std::function<void(RDM_KnobDarkMedium*)>([this, m](RDM_KnobDarkMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.variation;}, [](const Monsoon::ModViz& v){return v.big5Lane[1];},0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobDarkMedium>("param_LEGATO_PARAM",         MonsoonIds::LEGATO_PARAM,
                std::function<void(RDM_KnobDarkMedium*)>([this, m](RDM_KnobDarkMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.legato;}, [](const Monsoon::ModViz& v){return v.big5Lane[2];}, 0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobDarkMedium>("param_REST_PARAM",           MonsoonIds::REST_PARAM,
                std::function<void(RDM_KnobDarkMedium*)>([this, m](RDM_KnobDarkMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.rest;}, [](const Monsoon::ModViz& v){return v.big5Lane[3];}, 0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobDarkMedium>("param_ACCENT_KNOB",          MonsoonIds::ACCENT_KNOB,
                std::function<void(RDM_KnobDarkMedium*)>([this, m](RDM_KnobDarkMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.accent;}, [](const Monsoon::ModViz& v){return v.big5Lane[4];}, 0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));

            bindParam<RDM_KnobSmall>     ("param_BPM_PARAM",            MonsoonIds::BPM_PARAM);
            bindParam<RDM_KnobSmall>     ("param_PATTERN_LENGTH_PARAM", MonsoonIds::PATTERN_LENGTH_PARAM);
            bindParam<RDM_KnobSmall>     ("param_PATTERN_OFFSET_PARAM", MonsoonIds::PATTERN_OFFSET_PARAM);
        } else {
            bindParam<RDM_KnobCreamMedium>("param_NOTE_VALUE_PARAM",     MonsoonIds::NOTE_VALUE_PARAM,
                std::function<void(RDM_KnobCreamMedium*)>([this, m](RDM_KnobCreamMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.noteValue;}, [](const Monsoon::ModViz& v){return v.big5Lane[0];},0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobCreamMedium>("param_VARIATION_PARAM",      MonsoonIds::VARIATION_PARAM,
                std::function<void(RDM_KnobCreamMedium*)>([this, m](RDM_KnobCreamMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.variation;}, [](const Monsoon::ModViz& v){return v.big5Lane[1];},0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobCreamMedium>("param_LEGATO_PARAM",         MonsoonIds::LEGATO_PARAM,
                std::function<void(RDM_KnobCreamMedium*)>([this, m](RDM_KnobCreamMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.legato;}, [](const Monsoon::ModViz& v){return v.big5Lane[2];}, 0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobCreamMedium>("param_REST_PARAM",           MonsoonIds::REST_PARAM,
                std::function<void(RDM_KnobCreamMedium*)>([this, m](RDM_KnobCreamMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.rest;}, [](const Monsoon::ModViz& v){return v.big5Lane[3];}, 0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));
            bindParam<RDM_KnobCreamMedium>("param_ACCENT_KNOB",          MonsoonIds::ACCENT_KNOB,
                std::function<void(RDM_KnobCreamMedium*)>([this, m](RDM_KnobCreamMedium* k){ queueModArc(this, m, k, [](const Monsoon::ModViz& v){return v.accent;}, [](const Monsoon::ModViz& v){return v.big5Lane[4];}, 0.50f, [](const Monsoon& mm){return mm.modVizMonsoonOther;}); }));

            bindParam<RDM_KnobSmall>      ("param_BPM_PARAM",            MonsoonIds::BPM_PARAM);
            bindParam<RDM_KnobSmall>      ("param_PATTERN_LENGTH_PARAM", MonsoonIds::PATTERN_LENGTH_PARAM);
            bindParam<RDM_KnobSmall>      ("param_PATTERN_OFFSET_PARAM", MonsoonIds::PATTERN_OFFSET_PARAM);
        }

        // All big-5 knobs are bound; build their modulation-arc overlays on top.
        flushModArcs(this, m);
    }

void MonsoonWidget::step() {
    ModuleWidget::step();
    kitStep();  // variadic Compose: dev poll-reload
}

void MonsoonWidget::draw(const DrawArgs& args) {
        NVGcontext* vg=args.vg;

        // Force a solid opaque background fill (alpha 255) to prevent transparency
    // Draw solid opaque background to prevent transparency
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(vg, getLightTheme() ? nvgRGBA(0xe6, 0xe6, 0xe6, 255) : nvgRGBA(0x23, 0x23, 0x23, 255));
    nvgFill(vg);

    // (Cluster recess + wells + output-group accent are now SVG panel art —
    // see panel_src/embed_cluster_art.py. The widget only draws TEXT labels,
    // since nanosvg ignores <text>.)

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

        // Label anchored to a named SVG shape (so labels track their controls).
        auto labelAt = [&](const char* shapeId, float dy_mm, const char* txt){
            if (NSVGshape* s = findNamed(shapeId)) {
                Vec c = centerOf(s);
                nvgText(vg, c.x, c.y + mm2px(dy_mm), txt, nullptr);
            }
        };

        // Knob names sit BELOW each dial's own arc labels (1/1, 1/32, 0%, 100%), which the
        // arcLabel() calls place at r*sin(45 deg) ~= 8.5-9.5mm under the knob centre. At dy=12
        // the name spanned 10.3..13.7mm and nearly touched them; 14.5 leaves ~2-3mm clear.
        setNvgFontSize(3.4f); fillNvgColour(200,200,200);
        labelAt("param_NOTE_VALUE_PARAM", 14.5f, "NOTE VALUE");
        labelAt("param_VARIATION_PARAM",  14.5f, "VARIATION");
        labelAt("param_LEGATO_PARAM",     14.5f, "LEGATO");
        labelAt("param_REST_PARAM",       14.5f, "REST");
        labelAt("param_ACCENT_KNOB",      14.5f, "ACCENT");

        auto arcLabel = [&](float cx_mm, float cy_mm, float r_mm, float angle_deg, const char* text, int ri=160, int gi=160, int bi=160) {
            float a=angle_deg*float(M_PI)/180.f, tx=cx_mm+r_mm*std::cos(a), ty=cy_mm+r_mm*std::sin(a);
            nvgSave(vg); nvgTranslate(vg,mm2px(tx),mm2px(ty)); nvgRotate(vg,a+float(M_PI)/2.f);
            if(lt){auto inv=[](int v){return std::max(0,220-v);}; bool isGrey=(std::abs(ri-gi)<20&&std::abs(gi-bi)<20); if(isGrey){ri=inv(ri);gi=inv(gi);bi=inv(bi);}else{ri=ri*7/10;gi=gi*7/10;bi=bi*7/10;}}
            nvgFillColor(vg,nvgRGBA(ri,gi,bi,200)); nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
            nvgText(vg,0,0,text,nullptr); nvgRestore(vg);
        };

        setNvgFontSize(2.5f);
        { for(int i=0;i<NUM_NOTE_VALUES;++i) arcLabel(16.f,22.f,13.5f,-225.f+i*(270.f/(NUM_NOTE_VALUES-1)),NOTE_VALUES[i].label,150,150,135); }
        setNvgFontSize(2.8f);
        arcLabel(42.f,22.f,13.f,-225.f,"LONGER",130,130,120); arcLabel(42.f,22.f,13.f,45.f,"SHORTER",130,130,120);
        arcLabel(68.f,22.f,12.f,-225.f,"0%",130,130,120);     arcLabel(68.f,22.f,12.f,45.f,"100%",130,130,120);
        arcLabel(94.f,22.f,12.f,-225.f,"0%",130,130,120);     arcLabel(94.f,22.f,12.f,45.f,"100%",130,130,120);
        arcLabel(120.f,22.f,12.f,-225.f,"0%",130,130,120);     arcLabel(120.f,22.f,12.f,45.f,"100%",130,130,120);

        // Seq knob labels (below ring)
        setNvgFontSize(3.2f); fillNvgColour(170,170,170);
        labelAt("param_BPM_PARAM", 12.f, "BPM"); labelAt("param_PATTERN_LENGTH_PARAM", 12.f, "LEN"); labelAt("param_PATTERN_OFFSET_PARAM", 12.f, "OFFSET");

        // ── Control-row labels (single row at y=87; lights/labels above) ──────
        // Slots: SLEW R/M, DICE R/M, TRIAL R/M, MIX R/M, LOCK, MUTE, RESET, RUN.
        // rx(i) = 12 + i*16.7. Labels sit just above the row (y≈81).
        setNvgFontSize(2.4f); fillNvgColour(150,150,140);
        auto rowLbl=[&](int i,const char* s){ writeNvgText(12.f+i*16.7f, 81.f, s); };
        rowLbl(0,"SLEW R"); rowLbl(1,"SLEW M");
        rowLbl(2,"DICE R"); rowLbl(3,"DICE M");
        rowLbl(4,"TRIAL R"); rowLbl(5,"TRIAL M");
        rowLbl(6,"MIX R"); rowLbl(7,"MIX M");
        rowLbl(8,"LOCK"); rowLbl(9,"MUTE"); rowLbl(10,"RESET"); rowLbl(11,"RUN");

        // Semitone note labels
        setNvgFontSize(3.0f);
        const char* sn[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for(int i=0;i<12;++i){ fillNvgColour(200,200,200); writeNvgText(7.5f+i*9.f,43.f,sn[i]); }
        // Fader numerals: sit just under the sliders. They used to be at SL_TOP+SLH+6 = 80.5mm,
        // i.e. 0.5mm from the control-row labels at y=81 -- hence the overlap with SLEW/DICE/
        // TRIAL. Pulled up to 2.8mm below the travel, which leaves ~3.7mm clear of that row.
        // Brightness raised 85 -> 165: at 85 they read far fainter than the note names above.
        setNvgFontSize(2.7f); fillNvgColour(165,165,165);
        const char* nums[12]={"1","2","3","4","5","6","7","8","9","10","11","12"};
        for(int i=0;i<12;++i) writeNvgText(7.5f+i*9.f, SL_TOP+SLH+2.8f, nums[i]);
        setNvgFontSize(2.9f); fillNvgColour(38,166,154);
        writeNvgText(119.f,43.f,"LO"); writeNvgText(128.f,43.f,"HI");

        // Slider level ticks are SVG panel art now, emitted by
        // panel_src/fader_level_markers.py from the same loop as the fader anchors.
        // The runtime drawer that used to live here was a SECOND tick system: 9 levels x 14
        // faders at hw 2.2/1.4mm centred ON each slider, off its own hardcoded tickXs[14].
        // It is what drew lines through the tracks, and it was a third copy of the fader
        // positions (widget mm, tickXs, SVG). Do not reintroduce it.

        // Mode. The letters used to be drawn at EXACTLY the light coordinates, so each glyph
        // covered its own LED: the letter appeared to light up instead of the light, and on
        // the light theme a near-black glyph on an unlit light's dark circle vanished. They
        // now sit OUTBOARD of the LED column, anchored to the same shapes the lights bind to
        // so they cannot drift apart again.
        setNvgFontSize(3.2f); fillNvgColour(210,210,210); writeNvgText(194.f,7.f,"MODE");
        setNvgFontSize(3.4f); fillNvgColour(185,185,185);
        for (int i = 0; i < 5; ++i) {
            const std::string id = "light_MODE_" + std::string(1, char('A'+i)) + "_LIGHT";
            if (NSVGshape* sh = findNamed(id.c_str())) {
                Vec c = centerOf(sh);
                const char t[2] = { char('A'+i), 0 };
                nvgText(vg, c.x + mm2px(6.5f), c.y, t, nullptr);
            }
        }

        // (Legacy S/D/P expander-light labels removed along with their lights.)

        // ── Labels derived from the SAME named SVG shapes the controls bind to,
        //    so they can never fall behind a control reorg again. (labelAt is
        //    defined above.) ───────────────────────────────────────────────────

        // Control-row button labels (below each control)
        setNvgFontSize(2.7f); fillNvgColour(200,60,60);
        labelAt("param_DICE_R_PARAM", 6.5f, "DICE R");
        labelAt("param_DICE_M_PARAM", 6.5f, "DICE M");
        fillNvgColour(190,190,190);
        labelAt("param_LOCK_PARAM",         6.5f, "LOCK");
        labelAt("param_MUTE_PARAM",         6.5f, "MUTE");
        labelAt("param_RESET_BUTTON_PARAM", 6.5f, "RESET");
        labelAt("param_RUN_GATE_PARAM",     6.5f, "RUN");

        // Jack labels (above each jack) — names now MATCH the reorganised jacks
        // (incl. the added GATE3 / CV3), because they read the same shapes.
        const float PR = 7.7f/2.f, JLBL = -(PR + 2.2f);
        setNvgFontSize(2.7f); fillNvgColour(195,195,195);
        labelAt("input_RUN_GATE_INPUT",      JLBL, "RUN");
        labelAt("input_RESET_TRIGGER_INPUT", JLBL, "RST");
        labelAt("input_SEED_INPUT",          JLBL, "SEED");
        labelAt("input_GATE1_INPUT",         JLBL, "G1");
        labelAt("input_GATE2_INPUT",         JLBL, "G2");
        labelAt("input_GATE3_MOD_INPUT",     JLBL, "G3");
        labelAt("input_CLK_INPUT",           JLBL, "CLK");
        labelAt("input_LENGTH_INPUT",        JLBL, "LEN");
        labelAt("input_OFFSET_INPUT",        JLBL, "OFF");
        labelAt("input_CV1_INPUT",           JLBL, "CV1");
        labelAt("input_CV2_INPUT",           JLBL, "CV2");
        labelAt("input_CV3_MOD_INPUT",       JLBL, "CV3");
        fillNvgColour(180,180,180);
        labelAt("output_GATE_OUTPUT",          JLBL, "GATE");
        labelAt("output_TIE_OUTPUT",           JLBL, "TIE");
        labelAt("output_LEGATO_OUTPUT",        JLBL, "LEG");
        labelAt("output_TIE_OR_LEGATO_OUTPUT", JLBL, "T|L");
        labelAt("output_ACCENT_OUTPUT",        JLBL, "ACC");
        labelAt("output_CV_OUTPUT",            JLBL, "CV");
        labelAt("output_SEED_OUTPUT",          JLBL, "SEED");
        labelAt("output_RUN_GATE_OUTPUT",      JLBL, "RUN");
        labelAt("output_RESET_TRIGGER_OUTPUT", JLBL, "RST");
    }

void MonsoonWidget::appendContextMenu(ui::Menu* menu) {
        auto* m = dynamic_cast<Monsoon*>(module);
        if (!m) return;
        setDevMode(true);
        appendKitMenu(menu);
        menu->addChild(new ui::MenuSeparator);
        struct ThemeItem : ui::MenuItem {
            MonsoonWidget* widget = nullptr;
            void onAction(const event::Action&) override { widget->setLightTheme(!widget->getLightTheme()); widget->applyTheme(); }
            void step() override { rightText = widget->getLightTheme() ? "Light ✔" : "Dark ✔"; ui::MenuItem::step(); }
        };
        auto* themeItem = createMenuItem<ThemeItem>("Theme"); themeItem->widget = this; menu->addChild(themeItem);
        menu->addChild(new ui::MenuSeparator);

        // Single spread-interpolation target toggle (was per-visual on East/Macro;
        // now the one authoritative control, here on Monsoon).
        menu->addChild(createMenuLabel("Spread interpolation target"));
        struct SpreadTargetItem : ui::MenuItem {
            Monsoon* module = nullptr;
            bool valueMono = false;   // this item represents Mono Draw vs Avg Poly
            void onAction(const event::Action&) override { if (module) module->spreadInterpMono = valueMono; }
            void step() override { if (module) rightText = (module->spreadInterpMono == valueMono) ? "✔" : ""; ui::MenuItem::step(); }
        };
        { auto* avg = createMenuItem<SpreadTargetItem>("Average poly");
          avg->module = m; avg->valueMono = false; menu->addChild(avg);
          auto* mono = createMenuItem<SpreadTargetItem>("Mono draw");
          mono->module = m; mono->valueMono = true; menu->addChild(mono); }
        menu->addChild(new ui::MenuSeparator);

        // Modulation-arc visibility, by surface. Each flag toggles whether mod arcs
        // are drawn on that surface (default all on).
        struct ModVizFlagItem : ui::MenuItem {
            bool* flag = nullptr;
            void onAction(const event::Action&) override { if (flag) *flag = !*flag; }
            void step() override { rightText = (flag && *flag) ? "✔" : ""; ui::MenuItem::step(); }
        };
        menu->addChild(createSubmenuItem("Modulation arcs", "", [=](ui::Menu* sm) {
            auto add = [&](const char* label, bool* f) {
                auto* it = createMenuItem<ModVizFlagItem>(label); it->flag = f; sm->addChild(it);
            };
            add("Monsoon — pitch/octave sliders", &m->modVizMonsoonMelody);
            add("Monsoon — knobs (probability, slew, mix)", &m->modVizMonsoonOther);
            sm->addChild(new ui::MenuSeparator);
            add("Straits East (per-voice)", &m->modVizEast);
            add("Straits West (per-voice)", &m->modVizWest);
            add("Sands Macro (spread)", &m->modVizMacro);
            add("Sands Mono (spread)", &m->modVizMono);
        }));
        // Rhythm behaviour: the two leading-edge policy toggles (RHYTHM_BEHAVIOUR_POLICIES.md).
        // Reuses ModVizFlagItem (a plain bool* toggle) pointed at the engine flags.
        menu->addChild(createSubmenuItem("Rhythm behaviour", "", [=](ui::Menu* sm) {
            auto add = [&](const char* label, bool* f) {
                auto* it = createMenuItem<ModVizFlagItem>(label); it->flag = f; sm->addChild(it);
            };
            // ON (default): a rest cancels a committed slur (N+1 silent). OFF: slur wins, the
            // rest is ignored and N+1 plays as a legato/tie of its own drawn pitch.
            add("Rest beats legato", &m->engine.restBeatsLegato);
            sm->addChild(new ui::MenuSeparator);
            // OFF (default): CONTINUE — gate carries across the loop, laps can differ. ON:
            // INTERRUPT — reset at the boundary, every lap identical (no cross-lap memory).
            add("Interrupt at phrase boundary", &m->engine.boundaryInterrupt);
            sm->addChild(new ui::MenuSeparator);
            // OFF (default): every poly voice uses mono's note length, as before. ON: each voice
            // derives its own note length from its VARIATION LOR window onto the SHARED mono
            // variation array, CLAMPED so it can never hold past mono's next event. Articulation is
            // therefore subtractive (voices release early, never late) — the clock stays mono's.
            // Also gates Rule 2: each voice rolls its own leading-edge legato from its LEGATO LOR
            // window and connects/re-articulates independently (EAST_EXTRA_LANES §4d). Silent unless
            // a voice's VAR/LEG is set Local East on the East expander. See EAST_EXTRA_LANES.md.
            add("Per-voice articulation (East VARIATION/LEGATO)", &m->engine.perVoiceArticulation);
        }));
        // Lane direction context menu removed (step 4 of lane_direction_homes.md).
        // Direction is now expander-homed — set it via DirCell or gate-mod on the Sands panels.
        // The flip-quant setting is still accessible; it's a global engine property.
        {
            struct FlipQuantItem : ui::MenuItem {
                Monsoon* module = nullptr; SequencerEngine::LaneFlipQuant value{};
                void onAction(const event::Action&) override { if (module) module->engine.laneFlipQuant = value; }
                void step() override { if (module) rightText = (module->engine.laneFlipQuant == value) ? "✔" : ""; ui::MenuItem::step(); }
            };
            menu->addChild(createSubmenuItem("Lane flip quant", "", [=](ui::Menu* sm) {
                auto addQ = [&](const char* label, SequencerEngine::LaneFlipQuant v) {
                    auto* it = createMenuItem<FlipQuantItem>(label); it->module = m; it->value = v; sm->addChild(it);
                };
                addQ("Flip at phrase boundary", SequencerEngine::LaneFlipQuant::Phrase);
                addQ("Flip at step edge",       SequencerEngine::LaneFlipQuant::StepEdge);
            }));
        }
        menu->addChild(new ui::MenuSeparator);
        struct IntItem : ui::MenuItem {
            Monsoon* module; int* target; int value;
            void onAction(const event::Action&) override { if (module && target) *target = value; }
            void step() override { rightText = (target && *target == value) ? "✔" : ""; ui::MenuItem::step(); }
        };
        struct BoolToggle : ui::MenuItem {
            int* target = nullptr;
            void onAction(const event::Action&) override { if (target) *target = (*target ? 0 : 1); }
            void step() override { rightText = (target && *target) ? "✔" : ""; ui::MenuItem::step(); }
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
              const char* n[] = {"A: Sequencer","B: Seq + Gate","C: Quantizer 1","D: Quantizer 2","E: Phase (CV1)"};
              for (int v=0;v<5;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->modeSelect;it->value=v;sub->addChild(it);} }

            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "Reversible (Mode E phase)"; sub->addChild(l);
              // Per-stream Normal/Reversible. Reversible = pure dice, signed index,
              // forward/back; blocks trial + reseed-on-roll for that stream.
              const char* rm[] = {"Normal","Reversible"};
              { auto* ll = new ui::MenuLabel; ll->text = "  Rhythm"; sub->addChild(ll); }
              for (int v=0;v<2;++v){auto* it=createMenuItem<IntItem>(rm[v]);it->module=m;it->target=&m->rhythmReversibleMode;it->value=v;sub->addChild(it);}
              { auto* ll = new ui::MenuLabel; ll->text = "  Melody"; sub->addChild(ll); }
              for (int v=0;v<2;++v){auto* it=createMenuItem<IntItem>(rm[v]);it->module=m;it->target=&m->melodyReversibleMode;it->value=v;sub->addChild(it);}
              sub->addChild(new ui::MenuSeparator);
              // Global on entering reversible: reseed (+zero index), or just zero index.
              { auto* it=createMenuItem<BoolToggle>("Reseed on mode change");it->target=&m->reseedOnModeChange;sub->addChild(it); }
              { auto* it=createMenuItem<BoolToggle>("Reset index on mode change");it->target=&m->resetIndexOnModeChange;
                it->disabled = (m->reseedOnModeChange != 0);   // greyed when reseed handles it
                sub->addChild(it); }
            }

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
                // Reseed-on-roll is inert when BOTH streams are reversible (reversible
                // blocks it per stream); grey it then to signal that.
                auto* ror = createBoolPtrMenuItem("Reseed on roll (main dice)", "", &m->reseedOnRoll);
                ror->disabled = (m->rhythmReversibleMode != 0 && m->melodyReversibleMode != 0);
                sub->addChild(ror);
                sub->addChild(createBoolPtrMenuItem("Reseed on restart", "", &m->reseedOnRestart));
            }

            sub->addChild(new ui::MenuSeparator);
            {
                auto* l = new ui::MenuLabel; l->text = "Which dice live mode drives"; sub->addChild(l);
                // Live "trial as source" is blocked on a reversible stream — grey the
                // matching per-stream toggle.
                auto* rt = createBoolPtrMenuItem("Rhythm: trial (else main)", "", &m->rhythmLiveTrial);
                rt->disabled = (m->rhythmReversibleMode != 0);
                sub->addChild(rt);
                auto* mt = createBoolPtrMenuItem("Melody: trial (else main)", "", &m->melodyLiveTrial);
                mt->disabled = (m->melodyReversibleMode != 0);
                sub->addChild(mt);
            }

        }));

        // menu->addChild(createSubmenuItem("DNA Rotation", "", [=](ui::Menu* sub) {
        //     auto addRot = [=](ui::Menu* m, const char* label, std::function<void(int)> func) {
        //         m->addChild(createSubmenuItem(label, "", [=](ui::Menu* s) {
        //             s->addChild(createMenuItem("Rotate Forward (+1)", "", [=]() { func(1); }));
        //             s->addChild(createMenuItem("Rotate Backward (-1)", "", [=]() { func(-1); }));
        //         }));
        //     };

        //     addRot(sub, "Rhythm (Gates/Rests)", [=](int s) { m->rotateRhythm(s); }); // Individual
        //     addRot(sub, "Variation (Lengths)", [=](int s) { m->rotateVariation(s); });
        //     addRot(sub, "Legato/Tie", [=](int s) { m->rotateLegato(s); });
        //     sub->addChild(new ui::MenuSeparator);
        //     addRot(sub, "Melody (Pitch)", [=](int s) { m->rotateMelody(s); }); // Individual
        //     addRot(sub, "Octave", [=](int s) { m->rotateOctave(s); });
        //     sub->addChild(new ui::MenuSeparator);
        //     addRot(sub, "Rotate Rhythm Pattern", [=](int s) { m->rotateRhythmPattern(s); }); // Combined
        //     addRot(sub, "Rotate Melody Pattern", [=](int s) { m->rotateMelodyPattern(s); }); // Combined
        //     sub->addChild(new ui::MenuSeparator);
        //     sub->addChild(createMenuItem("Rotate EVERYTHING (+1)", "", [=]() { 
        //         m->rotateRhythmPattern(1); m->rotateMelodyPattern(1);
        //     }));
        //     sub->addChild(new ui::MenuSeparator);
        //     sub->addChild(createMenuItem("Scramble Rhythm DNA", "", [=]() { m->dnaManager.scrambleRhythmGroup(); }));
        //     sub->addChild(createMenuItem("Scramble Melody DNA", "", [=]() { m->dnaManager.scrambleMelodyGroup(); }));
        //     sub->addChild(createMenuItem("Scramble ALL DNA (Remix)", "DNA Remix", [=]() { m->dnaManager.scrambleAll(); }));
        //     sub->addChild(new ui::MenuSeparator);
        //     sub->addChild(createMenuItem("Reset DNA Alignment", "Original Draw", [=]() { m->dnaManager.resetAll(); }));
        // }));

        menu->addChild(createSubmenuItem("CV Assign", "", [=](ui::Menu* sub) {
            { auto* l = new ui::MenuLabel; l->text = "CV IN 1"; sub->addChild(l);
              const char* n[] = {"Add Seq","Transpose Seq","Mod Range LO","Mod Range HI","BPM Mod"}; // Added BPM Mod
              for (int v=0;v<5;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv1Mode;it->value=v;sub->addChild(it);} }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "CV IN 2"; sub->addChild(l);
              const char* n[] = {"Note value","Variation","Legato","Rest","Accent"}; // Added Accent
              for (int v=0;v<5;++v){auto* it=createMenuItem<IntItem>(n[v]);it->module=m;it->target=&m->cv2Mode;it->value=v;sub->addChild(it);} }
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
              const char* n3[] = {"Trial rhythm die","Trial melody die","Toggle reseed-on-roll","Toggle reseed-on-restart",
                                  "Toggle rhythm live source","Toggle melody live source"};
              for (int v=0;v<6;++v){auto* it=createMenuItem<IntItem>(n3[v]);it->module=m;it->target=&m->gate3Target;it->value=v;sub->addChild(it);} }
        }));

        menu->addChild(createSubmenuItem("Probability outs", "", [=](ui::Menu* sub) {
            sub->addChild(createMenuLabel("Sands visual expander CV outs"));
            sub->addChild(createIndexSubmenuItem("Output range",
                {"0-1 V", "0-5 V", "0-10 V"},
                [=]() { return m->probOutScale; },
                [=](int i) { m->probOutScale = i; }));
            sub->addChild(createIndexSubmenuItem("Output mode",
                {"Continuous", "Sample & hold (per step)"},
                [=]() { return m->probOutSampleHold ? 1 : 0; },
                [=](int i) { m->probOutSampleHold = (i == 1); }));
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
              add("Allow 1/4T",0); add("Allow 1/8T",1); add("Allow 1/32",2); }
            sub->addChild(new ui::MenuSeparator);
            { auto* l = new ui::MenuLabel; l->text = "PPQN"; sub->addChild(l);
              struct PItem : ui::MenuItem { Monsoon* module=nullptr; int value=4;
                void onAction(const event::Action&) override { if(module) module->ppqnSetting=value; }
                void step() override { if(module) rightText=(module->ppqnSetting==value)?"✔":""; ui::MenuItem::step(); } };
              for (int v : {24,48,96}){auto* it=createMenuItem<PItem>(string::f("%d",v).c_str());it->module=m;it->value=v;sub->addChild(it);} }
        }));
    }