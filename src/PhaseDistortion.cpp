// PhaseDistortion.cpp — the standalone phase-distortion module's process loop +
// widget (PHASE_DISTRIBUTION_SPEC §13, docs/design/CC_PROMPT_PHASE_DISTORTION_MODULE.md —
// the module-only comparison build, B; the expander model A stays untouched).
//
// THE PANEL follows the distribution-module conventions (the §13 prompt's Task 2):
// the SandsDistributionExpander's param axis VERBATIM — P1 P2 TILT LEVEL MASS |
// GAPLOC GAPSIZE | WARP (the gap pair immediately beside the atom pair, WARP
// trailing), store-backed knobs (StoreKnob — the de-param law), a focus button per
// channel (which set the permanent DistributionCurveWidget editor shows), and the
// §10 effective-plot refresh shape (the reading follows the SIGN of w, the markers
// exchange on the dual side, the always-K density view fills the warp mix). The
// generator is panel_src/gen_sands_phase_distortion.py — ONE geometry, two files
// (this widget + the script) kept in sync, the expander's discipline.
//
// THE KIND MENU: the bare palette MINUS Drawn. The phase store has no drawn-curve
// pool — its commit path lowers "drawn"/unknown to IdentityWarp (the store codec's
// law, shared with the expander build), so a Drawn entry would lie. The menu mints
// members through WarpCurvePopup::makeKindDefault (the ONE default factory) lowered
// by lowerWarp (the ONE lowering) — no Monsoon bank involved (B's members are
// parametric-only). openWarpKindMenu itself is Monsoon-bank-bound (its KindItem
// commits through m->editor.warp.fromDistribution), which is why this module owns a
// small menu loop aimed at ITS OWN store; the VALUE authorities stay shared.

#include "PhaseDistortion.hpp"

#include <cmath>
#include <cstring>

#include "ui/SvgPanelKit.hpp"        // Compose<ShapeQuery, Bind, Reload> + loadPanel/findNamed
#include "ui/StoreBound.hpp"         // bindStoreKnob (the de-param law)
#include "ui/Controls.hpp"           // Tag_Grey_Trim_Bar (the knob face)
#include "ui/WarpCurvePopup.hpp"     // WarpCurveEditor + makeKindDefault + distFromWarp
#include "ui/DistributionCurveWidget.hpp"
#include "ui/ModuleLookup.hpp"       // lookupMonsoon (the theme follows the network)
#include "dsp/WarpStages.hpp"        // the stage constants + the clamp/dual helpers
#include "dsp/WarpF.hpp"             // lowerWarp (the ONE lowering)

extern Model* modelPhaseDistortion;

using namespace dotModular;
namespace ws = dotModular::warpStages;   // the gap/dual display helpers (WarpStages.hpp)

// ── the module's process loop — the §13 signal path, eight channels ────────────
// ONE control-block snapshot per kCtrlEvery frames (PhaseDistBlockReader), then the
// per-sample hot path: read the input, run the channel core (the fixed one-sample
// hop + the identity raw-path bypass + the ONE shared primitive), write the output.
// No lock is ever taken per sample (the reader's copies are private).
void PhaseDistortion::process(const ProcessArgs&) {
    if (ctrlCount_ == 0) reader_.beginBlock(store);
    if (++ctrlCount_ >= kCtrlEvery) ctrlCount_ = 0;

    for (int k = 0; k < PhaseDistortionIds::kChannels; ++k) {
        const float in = inputs[k].getVoltage();
        outputs[k].setVoltage(ch_[k].process(in, reader_.snap_[k]));
    }
}

