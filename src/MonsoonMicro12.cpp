#include <rack.hpp>
#include <string>
#include <cmath>
#include <cstdio>
#include "MonsoonMicro12.hpp"
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "tuning/ScalaFile.hpp"
#include <osdialog.h>

using namespace rack;
using namespace Micro12Ids;

// ── Module process(): enforce root cents-lock, then (when claimed) publish BOTH cents[] and
// weight[] into the shared TuningTable and flag maskAuthored so Monsoon reads its scale mask from
// the Micro instead of its own faders (Model A delegation). Control-rate-cheap; no alloc/IO.
void MonsoonMicro12::process(const ProcessArgs&) {
    // Root cents locked at 0 (Scalar rule) — belt-and-braces beside the UI (no root cents knob).
    params[CENTS_PARAM_0].setValue(0.f);

    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    if (!mon) return;                          // standalone: publish nothing (ConnectMark greys)
    if (!mon->claimAsTuningSource(this)) return;   // not the claimant: don't write (loser greys)

    dotModular::TuningTable& tt = mon->getTuningTable();
    tt.N = Micro12Ids::N_DEGREES;              // 12
    for (int i = 0; i < Micro12Ids::N_DEGREES; ++i) {
        tt.cents[i]  = params[CENTS_PARAM_0  + i].getValue();
        tt.weight[i] = params[WEIGHT_PARAM_0 + i].getValue();
    }
    tt.maskAuthored = true;                    // Micro owns the scale mask this block (Model A)
    tt.recomputeDefaultFlag();                 // cents fast-path (weight doesn't affect the cents map)

    // Fader FLASH lights: mirror Monsoon (Monsoon.cpp:920-935 → updateSemitoneFlashLights). The
    // degree that PLAYS lights its fader — read the host's per-semitone play brightness (max over
    // mono + active poly voices) and drive the RED sub-light (2*i+1), leaving green (2*i) at 0, the
    // same convention as Monsoon's SEMI_LED bank. Since the Micro owns weight[] now, "which degree
    // plays" is authored here, so its own fader is the right place to show it.
    for (int i = 0; i < Micro12Ids::N_DEGREES; ++i) {
        float b = mon->engine.gs.semiLedBrightness(i);
        for (int v = 0; v < mon->engine.numPolyVoices; ++v)
            b = std::max(b, mon->engine.voices[v].gs.semiLedBrightness(i));
        lights[WEIGHT_LED_START + 2*i + 0].setBrightness(0.f);   // green sub unused (match Monsoon)
        lights[WEIGHT_LED_START + 2*i + 1].setBrightness(b);     // red sub = play flash
    }
}

// ── ColonnadesLightSlider — lift of MonsoonLightSlider (MonsoonWidget.cpp:33) ───────────────────
// A weight fader that LIGHTS from the active-degree state and DIMS when the degree is disabled
// (weight == 0), reusing Monsoon's dim-out-of-scale idiom. The Micro owns weight[] now, so "disabled"
// = this fader is at (or near) zero. Display-only: reads the param, never writes.
template <typename TLightBase = RedLight>
struct ColonnadesLightSlider : VCVLightSlider<TLightBase> {
    bool degreeDisabled() {
        auto* pq = this->getParamQuantity();   // non-const (Rack API), so this method isn't const
        return pq && pq->getValue() <= 1e-4f;   // weight 0 == disabled degree
    }
    void draw(const widget::Widget::DrawArgs& args) override {
        const bool dimmed = degreeDisabled();
        if (dimmed) nvgGlobalAlpha(args.vg, 0.4f);   // dim in place (Monsoon's chosen default)
        VCVLightSlider<TLightBase>::draw(args);
        if (dimmed) nvgGlobalAlpha(args.vg, 1.0f);
    }
};

// ── Widget-drawn labels (nanosvg ignores <text>): wordmark + per-degree NUMBER 1..12 + subtitle. ──
struct Micro12Labels : Widget {
    MonsoonMicro12* module = nullptr;
    Vec wordmarkPos;
    Vec cellPos[Micro12Ids::N_DEGREES];   // note-name anchor per strip (above the cents knob)
    float labelDy = 0.f;

