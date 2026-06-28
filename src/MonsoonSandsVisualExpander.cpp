#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
//#include "MonsoonSandsExpander.hpp"
#include "MonsoonSandsVisualExpander.hpp"
#include "StraitsSandsMacroVisual.hpp"  // complete type for macroBase (P4 delegated-lane tracking)
#include "StraitsEastSandsVisual.hpp"   // East type + StraitsEastVisualIds::cvId (Mono spread-arc East-CV gate)
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/OwnerCell.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "dsp/managers/MonoSandsParameterManager.hpp"
#include "dsp/managers/SpreadManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace SandsMonoVisualIds;

extern Model* modelMonsoon;
extern Model* modelMonsoonSandsExpander;

// ── Widget ────────────────────────────────────────────────────────────────────
struct MonsoonSandsVisualExpanderWidget : ModuleWidget {
    SandsVisualEditorV4*       visualEditor = nullptr;
    MonoSandsParameterManager* paramMgr     = nullptr;
    bool                       initialized  = false;
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;
    int dbgLockState[4] = { -1, -1, -1, -1 };  // DEBUG PROBE: last logged lock-state per lane (remove after diagnosis)

    // Spread mod-arcs (bipolar -1..1), same as Macro: set = SPR param,
    // effective = mod->spreadEffective[spreadIdx] (CV-modulated). Normalised (v+1)/2.
    // NOTE: the stored int is the SPREAD index (engine order REST=0,MEL=1,OCT=2,ACC=3),
    // matching how spreadEffective[] is written and how sprCvId() is indexed — NOT the
    // editor lane. This is the fix for the spread-arc off-by-one (arc was reading the
    // engine-indexed array by editor lane).
    std::vector<std::pair<rack::ParamWidget*, int>> pendingSpreadArcs;
    void flushSpreadArcs(MonsoonSandsVisualExpander* mod) {
        for (auto& pr : pendingSpreadArcs) {
            auto* knob = pr.first; int sprIdx = pr.second;
            if (!knob) continue;
            auto* arc = new redDot::ModArcOverlay();
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->attachOverKnob(knob, mm2px(2.5f));
            MonsoonSandsVisualExpander* mm = mod;
            int pid = knob->paramId;
            arc->getSetNorm = [mm, pid]() -> float {
                if (!mm) return 0.5f;
                auto* pq = mm->paramQuantities[pid];
                return pq ? (float)pq->getScaledValue() : 0.5f;
            };
            // spreadEffective[] is spread/engine-indexed (REST=0,MEL=1,OCT=2,ACC=3).
            arc->getModNorm = [mm, sprIdx]() -> float {
                if (!mm || sprIdx < 0 || sprIdx >= 4) return 0.5f;
                return rack::math::clamp((mm->spreadEffective[sprIdx] + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->isActive = [mm, sprIdx]() -> bool {
                if (!mm || sprIdx < 0 || sprIdx >= 4) return false;
                Monsoon* mon = findMonsoonEitherSide(mm);
                if (!mon || !mon->modVizMono) return false;
                // Active when THIS spread lane is being modulated from any source feeding
                // the mono strand: Mono's own spread CV jack, OR East's V1 spread CV jack
                // (East adds to V1 spread — its mod must show on Mono's arc too, as the
                // total). sprCvId is spread/engine-indexed (same as sprIdx); East's V1
                // spread CV jack is East::cvId(sprIdx,3) read at the mono slot.
                if (mm->inputs[sprCvId(sprIdx)].isConnected()) return true;
                auto* east = mon->expanderManager.cachedEastSandsVisual;
                if (east && east->inputs[StraitsEastVisualIds::cvId(sprIdx,3)].isConnected())
                    return true;
                // Delegated to Macro, OR Mono-owned receiving Macro's send: the helper
                // covers both (delegated → Macro spread CV live; owned → non-zero mono-slot
                // send + CV live). Mono send slot = 0 (V1's slice). engine lane = sprIdx.
                auto* macro = mon->expanderManager.cachedMacroSandsVisual;
                bool delegated = macro && monoMacroOwnsEngineLane(mm, sprIdx);  // engine→editor baked in
                if (StraitsMacroVisualIds::macroSpreadModulatesLane(macro, sprIdx, delegated, /*sendSlot=*/0))
                    return true;
                return false;
            };
            addChild(arc);
        }
        pendingSpreadArcs.clear();
    }

    explicit MonsoonSandsVisualExpanderWidget(MonsoonSandsVisualExpander* mod) {
        setModule(mod);
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/SandsMonoVisual_40HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/SandsMonoVisual_40HP_light.svg"));
        panelWidget = createPanel(asset::plugin(pluginInstance,
                            "res/panels/SandsMonoVisual_40HP.svg"));
        setPanel(panelWidget);

        redDot::addRedScrews(this);

        // ── Visual editor: right section, 6 lanes MONO ────────────────────
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::MONO);
        // Editor lane band == the left-control band (ROW_TOP..ROW_BOT) so the live
        // lanes line up with the left jacks/attens. With zero internal padding the
        // editor divides this box evenly by laneCount, exactly matching rowY().
        visualEditor->box.pos  = mm2px(Vec(ED_X, ROW_TOP));
        visualEditor->box.size = mm2px(Vec(ED_W, ROW_BOT - ROW_TOP));
        // Lanes fill the box evenly (no padding) so the live 6 lanes align with
        // the painted lanes + the left jacks/attens (which divide the band evenly).
        // MONO label suppressed (would land on lane 0); lane labels stay.
        visualEditor->layout.topPadding = 0.f;
        visualEditor->layout.botPadding = 0.f;
        visualEditor->showControlBar    = false;
        // P4 (G5): a Mono lane delegated to Macro is inoperable and tracks Macro.
        // Mono owns V1; only the 4 poly lanes (editor 0..3 = MEL/OCT/REST/ACC) are
        // delegable. ownerDispId is editor-ordered (poly lane). Delegated = value 0.
        visualEditor->laneLockedFn = [this](int editorLane) -> bool {
            if (editorLane < 0 || editorLane >= 4) return false;   // VAR/LEG mono-only
            auto* mon = getMonsoon();
            bool hasMacro = mon && mon->expanderManager.cachedMacroSandsVisual != nullptr;
            if (!hasMacro || !module) return false;
            float ownVal = module->params[ownerDispId(editorLane)].getValue();
            bool locked  = !(ownVal > 0.5f);  // Macro owns → locked
            // DEBUG PROBE (remove after diagnosis): log only when this lane's computed
            // state changes, so the log isn't flooded. Shows the real runtime gate values
            // for the Mono editor's lock. dbgLockState[] declared on the widget.
            int key = (hasMacro ? 4 : 0) | (locked ? 2 : 0) | (ownVal > 0.5f ? 1 : 0);
            if (dbgLockState[editorLane] != key) {
                dbgLockState[editorLane] = key;
                WARN("[MonoLock] lane=%d hasMacro=%d ownerDispId=%.3f -> locked=%d",
                     editorLane, (int)hasMacro, ownVal, (int)locked);
            }
            return locked;
        };
        addChild(visualEditor);

        // ── LOR controls: 3 CV jacks + 3 attens per lane (all 6 lanes) ────
        for (int lane = 0; lane < N_LANES; ++lane) {
            float y = rowY(lane);
            for (int p = 0; p < 3; ++p) {  // LEN/OFF/ROT
                addInput(createInputCentered<PJ301MPort>(
                    mm2px(Vec(JACK_X[p],  y)), mod, cvId(lane, p)));
                addParam(createParamCentered<Trimpot>(
                    mm2px(Vec(ATTEN_X[p], y)), mod, attenId(lane, p)));
            }
        }

        // ── Spread group: base trimpot + CV jack + atten ──────────────────
        // REST/MEL/OCT + ACCENT (the poly-derived lanes). Each spread control sits on its
        // editor-lane row (accent on row 4, skipping LEGATO at 3). LEG/VAR are mono-only.
        for (int l = 0; l < N_SPREAD_LANES; ++l) {
            int editorLane = SPREAD_LANE_TO_EDITOR[l];
            float y = rowY(editorLane);
            auto* sp = createParamCentered<Trimpot>(mm2px(Vec(SPR_BASE_X, y)), mod, sprId(l));
            addParam(sp);
            // Store the SPREAD index l (engine order REST/MEL/OCT/ACC), NOT the editor
            // lane: spreadEffective[] and sprCvId() are both spread/engine-indexed, so
            // the arc must look them up by l. (The knob sits on the editor row via y.)
            pendingSpreadArcs.push_back({sp, l});
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(SPR_CV_X, y)), mod, sprCvId(l)));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(SPR_ATTEN_X, y)), mod, sprAttenId(l)));
        }

        paramMgr = new MonoSandsParameterManager();
        flushSpreadArcs(mod);

        // ── V1 ownership cells (Option C / treatment A, shared OwnerCell) ──────
        // One per poly lane (editor rows 0..3 = MEL/OCT/REST/ACC), in the SRC
        // column right of the editor. FILLED = Macro owns V1's base for this lane,
        // OUTLINE = Mono owns (its own LOR edit). Click toggles. Inert+dimmed when
        // no Macro is attached (nothing to cede to). LEG/VAR (rows 4/5) are
        // mono-only → no owner cell.
        for (int l = 0; l < 4; ++l) {
            auto* oc = createParamCentered<OwnerCell>(
                mm2px(Vec(OWNER_X, rowY(l))), mod, ownerDispId(l));
            oc->laneCol = sandsLaneColorEditor(l);
            // Match the editor's lane-step cell: one step wide × ~90% lane tall.
            const float stepW   = (ED_W - 2.f*6.f) / 16.f;          // editor padding=6, 16 steps
            const float monoLaneH = (ROW_BOT - ROW_TOP) / N_LANES;  // 6 lanes
            oc->box.size = mm2px(Vec(stepW, monoLaneH * 0.9f));
            oc->box.pos  = mm2px(Vec(OWNER_X, rowY(l))).minus(oc->box.size.div(2.f));
            oc->lockWhen = [this]() {   // condition 2: no Macro → can't delegate
                auto* mon = getMonsoon();
                return !(mon && mon->expanderManager.cachedMacroSandsVisual != nullptr);
            };
            addParam(oc);
        }

        // Per-lane probability CV outs — one jack right of each of the 6 lane rows.
        for (int l = 0; l < N_LANES; ++l) {
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(PROB_OUT_X, rowY(l))), mod, PROB_OUT_START + l));
        }

        // dot.modular connect mark (brand mark; greyed when no Monsoon attached).
        {
            connectMark = redDot::makeConnectMark(module, mm2px(rack::math::Vec(W_MM * 0.5f, 124.f)), mm2px(8.f));
            addChild(connectMark);
        }
    }

    ~MonsoonSandsVisualExpanderWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }

        int wantLight = monsoon->lightTheme ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            if (panelWidget) panelWidget->setBackground(wantLight ? panelSvgLight : panelSvgDark);
            if (visualEditor) visualEditor->setTheme(wantLight != 0);
        }

        auto* mod = static_cast<MonsoonSandsVisualExpander*>(module);

        PatternEngine* pe = &monsoon->engine.pe;
        if (paramMgr->patternEngine != pe)
            paramMgr->setPatternEngine(pe);   // updates both patternEngine + spreadMgr.patternEngine

        // ── One-time initialisation from saved params ─────────────────────
        if (!initialized) {
            for (int l = 0; l < 6; ++l) {
                // l = editor lane; Mono LOR params are now editor-ordered → direct.
                visualEditor->currentState.lanes[l].length   = (int)std::round(mod->params[lenId(l)].getValue());
                visualEditor->currentState.lanes[l].offset   = (int)std::round(mod->params[offId(l)].getValue());
                visualEditor->currentState.lanes[l].rotation = (int)std::round(mod->params[rotId(l)].getValue());
            }
            for (int l = 0; l < N_SPREAD_LANES; ++l)   // spread stays engine-ordered
                mod->spreadEffective[SPREAD_LANE_TO_EDITOR[l]] = mod->params[sprId(l)].getValue();
            initialized = true;
        }

        // ── Editor → params (UI thread, own params). Skip delegated lanes: when a
        //    poly lane is owned by Macro it's inoperable + must TRACK Macro, so we
        //    don't write Mono's param from the (locked) editor, and below we refresh
        //    its display from Macro's global base instead. (G5.)
        Monsoon* monForOwn = getMonsoon();
        auto* macroForOwn = monForOwn ? monForOwn->expanderManager.cachedMacroSandsVisual : nullptr;
        auto laneDelegated = [&](int el) -> bool {
            return macroForOwn && el < 4 && !(mod->params[ownerDispId(el)].getValue() > 0.5f);
        };
        for (int l = 0; l < 6; ++l) {
            if (laneDelegated(l)) continue;   // delegated → tracks Macro, don't write Mono param
            const auto& lane = visualEditor->currentState.lanes[l];
            mod->params[lenId(l)].setValue((float)lane.length);
            mod->params[offId(l)].setValue((float)lane.offset);
            mod->params[rotId(l)].setValue((float)lane.rotation);
        }
        // Delegated lanes: show Macro's CV-APPLIED global value (base + macroCVDelta),
        // so Macro's modulation is reflected on Mono too (combo 6/G6) — not just the
        // static base. macroCVDelta is the true POST delta (Macro's own display value);
        // matches what Macro shows for the same lane. editor → engine lane for macro arrays.
        if (macroForOwn) {
            for (int el = 0; el < 4; ++el) {
                if (!laneDelegated(el)) continue;
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
                int mLen = (int)std::round(macroForOwn->macroBase[eng][0] + macroForOwn->macroSendDelta[eng][0]);
                int mOff = (int)std::round(macroForOwn->macroBase[eng][1] + macroForOwn->macroSendDelta[eng][1]);
                int mRot = (int)std::round(macroForOwn->macroBase[eng][2] + macroForOwn->macroSendDelta[eng][2]);
                visualEditor->currentState.lanes[el].setDisplayLOR(std::max(1,mLen), mOff, mRot);
            }
        }

        // ── Per-lane base spread. spreadEffective[] is SPREAD/engine-indexed
        // (0=REST,1=MEL,2=OCT,3=ACC). setLaneSpread expects the PatternEngine BUFFER
        // lane order (REST=0,MEL=1,OCT=2,LEG=3,ACC=4,VAR=5). Map spread idx → buffer
        // lane explicitly so neither side is read with the wrong convention. (This was
        // the per-lane analogue of the spread-arc off-by-one.)
        static const int SPREAD_TO_BUFFER[4] = { 0, 1, 2, 4 };  // REST,MEL,OCT,ACCENT
        for (int l = 0; l < N_SPREAD_LANES; ++l) {
            paramMgr->setLaneSpread(SPREAD_TO_BUFFER[l], mod->spreadEffective[l]);
        }

        // (Spread target mode is pulled from the engine by the param manager —
        // Monsoon::process mirrors the menu setting onto engine.pe each frame. This is
        // what fixes the original bug at the source: the Mono widget used to hardcode
        // AVERAGE_POLY here and silently ignore voice-1 mode.)

        // ── Sync PatternEngine → display (uses SpreadManager.getInterpolatedValue) ──
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // ── Per-lane playhead + CV-applied display window ─────────────────
        // The editor lanes' EDIT L/O/R are the user's canvas values (written to
        // params above). For DISPLAY, surface the engine's CV-APPLIED effective
        // L/O/R so the highlighted window + offset/rotation markers track LOR CV
        // modulation. processDNA writes these per-lane into engine.{strand}Len/Off/
        // Rot. Editor lane order = REST,MEL,OCT,LEG,ACC,VAR. Display-only: editing
        // still uses the edit values (editStartBar/editEndBar). With no CV the
        // effective values equal the params, so the window is unchanged.
        // Editor lane index → engine strand via the single source of truth
        // (dotModular::MONO_LANE_TO_STRAND in dsp/LaneMapping.hpp) + the engine's
        // indexable strand accessors. No hardcoded permutation here, so this can
        // never drift from readStrand() again.
        auto& eng = monsoon->engine;
        int effLen[6], effOff[6], effRot[6];
        for (int l = 0; l < 6; ++l) {
            int strand = dotModular::MONO_LANE_TO_STRAND[l];
            effLen[l] = eng.strandLen(strand);
            effOff[l] = eng.strandOff(strand);
            effRot[l] = eng.strandRot(strand);
        }
        int globalStep = monsoon->engine.stepIndex;
        visualEditor->setPlayDir(monsoon->engine.lastPlayDir);   // direction cue (Mode E reverse)
        for (int l = 0; l < 6; ++l) {
            visualEditor->currentState.lanes[l].setDisplayLOR(effLen[l], effOff[l], effRot[l]);
            visualEditor->setLanePlayStep(l,
                calcPlayhead(globalStep, effLen[l], effOff[l], effRot[l]));
        }
    }
};

