// SandsFold.cpp — the seamless-fold RESIZE SPIKE's WIDGET HALF
// (CC_PROMPT_FOLD_RESIZE_SPIKE.md). The model (src/SandsFold.hpp) is Monsoon-free;
// this TU owns the Rack surface:
//
//   - the width: box.size.x from the core (fold::hpToPx — EVERY number comes from
//     ui/FoldPanelWidthCore.hpp; no sizing arithmetic here), constructed at the
//     saved width, changed from a context-menu choice;
//   - the background: drawn PROCEDURALLY in draw() at the current box.size.x —
//     no setPanel, no per-width asset (the doc's ruling);
//   - the right-anchored children (the screw pair) repositioned via
//     fold::rightAnchoredX on every width change;
//   - the neighbours: the modules to the right shift by fold::neighbourShiftPx
//     via APP->scene->rack->setModulePosForce (the SDK-verified API; the walk is
//     engine->getModuleIds() -> rack->getModule(id), the public surface);
//   - the undo: FoldWidthChangeAction (a rack::history::Action, the repo's
//     DiceUndoAction shape — the module ID, not a pointer) pushed through
//     APP->history->push;
//   - the SENDS BAND: the 98 store-backed knobs re-hosted with the SAME store
//     getters and setters as SandsMacroSendSubpanel — lookupMonsoon for the host
//     (the topology-epoch lookup, ui/ModuleLookup.hpp), placeStoreKnob (Placement
//     B — explicit mm centres, the de-param law) at the subpanel's own mm
//     stations. BOTH surfaces edit the ONE Monsoon store (the spike's collision
//     rule — fine, noted in the prompt).
//
// CANNOT VERIFY HERE (Rodney's Rack feel-test list, verbatim from the prompt):
// the neighbour push leaving no overlap; undo/redo; patch save/load keeping the
// width (and old patches loading byte-for-byte); the background at every allowed
// width (no seams/flicker); the knobs reading/writing the same store as the old
// subpanel.
#include <rack.hpp>
#include "SandsFold.hpp"
#include "Monsoon.hpp"
#include "SandsMacroSendSubpanel.hpp"   // SandsMacroSendIds (counts + captions + anchors)
#include "ui/StoreBound.hpp"            // placeStoreKnob (the de-param law, Placement B)
#include "ui/ModuleLookup.hpp"          // lookupMonsoon (the topology-epoch store resolution)
#include "ui/Controls.hpp"              // Tag_Grey_Trim_Bar (the family's knob face)

using namespace rack;

// the band's knob counts are the SUBPANEL'S law — never a second opinion
static_assert(SandsFold::LANES == SandsMacroSendIds::LANES);
static_assert(SandsFold::NITEM == SandsMacroSendIds::SEND_ITEMS);
static_assert(SandsFold::NTAP  == SandsMacroSendIds::TAP_KINDS);
static_assert(SandsFold::NPAR  == SandsMacroSendIds::DIST_PARAMS);

// the mm geometry — MUST match SandsMacroSendSubpanel.cpp's file-local constants
// (SEND_X0=20, TAP_X0=58, DIST_X0=78, COL_DX=9, ROW_TOP=27, ROW_PITCH=12.6);
// the model header carries the same values (a future DRY pass can share them).
static inline float sendX(int item) { return SandsFold::SEND_X0 + item * SandsFold::COL_DX; }
static inline float tapX(int k)     { return SandsFold::TAP_X0  + k    * SandsFold::COL_DX; }
static inline float distX(int p)    { return SandsFold::DIST_X0 + p    * SandsFold::COL_DX; }
static inline float rowY(int l)     { return SandsFold::ROW_TOP + l    * SandsFold::ROW_PITCH; }

// the send-voice slot law — the SUBPANEL'S, verbatim (the shared slot resolves at
// CALL time; the store module arrives as the argument)
static int slotLiveFrom(Monsoon* m) { return m ? m->engine.pe.sendVoice().v : 0; }