    std::shared_ptr<window::Font> loadFont() {
        auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }
    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        auto f = loadFont();
        if (!f) return;
        NVGcontext* vg = args.vg;
        const bool light = [&]{ Monsoon* m = module ? redDot::findMonsoonEitherSide(module) : nullptr;
                                return m && m->lightTheme; }();
        const NVGcolor ink = light ? nvgRGB(0x1a,0x1a,0x1a) : nvgRGB(0xf0,0xf0,0xf0);
        const NVGcolor sub = light ? nvgRGB(0x5a,0x64,0x70) : nvgRGB(0x8a,0x94,0xa0);
        nvgFontFaceId(vg, f->handle);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFontSize(vg, 15.f);
        nvgFillColor(vg, ink);
        nvgText(vg, wordmarkPos.x, wordmarkPos.y - 4.f, "Micro 12", nullptr);
        nvgFontSize(vg, 6.f);
        nvgFillColor(vg, sub);
        nvgText(vg, wordmarkPos.x, wordmarkPos.y + 6.f, "tuning + scale", nullptr);
        // Degree NUMBERS 1..12 (not note names — arbitrary tunings have no note names), matching
        // Monsoon's step-number strip style.
        nvgFontSize(vg, 8.f);
        nvgFillColor(vg, ink);
        for (int i = 0; i < Micro12Ids::N_DEGREES; ++i)
            nvgText(vg, cellPos[i].x, cellPos[i].y, std::to_string(i + 1).c_str(), nullptr);
    }
};

// ── Micro12CentsDisplay — one LED readout showing all 12 cents in fader position ────────────────
// A single 7-segment (DSEG) display in the band above the faders. Each degree's rounded cents value
// is drawn at its fader's X, staggered on two rows (even indices upper, odd lower) to parallel the
// cents-knob offsets below. Reads the module's cents params live (root shows 0). Amber-on-dark.
struct Micro12CentsDisplay : Widget {
    MonsoonMicro12* module = nullptr;
    float faderX[Micro12Ids::N_DEGREES] = {};   // panel-local X per degree (set by owner)
    float rowUpperY = 0.f, rowLowerY = 0.f;     // panel-local Y for even / odd degrees

    std::shared_ptr<window::Font> ledFont() {
        // DSEG 7-segment for the LED look; fall back to bold sans if unavailable.
        auto f = APP->window->loadFont(rack::asset::plugin(pluginInstance, "res/fonts/DSEG7ClassicMini-Bold.ttf"));
        if (!f) f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }
    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        auto f = ledFont();
        if (!f) return;
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, 8.5f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (int i = 0; i < Micro12Ids::N_DEGREES; ++i) {
            const float x = faderX[i];
            const float y = (i % 2 == 0) ? rowUpperY : rowLowerY;
            int cents = module
                ? (int)std::lround(module->params[Micro12Ids::CENTS_PARAM_0 + i].getValue())
                : (int)std::lround(MonsoonMicro12::defaultCents(i));
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", cents);
            // Off-segment ghost (DSEG '8's dimly) for the classic LED backing, then the lit value.
            if (f->handle) {
                nvgFillColor(vg, nvgRGBA(0x40, 0x18, 0x00, 0x60));   // dim amber ghost
                nvgText(vg, x, y, "888", nullptr);
            }
            nvgFillColor(vg, nvgRGB(0xff, 0x9a, 0x2a));             // lit amber
            nvgText(vg, x, y, buf, nullptr);
        }
    }
};

