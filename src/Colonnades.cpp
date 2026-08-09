#include <rack.hpp>
#include <string>
#include <cmath>
#include <cstdio>
#include "Colonnades.hpp"
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "tuning/ScalaFile.hpp"
#include <osdialog.h>
#include "tuning/TuningPreset.hpp"

using namespace rack;
using namespace ColonnadesIds;

// ── Module process(): enforce root cents-lock, then (when claimed) publish BOTH cents[] and
// weight[] into the shared TuningTable and flag maskAuthored so Monsoon reads its scale mask from
// the Colonnades instead of its own faders (Model A delegation). Control-rate-cheap; no alloc/IO.
void Colonnades::process(const ProcessArgs&) {
    // Root cents locked at 0 (Scalar rule) — belt-and-braces beside the UI (no root cents knob).
    params[CENTS_PARAM_0].setValue(0.f);

    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    if (!mon) return;                          // standalone: publish nothing (ConnectMark greys)
    if (!mon->claimAsTuningSource(this)) return;   // not the claimant: don't write (loser greys)

    dotModular::TuningTable& tt = mon->getTuningTable();
    tt.N = ColonnadesIds::N_DEGREES;           // 12
    for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i) {
        tt.cents[i]  = params[CENTS_PARAM_0  + i].getValue();
        tt.weight[i] = params[WEIGHT_PARAM_0 + i].getValue();
    }
    tt.maskAuthored = true;                    // Colonnades owns the scale mask this block (Model A)
    tt.recomputeDefaultFlag();                 // cents fast-path (weight doesn't affect the cents map)

    // Fader FLASH lights: mirror Monsoon (Monsoon.cpp:920-935 → updateSemitoneFlashLights). The
    // degree that PLAYS lights its fader — read the host's per-semitone play brightness (max over
    // mono + active poly voices) and drive the RED sub-light (2*i+1), leaving green (2*i) at 0, the
    // same convention as Monsoon's SEMI_LED bank. Since Colonnades owns weight[] now, "which degree
    // plays" is authored here, so its own fader is the right place to show it.
    for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i) {
        float b = mon->engine.gs.semiLedBrightness(i);
        for (int v = 0; v < mon->engine.numPolyVoices; ++v)
            b = std::max(b, mon->engine.voices[v].gs.semiLedBrightness(i));
        lights[WEIGHT_LED_START + 2*i + 0].setBrightness(0.f);   // green sub unused (match Monsoon)
        lights[WEIGHT_LED_START + 2*i + 1].setBrightness(b);     // red sub = play flash
    }
}

// ── ColonnadesLightSlider — lift of MonsoonLightSlider (MonsoonWidget.cpp:33) ───────────────────
// A weight fader that LIGHTS from the active-degree state and DIMS when the degree is disabled
// (weight == 0), reusing Monsoon's dim-out-of-scale idiom. Colonnades owns weight[] now, so
// "disabled" = this fader is at (or near) zero. Display-only: reads the param, never writes.
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
struct ColonnadesLabels : Widget {
    Colonnades* module = nullptr;
    Vec wordmarkPos;
    Vec cellPos[ColonnadesIds::N_DEGREES];   // note-name anchor per strip (above the cents knob)
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
        nvgText(vg, wordmarkPos.x, wordmarkPos.y - 4.f, "Colonnades", nullptr);
        nvgFontSize(vg, 6.f);
        nvgFillColor(vg, sub);
        nvgText(vg, wordmarkPos.x, wordmarkPos.y + 6.f, "tuning + scale", nullptr);
        // Degree NUMBERS 1..12 (not note names — arbitrary tunings have no note names), matching
        // Monsoon's step-number strip style.
        nvgFontSize(vg, 8.f);
        nvgFillColor(vg, ink);
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i)
            nvgText(vg, cellPos[i].x, cellPos[i].y, std::to_string(i + 1).c_str(), nullptr);
    }
};

