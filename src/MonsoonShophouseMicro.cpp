#include <rack.hpp>
#include <cmath>
#include <osdialog.h>
#include "MonsoonShophouseMicro.hpp"
#include "Monsoon.hpp"
#include "MicroTuning.hpp"                 // MicroTuningModule (Colonnades/Duo) — the host we attach to
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/WrappingMenuLabel.hpp"       // full loaded-slot names in the context menu (spec §480)
#include "tuning/TuningPreset.hpp"

using namespace rack;
using namespace ShophouseMicroIds;

extern Model* modelMonsoonShophouseMicro;

// ── Driver only (Model Q): maintain scene selection; write NO engine state. The active front's
// cents[]+weight[] reach the TuningTable via the Colonnades/Duo publish FOLD (single writer). Here we
// only: resolve the host, reconcile 12/24 mode with the host tt.N (flag conflict, never wipe), and
// sample INDEX_CV at the phrase boundary → pending → commit (boundary-quantised scene switch). ──────
void MonsoonShophouseMicro::process(const ProcessArgs&) {
    // ATTACH via Monsoon's OWN cache (ENABLED_MASK_BUILD_BRIEF §4a): resolve the tuning host the SAME
    // way Monsoon does — mon->expanderManager.cachedColonnadesExpander (set for BOTH modelColonnades and
    // modelColonnadesDuo). The cached module's MODEL gives the mode (Colonnades=12 / Duo=24)
    // DETERMINISTICALLY — tuning.N is downstream (reads 12 until the Micro claims) so we don't infer
    // from it. Find Monsoon on either side (the Colonnades sits between us and Monsoon; the chain-walk
    // hops it) for the phrase-boundary edge.
    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    rack::Module* host = mon ? mon->expanderManager.cachedColonnadesExpander : nullptr;
    hostMicro_ = host;
    if (!host || !mon) { modeConflict = false; return; }   // standalone / no complete rig: nothing to modulate

    const int hostN = (host->model == modelColonnadesDuo) ? 24 : 12;   // model → mode (authoritative)

    // Mode reconciliation (spec §66/§91): connection INFORMS the mode but never silently wipes slots.
    if (!list.anyLoaded()) {
        // No slots yet → adopt the host's N as the mode (front count follows).
        if (list.degrees() != hostN) setMode(hostN);
        modeConflict = false;
    } else {
        // Slots loaded → if the host N disagrees, FLAG it (widget shows a warning); do not overwrite.
        modeConflict = (list.degrees() != hostN);
    }

    // Boundary-quantised FRONT SWITCH: sample INDEX_CV at the phrase edge → pending; commit on the edge.
    if (mon->engine.lastStepResult.wrapped) {
        auto& cv = inputs[INDEX_CV_INPUT];
        if (cv.isConnected()) {
            int n = frontCount();
            float att  = params[INDEX_CV_ATT_PARAM].getValue();
            float norm = clamp((cv.getVoltage() / 10.f) * att, 0.f, 0.999f);
            list.setPending((int)std::floor(norm * (float)n));
        }
        list.commitAtBoundary();
    }
    lastActive_ = list.active();
}

json_t* MonsoonShophouseMicro::dataToJson() {
    json_t* root = json_object();
    json_object_set_new(root, "degrees", json_integer(list.degrees()));
    json_object_set_new(root, "pending", json_integer(list.pending()));
    json_object_set_new(root, "active",  json_integer(list.active()));
    json_t* slots = json_array();
    for (int s = 0; s < list.size(); ++s) {
        const TuningSlot& sl = list.slot(s);
        json_t* o = json_object();
        json_object_set_new(o, "loaded", json_boolean(sl.loaded));
        if (sl.loaded) {
            json_object_set_new(o, "n", json_integer(sl.n));   // R10: this front's tuning size (1..capacity)
            if (!sl.name.empty()) json_object_set_new(o, "name", json_string(sl.name.c_str()));
            json_t* jc = json_array(); json_t* je = json_array();
            for (int i = 0; i < list.degrees(); ++i) {
                json_array_append_new(jc, json_real((double)sl.cents[i]));
                json_array_append_new(je, json_boolean(sl.enabled[i]));   // v2: scale mask, not weight
            }
            json_object_set_new(o, "cents", jc);
            json_object_set_new(o, "enabled", je);
        }
        json_array_append_new(slots, o);
    }
    json_object_set_new(root, "slots", slots);
    return root;
}

