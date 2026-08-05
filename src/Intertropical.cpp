// Intertropical  scene sequencer implementation.
// See INTERTROPICAL_SPEC.md. Process = boundary-crossing advance + route active scene to poly
// outs. Widget = continuous-grid display, store-backed cells, widget-drawn live state.

#include "Intertropical.hpp"
#include "Monsoon.hpp"
#include "MonsoonStraitsExpander.hpp"   // complete type for cachedPolyVoiceExpander->outputs
#include "ui/IntertropicalPairing.hpp"  // assignPairId (shared pairing mechanism)
#include "ui/SvgPanelKit.hpp"
#include "ui/GoldPolyPort.hpp"
#include "ui/DimmableTrimpot.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/RedScrew.hpp"

using namespace rack;

// ============================ MODULE ============================

Intertropical::Intertropical() {
    for (int s = 0; s < Ids::N_SCENES; ++s)
        for (int v = 0; v < Ids::N_VOICES; ++v)
            sceneSlots[s][v] = -1;   // all auto-pack by default (voice -> slot)
    // Global slot -> output: default identity permutation (slot i drives output i, single bit).
    for (int s = 0; s < Ids::MAX_VOICES_PER_SCENE; ++s)
        slotOutput[s] = (uint8_t)(1u << s);
    for (int s = 0; s < Ids::MAX_VOICES_PER_SCENE; ++s)
        effectiveTranspose[s] = 0.f;
    config(Ids::NUM_PARAMS, Ids::NUM_INPUTS, Ids::NUM_OUTPUTS, Ids::NUM_LIGHTS);
    // 8 per-output transpose knobs: -24..+24 semitones, integer-detented (snap), default 0.
    for (int o = 0; o < 8; ++o) {
        configParam(Ids::TRANSPOSE_FIRST + o, -24.f, 24.f, 0.f,
                    rack::string::f("Output %d transpose", o + 1), " semitones");
        paramQuantities[Ids::TRANSPOSE_FIRST + o]->snapEnabled = true;   // detented per semitone
    }
    configInput(Ids::PHASE_IN, "Phase (optional; else reads host)");
    configOutput(Ids::GATE_OUT,   "Gate (poly, active scene, <=8ch)");
    configOutput(Ids::CV_OUT,     "CV (poly, active scene, <=8ch)");
    configOutput(Ids::ACCENT_OUT, "Accent (poly, active scene, <=8ch)");
    configOutput(Ids::LEGATO_OUT, "Legato (poly, active scene, <=8ch)");
    configOutput(Ids::SLEG_OUT,   "SLEG (poly, active scene, <=8ch)");
}