// ── ColonnadesCentsDisplay — SCALAR'S TWO-ROW CELL GRID, lower row offset 50% in X (ROUND 7) ─────
// One 7-segment (DSEG) readout band above the faders, laid out as Scalar's degree grid: a normal
// rectangular two-row cell grid (thin dividers, contiguous cells tiling edge-to-edge within each
// row) with the LOWER ROW shifted half a cell horizontally so each cell centres over its staggered
// knob below. Upper row = even degrees, lower row = odd degrees. The horizontal divider between the
// rows appears to STEP because the rows are half-column offset (that stepping is the whole "stagger"
// — it is NOT a drawn waveform/zigzag line). Each cell shows its degree's cents (2 decimals, amber-
// on-dark; root shows 0.00). The owner sets faderX[] + the two row Ys + cell half-extents.
struct ColonnadesCentsDisplay : Widget {
    Colonnades* module = nullptr;
    float faderX[ColonnadesIds::N_DEGREES] = {};   // per-degree centre X (display-local)
    float rowUpperY = 0.f, rowLowerY = 0.f;     // even-row / odd-row centre Y (display-local)
    float cellHalfW = 0.f, cellHalfH = 0.f;     // cell half-extents (px)

    std::shared_ptr<window::Font> ledFont() {
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

        // SCALAR'S GRID, lower row offset 50% (round 7): two rows of contiguous rectangular cells.
        // cellHalfW == the full half-pitch of a same-row column, so cells in a row tile edge-to-edge
        // (no gaps, no floating boxes). Rows are vertically adjacent (upper cell bottom == lower cell
        // top). Because odd faders sit half-a-column offset from even faders, the lower row is 50%
        // offset in X → the shared horizontal divider steps. Draw each cell as a stroked rectangle;
        // adjacent same-row cells share a vertical edge → reads as one clean grid.
        nvgStrokeColor(vg, nvgRGBA(0x8a, 0x94, 0xa0, 0x55));
        nvgStrokeWidth(vg, 0.8f);
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i) {
            const float cx = faderX[i];
            const float cy = (i % 2 == 0) ? rowUpperY : rowLowerY;
            nvgBeginPath(vg);
            nvgRect(vg, cx - cellHalfW, cy - cellHalfH, 2*cellHalfW, 2*cellHalfH);   // one grid cell
            nvgStroke(vg);
        }

        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, 11.5f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i) {
            const float cx = faderX[i];
            const float cy = (i % 2 == 0) ? rowUpperY : rowLowerY;
            double cents = module
                ? (double)module->params[ColonnadesIds::CENTS_PARAM_0 + i].getValue()
                : (double)Colonnades::defaultCents(i);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.2f", cents);
            nvgFillColor(vg, nvgRGBA(0x40, 0x18, 0x00, 0x55));   // dim amber ghost (off-segments)
            nvgText(vg, cx, cy, "888.88", nullptr);
            nvgFillColor(vg, nvgRGB(0xff, 0x9a, 0x2a));          // lit amber
            nvgText(vg, cx, cy, buf, nullptr);
        }
    }
};

// ── ColonnadesNotesControl — zero-state NOTES readout + bulk-set (ROUND 5, Model B) ─────────────
// Holds NO independent state. It DISPLAYS the live active-degree count (number of non-zero weights),
// and when DRAGGED vertically it bulk-sets the first-N enable pattern (degrees 1..N weight=1, the
// rest 0). So it can never disagree with the faders — it IS the mask's cardinality, shown + actuated.
// A small DSEG readout, styled like the cents display.
struct ColonnadesNotesControl : OpaqueWidget {
    Colonnades* module = nullptr;
    float accum = 0.f;   // drag accumulator (px → count steps), transient during a drag only