void MonsoonShophouseMicro::dataFromJson(json_t* root) {
    int degrees = 12;
    if (json_t* jd = json_object_get(root, "degrees")) degrees = (int)json_integer_value(jd);
    list = TuningList((degrees == 24) ? 2 : 4, degrees);

    if (json_t* slots = json_object_get(root, "slots"); json_is_array(slots)) {
        for (int s = 0; s < (int)json_array_size(slots) && s < list.size(); ++s) {
            json_t* o = json_array_get(slots, s);
            if (!json_is_object(o)) continue;
            json_t* jl = json_object_get(o, "loaded");
            if (!jl || !json_boolean_value(jl)) continue;
            float cents[dotModular::TuningTable::MAXN] = {};
            bool  enabled[dotModular::TuningTable::MAXN] = {};
            for (int i = 0; i < degrees; ++i) enabled[i] = true;   // default in-scale
            if (json_t* a = json_object_get(o, "cents"); json_is_array(a))
                for (int i = 0; i < degrees && i < (int)json_array_size(a); ++i)
                    cents[i] = (float)json_number_value(json_array_get(a, i));
            // enabled (v2). Older patches persisted "weight" → migrate (enabled = weight>0).
            if (json_t* a = json_object_get(o, "enabled"); json_is_array(a)) {
                for (int i = 0; i < degrees && i < (int)json_array_size(a); ++i)
                    enabled[i] = json_boolean_value(json_array_get(a, i));
            } else if (json_t* w = json_object_get(o, "weight"); json_is_array(w)) {
                for (int i = 0; i < degrees && i < (int)json_array_size(w); ++i)
                    enabled[i] = json_number_value(json_array_get(w, i)) > 0.0;
            }
            std::string name;
            if (json_t* jn = json_object_get(o, "name")) if (json_is_string(jn)) name = json_string_value(jn);
            // R10: the slot's own tuning size (1..capacity). Older patches without "n" fall back to the
            // full capacity (degrees) — matches their pre-R10 behaviour.
            int sn = degrees;
            if (json_t* jn2 = json_object_get(o, "n")) { sn = (int)json_integer_value(jn2);
                if (sn < 1) sn = 1; if (sn > degrees) sn = degrees; }
            list.loadSlot(s, sn, cents, enabled, name, /*adoptModeIfEmpty=*/false);
        }
    }
    if (json_t* jp = json_object_get(root, "pending")) list.setPending((int)json_integer_value(jp));
    // active follows pending on the next boundary; seed it so the readout is right immediately.
    for (int i = 0, want = json_object_get(root, "active") ? (int)json_integer_value(json_object_get(root, "active")) : 0;
         i < list.size() && list.active() != want; ++i) { list.setPending(want); list.commitAtBoundary(); }
}

// ── Load a .dmtune into a front, validated against the module's 12/24 mode (empty module adopts the
// file's N as its mode). Shared by the mask-window + name-band click targets. ───────────────────────
static void loadDmtuneInto(MonsoonShophouseMicro* module, int front) {
    if (!module) return;
    osdialog_filters* filters = osdialog_filters_parse("dot.modular Tuning:dmtune");
    char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
    osdialog_filters_free(filters);
    if (!path) return;
    std::string pathStr(path); std::free(path);
    // ROUND 10 full model: a front is a .dmtune of ANY n in 1..CAPACITY (the Shophouse Micro's mode /
    // host capacity — 12 for a Colonnades rig, 24 for a Duo). Slots may differ in n. Accept 1..capacity;
    // the front's own n sizes the tuning when it becomes active.
    const int cap = module->list.degrees();
    auto p = dotModular::loadTuningPreset(pathStr,
        [cap](int nn){ return nn >= 1 && nn <= cap; },
        "This .dmtune has more degrees than this Shophouse Micro's capacity.");
    if (!p.ok()) { osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, p.errorMessage.c_str()); return; }
    std::string nm = p.name;
    if (nm.empty()) {
        size_t slash = pathStr.find_last_of("/\\");
        std::string base = (slash == std::string::npos) ? pathStr : pathStr.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        nm = (dot == std::string::npos) ? base : base.substr(0, dot);
    }
    if (!module->list.loadSlot(front, p.n, p.cents, p.enabled, nm))
        osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, "Could not load into this slot (mode mismatch).");
}

