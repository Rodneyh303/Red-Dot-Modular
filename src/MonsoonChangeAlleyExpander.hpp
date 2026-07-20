#pragma once
// MonsoonChangeAlleyExpander — 16×16 pin-matrix expander
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
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/StoreEditAction.hpp"   // pin edits: store-backed, undoable (DAW_PARAM_AUDIT 5b)

using namespace rack;
// NOT 'using namespace ChangeAlleyIds' — Monsoon.hpp exposes MonsoonIds with the same
// NUM_PARAMS/NUM_INPUTS/... names, so we qualify explicitly (same rule as the Sands
// managers: 'NOT using namespace ... to avoid ambiguous calls — qualify below').
namespace CA = ChangeAlleyIds;

struct MonsoonChangeAlleyExpander : Module {
    uint8_t rhythmSrc[CA::N_VOICES];
    uint8_t melodySrc[CA::N_VOICES];

    static constexpr const char* CURRENCIES[CA::N_VOICES] = {
        "SGD","MYR","IDR","THB","PHP","VND","MMK","KHR",
        "HKD","CNY","TWD","KRW","JPY","AUD","INR","USD",
    };

    MonsoonChangeAlleyExpander() {
        config(CA::NUM_PARAMS, CA::NUM_INPUTS, CA::NUM_OUTPUTS, CA::NUM_LIGHTS);
        resetToIdentity();
    }

    void resetToIdentity() {
        for (int v = 0; v < CA::N_VOICES; ++v) { rhythmSrc[v] = v; melodySrc[v] = v; }
    }

    void process(const ProcessArgs&) override {}

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
struct MonsoonChangeAlleyExpanderWidget : ModuleWidget {

    // Matrix geometry in mm (must match gen_change_alley.py exactly)
    static constexpr float PW_MM    = 18.f * 5.08f;
    static constexpr float PH_MM    = 128.5f;
    static constexpr float GUTTER_L = 13.0f;
    static constexpr float GUTTER_R =  3.5f;
    static constexpr float GUTTER_T = 10.0f;
    static constexpr float GUTTER_B = 12.0f;
    static constexpr float MX_MM    = GUTTER_L;
    static constexpr float MY_MM    = GUTTER_T + 8.0f;              // square grid: matches generator
    static constexpr float MW_MM    = PW_MM - GUTTER_L - GUTTER_R;
    static constexpr float CELL_W   = MW_MM / CA::N_VOICES;             // SQUARE cells (width-constrained)
    static constexpr float CELL_H   = CELL_W;
    static constexpr float MH_MM    = CELL_H * CA::N_VOICES;

    // Cell centre in px (rack mm2px)
    static Vec cellCentre(int row, int col) {
        return mm2px(Vec(MX_MM + col * CELL_W + CELL_W * 0.5f,
                         MY_MM + row * CELL_H + CELL_H * 0.5f));
    }
    static float cellRadius() {
        return mm2px(Vec(std::min(CELL_W, CELL_H) * 0.28f, 0)).x;
    }
    static float innerRadius() {
        return mm2px(Vec(std::min(CELL_W, CELL_H) * 0.13f, 0)).x;
    }
    static bool hitCell(Vec pos, int row, int col) {
        Vec c = cellCentre(row, col);
        float r = mm2px(Vec(std::min(CELL_W, CELL_H) * 0.5f, 0)).x;
        Vec d = pos - c;
        return d.x*d.x + d.y*d.y < r*r;
    }