void Intertropical::process(const ProcessArgs& args) {
    // ── One-shot pair-id resolution (runs here, NOT onAdd) ───────────────────────
    // onAdd() executes while Rack holds the engine mutex during module insertion;
    // getModuleIds() re-locks it => deadlock/hang. process() is the safe place: assign
    // exactly once. Fresh/zero id => take the lowest unused. A pasted/duplicated module
    // carries its origin's id via JSON; if that id now clashes with another present
    // instance, re-assign so the copy gets its own number.
    if (!pairChecked) {
        pairChecked = true;
        bool clash = false;
        if (APP && APP->engine) {
            for (int64_t id : APP->engine->getModuleIds()) {
                rack::Module* m = APP->engine->getModule(id);
                if (!m || m == this) continue;
                if (auto* it = dynamic_cast<Intertropical*>(m))
                    if (it->pairId == pairId) { clash = true; break; }
            }
        }
        if (pairId <= 0 || clash) pairId = redDot::assignPairId(this);
    }

    Monsoon* host = redDot::findMonsoonEitherSide(this);
    if (!host) {
        for (int o = 0; o < Ids::NUM_OUTPUTS; ++o) outputs[o].setChannels(0);
        return;
    }
    auto& eng = host->engine;

    // ---- RESET: sync back to scene 1 repeat 1 when Monsoon resets ----
    // Read the RESET_TRIGGER_OUTPUT pulse jack directly from the host. This fires only on a
    // genuine reset (button or gate input), NOT on phrase wraps or DNA_LCM counter wraps.
    // Check every process() call (not just on step edges) so a mid-step reset isn't missed.
    if (host->outputs[MonsoonIds::RESET_TRIGGER_OUTPUT].getVoltage() >= 1.f) {
        activeScene = 0;
        repeatPos   = 0;
        liveMask    = sceneMask[0];
        lastStepIndex = -1;   // force re-sample on the next step
        justReset   = true;   // swallow the first post-reset wrap (reset makes engine report wrapped)
    }
    // Use the engine's own phrase-boundary detection: lastStepResult.wrapped is true on the
    // step where the pattern wraps (accounts for modulated start/endStep correctly).
    // We track the stepIndex to detect when a new step result is available.
    // ---- boundary-crossing advance (phrase-boundary, direction-agnostic) ----
    const int si = eng.stepIndex;
    if (lastStepIndex >= 0 && si != lastStepIndex) {
        // A new step was processed. Check if the engine reported a phrase boundary wrap.
        if (eng.lastStepResult.wrapped) {
            if (justReset) {
                // The reset itself makes the engine report wrapped=true on the first step after
                // reset. Swallow exactly this one wrap so the reset holds on scene 0 instead of
                // advancing 0 -> 1 (the off-by-one). Subsequent wraps advance normally.
                justReset = false;
            } else {
                repeatPos++;
                if (repeatPos >= getRepeats(activeScene)) {
                    repeatPos = 0;
                    activeScene = (activeScene + 1) % getLoopLen();
                }
                // Read-at-boundary rule: sample the active scene's membership NOW.
                liveMask = sceneMask[activeScene];
            }
        }
    }
    lastStepIndex = si;

    // ---- route: hybrid auto-pack + override routing (<=8 output channels) ----
    // ---- route: read the Straits expander's 16-channel poly outputs (the poly output source) ----
    // The Straits expander (cachedPolyVoiceExpander) has the 16-channel poly outputs computed by
    // the OutputGenerator (gs.process — authoritative gate voltage). The host Monsoon's own outputs
    // are mono-only (1 channel). Straits is required for poly mode.
    // computeRouting() maps each active voice to an output channel (0..7): forced overrides
    // first, then auto-pack. Output channel count = number of routed voices (<=8).
    // Routing model (SETTLED): voice -> slot (per scene) -> output MASK (global, fan-out capable).
    // A voice drives EVERY output bit set in its slot's slotOutput mask, each at that output's own
    // transpose. Output channel = the OUTPUT index (0..7), fixed 8-wide; fan-out writes one voice to
    // several channels. See INTERTROPICAL_SPEC "Routing model + fan-out".
    auto* straits = host->expanderManager.cachedPolyVoiceExpander;
    int8_t slotOf[Ids::N_VOICES];
    computeSlots(activeScene, slotOf);
    // Highest output channel actually driven (so we size the poly cable to what's used).
    int nOut = 0;
    for (int v = 0; v < Ids::N_VOICES; ++v) {
        if (slotOf[v] < 0) continue;
        const uint8_t m = slotOutput[slotOf[v]];
        for (int o = 0; o < Ids::MAX_VOICES_PER_SCENE; ++o) if ((m >> o) & 1u) nOut = std::max(nOut, o + 1);
    }
    for (int o = 0; o < Ids::NUM_OUTPUTS; ++o) outputs[o].setChannels(nOut);
    for (int o = 0; o < Ids::NUM_OUTPUTS; ++o)
        for (int ch = 0; ch < nOut; ++ch)
            outputs[o].setVoltage(0.f, ch);
    if (!straits) return;  // no Straits = no poly outputs = silence
    // Straits output IDs: POLY_GATE_OUT=0, POLY_STEP_GATE_OUT=1, POLY_STEP_LEGATO_GATE_OUTPUT=2,
    // POLY_CV_OUT=3, POLY_ACCENT_OUT=4 (from StraitsIds::OutputIds).
    for (int v = 0; v < Ids::N_VOICES; ++v) {
        const int slot = slotOf[v];
        if (slot < 0) continue;
        const uint8_t mask = slotOutput[slot];
        if (!mask) continue;
        const float gate = straits->outputs[0].getVoltage(v);
        const float cv   = straits->outputs[3].getVoltage(v);
        const float acc  = straits->outputs[4].getVoltage(v);
        const float leg  = straits->outputs[1].getVoltage(v);
        const float sleg = straits->outputs[2].getVoltage(v);
        // Tie vs legato for the transpose rule. Read the SAME articulation Lantern reads: global
        // voice v -> mono/V1 uses eng.gs, poly uses eng.voices[v-1].gs. lastNoteType is Single/Tie/
        // Legato. TRUE TIE = one sustained note, pitch must NOT move, so hold transpose from the
        // tie's onset. LEGATO = a connected note allowed to glide, so transpose reads LIVE (worst
        // case a legato with unchanged transpose just looks like a tie -- a valid note). Single =
        // fresh onset, live. Rule: hold while lastNoteType==Tie, else re-capture the live knob.
        const GateState& vgs = (v == 0) ? eng.gs : eng.voices[v - 1].gs;
        const bool tieHold = (vgs.lastNoteType == GateState::NoteType::Tie);
        for (int ch = 0; ch < Ids::MAX_VOICES_PER_SCENE; ++ch) {
            if (!((mask >> ch) & 1u)) continue;    // this slot doesn't drive output ch
            // Per-output TRANSPOSE (ch = OUTPUT channel). effectiveTranspose[ch] is what actually
            // reaches the CV jack; Lantern reads it so the piano-roll shows the sounding pitch.
            if (!tieHold) effectiveTranspose[ch] = params[Ids::TRANSPOSE_FIRST + ch].getValue();
            const float trSemi = effectiveTranspose[ch];
            outputs[Ids::GATE_OUT].setVoltage(gate, ch);
            outputs[Ids::CV_OUT].setVoltage(cv + trSemi / 12.f, ch);
            outputs[Ids::ACCENT_OUT].setVoltage(acc, ch);
            outputs[Ids::LEGATO_OUT].setVoltage(leg, ch);
            outputs[Ids::SLEG_OUT].setVoltage(sleg, ch);
        }
    }
}