// Map a PANEL WINDOW index (0..3, the fixed 2x2 SVG layout) to (front, degreeOffset) by mode
// (SHOPHOUSE_MICRO_SPEC §234). 12-mode: window i == front i, showing degrees 0..11. 24-mode: a STORY's
// two windows form one 24-strip — the two upper windows (panels 0,2) are front 0's low/high halves, the
// two lower windows (panels 1,3) are front 1's. Each window always shows 12 cells (a half or a whole).
static inline void windowToFront(int panelIdx, int degrees, int& front, int& degOff) {
    if (degrees == 24) { front = panelIdx % 2; degOff = (panelIdx < 2) ? 0 : 12; }
    else               { front = panelIdx;     degOff = 0; }
}

static std::string smTrunc(const std::string& s, size_t maxc) {
    if (s.size() <= maxc) return s;
    return s.substr(0, maxc > 1 ? maxc - 1 : 1) + "\u2026";
}
static std::shared_ptr<window::Font> smFont() {
    auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
    if (!f) f = APP->window->uiFont;
    return f;
}

// ── The window fill: N=12 EQUAL alternating black/blue MASK CELLS over one panel window (never piano
// keys). A cell lights when its degree is ENABLED (in scale) in the .dmtune, dims when masked (out of
// scale) — the same active/masked semantic as the Colonnades faders. Click = load a .dmtune. Also
// draws the active-front lantern at the window's top-left. ─────────────────────────────────────────
struct MaskWindowWidget : OpaqueWidget {
    MonsoonShophouseMicro* module = nullptr;
    int panelIdx = 0;
    void draw(const DrawArgs& args) override {
        if (!module) return;
        NVGcontext* vg = args.vg;
        int front, degOff; windowToFront(panelIdx, module->list.degrees(), front, degOff);
        const TuningSlot& slot = module->list.slot(front);
        const float W = box.size.x, H = box.size.y;
        const int N = 12;                                  // always a 12-cell half/whole strip
        const float gap = 0.6f;
        const float cw = (W - gap * (N - 1)) / (float)N;
        for (int i = 0; i < N; ++i) {
            const int deg = degOff + i;
            const bool even    = (i % 2 == 0);             // ALTERNATING black/blue identity (not keys)
            const bool beyond  = slot.loaded && deg >= slot.n;         // ROUND 10: past this front's tuning size
            const bool lit     = slot.loaded && !beyond && slot.enabled[deg];   // in-scale vs masked
            NVGcolor c;
            if (beyond) c = nvgRGB(0x0c,0x0e,0x12);        // GREYED: not in the tuning (near-black)
            else c = even ? (lit ? nvgRGB(0x34,0x7e,0xe0) : nvgRGB(0x17,0x24,0x38))   // blue
                          : (lit ? nvgRGB(0x8c,0x94,0xa0) : nvgRGB(0x13,0x15,0x1b));  // black
            float x = i * (cw + gap);
            nvgBeginPath(vg); nvgRoundedRect(vg, x, 0, cw, H, 0.6f);
            nvgFillColor(vg, c); nvgFill(vg);
        }
        // Active-front lantern (top-left of the window).
        const bool active = slot.loaded && (module->list.active() == front);
        nvgBeginPath(vg); nvgCircle(vg, mm2px(1.6f), mm2px(1.6f), mm2px(1.2f));
        nvgFillColor(vg, active ? nvgRGB(0xd4,0x00,0x1a) : nvgRGBA(0x60,0x30,0x34,0xa0)); nvgFill(vg);
    }
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            int front, degOff; windowToFront(panelIdx, module->list.degrees(), front, degOff);
            e.consume(this); loadDmtuneInto(module, front); return;
        }
        OpaqueWidget::onButton(e);
    }
};

