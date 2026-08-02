#pragma once
// MonsoonChangeAlleyV2 — 16×16 pin-matrix expander
// Rows = consuming voices (0=mono/V1, 1..15=poly V2..V16)
// Columns = source voices (same indexing)
// Two pin types per cell: white=rhythm, red=melody (concentric when both)
// Row-radio: exactly one rhythm pin and one melody pin per row.
// Up to 16 rows may share the same column (fan-in is the musical point).
// Default: identity diagonal (rhythmSrc[v]=v, melodySrc[v]=v).
// Does NOT require Straits — operates at the Philox table level.
// Zero param slots by design (DAW_PARAM_AUDIT.md).

#include <rack.hpp>
#include <cmath>
#include <cstdio>
#include <atomic>
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "ui/StoreEditAction.hpp"   // pin edits: store-backed, undoable (DAW_PARAM_AUDIT 5b)
#include "dsp/ChangeAlleyTransforms.hpp"   // ca::applyCorrelation (transform apply owned here)

using namespace rack;
// NOT 'using namespace ChangeAlleyIds' — Monsoon.hpp exposes MonsoonIds with the same
// NUM_PARAMS/NUM_INPUTS/... names, so we qualify explicitly (same rule as the Sands
// managers: 'NOT using namespace ... to avoid ambiguous calls — qualify below').
namespace CA = ChangeAlleyV2Ids;

struct MonsoonChangeAlleyV2 : Module {
    uint8_t rhythmSrc[CA::N_VOICES];
    uint8_t melodySrc[CA::N_VOICES];

    static constexpr const char* CURRENCIES[CA::N_VOICES] = {
        "SGD","MYR","IDR","THB","PHP","VND","MMK","KHR",
        "HKD","CNY","TWD","KRW","JPY","AUD","INR","USD",
    };

    // ── Transforms (§15): the full Temasek control set, in one module ──────────────────
    CA::PendingAction pendingRows[CA::N_ROWS];
    rack::dsp::SchmittTrigger domTrig  [CA::N_ROWS];
    rack::dsp::SchmittTrigger codTrig  [CA::N_ROWS];
    rack::dsp::BooleanTrigger btnTrig  [CA::N_ROWS * 2];
    rack::dsp::SchmittTrigger sBackDom [CA::SIDES * CA::TYPES];
    rack::dsp::SchmittTrigger sBackCod [CA::SIDES * CA::TYPES];
    uint64_t scatterCounter[CA::SIDES * CA::TYPES * 2] = {};

    // --- Transform-undo groundwork (item 5) ---------------------------------------------------
    // applyPendingTransforms() runs on the AUDIO thread (control-rate, Monsoon::process), where
    // APP->history->push is ILLEGAL (UI-thread only). So we snapshot the pre-transform pin state
    // into a small single-producer/single-consumer RING here, and the widget's step() (UI thread)
    // drains it and pushes a Rack history action per committed transform. Transform commits are
    // infrequent (phrase boundaries), so 16 slots is ample.
    struct TransformUndoSnapshot {
        uint8_t  beforeR[CA::N_VOICES];
        uint8_t  beforeM[CA::N_VOICES];
        uint8_t  afterR[CA::N_VOICES];
        uint8_t  afterM[CA::N_VOICES];
        uint64_t counterBefore[CA::SIDES * CA::TYPES * 2];
        uint64_t counterAfter [CA::SIDES * CA::TYPES * 2];
    };
    static constexpr int UNDO_RING = 16;
    TransformUndoSnapshot undoRing[UNDO_RING];
    std::atomic<uint32_t> undoHead{0};   // producer (audio) writes, then advances
    std::atomic<uint32_t> undoTail{0};   // consumer (UI) reads, then advances

    MonsoonChangeAlleyV2() {
        config(CA::NUM_PARAMS_TOTAL, CA::NUM_INPUTS, 0, CA::NUM_LIGHTS);
        static const char* VN[CA::N_VERBS] = {"Collapse","Rotate","Reflect","Scatter"};
        static const char* SN[CA::SIDES]   = {"Intra","Inter"};
        static const char* PN[CA::TYPES]   = {"Rhythm","Melody"};
        static const char* GL[] = {"1","2","4","8","16"};
        for (int v = 0; v < CA::N_VERBS; ++v)
          for (int sd = 0; sd < CA::SIDES; ++sd)
            for (int ty = 0; ty < CA::TYPES; ++ty) {
                const int r = CA::rowId(v, sd, ty);
                const std::string nm = std::string(VN[v]) + " " + SN[sd] + " " + PN[ty];
                configSwitch(CA::GRAIN_START + r, 0.f, 4.f, 2.f, nm + " grain",
                             {GL[0],GL[1],GL[2],GL[3],GL[4]});
            }
        for (int r = 0; r < CA::N_ROWS / 2; ++r) {
            configParam(CA::LEADER_START + r, 0.f, 15.f, 0.f, "Leader offset")->snapEnabled = true;
            configParam(CA::STEP_START   + r, -7.f, 7.f, 1.f, "Step")->snapEnabled = true;
        }
        for (int i = 0; i < CA::N_ROWS * 2; ++i)
            configButton(CA::BTN_START + i, (i % 2 == 0) ? "Domain trigger" : "Codomain trigger");
        for (int r = 0; r < CA::N_ROWS; ++r) {
            configInput(CA::DOMAIN_TRIG_START   + r, "Domain trigger");
            configInput(CA::CODOMAIN_TRIG_START + r, "Codomain trigger");
        }
        for (int i = 0; i < CA::SIDES * CA::TYPES; ++i) {
            configInput(CA::SCATTER_BACK_DOM_START + i, "Scatter domain back");
            configInput(CA::SCATTER_BACK_COD_START + i, "Scatter codomain back");
        }
        configInput(CA::GRAIN_POLY_IN, "Grain poly CV (16ch -> 16 grain knobs; mono=all)");
        configInput(CA::STEP_POLY_IN,  "Step poly CV (ch 1-4 leader, 5-8 step; mono=all)");
        resetToIdentity();
    }

    static int grainFromKnob(float v) {
        static const int B[5] = {1,2,4,8,16};
        int i = (int)std::lround(v); if (i < 0) i = 0; if (i > 4) i = 4;
        return B[i];
    }