// ============================ WIDGET ============================

// Panel geometry constants (mm). Panel is ~22HP (330px × 379.43px at 75 DPI = ~112mm × 128.5mm).
// Derived from the panel SVG's rect/circle coordinates (converted px→mm at 75/25.4).
// Repeat row rect: px(35.4, 47.2, 276.9, 26.6) → mm(12.0, 16.0, 93.7, 9.0)
// Main grid rect:  px(35.4, 82.7, 276.9, 240.7) → mm(12.0, 28.0, 93.7, 81.5)
// Jack wells (5):  px(63.1, 118.5, 173.9, 229.2, 284.6) × cy=346.9 → mm y=117.5
static constexpr float IT_GRID_X   = 12.0f;  // membership grid left (panel v5 MEM_L)
static constexpr float IT_GRID_Y   = 16.0f;  // grid top = 16mm (LANTERN-ALIGNED, panel v5)
static constexpr float IT_GRID_W   = 92.0f;  // grid width (panel MEM_W=92; 8 cols 11.5mm)
static constexpr float IT_GRID_H   = 96.0f;  // grid height 96mm (16 rows x 6.0mm = Lantern)
static constexpr float IT_REP_H    = 6.0f;   // repeat row height (panel v5 revised)
static constexpr float IT_REP_Y    = 7.5f;   // repeat row top: ABOVE the grid (GRID_TOP-REP_H-2.5)
// Transpose knobs + jacks now bound via panel MARKERS (param_0..7, output_0..4), not hardcoded
// coords -- see kit binding in the widget ctor. IT_JACK_Y kept only as a fallback reference.
static constexpr float IT_JACK_Y   = 99.0f;  // (panel v5 jy; markers are the source of truth)
static constexpr float IT_VS_TOP    = 20.0f;  // voice->slot grid top (panel VS_TOP)
static constexpr float IT_VS_ROWH   = 3.5f;   // voice->slot row height (mm)
static constexpr float IT_VS_H      = 8*IT_VS_ROWH; // 28mm total
static constexpr float IT_ROUT_TOP  = 52.0f;  // slot->output grid top (panel ROUT_TOP)
static constexpr float IT_ROUT_ROWH = 5.0f;   // slot->output row height (mm)
static constexpr float IT_ROUT_H    = 8*IT_ROUT_ROWH; // 40mm total
// Right block X/width (panel px 330.7 / 281.6 -> mm). Shared by both right-side grids.
static constexpr float IT_RIGHT_X   = 112.0f; // right block left (both sub-grids)
static constexpr float IT_RIGHT_W   = 95.37f; // right block width (8 cols = 11.92mm each)

