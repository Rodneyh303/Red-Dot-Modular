// SandsMacroSendSubpanel — widget + model (the module struct lives in the
// header; see there for the spec mapping and the snapping/addressing design).
#include "SandsMacroSendSubpanel.hpp"

#include <cmath>

#include "ui/SvgPanelKit.hpp"        // Compose<ShapeQuery, Bind, Reload> + loadPanel/findNamed + kitStep
#include "ui/StoreBound.hpp"         // bindStoreKnob (the de-param law)
#include "ui/Controls.hpp"           // Tag_Grey_Trim_Bar (the family's knob face)
#include "ui/StraitsSeamFiller.hpp"  // the family seam (spec: StraitsSeamFiller-style continuity)
#include "ui/ConnectMark.hpp"        // the connect mark (its own next-hop)
#include "ui/MonsoonDiscovery.hpp"   // isSuiteChainModel (the hop set + the host walk's suite)
#include "dsp/LaneMapping.hpp"       // SandsLaneNames (the canonical lane-name table)
#include "dsp/SandsDistributionRole.hpp"  // the host walk (rack-free, unit-tested template)

using namespace rack;

// ── module impl ───────────────────────────────────────────────────────────────

bool SandsMacroSendSubpanel::isFamilyModel(const rack::Model* m) {
    // The Sands subpanel family (seam continuity + the left-link check): the two
    // host visuals + both subpanels. Seating is the HOSTS' decision (the Macro
    // visual seats both subpanels; East seats the Distribution expander only —
    // this panel is a MACRO subpanel).
    return m == modelStraitsSandsMacroVisual
        || m == modelStraitsEastSandsVisual
        || m == modelSandsDistributionExpander
        || m == modelSandsMacroSendSubpanel;
}

bool SandsMacroSendSubpanel::resolveMacroHost() {
    // Memoized by the topology epoch (ModuleLookupCore): the walk runs once per
    // adjacency event, never per-frame-walk (the L5 law). The EAST visual
    // terminates the walk (isSuite = the hop set MINUS East): it is a host this
    // panel cannot be hosted by — the segment rule's "never walk PAST a host".
    // A foreign boundary, a walked-out chain, or a bare left link resolves false
    // (the panel stays a working store UI wherever a Monsoon is reachable — the
    // badge is honesty, not a gate).
    return redDot::lookupMemoizedT(this, 0, [](SandsMacroSendSubpanel* self, int) -> bool {
        namespace R = SandsDistributionRole;   // a NAMESPACE alias — `using R = <ns>` is ill-formed
        const R::Host h = R::resolve(
            self->leftExpander.module,
            [](rack::Module* m) { return m ? m->leftExpander.module : nullptr; },
            [](rack::Module* m) -> R::Host {
                if (m && m->model == modelStraitsSandsMacroVisual) return R::Host::MACRO;
                return R::Host::NONE;
            },
            [](rack::Module* m) {
                return m && m->model != modelStraitsEastSandsVisual
                    && redDot::isSuiteChainModel(m->model);
            });
        return h == R::Host::MACRO;
    });
}

// ── widget ────────────────────────────────────────────────────────────────────