    // Poly CV read with MONO NORMALLING: a 1-channel cable drives ALL channels (the standard
    // Rack idiom -- a mono LFO into a poly mod input modulates everything equally).
    static float polyCV(rack::engine::Input& in, int channel) {
        if (!in.isConnected()) return 0.f;
        return (in.getChannels() <= 1) ? in.getVoltage(0) : in.getVoltage(channel);
    }

    void latchRow(int r, int verb, int side, int type, bool domain) {
        auto& p    = pendingRows[r];
        p.armed    = true;
        p.isDomain = domain;
        p.isInter  = (side == 1);
        // Grain = knob + poly CV (channel = row). No attenuverter (§ Rodney): 16 channels
        // map straight to the 16 grain knobs. CV is added in knob-detent units (0..4).
        float gv = params[CA::GRAIN_START + r].getValue();
        gv += polyCV(inputs[CA::GRAIN_POLY_IN], r) * 0.4f;   // ~2V per detent, mono-normalled
        p.grain    = grainFromKnob(gv);
        if      (verb == CA::V_COLLAPSE) {
            const int li = side*CA::TYPES + type;           // 0..3 -> STEP poly ch 1..4
            float lv = params[CA::LEADER_START + li].getValue();
            lv += polyCV(inputs[CA::STEP_POLY_IN], li);      // 1V per leader step
            p.leaderOrStep = (int)std::lround(lv);
        }
        else if (verb == CA::V_ROTATE)
            {   const int si = side*CA::TYPES + type;
                const int sch = 4 + si;                      // 4..7 -> STEP poly ch 5..8
                float sv = params[CA::STEP_START + si].getValue();
                sv += polyCV(inputs[CA::STEP_POLY_IN], sch); // 1V per step
                p.leaderOrStep = (int)std::lround(sv); }
        else
            p.leaderOrStep = 0;
        if (verb == CA::V_SCATTER) p.scatterDelta = 1;
        lights[CA::PENDING_LIGHT_START + r].setBrightness(1.f);
    }

    // Apply all ARMED pending transforms to this module's own pin matrix. Owns the state
    // mutation (rhythmSrc/melodySrc + scatterCounter) -- the manager only decides WHEN to call
    // this (phrase boundary / unlock). Moved out of MonsoonExpanderManager so the module that
    // holds the state also owns its mutation (and, next, its undo snapshot). `active` is the
    // active voice count (numPolyVoices+1, clamped >=1).
    void applyPendingTransforms(int active) {
        // Any armed row this call?  If none, nothing to snapshot or apply.
        bool any = false;
        for (int row = 0; row < CA::N_ROWS; ++row) if (pendingRows[row].armed) { any = true; break; }
        if (!any) return;

        // Snapshot BEFORE (whole pin matrix + scatter counters). One phrase-boundary commit = one
        // undo step (mirrors ResetPinsAction: a multi-change gesture is a single snapshot).
        TransformUndoSnapshot snap;
        for (int v = 0; v < CA::N_VOICES; ++v) { snap.beforeR[v] = rhythmSrc[v]; snap.beforeM[v] = melodySrc[v]; }
        for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) snap.counterBefore[i] = scatterCounter[i];

        for (int row = 0; row < CA::N_ROWS; ++row) {
            auto& p = pendingRows[row];
            if (!p.armed) continue;
            const int verb = row / 4;
            const int side = (row % 4) / 2;
            const int type = row % 2;
            uint8_t* tbl   = (type == 0) ? rhythmSrc : melodySrc;
            const int ci   = (side * CA::TYPES + type) * 2 + (p.isDomain ? 0 : 1);
            if (verb == CA::V_SCATTER)
                scatterCounter[ci] += (uint64_t)(int64_t)p.scatterDelta;
            dotModular::ca::applyCorrelation(
                verb, p.isDomain, p.isInter,
                tbl, active, p.grain, p.leaderOrStep, scatterCounter[ci]);
            p.armed = false;
            lights[CA::PENDING_LIGHT_START + row].setBrightness(0.f);
        }

        // Snapshot AFTER, and publish to the ring for the UI thread to turn into a history action.
        for (int v = 0; v < CA::N_VOICES; ++v) { snap.afterR[v] = rhythmSrc[v]; snap.afterM[v] = melodySrc[v]; }
        for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) snap.counterAfter[i] = scatterCounter[i];
        const uint32_t h = undoHead.load(std::memory_order_relaxed);
        const uint32_t t = undoTail.load(std::memory_order_acquire);
        if (h - t < (uint32_t)UNDO_RING) {           // drop if UI hasn't drained (never in practice)
            undoRing[h % UNDO_RING] = snap;
            undoHead.store(h + 1, std::memory_order_release);
        }
    }

    void process(const ProcessArgs&) override {
        for (int v = 0; v < CA::N_VERBS; ++v)
          for (int sd = 0; sd < CA::SIDES; ++sd)
            for (int ty = 0; ty < CA::TYPES; ++ty) {
                const int r = CA::rowId(v, sd, ty);
                if (domTrig[r].process(inputs[CA::DOMAIN_TRIG_START + r].getVoltage(), 0.1f, 1.f))
                    latchRow(r, v, sd, ty, true);
                if (codTrig[r].process(inputs[CA::CODOMAIN_TRIG_START + r].getVoltage(), 0.1f, 1.f))
                    latchRow(r, v, sd, ty, false);
                if (btnTrig[r*2].process(params[CA::BTN_START + r*2].getValue() > 0.5f))
                    latchRow(r, v, sd, ty, true);
                if (btnTrig[r*2+1].process(params[CA::BTN_START + r*2+1].getValue() > 0.5f))
                    latchRow(r, v, sd, ty, false);
            }
        for (int sd = 0; sd < CA::SIDES; ++sd)
          for (int ty = 0; ty < CA::TYPES; ++ty) {
            const int i = sd * CA::TYPES + ty;
            const int r = CA::rowId(CA::V_SCATTER, sd, ty);
            if (sBackDom[i].process(inputs[CA::SCATTER_BACK_DOM_START + i].getVoltage(), 0.1f, 1.f)) {
                latchRow(r, CA::V_SCATTER, sd, ty, true);  pendingRows[r].scatterDelta = -1;
            }
            if (sBackCod[i].process(inputs[CA::SCATTER_BACK_COD_START + i].getVoltage(), 0.1f, 1.f)) {
                latchRow(r, CA::V_SCATTER, sd, ty, false); pendingRows[r].scatterDelta = -1;
            }
          }
    }

    void resetToIdentity() {
        for (int v = 0; v < CA::N_VOICES; ++v) { rhythmSrc[v] = v; melodySrc[v] = v; }
    }


    json_t* dataToJson() override {
        json_t* root = json_object();
        auto save = [&](const char* k, const uint8_t* a) {
            json_t* arr = json_array();
            for (int v = 0; v < CA::N_VOICES; ++v) json_array_append_new(arr, json_integer(a[v]));
            json_object_set_new(root, k, arr);
        };
        save("rhythmSrc", rhythmSrc);
        save("melodySrc", melodySrc);
        return root;
    }

    void dataFromJson(json_t* root) override {
        resetToIdentity();
        auto load = [&](const char* k, uint8_t* a) {
            json_t* arr = json_object_get(root, k);
            if (!arr) return;
            for (int v = 0; v < CA::N_VOICES && v < (int)json_array_size(arr); ++v) {
                json_t* val = json_array_get(arr, v);
                if (json_is_integer(val))
                    a[v] = (uint8_t)math::clamp((int)json_integer_value(val), 0, CA::N_VOICES-1);
            }
        };
        load("rhythmSrc", rhythmSrc);
        load("melodySrc", melodySrc);
    }

    void onReset(const ResetEvent& e) override { resetToIdentity(); Module::onReset(e); }
};