    static int activeCount(Colonnades* m) {
        if (!m) return ColonnadesIds::N_DEGREES;
        int n = 0;
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i)
            if (m->params[ColonnadesIds::WEIGHT_PARAM_0 + i].getValue() > 1e-4f) ++n;
        return n;
    }
    // Bulk-set: enable degrees 1..N (weight 1), disable N+1..11. Degree 0 (root) always enabled. N is
    // clamped 1..12. Preserves each degree's CENTS (only weights change) — mask cardinality gesture.
    static void setActiveCount(Colonnades* m, int N) {
        if (!m) return;
        N = clamp(N, 1, ColonnadesIds::N_DEGREES);
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i)
            m->params[ColonnadesIds::WEIGHT_PARAM_0 + i].setValue(i < N ? 1.f : 0.f);
    }

    std::shared_ptr<window::Font> ledFont() {
        auto f = APP->window->loadFont(rack::asset::plugin(pluginInstance, "res/fonts/DSEG7ClassicMini-Bold.ttf"));
        if (!f) f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }
    void draw(const DrawArgs& args) override {
        auto f = ledFont();
        if (!f) return;
        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, 10.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", activeCount(module));
        nvgFillColor(vg, nvgRGBA(0x40, 0x18, 0x00, 0x55));
        nvgText(vg, cx, cy, "88", nullptr);
        nvgFillColor(vg, nvgRGB(0xff, 0x9a, 0x2a));
        nvgText(vg, cx, cy, buf, nullptr);
    }
    void onDragStart(const event::DragStart&) override { accum = 0.f; }
    void onDragMove(const event::DragMove& e) override {
        if (!module) return;
        // Up = more notes. ~10px per step. Bulk-set from the accumulated delta off the current count.
        accum -= e.mouseDelta.y;
        int step = (int)(accum / 10.f);
        if (step != 0) {
            setActiveCount(module, activeCount(module) + step);
            accum -= step * 10.f;
        }
    }
};

