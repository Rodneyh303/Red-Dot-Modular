// SandsPhaseDistSubpanel.cpp — the phase-distortion sets panel's process glue +
// widget (docs/design/CC_PROMPT_PHASE_DISTORTION_PANEL.md TASK 1; the snapshot and
// the primitive are the LANDED engine — untouched, the prompt's DO-NOT; the store's
// OWNER moved onto this module — CC_FIX_PHASE_STORE_ON_SUBPANEL, the type itself
// untouched).
//
// THE WIDGET mirrors the standalone module's panel (build B) minus the jacks: the
// 8×8 store-backed grid, the focus buttons, ONE permanent DistributionCurveWidget
// with the §10 effective-plot refresh shape (the reading follows the SIGN of w, the
// markers exchange on the dual side, the always-K density view fills the mix), and
// the kind menu (the bare palette minus Drawn — the phase store has no drawn pool;
// the menu mints members through makeKindDefault + lowerWarp, the shared value
// authorities). The source here is THIS module's own PhaseDistStore (the owner IS
// the subpanel — no rack-wide scan anymore).
//
// THE ROW GRID: the distribution expander's param axis verbatim (P1 P2 TILT LEVEL
// MASS | GAPLOC GAPSIZE | WARP). Row k edits store set k ONLY (the gate: write on
// the panel, read back set k, other sets unchanged — the store's writeSet atomicity,
// pinned by test_phase_distortion). Modulation jacks are deliberately ABSENT: no
// phase-set parameter is modulated anywhere in the landed engine (the distribution
// rows' MOD jacks publish delta planes the phase model does not have) — inert jacks
// would be dead UI; flagged in the commit for Rodney's confirmation.

#include "SandsPhaseDistSubpanel.hpp"

#include <cmath>
#include <cstring>

#include "ui/SvgPanelKit.hpp"        // Compose<ShapeQuery, Bind, Reload> + loadPanel/findNamed
#include "ui/StoreBound.hpp"         // bindStoreKnob (the de-param law)
#include "ui/Controls.hpp"           // Tag_Grey_Trim_Bar (the knob face)
#include "ui/WarpCurvePopup.hpp"     // WarpCurveEditor + makeKindDefault + distFromWarp
#include "ui/DistributionCurveWidget.hpp"
#include "ui/ModuleLookup.hpp"       // lookupMonsoon (the theme follows the network)
#include "ui/StraitsSeamFiller.hpp"  // the family seam
#include "ui/ConnectMark.hpp"        // the connect mark (its own next-hop)
#include "dsp/WarpStages.hpp"        // the stage constants + the clamp/dual helpers
#include "dsp/WarpF.hpp"             // lowerWarp (the ONE lowering)

using namespace dotModular;
namespace ws = dotModular::warpStages;   // the gap/dual display helpers (WarpStages.hpp)

bool SandsPhaseDistSubpanel::isFamilyModel(const rack::Model* m) {
    // The Sands subpanel family (seam continuity + the left-link check): the two
    // host visuals + the distribution expander + both subpanels. Seating is the
    // HOSTS' decision (Macro's seating family seats this panel; East unchanged).
    return m == modelStraitsSandsMacroVisual
        || m == modelStraitsEastSandsVisual
        || m == modelSandsDistributionExpander
        || m == modelSandsMacroSendSubpanel
        || m == modelSandsPhaseDistSubpanel;
}

// ── widget ──────────────────────────────────────────────────────────────────────

