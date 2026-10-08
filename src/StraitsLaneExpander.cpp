// StraitsLaneExpander — generic per-lane panel extension widget.
// This is a REFACTOR of the working Straits widget (MonsoonStraitsExpander.cpp):
// the knob binding loop, mod-arc overlay, dim/lock logic, and theme following
// are all COPIED from MonsoonStraitsExpanderWidget and parameterised by
// LaneDescriptor. No new per-voice-read or binding logic — it all exists in Straits.
//
// See docs/design/STRAITS_CAUSEWAY_LANE_EXTENSIONS.md REFINED MODEL + METHOD:REFACTOR.
#include <rack.hpp>
#include <vector>
#include <tuple>
#include <cmath>
#include "Monsoon.hpp"
#include "StraitsLaneExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/ModArcOverlay.hpp"
#include "ui/Controls.hpp"

using namespace rack;
using namespace MonsoonIds;

// ── Lane descriptors — read off existing Straits bank differences ────────────
static const LaneDescriptor QMIX_DESCRIPTOR = {
    "StraitsLaneQMIX", "param_qmix_", "Q-MIX",
    MonsoonIds::QMIX_LEVEL_PARAM, MonsoonIds::POLY_QMIX_PARAM_1, 2, "qmix"
};
static const LaneDescriptor REST_DESCRIPTOR = {
    "StraitsLaneREST", "param_rest_", "Rest",
    MonsoonIds::REST_PARAM, MonsoonIds::POLY_REST_PARAM_1, 0, "rest"
};
static const LaneDescriptor ACCENT_DESCRIPTOR = {
    "StraitsLaneACCENT", "param_accent_", "Accent",
    MonsoonIds::ACCENT_KNOB, MonsoonIds::POLY_ACCENT_PARAM_1, 1, "accent"
};
static const LaneDescriptor VARIATION_DESCRIPTOR = {
    "StraitsLaneVARIATION", "param_variation_", "Variation",
    MonsoonIds::VARIATION_PARAM, MonsoonIds::POLY_VARIATION_PARAM_1, 5, "variation"
};
static const LaneDescriptor LEGATO_DESCRIPTOR = {
    "StraitsLaneLEGATO", "param_legato_", "Legato",
    MonsoonIds::LEGATO_PARAM, MonsoonIds::POLY_LEGATO_PARAM_1, 6, "legato"
};