// ── Widget ───────────────────────────────────────────────────────────────────────────────────
struct ColonnadesWidget : ModuleWidget,
    dotModular::Compose<ColonnadesWidget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    ColonnadesLabels* labels = nullptr;
    int lastThemeLight = -1;

    ColonnadesWidget(Colonnades* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Colonnades_panel_dark.svg";
        const char* lightPath = "res/panels/Colonnades_panel_light.svg";
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
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i)
            bindLightParam<ColonnadesLightSlider<GreenRedLight>>(
                "param_weight_" + std::to_string(i), WEIGHT_PARAM_0 + i, WEIGHT_LED_START + 2*i);
        for (int i = 1; i < ColonnadesIds::N_DEGREES; ++i)
            bindParam<Trimpot>("param_cents_" + std::to_string(i), CENTS_PARAM_0 + i);

        // Cents LED display — Scalar's two-row grid, lower row offset 50% (round 7). Capture each
        // fader's X (display-local) so cells centre over their knobs; the two row Ys + cell half-
        // extents make the cells CONTIGUOUS (half-pitch wide) and the rows adjacent (share a divider).
        if (auto* dispShape = findNamed("cents_display")) {
            auto* disp = new ColonnadesCentsDisplay();
            disp->module = mod;
            Rect db = boundsOf(dispShape);
            disp->box.pos  = db.pos;
            disp->box.size = db.size;
            for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i)
                if (auto* w = findNamed("param_weight_" + std::to_string(i)))
                    disp->faderX[i] = centerOf(w).x - db.pos.x;   // fader/knob column X, display-local
            // Contiguous cells: half-width = one full fader pitch (9mm) so same-row cells (which are
            // 2 pitches apart) tile edge-to-edge; the lower row is naturally offset 50% because odd
            // faders sit half a pitch from even faders. Two rows fill the band and share a divider.
            disp->cellHalfW = mm2px(9.0f);
            disp->cellHalfH = db.size.y * 0.25f;
            disp->rowUpperY = db.size.y * 0.25f;   // even degrees (upper row)
            disp->rowLowerY = db.size.y * 0.75f;   // odd degrees (lower row, +half-column in X)
            addChild(disp);
        }

        // NOTES control — zero-state readout/bulk-set of active-degree count (round 5, Model B).
        if (auto* nShape = findNamed("notes_ctrl")) {
            auto* nc = new ColonnadesNotesControl();
            nc->module = mod;
            Rect nb = boundsOf(nShape);
            nc->box.pos  = nb.pos;
            nc->box.size = nb.size;
            addChild(nc);
        }

        // Widget-drawn labels overlay.
        labels = new ColonnadesLabels();
        labels->module = mod;
        labels->box.pos = Vec(0, 0);
        labels->box.size = box.size;
        if (auto* wm = findNamed("wordmark")) labels->wordmarkPos = centerOf(wm);
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i)
            if (auto* c = findNamed("notelabel_" + std::to_string(i))) labels->cellPos[i] = centerOf(c);
        addChild(labels);

        // ConnectMark: lights only when THIS Colonnades is the claimed tuning source.
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
        Colonnades* mod = dynamic_cast<Colonnades*>(module);
        if (!mod) return;
        osdialog_filters* filters = osdialog_filters_parse("Scala Tuning:scl");
        char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
        osdialog_filters_free(filters);
        if (!path) return;
        std::string pathStr(path);
        std::free(path);

        // Colonnades (Micro-12): UP TO 12 degrees (a shorter .scl is meaningful — 7-note major, 5-note
        // pentatonic — it fills the first N degrees and disables the rest). See SCALA_FILE_AND_LOAD_UI.md.
        auto sf = dotModular::loadScala(pathStr,
            [](int n){ return n >= 1 && n <= 12; },
            "Colonnades supports up to 12 tones per octave. For more, use Colonnades Duo (a future release).");
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

    // Export the current tuning to a standard .scl. Writes the ENABLED degrees' cents (root implicit,
    // ascending) then the octave 1200 as the Scala period — the inverse of the loader (which drops the
    // period + disables beyond-N degrees), so load→save round-trips the enabled scale.
    void openScalaSavePicker() {
        Colonnades* mod = dynamic_cast<Colonnades*>(module);
        if (!mod) return;
        osdialog_filters* filters = osdialog_filters_parse("Scala Tuning:scl");
        char* path = osdialog_file(OSDIALOG_SAVE, nullptr, "tuning.scl", filters);
        osdialog_filters_free(filters);
        if (!path) return;
        std::string pathStr(path);
        std::free(path);
        if (pathStr.size() < 4 || pathStr.substr(pathStr.size() - 4) != ".scl")
            pathStr += ".scl";

        std::vector<float> cents;
        for (int deg = 1; deg < ColonnadesIds::N_DEGREES; ++deg)
            if (mod->params[ColonnadesIds::WEIGHT_PARAM_0 + deg].getValue() > 1e-4f)
                cents.push_back(mod->params[ColonnadesIds::CENTS_PARAM_0 + deg].getValue());
        cents.push_back(1200.f);   // period (octave) — Scala convention

        std::string desc = mod->loadedTuningName.empty() ? "Colonnades tuning" : mod->loadedTuningName;
        if (!dotModular::saveScala(pathStr, cents, desc))
            osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Could not write file: " + pathStr).c_str());
    }

    // ── .dmtune (lossless full-state) ────────────────────────────────────────────────────────────
    void openDmtuneLoadPicker() {
        Colonnades* mod = dynamic_cast<Colonnades*>(module);
        if (!mod) return;
        osdialog_filters* filters = osdialog_filters_parse("dot.modular Tuning:dmtune");
        char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
        osdialog_filters_free(filters);
        if (!path) return;
        std::string pathStr(path);
        std::free(path);
        auto p = dotModular::loadTuningPreset(pathStr,
            [](int n){ return n == ColonnadesIds::N_DEGREES; },
            "This .dmtune is not a 12-degree tuning. Load it into the matching Colonnades.");
        if (!p.ok()) {
            osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, p.errorMessage.c_str());
            return;
        }
        // Lossless: restore ALL cents + ALL weights. Root cents forced to 0 (belt-and-braces).
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i) {
            mod->params[ColonnadesIds::CENTS_PARAM_0  + i].setValue(i == 0 ? 0.f : p.cents[i]);
            mod->params[ColonnadesIds::WEIGHT_PARAM_0 + i].setValue(p.weight[i]);
        }
        mod->loadedTuningName = !p.name.empty() ? p.name : std::string();
    }
    void openDmtuneSavePicker() {
        Colonnades* mod = dynamic_cast<Colonnades*>(module);
        if (!mod) return;
        osdialog_filters* filters = osdialog_filters_parse("dot.modular Tuning:dmtune");
        char* path = osdialog_file(OSDIALOG_SAVE, nullptr, "tuning.dmtune", filters);
        osdialog_filters_free(filters);
        if (!path) return;
        std::string pathStr(path);
        std::free(path);
        if (pathStr.size() < 7 || pathStr.substr(pathStr.size() - 7) != ".dmtune")
            pathStr += ".dmtune";
        dotModular::TuningPreset p;
        p.n = ColonnadesIds::N_DEGREES;
        for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i) {
            p.cents[i]  = mod->params[ColonnadesIds::CENTS_PARAM_0  + i].getValue();
            p.weight[i] = mod->params[ColonnadesIds::WEIGHT_PARAM_0 + i].getValue();
        }
        p.name = mod->loadedTuningName;
        if (!dotModular::saveTuningPreset(pathStr, p))
            osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, ("Could not write file: " + pathStr).c_str());
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        menu->addChild(new MenuSeparator);
        if (auto* mod = dynamic_cast<Colonnades*>(module); mod && !mod->loadedTuningName.empty())
            menu->addChild(createMenuLabel("Loaded: " + mod->loadedTuningName));
        menu->addChild(createMenuItem("Load .scl...", "", [this]() { this->openScalaFilePicker(); }));
        menu->addChild(createMenuItem("Save .scl...", "", [this]() { this->openScalaSavePicker(); }));
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Load .dmtune... (full state)", "", [this]() { this->openDmtuneLoadPicker(); }));
        menu->addChild(createMenuItem("Save .dmtune... (full state)", "", [this]() { this->openDmtuneSavePicker(); }));
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reset to 12-TET (all degrees, equal division)", "", [this]() {
            if (auto* mod = dynamic_cast<Colonnades*>(module)) {
                for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i) {
                    mod->params[ColonnadesIds::CENTS_PARAM_0  + i].setValue(Colonnades::defaultCents(i));
                    mod->params[ColonnadesIds::WEIGHT_PARAM_0 + i].setValue(1.f);
                }
                mod->loadedTuningName.clear();
            }
        }));
        // Set equal-tempered across the ACTIVE-N degrees (round 5b): N-EDO seed for creating a tuning.
        // N = current active count; cents[i] = i*(1200/N) for the active degrees (root stays 0).
        menu->addChild(createMenuItem("Set equal-tempered (N divisions)", "", [this]() {
            if (auto* mod = dynamic_cast<Colonnades*>(module)) {
                int N = ColonnadesNotesControl::activeCount(mod);
                if (N < 1) N = 1;
                for (int i = 0; i < ColonnadesIds::N_DEGREES; ++i)
                    if (mod->params[ColonnadesIds::WEIGHT_PARAM_0 + i].getValue() > 1e-4f)
                        mod->params[ColonnadesIds::CENTS_PARAM_0 + i].setValue((float)i * (1200.f / (float)N));
                mod->params[ColonnadesIds::CENTS_PARAM_0].setValue(0.f);   // root always 0
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

Model* modelColonnades = createModel<Colonnades, ColonnadesWidget>("Colonnades");