// ── Widget ───────────────────────────────────────────────────────────────────────────────────
struct MonsoonMicro12Widget : ModuleWidget,
    dotModular::Compose<MonsoonMicro12Widget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    Micro12Labels* labels = nullptr;
    int lastThemeLight = -1;

    MonsoonMicro12Widget(MonsoonMicro12* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/MonsoonMicro12_panel_dark.svg";
        const char* lightPath = "res/panels/MonsoonMicro12_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Per-degree strips: WEIGHT light-fader (all 12; GreenRedLight flashes red when the degree
        // plays — mirrors Monsoon SEMI_LED; also dimmed by ColonnadesLightSlider when weight==0) +
        // CENTS knob (degrees 1..11; root cents locked, no knob).
        for (int i = 0; i < Micro12Ids::N_DEGREES; ++i)
            bindLightParam<ColonnadesLightSlider<GreenRedLight>>(
                "param_weight_" + std::to_string(i), WEIGHT_PARAM_0 + i, WEIGHT_LED_START + 2*i);
        for (int i = 1; i < Micro12Ids::N_DEGREES; ++i)
            bindParam<Trimpot>("param_cents_" + std::to_string(i), CENTS_PARAM_0 + i);

        // Cents LED display — one readout over the whole above-faders band, values at each fader's X,
        // staggered two rows (even upper / odd lower) to parallel the cents-knob offsets below.
        if (auto* dispShape = findNamed("cents_display")) {
            auto* disp = new Micro12CentsDisplay();
            disp->module = mod;
            Rect db = boundsOf(dispShape);
            disp->box.pos = db.pos;
            disp->box.size = db.size;
            for (int i = 0; i < Micro12Ids::N_DEGREES; ++i) {
                // fader centre X (panel-local), rebased into the display's local coords
                Vec fc = centerOf(findNamed("param_weight_" + std::to_string(i)));
                disp->faderX[i] = fc.x - db.pos.x;
            }
            disp->rowUpperY = db.size.y * 0.32f;
            disp->rowLowerY = db.size.y * 0.72f;
            addChild(disp);
        }

        // Widget-drawn labels overlay.
        labels = new Micro12Labels();
        labels->module = mod;
        labels->box.pos = Vec(0, 0);
        labels->box.size = box.size;
        if (auto* wm = findNamed("wordmark")) labels->wordmarkPos = centerOf(wm);
        for (int i = 0; i < Micro12Ids::N_DEGREES; ++i)
            if (auto* c = findNamed("notelabel_" + std::to_string(i))) labels->cellPos[i] = centerOf(c);
        addChild(labels);

        // ConnectMark: lights only when THIS Micro is the claimed tuning source.
        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            rack::Module* self = module;
            connectMark->connected = [self]() {
                Monsoon* m = self ? redDot::findMonsoonEitherSide(self) : nullptr;
                return m && m->getTuningSourceExpander() == self;
            };
            addChild(connectMark);
        }
    }

    void openScalaFilePicker() {
        MonsoonMicro12* mod = dynamic_cast<MonsoonMicro12*>(module);
        if (!mod) return;
        osdialog_filters* filters = osdialog_filters_parse("Scala Tuning:scl");
        char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
        osdialog_filters_free(filters);
        if (!path) return;
        std::string pathStr(path);
        std::free(path);

        // Micro-12: UP TO 12 degrees (a shorter .scl is meaningful — 7-note major, 5-note pentatonic —
        // it fills the first N degrees and disables the rest). See SCALA_FILE_AND_LOAD_UI.md.
        auto sf = dotModular::loadScala(pathStr,
            [](int n){ return n >= 1 && n <= 12; },
            "Micro-12 supports up to 12 tones per octave. For more, use Micro-24 (a future release).");
        if (!sf.ok()) {
            osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, sf.errorMessage.c_str());
            return;
        }
        const int n = sf.degreeCount();
        // Scala lists degrees 1..N (root implicit 0). Fill cents[1..n]; enable those degrees (weight 1),
        // disable the rest (weight 0). Root (degree 0) stays 0 cents and enabled.
        mod->params[CENTS_PARAM_0].setValue(0.f);
        mod->params[WEIGHT_PARAM_0].setValue(1.f);
        // Scala convention: the LAST listed pitch is the PERIOD (the octave, usually 1200¢) — it is
        // NOT a within-octave degree, it's where the scale repeats (same pitch class as the root an
        // octave up). So the within-octave NON-ROOT degrees are the first (n-1) listed pitches; the
        // period is dropped (the octave is implicit in 1V/oct playback). A 7-note major .scl lists 6
        // steps + the octave → root + 6 = 7 enabled degrees (C D E F G A B), NOT 8. (This mirrors
        // Sikit, whose 12-only loader naturally drops the 12th/period entry.)
        const int nonRoot = (n > 0) ? (n - 1) : 0;      // listed pitches minus the period
        for (int deg = 1; deg <= 11; ++deg) {
            int centsIdx = CENTS_PARAM_0 + deg;
            int wtIdx    = WEIGHT_PARAM_0 + deg;
            if (deg <= nonRoot) {
                mod->params[centsIdx].setValue(sf.centsFromRoot[deg - 1]);   // degree d ← pitch d-1
                mod->params[wtIdx].setValue(1.f);        // enabled
            } else {
                mod->params[wtIdx].setValue(0.f);        // beyond the scale's degrees: disabled
            }
        }
        std::string nm = sf.description;
        if (nm.empty()) {
            size_t slash = pathStr.find_last_of("/\\");
            std::string base = (slash == std::string::npos) ? pathStr : pathStr.substr(slash + 1);
            size_t dot = base.find_last_of('.');
            nm = (dot == std::string::npos) ? base : base.substr(0, dot);
        }
        mod->loadedTuningName = nm;
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        menu->addChild(new MenuSeparator);
        if (auto* mod = dynamic_cast<MonsoonMicro12*>(module); mod && !mod->loadedTuningName.empty())
            menu->addChild(createMenuLabel("Loaded: " + mod->loadedTuningName));
        menu->addChild(createMenuItem("Load .scl...", "", [this]() { this->openScalaFilePicker(); }));
        menu->addChild(createMenuItem("Reset to 12-TET (all degrees, equal division)", "", [this]() {
            if (auto* mod = dynamic_cast<MonsoonMicro12*>(module)) {
                for (int i = 0; i < Micro12Ids::N_DEGREES; ++i) {
                    mod->params[Micro12Ids::CENTS_PARAM_0  + i].setValue(MonsoonMicro12::defaultCents(i));
                    mod->params[Micro12Ids::WEIGHT_PARAM_0 + i].setValue(1.f);
                }
                mod->loadedTuningName.clear();
            }
        }));
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* m = redDot::findMonsoonEitherSide(module);
        int wantLight = (m && m->lightTheme) ? 1 : 0;
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
};

Model* modelMonsoonMicro12 = createModel<MonsoonMicro12, MonsoonMicro12Widget>("MonsoonMicro12");
