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

    // Restructure queue (§11): one pending per (transform × type) row, latest-overwrites.
    // Set by button press or gate rising edge here (either thread-safe bool latch);
    // APPLIED by the expander manager at phrase boundary / unlock (audio side), which
    // writes the transform result into rhythmSrc/melodySrc and clears the flag.
    // Restructure queue (§11 + §14a latch fix): parameters LATCHED AT TRIGGER TIME.
    // A bare bool pendingRow caused the manager to re-read the grain knob at the phrase
    // boundary -- turning the knob between trigger and bar line silently changed what fired.
    // Now the struct captures {armed, blk, scatterSeed} at the moment of the trigger.
    struct PendingAction {
        bool     armed       = false;
        int      blk         = 4;
        uint32_t scatterSeed = 0;
    };
    PendingAction pendingRow[8];

    // Submatrix highlight state, filled by MonsoonExpanderManager from the attached
    // Temasek expander. A PLAIN POD deliberately: Change Alley must not know Temasek's
    // type, or the two headers form an include cycle (Temasek would need Change Alley
    // for nothing, and Change Alley would need Temasek's complete type here).
    struct TkHighlight { bool armed = false; int blk = 4; bool isInter = false; int type = 0; };
    static constexpr int TK_HL_ROWS = 16;
    TkHighlight tkHighlight[TK_HL_ROWS];
    rack::dsp::SchmittTrigger gateTrig[8];
    rack::dsp::BooleanTrigger btnTrig[8];
    // (scatterSeed is now per-row inside PendingAction)

    MonsoonChangeAlleyExpander() {
        config(CA::NUM_PARAMS, CA::NUM_INPUTS, CA::NUM_OUTPUTS, CA::NUM_LIGHTS);
        static constexpr const char* TN[4] = {"Collapse","Rotate","Scatter","Reflect"};
        for (int t = 0; t < CA::N_TRANSFORMS; ++t)
            for (int r = 0; r < 2; ++r) {
                const int row = CA::ctrlRow(t, r == 0);
                const std::string nm = std::string(TN[t]) + (r == 0 ? " rhythm" : " melody");
                // Stepped block-size knob: detents 0..4 → block {1,2,4,8,16}
                configSwitch(CA::BLOCK_KNOB_START + row, 0.f, 4.f, 2.f, nm + " block size",
                             {"1 (per voice)","2","4","8","16 (whole pool)"});
                configButton(CA::TRIG_BTN_START + row, nm + " trigger");
                configInput(CA::TRIG_IN_START + row, nm + " trigger gate");
            }
        resetToIdentity();
    }

    static int blockFromKnob(float v) {
        static const int B[5] = {1,2,4,8,16};
        int i = (int)std::lround(v); if (i < 0) i = 0; if (i > 4) i = 4;
        return B[i];
    }

    void process(const ProcessArgs&) override {
        // Latch pendings from gates + buttons; light = pending. Application happens in
        // the expander manager at phrase boundary / unlock (audio-side, same thread).
        for (int row = 0; row < 8; ++row) {
            // BRACES ARE LOAD-BEARING: without them only `armed` is guarded and blk is
            // re-read EVERY SAMPLE, which silently defeats the whole §14a latch.
            auto latch = [&]() {
                pendingRow[row].armed = true;
                pendingRow[row].blk   = MonsoonChangeAlleyExpander::blockFromKnob(
                                            params[CA::BLOCK_KNOB_START + row].getValue());
            };
            if (gateTrig[row].process(inputs[CA::TRIG_IN_START + row].getVoltage(), 0.1f, 1.f))
                latch();
            if (btnTrig[row].process(params[CA::TRIG_BTN_START + row].getValue() > 0.5f))
                latch();
            lights[CA::PENDING_LIGHT_START + row].setBrightness(pendingRow[row].armed ? 1.f : 0.f);
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
struct MonsoonChangeAlleyExpanderWidget : ModuleWidget {

    // Matrix geometry in mm (must match gen_change_alley.py exactly — 29HP kit layout)
    static constexpr float PW_MM    = 29.f * 5.08f;
    static constexpr float PH_MM    = 128.5f;
    static constexpr float GUTTER_T =  9.0f;
    static constexpr float GUTTER_B =  8.0f;
    static constexpr float MY_MM    = GUTTER_T + 8.0f;
    static constexpr float AVAIL_H  = PH_MM - MY_MM - GUTTER_B - 2.0f;
    static constexpr float CELL_H   = AVAIL_H / CA::N_VOICES;
    static constexpr float CELL_W   = CELL_H;
    static constexpr float MW_MM    = CELL_W * CA::N_VOICES;
    static constexpr float MH_MM    = CELL_H * CA::N_VOICES;
    static constexpr float MX_MM    = PW_MM - 4.0f - MW_MM;                  // grid claims the RIGHT
    // Control column (generator CTRL_*)
    static constexpr float CTRL_X_JACK = 6.0f,  CTRL_X_KNOB = 15.5f,
                           CTRL_X_BTN  = 24.0f, CTRL_X_LED  = 29.5f;
    static constexpr float CTRL_TOP    = MY_MM + 4.5f;
    static constexpr float CTRL_ROW_H  = 9.0f;
    static constexpr float GROUP_GAP   = 6.8f;
    static float ctrlRowY(int row) {
        return CTRL_TOP + (row/2)*(2.f*CTRL_ROW_H + GROUP_GAP) + (row%2)*CTRL_ROW_H + CTRL_ROW_H*0.5f;
    }

    // Cell centre in px (rack mm2px)
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

    MonsoonChangeAlleyExpanderWidget(MonsoonChangeAlleyExpander* module) {
        setModule(module);
        std::string dark  = asset::plugin(pluginInstance, "res/panels/ChangeAlley_panel_dark.svg");
        std::string light = asset::plugin(pluginInstance, "res/panels/ChangeAlley_panel_light.svg");
        panelSvgDark  = APP->window->loadSvg(dark);
        panelSvgLight = APP->window->loadSvg(light);
        setPanel(Svg::load(dark));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5,    1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 1.5, 1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(1.5,    PH_MM - 1.5))));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(PW_MM - 1.5, PH_MM - 1.5))));

        // Restructure control column: 8 rows of [block knob | gate jack | button | light]
        if (true) for (int row = 0; row < 8; ++row) {
            const float y = ctrlRowY(row);
            auto* k = createParamCentered<Trimpot>(mm2px(Vec(CTRL_X_KNOB, y)), module,
                                                   CA::BLOCK_KNOB_START + row);
            k->getParamQuantity() ? (void)(k->getParamQuantity()->snapEnabled = true) : (void)0;
            addParam(k);
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(CTRL_X_JACK, y)), module,
                                                     CA::TRIG_IN_START + row));
            addParam(createParamCentered<TL1105>(mm2px(Vec(CTRL_X_BTN, y)), module,
                                                 CA::TRIG_BTN_START + row));
            addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(CTRL_X_LED, y)), module,
                                                               CA::PENDING_LIGHT_START + row));
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
        MonsoonChangeAlleyExpander* module;
        int hoverRow = -1, hoverCol = -1;   // XILS crosshair target (-1 = none)
        PinOverlay(MonsoonChangeAlleyExpander* m) : module(m) {}

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
                    // Transform group labels in the gaps above each pair (room per Rodney)
                    static constexpr const char* TN[4] = {"COLLAPSE","ROTATE","SCATTER","REFLECT"};
                    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
                    nvgFontSize(vg, mm2px(Vec(2.7f,0)).x);
                    nvgFillColor(vg, inkdim);
                    for (int t2 = 0; t2 < 4; ++t2) {
                        float gy = mm2px(Vec(0, ctrlRowY(t2*2) - CTRL_ROW_H*0.5f - 1.4f)).y;
                        nvgText(vg, mm2px(Vec(CTRL_X_JACK - 3.5f, 0)).x, gy, TN[t2], NULL);
                    }
                    // Title + legend
                    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                    nvgFontSize(vg, mm2px(Vec(3.6f,0)).x);
                    nvgFillColor(vg, ink);
                    nvgText(vg, box.size.x * 0.5f, mm2px(Vec(0,6.0f)).y, "CHANGE ALLEY", NULL);
                    float legY = mm2px(Vec(0, MY_MM + MH_MM + 4.5f)).y;
                    float legX = mm2px(Vec(MX_MM,0)).x;
                    // Legend swatches sit on a small DARK CHIP in both themes: the white
                    // rhythm pin would be invisible on the light body, and showing the pins
                    // on grid-coloured ground is also semantically right (that is where
                    // they live). Text stays on the body, so it uses theme ink.
                    {
                        float chipX = legX, chipY = legY - 5.f;
                        float chipW = mm2px(Vec(24.5f,0)).x, chipH = 10.f;
                        nvgBeginPath(vg); nvgRoundedRect(vg, chipX, chipY, chipW, chipH, 2.f);
                        nvgFillColor(vg, nvgRGB(0x0b,0x0c,0x0d)); nvgFill(vg);
                    }
                    nvgBeginPath(vg); nvgCircle(vg, legX + 6.f, legY, 3.2f);
                    nvgFillColor(vg, nvgRGB(0xf0,0xf0,0xee)); nvgFill(vg);
                    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                    nvgFontSize(vg, mm2px(Vec(2.2f,0)).x);
                    nvgFillColor(vg, nvgRGBAf(0.94f,0.94f,0.93f,0.85f));   // on the chip
                    nvgText(vg, legX + 12.f, legY, "rhythm", NULL);
                    nvgBeginPath(vg); nvgCircle(vg, legX + mm2px(Vec(15.5f,0)).x, legY, 3.2f);
                    nvgFillColor(vg, nvgRGB(0xd4,0x00,0x1a)); nvgFill(vg);
                    nvgFillColor(vg, nvgRGBAf(0.94f,0.94f,0.93f,0.85f));   // on the chip
                    nvgText(vg, legX + mm2px(Vec(15.5f,0)).x + 6.f, legY, "melody", NULL);
                    nvgFillColor(vg, inkdim);                              // back on the body
                    nvgText(vg, legX + mm2px(Vec(26.5f,0)).x, legY, "right-click / ctrl-click", NULL);

                    // ── Connect indicator: a small state dot to the RIGHT of the SVG
                    //    logo (which draws the wordmark itself). BRIGHT red w/ halo =
                    //    connected + claimed; HOLLOW = not. No wordmark here — the panel
                    //    SVG embeds the real dot.modular logo. ──
                    {
                        bool connected = module && redDot::isConnectedAndClaimed(module);
                        float mx = box.size.x * 0.5f + mm2px(Vec(18.f,0)).x;
                        float myv = mm2px(Vec(0, PH_MM - 5.5f)).y;
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
            // Reads Change Alley's OWN tkHighlight POD, which the expander manager fills
            // from the attached Temasek module. No Temasek type is referenced here, so the
            // headers stay acyclic.
            if (module) {
                const int active = std::max(1, poly + 1);
                for (int hr = 0; hr < MonsoonChangeAlleyExpander::TK_HL_ROWS; ++hr) {
                    const auto& h = module->tkHighlight[hr];
                    if (!h.armed) continue;
                    NVGcolor hcol = (h.type == 0) ? nvgRGBAf(0.95f,0.95f,0.94f,0.55f)
                                                  : nvgRGBAf(0.83f,0.f,0.10f,0.55f);
                    const float sw = mm2px(Vec(0.45f,0)).x;
                    const float hw = mm2px(Vec(CELL_W * 0.5f, 0)).x;
                    const float hh = mm2px(Vec(0, CELL_H * 0.5f)).y;
                    if (h.isInter) {
                        Vec tl = cellCentre(0, 0), br = cellCentre(active-1, active-1);
                        nvgBeginPath(vg);
                        nvgRect(vg, tl.x - hw - sw, tl.y - hh - sw,
                                (br.x + hw + sw) - (tl.x - hw - sw),
                                (br.y + hh + sw) - (tl.y - hh - sw));
                        nvgStrokeColor(vg, hcol); nvgStrokeWidth(vg, sw*1.5f); nvgStroke(vg);
                    } else {
                        const int b = std::max(1, h.blk);
                        for (int blk = 0; blk * b < active; ++blk) {
                            const int b0 = blk * b;
                            const int b1 = std::min(b0 + b, active) - 1;
                            Vec tl = cellCentre(b0, b0), br = cellCentre(b1, b1);
                            nvgBeginPath(vg);
                            nvgRect(vg, tl.x - hw, tl.y - hh,
                                    (br.x + hw) - (tl.x - hw), (br.y + hh) - (tl.y - hh));
                            nvgStrokeColor(vg, hcol); nvgStrokeWidth(vg, sw); nvgStroke(vg);
                        }
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
            TransparentWidget::onButton(e);
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

    void step() override {
        ModuleWidget::step();
        if (!module) return;
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