// Continuous grid display: reads cell geometry from the panel constants and draws live state
// (membership fill via voiceColour, active-scene highlight, repeat count + progress, voice
// numbers) over the static screen. Cells are store-backed toggles.
struct IntertropicalGrid : Widget {
    Intertropical* module = nullptr;
    Rect gridBox;     // main 8x16 membership grid (px)
    Rect repBox;      // repeat row (px)
    Rect vsBox;       // voice->slot display grid (top-right, read-only)
    Rect routBox;     // slot->output routing grid (bottom-right, 8x8, interactive)

    // voiceColour: reuse Lantern's palette so voice identity is consistent across modules.
    static NVGcolor voiceColour(int v) {
        static const NVGcolor P[8] = {
            nvgRGB(0x6c,0x8c,0xd4), nvgRGB(0x26,0xa6,0x9a), nvgRGB(0xd4,0x8a,0x3c),
            nvgRGB(0xb0,0x6c,0xd4), nvgRGB(0x5c,0xb8,0x5c), nvgRGB(0xd4,0x6c,0x8c),
            nvgRGB(0x4c,0xb0,0xc8), nvgRGB(0xc8,0xb0,0x4c),
        };
        return P[((v % 8) + 8) % 8];
    }

    void onButton(const event::Button& e) override {
        if (!module || e.action != GLFW_PRESS) { Widget::onButton(e); return; }
        // Right-click on a filled cell: cycle output override (Auto -> 0 -> 1 -> ... -> 7 -> Auto).
        if (e.button == GLFW_MOUSE_BUTTON_RIGHT && gridBox.contains(e.pos)) {
            const float cw = gridBox.size.x / Intertropical::Ids::N_SCENES;
            const float ch = gridBox.size.y / Intertropical::Ids::N_VOICES;
            int scene = (int)((e.pos.x - gridBox.pos.x) / cw);
            int voice = (int)((e.pos.y - gridBox.pos.y) / ch);
            if (module->getCell(scene, voice)) module->cycleSlot(scene, voice);
            e.consume(this); return;
        }
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) { Widget::onButton(e); return; }
        // Left-click: toggle cell membership (store-backed).
        if (gridBox.contains(e.pos)) {
            const float cw = gridBox.size.x / Intertropical::Ids::N_SCENES;
            const float ch = gridBox.size.y / Intertropical::Ids::N_VOICES;
            int scene = (int)((e.pos.x - gridBox.pos.x) / cw);
            int voice = (int)((e.pos.y - gridBox.pos.y) / ch);
            module->setCell(scene, voice, !module->getCell(scene, voice));
            e.consume(this); return;
        }
        // Hit-test the repeat row: set repeats N by horizontal position within the scene cell.
        if (repBox.contains(e.pos)) {
            const float cw = repBox.size.x / Intertropical::Ids::N_SCENES;
            int scene = (int)((e.pos.x - repBox.pos.x) / cw);
            float frac = ((e.pos.x - repBox.pos.x) - scene*cw) / cw;
            int n = 1 + (int)(frac * Intertropical::Ids::MAX_REPEAT);
            module->setRepeats(scene, n);
            e.consume(this); return;
        }
        // Slot->output routing grid (8x8): left-click toggles whether slot (row) drives output (col).
        // A slot with >1 lit cell is fanned out. Global setup (not per-scene).
        if (routBox.contains(e.pos)) {
            const int MV = Intertropical::Ids::MAX_VOICES_PER_SCENE;   // 8
            const float cw = routBox.size.x / MV;
            const float ch = routBox.size.y / MV;
            int output = (int)((e.pos.x - routBox.pos.x) / cw);   // column = output
            int slot   = (int)((e.pos.y - routBox.pos.y) / ch);   // row = slot
            module->toggleSlotOutput(slot, output);
            e.consume(this); return;
        }
        Widget::onButton(e);
    }

    void draw(const DrawArgs& args) override {
        if (!module) return;
        NVGcontext* vg = args.vg;
        const int NS = Intertropical::Ids::N_SCENES;
        const int NV = Intertropical::Ids::N_VOICES;

        // --- grid lines (faint) ---
        const float cw = gridBox.size.x / NS;
        const float ch = gridBox.size.y / NV;
        nvgStrokeColor(vg, nvgRGBA(0x40,0x40,0x40,0x60));
        nvgStrokeWidth(vg, 0.5f);
        for (int s = 0; s <= NS; ++s) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, gridBox.pos.x + s*cw, gridBox.pos.y);
            nvgLineTo(vg, gridBox.pos.x + s*cw, gridBox.pos.y + gridBox.size.y);
            nvgStroke(vg);
        }
        for (int v = 0; v <= NV; ++v) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, gridBox.pos.x, gridBox.pos.y + v*ch);
            nvgLineTo(vg, gridBox.pos.x + gridBox.size.x, gridBox.pos.y + v*ch);
            nvgStroke(vg);
        }

        // --- membership cells: filled = in (voiceColour), hollow = out ---
        for (int s = 0; s < NS; ++s) {
            for (int v = 0; v < NV; ++v) {
                float cx = gridBox.pos.x + s*cw + 1;
                float cy = gridBox.pos.y + v*ch + 1;
                float cwid = cw - 2;
                float cht = ch - 2;
                if (module->getCell(s, v)) {
                    NVGcolor col = voiceColour(v);
                    nvgBeginPath(vg);
                    nvgRect(vg, cx, cy, cwid, cht);
                    nvgFillColor(vg, col);
                    nvgFill(vg);
                    // Slot assignment: draw the SLOT number (1-8) this voice is seated in THIS scene.
                    // computeSlots gives the effective slot (explicit seatings + auto-pack), so every
                    // member shows where it actually sits. Explicit seat = bright; auto-pack = dim.
                    int8_t slotOf[Intertropical::Ids::N_VOICES];
                    module->computeSlots(s, slotOf);
                    int slot = slotOf[v];
                    if (slot >= 0) {
                        const bool explicitSeat = (module->getSlot(s, v) >= 0);
                        nvgFontSize(vg, 6.f);
                        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                        nvgFillColor(vg, nvgRGBA(0xff,0xff,0xff, explicitSeat ? 0xe0 : 0x70));
                        char buf[4]; snprintf(buf, sizeof(buf), "%d", slot + 1);   // 1-based label
                        nvgText(vg, cx + 1.f, cy + 0.5f, buf, nullptr);
                    }
                } else {
                    // Hollow outline in faint voiceColour
                    NVGcolor col = voiceColour(v);
                    col.a = 0.25f;
                    nvgBeginPath(vg);
                    nvgRect(vg, cx, cy, cwid, cht);
                    nvgStrokeColor(vg, col);
                    nvgStrokeWidth(vg, 0.8f);
                    nvgStroke(vg);
                }
            }
        }

        // --- active-scene column highlight ---
        {
            const int as = module->activeScene;
            nvgBeginPath(vg);
            nvgRect(vg, gridBox.pos.x + as*cw, gridBox.pos.y, cw, gridBox.size.y);
            nvgStrokeColor(vg, nvgRGBA(0xd4,0x00,0x1a,0xd0));   // Singapore red
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }

        // --- repeat row: N lit sub-segments per scene + current-repeat emphasis ---
        {
            const float rcw = repBox.size.x / NS;
            const float rsw = rcw / Intertropical::Ids::MAX_REPEAT;  // sub-segment width
            for (int s = 0; s < NS; ++s) {
                int rep = module->getRepeats(s);
                bool isActive = (s == module->activeScene);
                for (int r = 0; r < Intertropical::Ids::MAX_REPEAT; ++r) {
                    float sx = repBox.pos.x + s*rcw + r*rsw + 0.5f;
                    float sy = repBox.pos.y + 0.5f;
                    float sw = rsw - 1.f;
                    float sh = repBox.size.y - 1.f;
                    if (r < rep) {
                        // Lit segment
                        NVGcolor col = voiceColour(s % 8);
                        if (isActive && r == module->repeatPos) {
                            // Current repeat in active scene: brighter
                            col.a = 1.0f;
                        } else if (isActive) {
                            col.a = 0.7f;  // done repeats in active scene
                        } else {
                            col.a = 0.4f;  // non-active scene repeats
                        }
                        nvgBeginPath(vg);
                        nvgRect(vg, sx, sy, sw, sh);
                        nvgFillColor(vg, col);
                        nvgFill(vg);
                    } else {
                        // Unlit segment
                        nvgBeginPath(vg);
                        nvgRect(vg, sx, sy, sw, sh);
                        nvgFillColor(vg, nvgRGBA(0x30,0x30,0x30,0x50));
                        nvgFill(vg);
                    }
                }
                // Scene boundary line
                nvgBeginPath(vg);
                nvgMoveTo(vg, repBox.pos.x + (s+1)*rcw, repBox.pos.y);
                nvgLineTo(vg, repBox.pos.x + (s+1)*rcw, repBox.pos.y + repBox.size.y);
                nvgStrokeColor(vg, nvgRGBA(0x50,0x50,0x50,0x80));
                nvgStrokeWidth(vg, 0.8f);
                nvgStroke(vg);
            }
        }

        // --- voice numbers 1..16 in the left gutter ---
        nvgFillColor(vg, nvgRGBA(0x80,0x80,0x80,0xff));
        nvgFontSize(vg, 7.f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgTextLetterSpacing(vg, 0.f);
        for (int v = 0; v < NV; ++v) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", v + 1);
            nvgText(vg, gridBox.pos.x - 2.f, gridBox.pos.y + v*ch + ch/2, buf, nullptr);
        }

        drawSlotOutputGrid(vg);
        drawVoiceSlotDisplay(vg);
        drawPairBadge(vg);
    }

    // --- Pair identity badge (top-left): number + pair colour, so a consumer set to
    // "Follow #N" can be matched to THIS instance by eye. See ui/IntertropicalPairing.hpp. ---
    void drawPairBadge(NVGcontext* vg) {
        const int id = module->pairId;
        if (id <= 0) return;
        const float bx = mm2px(3.0f), by = mm2px(3.0f), r = mm2px(3.0f);
        NVGcolor col = redDot::pairColour(id);
        nvgBeginPath(vg);
        nvgCircle(vg, bx + r, by + r, r);
        nvgFillColor(vg, col);
        nvgFill(vg);
        nvgFillColor(vg, nvgRGBA(0x0a,0x0a,0x0a,0xff));
        nvgFontSize(vg, mm2px(3.2f));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        char b[8]; snprintf(b, sizeof(b), "%d", id);
        nvgText(vg, bx + r, by + r + mm2px(0.2f), b, nullptr);
    }

    // --- Slot->output routing grid (bottom-right, 8x8, GLOBAL, interactive) ---
    // Rows = slots (0..7), cols = outputs (0..7). Lit cell = that slot drives that output.
    // FAN-OUT is simply TWO+ lit cells in a slot's row (spec "Routing model + fan-out"). The 8
    // columns align under the 8 transpose knobs. Same idiom as Change Alley's pin matrix.
    void drawSlotOutputGrid(NVGcontext* vg) {
        const int MV = Intertropical::Ids::MAX_VOICES_PER_SCENE;   // 8
        const float cw = routBox.size.x / MV;
        const float chh = routBox.size.y / MV;
        // faint grid lines
        nvgStrokeColor(vg, nvgRGBA(0x50,0x50,0x50,0x80));
        nvgStrokeWidth(vg, 0.5f);
        for (int c = 0; c <= MV; ++c) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, routBox.pos.x + c*cw, routBox.pos.y);
            nvgLineTo(vg, routBox.pos.x + c*cw, routBox.pos.y + routBox.size.y);
            nvgStroke(vg);
        }
        for (int r = 0; r <= MV; ++r) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, routBox.pos.x, routBox.pos.y + r*chh);
            nvgLineTo(vg, routBox.pos.x + routBox.size.x, routBox.pos.y + r*chh);
            nvgStroke(vg);
        }
        // lit cells (slot=row drives output=col). Colour by SLOT so a fanned-out row reads as one slot.
        for (int slot = 0; slot < MV; ++slot) {
            const uint8_t mask = module->slotOutput[slot];
            int litCount = 0; for (int o = 0; o < MV; ++o) if ((mask >> o) & 1u) litCount++;
            for (int out = 0; out < MV; ++out) {
                if (!((mask >> out) & 1u)) continue;
                NVGcolor col = voiceColour(slot);          // slot's identity hue
                if (litCount > 1) { col.r = std::min(1.f, col.r*1.15f); }  // fanned rows slightly hotter
                const float x = routBox.pos.x + out*cw;
                const float y = routBox.pos.y + slot*chh;
                nvgBeginPath(vg);
                nvgRoundedRect(vg, x + cw*0.16f, y + chh*0.16f, cw*0.68f, chh*0.68f, 1.2f);
                nvgFillColor(vg, col);
                nvgFill(vg);
            }
        }
        // Axis labels: output columns (O1..O8) above the grid, slot rows (S1..S8) at the left edge.
        // Columns line up with the transpose knobs below, so "O3" reads straight down to knob 3.
        nvgFillColor(vg, nvgRGBA(0x88,0x88,0x88,0xff));
        nvgFontSize(vg, 5.5f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        for (int o = 0; o < MV; ++o) {
            char b[4]; snprintf(b, sizeof(b), "%d", o + 1);
            nvgText(vg, routBox.pos.x + o*cw + cw*0.5f, routBox.pos.y - 1.f, b, nullptr);
        }
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        for (int s = 0; s < MV; ++s) {
            char b[6]; snprintf(b, sizeof(b), "S%d", s + 1);
            nvgText(vg, routBox.pos.x - 1.5f, routBox.pos.y + s*chh + chh*0.5f, b, nullptr);
        }
    }

    // --- Voice->slot display (top-right, per-scene, READ-ONLY) ---
    // Shows, for the ACTIVE scene, which global voice is seated in each of the 8 slots (rows).
    // Read-only mirror of computeSlots(activeScene): a labelled chip per occupied slot.
    void drawVoiceSlotDisplay(NVGcontext* vg) {
        // Grid is dimensioned SCENES (columns) x SLOTS (rows). Show the LIVE voice->slot seating for
        // ALL scenes at once (each column = that scene's seating, from its global-to-scene selections),
        // with a cursor traversing across the scene columns as playback advances -- matching the
        // global-to-scene membership grid. (Earlier this only filled the active scene's single column.)
        const int NSC = Intertropical::Ids::N_SCENES;              // 8 scene columns
        const int MV  = Intertropical::Ids::MAX_VOICES_PER_SCENE;  // 8 slot rows
        const int loopLen = module->getLoopLen();
        const int active  = module->activeScene;
        const float colw = vsBox.size.x / NSC;
        const float rowh = vsBox.size.y / MV;
        nvgFontSize(vg, 6.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        // cursor: red stroke frame on the active scene's column -- matches the main membership grid's
        // active-scene cursor style (Singapore red, nvgRGBA(0xd4,0x00,0x1a,0xd0), stroke 1.5f).
        {
            const float cx = vsBox.pos.x + active*colw;
            nvgBeginPath(vg);
            nvgRect(vg, cx, vsBox.pos.y, colw, vsBox.size.y);
            nvgStrokeColor(vg, nvgRGBA(0xd4,0x00,0x1a,0xd0));   // Singapore red, matches main grid
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
        // faint grid lines
        nvgStrokeColor(vg, nvgRGBA(0x50,0x50,0x50,0x50));
        nvgStrokeWidth(vg, 0.5f);
        for (int c = 0; c <= NSC; ++c) {
            const float x = vsBox.pos.x + c*colw;
            nvgBeginPath(vg); nvgMoveTo(vg, x, vsBox.pos.y); nvgLineTo(vg, x, vsBox.pos.y + vsBox.size.y); nvgStroke(vg);
        }
        for (int r = 0; r <= MV; ++r) {
            const float y = vsBox.pos.y + r*rowh;
            nvgBeginPath(vg); nvgMoveTo(vg, vsBox.pos.x, y); nvgLineTo(vg, vsBox.pos.x + vsBox.size.x, y); nvgStroke(vg);
        }

        // each scene column: compute that scene's voice->slot seating and draw a chip per occupied slot
        int8_t slotOf[Intertropical::Ids::N_VOICES];
        for (int c = 0; c < NSC; ++c) {
            const bool inLoop = (c < loopLen);
            module->computeSlots(c, slotOf);
            int voiceInSlot[8]; for (int s = 0; s < MV; ++s) voiceInSlot[s] = -1;
            for (int v = 0; v < Intertropical::Ids::N_VOICES; ++v)
                if (slotOf[v] >= 0 && slotOf[v] < MV) voiceInSlot[slotOf[v]] = v;
            const float x = vsBox.pos.x + c*colw;
            for (int s = 0; s < MV; ++s) {
                const int v = voiceInSlot[s];
                if (v < 0) continue;                 // empty slot in this scene
                const float y = vsBox.pos.y + s*rowh;
                NVGcolor col = voiceColour(s);
                if (!inLoop) col.a *= 0.35f;         // scenes beyond the loop length: dimmed
                nvgBeginPath(vg);
                nvgRoundedRect(vg, x + colw*0.12f, y + rowh*0.15f, colw*0.76f, rowh*0.7f, 1.5f);
                nvgFillColor(vg, col);
                nvgFill(vg);
                nvgFillColor(vg, nvgRGBA(0x0a,0x0a,0x0a,0xff));
                char vb[8]; snprintf(vb, sizeof(vb), "%d", v + 1);
                nvgText(vg, x + colw/2, y + rowh/2, vb, nullptr);
            }
        }
    }
};

struct IntertropicalWidget : ModuleWidget,
    dotModular::Compose<IntertropicalWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    IntertropicalWidget(Intertropical* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/panels/Intertropical_panel_dark.svg"));

        // Grid widget — covers the repeat row + main grid area
        auto* grid = new IntertropicalGrid;
        grid->module = module;
        grid->gridBox = Rect(mm2px(Vec(IT_GRID_X, IT_GRID_Y)),
                             mm2px(Vec(IT_GRID_W, IT_GRID_H)));
        grid->repBox  = Rect(mm2px(Vec(IT_GRID_X, IT_REP_Y)),
                             mm2px(Vec(IT_GRID_W, IT_REP_H)));
        // Right block: voice->slot DISPLAY (top) + slot->output ROUTING grid (bottom). X/W from the
        // panel's two right-side boxes (px 330.7 / w 281.6 -> mm 112.0 / 95.37); the routing grid's
        // 8 output columns line up under the 8 transpose knobs (param_0..7) by construction.
        grid->vsBox   = Rect(mm2px(Vec(IT_RIGHT_X, IT_VS_TOP)),
                             mm2px(Vec(IT_RIGHT_W, IT_VS_H)));
        grid->routBox = Rect(mm2px(Vec(IT_RIGHT_X, IT_ROUT_TOP)),
                             mm2px(Vec(IT_RIGHT_W, IT_ROUT_H)));
        // FULL-PANEL box (origin 0,0) so the absolute panel-mm coords in gridBox/repBox equal
        // the widget's LOCAL draw coords. Bug fixed: box.pos was offset to (IT_GRID_X-6,
        // IT_GRID_Y-2), so drawing at absolute gridBox.pos rendered offset by that origin -- the
        // "second, shifted grid" over the panel's own static grid, and clipped the repeats.
        grid->box = Rect(Vec(0, 0), box.size);
        addChild(grid);

        // 8 per-output TRANSPOSE knobs -- bound to panel markers param_0..7 (real params).
        for (int o = 0; o < 8; ++o)
            bindParam<DimmableTrimpot>(rack::string::f("param_%d", o),
                                         Intertropical::Ids::TRANSPOSE_FIRST + o);

        // 5 poly OUTPUT jacks -- bound to panel markers output_0..4.
        for (int o = 0; o < Intertropical::Ids::NUM_OUTPUTS; ++o)
            bindOutput<redDot::GoldPolyPort>(rack::string::f("output_%d", o), o);

        // dot.modular connect mark (greyed when no Monsoon attached).
        if (auto* s = findNamed("light_connect")) {
            auto* cm = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            // Intertropical is an OBSERVER (not a claimed expander), so light on REACHABILITY
            // (can we see a Monsoon/Straits system?) not isConnectedAndClaimed (which requires
            // a claim slot that doesn't exist for observers and can't scale to N pairs).
            cm->connected = [module]() {
                return module && redDot::findMonsoonEitherSide(module) != nullptr;
            };
            addChild(cm);
        }

        // Branded dot.modular screws (matches the family).
        redDot::addRedScrews(this);
    }

    void step() override { ModuleWidget::step(); kitStep(); }

    void appendContextMenu(Menu* menu) override {
        auto* m = dynamic_cast<Intertropical*>(module);
        if (!m) return;
        menu->addChild(new MenuSeparator);
        auto* label = new MenuLabel;
        label->text = "Scenes (loop length)";
        menu->addChild(label);
        for (int n = 1; n <= Intertropical::Ids::N_SCENES; ++n) {
            auto* item = new MenuItem;
            item->text = std::to_string(n) + (n == 1 ? " scene" : " scenes");
            item->rightText = (m->getLoopLen() == n) ? "\xe2\x9c\x93" : "";
            struct SetLoop : MenuItem {
                Intertropical* m; int n;
                void onAction(const event::Action& e) override { m->setLoopLen(n); }
            };
            auto* sl = new SetLoop;
            sl->text = item->text;
            sl->rightText = item->rightText;
            sl->m = m; sl->n = n;
            menu->addChild(sl);
            delete item;  // replaced by sl
        }
    }
};

Model* modelIntertropical = createModel<Intertropical, IntertropicalWidget>("Intertropical");
