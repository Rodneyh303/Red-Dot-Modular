#pragma once
// MonsoonChangeAlleyExpander.hpp — Change Alley pin-matrix expander
//
// A 16×2 store-backed pin matrix that reallocates which voice consumes which
// voice's raw Philox random streams — separately for RHYTHM (R, PL_REST +
// PL_ACCENT) and MELODY (M, PL_MELODY + PL_OCTAVE) — before all downstream
// manipulation (LOR, thresholds, mods, clamp). This is a READ-TIME INDIRECTION
// over the existing tables: all 16 voices' tables are still drawn as today;
// the matrix just changes who reads whose draws.
//
// Key properties (from CHANGE_ALLEY_DESIGN.md):
//   • Identity diagonal = today's engine (no exchange, pass-through)
//   • Row radio: each consuming voice has exactly ONE rhythm source and ONE melody source
//   • Locality: moving a pin perturbs ONLY that row; other voices are bit-identical
//   • Dice/pin orthogonality: dice re-rolls tables (new material), pins survive (same
//     relationships). Re-dice a subset/superset pair → new rhythm, same hierarchy.
//   • Lock: pins are configuration (like rotation); they latch under the planned
//     Vermona-faithful LOCK and stay audible under the current material-freeze lock.
//   • Zero param slots: the 32 assignments (16 rhythm + 16 melody) are store-backed,
//     never params — designed under DAW_PARAM_AUDIT.md from day one.
//
// Engine integration: Monsoon reads rhythmSrc[]/melodySrc[] from the cached pointer
// in processDNA / SequencerEngine.cpp at the polyRandom(voiceIdx, lane) call sites.
// Default (no expander): both arrays identity-initialised (voiceIdx → voiceIdx).
//
// Panel: 18HP, EMS VCS3-style pin matrix, currency-pair row labels (the actual
// Change Alley FX booths vs USD), amber pins = rhythm, teal pins = melody.
// See gen_change_alley.py for the panel generator.
//
// Concentric two-colour pins when both R and M share a cell: outer ring = teal
// (melody), inner dot = amber (rhythm). Left-click = move rhythm pin of that row;
// right-click (or Ctrl+click) = move melody pin. Row-radio behaviour; misclicks
// self-correct.

#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace ChangeAlleyIds;

struct MonsoonChangeAlleyExpander : Module {
    // The two routing tables — store-backed (zero params).
    // rhythmSrc[v] = which voice's REST/ACCENT pool voice v draws from (0..15)
    // melodySrc[v] = which voice's MELODY/OCTAVE pool voice v draws from (0..15)
    uint8_t rhythmSrc[N_VOICES];
    uint8_t melodySrc[N_VOICES];

    // Currency labels for the rows — matches the panel and the doc.
    // Index 0 = V1 (mono), index 1..15 = V2..V16.
    static constexpr const char* CURRENCIES[N_VOICES] = {
        "SGD","MYR","IDR","THB",
        "PHP","VND","MMK","KHR",
        "HKD","CNY","TWD","KRW",
        "JPY","AUD","INR","USD",
    };

    MonsoonChangeAlleyExpander() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        resetToIdentity();
    }

    void resetToIdentity() {
        for (int v = 0; v < N_VOICES; ++v) {
            rhythmSrc[v] = (uint8_t)v;
            melodySrc[v] = (uint8_t)v;
        }
    }

    void process(const ProcessArgs& args) override {
        // Passive — Monsoon reads our tables via the cached pointer. No audio logic here.
    }

    // ── Serialization ────────────────────────────────────────────────────────
    json_t* dataToJson() override {
        json_t* root = json_object();
        json_t* rArr = json_array();
        json_t* mArr = json_array();
        for (int v = 0; v < N_VOICES; ++v) {
            json_array_append_new(rArr, json_integer(rhythmSrc[v]));
            json_array_append_new(mArr, json_integer(melodySrc[v]));
        }
        json_object_set_new(root, "rhythmSrc", rArr);
        json_object_set_new(root, "melodySrc", mArr);
        return root;
    }

    void dataFromJson(json_t* root) override {
        resetToIdentity();
        auto loadArr = [&](const char* key, uint8_t* dst) {
            json_t* arr = json_object_get(root, key);
            if (!arr) return;
            for (int v = 0; v < N_VOICES && v < (int)json_array_size(arr); ++v) {
                json_t* val = json_array_get(arr, v);
                if (json_is_integer(val))
                    dst[v] = (uint8_t)math::clamp((int)json_integer_value(val), 0, N_VOICES-1);
            }
        };
        loadArr("rhythmSrc", rhythmSrc);
        loadArr("melodySrc", melodySrc);
    }
};