namespace {

using namespace dotModular;

struct SandsMacroSendSubpanelWidget : ModuleWidget,
    dotModular::Compose<SandsMacroSendSubpanelWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {

    static constexpr int LANES   = SandsMacroSendIds::LANES;
    static constexpr int NITEM   = SandsMacroSendIds::SEND_ITEMS;
    static constexpr int NTAP    = SandsMacroSendIds::TAP_KINDS;
    static constexpr int NPAR    = SandsMacroSendIds::DIST_PARAMS;

    // panel + theme (the lane-widget idiom)
    std::shared_ptr<window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;
    bool themeLight_ = false;

    // the seam (family continuity, spec §HARD REQUIREMENT)
    redDot::StraitsSeamFiller* leftFiller  = nullptr;
    redDot::StraitsSeamFiller* rightFiller = nullptr;
    redDot::ConnectMark* connectMark = nullptr;

    SandsMacroSendSubpanel* mod() const {
        return dynamic_cast<SandsMacroSendSubpanel*>(module);
    }

    // The shared voice selection — READ, never owned (the spec's subpanel law).
    // PatternEngine::displayedVoice is THE shared state; the knobs author the
    // selected voice's row, resolved LIVE per read (no sync dance — the Macro
    // strip's own convention, moved here; the view re-binds on a voice switch).
    static int slotLiveFrom(Monsoon* m) { return m ? m->engine.pe.displayedVoice().v : 0; }
    int slotLive() const { return slotLiveFrom(redDot::lookupMonsoon(module)); }

    // column centres (mm) — MUST match panel_src/gen_sands_macro_send.py
    static constexpr float SEND_X0 = 20.f, TAP_X0 = 58.f, DIST_X0 = 78.f, COL_DX = 9.f;
    static inline float sendX(int item)  { return SEND_X0 + item * COL_DX; }
    static inline float tapX(int k)      { return TAP_X0  + k    * COL_DX; }
    static inline float distX(int p)     { return DIST_X0 + p    * COL_DX; }
    static constexpr float ROW_TOP = 27.0f, ROW_PITCH = 12.6f;
    static inline float rowY(int l)      { return ROW_TOP + l * ROW_PITCH; }

    SandsMacroSendSubpanelWidget(SandsMacroSendSubpanel* mod) {
        setModule(mod);
        // Browser preview creates widgets with mod=nullptr; guard against that.
        if (!mod) {
            loadPanel(asset::plugin(pluginInstance, "res/panels/SandsMacroSend_dark.svg"));
            return;
        }

        // Panel SVG paths (both themes, the lane-widget idiom).
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/SandsMacroSend_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/SandsMacroSend_light.svg"));
        loadPanel(asset::plugin(pluginInstance, "res/panels/SandsMacroSend_dark.svg"));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;

        // ── the 84 store-backed send controls ────────────────────────────────
        // SETTER LIFETIME LAW: setters capture ONLY by-value indices (no widget,
        // no module pointer — the store module arrives as the argument); the
        // shared slot resolves at CALL time (the spec's "read the selected voice
        // from the shared state").
        //
        // 1) the EXISTING mix-in sends (moved off the Macro visual's right strip,
        //    verbatim conventions: bipolar −1..1, default 0 = opt-in) — anchor
        //    param_msend_<l>_<item> (inline prefix literals — the audit reads the
        //    bind's FIRST string literal as the family prefix; the *_ANCHOR
        //    constants in the header are the documentation of that), store
        //    editor.macroSend.
        for (int l = 0; l < LANES; ++l)
            for (int item = 0; item < NITEM; ++item)
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_msend_" + std::to_string(l) + "_" + std::to_string(item),
                    resolveMon, -1.f, 1.f, 0.f,
                    std::string(laneNames[l]) + " mix-in "
                        + SandsMacroSendIds::SEND_CAPTIONS[item] + " (viewed voice)",
                    [l, item](Monsoon& m) { return m.getMacroSend(slotLiveFrom(&m), l, item); },
                    [l, item](Monsoon& m, float v) { m.setMacroSend(slotLiveFrom(&m), l, item, v); });

        // 2) the PRE/POST taps (verbatim conventions: 0..1, default 1.0 = POST) —
        //    anchor param_mtap_<l>_<k>, store editor.macroTap.
        for (int l = 0; l < LANES; ++l)
            for (int k = 0; k < NTAP; ++k)
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_mtap_" + std::to_string(l) + "_" + std::to_string(k),
                    resolveMon, 0.f, 1.f, 1.f,
                    std::string(laneNames[l]) + " "
                        + SandsMacroSendIds::TAP_CAPTIONS[k] + " send tap (PRE-POST, viewed voice)",
                    [l, k](Monsoon& m) { return m.getMacroTap(slotLiveFrom(&m), l, k); },
                    [l, k](Monsoon& m, float v) { m.setMacroTap(slotLiveFrom(&m), l, k, v); });