namespace {

// The channel-focus button (which set the permanent editor shows) — the expander's
// DexpFocusButton shape verbatim, aimed at B's own selection. UI-only state,
// deliberately NOT a store/param: a VIEW choice, not musical state.
struct PdFocusButton : rack::widget::Widget {
    int ch = 0;
    int* selected = nullptr;
    std::function<bool()> isLight;
    bool hovered = false;

    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            if (selected) *selected = ch;
            e.consume(this);
        }
    }
    void onHover(const rack::event::Hover& e) override {
        hovered = true;
        e.consume(this);
    }
    void onLeave(const rack::event::Leave& e) override {
        hovered = false;
        Widget::onLeave(e);
    }
    void draw(const DrawArgs& args) override {
        const bool lit = isLight && isLight();
        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float r  = std::min(box.size.x, box.size.y) * 0.42f;
        const bool sel = selected && (*selected == ch);
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgFillColor(vg, sel ? nvgRGB(0xd4, 0x00, 0x1a)
                             : (lit ? nvgRGB(0x9a, 0x9a, 0x9a) : nvgRGB(0x5a, 0x64, 0x70)));
        nvgFill(vg);
        if (sel) {
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r * 0.45f);
            nvgFillColor(vg, lit ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0xff, 0xf2, 0xe0));
            nvgFill(vg);
        } else if (hovered) {
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r);
            nvgStrokeColor(vg, lit ? nvgRGB(0x60, 0x60, 0x60) : nvgRGB(0x9a, 0xa4, 0xb0));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }
        Widget::draw(args);
    }
};

// The bare palette's names, minus Drawn (the phase store lowers drawn → identity —
// the entry would lie; see the file header). Index = WarpF variant index for the ✓.
static const char* const kKindNames[] = {
    "Identity (uniform)", "Triangular", "Power", "Kumaraswamy", "Logit-normal",
    "Logit-sigmoid", "Mobius", "Truncated normal", "Reversed power",
};