namespace {

using namespace dotModular;

// The row-focus button (which set the permanent editor shows) — the expander's
// DexpFocusButton shape verbatim, aimed at this panel's own selection. UI-only
// state, deliberately NOT a store/param: a VIEW choice, not musical state.
struct PdsFocusButton : rack::widget::Widget {
    int set = 0;
    int* selected = nullptr;
    std::function<bool()> isLight;
    bool hovered = false;

    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            if (selected) *selected = set;
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
        const bool sel = selected && (*selected == set);
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
// the entry would lie). Index = WarpF variant index for the ✓.
static const char* const kKindNames[] = {
    "Identity (uniform)", "Triangular", "Power", "Kumaraswamy", "Logit-normal",
    "Logit-sigmoid", "Mobius", "Truncated normal", "Reversed power",
};

struct SandsPhaseDistSubpanelWidget : ModuleWidget,
    dotModular::Compose<SandsPhaseDistSubpanelWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {

    static constexpr int KSETS = SandsPhaseDistIds::SETS;
    static constexpr int NPAR  = SandsPhaseDistIds::N_PARAMS;

    // ── panel geometry (mm) — MUST match gen_sands_phase_dist.py (the
    // one-geometry-two-files discipline) ────────────────────────────────────────
    static constexpr float ROW_TOP  = 24.0f;
    static constexpr float ROW_PITCH = 12.0f;
    static constexpr float FOCUS_X  = 8.0f;
    static constexpr float KNOB_X0  = 18.0f;
    static constexpr float KNOB_DX  = 10.0f;
    static constexpr float EDITOR_X0 = 96.0f, EDITOR_Y0 = 20.0f;
    static constexpr float EDITOR_W  = 72.7f,  EDITOR_H  = 90.0f;

    // panel + theme (the lane-widget idiom; the theme follows the network Monsoon)
    std::shared_ptr<window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;
    bool themeLight_ = false;

    // the seam (family continuity)
    redDot::StraitsSeamFiller* leftFiller  = nullptr;
    redDot::StraitsSeamFiller* rightFiller = nullptr;
    redDot::ConnectMark* connectMark = nullptr;

    // the permanent editor + its view holders (the §10 refresh shape)
    WarpCurveEditor* curve = nullptr;
    std::unique_ptr<Distribution> memberView_, view_;
    WarpStagesView* stagesView_ = nullptr;
    std::unique_ptr<WarpStagesView> kView_;   // the ALWAYS-K density view (CC_FIX_PHASE_PLOT_NEGATIVE_WARP)
    WarpF lastF_{};
    double lastTilt_ = -1e9, lastLevel_ = -1e9, lastMass_ = -1e9;
    double lastGapLoc_ = -1e9, lastGapSize_ = -1e9;
    float lastW_ = -1e9f;
    int  lastSet_ = -1;

    // row focus (UI-only) + the knob registry for the per-kind windows
    int editorSet_ = 0;
    redDot::StoreKnob<SandsPhaseDistSubpanel>* knobs_[KSETS][NPAR] = {};
    int lastKind_[KSETS] = {-1, -1, -1, -1, -1, -1, -1, -1};

    SandsPhaseDistSubpanel* mod() const {
        return dynamic_cast<SandsPhaseDistSubpanel*>(module);
    }

    // ── the kind menu (the bare palette minus Drawn; the file header records why
    // this panel owns the loop — openWarpKindMenu is Monsoon-bank-bound) ────────
    void openPdsKindMenu() {
        SandsPhaseDistSubpanel* sp = mod();
        if (!sp) return;
        SandsPhaseDistSubpanel* owner = sp;   // THE OWNER IS THIS MODULE (CC_FIX_PHASE_STORE_ON_SUBPANEL)
        Menu* menu = createMenu();
        menu->addChild(createMenuLabel(std::string("Warp curve — set ")
                                       + std::to_string(editorSet_ + 1)));
        menu->addChild(createMenuLabel("click a type to author it (parametric only)"));
        const WarpF curF = owner->phaseDistStore.sets_[editorSet_].f;
        const int curKind = (int)curF.index();
        struct KindItem : MenuItem {
            SandsPhaseDistSubpanel* owner = nullptr;
            int set = 0;
            int kind = 0;
            SandsPhaseDistSubpanelWidget* w = nullptr;
            void onAction(const rack::event::Action&) override {
                if (!owner) return;
                WarpF nf;
                if (kind == 0) nf = IdentityWarp{};
                else {
                    std::unique_ptr<Distribution> d = WarpCurvePopup::makeKindDefault(kind);
                    if (!d) return;
                    nf = lowerWarp(*d);   // the ONE lowering (rack-free — no bank)
                }
                redDot::PhaseDistSet s = owner->phaseDistStore.sets_[set];   // ONE set
                s.f = nf;
                owner->phaseDistStore.writeSet(set, s);
                if (w) w->refreshEditor(set);
            }
        };
        for (int k = 0; k < 9; ++k) {
            auto* it = new KindItem;
            it->text = kKindNames[k];
            it->rightText = (k == curKind) ? "✓" : "";
            it->owner = owner; it->set = editorSet_; it->kind = k; it->w = this;
            menu->addChild(it);
        }
        menu->addChild(createMenuLabel("negative WARP = the dual reading (CDF/quantile exchanged)"));
        // DELEGATION OMITTED (the panel prompt's DECISION TO CONFIRM #1, applied as
        // proposed): a phase set has no Macro/East to delegate to; the store's
        // `delegated` field stays (engine-inert, persisted) with no row control.
    }

    // ── the permanent editor's refresh (the §10 effective-plot shape, sourced
    // from the RESOLVED owner's store) ──────────────────────────────────────────
    void refreshEditor(int set) {
        SandsPhaseDistSubpanel* sp = mod();
        if (!sp || !curve) return;
        SandsPhaseDistSubpanel* owner = sp;   // THE OWNER IS THIS MODULE (CC_FIX_PHASE_STORE_ON_SUBPANEL)
        const redDot::PhaseDistSet& s = owner->phaseDistStore.sets_[set];   // UI thread: its own writes
        memberView_ = WarpCurvePopup::distFromWarp(s.f);
        curve->editCurve = nullptr;   // DISPLAY-ONLY: the phase tier has no drawn pool
        lastF_ = s.f; lastTilt_ = s.tilt; lastLevel_ = s.atomLevel; lastMass_ = s.atomMass;
        lastGapLoc_ = s.gapLoc; lastGapSize_ = s.gapSize;
        lastW_ = s.w; lastSet_ = set;
        view_ = std::make_unique<WarpStagesView>(memberView_.get(), s.tilt, s.atomLevel, s.atomMass);
        stagesView_ = static_cast<WarpStagesView*>(view_.get());
        stagesView_->gapLoc = s.gapLoc; stagesView_->gapSize = s.gapSize;
        // THE READING FOLLOWS THE SIGN (the bidirectional warp): negative w the DUAL.
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
        // THE EFFECTIVE PLOT (PHASE_DISTRIBUTION_SPEC §10, display-only).
        curve->warpDepth = lastW_;
        const bool dualReading = (lastW_ < 0.f);
        curve->warpTDual = dualReading;
        curve->warpT     = [this](double u) { return view_ ? view_->invCDF(u) : u; };
        curve->warpTpdf  = [this, dualReading](double u) {
            if (!kView_) return 1.0;
            // K reading: T = Q → the factor is f(Q(u)); dual: T = g → g'(u) — the
            // chain pdf AT u (the CC_FIX_PHASE_PLOT_NEGATIVE_WARP law).
            return dualReading ? kView_->pdf(u) : kView_->pdf(kView_->invCDF(u));
        };
        // THE CHANGE-GATE'S HOST PIN (CC_FIX_CURVE_WIDGET_CPU): this refresh re-binds
        // the plot's callables/markers — bump the host generation so the widget's
        // fingerprint differs THIS frame even if the new bindings probe-identically
        // (a row-focus change to a value-identical set still rebuilds exactly once).
        curve->hostGeneration++;
    }

    SandsPhaseDistSubpanelWidget(SandsPhaseDistSubpanel* mod) {
        setModule(mod);
        // Browser preview creates widgets with mod=nullptr; guard against that.
        if (!mod) {
            loadPanel(asset::plugin(pluginInstance, "res/panels/SandsPhaseDist_dark.svg"));
            return;
        }

        // Panel SVG paths (both themes, the lane-widget idiom).
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/SandsPhaseDist_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/SandsPhaseDist_light.svg"));
        loadPanel(asset::plugin(pluginInstance, "res/panels/SandsPhaseDist_dark.svg"));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // the store owner is THIS module (CC_FIX_PHASE_STORE_ON_SUBPANEL — the
        // StoreKnob resolver law's shape kept: resolve at USE time, dynamic_cast to
        // the subpanel, null only in the browser preview)
        auto resolveOwner = [this]() -> SandsPhaseDistSubpanel* {
            return dynamic_cast<SandsPhaseDistSubpanel*>(module);
        };

        // ── the 64 store-backed phase-distortion controls (8 sets x 8 params) ──
        // SETTER LIFETIME LAW: setters capture ONLY by-value indices (no widget, no
        // module pointer — the store owner arrives as the argument); Row k edits
        // store set k through writeSet (atomic per set — the store mutex).
        for (int set = 0; set < KSETS; ++set) {
            for (int p = 0; p < NPAR; ++p) {
                float lo = 0.f, hi = 1.f, def = 0.f;
                if (p == 2) { lo = -(float)warpStages::kTiltMax; hi = (float)warpStages::kTiltMax; def = 0.f; }
                if (p == 3) { lo = 0.f; hi = 1.f; def = (float)warpStages::kAtomLevelDefault; }
                if (p == 4) { lo = 0.f; hi = (float)warpStages::kAtomMassMax; def = 0.f; }
                // 5 WARP — the BIDIRECTIONAL DEPTH: a SIGNED axis [−1,+1], 0 = no
                // warp, the sign IS the reading, the depth is |w|.
                if (p == 5) { lo = -1.f; hi = 1.f; def = 0.f; }
                if (p == 6) { lo = 0.f; hi = 1.f; def = (float)warpStages::kGapLocDefault; }
                if (p == 7) { lo = 0.f; hi = 1.f; def = (float)warpStages::kGapSizeDefault; }
                auto* k = redDot::bindStoreKnob<SandsPhaseDistSubpanel, redDot::Tag_Grey_Trim_Bar>(
                    // the anchor name is a LITERAL (the audit's bind matcher reads
                    // the widget source's string tokens)
                    this, "param_pds_" + std::to_string(set) + "_" + std::to_string(p),
                    resolveOwner, lo, hi, def,
                    "Set " + std::to_string(set + 1) + " " + SandsPhaseDistIds::PARAM_CAPTIONS[p],
                    [set, p](SandsPhaseDistSubpanel& o) -> float {
                        const redDot::PhaseDistSet& s = o.phaseDistStore.sets_[set];
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
                    [set, p](SandsPhaseDistSubpanel& o, float v) {
                        redDot::PhaseDistSet s = o.phaseDistStore.sets_[set];
                        switch (p) {
                            case 0: case 1: s.f = warpWithParam(s.f, p, (double)v); break;
                            case 2: s.tilt = v; break;
                            case 3: s.atomLevel = v; break;
                            case 4: s.atomMass = v; break;
                            case 5: s.w = v; break;
                            case 6: s.gapLoc = v; break;
                            default: s.gapSize = v; break;
                        }
                        o.phaseDistStore.writeSet(set, s);
                    });
                knobs_[set][p] = k;
                // P1/P2: the kind window + the absent-param lock refresh in step()
                if (p <= 1) {
                    k->lockWhen = [mod, set, p]() -> bool {
                        if (!mod) return true;   // THE OWNER IS THIS MODULE — mod's own store
                        const WarpF& f = mod->phaseDistStore.sets_[set].f;
                        return !warpParamSpec(warpEffKind(f), p).present;
                    };
                }
            }
        }

        // ── the 8 row-focus buttons (which set the editor shows) ───────────────
        for (int set = 0; set < KSETS; ++set) {
            bindWidget<PdsFocusButton>(
                "param_pdsfocus_" + std::to_string(set),
                std::function<void(PdsFocusButton*)>([this, set](PdsFocusButton* b) {
                    b->set = set;
                    b->selected = &editorSet_;
                    b->isLight = [this]() { return themeLight_; };
                    b->box.size = mm2px(Vec(5.4f, 5.4f));
                }));
        }

        // ── the permanent DistributionCurveWidget (the selected set's plot) ────
        curve = new WarpCurveEditor();
        curve->box.pos = mm2px(Vec(EDITOR_X0, EDITOR_Y0));
        curve->box.size = mm2px(Vec(EDITOR_W, EDITOR_H));
        addChild(curve);
        curve->onKindMenu = [this]() { openPdsKindMenu(); };
        refreshEditor(0);

        // ── the seam fillers (family continuity — bg is the HOSTS' dotmod_design) ──
        leftFiller = new redDot::StraitsSeamFiller();
        leftFiller->isLeft = true;
        leftFiller->box.pos = Vec(-2, 0);
        leftFiller->box.size.y = box.size.y;
        leftFiller->ruleHeightPx = 0.f;
        addChild(leftFiller);
        rightFiller = new redDot::StraitsSeamFiller();
        rightFiller->isLeft = false;
        rightFiller->box.pos = Vec(box.size.x - 2, 0);
        rightFiller->box.size.y = box.size.y;
        rightFiller->ruleHeightPx = 0.f;
        addChild(rightFiller);

        // ── the connect mark (its own next-hop: the left neighbour is a family panel) ──
        if (auto* s = findNamed("light_pd_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            connectMark->connected = [this]() {
                if (!module) return false;
                auto* left = module->leftExpander.module;
                return left && SandsPhaseDistSubpanel::isFamilyModel(left->model);
            };
            addChild(connectMark);
        }
    }

    // ── per-frame upkeep: theme swap, seam toggles, per-kind windows, editor ──
    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        auto* sp = mod();
        if (!sp) return;
        Monsoon* mon = redDot::lookupMonsoon(module);
        themeLight_ = mon && mon->lightTheme;

        int wantLight = themeLight_ ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp2 = dynamic_cast<app::SvgPanel*>(child)) {
                    sp2->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }

        // seam fillers: family neighbours (bg is the HOSTS' dotmod_design)
        NVGcolor bg = themeLight_ ? nvgRGB(0xe8, 0xe8, 0xea) : nvgRGB(0x16, 0x18, 0x1c);
        auto familyModel = [](rack::Module* m) -> bool {
            return m && SandsPhaseDistSubpanel::isFamilyModel(m->model);
        };
        if (leftFiller)  { leftFiller->active  = familyModel(module->leftExpander.module);  leftFiller->bgColour  = bg; }
        if (rightFiller) { rightFiller->active = familyModel(module->rightExpander.module); rightFiller->bgColour = bg; }

        // per-kind windows for P1/P2 (+label) — only on a kind flip per set
        // (the owner IS this module — the old resolveStoreOwner branch is gone)
        {
            SandsPhaseDistSubpanel* owner = sp;
            for (int set = 0; set < KSETS; ++set) {
                const int kind = warpEffKind(owner->phaseDistStore.sets_[set].f);
                if (kind == lastKind_[set]) continue;
                lastKind_[set] = kind;
                for (int p = 0; p <= 1; ++p) {
                    auto* k = knobs_[set][p];
                    if (!k) continue;
                    const auto spec = warpParamSpec(kind, p);
                    k->minValue = spec.lo; k->maxValue = spec.hi; k->defaultValue = spec.def;
                    k->label = "Set " + std::to_string(set + 1) + " warp "
                             + (spec.present ? spec.label : "—");
                }
            }

            // the editor: rebuild on ANY change to the SELECTED set (the step-detector)
            const redDot::PhaseDistSet& s = owner->phaseDistStore.sets_[editorSet_];
            if (editorSet_ != lastSet_
                    || std::memcmp(&s.f, &lastF_, sizeof(WarpF)) != 0
                    || s.tilt != lastTilt_ || s.atomLevel != lastLevel_ || s.atomMass != lastMass_
                    || s.gapLoc != lastGapLoc_ || s.gapSize != lastGapSize_
                    || s.w != lastW_) {
                refreshEditor(editorSet_);
            }
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
        // MASS | GAPLOC GAPSIZE | WARP) is the generator's column order
        nvgFillColor(vg, dim);
        nvgFontSize(vg, mm2px(1.7f));
        for (int col = 0; col < NPAR; ++col) {
            const int p = SandsPhaseDistIds::VISUAL_ORDER[col];
            const float knobXmm = KNOB_X0 + col * KNOB_DX;
            nvgText(vg, mm2px(knobXmm), mm2px(18.6f), SandsPhaseDistIds::PARAM_CAPTIONS[p], NULL);
        }

        // set numerals, left of each focus button
        nvgFontSize(vg, mm2px(2.0f));
        char num[8];
        for (int set = 0; set < KSETS; ++set) {
            const float rowYmm = ROW_TOP + set * ROW_PITCH;
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            std::snprintf(num, sizeof num, "%d", set + 1);
            nvgText(vg, mm2px(6.2f), mm2px(rowYmm), num, NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }

        // the footer: which set the plot shows + the ownership badge
        if (module) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "SET %d", editorSet_ + 1);
            nvgFillColor(vg, ink);
            nvgFontSize(vg, mm2px(2.2f));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, mm2px(EDITOR_X0), mm2px(114.5f), buf, NULL);
            nvgFillColor(vg, dim);
            nvgFontSize(vg, mm2px(1.8f));
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            // THE STORE IS THIS PANEL'S OWN (CC_FIX_PHASE_STORE_ON_SUBPANEL) — the
            // "NO STORE IN REACH" badge branch is gone: it can no longer happen.
            nvgText(vg, mm2px(box.size.x * (1.f / mm2px(1.f)) - 4.f), mm2px(114.5f),
                    "EDITS THE NETWORK'S PHASE-DISTORTION STORE", NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
    }
};

} // namespace

// ── model ────────────────────────────────────────────────────────────────────────
Model* modelSandsPhaseDistSubpanel =
    createModel<SandsPhaseDistSubpanel, SandsPhaseDistSubpanelWidget>("SandsPhaseDistSubpanel");