// the undo action — defined BEFORE the widget (the widget news it); its applyHp
// body is defined AFTER the widget (it dynamic_casts to it).
struct SandsFoldWidget;
struct FoldWidthChangeAction : history::Action {
    int64_t moduleId = -1;
    int oldHp = 0, newHp = 0;

    FoldWidthChangeAction() { name = "fold width"; }

    // Re-apply a width WITHOUT pushing a new history action (the body lives after
    // SandsFoldWidget — it finds the fold's widget by module id, the rack's public
    // getModule, and calls its applyWidth).
    void applyHp(int hp);
    void undo() override { applyHp(oldHp); }
    void redo() override { applyHp(newHp); }
};

// ── the widget ─────────────────────────────────────────────────────────────────
struct SandsFoldWidget : app::ModuleWidget {
    widget::Widget* screwTR = nullptr;   // the right-anchored pair — repositioned on
    widget::Widget* screwBR = nullptr;   // every width change (fold::rightAnchoredX)

    explicit SandsFoldWidget(SandsFold* m) {
        setModule(m);
        // Browser preview creates widgets with mod=nullptr: construct at the default
        // width (the subpanel's guard pattern).
        const int hp = m ? m->panelHp : SandsFold::defaultHp();
        box.size.x = redDot::fold::hpToPx(hp);
        box.size.y = RACK_GRID_HEIGHT;

        // the screws — the left pair fixed, the right pair right-anchored
        using componentlibrary::ScrewSilver;
        addChild(createWidget<ScrewSilver>(math::Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(math::Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        screwTR = createWidget<ScrewSilver>(math::Vec(0, 0));
        screwBR = createWidget<ScrewSilver>(math::Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH));
        addChild(screwTR);
        addChild(screwBR);
        repositionRightAnchored();

        // ── the SENDS BAND — the 98 store-backed knobs, the SAME store getters /
        //    setters / ranges / labels as SandsMacroSendSubpanel (Placement B: the
        //    subpanel's SVG anchors become explicit mm centres — the fold has no
        //    panel asset). placeStoreKnob centres the knob on the mm point. ──
        auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;

        // 1) the EXISTING mix-in sends (bipolar -1..1, default 0 = opt-in) — the
        //    subpanel's param_msend_<l>_<item> family, store editor.macroSend
        for (int l = 0; l < SandsFold::LANES; ++l)
            for (int item = 0; item < SandsFold::NITEM; ++item)
                redDot::placeStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    math::Vec(sendX(item), rowY(l)),
                    resolveMon, -1.f, 1.f, 0.f,
                    std::string(laneNames[l]) + " mix-in "
                        + SandsMacroSendIds::SEND_CAPTIONS[item] + " (send voice)",
                    [l, item](Monsoon& mm) { return mm.getMacroSend(slotLiveFrom(&mm), l, item); },
                    [l, item](Monsoon& mm, float v) { mm.setMacroSend(slotLiveFrom(&mm), l, item, v); });

        // 2) the PRE/POST taps (0..1, default 1.0 = POST) — param_mtap_<l>_<k>,
        //    store editor.macroTap
        for (int l = 0; l < SandsFold::LANES; ++l)
            for (int k = 0; k < SandsFold::NTAP; ++k)
                redDot::placeStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    math::Vec(tapX(k), rowY(l)),
                    resolveMon, 0.f, 1.f, 1.f,
                    std::string(laneNames[l]) + " "
                        + SandsMacroSendIds::TAP_CAPTIONS[k] + " send tap (PRE-POST, send voice)",
                    [l, k](Monsoon& mm) { return mm.getMacroTap(slotLiveFrom(&mm), l, k); },
                    [l, k](Monsoon& mm, float v) { mm.setMacroTap(slotLiveFrom(&mm), l, k, v); });

        // 3) the distribution-param send amounts (0..1, default full; the setter
        //    pairs the write with the pending rebuild scope — verbatim) —
        //    param_mdist_<l>_<p>, store editor.warp.distSend
        for (int l = 0; l < SandsFold::LANES; ++l)
            for (int p = 0; p < SandsFold::NPAR; ++p)
                redDot::placeStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    math::Vec(distX(p), rowY(l)),
                    resolveMon, 0.f, 1.f, dotModular::WarpBank::kDistSendDefault,
                    std::string(laneNames[l]) + " dist-send "
                        + SandsMacroSendIds::DIST_CAPTIONS[p] + " (send voice)",
                    [l, p](Monsoon& mm) { return mm.getDistSend(slotLiveFrom(&mm), l, p); },
                    [l, p](Monsoon& mm, float v) {
                        const int slot = slotLiveFrom(&mm);
                        mm.setDistSend(slot, l, p, v);
                        mm.requestWarpRebuildVoiceLane(slot, l);
                    });
    }

    // the right-anchored children — the core's law, called on every width change
    void repositionRightAnchored() {
        const float inset = 2.f * RACK_GRID_WIDTH;   // the subpanel's screw law
        const float x = redDot::fold::rightAnchoredX(box.size.x, inset);
        if (screwTR) screwTR->box.pos.x = x;
        if (screwBR) screwBR->box.pos.x = x;
    }

    // ── THE RESIZE (the spike's point) ──
    // pushUndo=false for the history action's own re-apply (no nested push).
    void applyWidth(int newHp, bool pushUndo) {
        SandsFold* m = dynamic_cast<SandsFold*>(module);
        if (!m) return;
        newHp = redDot::fold::clampHp(newHp, SandsFold::minContentHp());
        const int oldHp = m->panelHp;
        if (newHp == oldHp) return;

        // THE NEIGHBOUR PUSH: every module whose left edge sits at or beyond our
        // right edge, in our row, shifts by the width delta (setModulePosForce —
        // the SDK-verified RackWidget API). Our own position never moves: the
        // module grows rightward from its left edge. The walk is the public
        // surface only: the engine's module IDs -> the rack's widgets.
        const float oldRight = box.pos.x + box.size.x;
        const float shift = redDot::fold::neighbourShiftPx(oldHp, newHp);
        for (int64_t id : APP->engine->getModuleIds()) {
            app::ModuleWidget* nw = APP->scene->rack->getModule(id);
            if (!nw || nw == this) continue;
            if (nw->box.pos.x < oldRight - 1.f) continue;                       // left of us — untouched
            if (std::fabs(nw->box.pos.y - box.pos.y) >= RACK_GRID_HEIGHT) continue;   // not our row
            APP->scene->rack->setModulePosForce(nw, nw->box.pos + math::Vec(shift, 0.f));
        }

        // our own size + the right-anchored children
        m->setPanelHp(newHp);
        box.size.x = redDot::fold::hpToPx(m->panelHp);
        repositionRightAnchored();

        // THE UNDO STEP (one action per width change)
        if (pushUndo) {
            FoldWidthChangeAction* a = new FoldWidthChangeAction;
            a->moduleId = m->id;
            a->oldHp = oldHp;
            a->newHp = m->panelHp;
            APP->history->push(a);
        }
    }

    void appendContextMenu(ui::Menu* menu) override {
        SandsFold* m = dynamic_cast<SandsFold*>(module);
        if (!m) return;
        ModuleWidget::appendContextMenu(menu);
        menu->addChild(new ui::MenuSeparator);
        menu->addChild(createMenuLabel("Panel width"));
        const int minC = SandsFold::minContentHp();
        const int choices[5] = { minC, SandsFold::defaultHp(), 60, 90, redDot::fold::kMaxHp };
        for (int raw : choices) {
            const int hp = redDot::fold::clampHp(raw, minC);
            menu->addChild(createCheckMenuItem(std::to_string(hp) + " HP", "",
                [m, hp]() { return m->panelHp == hp; },
                [this, hp]() { applyWidth(hp, true); }));
        }
    }

    // ── the procedural background (no setPanel, no per-width asset) ──
    // The Sands family's dark panel (#16181c, dotmod_design) + the subpanel's own
    // text law (the SEND/TAP/DIST SEND captions and the per-column captions at the
    // same mm stations) + the lane labels. Everything scales with box.size.x.
    void draw(const DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        nvgBeginPath(vg);
        nvgRect(vg, 0.f, 0.f, box.size.x, box.size.y);
        nvgFillColor(vg, nvgRGB(0x16, 0x18, 0x1c));   // the family dark
        nvgFill(vg);
        // the header + footer rules (subtle, the family's no-red-rule design)
        nvgStrokeColor(vg, nvgRGB(0x2a, 0x2d, 0x33));
        nvgStrokeWidth(vg, 1.f);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0.f, mm2px(24.f)); nvgLineTo(vg, box.size.x, mm2px(24.f));
        nvgMoveTo(vg, 0.f, box.size.y - mm2px(8.f)); nvgLineTo(vg, box.size.x, box.size.y - mm2px(8.f));
        nvgStroke(vg);
        // the title
        nvgFillColor(vg, nvgRGB(0xe8, 0xe8, 0xea));
        nvgFontSize(vg, mm2px(2.4f));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, mm2px(4.f), mm2px(12.f), "SENDS · FOLD", NULL);
        // the column captions (the subpanel's text law, same mm stations)
        nvgFillColor(vg, nvgRGB(0x9a, 0x9a, 0xa2));
        nvgFontSize(vg, mm2px(1.8f));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, mm2px((sendX(0) + sendX(SandsFold::NITEM - 1)) * 0.5f), mm2px(17.5f), "SEND", NULL);
        nvgText(vg, mm2px((tapX(0)  + tapX(SandsFold::NTAP - 1))  * 0.5f), mm2px(17.5f), "TAP", NULL);
        nvgText(vg, mm2px((distX(0) + distX(SandsFold::NPAR - 1)) * 0.5f), mm2px(17.5f), "DIST SEND", NULL);
        for (int i = 0; i < SandsFold::NITEM; ++i)
            nvgText(vg, mm2px(sendX(i)), mm2px(21.5f), SandsMacroSendIds::SEND_CAPTIONS[i], NULL);
        for (int k = 0; k < SandsFold::NTAP; ++k)
            nvgText(vg, mm2px(tapX(k)), mm2px(21.5f), SandsMacroSendIds::TAP_CAPTIONS[k], NULL);
        for (int p = 0; p < SandsFold::NPAR; ++p)
            nvgText(vg, mm2px(distX(p)), mm2px(21.5f), SandsMacroSendIds::DIST_CAPTIONS[p], NULL);
        // the lane labels (left of the band, one per row)
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;
        for (int l = 0; l < SandsFold::LANES; ++l)
            nvgText(vg, mm2px(SandsFold::SEND_X0 - 4.f), mm2px(rowY(l)), laneNames[l], NULL);
        // the footer
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, mm2px(4.f), box.size.y - mm2px(4.f), "dot modular — fold spike", NULL);
        ModuleWidget::draw(args);
    }
};

// the undo action's body (SandsFoldWidget is complete now) — the repo's
// DiceUndoAction shape: the module ID, not a pointer.
void FoldWidthChangeAction::applyHp(int hp) {
    if (auto* mw = APP->scene->rack->getModule(moduleId))
        if (auto* fw = dynamic_cast<SandsFoldWidget*>(mw))
            fw->applyWidth(hp, false);
}

// ── the model (the slug in ONE constant — SandsFold.hpp's kSandsFoldSlug) ──────
Model* modelSandsFold = createModel<SandsFold, SandsFoldWidget>(kSandsFoldSlug);