struct StraitsLaneExpanderWidget : ModuleWidget,
    dotModular::Compose<StraitsLaneExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    bool themeLight_ = false;
    int activeVoices_ = -1;
    const LaneDescriptor* desc;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    void onButton(const event::Button& e) override {
        ModuleWidget::onButton(e);
        if (e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            auto* lane = dynamic_cast<StraitsLaneExpander*>(module);
            if (lane) lane->beingDragged = (e.action == GLFW_PRESS && !e.isConsumed());
        }
    }

    void onDragEnd(const event::DragEnd& e) override {
        ModuleWidget::onDragEnd(e);
        // Set our own needsResnap flag — the base's step() checks it
        if (module) {
            auto* lane = dynamic_cast<StraitsLaneExpander*>(module);
            if (lane) lane->needsResnap = true;
        }
    }

    // Per-voice mod-arc overlays — COPIED from MonsoonStraitsExpanderWidget.
    // lane index = desc->arcLane (0=REST, 1=ACCENT, 2=QMIX).
    std::vector<std::tuple<rack::ParamWidget*, int, int>> pendingArcs;  // (knob, voice, lane)
    void queueArc(rack::ParamWidget* knob, int voice, int lane) {
        if (knob) pendingArcs.push_back({knob, voice, lane});
    }
    void flushArcs() {
        for (auto& pr : pendingArcs) {
            rack::ParamWidget* knob = std::get<0>(pr);
            int voice = std::get<1>(pr);
            int lane  = std::get<2>(pr);
            auto* self = this;
            auto* arc = new redDot::ModArcOverlay();
            arc->radius = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->attachOverKnob(knob, mm2px(2.5f));
            // COPIED from MonsoonStraitsExpanderWidget — set/mod/isActive logic.
            // voice == -1 → MONO lane (voice 1); voice 0..14 → poly voices 2..16.
            // lane = desc->arcLane (REST=0, ACCENT=1, QMIX=2).
            auto setOf = [self, lane](Monsoon* m, int voice) -> float {
                if (voice == -1) {
                    switch (lane) {
                        case 0: return m->getMonoRestBase();
                        case 1: return m->getMonoAccentBase();
                        case 2: return m->getMonoQmixBase();
                        case 5: return m->getMonoVariationBase();
                        case 6: return m->getMonoLegatoBase();
                        default: return 0.f;
                    }
                }
                if (voice < 0 || voice >= 15) return 0.f;
                switch (lane) {
                    case 0: return m->getBasePolyRest(voice);
                    case 1: return m->getBasePolyAccent(voice);
                    case 2: return m->getBasePolyQmix(voice);
                    case 5: return m->getBasePolyVariation(voice);
                    case 6: return m->getBasePolyLegato(voice);
                    default: return 0.f;
                }
            };
            auto modOf = [self, lane](Monsoon* m, int voice) -> float {
                if (voice == -1) {
                    switch (lane) {
                        case 0: return m->getRestParam();
                        case 1: return m->getAccentParam();
                        case 2: return m->getQmixParam();
                        case 5: return m->getVariationParam();
                        case 6: return m->getLegatoParam();
                        default: return 0.f;
                    }
                }
                if (voice < 0 || voice >= 15) return 0.f;
                switch (lane) {
                    case 0: return m->getEffectivePolyRest(voice);
                    case 1: return m->getEffectivePolyAccent(voice);
                    case 2: return m->getEffectivePolyQmix(voice);
                    case 5: return m->getEffectivePolyVariation(voice);
                    case 6: return m->getEffectivePolyLegato(voice);
                    default: return 0.f;
                }
            };
            arc->getSetNorm = [self, voice, setOf]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                return m ? setOf(m, voice) : 0.f;
            };
            arc->getModNorm = [self, voice, modOf]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                return m ? modOf(m, voice) : 0.f;
            };
            arc->isActive = [self, voice, setOf, modOf]() -> bool {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                if (!m || !m->modVizEast) return false;
                if (voice >= 0 && self->activeVoices_ >= 0 && voice >= self->activeVoices_) return false;
                if (voice >= 0 && (voice < 0 || voice >= 15)) return false;
                return std::fabs(modOf(m, voice) - setOf(m, voice)) > 1e-4f;
            };
            addChild(arc);
        }
        pendingArcs.clear();
    }

    StraitsLaneExpanderWidget(StraitsLaneExpander* mod) {
        setModule(mod);
        // Browser preview creates widgets with mod=nullptr; guard against that.
        if (!mod) {
            // Load a default panel so the preview doesn't crash.
            loadPanel(asset::plugin(pluginInstance, "res/panels/StraitsLane_qmix_dark.svg"));
            return;
        }
        desc = mod->desc;

        // Panel SVG paths (per-lane tint)
        std::string darkPath  = std::string("res/panels/StraitsLane_") + desc->tintKey + "_dark.svg";
        std::string lightPath = std::string("res/panels/StraitsLane_") + desc->tintKey + "_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Voice 0 = mono: LOCKED knob that MIRRORS Monsoon's mono param ──────
        // COPIED from MonsoonStraitsExpanderWidget — only the param ID and anchor
        // are parameterised by desc.
        bindParam<redDot::Themed_Compact_Cog_Dim>(std::string(desc->anchorPrefix) + "0", desc->monoParamId,
            std::function<void(redDot::Themed_Compact_Cog_Dim*)>([this](redDot::Themed_Compact_Cog_Dim* k){
                k->lightWhen = [this](){ return themeLight_; };
                k->lockWhen = [](){ return true; };
                k->displayValueFn = [this]() -> float {
                    Monsoon* m = redDot::findMonsoonEitherSide(module);
                    return m ? m->params[desc->monoParamId].getValue() : NAN;
                };
                queueArc(k, -1, desc->arcLane);
            }));

        // ── Voices 1..15 = poly: editable per-voice knobs ──────────────────────
        // COPIED from MonsoonStraitsExpanderWidget — the loop body is identical
        // except param IDs and anchors are parameterised by desc.
        for (int i = 1; i < 16; i++) {
            std::string r = std::to_string(i);
            int polyIdx = i - 1;
            int voiceNum = i;
            auto dimIfInactive = [this, voiceNum](){
                return activeVoices_ >= 0 && voiceNum > activeVoices_;
            };
            bindParam<redDot::Themed_Compact_Cog_Dim>(std::string(desc->anchorPrefix) + r,
                desc->polyParamIdBase + polyIdx,
                std::function<void(redDot::Themed_Compact_Cog_Dim*)>([this, polyIdx, dimIfInactive](redDot::Themed_Compact_Cog_Dim* k){
                    k->lightWhen = [this](){ return themeLight_; };
                    k->dimWhen   = dimIfInactive;
                    k->lockWhen  = dimIfInactive;
                    queueArc(k, polyIdx, desc->arcLane);
                }));
        }

        flushArcs();

        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            // Lane expander's connect mark indicates its OWN next-hop: the left neighbour
            // is the StraitsR base. Chain: QMIX → base → Monsoon. Each link's mark shows
            // its own hop, so the first unlit mark = break point.
            connectMark->connected = [this]() {
                if (!module) return false;
                auto* left = module->leftExpander.module;
                if (!left) return false;
                // Light if left neighbour is the base OR another lane expander
                // (chain: QMIX → lane → ... → base → Monsoon)
                return left->model == modelStraitsBase
                    || left->model == modelStraitsLaneQMIX
                    || left->model == modelStraitsLaneREST
                    || left->model == modelStraitsLaneACCENT
                    || left->model == modelStraitsLaneVARIATION
                    || left->model == modelStraitsLaneLEGATO;
            };
            addChild(connectMark);
        }
    }

    void step() override {
        // COPIED from MonsoonStraitsExpanderWidget — theme + active-voice tracking.
        // The lane expander finds the BASE (leftExpander) for voice count, and
        // Monsoon (findMonsoonEitherSide) for theme + mod-arc values.
        if (module) {
            // Voice count: read from the base (leftExpander), which owns it.
            // The base is a MonsoonStraitsExpander; its VOICE_COUNT_PARAM drives
            // Monsoon's numPolyVoices. We read activeVoices_ from Monsoon directly.
            Monsoon* mm = redDot::findMonsoonEitherSide(module);
            themeLight_ = (mm && mm->lightTheme);
            if (mm) {
                activeVoices_ = mm->engine.numPolyVoices;
            } else {
                activeVoices_ = -1;
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

// ── Lane module subclasses — one per descriptor, so createModel sets the desc ─
// Each lane is a separate slug so the user can add exactly the lanes they want.
// The module subclass sets the descriptor in its constructor; the widget reads
// it from mod->desc.

struct StraitsLaneQMIXModule : StraitsLaneExpander {
    StraitsLaneQMIXModule() { setDescriptor(&QMIX_DESCRIPTOR); }
};
struct StraitsLaneRESTModule : StraitsLaneExpander {
    StraitsLaneRESTModule() { setDescriptor(&REST_DESCRIPTOR); }
};
struct StraitsLaneACCENTModule : StraitsLaneExpander {
    StraitsLaneACCENTModule() { setDescriptor(&ACCENT_DESCRIPTOR); }
};
struct StraitsLaneVARIATIONModule : StraitsLaneExpander {
    StraitsLaneVARIATIONModule() { setDescriptor(&VARIATION_DESCRIPTOR); }
};
struct StraitsLaneLEGATOModule : StraitsLaneExpander {
    StraitsLaneLEGATOModule() { setDescriptor(&LEGATO_DESCRIPTOR); }
};

Model* modelStraitsLaneQMIX =
    createModel<StraitsLaneQMIXModule, StraitsLaneExpanderWidget>("StraitsLaneQMIX");
Model* modelStraitsLaneREST =
    createModel<StraitsLaneRESTModule, StraitsLaneExpanderWidget>("StraitsLaneREST");
Model* modelStraitsLaneACCENT =
    createModel<StraitsLaneACCENTModule, StraitsLaneExpanderWidget>("StraitsLaneACCENT");
Model* modelStraitsLaneVARIATION =
    createModel<StraitsLaneVARIATIONModule, StraitsLaneExpanderWidget>("StraitsLaneVARIATION");
Model* modelStraitsLaneLEGATO =
    createModel<StraitsLaneLEGATOModule, StraitsLaneExpanderWidget>("StraitsLaneLEGATO");