// ── Widget ───────────────────────────────────────────────────────────────────
struct MonsoonChangeAlleyV2Widget : ModuleWidget {

    // Geometry -- MUST MATCH gen_change_alley_v2.py (48HP: V1-size grid, generous controls)
    static constexpr float PW_MM   = 48.f * 5.08f;
    static constexpr float PH_MM   = 128.5f;
    static constexpr float MARGIN  = 6.0f;
    static constexpr float J_DOM   = MARGIN +  0.0f;
    static constexpr float J_COD   = MARGIN +  9.5f;
    static constexpr float KNOB1   = MARGIN + 18.5f;   // grain
    static constexpr float KNOB2   = MARGIN + 27.0f;   // leader/step/scatter dom-back
    static constexpr float J_BACK2 = MARGIN + 34.5f;   // scatter cod-back
    static constexpr float BTN_D   = MARGIN + 42.5f;
    static constexpr float BTN_C   = MARGIN + 48.5f;
    static constexpr float LIGHT_X = MARGIN + 54.0f;
    static constexpr float CTRL_W  = LIGHT_X + 2.5f;   // 62.5
    static constexpr float GUTTER  = 9.6f;
    static constexpr float MX_MM   = CTRL_W + GUTTER;
    static constexpr float MW_MM   = PW_MM - 2.f * (CTRL_W + GUTTER);
    static constexpr float CELL_W  = MW_MM / CA::N_VOICES;
    static constexpr float CELL_H  = CELL_W;
    static constexpr float MY_MM   = 20.0f;
    static constexpr float MH_MM   = CELL_H * CA::N_VOICES;
    static constexpr float CTRL_ROW_H = 9.0f;
    static constexpr float GROUP_GAP  = 6.8f;
    static constexpr float CTRL_TOP   = 21.0f;
    static float rowY(int verb, int sub) {
        return CTRL_TOP + verb*(2.f*CTRL_ROW_H + GROUP_GAP) + sub*CTRL_ROW_H + CTRL_ROW_H*0.5f;
    }
    static float lx(float x_mm, bool flip) { return flip ? PW_MM - x_mm : x_mm; }

    static Vec cellCentre(int row, int col) {
        return mm2px(Vec(MX_MM + col * CELL_W + CELL_W * 0.5f,
                         MY_MM + row * CELL_H + CELL_H * 0.5f));
    }

    static float cellRadius() {          // BOTH pin types draw at this size (same, bigger)
        return mm2px(Vec(std::min(CELL_W, CELL_H) * 0.32f, 0)).x;
    }
    static float innerRadius() {         // concentric red-on-white inner: proportionally bigger
        return mm2px(Vec(std::min(CELL_W, CELL_H) * 0.32f * 0.55f, 0)).x;
    }
    static bool hitCell(Vec pos, int row, int col) {
        Vec c = cellCentre(row, col);
        float r = mm2px(Vec(std::min(CELL_W, CELL_H) * 0.5f, 0)).x;
        Vec d = pos - c;
        return d.x*d.x + d.y*d.y < r*r;
    }

    // Theme follows the CONNECTED MONSOON's lightTheme flag — the plugin-wide convention
    // (Raffles/Shophouse do the same). It is NOT Rack's global settings::preferDarkPanels;
    // using that was why Change Alley ignored Monsoon's dark setting.
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;

