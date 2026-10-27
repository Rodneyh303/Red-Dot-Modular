#pragma once
// MonsoonCantonment — the articulation-envelope routing module (the "containment" sibling of
// Change Alley). Authority: CC_PROMPT_CANTONMENT_MODULE_SPEC.md + ARTICULATION_ENVELOPE_VS_
// CORRELATION.md, amended by Rodney's rulings 2026-10-26 (multi-parent acyclic DAG with
// intersection semantics; disconnected vertices valid; EXPAND = trajectory memory). Build plan:
// docs/design/CC_PROMPT_CANTONMENT_BUILD_PLAN.md.
//
// WHAT THIS MODULE IS: an EDITOR + DRIVER for the containment graph — per-voice gate-support
// nesting (gate(v) = clamp(own, ∩ resolved parents)), which is cycle-FATAL and therefore owns
// its own routing, structurally acyclic, CA-untouched. The graph state is ENGINE-SIDE
// authoritative (mirroring the CA seam: PatternEngine owns caRhythmSrc, the CA module edits and
// drives it); this module edits pins (cycle-rejecting) and drives the DAG-closed gesture verbs
// (INVERT / ROTATE ROLES / REFLECT ROLES / COLLAPSE / COLLAPSE-ALL / EXPAND) armed as pending
// and applied at the PHRASE BOUNDARY. The gate-clamp recursion that CONSUMES the graph is the
// decision manager's job (separate work — MONO_SPECIAL_IN_LOGIC_AUDIT.md); this module only
// PRODUCES the graph.
//
// PANEL (Rodney correction 2026-10-26): "a 16 by 16 panel like change alley (square near full
// module height with controls jacks to the side not underneath). Full Polyphony is 16 voices."
// So the matrix is CA V2's square at CA scale (99.6mm, 6.225mm cells, top at 16.0mm — 40HP
// module), ALWAYS 16x16: rows = voices, columns = parents; rows/cols >= activeCount are
// WIDGET-DIMMED (the pins there are HELD, not suspended — the transforms' above-pool rule),
// the geometry never shrinks. Trig jacks run down the LEFT flank, fire buttons + pending
// lamps down the RIGHT (one row per verb — the ROLES trio, the FOLD trio + RANDOM — plus the
// TRUE-REV row in its own group gap below, NOT a verb: it arms the true-reverse queue, the
// global-reverse consumer; the RANDOM row also carries its back jack, the CA scatter-back
// shape). Art is GENERATED:
// panel_src/gen_containment.py (house pipeline, dark+light); ALL lettering is widget-drawn
// here because nanosvg — the parser Rack uses — silently drops <text>.
//
// ZERO PARAM SLOTS BY DESIGN (DAW_PARAM_AUDIT.md, the ChangeAlleyV2 rule): transforms are
// triggered verbs (buttons/CV), config lives in context menus (the ROTATE step) — nothing here
// is automation-visible. PIN EDITS are store-backed + undoable (StoreEditAction, the house
// standard): left/right click TOGGLES a pin (multi-pin rows ARE the intersection — the CA
// row-radio is dropped for this plane); a pin that would close a cycle is REJECTED WITH AN
// AFFORDANCE (red cell flash + WOULD-CYCLE hover readout), never silently. One committed
// phrase-boundary batch = one undo step (the CA undoRing precedent; the audio thread publishes
// snapshots, the widget drains them into rack history). Serialization: the raw uint16 array
// (step 1), the rotate-step config, and the trajectory ring's bounded tail (the CA tail shape,
// version-guarded) — all in CantonmentModuleCore (rack-free, CI-tested).
//
// ENGINE-SIDE PUBLISH (step 4, the CA seam mirrored): PatternEngine owns
// containmentParents[16] (all-zeros default); MonsoonSandsManager::processDNA stages this
// module's editor graph into it each block (absent module => the R2 all-disconnected default).
// Kept honest: process() does NOT apply anything itself — the module is DRIVEN (the manager's
// fire block calls applyPendingTransforms at the phrase boundary / unlock flush; the R7
// grep-gate discipline: no raw rate primitives, no clock-divider/grid-pulse members, by
// design — see CantonmentPending.hpp's header).

#include <chrono>
#include <rack.hpp>
#include <jansson.h>
#include "Monsoon.hpp"                        // extern Plugin* pluginInstance
#include "ui/SvgPanelKit.hpp"                 // Option B-full: loadPanel/kitStep/findNamed
#include "ui/ModuleLookup.hpp"                // THE lookup authority: redDot::lookupMonsoon
#include "ui/RedScrew.hpp"                    // house screws
#include "ui/StoreEditAction.hpp"             // pin edits: store-backed, undoable (DAW_PARAM_AUDIT 5b)
#include "dsp/CantonmentGraph.hpp"           // the state (pure, rack-free)
#include "dsp/CantonmentTransforms.hpp"      // the DAG-closed verbs (pure, rack-free)
#include "dsp/CantonmentPending.hpp"         // pending rows + EXPAND trajectory (pure, rack-free)
#include "dsp/CantonmentModuleCore.hpp"      // step 4: the rack-free MODULE-STATE core

using namespace rack;
// Qualified ids on purpose: Monsoon.hpp exposes MonsoonIds/ChangeAlleyIds with the same
// NUM_PARAMS/... names (the MonsoonChangeAlleyV2.hpp rule).

// ── Panel geometry in mm — MUST MATCH panel_src/gen_containment.py ──────────────
// (the gen_change_alley_v2.py GRID_Y rule: one authority per number, cross-declared).
// Declared ABOVE the module: the ctor's config labels read VERB_LABEL.
namespace CantonmentPanelMm {
    constexpr float PW = 203.2f;                   // 40HP
    constexpr float GRID_X = 51.8f, GRID_Y = 16.0f, GRID_W = 99.6f;
    constexpr float CELL = GRID_W / 16.0f;         // 6.225
    constexpr float J_X = 18.75f, L_X = 27.75f;    // trig-jack col (left); lamp col (right)
    constexpr float BACK_X = 29.0f;                // RANDOM row's back jack (inboard of J_X — CA scback shape)
    // Verb rows at the 9.0mm pitch (ROW_H tightened 12 -> 10 -> 9 to fit the 9th flank row:
    // DECLUTTER joins the FOLD group — the reduction gesture pulled forward, Rodney 2026-10-27
    // — leaving the TRUE-REV row at TRUE_REV_Y with its own 3.5mm group gap below RANDOM;
    // 4.5mm between the verb groups). Mirrors gen_containment.py rowY() — keep the three in
    // lockstep.
    constexpr float ROW_Y[8] = {28.5f, 37.5f, 46.5f, 60.0f, 69.0f, 78.0f, 87.0f, 96.0f};
    constexpr float TRUE_REV_Y = 108.5f;
    constexpr const char* VERB_ID[8]    = {"rotate", "reflect", "invert", "collapse", "collapseall", "declutter", "expand", "random"};
    constexpr const char* VERB_LABEL[8] = {"ROTATE", "REFLECT", "INVERT", "COLLAPSE", "COLLAPSE-ALL", "DECLUTTER", "EXPAND", "RANDOM"};
    constexpr float LABEL_X = 47.4f, NUM_X = 50.4f;
    constexpr float TITLE_Y = 9.8f, SIDE_Y = 14.8f, CAP1_Y = 121.0f, CAP2_Y = 125.2f;
} // namespace CantonmentPanelMm