// ── The name band: abbreviated .dmtune name (truncated + ellipsis), placeholder when empty; doubles as
// a load click target. In 24-mode only the LEFT-house band of each story draws the name (the story's two
// windows share one front) so the label isn't drawn twice. ─────────────────────────────────────────
struct NameBandWidget : OpaqueWidget {
    MonsoonShophouseMicro* module = nullptr;
    int panelIdx = 0;
    void draw(const DrawArgs& args) override {
        if (!module) return;
        auto f = smFont(); if (!f) return;
        NVGcontext* vg = args.vg;
        const int degrees = module->list.degrees();
        int front, degOff; windowToFront(panelIdx, degrees, front, degOff);
        // 24-mode: a front spans two windows; draw its name only on the left-house half (panels 0,1).
        const bool primary = (degrees == 24) ? (panelIdx < 2) : true;
        if (!primary) return;
        const TuningSlot& slot = module->list.slot(front);
        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, 8.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        std::string label = slot.loaded ? (slot.name.empty() ? "(tuning)" : smTrunc(slot.name, 14))
                                        : "\u2014 empty \u2014";
        nvgFillColor(vg, slot.loaded ? nvgRGB(0xff,0x9a,0x2a) : nvgRGB(0x6a,0x72,0x7c));
        nvgText(vg, box.size.x * 0.5f, box.size.y * 0.5f, label.c_str(), nullptr);
    }
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
            int front, degOff; windowToFront(panelIdx, module->list.degrees(), front, degOff);
            e.consume(this); loadDmtuneInto(module, front); return;
        }
        OpaqueWidget::onButton(e);
    }
};