    MonsoonChangeAlleyExpanderWidget(MonsoonChangeAlleyExpander* module) {
        setModule(module);
        std::string dark  = asset::plugin(pluginInstance, "res/panels/ChangeAlley_panel_dark.svg");
        std::string light = asset::plugin(pluginInstance, "res/panels/ChangeAlley_panel_light.svg");
        setPanel(Svg::load(settings::preferDarkPanels ? dark : light));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5,    1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 1.5, 1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5,    PH_MM - 1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 1.5, PH_MM - 1.5))));

        auto* ov = new PinOverlay(module);
        ov->box.pos  = Vec(0, 0);
        ov->box.size = box.size;
        addChild(ov);
    }

    struct PinOverlay : widget::OpaqueWidget {
        MonsoonChangeAlleyExpander* module;
        PinOverlay(MonsoonChangeAlleyExpander* m) : module(m) {}

        int getPolyCount() const {
            if (!module) return 0;
            auto* mon = redDot::findMonsoonEitherSide(module);
            return mon ? mon->engine.numPolyVoices : 0;
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
                    const bool darkPanel = settings::preferDarkPanels;
                    NVGcolor ink    = darkPanel ? nvgRGB(0xe8,0xe2,0xd0) : nvgRGB(0x1a,0x18,0x10);
                    NVGcolor inkdim = darkPanel ? nvgRGBA(0x7a,0x70,0x60,0xb0) : nvgRGBA(0x6a,0x60,0x50,0xb0);
                    NVGcolor amber  = nvgRGB(0xc8,0x90,0x0c);
                    static constexpr const char* CUR[CA::N_VOICES] = {
                        "SGD","MYR","IDR","THB","PHP","VND","MMK","KHR",
                        "HKD","CNY","TWD","KRW","JPY","AUD","INR","USD" };
                    char num[4];
                    // Column labels: number (near grid) + currency (dim, above)
                    for (int col = 0; col < CA::N_VOICES; ++col) {
                        Vec c = cellCentre(0, col);
                        float topY = mm2px(Vec(0, MY_MM)).y;
                        snprintf(num, sizeof(num), "%d", col + 1);
                        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                        nvgFontSize(vg, mm2px(Vec(2.4f,0)).x);
                        nvgFillColor(vg, col == 0 ? amber : ink);
                        nvgText(vg, c.x, topY - mm2px(Vec(0,1.2f)).y, num, NULL);
                        nvgFontSize(vg, mm2px(Vec(1.8f,0)).x);
                        nvgFillColor(vg, inkdim);
                        nvgText(vg, c.x, topY - mm2px(Vec(0,4.0f)).y, CUR[col], NULL);
                    }
                    // Row labels: number (near grid) + currency (dim, further left)
                    for (int row = 0; row < CA::N_VOICES; ++row) {
                        Vec c = cellCentre(row, 0);
                        float leftX = mm2px(Vec(MX_MM,0)).x;
                        snprintf(num, sizeof(num), "%d", row + 1);
                        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
                        nvgFontSize(vg, mm2px(Vec(2.4f,0)).x);
                        nvgFillColor(vg, row == 0 ? amber : ink);
                        nvgText(vg, leftX - mm2px(Vec(1.0f,0)).x, c.y, num, NULL);
                        nvgFontSize(vg, mm2px(Vec(1.8f,0)).x);
                        nvgFillColor(vg, inkdim);
                        nvgText(vg, leftX - mm2px(Vec(4.6f,0)).x, c.y, CUR[row], NULL);
                    }
                    // Title + legend
                    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                    nvgFontSize(vg, mm2px(Vec(3.6f,0)).x);
                    nvgFillColor(vg, ink);
                    nvgText(vg, box.size.x * 0.5f, mm2px(Vec(0,6.0f)).y, "CHANGE ALLEY", NULL);
                    float legY = mm2px(Vec(0, MY_MM + MH_MM + 4.5f)).y;
                    float legX = mm2px(Vec(MX_MM,0)).x;
                    nvgBeginPath(vg); nvgCircle(vg, legX + 4.f, legY, 3.2f);
                    nvgFillColor(vg, nvgRGB(0xf0,0xf0,0xee)); nvgFill(vg);
                    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                    nvgFontSize(vg, mm2px(Vec(2.2f,0)).x);
                    nvgFillColor(vg, inkdim);
                    nvgText(vg, legX + 10.f, legY, "rhythm", NULL);
                    nvgBeginPath(vg); nvgCircle(vg, legX + mm2px(Vec(20.f,0)).x, legY, 3.2f);
                    nvgFillColor(vg, nvgRGB(0xd4,0x00,0x1a)); nvgFill(vg);
                    nvgFillColor(vg, inkdim);
                    nvgText(vg, legX + mm2px(Vec(20.f,0)).x + 6.f, legY, "melody  (right-click / ctrl-click)", NULL);
                }
            }
            if (!module) return;

            // Pin colours are inlined below (white=rhythm, red=melody; identity pins at
            // 0.7 alpha, inactive rows at 0.4). Single literals, easy to tune.

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

                    if (hasR && hasM) {
                        // Concentric: white outer, red inner
                        NVGcolor oc = rIdentity ? nvgRGBAf(0.94f,0.94f,0.93f,0.7f*alpha) : nvgRGBAf(0.94f,0.94f,0.93f,alpha);
                        NVGcolor ic = mIdentity ? nvgRGBAf(0.83f,0.f,0.1f,0.7f*alpha) : nvgRGBAf(0.83f,0.f,0.1f,alpha);
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ro);
                        nvgFillColor(vg, oc); nvgFill(vg);
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ri);
                        nvgFillColor(vg, ic); nvgFill(vg);
                    } else if (hasR) {
                        NVGcolor col_ = rIdentity ? nvgRGBAf(0.94f,0.94f,0.93f,0.7f*alpha) : nvgRGBAf(0.94f,0.94f,0.93f,alpha);
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ro);
                        nvgFillColor(vg, col_); nvgFill(vg);
                    } else if (hasM) {
                        // Lone melody pin draws at NEAR-FULL size — at the concentric inner
                        // radius it was a ~2px dot, invisible on the dark grid (the "can't
                        // see moved red pins" bug). Slightly smaller than white so the two
                        // types stay distinguishable even when separated.
                        NVGcolor col_ = mIdentity ? nvgRGBAf(0.83f,0.f,0.1f,0.7f*alpha) : nvgRGBAf(0.83f,0.f,0.1f,alpha);
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ro * 0.78f);
                        nvgFillColor(vg, col_); nvgFill(vg);
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
        }

        // Row-radio click: left=rhythm, right/ctrl=melody
        // Clicking cell (row, col) sets rhythmSrc[row]=col or melodySrc[row]=col.
        // Row-radio is automatic: src[row] holds exactly one value — this overwrites it.
        void onButton(const event::Button& e) override {
            if (!module || e.action != GLFW_PRESS) { OpaqueWidget::onButton(e); return; }
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
                        redDot::applyAndPushStoreEdit<MonsoonChangeAlleyExpander>(
                            module,
                            setMelody ? "move melody pin" : "move rhythm pin",
                            [row, setMelody](MonsoonChangeAlleyExpander& m, float v) {
                                uint8_t c = (uint8_t)math::clamp((int)std::lround(v), 0, CA::N_VOICES - 1);
                                (setMelody ? m.melodySrc : m.rhythmSrc)[row] = c;
                            },
                            oldV, (float)col);
                    }
                    e.consume(this);
                    return;
                }
            }
            OpaqueWidget::onButton(e);
        }

    };

    // Reset = up to 32 cell changes; one gesture must be ONE undo step, so it gets a
    // whole-table snapshot action rather than 32 StoreEditActions. Same module-id
    // resolution discipline as StoreEditAction (survives deletion; no-ops while gone).
    struct ResetPinsAction : rack::history::Action {
        int64_t moduleId;
        uint8_t oldR[CA::N_VOICES], oldM[CA::N_VOICES];
        ResetPinsAction(MonsoonChangeAlleyExpander* m) : moduleId(m->id) {
            name = "reset pins to identity";
            for (int v = 0; v < CA::N_VOICES; ++v) { oldR[v] = m->rhythmSrc[v]; oldM[v] = m->melodySrc[v]; }
        }
        MonsoonChangeAlleyExpander* resolve() {
            return dynamic_cast<MonsoonChangeAlleyExpander*>(APP->engine->getModule(moduleId));
        }
        void undo() override {
            if (auto* m = resolve())
                for (int v = 0; v < CA::N_VOICES; ++v) { m->rhythmSrc[v] = oldR[v]; m->melodySrc[v] = oldM[v]; }
        }
        void redo() override {
            if (auto* m = resolve()) m->resetToIdentity();
        }
    };

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* module = dynamic_cast<MonsoonChangeAlleyExpander*>(this->module);
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