struct MonsoonCantonment : Module {
    // Single-sourced row count (the pending header owns it — the panel constants above mirror
    // its ROW ORDER; the three-file lockstep is gen_containment.py + this file + the header).
    // The TRUE-REV row rides the SAME flank affordance families (trig jack / fire button /
    // pending lamp) but is NOT a verb row — it arms the core's true-reverse queue (the
    // global-reverse consumer); TRIG_BACK_RANDOM is the RANDOM row's reverse-draw jack (the CA
    // scatter-back precedent, on the row like CA's back jacks). Zero param slots preserved: a
    // trigger input + one momentary button, both DAW-invisible gestures (DAW_PARAM_AUDIT rule).
    enum ParamIds  { BTN_START = 0, BTN_TRUE_REV = dotModular::ct::CantonmentPending::N_VERBS,
                     NUM_PARAMS  = BTN_TRUE_REV + 1 };   // fire buttons + the TRUE-REV arm button
    enum InputIds  { TRIG_START = 0, TRIG_TRUE_REV = dotModular::ct::CantonmentPending::N_VERBS,
                     TRIG_BACK_RANDOM = TRIG_TRUE_REV + 1,
                     NUM_INPUTS  = TRIG_BACK_RANDOM + 1 }; // trig jacks + TRUE-REV + RANDOM back
    enum LightIds  { PENDING_LIGHT_START = 0, LIGHT_TRUE_REV = dotModular::ct::CantonmentPending::N_VERBS,
                     NUM_LIGHTS  = LIGHT_TRUE_REV + 1 };  // pending lamps + the TRUE-REV lamp

    static constexpr int N_VERBS = dotModular::ct::CantonmentPending::N_VERBS;

    // The rack-free MODULE-STATE core (CantonmentModuleCore.hpp): graph + pending + trajectory
    // + owner-guard flag + undo publication + the save schema. CI-tested as a unit
    // (test_cantonment_module); this module adds only the SDK surface around it.
    dotModular::ct::CantonmentModuleCore core;

    // OWNER GUARD (the CA precedent, MonsoonChangeAlleyV2::transformsAppliedThisBlock): the
    // ExpanderManager's fire block reads AND writes this by name, so the CA idiom name is kept
    // as an alias — the STORAGE is the core's, so the reset/apply/set protocol is CI-tested.
    // Reset at the top of process() each block; the FIRST driver to apply sets it.
    bool& transformsAppliedThisBlock = core.transformsAppliedThisBlock;

    // The read-only graph diagram toggle (Rodney 2026-10-28: "drawn diagrams like this,
    // not editable just for visualisation"). A VIEW, not musical state: runtime-only,
    // deliberately OUTSIDE the core's save schema (the matrix returns on load; the five
    // persisted fields stay five). While it is up the PinOverlay consumes nothing — the
    // matrix stays the only editor (the read-only contract is enforced, not implied).
    bool graphView = false;

    // Arming edge detectors (the CA idiom: SchmittTrigger for the jacks, BooleanTrigger for
    // the buttons). Runtime only (not persisted) — like CA's trigger arrays. The TRUE-REV
    // jack/button arm the true-reverse QUEUE (not a verb row); the RANDOM back jack arms the
    // RANDOM row backward (the same row, the reverse direction — the CA scatter-back shape).
    rack::dsp::SchmittTrigger trigTrig[N_VERBS];
    rack::dsp::BooleanTrigger  btnTrig[N_VERBS];
    rack::dsp::SchmittTrigger  trueRevTrig;
    rack::dsp::BooleanTrigger  trueRevBtn;
    rack::dsp::SchmittTrigger  backTrig;