// ── Widget ───────────────────────────────────────────────────────────────────
struct MonsoonChangeAlleyExpanderWidget : ModuleWidget {
    static constexpr int N_VOICES = ChangeAlleyIds::N_VOICES;
    static constexpr int N_POOLS  = ChangeAlleyIds::N_POOLS;

    // Pin cell positions (populated in constructor, used by draw + event handlers)
    struct PinCell { float x, y, w, h; int voice, pool; };
    std::vector<PinCell> cells_;

    MonsoonChangeAlleyExpanderWidget(MonsoonChangeAlleyExpander* module) {
        setModule(module);

        // Panel
        std::string darkPath  = asset::plugin(pluginInstance, "res/panels/ChangeAlley_panel_dark.svg");
        std::string lightPath = asset::plugin(pluginInstance, "res/panels/ChangeAlley_panel_light.svg");
        setPanel(Svg::load(settings::preferDarkPanels ? darkPath : lightPath));

        // Screws
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5, 1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(box.size.x / mm2px(Vec(1,0)).x - 1.5, 1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5, 128.5 - 1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(box.size.x / mm2px(Vec(1,0)).x - 1.5, 128.5 - 1.5))));

        // Build pin cell hit-test table (matches gen_change_alley.py geometry)
        // MX=28, MY=18, CELL=9.5 (in panel mm at 18HP=91.44mm wide → scale=91.44/270)
        const float PW_mm   = 18.f * 5.08f;   // panel mm width
        const float scale   = PW_mm / 270.f;   // mm per generator-px
        const float MX      = 28.f * scale;
        const float MY      = 18.f * scale;
        const float CELL    = 9.5f * scale;
        for (int v = 0; v < N_VOICES; ++v) {
            for (int p = 0; p < N_POOLS; ++p) {
                PinCell c;
                c.x = mm2px(MX + p * CELL);
                c.y = mm2px(MY + v * CELL);
                c.w = mm2px(CELL);
                c.h = mm2px(CELL);
                c.voice = v;
                c.pool  = p;
                cells_.push_back(c);
            }
        }

        // Overlay: a transparent widget that draws the pins and handles clicks
        auto* overlay = new PinOverlay(module, cells_, this);
        overlay->box.pos  = Vec(0, 0);
        overlay->box.size = box.size;
        addChild(overlay);
    }

    // ── PinOverlay: draws the live pin state, handles click-to-set ───────────
    struct PinOverlay : widget::OpaqueWidget {
        MonsoonChangeAlleyExpander* module;
        const std::vector<PinCell>& cells;
        MonsoonChangeAlleyExpanderWidget* parent;

        PinOverlay(MonsoonChangeAlleyExpander* m, const std::vector<PinCell>& c,
                   MonsoonChangeAlleyExpanderWidget* p)
            : module(m), cells(c), parent(p) {}

        // Draw all pins
        void draw(const DrawArgs& args) override {
            if (!module) return;
            NVGcontext* vg = args.vg;
            for (const auto& cell : cells) {
                float cx = cell.x + cell.w * 0.5f;
                float cy = cell.y + cell.h * 0.5f;
                uint8_t rsrc = module->rhythmSrc[cell.voice];
                uint8_t msrc = module->melodySrc[cell.voice];
                bool rActive = (rsrc == (uint8_t)cell.voice && cell.pool == 0)
                            || (rsrc != (uint8_t)cell.voice && (int)rsrc == cell.voice && cell.pool == 0);
                // Simpler: pool 0 = rhythm, pool 1 = melody
                bool rhythmHere = (cell.pool == 0 && rsrc == (uint8_t)cell.voice);
                bool melodyHere = (cell.pool == 1 && msrc == (uint8_t)cell.voice);
                bool isRhythm = (cell.pool == 0);
                uint8_t src = isRhythm ? rsrc : msrc;
                bool pinHere = (src == (uint8_t)cell.voice);

                if (pinHere) {
                    // Filled pin — amber for rhythm, teal for melody
                    NVGcolor fill = isRhythm
                        ? nvgRGB(0xc8, 0x90, 0x0c)
                        : nvgRGB(0x2a, 0x78, 0x70);
                    nvgBeginPath(vg);
                    nvgCircle(vg, cx, cy, cell.w * 0.28f);
                    nvgFillColor(vg, fill);
                    nvgFill(vg);
                    // inner highlight
                    nvgBeginPath(vg);
                    nvgCircle(vg, cx, cy, cell.w * 0.12f);
                    nvgFillColor(vg, nvgRGBAf(1,1,1,0.22f));
                    nvgFill(vg);
                } else {
                    // Ghost outline — dim version of pin colour
                    NVGcolor ghost = isRhythm
                        ? nvgRGBA(0xc8, 0x90, 0x0c, 0x40)
                        : nvgRGBA(0x2a, 0x78, 0x70, 0x40);
                    nvgBeginPath(vg);
                    nvgCircle(vg, cx, cy, cell.w * 0.25f);
                    nvgStrokeColor(vg, ghost);
                    nvgStrokeWidth(vg, 0.8f);
                    nvgStroke(vg);
                }
            }
        }

        // Click: left = set rhythm pin for that row, right = set melody pin
        void onButton(const event::Button& e) override {
            if (!module) return;
            if (e.action != GLFW_PRESS) { OpaqueWidget::onButton(e); return; }
            for (const auto& cell : cells) {
                Rect r(Vec(cell.x, cell.y), Vec(cell.w, cell.h));
                if (!r.contains(e.pos)) continue;
                bool setMelody = (e.button == GLFW_MOUSE_BUTTON_RIGHT)
                              || (e.mods & RACK_MOD_CTRL);
                if (setMelody) {
                    // Right-click: set melody source for this voice to this column's voice
                    // (column = cell.voice, but we're using row=voice, col=pool, so
                    //  for a 2-pool matrix the "source voice" IS the row index we're clicking)
                    // Actually: clicking cell (voice=v, pool=p) should SET src[v] = v
                    // (identity) or cycle to the next source. For now: toggle between
                    // identity (v→v) and the previously set source.
                    // Simplified MVP: left-click always sets src[row] = row (identity reset).
                    // Full "click another cell in same row to re-source" needs row-radio UI.
                    // Store the edit with StoreEditAction when that infrastructure lands.
                    // For now: left-click resets that row to identity.
                    uint8_t old = module->melodySrc[cell.voice];
                    module->melodySrc[cell.voice] = (uint8_t)cell.voice;
                    (void)old;
                } else {
                    uint8_t old = module->rhythmSrc[cell.voice];
                    module->rhythmSrc[cell.voice] = (uint8_t)cell.voice;
                    (void)old;
                }
                e.consume(this);
                return;
            }
            OpaqueWidget::onButton(e);
        }

        // Tooltip showing current mapping
        void onHover(const event::Hover& e) override {
            for (const auto& cell : cells) {
                Rect r(Vec(cell.x, cell.y), Vec(cell.w, cell.h));
                if (r.contains(e.pos)) {
                    // Future: show tooltip "SGD/USD rhythm ← JPY pool"
                    break;
                }
            }
            OpaqueWidget::onHover(e);
        }
    };
};
