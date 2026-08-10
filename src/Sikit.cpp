#include <rack.hpp>
#include <string>
#include "Sikit.hpp"
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/WrappingMenuLabel.hpp"   // wrap a long .scl description in the context menu
#include "tuning/ScalaFile.hpp"
#include <osdialog.h>

using namespace rack;
using namespace SikitIds;

// ── Module process(): enforce root-lock, then publish cents[] into the shared TuningTable when
// this Sikit is the claimed tuning source. Control-rate cost is trivial (12 param reads + copy);
// no allocation, no file IO. The claim itself is resolved by Monsoon::updateExpanderPointers.
void Sikit::process(const ProcessArgs&) {
    // Root (degree 0) locked at 0 cents — belt-and-braces beside the UI hiding the knob. Guards
    // against preset/.scl writes trying to move it.
    params[CENTS_PARAM_0].setValue(0.f);

    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    if (!mon) return;                       // standalone: nothing to publish (ConnectMark greys)
    if (!mon->claimAsTuningSource(this)) return;   // not the claimant: don't write (loser greys)

    // Write cents[] into the shared table. ONLY cents — weight[] stays with Monsoon's scale system.
    dotModular::TuningTable& tt = mon->getTuningTable();
    tt.N = SikitIds::N_DEGREES;             // 12 (Phase 1)
    for (int i = 0; i < SikitIds::N_DEGREES; ++i)
        tt.cents[i] = params[CENTS_PARAM_0 + i].getValue();
    // Recompute the byte-identical fast-path flag: at equal-division defaults this stays true, so a
    // default Sikit reproduces 12-TET exactly (Step-I regression guarantee). Any detune flips it.
    tt.recomputeDefaultFlag();
}

// ── Widget-drawn labels ────────────────────────────────────────────────────────────────────────
// nanosvg (Rack's panel loader) does NOT render SVG <text>, so all on-panel Sikit text — wordmark,
// per-degree note names, and the locked-root "0" — is drawn here in the widget layer. The panel SVG
// supplies only geometry + named markers (wordmark, cell_<i>). The loaded-.scl name is menu-only.
struct SikitLabels : Widget {
    Sikit* module = nullptr;
    // Positions (px, panel-local) resolved once from the panel markers by the owning widget.
    Vec wordmarkPos;
    Vec cellPos[SikitIds::N_DEGREES];
    float cellR = 0.f;                      // knob radius (px) for label offset

    std::shared_ptr<window::Font> font_;
    std::shared_ptr<window::Font> loadFont() {
        auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }

    void draw(const DrawArgs& args) override {
        Widget::draw(args);
        std::shared_ptr<window::Font> f = loadFont();
        if (!f) return;
        NVGcontext* vg = args.vg;
        const bool light = [&]{ Monsoon* m = module ? redDot::findMonsoonEitherSide(module) : nullptr;
                                return m && m->lightTheme; }();
        const NVGcolor ink = light ? nvgRGB(0x1a,0x1a,0x1a) : nvgRGB(0xf0,0xf0,0xf0);
        const NVGcolor sub = light ? nvgRGB(0x5a,0x64,0x70) : nvgRGB(0x8a,0x94,0xa0);
        const NVGcolor gold= light ? nvgRGB(0xb0,0x7d,0x00) : nvgRGB(0xc8,0x96,0x0c);
        nvgFontFaceId(vg, f->handle);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        // Wordmark
        nvgFontSize(vg, 15.f);
        nvgFillColor(vg, ink);
        nvgText(vg, wordmarkPos.x, wordmarkPos.y - 4.f, "Sikit", nullptr);
        nvgFontSize(vg, 6.5f);
        nvgFillColor(vg, sub);
        nvgText(vg, wordmarkPos.x, wordmarkPos.y + 7.f, "cents / degree", nullptr);

        // Per-degree note names (above each cell); root cell also gets a "0" + "root".
        nvgFontSize(vg, 8.f);
        for (int i = 0; i < SikitIds::N_DEGREES; ++i) {
            nvgFillColor(vg, ink);
            nvgText(vg, cellPos[i].x, cellPos[i].y - cellR - 5.f, SikitIds::noteName(i), nullptr);
            if (i == 0) {
                nvgFontSize(vg, 9.f);
                nvgFillColor(vg, gold);
                nvgText(vg, cellPos[i].x, cellPos[i].y, "0", nullptr);
                nvgFontSize(vg, 6.f);
                nvgFillColor(vg, sub);
                nvgText(vg, cellPos[i].x, cellPos[i].y + cellR + 5.f, "root", nullptr);
                nvgFontSize(vg, 8.f);
            }
        }
        // The loaded-.scl name is NOT drawn on-panel (too tight on 8HP) — it lives in the context
        // menu ("Loaded: …" line). See SikitWidget::appendContextMenu.
    }
};