// ── Module process(): emit per-lane probability CV outs (audio rate) ──────────
void MonsoonSandsVisualExpander::process(const ProcessArgs&) {
    using namespace SandsMonoVisualIds;
    Monsoon* monsoon = redDot::findMonsoonEitherSide(this);
    if (!monsoon) {
        for (int l = 0; l < 6; ++l) outputs[PROB_OUT_START + l].setVoltage(0.f);
        return;
    }
    auto& eng = monsoon->engine;
    const int globalStep = eng.stepIndex;
    const float scaleV = (monsoon->probOutScale == 0) ? 1.f : (monsoon->probOutScale == 1) ? 5.f : 10.f;
    const bool sh = monsoon->probOutSampleHold;
    for (int l = 0; l < 6; ++l) {
        int strand = dotModular::MONO_LANE_TO_STRAND[l];
        // Lane's post-LOR step — same mapping the visual uses for the playhead.
        int step = calcPlayhead(globalStep, eng.strandLen(strand),
                                eng.strandOff(strand), eng.strandRot(strand));
        float v;
        if (sh) {
            if (step != probLastStep[l]) {          // latch at the 16th step edge
                probHeld[l] = eng.pe.finalRandomByStrand(strand, step);
                probLastStep[l] = step;
            }
            v = probHeld[l];
        } else {
            v = eng.pe.finalRandomByStrand(strand, step);   // continuous surface
        }
        outputs[PROB_OUT_START + l].setVoltage(rack::math::clamp(v, 0.f, 1.f) * scaleV);
    }
}

Model* modelMonsoonSandsVisualExpander =
    createModel<MonsoonSandsVisualExpander, MonsoonSandsVisualExpanderWidget>(
        "MonsoonSandsVisualExpander");