    MonsoonCantonment() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int k = 0; k < N_VERBS; ++k) {
            configInput(TRIG_START + k, std::string(CantonmentPanelMm::VERB_LABEL[k]) + " trigger");
            configButton(BTN_START + k, std::string(CantonmentPanelMm::VERB_LABEL[k]) + " fire");
        }
        configInput(TRIG_BACK_RANDOM, "RANDOM reverse trigger (the back-draw jack)");
        configInput(TRIG_TRUE_REV, "TRUE-REV trigger (retrace one committed phrase)");
        configButton(BTN_TRUE_REV, "TRUE-REV arm");
    }

    // The single arming choke point (the CA latchRow rule): the 8 trig jacks + 8 fire buttons
    // (and any future UI gesture) call THIS. Stamps on the unarmed->armed transition only;
    // re-arming an armed row keeps its stamp. Lights the pending lamp. NEVER touches the
    // graph — the verb waits for the host-driven boundary (the hold).
    void armVerb(int verb) {
        core.pending.latch(verb);
        if (verb >= 0 && verb < N_VERBS)
            lights[PENDING_LIGHT_START + verb].setBrightness(1.f);
    }

    // The reverse-draw arm (requirement 2 — the CA back-jack ACTION): arms the SAME RANDOM row
    // (one stamp, the lamp lights — the latchRow choke point) with the backward direction. Two
    // entry points share this choke point: the hardware back jack (TRIG_BACK_RANDOM, the CA
    // scatter-back shape — the row carries its back jack like CA's carry theirs) and the context
    // menu item. The STATE change happens at the phrase boundary and publishes its own undo
    // step, like any row commit.
    void armRandomReverse() {
        core.pending.latchRandomReverse();
        // static_cast: Verb is a DIFFERENT enum from LightIds, and C++20 deprecates
        // enum-enum arithmetic (-Wdeprecated-enum-enum-conversion, the MSYS2 build's
        // -std=c++20 -Wall flags it). armVerb(int) already crosses as int; this is the
        // same index spelled through the enumerator — the cast changes nothing numerically.
        lights[PENDING_LIGHT_START + static_cast<int>(dotModular::ct::CantonmentPending::V_RANDOM)].setBrightness(1.f);
    }

    // The TRUE-REVERSE arm (the global-reverse consumer; the CA jack/button gesture): arms the
    // committed-state trajectory step-back — a PERFORMANCE-class request, NOT a verb row. It is
    // consumed at the phrase boundary BEFORE the verb bracket (core.applyAtBoundary), so a pure
    // true-reverse publishes NO undo (the modulation-class rule) and the restored state is the
    // previous COMMITTED graph — verb-agnostic, RANDOM commits included (requirement 3 of the
    // 2026-10-27 ruling). The lamp is the queue affordance: lit while the request is pending,
    // cleared on consume (applyPendingTransforms syncs it from the queue flag) — idempotent
    // while lit, the CA rule.
    void armTrueReverse() {
        core.pending.latchTrueReverse();
        lights[LIGHT_TRUE_REV].setBrightness(1.f);
    }

    // The host-driven boundary commit (expanderManager.sync, the CA applyPendingTransforms
    // shape): the manager decides WHEN (phrase boundary / unlock flush) and passes the operating
    // voice count (numPolyVoices+1, the owner's count — the CA convention); the module owns the
    // mutation. axisMask: bit0 = the containment plane (default = commit; the out-of-axis
    // persistence machinery is core-tested — test_cantonment_pending — and narrows here when a
    // lock-scope bit lands). Returns rows applied (0 = nothing committed).
    // After the apply the pending lamps follow the pending set: applied rows cleared,
    // out-of-axis rows STAY lit (they are still armed — the persistence affordance, the CA rule).
    int applyPendingTransforms(int active, unsigned axisMask = dotModular::ct::CantonmentPending::AXIS_PLANE) {
        const int applied = core.applyAtBoundary(active, axisMask);
        for (int k = 0; k < N_VERBS; ++k)
            lights[PENDING_LIGHT_START + k].setBrightness(core.pending.armed[k] ? 1.f : 0.f);
        // The TRUE-REV lamp follows the queue flag (the CA consume rule): a consumed request —
        // pure, mixed, or STOPPED at the buffer start — clears here; an out-of-axis request
        // STAYS queued and STAYS lit (the persistence affordance, same as the verb rows).
        lights[LIGHT_TRUE_REV].setBrightness(core.pending.trueRevRequested ? 1.f : 0.f);
        return applied;
    }

    void process(const ProcessArgs& args) override {
        // Owner-guard window opens: the first driver to apply this block becomes the mutator (the
        // CA transformsAppliedThisBlock idiom — reset here, set by expanderManager.sync).
        core.transformsAppliedThisBlock = false;
        // Arming: 7 trig jacks + 7 fire buttons -> armVerb, the RANDOM back jack ->
        // armRandomReverse, the TRUE-REV jack/button -> armTrueReverse (all through their choke
        // points; the module is DRIVEN for applies — it never self-clocks, so process() does
        // nothing else).
        for (int k = 0; k < N_VERBS; ++k) {
            if (trigTrig[k].process(inputs[TRIG_START + k].getVoltage(), 0.1f, 1.f))
                armVerb(k);
            if (btnTrig[k].process(params[BTN_START + k].getValue() > 0.5f))
                armVerb(k);
        }
        if (backTrig.process(inputs[TRIG_BACK_RANDOM].getVoltage(), 0.1f, 1.f))
            armRandomReverse();
        if (trueRevTrig.process(inputs[TRIG_TRUE_REV].getVoltage(), 0.1f, 1.f))
            armTrueReverse();
        if (trueRevBtn.process(params[BTN_TRUE_REV].getValue() > 0.5f))
            armTrueReverse();
    }

    // Rack reset (context menu / init): the fresh-module state via the core (the R2
    // all-disconnected graph, no pending rows, empty trajectory, rotate step back to default,
    // the true-reverse queue clear).
    void onReset(const ResetEvent& e) override {
        core.resetAll();
        for (int k = 0; k < N_VERBS; ++k)
            lights[PENDING_LIGHT_START + k].setBrightness(0.f);
        lights[LIGHT_TRUE_REV].setBrightness(0.f);
        Module::onReset(e);
    }

    // ── serialization: delegated to the core (single-sourced; test_cantonment_module
    // round-trips the EXACT production schema). The keys: "containmentParents" (the step-1
    // raw-uint16 array, absent-key-keeps / malformed-defaults policy), "rotateStep", and
    // "trajectory" (the EXPAND ring's bounded tail, version-guarded). ──
    json_t* dataToJson() override {
        return core.toJson();
    }

    void dataFromJson(json_t* rootJ) override {
        core.fromJson(rootJ);
    }
};