// ── Widget ───────────────────────────────────────────────────────────────────────────────────
struct SikitWidget : ModuleWidget,
    dotModular::Compose<SikitWidget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    SikitLabels* labels = nullptr;
    int lastThemeLight = -1;

    SikitWidget(Sikit* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Sikit_panel_dark.svg";
        const char* lightPath = "res/panels/Sikit_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // 12 cents knobs bound to their panel markers. Degree 0 (root) is the locked plate on the
        // panel (no marker) so there is no interactive knob for it — the UI half of the root-lock
        // (process() clamps the value belt-and-braces).
        for (int i = 1; i < SikitIds::N_DEGREES; ++i)
            bindParam<Trimpot>("param_cents_" + std::to_string(i), CENTS_PARAM_0 + i);

        // Widget-drawn labels overlay (nanosvg can't render <text>). Full-panel widget; positions
        // are resolved from the panel markers.
        labels = new SikitLabels();
        labels->module = mod;
        labels->box.pos = Vec(0, 0);
        labels->box.size = box.size;
        if (auto* wm = findNamed("wordmark")) labels->wordmarkPos = centerOf(wm);
        for (int i = 0; i < SikitIds::N_DEGREES; ++i)
            if (auto* c = findNamed("cell_" + std::to_string(i))) labels->cellPos[i] = centerOf(c);
        labels->cellR = mm2px(3.0f);
        addChild(labels);

        // ConnectMark with a TUNING-SOURCE predicate: the mark lights only when THIS Sikit is the
        // claimed tuning source of a reachable Monsoon.
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
        Sikit* mod = dynamic_cast<Sikit*>(module);
        if (!mod) return;
        osdialog_filters* filters = osdialog_filters_parse("Scala Tuning:scl");
        char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
        osdialog_filters_free(filters);
        if (!path) return;                  // cancelled
        std::string pathStr(path);
        std::free(path);

        // Sikit: EXACTLY 12 degrees (it retunes Monsoon's fixed 12-degree system; no natural place
        // for a shorter/longer file). All-or-nothing per SCALA_FILE_AND_LOAD_UI.md.
        auto sf = dotModular::loadScala(pathStr,
            [](int n){ return n == 12; },
            "Sikit reads 12-note .scl files only. For non-12 tunings, use a Micro expander "
            "(a future release).");
        if (!sf.ok()) {
            osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, sf.errorMessage.c_str());
            return;                         // rejection/parse error: leave current state untouched
        }
        // Apply: Scala lists degrees 1..N; centsFromRoot[i] is degree (i+1). Root (param 0) stays 0.
        for (int i = 0; i < 12 && i < sf.degreeCount(); ++i) {
            int paramIdx = CENTS_PARAM_0 + (i + 1);
            if (paramIdx <= CENTS_PARAM_END)
                mod->params[paramIdx].setValue(sf.centsFromRoot[i]);
        }
        // Record a display name: prefer the .scl description, else the file stem (for the name band).
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
        if (auto* mod = dynamic_cast<Sikit*>(module); mod && !mod->loadedTuningName.empty())
            // A .scl description can be long; wrap it so it can't blow out the menu width
            // (SCALA_FILE_AND_LOAD_UI width fix — same widget used by Colonnades/Duo/Shophouse Micro).
            menu->addChild(redDot::makeWrappingMenuLabel("Loaded: " + mod->loadedTuningName));
        menu->addChild(createMenuItem("Load .scl...", "", [this]() { this->openScalaFilePicker(); }));
        menu->addChild(createMenuItem("Reset to 12-TET (equal division)", "", [this]() {
            if (auto* mod = dynamic_cast<Sikit*>(module)) {
                for (int i = 0; i < SikitIds::N_DEGREES; ++i)
                    mod->params[SikitIds::CENTS_PARAM_0 + i].setValue(Sikit::defaultCents(i));
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

Model* modelSikit = createModel<Sikit, SikitWidget>("Sikit");