struct MonsoonShophouseMicroWidget : ModuleWidget,
    dotModular::Compose<MonsoonShophouseMicroWidget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonShophouseMicroWidget(MonsoonShophouseMicro* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/ShophouseMicro_panel_dark.svg";
        const char* lightPath = "res/panels/ShophouseMicro_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Per-window mask-cell strips + name bands (the 2x2 shophouse windows). Each panel window shows
        // 12 alternating black/blue mask cells; the widget re-maps window→front by mode (12: 1:1;
        // 24: a story's two windows are one front's two 12-halves). Both are click-to-load targets.
        for (int p = 0; p < ShophouseMicroIds::MAX_FRONTS; ++p) {
            if (auto* s = findNamed("window_" + std::to_string(p))) {
                auto* mw = new MaskWindowWidget();
                mw->module = mod; mw->panelIdx = p;
                Rect b = boundsOf(s);
                mw->box.pos = b.pos; mw->box.size = b.size;
                addChild(mw);
            }
            if (auto* s = findNamed("name_band_" + std::to_string(p))) {
                auto* nb = new NameBandWidget();
                nb->module = mod; nb->panelIdx = p;
                Rect b = boundsOf(s);
                nb->box.pos = b.pos; nb->box.size = b.size;
                addChild(nb);
            }
        }

        bindParam<CKSS>("param_conservation", ShophouseMicroIds::CONSERVATION_PARAM);
        bindInput<PJ301MPort>("input_indexcv", ShophouseMicroIds::INDEX_CV_INPUT);
        bindParam<Trimpot>("param_indexcvatt", ShophouseMicroIds::INDEX_CV_ATT_PARAM);

        // Connection mark — lit when attached to a Colonnades/Duo (spec §91-100). Shophouse Micro is NOT
        // a Monsoon-claimed expander, so it wires to attachedToMicro() rather than isConnectedAndClaimed.
        if (auto* s = findNamed("light_connect")) {
            connectMark = new redDot::ConnectMark();
            Rect b = boundsOf(s);
            float sz = std::max(b.size.x, b.size.y) * 1.6f; if (sz < 12.f) sz = 12.f;
            connectMark->box.size = Vec(sz, sz);
            connectMark->box.pos  = centerOf(s).minus(connectMark->box.size.div(2));
            connectMark->markPx   = sz * 0.85f;
            MonsoonShophouseMicro* mm = mod;
            connectMark->connected  = [mm]() { return mm && mm->attachedToMicro(); };
            connectMark->lightTheme = [mm]() { Monsoon* h = mm ? redDot::findMonsoonEitherSide(mm) : nullptr; return h && h->lightTheme; };
            addChild(connectMark);
        }

        if (auto* s = findNamed("wordmark")) {
            // wordmark is widget-drawn in draw()
            (void)s;
        }
    }

    void appendContextMenu(Menu* menu) override {
        auto* mod = dynamic_cast<MonsoonShophouseMicro*>(module);
        if (!mod) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel(std::string("Mode: ") + (mod->list.degrees() == 24 ? "24-tone (Duo)" : "12-tone (Colonnades)")));
        if (mod->modeConflict)
            menu->addChild(createMenuLabel("⚠ Host degree count doesn't match loaded slots"));
        // Explicit mode toggle — only when no slot is loaded (else clear first).
        struct ModeItem : MenuItem { MonsoonShophouseMicro* m; int n;
            void onAction(const event::Action&) override { m->setMode(n); } };
        for (int nn : {12, 24}) {
            auto* it = new ModeItem(); it->m = mod; it->n = nn;
            it->text = (nn == 12) ? "Set 12-tone mode" : "Set 24-tone mode";
            it->rightText = CHECKMARK(mod->list.degrees() == nn);
            it->disabled = mod->list.anyLoaded() && mod->list.degrees() != nn;
            menu->addChild(it);
        }
        // Full loaded-slot names (spec §480): the panel name band truncates to the narrow window; the
        // menu has room for the FULL .dmtune name, word-wrapped via redDot::WrappingMenuLabel so a long
        // name can't blow out the menu width. The ACTIVE front is marked with a leading "▶".
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Fronts"));
        for (int f = 0; f < mod->frontCount(); ++f) {
            const TuningSlot& slot = mod->list.slot(f);
            const bool active = (mod->list.active() == f);
            std::string label = std::string(active ? "\u25B6 " : "   ")
                              + "Front " + std::to_string(f + 1) + ": "
                              + (slot.loaded ? (slot.name.empty() ? std::string("(tuning)") : slot.name)
                                             : std::string("(empty)"));
            auto* wl = new redDot::WrappingMenuLabel();
            wl->text = label;
            wl->maxWidth = 260.f;   // a little wider than the 220 default is fine in a menu
            menu->addChild(wl);
        }

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Clear all slots", "", [mod]() { mod->list.clear(); }));
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* m = redDot::findMonsoonEitherSide(module);
        int wantLight = (m && m->lightTheme) ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children)
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) { sp->setBackground(wantLight ? panelSvgLight : panelSvgDark); break; }
        }
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        // Widget-drawn wordmark (nanosvg ignores <text>).
        auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (f) {
            const bool light = (lastThemeLight == 1);
            nvgFontFaceId(args.vg, f->handle);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFontSize(args.vg, 10.f);
            nvgFillColor(args.vg, light ? nvgRGB(0x1a,0x1a,0x1a) : nvgRGB(0xf0,0xf0,0xf0));
            nvgText(args.vg, box.size.x * 0.5f, mm2px(11.0f), "Shophouse Micro", nullptr);
        }
    }
};

Model* modelMonsoonShophouseMicro =
    createModel<MonsoonShophouseMicro, MonsoonShophouseMicroWidget>("MonsoonShophouseMicro");
