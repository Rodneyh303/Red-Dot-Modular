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
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"

using namespace rack;
using namespace ChangeAlleyIds;

struct MonsoonChangeAlleyExpander : Module {
    uint8_t rhythmSrc[N_VOICES];
    uint8_t melodySrc[N_VOICES];

    static constexpr const char* CURRENCIES[N_VOICES] = {
        "SGD","MYR","IDR","THB","PHP","VND","MMK","KHR",
        "HKD","CNY","TWD","KRW","JPY","AUD","INR","USD",
    };

    MonsoonChangeAlleyExpander() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        resetToIdentity();
    }

    void resetToIdentity() {
        for (int v = 0; v < N_VOICES; ++v) { rhythmSrc[v] = v; melodySrc[v] = v; }
    }

    void process(const ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* root = json_object();
        auto save = [&](const char* k, const uint8_t* a) {
            json_t* arr = json_array();
            for (int v = 0; v < N_VOICES; ++v) json_array_append_new(arr, json_integer(a[v]));
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
            for (int v = 0; v < N_VOICES && v < (int)json_array_size(arr); ++v) {
                json_t* val = json_array_get(arr, v);
                if (json_is_integer(val))
                    a[v] = (uint8_t)math::clamp((int)json_integer_value(val), 0, N_VOICES-1);
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
    static constexpr float MY_MM    = GUTTER_T + 4.0f;
    static constexpr float MW_MM    = PW_MM - GUTTER_L - GUTTER_R;
    static constexpr float MH_MM    = PH_MM - MY_MM - GUTTER_B - 2.0f;
    static constexpr float CELL_W   = MW_MM / N_VOICES;
    static constexpr float CELL_H   = MH_MM / N_VOICES;

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
            if (!module) return;
            NVGcontext* vg = args.vg;
            int poly = getPolyCount();
            float ro = cellRadius(), ri = innerRadius();

            // Colours
            NVGcolor white     = nvgRGB(0xf0,0xf0,0xee);
            NVGcolor red       = nvgRGB(0xd4,0x00,0x1a);
            NVGcolor whiteDim  = nvgRGBA(0xf0,0xf0,0xee,0x30);
            NVGcolor redDimC   = nvgRGBA(0xd4,0x00,0x1a,0x30);
            NVGcolor whiteDef  = nvgRGBA(0xf0,0xf0,0xee,0x70);  // identity — dim filled
            NVGcolor redDef    = nvgRGBA(0xd4,0x00,0x1a,0x70);

            for (int row = 0; row < N_VOICES; ++row) {
                bool active = (row == 0) || (row <= poly);  // row 0=mono always active
                float alpha = active ? 1.f : 0.4f;
                uint8_t rSrc = module->rhythmSrc[row];
                uint8_t mSrc = module->melodySrc[row];

                for (int col = 0; col < N_VOICES; ++col) {
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
                        NVGcolor col_ = mIdentity ? nvgRGBAf(0.83f,0.f,0.1f,0.7f*alpha) : nvgRGBAf(0.83f,0.f,0.1f,alpha);
                        nvgBeginPath(vg); nvgCircle(vg, c.x, c.y, ri);
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
            for (int row = 0; row < N_VOICES; ++row) {
                for (int col = 0; col < N_VOICES; ++col) {
                    if (!hitCell(e.pos, row, col)) continue;
                    if (setMelody) {
                        module->melodySrc[row] = (uint8_t)col;
                    } else {
                        module->rhythmSrc[row] = (uint8_t)col;
                    }
                    e.consume(this);
                    return;
                }
            }
            OpaqueWidget::onButton(e);
        }

        void appendContextMenu(ui::Menu* menu) {
            if (!module) return;
            menu->addChild(new MenuSeparator);
            menu->addChild(createMenuItem("Reset to identity diagonal", "",
                [this]() { if (module) module->resetToIdentity(); }));
        }
    };

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* module = dynamic_cast<MonsoonChangeAlleyExpander*>(this->module);
        if (!module) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reset to identity diagonal", "",
            [module]() { module->resetToIdentity(); }));
    }
};