    MonsoonChangeAlleyV2Widget(MonsoonChangeAlleyV2* module) {
        setModule(module);
        std::string dark  = asset::plugin(pluginInstance, "res/panels/ChangeAlleyV2_panel_dark.svg");
        std::string light = asset::plugin(pluginInstance, "res/panels/ChangeAlleyV2_panel_light.svg");
        panelSvgDark  = APP->window->loadSvg(dark);
        panelSvgLight = APP->window->loadSvg(light);
        setPanel(Svg::load(dark));
        // Screws inset to the rails: RACK_GRID_WIDTH is 5.08mm, and Rack's own convention
        // is half a hole from the edges. 1.5mm put them partly off the panel edge.
        // Screws on the rails. RACK_GRID_HEIGHT is 128.5mm; a screw is ~5.5mm across, so the
        // bottom pair must sit ~5mm above the edge to stay on-panel (2.5mm clipped it).
        addChild(createWidget<ScrewSilver>(mm2px(Vec(7.5,          5.0))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 7.5,  5.0))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(7.5,          PH_MM - 5.0))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 7.5,  PH_MM - 5.0))));

        // Mod arc factory: overlay a red arc on a knob showing where poly CV pushes it.
        // getSetNorm = knob's own value; getModNorm = resolved knob+CV; gated on the
        // Monsoon menu flag modVizChangeAlley (matches every other surface).
        auto* mod = module;   // capture the ctor param explicitly (member `this->module`
                              //   is set by setModule but the local shadows it here)
        auto addArc = [&, mod](rack::app::Knob* knob, int paramId,
                          std::function<float()> resolved) {
            auto* arc = new redDot::ModArcOverlay();
            arc->radius = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->getSetNorm = [mod, paramId]() -> float {
                if (!mod) return 0.f;
                auto* pq = mod->paramQuantities[paramId];
                return pq ? (float)pq->getScaledValue() : 0.f;
            };
            arc->getModNorm = resolved;
            arc->isActive   = [mod]() -> bool {
                Monsoon* mm = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                return mm ? mm->modVizChangeAlley : false;
            };
            arc->attachOverKnob(knob, 1.5f);
            addChild(arc);
        };

        // Transform controls: intra (left) and inter (right), mirrored, jacks outside.
        for (int verb = 0; verb < CA::N_VERBS; ++verb)
          for (int sub = 0; sub < 2; ++sub) {
            const float y = rowY(verb, sub);
            for (int side = 0; side < 2; ++side) {
                const bool flip = (side == 1);
                const int r = CA::rowId(verb, side, sub);
                addInput(createInputCentered<PJ301MPort>(
                    mm2px(Vec(lx(J_DOM, flip), y)), module, CA::DOMAIN_TRIG_START + r));
                addInput(createInputCentered<PJ301MPort>(
                    mm2px(Vec(lx(J_COD, flip), y)), module, CA::CODOMAIN_TRIG_START + r));
                {   auto* k = createParamCentered<Trimpot>(
                        mm2px(Vec(lx(KNOB1, flip), y)), module, CA::GRAIN_START + r);
                    if (k->getParamQuantity()) k->getParamQuantity()->snapEnabled = true;
                    addParam(k);
                    const int gr = r;
                    addArc(k, CA::GRAIN_START + r, [mod, gr]() -> float {
                        if (!mod) return 0.f;
                        float v = mod->params[CA::GRAIN_START + gr].getValue()
                                + MonsoonChangeAlleyV2::polyCV(mod->inputs[CA::GRAIN_POLY_IN], gr) * 0.4f;
                        return rack::math::clamp(v / 4.f, 0.f, 1.f);   // 0..4 detents -> 0..1
                    }); }
                if (verb == CA::V_COLLAPSE) {
                    const int li = side*CA::TYPES + sub;      // STEP poly ch 1..4
                    auto* k = createParamCentered<Trimpot>(mm2px(Vec(lx(KNOB2, flip), y)),
                        module, CA::LEADER_START + li);
                    if (k->getParamQuantity()) k->getParamQuantity()->snapEnabled = true;
                    addParam(k);
                    addArc(k, CA::LEADER_START + li, [mod, li]() -> float {
                        if (!mod) return 0.f;
                        float v = mod->params[CA::LEADER_START + li].getValue()
                                + MonsoonChangeAlleyV2::polyCV(mod->inputs[CA::STEP_POLY_IN], li);
                        return rack::math::clamp(v / 15.f, 0.f, 1.f);   // leader 0..15
                    });
                } else if (verb == CA::V_ROTATE) {
                    const int sIdx = side*CA::TYPES + sub;
                    auto* k = createParamCentered<Trimpot>(mm2px(Vec(lx(KNOB2, flip), y)),
                        module, CA::STEP_START + sIdx);
                    if (k->getParamQuantity()) k->getParamQuantity()->snapEnabled = true;
                    addParam(k);
                    addArc(k, CA::STEP_START + sIdx, [mod, sIdx]() -> float {
                        if (!mod) return 0.f;
                        float v = mod->params[CA::STEP_START + sIdx].getValue()   // -7..7
                                + MonsoonChangeAlleyV2::polyCV(mod->inputs[CA::STEP_POLY_IN], 4 + sIdx);
                        return rack::math::clamp((v + 7.f) / 14.f, 0.f, 1.f);
                    });
                } else if (verb == CA::V_SCATTER) {
                    const int si = side*CA::TYPES + sub;
                    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lx(KNOB2, flip), y)),
                        module, CA::SCATTER_BACK_DOM_START + si));
                    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(lx(J_BACK2, flip), y)),
                        module, CA::SCATTER_BACK_COD_START + si));
                }
                addParam(createParamCentered<TL1105>(mm2px(Vec(lx(BTN_D, flip), y)),
                    module, CA::BTN_START + r*2));
                addParam(createParamCentered<TL1105>(mm2px(Vec(lx(BTN_C, flip), y)),
                    module, CA::BTN_START + r*2 + 1));
                addChild(createLightCentered<SmallLight<RedLight>>(
                    mm2px(Vec(lx(LIGHT_X, flip), y)), module, CA::PENDING_LIGHT_START + r));
            }
          }

        // Two poly modulation inputs, bottom-right under the last REFLECT row.
        {
            // MUST match gen_change_alley_v2.py: by = lastBottom()+9, rx = PW-MARGIN-4.45
            const float by = rowY(CA::N_VERBS - 1, 1) + CTRL_ROW_H * 0.5f + 9.0f;
            const float rx = PW_MM - MARGIN - 4.45f;
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(rx,         by)),
                     module, CA::STEP_POLY_IN));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(rx - 10.0f, by)),
                     module, CA::GRAIN_POLY_IN));
        }

        auto* ov = new PinOverlay(module);
        ov->box.pos  = Vec(0, 0);
        ov->box.size = box.size;
        addChild(ov);
    }

    // TransparentWidget, NOT Opaque: an opaque overlay sized to the module box consumed
    // every left-press, leaving nowhere to grab the panel for dragging (and blocked the
    // context menu outside the grid). Transparent passes everything through; we consume
    // ONLY genuine cell hits in onButton.
    struct PinOverlay : widget::TransparentWidget {
        MonsoonChangeAlleyV2* module;
        int hoverRow = -1, hoverCol = -1;   // XILS crosshair target (-1 = none)
        PinOverlay(MonsoonChangeAlleyV2* m) : module(m) {}

        int getPolyCount() const {
            if (!module) return 0;
            auto* mon = redDot::findMonsoonEitherSide(module);
            return mon ? mon->engine.numPolyVoices : 0;
        }

        // A pin that reads as a physical peg: soft drop shadow, flat colour body,
        // a rim a shade darker, and an offset specular highlight. col = body colour.
        static void drawPin(NVGcontext* vg, float cx, float cy, float r,
                            NVGcolor body, float alpha) {
            // drop shadow
            nvgBeginPath(vg); nvgCircle(vg, cx + r*0.12f, cy + r*0.16f, r);
            nvgFillColor(vg, nvgRGBAf(0,0,0,0.35f*alpha)); nvgFill(vg);
            // body
            nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
            NVGcolor b = body; b.a *= alpha;
            nvgFillColor(vg, b); nvgFill(vg);
            // rim (slightly darker ring)
            nvgBeginPath(vg); nvgCircle(vg, cx, cy, r);
            nvgStrokeColor(vg, nvgRGBAf(0,0,0,0.30f*alpha)); nvgStrokeWidth(vg, r*0.16f);
            nvgStroke(vg);
            // specular highlight, upper-left
            nvgBeginPath(vg); nvgCircle(vg, cx - r*0.30f, cy - r*0.32f, r*0.30f);
            nvgFillColor(vg, nvgRGBAf(1,1,1,0.55f*alpha)); nvgFill(vg);
        }

        void draw(const DrawArgs& args) override {
            NVGcontext* vg = args.vg;
            int poly = getPolyCount();
            float ro = cellRadius(), ri = innerRadius();

            // ── Labels: drawn HERE because nanosvg ignores SVG <text> (the brand rule
            //    "fonts outlined to paths" exists for panels; for a live widget nvgText
            //    is simpler and theme-aware). Drawn with module==nullptr too, so the
            //    browser preview shows a labelled panel. ──
            {
                std::shared_ptr<Font> font = APP->window->loadFont(
                    asset::system("res/fonts/ShareTechMono-Regular.ttf"));
                if (font) {
                    nvgFontFaceId(vg, font->handle);
                    // Ink follows WHERE the text sits, not just the theme:
                    //   • on the BODY (row/col numbers, transform labels) -> theme ink,
                    //     because the body is light in light theme, dark in dark theme.
                    //   • inside the GRID (tooltip) -> always light; the grid is dark in
                    //     BOTH themes and the tooltip has its own dark backing box.
                    // (The earlier 'only row 1 numbered' bug was dark ink on a dark body;
                    //  the fix is theme-correct ink, not permanently-light ink.)
                    // Same source as the panel swap: the connected Monsoon's flag.
                    Monsoon* themeM = module ? redDot::findMonsoonEitherSide(module) : nullptr;
                    const bool lightBody = themeM && themeM->lightTheme;
                    NVGcolor ink    = lightBody ? nvgRGB(0x2a,0x2a,0x2e) : nvgRGB(0xe8,0xe2,0xd0);
                    NVGcolor inkdim = lightBody ? nvgRGBA(0x88,0x8d,0x96,0xd0)
                                                : nvgRGBA(0x9a,0x95,0x88,0xb0);
                    NVGcolor amber  = lightBody ? nvgRGB(0xa0,0x78,0x08)   // kit light gold
                                                : nvgRGB(0xc8,0x90,0x0c);
                    char num[4];
                    // Voice-number labels only (currency codes dropped — tiny + noisy in-rack).
                    // ShareTechMono, 3.2mm — readable at 100% zoom.
                    for (int col = 0; col < CA::N_VOICES; ++col) {
                        Vec c = cellCentre(0, col);
                        float topY = mm2px(Vec(0, MY_MM)).y;
                        snprintf(num, sizeof(num), "%d", col + 1);
                        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                        nvgFontSize(vg, mm2px(Vec(3.2f,0)).x);
                        nvgFillColor(vg, col == 0 ? amber : ink);
                        nvgText(vg, c.x, topY - mm2px(Vec(0,1.6f)).y, num, NULL);
                    }
                    for (int row = 0; row < CA::N_VOICES; ++row) {
                        Vec c = cellCentre(row, 0);
                        float leftX = mm2px(Vec(MX_MM,0)).x;
                        snprintf(num, sizeof(num), "%d", row + 1);
                        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
                        nvgFontSize(vg, mm2px(Vec(3.2f,0)).x);
                        nvgFillColor(vg, row == 0 ? amber : ink);
                        nvgText(vg, leftX - mm2px(Vec(1.4f,0)).x, c.y, num, NULL);
                    }
                    // Verb labels BOTH sides: "COLLAPSE INTRA" left, "COLLAPSE INTER" right.
                    // Panel row order is Collapse, Rotate, Reflect, Scatter (matches V_*).
                    static constexpr const char* TN[4] = {"COLLAPSE","ROTATE","REFLECT","SCATTER"};
                    nvgFontSize(vg, mm2px(Vec(2.7f,0)).x);
                    nvgFillColor(vg, inkdim);
                    for (int t2 = 0; t2 < 4; ++t2) {
                        float gy = mm2px(Vec(0, rowY(t2, 0) - CTRL_ROW_H*0.5f - 1.4f)).y;
                        char lbl[24];
                        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
                        snprintf(lbl, sizeof(lbl), "%s INTRA", TN[t2]);
                        nvgText(vg, mm2px(Vec(MARGIN, 0)).x, gy, lbl, NULL);
                        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
                        snprintf(lbl, sizeof(lbl), "%s INTER", TN[t2]);
                        nvgText(vg, mm2px(Vec(PW_MM - MARGIN, 0)).x, gy, lbl, NULL);
                    }
                    // Bottom-right cluster: GRAIN/STEP jack captions + VERTICAL legend.
                    {
                        const float by = rowY(CA::N_VERBS - 1, 1) + CTRL_ROW_H*0.5f + 9.0f;
                        const float rx = PW_MM - MARGIN - 4.45f;
                        // captions ABOVE the jacks
                        nvgFontSize(vg, mm2px(Vec(2.3f,0)).x);
                        nvgFillColor(vg, inkdim);
                        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                        float capY = mm2px(Vec(0, by - 4.5f)).y;
                        nvgText(vg, mm2px(Vec(rx, 0)).x,          capY, "STEP",  NULL);
                        nvgText(vg, mm2px(Vec(rx - 10.0f, 0)).x,  capY, "GRAIN", NULL);
                        // VERTICAL legend, enlarged, to the LEFT of the jacks
                        nvgFontSize(vg, mm2px(Vec(2.8f,0)).x);
                        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                        const float lgX = mm2px(Vec(rx - 10.0f - 22.0f, 0)).x;
                        const float sw  = mm2px(Vec(1.3f,0)).x;
                        float r1Y = mm2px(Vec(0, by - 2.2f)).y;
                        float r2Y = mm2px(Vec(0, by + 2.2f)).y;
                        nvgBeginPath(vg); nvgCircle(vg, lgX, r1Y, sw);
                        nvgFillColor(vg, nvgRGBf(0.95f,0.95f,0.94f)); nvgFill(vg);
                        nvgFillColor(vg, inkdim);
                        nvgText(vg, lgX + mm2px(Vec(2.4f,0)).x, r1Y, "rhythm", NULL);
                        nvgBeginPath(vg); nvgCircle(vg, lgX, r2Y, sw);
                        nvgFillColor(vg, nvgRGBf(0.83f,0.f,0.10f)); nvgFill(vg);
                        nvgFillColor(vg, inkdim);
                        nvgText(vg, lgX + mm2px(Vec(2.4f,0)).x, r2Y, "melody", NULL);
                    }
                    // Title + legend
                    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                    nvgFontSize(vg, mm2px(Vec(3.6f,0)).x);
                    nvgFillColor(vg, ink);
                    nvgText(vg, box.size.x * 0.5f, mm2px(Vec(0,6.0f)).y, "CHANGE ALLEY", NULL);

                    // ── Connect indicator: a small state dot to the RIGHT of the SVG
                    //    logo (which draws the wordmark itself). BRIGHT red w/ halo =
                    //    connected + claimed; HOLLOW = not. No wordmark here — the panel
                    //    SVG embeds the real dot.modular logo. ──
                    {
                        bool connected = module && redDot::isConnectedAndClaimed(module);
                        // Connect dot beside the LHS logo (generator places logo at MARGIN,
                        // under the last REFLECT row).
                        float mx = mm2px(Vec(MARGIN + 36.0f, 0)).x;
                        float myv = mm2px(Vec(0, rowY(CA::N_VERBS-1,1) + CTRL_ROW_H*0.5f + 8.0f + 5.5f)).y;
                        if (connected) {
                            nvgBeginPath(vg); nvgCircle(vg, mx, myv, 3.6f);
                            nvgFillColor(vg, nvgRGBA(0xd4,0x00,0x1a,0x30)); nvgFill(vg);
                            nvgBeginPath(vg); nvgCircle(vg, mx, myv, 2.2f);
                            nvgFillColor(vg, nvgRGB(0xd4,0x00,0x1a)); nvgFill(vg);
                        } else {
                            nvgBeginPath(vg); nvgCircle(vg, mx, myv, 2.2f);
                            nvgStrokeColor(vg, nvgRGBA(0xd4,0x00,0x1a,0x70));
                            nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
                        }
                    }
                }
            }
            if (!module) return;

            // Pin colours are inlined below (white=rhythm, red=melody; identity pins at
            // 0.7 alpha, inactive rows at 0.4). Single literals, easy to tune.

            // ── Temasek pending: highlight affected submatrices ──────────────────────
            // Transforms are LOCAL to this module now, so the highlight reads pendingRows
            // directly -- no POD indirection, no header cycle to avoid (that machinery was
            // only for the two-module split).
            if (module) {
                const int active = std::max(1, poly + 1);
                for (int hr = 0; hr < CA::N_ROWS; ++hr) {
                    const auto& h = module->pendingRows[hr];
                    if (!h.armed) continue;
                    const int hType = hr % 2;
                    NVGcolor hcol = (hType == 0) ? nvgRGBAf(0.95f,0.95f,0.94f,0.55f)
                                                  : nvgRGBAf(0.83f,0.f,0.10f,0.55f);
                    const float sw = mm2px(Vec(0.45f,0)).x;
                    const float hw = mm2px(Vec(CELL_W * 0.5f, 0)).x;
                    const float hh = mm2px(Vec(0, CELL_H * 0.5f)).y;
                    // What a transform actually touches:
                    //   DOMAIN   ops partition the ROWS    -> horizontal bands
                    //   CODOMAIN ops partition the SOURCES -> vertical bands
                    // (An earlier version outlined diagonal squares, which is only correct
                    //  near identity: rotateValues takes its block from src[v], the COLUMN,
                    //  and collapseDomain hands row v the value tmp[leader], any column.)
                    // INTRA bands are `grain` wide; INTER bands are the whole pool split
                    // into blocks, drawn heavier because whole blocks move as units.
                    const int b    = std::max(1, h.grain);
                    const float lw = h.isInter ? sw * 1.6f : sw;
                    for (int base = 0; base < active; base += b) {
                        const int last = std::min(base + b, active) - 1;
                        if (last < base) continue;
                        Vec tl, br;
                        if (h.isDomain) {                    // rows: full-width band
                            tl = cellCentre(base, 0);
                            br = cellCentre(last, active - 1);
                        } else {                             // sources: full-height band
                            tl = cellCentre(0, base);
                            br = cellCentre(active - 1, last);
                        }
                        nvgBeginPath(vg);
                        nvgRect(vg, tl.x - hw, tl.y - hh,
                                (br.x + hw) - (tl.x - hw), (br.y + hh) - (tl.y - hh));
                        nvgStrokeColor(vg, hcol); nvgStrokeWidth(vg, lw); nvgStroke(vg);
                    }
                }
            }

            for (int row = 0; row < CA::N_VOICES; ++row) {
                bool active = (row == 0) || (row <= poly);  // row 0=mono always active
                float alpha = active ? 1.f : 0.4f;
                uint8_t rSrc = module->rhythmSrc[row];
                uint8_t mSrc = module->melodySrc[row];

                for (int col = 0; col < CA::N_VOICES; ++col) {
                    Vec c = cellCentre(row, col);
                    bool hasR = (rSrc == (uint8_t)col);
                    bool hasM = (mSrc == (uint8_t)col);
                    bool rIdentity = hasR && (col == row);
                    bool mIdentity = hasM && (col == row);

                    NVGcolor white = nvgRGBf(0.95f,0.95f,0.94f);
                    NVGcolor red   = nvgRGBf(0.83f,0.f,0.10f);
                    if (hasR && hasM) {
                        // Concentric: white peg with a red inset dot on top
                        drawPin(vg, c.x, c.y, ro, white, rIdentity ? 0.72f*alpha : alpha);
                        NVGcolor ic = red; ic.a = (mIdentity ? 0.72f : 1.f) * alpha;
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ri);
                        nvgFillColor(vg, ic); nvgFill(vg);
                    } else if (hasR) {
                        drawPin(vg, c.x, c.y, ro, white, rIdentity ? 0.72f*alpha : alpha);
                    } else if (hasM) {
                        drawPin(vg, c.x, c.y, ro, red, mIdentity ? 0.72f*alpha : alpha);   // same size as rhythm
                    } else {
                        // Empty — very faint ghost
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ro * 0.55f);
                        nvgStrokeColor(vg, nvgRGBAf(0.5f,0.5f,0.5f,0.12f*alpha));
                        nvgStrokeWidth(vg, 0.5f); nvgStroke(vg);
                    }
                }

                // Poly activity bar on right edge
                if (row > 0) {
                    float rx  = mm2px(Vec(MX_MM + MW_MM + 0.8f, 0)).x;
                    float cy  = cellCentre(row, 0).y;
                    float bh  = mm2px(Vec(0, CELL_H * 0.55f)).y;
                    NVGcolor bc = active ? nvgRGBA(0xd4,0x00,0x1a,0xa0) : nvgRGBA(0x30,0x30,0x30,0x60);
                    nvgBeginPath(vg);
                    nvgRect(vg, rx, cy - bh*0.5f, mm2px(Vec(1.2f,0)).x, bh);
                    nvgFillColor(vg, bc); nvgFill(vg);
                }
            }

            // ── XILS-style targeting crosshair + readout on hover ─────────────
            if (hoverRow >= 0 && hoverCol >= 0) {
                Vec c = cellCentre(hoverRow, hoverCol);
                float gx0 = mm2px(Vec(MX_MM, 0)).x, gx1 = mm2px(Vec(MX_MM + MW_MM, 0)).x;
                float gy0 = mm2px(Vec(0, MY_MM)).y, gy1 = mm2px(Vec(0, MY_MM + MH_MM)).y;
                (void)gx1; (void)gy1;
                nvgStrokeColor(vg, nvgRGBAf(1,1,1,0.55f));
                nvgStrokeWidth(vg, 0.8f);
                // XILS-style: guides run from the AXES to the cell only (not full-span)
                nvgBeginPath(vg); nvgMoveTo(vg, gx0, c.y); nvgLineTo(vg, c.x - ro*1.4f, c.y); nvgStroke(vg);
                nvgBeginPath(vg); nvgMoveTo(vg, c.x, gy0); nvgLineTo(vg, c.x, c.y - ro*1.4f); nvgStroke(vg);
                // hovered-cell ring
                nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ro * 1.15f);
                nvgStrokeColor(vg, nvgRGBAf(1,1,1,0.8f)); nvgStrokeWidth(vg, 0.8f); nvgStroke(vg);
                // readout "row->col" near the cursor (top-left of grid)
                std::shared_ptr<Font> font = APP->window->loadFont(
                    asset::system("res/fonts/ShareTechMono-Regular.ttf"));
                if (font) {
                    // Typed readout: states RHYTHM or MELODY (the gesture that would fire)
                    // per current mouse expectation: plain hover previews rhythm; the melody
                    // half is stated so the mapping reads even before clicking. Both shown.
                    char buf[64];
                    uint8_t rs = module->rhythmSrc[hoverRow], ms = module->melodySrc[hoverRow];
                    snprintf(buf, sizeof(buf), "v%d  rhythm<-v%d  melody<-v%d",
                             hoverRow + 1, rs + 1, ms + 1);
                    nvgFontFaceId(vg, font->handle);
                    nvgFontSize(vg, mm2px(Vec(3.4f,0)).x);          // was 2.6 — readable now
                    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                    float bounds[4];
                    nvgTextBounds(vg, 0, 0, buf, NULL, bounds);
                    float tw = bounds[2] - bounds[0] + mm2px(Vec(2.f,0)).x;
                    float th = mm2px(Vec(4.6f,0)).x;
                    // near the cursor cell, clamped inside the grid
                    float tx = c.x + ro*1.8f, ty = c.y - th*0.5f;
                    if (tx + tw > gx1) tx = c.x - ro*1.8f - tw;
                    if (ty < gy0) ty = gy0;
                    if (ty + th > gy1) ty = gy1 - th;
                    nvgBeginPath(vg); nvgRect(vg, tx, ty, tw, th);
                    nvgFillColor(vg, nvgRGBAf(0,0,0,0.8f)); nvgFill(vg);
                    nvgFillColor(vg, nvgRGBf(0.95f,0.95f,0.94f));
                    nvgText(vg, tx + mm2px(Vec(1.f,0)).x, ty + th*0.5f, buf, NULL);
                }
            }
        }

        // Track hovered cell for the crosshair; keep events passing through.
        void onHover(const event::Hover& e) override {
            hoverRow = hoverCol = -1;
            for (int row = 0; row < CA::N_VOICES && hoverRow < 0; ++row)
                for (int col = 0; col < CA::N_VOICES; ++col)
                    if (hitCell(e.pos, row, col)) { hoverRow = row; hoverCol = col; break; }
            TransparentWidget::onHover(e);
        }
        void onLeave(const event::Leave& e) override {
            hoverRow = hoverCol = -1;
            TransparentWidget::onLeave(e);
        }

        // Row-radio click: left=rhythm, right/ctrl=melody
        // Clicking cell (row, col) sets rhythmSrc[row]=col or melodySrc[row]=col.
        // Row-radio is automatic: src[row] holds exactly one value — this overwrites it.
        void onButton(const event::Button& e) override {
            if (!module || e.action != GLFW_PRESS) { TransparentWidget::onButton(e); return; }
            bool setMelody = (e.button == GLFW_MOUSE_BUTTON_RIGHT) || (e.mods & RACK_MOD_CTRL);
            for (int row = 0; row < CA::N_VOICES; ++row) {
                for (int col = 0; col < CA::N_VOICES; ++col) {
                    if (!hitCell(e.pos, row, col)) continue;
                    // Store-backed + undoable: the pin tables are NOT params (zero DAW
                    // slots -- DAW_PARAM_AUDIT), so undo goes through StoreEditAction.
                    // The action targets the EXPANDER's module id and bakes (row, which
                    // table) into the setter, so undo lands on the row actually edited
                    // no matter what has happened since. Equal old/new never records.
                    {
                        float oldV = setMelody ? (float)module->melodySrc[row]
                                               : (float)module->rhythmSrc[row];
                        redDot::applyAndPushStoreEdit<MonsoonChangeAlleyV2>(
                            module,
                            setMelody ? "move melody pin" : "move rhythm pin",
                            [row, setMelody](MonsoonChangeAlleyV2& m, float v) {
                                uint8_t c = (uint8_t)math::clamp((int)std::lround(v), 0, CA::N_VOICES - 1);
                                (setMelody ? m.melodySrc : m.rhythmSrc)[row] = c;
                            },
                            oldV, (float)col);
                    }
                    e.consume(this);
                    return;
                }
            }
            TransparentWidget::onButton(e);
        }

    };

    // Reset = up to 32 cell changes; one gesture must be ONE undo step, so it gets a
    // whole-table snapshot action rather than 32 StoreEditActions. Same module-id
    // resolution discipline as StoreEditAction (survives deletion; no-ops while gone).
    struct ResetPinsAction : rack::history::Action {
        int64_t moduleId;
        uint8_t oldR[CA::N_VOICES], oldM[CA::N_VOICES];
        ResetPinsAction(MonsoonChangeAlleyV2* m) : moduleId(m->id) {
            name = "reset pins to identity";
            for (int v = 0; v < CA::N_VOICES; ++v) { oldR[v] = m->rhythmSrc[v]; oldM[v] = m->melodySrc[v]; }
        }
        MonsoonChangeAlleyV2* resolve() {
            return dynamic_cast<MonsoonChangeAlleyV2*>(APP->engine->getModule(moduleId));
        }
        void undo() override {
            if (auto* m = resolve())
                for (int v = 0; v < CA::N_VOICES; ++v) { m->rhythmSrc[v] = oldR[v]; m->melodySrc[v] = oldM[v]; }
        }
        void redo() override {
            if (auto* m = resolve()) m->resetToIdentity();
        }
    };

    // One committed phrase-boundary transform batch = one undo step. Snapshots produced on the
    // audio thread (module->applyPendingTransforms) are drained here (UI thread) into these
    // actions. Same module-id resolution discipline as ResetPinsAction (survives deletion).
    struct TransformUndoAction : rack::history::Action {
        int64_t  moduleId;
        uint8_t  beforeR[CA::N_VOICES], beforeM[CA::N_VOICES];
        uint8_t  afterR[CA::N_VOICES],  afterM[CA::N_VOICES];
        uint64_t counterBefore[CA::SIDES * CA::TYPES * 2];
        uint64_t counterAfter [CA::SIDES * CA::TYPES * 2];
        TransformUndoAction() { name = "Change Alley transform"; }
        MonsoonChangeAlleyV2* resolve() {
            return dynamic_cast<MonsoonChangeAlleyV2*>(APP->engine->getModule(moduleId));
        }
        void undo() override {
            if (auto* m = resolve()) {
                for (int v = 0; v < CA::N_VOICES; ++v) { m->rhythmSrc[v] = beforeR[v]; m->melodySrc[v] = beforeM[v]; }
                for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) m->scatterCounter[i] = counterBefore[i];
            }
        }
        void redo() override {
            if (auto* m = resolve()) {
                for (int v = 0; v < CA::N_VOICES; ++v) { m->rhythmSrc[v] = afterR[v]; m->melodySrc[v] = afterM[v]; }
                for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) m->scatterCounter[i] = counterAfter[i];
            }
        }
    };

    void step() override {
        ModuleWidget::step();
        if (!module) return;

        // Drain the transform-undo ring produced on the audio thread. Each snapshot becomes one
        // Rack history action (UI-thread push, which is required). SPSC: we are the sole consumer.
        if (auto* ca = dynamic_cast<MonsoonChangeAlleyV2*>(module)) {
            uint32_t t = ca->undoTail.load(std::memory_order_relaxed);
            uint32_t h = ca->undoHead.load(std::memory_order_acquire);
            while (t != h) {
                const auto& snap = ca->undoRing[t % MonsoonChangeAlleyV2::UNDO_RING];
                auto* act = new TransformUndoAction();
                act->moduleId = ca->id;
                for (int v = 0; v < CA::N_VOICES; ++v) {
                    act->beforeR[v] = snap.beforeR[v]; act->beforeM[v] = snap.beforeM[v];
                    act->afterR[v]  = snap.afterR[v];  act->afterM[v]  = snap.afterM[v];
                }
                for (int i = 0; i < CA::SIDES * CA::TYPES * 2; ++i) {
                    act->counterBefore[i] = snap.counterBefore[i];
                    act->counterAfter[i]  = snap.counterAfter[i];
                }
                APP->history->push(act);
                ++t;
            }
            ca->undoTail.store(t, std::memory_order_release);
        }

        Monsoon* m = redDot::findMonsoonEitherSide(module);
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

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* module = dynamic_cast<MonsoonChangeAlleyV2*>(this->module);
        if (!module) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reset to identity diagonal", "",
            [module]() {
                // Skip the no-op (already identity) so undo history stays clean.
                bool isIdentity = true;
                for (int v = 0; v < CA::N_VOICES; ++v)
                    if (module->rhythmSrc[v] != v || module->melodySrc[v] != v) { isIdentity = false; break; }
                if (isIdentity) return;
                auto* act = new ResetPinsAction(module);
                module->resetToIdentity();
                APP->history->push(act);
            }));
    }
};