struct PhaseDistortionWidget : ModuleWidget,
    dotModular::Compose<PhaseDistortionWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {

    static constexpr int KCH = PhaseDistortionIds::kChannels;
    static constexpr int NPAR = PhaseDistortionIds::N_PARAMS;

    // ── panel geometry (mm) — MUST match gen_sands_phase_distortion.py (the
    // one-geometry-two-files discipline; the expander's note verbatim) ──────────
    static constexpr float ROW_TOP  = 24.0f;
    static constexpr float ROW_PITCH = 12.0f;
    static constexpr float FOCUS_X  = 8.0f;
    static constexpr float IN_X     = 14.5f;
    static constexpr float OUT_X    = 103.5f;
    static constexpr float KNOB_X0  = 24.0f;
    static constexpr float KNOB_DX  = 10.0f;
    static constexpr float EDITOR_X0 = 112.0f, EDITOR_Y0 = 20.0f;
    static constexpr float EDITOR_W  = 56.7f,  EDITOR_H  = 90.0f;

    // panel + theme (the lane-widget idiom; a standalone module follows the
    // Monsoon network's theme when a Monsoon is discoverable — Lantern's
    // convention — else dark)
    std::shared_ptr<window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;
    bool themeLight_ = false;

    // the permanent editor + its view holders (the expander's refresh shape)
    WarpCurveEditor* curve = nullptr;
    std::unique_ptr<Distribution> memberView_, view_;
    WarpStagesView* stagesView_ = nullptr;
    std::unique_ptr<WarpStagesView> kView_;   // the ALWAYS-K density view (CC_FIX_PHASE_PLOT_NEGATIVE_WARP)
    WarpF lastF_{};
    double lastTilt_ = -1e9, lastLevel_ = -1e9, lastMass_ = -1e9;
    double lastGapLoc_ = -1e9, lastGapSize_ = -1e9;
    float lastW_ = -1e9f;
    bool lastDel_ = false;
    int  lastCh_ = -1;

    // channel focus (UI-only) + the knob registry for the per-kind windows
    int editorCh_ = 0;
    redDot::StoreKnob<PhaseDistortion>* knobs_[KCH][NPAR] = {};
    int lastKind_[KCH] = {-1, -1, -1, -1, -1, -1, -1, -1};

    PhaseDistortion* mod() const {
        return dynamic_cast<PhaseDistortion*>(module);
    }

    // ── the kind menu (the file header records why B owns the loop) ────────────
    void openPdKindMenu() {
        PhaseDistortion* m = mod();
        if (!m) return;
        Menu* menu = createMenu();
        menu->addChild(createMenuLabel(std::string("Warp curve — set ")
                                       + std::to_string(editorCh_ + 1)));
        menu->addChild(createMenuLabel("click a type to author it (parametric only)"));
        const WarpF curF = m->store.sets_[editorCh_].f;
        const int curKind = (int)curF.index();
        struct KindItem : MenuItem {
            PhaseDistortion* m = nullptr;
            int ch = 0;
            int kind = 0;
            PhaseDistortionWidget* w = nullptr;
            void onAction(const rack::event::Action&) override {
                if (!m) return;
                WarpF nf;
                if (kind == 0) nf = IdentityWarp{};
                else {
                    std::unique_ptr<Distribution> d = WarpCurvePopup::makeKindDefault(kind);
                    if (!d) return;
                    nf = lowerWarp(*d);   // the ONE lowering (rack-free — no bank)
                }
                redDot::PhaseDistSet s = m->store.sets_[ch];   // read-modify-write ONE set
                s.f = nf;
                m->store.writeSet(ch, s);
                if (w) w->refreshEditor(ch);
            }
        };
        for (int k = 0; k < 9; ++k) {
            auto* it = new KindItem;
            it->text = kKindNames[k];
            it->rightText = (k == curKind) ? "✓" : "";
            it->m = m; it->ch = editorCh_; it->kind = k; it->w = this;
            menu->addChild(it);
        }
        // THE DELEGATED TOGGLE — the model's fifth element: carried with the set
        // (persisted by the store's codec) and ENGINE-INERT (the transform reads
        // only the numbers); the §14 store description's own words.
        menu->addChild(createMenuLabel("negative WARP = the dual reading (CDF/quantile exchanged)"));
        struct DelegItem : MenuItem {
            PhaseDistortion* m = nullptr;
            int ch = 0;
            void onAction(const rack::event::Action&) override {
                if (!m) return;
                redDot::PhaseDistSet s = m->store.sets_[ch];
                s.delegated = !s.delegated;
                m->store.writeSet(ch, s);
            }
        };
        auto* di = new DelegItem;
        di->text = "Delegated (carried; engine-inert)";
        di->rightText = m->store.sets_[editorCh_].delegated ? "✓" : "";
        di->m = m; di->ch = editorCh_;
        menu->addChild(di);
    }

    // ── the permanent editor's refresh (the expander's refreshEditor shape,
    // sourced from B's OWN store — no Monsoon, no role walk, no send bus) ──────
    void refreshEditor(int ch) {
        PhaseDistortion* m = mod();
        if (!m || !curve) return;
        const redDot::PhaseDistSet& s = m->store.sets_[ch];   // UI thread: its own writes
        // the member view — parametric-only (drawn cannot enter the store; identity
        // displays the diagonal through the default PchipDistribution)
        memberView_ = WarpCurvePopup::distFromWarp(s.f);
        curve->editCurve = nullptr;   // DISPLAY-ONLY: the phase tier has no drawn pool
        lastF_ = s.f; lastTilt_ = s.tilt; lastLevel_ = s.atomLevel; lastMass_ = s.atomMass;
        lastGapLoc_ = s.gapLoc; lastGapSize_ = s.gapSize;
        lastW_ = s.w; lastDel_ = s.delegated; lastCh_ = ch;
        view_ = std::make_unique<WarpStagesView>(memberView_.get(), s.tilt, s.atomLevel, s.atomMass);
        stagesView_ = static_cast<WarpStagesView*>(view_.get());
        stagesView_->gapLoc = s.gapLoc; stagesView_->gapSize = s.gapSize;
        // THE READING FOLLOWS THE SIGN (the bidirectional warp): the editor composes
        // what the CHANNEL passes — negative w the DUAL (the composed pair exchanged).
        stagesView_->dual = (s.w < 0.f);
        kView_ = std::make_unique<WarpStagesView>(memberView_.get(), s.tilt, s.atomLevel, s.atomMass);
        kView_->gapLoc = s.gapLoc; kView_->gapSize = s.gapSize; kView_->dual = false;
        curve->dist = view_.get();
        curve->isLight = [this]() { return themeLight_; };
        // the atom bar / gap band read the READING's markers (the atom<->gap
        // exchange on the dual side — the expander's block verbatim)
        double barLevel = s.atomLevel, barMass = s.atomMass, bandLo = s.gapLoc, bandHi = s.gapLoc + s.gapSize;
        if (stagesView_->dual) {
            const double a = ws::clampGapLoc(s.gapLoc), b = ws::clampGapLoc(s.gapLoc + s.gapSize);
            ws::dualMarkers(a, b, s.atomLevel, s.atomMass,
                [&](double x) { return memberView_->cdf(ws::tiltMapInv(x, ws::clampTilt(s.tilt))); },
                [&](double x) { return ws::gapCdf(x, a, b,
                    [&](double xx) { return memberView_->cdf(ws::tiltMapInv(xx, ws::clampTilt(s.tilt))); }); },
                barLevel, barMass, bandLo, bandHi);
        }
        curve->atomLevel = barLevel;
        curve->atomMass  = barMass;
        curve->gapLo = ws::clampGapLoc(bandLo);
        curve->gapHi = ws::clampGapLoc(bandHi);
        // THE EFFECTIVE PLOT (PHASE_DISTRIBUTION_SPEC §10, display-only): the drawn
        // curve is the warp mix at the signed depth, T the READING's target and the
        // fill factor the READING's, composed off the always-K view — continuously
        // through zero (w = 0 draws the diagonal, either side).
        curve->warpDepth = lastW_;
        const bool dualReading = (lastW_ < 0.f);
        curve->warpTDual = dualReading;
        curve->warpT     = [this](double u) { return view_ ? view_->invCDF(u) : u; };
        curve->warpTpdf  = [this, dualReading](double u) {
            if (!kView_) return 1.0;
            // K reading: T = Q → the factor is f(Q(u)); dual: T = g → the factor is
            // g'(u) — the chain pdf AT u (the CC_FIX_PHASE_PLOT_NEGATIVE_WARP law).
            return dualReading ? kView_->pdf(u) : kView_->pdf(kView_->invCDF(u));
        };
        // THE CHANGE-GATE'S HOST PIN (CC_FIX_CURVE_WIDGET_CPU): this refresh re-binds
        // the plot's callables/markers — bump the host generation so the widget's
        // fingerprint differs THIS frame even if the new bindings probe-identically
        // (a selection change to a value-identical set still rebuilds exactly once).
        curve->hostGeneration++;
    }

    PhaseDistortionWidget(PhaseDistortion* mod) {
        setModule(mod);
        // Browser preview creates widgets with mod=nullptr; guard against that.
        if (!mod) {
            loadPanel(asset::plugin(pluginInstance, "res/panels/PhaseDistortion_dark.svg"));
            return;
        }

        // Panel SVG paths (both themes, the lane-widget idiom).
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/PhaseDistortion_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/PhaseDistortion_light.svg"));
        loadPanel(asset::plugin(pluginInstance, "res/panels/PhaseDistortion_dark.svg"));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto resolveSelf = [this]() -> PhaseDistortion* { return dynamic_cast<PhaseDistortion*>(module); };

        // ── the 64 store-backed distribution controls (8 channels x 8 params) ──
        // SETTER LIFETIME LAW (the expander's): setters capture ONLY the module
        // pointer + by-value indices — Rack history may replay them after the
        // widget is gone. The store read-modify-writes ONE set through writeSet
        // (atomic per set — the store mutex).
        for (int ch = 0; ch < KCH; ++ch) {
            for (int p = 0; p < NPAR; ++p) {
                float lo = 0.f, hi = 1.f, def = 0.f;
                if (p == 2) { lo = -(float)warpStages::kTiltMax; hi = (float)warpStages::kTiltMax; def = 0.f; }
                if (p == 3) { lo = 0.f; hi = 1.f; def = (float)warpStages::kAtomLevelDefault; }
                if (p == 4) { lo = 0.f; hi = (float)warpStages::kAtomMassMax; def = 0.f; }
                // 5 WARP — the BIDIRECTIONAL DEPTH (the amended law): a SIGNED axis
                // [−1,+1], 0 = no warp, the sign IS the reading, the depth is |w|.
                if (p == 5) { lo = -1.f; hi = 1.f; def = 0.f; }
                if (p == 6) { lo = 0.f; hi = 1.f; def = (float)warpStages::kGapLocDefault; }
                if (p == 7) { lo = 0.f; hi = 1.f; def = (float)warpStages::kGapSizeDefault; }
                auto* k = redDot::bindStoreKnob<PhaseDistortion, redDot::Tag_Grey_Trim_Bar>(
                    // the anchor name is a LITERAL (the audit's bind matcher reads the
                    // widget source's string tokens — the expander's own pattern)
                    this, "param_pd_" + std::to_string(ch) + "_" + std::to_string(p),
                    resolveSelf, lo, hi, def,
                    "Set " + std::to_string(ch + 1) + " " + PhaseDistortionIds::PARAM_CAPTIONS[p],
                    [ch, p](PhaseDistortion& m) -> float {
                        const redDot::PhaseDistSet& s = m.store.sets_[ch];
                        switch (p) {
                            case 0: case 1: return (float)warpParamValue(s.f, p);
                            case 2: return s.tilt;
                            case 3: return s.atomLevel;
                            case 4: return s.atomMass;
                            case 5: return s.w;
                            case 6: return s.gapLoc;
                            default: return s.gapSize;
                        }
                    },
                    [ch, p](PhaseDistortion& m, float v) {
                        redDot::PhaseDistSet s = m.store.sets_[ch];
                        switch (p) {
                            case 0: case 1: s.f = warpWithParam(s.f, p, (double)v); break;
                            case 2: s.tilt = v; break;
                            case 3: s.atomLevel = v; break;
                            case 4: s.atomMass = v; break;
                            case 5: s.w = v; break;
                            case 6: s.gapLoc = v; break;
                            default: s.gapSize = v; break;
                        }
                        m.store.writeSet(ch, s);
                    });
                knobs_[ch][p] = k;
                // P1/P2: the kind window + the absent-param lock refresh in step()
                // (the unused-param law: absent → a locked, harmless control).
                if (p <= 1) {
                    k->lockWhen = [mod, ch, p]() -> bool {
                        if (!mod) return true;
                        const WarpF& f = mod->store.sets_[ch].f;
                        return !warpParamSpec(warpEffKind(f), p).present;
                    };
                }
            }
        }

        // ── the 16 phase jacks (8 in / 8 out — the channel IS the jack) ─────────
        for (int ch = 0; ch < KCH; ++ch) {
            bindInput<PJ301MPort>("input_pdin_" + std::to_string(ch), ch);
            bindOutput<PJ301MPort>("output_pdout_" + std::to_string(ch), ch);
        }

        // ── the 8 channel-focus buttons (which set the editor shows) ───────────
        for (int ch = 0; ch < KCH; ++ch) {
            bindWidget<PdFocusButton>(
                "param_pdfocus_" + std::to_string(ch),
                std::function<void(PdFocusButton*)>([this, ch](PdFocusButton* b) {
                    b->ch = ch;
                    b->selected = &editorCh_;
                    b->isLight = [this]() { return themeLight_; };
                    b->box.size = mm2px(Vec(5.4f, 5.4f));
                }));
        }

        // ── the permanent DistributionCurveWidget (the selected channel's plot —
        // the §13 prompt's "one per channel or for the selected channel") ────────
        curve = new WarpCurveEditor();
        curve->box.pos = mm2px(Vec(EDITOR_X0, EDITOR_Y0));
        curve->box.size = mm2px(Vec(EDITOR_W, EDITOR_H));
        addChild(curve);
        // Right-click on the editor = B's own kind menu (the bare palette minus
        // Drawn + the delegated toggle — see openPdKindMenu).
        curve->onKindMenu = [this]() { openPdKindMenu(); };
        refreshEditor(0);
    }

    // ── per-frame upkeep: theme swap, per-kind windows, the editor detector ────
    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        PhaseDistortion* m = mod();
        if (!m) return;
        // theme: follow the Monsoon network when discoverable (Lantern's convention)
        Monsoon* mon = redDot::lookupMonsoon(module);
        themeLight_ = mon && mon->lightTheme;
        int wantLight = themeLight_ ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }

        // per-kind windows for P1/P2 (+label) — only on a kind flip per channel
        for (int ch = 0; ch < KCH; ++ch) {
            const int kind = warpEffKind(m->store.sets_[ch].f);
            if (kind == lastKind_[ch]) continue;
            lastKind_[ch] = kind;
            for (int p = 0; p <= 1; ++p) {
                auto* k = knobs_[ch][p];
                if (!k) continue;
                const auto spec = warpParamSpec(kind, p);
                k->minValue = spec.lo; k->maxValue = spec.hi; k->defaultValue = spec.def;
                k->label = "Set " + std::to_string(ch + 1) + " warp "
                         + (spec.present ? spec.label : "—");
            }
        }

        // the editor: rebuild on ANY change to the SELECTED set (the step-detector)
        const redDot::PhaseDistSet& s = m->store.sets_[editorCh_];
        if (editorCh_ != lastCh_
                || std::memcmp(&s.f, &lastF_, sizeof(WarpF)) != 0
                || s.tilt != lastTilt_ || s.atomLevel != lastLevel_ || s.atomMass != lastMass_
                || s.gapLoc != lastGapLoc_ || s.gapSize != lastGapSize_
                || s.w != lastW_ || s.delegated != lastDel_) {
            refreshEditor(editorCh_);
        }
    }

    // ── lettering (nanosvg drops <text> — the Cantonment widget-lettering idiom) ─
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font) return;
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, font->handle);
        const bool lit = themeLight_;
        const NVGcolor ink = lit ? nvgRGB(0x2a, 0x2a, 0x2e) : nvgRGB(0xe8, 0xe2, 0xd0);
        const NVGcolor dim = lit ? nvgRGB(0x88, 0x8d, 0x96) : nvgRGB(0x8a, 0x85, 0x78);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        // title
        nvgFillColor(vg, ink);
        nvgFontSize(vg, mm2px(3.4f));
        nvgTextLetterSpacing(vg, mm2px(0.4f));
        nvgText(vg, box.size.x * 0.5f, mm2px(7.5f), "PHASE DISTORTION", NULL);
        nvgTextLetterSpacing(vg, 0.f);

        // column captions over the knob block — the VISUAL order (P1 P2 TILT LEVEL
        // MASS | GAPLOC GAPSIZE | WARP) is the generator's column order; the anchors
        // are (channel, param)-named, so the map lives HERE and in
        // gen_sands_phase_distortion.py (one geometry, two files, kept in sync).
        nvgFillColor(vg, dim);
        nvgFontSize(vg, mm2px(1.7f));
        for (int col = 0; col < NPAR; ++col) {
            const int p = PhaseDistortionIds::VISUAL_ORDER[col];
            const float knobXmm = KNOB_X0 + col * KNOB_DX;
            nvgText(vg, mm2px(knobXmm), mm2px(18.6f), PhaseDistortionIds::PARAM_CAPTIONS[p], NULL);
        }
        nvgText(vg, mm2px(IN_X),  mm2px(18.6f), "IN",  NULL);
        nvgText(vg, mm2px(OUT_X), mm2px(18.6f), "OUT", NULL);

        // channel numerals, left of each focus button
        nvgFontSize(vg, mm2px(2.0f));
        char num[8];
        for (int ch = 0; ch < KCH; ++ch) {
            const float rowYmm = ROW_TOP + ch * ROW_PITCH;
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            std::snprintf(num, sizeof num, "%d", ch + 1);
            nvgText(vg, mm2px(6.2f), mm2px(rowYmm), num, NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }

        // the editor caption: which set the plot shows + the hop/convention note
        if (module) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "PLOT: SET %d%s", editorCh_ + 1,
                          lastDel_ ? "  ·  DELEGATED" : "");
            nvgFillColor(vg, ink);
            nvgFontSize(vg, mm2px(2.2f));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, mm2px(EDITOR_X0), mm2px(113.2f), buf, NULL);
            nvgFillColor(vg, dim);
            nvgFontSize(vg, mm2px(1.6f));
            nvgText(vg, mm2px(EDITOR_X0), mm2px(116.6f),
                    "ONE-SAMPLE HOP — PATCH EVERY MONSOON'S", NULL);
            nvgText(vg, mm2px(EDITOR_X0), mm2px(119.2f),
                    "PHASE + SUBPHASE FROM THESE OUTPUTS", NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
    }
};

} // namespace

// ── model ────────────────────────────────────────────────────────────────────────
Model* modelPhaseDistortion =
    createModel<PhaseDistortion, PhaseDistortionWidget>("PhaseDistortion");