        // 3) the NEW distribution-param send amounts (0..1, default 1.0 = full
        //    send — the fold's identity default; 0 = the voice receives none of
        //    the Macro-sent global component) — anchor param_mdist_<l>_<p>,
        //    store editor.warp.distSend. The setter pairs the write with the
        //    matching pending rebuild scope (the fold lands in the baked mirror).
        for (int l = 0; l < LANES; ++l)
            for (int p = 0; p < NPAR; ++p)
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_mdist_" + std::to_string(l) + "_" + std::to_string(p),
                    resolveMon, 0.f, 1.f, dotModular::WarpBank::kDistSendDefault,
                    std::string(laneNames[l]) + " dist-send "
                        + SandsMacroSendIds::DIST_CAPTIONS[p] + " (viewed voice)",
                    [l, p](Monsoon& m) { return m.getDistSend(slotLiveFrom(&m), l, p); },
                    [l, p](Monsoon& m, float v) {
                        const int slot = slotLiveFrom(&m);
                        m.setDistSend(slot, l, p, v);
                        m.requestWarpRebuildVoiceLane(slot, l);
                    });

        // ── the seam fillers (family continuity — the lane-widget wiring) ─────
        // RULE-FREE + host bg: the Sands family's seam design is the HOST's
        // (dotmod_design: #16181c/#e8e8ea, no red top rule) — see the generator
        // note. The filler bridges the sub-pixel seam with bg only; a red rule
        // here would draw onto the host panel where it carries no rule.
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

        // ── the connect mark (its own next-hop: the left neighbour is the host
        // visual or another family subpanel) ──────────────────────────────────
        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            connectMark->connected = [this]() {
                if (!module) return false;
                auto* left = module->leftExpander.module;
                return left && SandsMacroSendSubpanel::isFamilyModel(left->model);
            };
            addChild(connectMark);
        }
    }

    // ── per-frame upkeep: host badge, theme swap, seam toggles ────────────────
    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* mm = redDot::lookupMonsoon(module);
        themeLight_ = (mm && mm->lightTheme);

        // the host badge (memoized walk — one compare in steady state)
        if (auto* s = mod()) s->macroHost = s->resolveMacroHost();

        // theme swap (the lane-widget idiom)
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

        // seam fillers: family neighbours (the lane's isStraitsRPanel predicate,
        // family swapped) — bg is the HOST's (dotmod_design), not the Straits lanes'
        NVGcolor bg = themeLight_ ? nvgRGB(0xe8, 0xe8, 0xea) : nvgRGB(0x16, 0x18, 0x1c);
        auto familyModel = [](rack::Module* m) -> bool {
            return m && SandsMacroSendSubpanel::isFamilyModel(m->model);
        };
        if (leftFiller)  { leftFiller->active  = familyModel(module->leftExpander.module);  leftFiller->bgColour  = bg; }
        if (rightFiller) { rightFiller->active = familyModel(module->rightExpander.module); rightFiller->bgColour = bg; }
    }

    // ── lettering (nanosvg drops <text> — the family's widget-lettering idiom)
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
        nvgFontSize(vg, mm2px(3.6f));
        nvgTextLetterSpacing(vg, mm2px(0.4f));
        nvgText(vg, box.size.x * 0.5f, mm2px(8.f), "SENDS", NULL);
        nvgTextLetterSpacing(vg, 0.f);

        // group labels + column captions over the three knob groups
        nvgFillColor(vg, dim);
        nvgFontSize(vg, mm2px(1.8f));
        nvgText(vg, mm2px((sendX(0) + sendX(NITEM - 1)) * 0.5f), mm2px(17.5f), "SEND", NULL);
        nvgText(vg, mm2px((tapX(0)  + tapX(NTAP - 1))  * 0.5f), mm2px(17.5f), "TAP", NULL);
        nvgText(vg, mm2px((distX(0) + distX(NPAR - 1)) * 0.5f), mm2px(17.5f), "DIST SEND", NULL);
        for (int i = 0; i < NITEM; ++i) nvgText(vg, mm2px(sendX(i)), mm2px(21.5f), SandsMacroSendIds::SEND_CAPTIONS[i], NULL);
        for (int k = 0; k < NTAP;  ++k) nvgText(vg, mm2px(tapX(k)),  mm2px(21.5f), SandsMacroSendIds::TAP_CAPTIONS[k],  NULL);
        for (int p = 0; p < NPAR;  ++p) nvgText(vg, mm2px(distX(p)), mm2px(21.5f), SandsMacroSendIds::DIST_CAPTIONS[p], NULL);

        // lane names, right-aligned in the gutter
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;
        nvgFontSize(vg, mm2px(2.0f));
        for (int l = 0; l < LANES; ++l) {
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgText(vg, mm2px(14.f), mm2px(rowY(l)), laneNames[l], NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }

        // the badge line: which voice the knobs author + the host claim
        if (module) {
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, ink);
            nvgFontSize(vg, mm2px(2.2f));
            char buf[48];
            std::snprintf(buf, sizeof(buf), "VIEWED VOICE: V%d", slotLive() + 1);
            nvgText(vg, mm2px(6.f), mm2px(115.5f), buf, NULL);
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, dim);
            nvgFontSize(vg, mm2px(1.8f));
            auto* s = mod();
            nvgText(vg, box.size.x - mm2px(6.f), mm2px(115.5f),
                    (s && s->macroHost) ? "MACRO HOST — ROUTES THE WHOLE SEND BUS"
                                        : "NO MACRO HOST — STORE UI (KNOBS STILL ROUTE)", NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
    }

    // ── the resnap hardening: the SUBPANEL's own drag-end flags (the host's
    // poll/consumer re-seats the run) ─────────────────────────────────────────
    void onDragEnd(const rack::event::DragEnd& e) override {
        ModuleWidget::onDragEnd(e);
        if (auto* g = dynamic_cast<redDot::SandsSnapGuest*>(module))
            g->needsResnap.store(true, std::memory_order_relaxed);
    }
};

} // namespace

// ── model ─────────────────────────────────────────────────────────────────────
Model* modelSandsMacroSendSubpanel =
    createModel<SandsMacroSendSubpanel, SandsMacroSendSubpanelWidget>("SandsMacroSendSubpanel");