// ── Widget ──────────────────────────────────────────────────────────────────────────────────
struct MonsoonCantonmentWidget : ModuleWidget,
    dotModular::Compose<MonsoonCantonmentWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;

    MonsoonCantonmentWidget(MonsoonCantonment* module) {
        setModule(module);
        // Dual-panel house convention (theme follows the host Monsoon, the junction idiom);
        // loadPanel = kit: sets the panel AND caches the #components anchors for bind-by-name.
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Cantonment_panel_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Cantonment_panel_light.svg"));
        loadPanel(asset::plugin(pluginInstance, "res/panels/Cantonment_panel_dark.svg"));
        redDot::addRedScrews(this);

        // The flank anchors (gen_containment.py): one trig jack + one fire button + one pending
        // lamp per verb row PLUS the TRUE-REV row (same families, NOT a verb row — it arms the
        // true-reverse queue) and the RANDOM row's back jack, bound BY NAME (the prefix+"verb"
        // concat form — the audit's prefix-family rule covers all of each). Verb order = the
        // panel flank order = CantonmentPending's row order (the tie-break rule) — keep the
        // three in lockstep.
        for (int k = 0; k < MonsoonCantonment::N_VERBS; ++k) {
            const std::string verb = CantonmentPanelMm::VERB_ID[k];
            bindInput <PJ301MPort>("input_trig_" + verb, MonsoonCantonment::TRIG_START + k);
            bindParam <TL1105>    ("param_btn_" + verb, MonsoonCantonment::BTN_START + k);
            bindLight<SmallLight<RedLight>>("light_pending_" + verb, MonsoonCantonment::PENDING_LIGHT_START + k);
        }
        bindInput <PJ301MPort>("input_back_random",   MonsoonCantonment::TRIG_BACK_RANDOM);
        bindInput <PJ301MPort>("input_trig_truerev",  MonsoonCantonment::TRIG_TRUE_REV);
        bindParam <TL1105>    ("param_btn_truerev",   MonsoonCantonment::BTN_TRUE_REV);
        bindLight<SmallLight<RedLight>>("light_pending_truerev", MonsoonCantonment::LIGHT_TRUE_REV);

        // The multi-pin plane overlay (transparent: consumes ONLY genuine cell hits — the CA
        // PinOverlay rule, so panel dragging + the context menu keep working everywhere else).
        auto* ov = new PinOverlay(this, module);
        ov->box.pos  = Vec(0, 0);
        ov->box.size = box.size;
        addChild(ov);

        // The read-only graph diagram (Rodney 2026-10-28): the SAME square, re-rendered as
        // the node-arrow circle. Added ABOVE the PinOverlay so its scrim covers the printed
        // grid art; it declares no event handlers (read-only by construction) and the
        // PinOverlay consumes nothing while it is up (the module's graphView gate).
        auto* gv = new GraphOverlay(this, module);
        gv->box.pos  = Vec(0, 0);
        gv->box.size = box.size;
        addChild(gv);
    }

    // One committed phrase-boundary batch = one undo step (the CA precedent). Snapshots are
    // published on the audio thread (CantonmentModuleCore::applyAtBoundary); this UI-thread
    // drain turns each into one rack history action. Same module-id resolution discipline as
    // StoreEditAction (survives deletion; no-ops while gone).
    struct CantonmentTransformUndoAction : rack::history::Action {
        int64_t  moduleId = -1;
        uint16_t before[16];
        uint16_t after[16];
        CantonmentTransformUndoAction() { name = "Cantonment transform"; }
        MonsoonCantonment* resolve() {
            return dynamic_cast<MonsoonCantonment*>(APP->engine->getModule(moduleId));
        }
        void undo() override {
            // Restores a state that WAS valid (committed states are always DAGs — every verb
            // is closure-proven), so the blind write can never promote a cycle.
            if (auto* m = resolve())
                std::memcpy(m->core.graph.parents, before, sizeof(before));
        }
        void redo() override {
            if (auto* m = resolve())
                std::memcpy(m->core.graph.parents, after, sizeof(after));
        }
    };

    // "Clear all containment pins" = up to 256 cell changes; one gesture must be ONE undo
    // step, so it gets a whole-graph snapshot action rather than N StoreEditActions (the CA
    // ResetPinsAction shape). Same module-id discipline (survives deletion; no-ops while gone).
    struct CantonmentClearAllAction : rack::history::Action {
        int64_t  moduleId;
        uint16_t oldG[16];
        CantonmentClearAllAction(MonsoonCantonment* m) : moduleId(m->id) {
            name = "clear containment pins";
            std::memcpy(oldG, m->core.graph.parents, sizeof(oldG));
        }
        MonsoonCantonment* resolve() {
            return dynamic_cast<MonsoonCantonment*>(APP->engine->getModule(moduleId));
        }
        void undo() override {
            if (auto* m = resolve())
                std::memcpy(m->core.graph.parents, oldG, sizeof(oldG));
        }
        void redo() override {
            if (auto* m = resolve()) m->core.clearAllPins();
        }
    };

    // ── the one-plane multi-pin matrix overlay ───────────────────────────────────────────────
    // TransparentWidget, NOT Opaque: an opaque overlay sized to the module box consumed every
    // left-press (the CA lesson). We consume ONLY genuine cell hits. Click model (the CA
    // row-radio is DROPPED for this plane — multi-pin rows ARE the intersection): left/right
    // click both TOGGLE the cell's pin. A pin that would close a cycle is REJECTED WITH AN
    // AFFORDANCE (red flash + WOULD-CYCLE hover readout), never silently (the build plan's
    // "UI SHOWS the rejection" rule).
    struct PinOverlay : widget::TransparentWidget {
        MonsoonCantonmentWidget* owner;   // theme + kit lookups
        MonsoonCantonment*       module;
        int hoverV = -1, hoverU = -1;      // crosshair target (-1 = none)
        int rejectV = -1, rejectU = -1;    // the cycle-reject flash cell (-1 = none)
        double rejectUntil = 0.0;          // steady-clock seconds: flash window end

        PinOverlay(MonsoonCantonmentWidget* o, MonsoonCantonment* m) : owner(o), module(m) {}

        static double nowSec() {
            return std::chrono::duration<double>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        static Vec cellCentre(int v, int u) {
            using namespace CantonmentPanelMm;
            return mm2px(Vec(GRID_X + (u + 0.5f) * CELL, GRID_Y + (v + 0.5f) * CELL));
        }
        static bool hitCell(Vec p, int v, int u) {
            const Vec c = cellCentre(v, u);
            const float r = mm2px(CantonmentPanelMm::CELL * 0.5f);
            const float dx = p.x - c.x, dy = p.y - c.y;
            return dx * dx + dy * dy <= r * r;
        }

        // The operating pool (the manager's convention: numPolyVoices+1, clamped to 1..16).
        // No host Monsoon (or browser preview) = the FULL pool: every voice containable, no
        // dimming (R2 — the standalone editor does not shrink the world).
        int getActiveCount() const {
            if (!module) return dotModular::ct::VOICES;
            Monsoon* mon = redDot::lookupMonsoon(module);
            if (!mon) return dotModular::ct::VOICES;
            return math::clamp(mon->engine.numPolyVoices + 1, 1, dotModular::ct::VOICES);
        }

        // A pin that reads as a physical peg (the CA drawPin idiom, one plane). Containment's
        // plane colour = the panel's house red (#d4001a family).
        static NVGcolor pinBody() { return nvgRGBf(0.83f, 0.f, 0.10f); }

        static void drawPin(NVGcontext* vg, float cx, float cy, float r, NVGcolor body, float alpha) {
            // drop shadow
            nvgBeginPath(vg); nvgCircle(vg, cx + r * 0.12f, cy + r * 0.16f, r);
            nvgFillColor(vg, nvgRGBAf(0, 0, 0, 0.35f * alpha)); nvgFill(vg);
            // body
            nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
            NVGcolor b = body; b.a *= alpha;
            nvgFillColor(vg, b); nvgFill(vg);
            // rim (slightly darker ring)
            nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
            nvgStrokeColor(vg, nvgRGBAf(0, 0, 0, 0.30f * alpha)); nvgStrokeWidth(vg, r * 0.16f);
            nvgStroke(vg);
            // specular highlight, upper-left
            nvgBeginPath(vg); nvgCircle(vg, cx - r * 0.30f, cy - r * 0.32f, r * 0.30f);
            nvgFillColor(vg, nvgRGBAf(1, 1, 1, 0.55f * alpha)); nvgFill(vg);
        }

        void draw(const DrawArgs& args) override {
            // The graph diagram owns the square while toggled (the module's graphView):
            // nothing drawn here, the GraphOverlay scrims and re-renders instead.
            if (module && module->graphView) return;
            widget::TransparentWidget::draw(args);
            NVGcontext* vg = args.vg;
            const int active = getActiveCount();
            const float r    = mm2px(CantonmentPanelMm::CELL * 0.32f);   // peg radius (CA ratio)
            const float half = mm2px(CantonmentPanelMm::CELL * 0.5f);

            // Active-area frame around the operating pool (the CA Phase B idiom). Outside it,
            // pins are HELD (drawn dimmed) — not suspended, not identity (R2 + the transforms'
            // above-pool rule: manual pins outlive a polyphony reduction).
            if (module && active < dotModular::ct::VOICES) {
                const Vec tl = cellCentre(0, 0), br = cellCentre(active - 1, active - 1);
                nvgBeginPath(vg);
                nvgRect(vg, tl.x - half, tl.y - half,
                        (br.x + half) - (tl.x - half), (br.y + half) - (tl.y - half));
                nvgStrokeColor(vg, nvgRGBAf(1, 1, 1, 0.25f));
                nvgStrokeWidth(vg, mm2px(0.4f));
                nvgStroke(vg);
            }

            // Pins + empty-cell ghosts (the CA ghost idiom, fainter: the generator's grid art
            // already carries the diagonal self-bit shading).
            for (int v = 0; v < dotModular::ct::VOICES; ++v) {
                for (int u = 0; u < dotModular::ct::VOICES; ++u) {
                    const Vec c = cellCentre(v, u);
                    const float alpha = (v < active && u < active) ? 1.f : 0.4f;
                    if (module && module->core.graph.isContainedIn(v, u)) {
                        drawPin(vg, c.x, c.y, r, pinBody(), alpha);
                    } else {
                        nvgBeginPath(vg);
                        nvgCircle(vg, c.x, c.y, r * 0.55f);
                        nvgStrokeColor(vg, nvgRGBAf(0.5f, 0.5f, 0.5f, 0.10f));
                        nvgStrokeWidth(vg, 0.5f);
                        nvgStroke(vg);
                    }
                }
            }

            // Hover crosshair + readout (the CA XILS idiom) — including the WOULD-CYCLE
            // preview, so the rejection is visible BEFORE the click, not only after.
            if (module && hoverV >= 0 && hoverU >= 0) {
                const Vec c = cellCentre(hoverV, hoverU);
                const float gx0 = mm2px(CantonmentPanelMm::GRID_X);
                const float gy0 = mm2px(CantonmentPanelMm::GRID_Y);
                nvgStrokeColor(vg, nvgRGBAf(1, 1, 1, 0.55f));
                nvgStrokeWidth(vg, 0.8f);
                nvgBeginPath(vg); nvgMoveTo(vg, gx0, c.y); nvgLineTo(vg, c.x - r * 1.4f, c.y); nvgStroke(vg);
                nvgBeginPath(vg); nvgMoveTo(vg, c.x, gy0); nvgLineTo(vg, c.x, c.y - r * 1.4f); nvgStroke(vg);
                nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, r * 1.15f);
                nvgStrokeColor(vg, nvgRGBAf(1, 1, 1, 0.8f)); nvgStrokeWidth(vg, 0.8f); nvgStroke(vg);

                const bool pinned = module->core.graph.isContainedIn(hoverV, hoverU);
                const bool would  = !pinned && module->core.graph.wouldCycle(hoverV, hoverU);
                char buf[96];
                if (pinned)
                    snprintf(buf, sizeof(buf), "v%d in v%d  PIN -- click removes", hoverV + 1, hoverU + 1);
                else if (would)
                    snprintf(buf, sizeof(buf), "v%d in v%d  WOULD CYCLE -- rejected", hoverV + 1, hoverU + 1);
                else
                    snprintf(buf, sizeof(buf), "v%d in v%d  EMPTY -- click adds", hoverV + 1, hoverU + 1);
                std::shared_ptr<Font> font = APP->window->loadFont(
                    asset::system("res/fonts/ShareTechMono-Regular.ttf"));
                if (font) {
                    nvgFontFaceId(vg, font->handle);
                    nvgFontSize(vg, mm2px(3.2f));
                    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                    float bounds[4];
                    nvgTextBounds(vg, 0, 0, buf, NULL, bounds);
                    const float tw = bounds[2] - bounds[0] + mm2px(2.f);
                    const float th = mm2px(4.6f);
                    const float gx1 = mm2px(CantonmentPanelMm::GRID_X + CantonmentPanelMm::GRID_W);
                    const float gy1 = mm2px(CantonmentPanelMm::GRID_Y + CantonmentPanelMm::GRID_W);
                    float tx = c.x + r * 1.8f, ty = c.y - th * 0.5f;
                    if (tx + tw > gx1) tx = c.x - r * 1.8f - tw;
                    if (ty < gy0) ty = gy0;
                    if (ty + th > gy1) ty = gy1 - th;
                    nvgBeginPath(vg); nvgRect(vg, tx, ty, tw, th);
                    nvgFillColor(vg, nvgRGBAf(0, 0, 0, 0.8f)); nvgFill(vg);
                    nvgFillColor(vg, nvgRGBf(0.95f, 0.95f, 0.94f));
                    nvgText(vg, tx + mm2px(1.f), ty + th * 0.5f, buf, NULL);
                }
            }

            // THE REJECTION AFFORDANCE: a rejected pin attempt flashes the cell red with a
            // slash for a moment — the click is consumed, the state is UNCHANGED, the graph
            // invariant is untouched (the plan: "UI SHOWS the rejection, never silently fails").
            if (module && rejectV >= 0 && nowSec() < rejectUntil) {
                const Vec c = cellCentre(rejectV, rejectU);
                nvgBeginPath(vg); nvgRect(vg, c.x - half, c.y - half, half * 2.f, half * 2.f);
                nvgFillColor(vg, nvgRGBAf(0.85f, 0.10f, 0.10f, 0.30f)); nvgFill(vg);
                nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, r * 1.15f);
                nvgStrokeColor(vg, nvgRGBAf(1.f, 0.25f, 0.25f, 0.9f));
                nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
                nvgBeginPath(vg);
                nvgMoveTo(vg, c.x - r * 0.7f, c.y - r * 0.7f);
                nvgLineTo(vg, c.x + r * 0.7f, c.y + r * 0.7f);
                nvgStrokeColor(vg, nvgRGBAf(1.f, 0.35f, 0.35f, 0.9f));
                nvgStrokeWidth(vg, 1.2f); nvgStroke(vg);
            }
        }

        // Track the hovered cell for the crosshair; keep events passing through.
        void onHover(const event::Hover& e) override {
            hoverV = hoverU = -1;
            if (module && module->graphView) { TransparentWidget::onHover(e); return; }   // no crosshair under the diagram
            for (int v = 0; v < dotModular::ct::VOICES && hoverV < 0; ++v)
                for (int u = 0; u < dotModular::ct::VOICES; ++u)
                    if (hitCell(e.pos, v, u)) { hoverV = v; hoverU = u; break; }
            TransparentWidget::onHover(e);
        }
        void onLeave(const event::Leave& e) override {
            hoverV = hoverU = -1;
            TransparentWidget::onLeave(e);
        }

        // Click = toggle the cell's pin (single plane; both buttons, the row-radio is dropped).
        // Store-backed + undoable (StoreEditAction — the pins are NOT params, zero DAW slots):
        // the action targets the module id and bakes (v, u) into the setter, so undo lands on
        // the cell actually edited. Equal old/new never records. THE SAFETY RULE: the setter
        // re-runs the edit through the graph's checks, so a LATE undo (after a transform moved
        // the roles underneath it) that would close a cycle simply no-ops — safety over
        // fidelity; the alternative (a promoted cycle) is the one thing this module may never do.
        void onButton(const event::Button& e) override {
            // graphView: the read-only contract — while the diagram is up this plane
            // consumes NOTHING (no edits, no rejection flashes); clicks fall through to the
            // panel like any other square of blank fascia.
            if (!module || e.action != GLFW_PRESS || module->graphView) { TransparentWidget::onButton(e); return; }
            for (int v = 0; v < dotModular::ct::VOICES; ++v) {
                for (int u = 0; u < dotModular::ct::VOICES; ++u) {
                    if (!hitCell(e.pos, v, u)) continue;
                    const bool had  = module->core.graph.isContainedIn(v, u);
                    const bool want = !had;
                    if (want && module->core.graph.wouldCycle(v, u)) {
                        // REJECTED: show it, change nothing, consume the click (falling through
                        // would start a panel drag under the user's hand).
                        rejectV = v; rejectU = u;
                        rejectUntil = nowSec() + 0.7;
                        e.consume(this);
                        return;
                    }
                    redDot::applyAndPushStoreEdit<MonsoonCantonment>(
                        module,
                        want ? "add containment pin" : "remove containment pin",
                        [v, u](MonsoonCantonment& m, bool contained) {
                            if (contained) m.core.graph.addParent(v, u);
                            else           m.core.graph.removeParent(v, u);
                        },
                        had, want);
                    e.consume(this);
                    return;
                }
            }
            TransparentWidget::onButton(e);
        }
    };

    // ── the read-only graph diagram (Rodney 2026-10-28: "drawn diagrams like this, not
    // editable just for visualisation") ─────────────────────────────────────────────────────
    // The SAME one-plane direct-offer edges the matrix pegs, re-rendered as the classic
    // node-arrow circle: 16 numbered pegs (voice 1 at 12 o'clock, clockwise), one arrow per
    // containment edge pointing CHILD -> PARENT (the module's "points at my parent" language),
    // roots (no parents) in the sample image's mint green, the operating pool arced (the
    // matrix active-frame idiom, angular form), above-pool voices and their edges dimmed-held
    // (the matrix 0.4-alpha idiom). MULTI-PARENT SAFE: the plane is a DAG, not just the draw's
    // strict forests — every set bit draws its own arrow (the matrix's caption says so).
    // Read-only BY CONSTRUCTION: no event overrides at all — events fall through to the
    // PinOverlay, which consumes nothing while the diagram is up (the module's graphView gate).
    // Runtime toggle only (the context-menu check item) — NOT in the save schema, the five
    // persisted fields stay five, and the matrix returns on load.
    struct GraphOverlay : widget::TransparentWidget {
        MonsoonCantonmentWidget* owner;   // theme lookup (the PinOverlay idiom)
        MonsoonCantonment*       module;

        GraphOverlay(MonsoonCantonmentWidget* o, MonsoonCantonment* m) : owner(o), module(m) {}

        // The operating pool (the PinOverlay idiom verbatim: numPolyVoices+1 clamped 1..16;
        // no host Monsoon or browser preview = the FULL pool, no dimming — R2).
        int getActiveCount() const {
            if (!module) return dotModular::ct::VOICES;
            Monsoon* mon = redDot::lookupMonsoon(module);
            if (!mon) return dotModular::ct::VOICES;
            return math::clamp(mon->engine.numPolyVoices + 1, 1, dotModular::ct::VOICES);
        }

        // Voice 1 at 12 o'clock, clockwise (16 slots = 22.5 degrees). Literal angles — no
        // M_PI dependency (the -D_USE_MATH_DEFINES dance is Windows-only noise we skip).
        static constexpr float kStartAngle = -1.57079632679f;   // -pi/2 (12 o'clock)
        static constexpr float kTwoPi      =  6.28318530718f;
        static float nodeAngle(int i) {
            return kStartAngle + float(i) * (kTwoPi / float(dotModular::ct::VOICES));
        }
        static Vec nodeCentre(int i, Vec c0, float R) {
            const float a = nodeAngle(i);
            return Vec(c0.x + R * std::cos(a), c0.y + R * std::sin(a));
        }

        // Roots (no parents) wear the sample's mint green; contained voices stay house red
        // (the PinOverlay plane colour).
        static NVGcolor rootNode() { return nvgRGBf(0.36f, 0.72f, 0.50f); }

        // One containment edge: shaft trimmed clear of both pegs + a filled head at the
        // parent's rim (the sample's straight-arrow look, the house red).
        static void drawArrow(NVGcontext* vg, Vec from, Vec to, float pegR, float alpha) {
            const float dx = to.x - from.x, dy = to.y - from.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-4f) return;
            const float ux = dx / len, uy = dy / len;
            NVGcolor col = PinOverlay::pinBody(); col.a *= 0.8f * alpha;
            const float sx = from.x + ux * (pegR + mm2px(0.5f));
            const float sy = from.y + uy * (pegR + mm2px(0.5f));
            const float ex = to.x   - ux * (pegR + mm2px(1.9f));
            const float ey = to.y   - uy * (pegR + mm2px(1.9f));
            nvgBeginPath(vg); nvgMoveTo(vg, sx, sy); nvgLineTo(vg, ex, ey);
            nvgStrokeColor(vg, col); nvgStrokeWidth(vg, mm2px(0.55f)); nvgStroke(vg);
            const float tx = to.x - ux * (pegR + mm2px(0.4f));
            const float ty = to.y - uy * (pegR + mm2px(0.4f));
            const float px = -uy, py = ux;   // perpendicular for the head's base
            nvgBeginPath(vg);
            nvgMoveTo(vg, tx, ty);
            nvgLineTo(vg, ex + px * mm2px(0.85f), ey + py * mm2px(0.85f));
            nvgLineTo(vg, ex - px * mm2px(0.85f), ey - py * mm2px(0.85f));
            nvgClosePath(vg);
            nvgFillColor(vg, col); nvgFill(vg);
        }

        void draw(const DrawArgs& args) override {
            if (!module || !module->graphView) return;   // the matrix owns the square unless toggled
            NVGcontext* vg = args.vg;
            using namespace CantonmentPanelMm;
            const bool lit = (owner && owner->lastThemeLight > 0);
            const NVGcolor ink = lit ? nvgRGB(0x2a, 0x2a, 0x2e) : nvgRGB(0xe8, 0xe2, 0xd0);

            // Scrim over the printed grid art (theme-matched: the diagram floats on its own
            // paper, the generator's cells do not ghost through), hairline border in ink.
            const float x0 = mm2px(GRID_X), y0 = mm2px(GRID_Y), gsz = mm2px(GRID_W);
            nvgBeginPath(vg); nvgRoundedRect(vg, x0, y0, gsz, gsz, mm2px(1.2f));
            nvgFillColor(vg, lit ? nvgRGBAf(0.95f, 0.94f, 0.91f, 0.94f)
                                 : nvgRGBAf(0.055f, 0.055f, 0.065f, 0.94f));
            nvgFill(vg);
            NVGcolor rimc = ink; rimc.a *= 0.18f;
            nvgBeginPath(vg); nvgRoundedRect(vg, x0, y0, gsz, gsz, mm2px(1.2f));
            nvgStrokeColor(vg, rimc); nvgStrokeWidth(vg, mm2px(0.4f)); nvgStroke(vg);

            const Vec c0 = Vec(x0 + gsz * 0.5f, y0 + gsz * 0.5f);
            const float R  = gsz * 0.5f - mm2px(8.0f);   // 8 mm inboard: numbers live outside the ring
            const float pr = mm2px(2.9f);
            const int active = getActiveCount();

            // The operating pool arc (the matrix active-frame idiom, angular form): the
            // clockwise span from voice 1 over the active voices, same weight/alpha.
            if (active < dotModular::ct::VOICES) {
                nvgBeginPath(vg);
                nvgArc(vg, c0.x, c0.y, R + pr + mm2px(1.0f),
                       nodeAngle(0), nodeAngle(active - 1), NVG_CW);
                nvgStrokeColor(vg, nvgRGBAf(1, 1, 1, 0.25f));
                nvgStrokeWidth(vg, mm2px(0.4f));
                nvgStroke(vg);
            }

            // Edges FIRST (under the pegs): the matrix's own isContainedIn read, so the
            // diagram and the pegs can never disagree. Above-pool endpoints dim (the held idiom).
            for (int v = 0; v < dotModular::ct::VOICES; ++v)
                for (int u = 0; u < dotModular::ct::VOICES; ++u) {
                    if (u == v || !module->core.graph.isContainedIn(v, u)) continue;
                    const float ea = (v < active && u < active) ? 1.f : 0.4f;
                    drawArrow(vg, nodeCentre(v, c0, R), nodeCentre(u, c0, R), pr, ea);
                }

            // Nodes: contained voices red, roots mint (the sample's two-colour scheme).
            for (int v = 0; v < dotModular::ct::VOICES; ++v) {
                const Vec c = nodeCentre(v, c0, R);
                const float a = (v < active) ? 1.f : 0.4f;
                PinOverlay::drawPin(vg, c.x, c.y, pr,
                                    module->core.graph.parents[v] ? PinOverlay::pinBody() : rootNode(), a);
            }

            // Numbers 1..16, radially outside the ring (arrows never leave the circle) —
            // uiFont, the widget-lettering idiom, theme-matched ink.
            std::shared_ptr<window::Font> font = APP->window->uiFont;
            if (font) {
                nvgFontFaceId(vg, font->handle);
                nvgFontSize(vg, mm2px(2.4f));
                nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, ink);
                for (int v = 0; v < dotModular::ct::VOICES; ++v) {
                    const float a = nodeAngle(v);
                    const Vec c = Vec(c0.x + (R + pr + mm2px(2.6f)) * std::cos(a),
                                      c0.y + (R + pr + mm2px(2.6f)) * std::sin(a));
                    char b[4];
                    snprintf(b, sizeof(b), "%d", v + 1);
                    nvgText(vg, c.x, c.y, b, NULL);
                }
            }
        }
    };

    void step() override {
        ModuleWidget::step();
        kitStep();          // kit: dev live-reload poll (Option B-full)
        if (!module) return;

        // Drain the transform-undo ring produced on the audio thread. Each snapshot becomes one
        // rack history action (the UI-thread push is required). SPSC: we are the sole consumer.
        if (auto* c = dynamic_cast<MonsoonCantonment*>(module)) {
            uint32_t t = c->core.undoTail.load(std::memory_order_relaxed);
            uint32_t h = c->core.undoHead.load(std::memory_order_acquire);
            while (t != h) {
                const auto& snap = c->core.undoRing[t % dotModular::ct::CantonmentModuleCore::UNDO_RING];
                auto* act = new CantonmentTransformUndoAction();
                act->moduleId = c->id;
                std::memcpy(act->before, snap.before, sizeof(act->before));
                std::memcpy(act->after,  snap.after,  sizeof(act->after));
                APP->history->push(act);
                ++t;
            }
            c->core.undoTail.store(t, std::memory_order_release);
        }

        Monsoon* m = redDot::lookupMonsoon(module);
        const int wantLight = (m && m->lightTheme) ? 1 : 0;
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

    // Lettering is widget-drawn at the generator's mm coordinates (nanosvg drops <text>):
    // title, side captions, group captions, verb labels, and the 1..16 vertex numbers on both
    // matrix axes. ASCII only — uiFont glyph coverage is not guaranteed beyond that.
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font) return;
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, font->handle);
        using namespace CantonmentPanelMm;
        const bool lit = (lastThemeLight > 0);
        const NVGcolor ink = lit ? nvgRGB(0x2a, 0x2a, 0x2e) : nvgRGB(0xe8, 0xe2, 0xd0);
        const NVGcolor dim = lit ? nvgRGB(0x88, 0x8d, 0x96) : nvgRGB(0x8a, 0x85, 0x78);
        // The graph diagram replaces the matrix-geometry lettering: the circle carries its
        // own 1..16 numbers, and the bottom captions describe the arrows instead of the axes.
        auto* mcp = dynamic_cast<MonsoonCantonment*>(module);
        const bool gview = (mcp && mcp->graphView);

        // Title — centred, CA's widget-drawn-title idiom.
        nvgFillColor(vg, ink);
        nvgFontSize(vg, mm2px(3.6f));
        nvgTextLetterSpacing(vg, mm2px(0.4f));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, box.size.x * 0.5f, mm2px(TITLE_Y), "CONTAINMENT", NULL);
        nvgTextLetterSpacing(vg, 0.f);

        // Side captions over the control columns; group captions over the verb groups.
        nvgFillColor(vg, dim);
        nvgFontSize(vg, mm2px(2.0f));
        nvgText(vg, mm2px(J_X), mm2px(SIDE_Y), "TRIG", NULL);
        nvgText(vg, mm2px(PW - J_X), mm2px(SIDE_Y), "FIRE", NULL);
        nvgText(vg, mm2px(J_X), mm2px(ROW_Y[0] - 7.5f), "ROLES", NULL);
        nvgText(vg, mm2px(J_X), mm2px(ROW_Y[3] - 7.5f), "FOLD", NULL);

        // Verb labels — right-aligned inboard of the voice-number column.
        nvgFillColor(vg, ink);
        nvgFontSize(vg, mm2px(2.8f));
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        for (int k = 0; k < MonsoonCantonment::N_VERBS; ++k)
            nvgText(vg, mm2px(LABEL_X), mm2px(ROW_Y[k]), VERB_LABEL[k], NULL);
        // The TRUE-REV row label (NOT a verb — the global-reverse consumer's flank row; the
        // generator places it in its own group gap below RANDOM, the CA true-rev group shape).
        nvgText(vg, mm2px(LABEL_X), mm2px(TRUE_REV_Y), "TRUE-REV", NULL);

        // Vertex numbers: voices 1..16 down the left edge, parents 1..16 across the top edge.
        // Matrix geometry only — the graph diagram's circle numbers take over in that view.
        if (!gview) {
            nvgFillColor(vg, dim);
            nvgFontSize(vg, mm2px(2.0f));
            for (int i = 0; i < 16; ++i) {
                char b[4];
                snprintf(b, sizeof(b), "%d", i + 1);
                nvgText(vg, mm2px(NUM_X), mm2px(GRID_Y + (i + 0.5f) * CELL), b, NULL);
                nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgText(vg, mm2px(GRID_X + (i + 0.5f) * CELL), mm2px(SIDE_Y), b, NULL);
                nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            }
        }

        // Bottom captions (ASCII separators only) — swapped under the graph diagram.
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        if (gview) {
            nvgText(vg, box.size.x * 0.5f, mm2px(CAP1_Y),
                    "READ-ONLY VIEW -- EDIT IN THE PIN MATRIX", NULL);
            nvgText(vg, box.size.x * 0.5f, mm2px(CAP2_Y),
                    "ARROWS POINT CHILD -> PARENT    GREEN = ROOT", NULL);
        } else {
            nvgText(vg, box.size.x * 0.5f, mm2px(CAP1_Y),
                    "ROWS = VOICE    COLUMNS = PARENT (CONTAINED-BY)", NULL);
            nvgText(vg, box.size.x * 0.5f, mm2px(CAP2_Y),
                    "MULTI-PARENT ACYCLIC DAG    INTERSECTION OF CONTAINMENTS", NULL);
        }
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* module = dynamic_cast<MonsoonCantonment*>(this->module);
        if (!module) return;
        menu->addChild(new MenuSeparator);

        // ROTATE's <=1 param (the plan: transform step config via context menu, not param
        // slots — zero DAW-visible slots by design). 1..7 mirrors CA's positive step range;
        // the verb's clampStep owns the runtime clamp (0/span can never select nothing).
        menu->addChild(createSubmenuItem("Rotate step", "",
            [module](Menu* sub) {
                for (int s = 1; s <= 7; ++s) {
                    sub->addChild(createCheckMenuItem(
                        (s == 1) ? "1  (default)" : std::to_string(s), "",
                        [module, s]() { return module->core.pending.rotateStep == s; },
                        [module, s]() { module->core.pending.rotateStep = s; }));
                }
            }));

        // The RANDOM row's config (the rotateStep precedent — context menu, zero param slots):
        // the page interpretation (the two variations, Rodney 2026-10-27) and the density dial
        // (p = density/10; the F-5 recipe's p=0.28 sits at the default 3).
        menu->addChild(createSubmenuItem("Random mode", "",
            [module](Menu* sub) {
                sub->addChild(createCheckMenuItem("Outright new forest  (default)", "",
                    [module]() { return module->core.pending.randomMode == 0; },
                    [module]() { module->core.pending.randomMode = 0; }));
                sub->addChild(createCheckMenuItem("Scatter (modify current)", "",
                    [module]() { return module->core.pending.randomMode == 1; },
                    [module]() { module->core.pending.randomMode = 1; }));
            }));
        menu->addChild(createSubmenuItem("Random density", "",
            [module](Menu* sub) {
                for (int d = 1; d <= 10; ++d) {
                    sub->addChild(createCheckMenuItem(
                        (d == 3) ? "3  (default)" : std::to_string(d), "",
                        [module, d]() { return module->core.pending.randomDensity == d; },
                        [module, d]() { module->core.pending.randomDensity = d; }));
                }
            }));
        // The islands dial (path B, Rodney 2026-10-27: "islands are desired... groups
        // connected within and disconnected from others with some disconnected single
        // voices too"): the drawn forest's partition count — each island internally
        // connected with exactly one root, islands mutually disconnected, a size-1 island
        // is a free single voice. Consumed by OUTRIGHT (the stateless draw); SCATTER
        // ignores it (a modify pass over current edges). The draw spans the polyphony at
        // draw; rows above the pool clear to free singles.
        menu->addChild(createSubmenuItem("Random islands", "",
            [module](Menu* sub) {
                for (int k = 1; k <= 16; ++k) {
                    sub->addChild(createCheckMenuItem(
                        (k == 1) ? "1  (default)" : std::to_string(k), "",
                        [module, k]() { return module->core.pending.randomIslands == k; },
                        [module, k]() { module->core.pending.randomIslands = k; }));
                }
            }));
        // The reduction post-pass flag (the banked ONE CALL, IDEAS_PARKED.md — "a flag, not
        // forced: random shortcut edges may be exactly the variety wanted"): when ON, an
        // OUTRIGHT draw runs reduceTransitive(g) after the boost — the page's implied shortcut
        // pins strip to the load-bearing spine. Islands, roots and the reverse-draw restore
        // survive (the T11 properties ride the strip); off keeps the dice's full variety.
        // SCATTER ignores it; the DECLUTTER gesture row (manual-graph reduce) stays parked.
        menu->addChild(createSubmenuItem("Random reduce", "",
            [module](Menu* sub) {
                sub->addChild(createCheckMenuItem("Off  (default)", "",
                    [module]() { return !module->core.pending.randomReduce; },
                    [module]() { module->core.pending.randomReduce = false; }));
                sub->addChild(createCheckMenuItem("On  (declutter the draw)", "",
                    [module]() { return module->core.pending.randomReduce; },
                    [module]() { module->core.pending.randomReduce = true; }));
            }));
        // The reverse-draw action (requirement 2 — the CA back action): arms the RANDOM row
        // backward; the boundary commit re-derives the previous page (an EXACT state restore
        // in Outright mode, the generative scrub in Scatter mode). One pending row, one undo
        // step, the standard lamp affordance. Two entry points share the choke point: this
        // menu item and the RANDOM row's hardware back jack (the CA scatter-back shape).
        menu->addChild(createMenuItem("Random: reverse draw", "",
            [module]() { module->armRandomReverse(); }));

        menu->addChild(createMenuItem("Clear all containment pins", "",
            [module]() {
                // Skip the no-op (already the R2 default) so undo history stays clean.
                bool empty = true;
                for (int v = 0; v < dotModular::ct::VOICES; ++v)
                    if (module->core.graph.parents[v]) { empty = false; break; }
                if (empty) return;
                auto* act = new CantonmentClearAllAction(module);
                module->core.clearAllPins();
                APP->history->push(act);
            }));

        // The read-only graph diagram (Rodney 2026-10-28: "drawn diagrams like this, not
        // editable just for visualisation"): the same one-plane direct-offer edges the
        // matrix pegs, re-rendered as the node-arrow circle (voice 1 at 12 o'clock, arrows
        // child -> parent, roots green, the operating pool arced, above-pool dimmed-held).
        // A VIEW, not a musical state: runtime-only, deliberately OUTSIDE the save schema
        // (the matrix returns on load; the five persisted fields stay five). Read-only by
        // construction — the overlay declares no event handlers, and the PinOverlay consumes
        // nothing while the diagram is up. The matrix stays the editor, always.
        menu->addChild(createCheckMenuItem("Graph diagram (read-only view)", "",
            [module]() { return module->graphView; },
            [module]() { module->graphView = !module->graphView; }));
    }
};
