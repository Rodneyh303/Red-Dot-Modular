#include <rack.hpp>
#include <cmath>
#include <osdialog.h>
#include "MonsoonShophouseMicro.hpp"
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "tuning/TuningPreset.hpp"

using namespace rack;
using namespace ShophouseMicroIds;

extern Model* modelMonsoonShophouseMicro;

// ── Driver only (Model Q): maintain scene selection; write NO engine state. The active front's
// cents[]+weight[] reach the TuningTable via the Colonnades/Duo publish FOLD (single writer). Here we
// only: resolve the host, reconcile 12/24 mode with the host tt.N (flag conflict, never wipe), and
// sample INDEX_CV at the phrase boundary → pending → commit (boundary-quantised scene switch). ──────
void MonsoonShophouseMicro::process(const ProcessArgs&) {
    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    if (!mon) { modeConflict = false; return; }   // standalone: no host to modulate

    const int hostN = mon->engine.pe.tuning.N;

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
            if (!sl.name.empty()) json_object_set_new(o, "name", json_string(sl.name.c_str()));
            json_t* jc = json_array(); json_t* jw = json_array();
            for (int i = 0; i < list.degrees(); ++i) {
                json_array_append_new(jc, json_real((double)sl.cents[i]));
                json_array_append_new(jw, json_real((double)sl.weight[i]));
            }
            json_object_set_new(o, "cents", jc);
            json_object_set_new(o, "weight", jw);
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
            float weight[dotModular::TuningTable::MAXN] = {};
            auto readArr = [&](const char* key, float* dst) {
                json_t* a = json_object_get(o, key);
                if (!json_is_array(a)) return;
                for (int i = 0; i < degrees && i < (int)json_array_size(a); ++i)
                    dst[i] = (float)json_number_value(json_array_get(a, i));
            };
            readArr("cents", cents);
            readArr("weight", weight);
            std::string name;
            if (json_t* jn = json_object_get(o, "name")) if (json_is_string(jn)) name = json_string_value(jn);
            list.loadSlot(s, degrees, cents, weight, name, /*adoptModeIfEmpty=*/false);
        }
    }
    if (json_t* jp = json_object_get(root, "pending")) list.setPending((int)json_integer_value(jp));
    // active follows pending on the next boundary; seed it so the readout is right immediately.
    for (int i = 0, want = json_object_get(root, "active") ? (int)json_integer_value(json_object_get(root, "active")) : 0;
         i < list.size() && list.active() != want; ++i) { list.setPending(want); list.commitAtBoundary(); }
}

// ── Per-front cell: draws the loaded .dmtune name (truncated) + active lantern; click = load a .dmtune
// into this slot (validated against the module's 12/24 mode). Neutral scene-slot, no piano keys. ────
struct MicroFrontWidget : OpaqueWidget {
    MonsoonShophouseMicro* module = nullptr;
    int front = 0;

    std::shared_ptr<window::Font> font() {
        auto f = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!f) f = APP->window->uiFont;
        return f;
    }
    // Truncate to fit the band (reuses the .scl width-fix idea: ellipsis when too long).
    static std::string trunc(const std::string& s, size_t maxc) {
        if (s.size() <= maxc) return s;
        return s.substr(0, maxc > 1 ? maxc - 1 : 1) + "\u2026";
    }
    void draw(const DrawArgs& args) override {
        if (!module) return;
        auto f = font(); if (!f) return;
        NVGcontext* vg = args.vg;
        const bool visible = front < module->frontCount();
        if (!visible) {   // 24-mode hides fronts 3-4: grey the cell
            nvgBeginPath(vg); nvgRect(vg, 0, 0, box.size.x, box.size.y);
            nvgFillColor(vg, nvgRGBA(0x10,0x12,0x15,0xc0)); nvgFill(vg);
            return;
        }
        const TuningSlot& slot = module->list.slot(front);
        // Active-front lantern (top-left).
        const bool active = (module->list.active() == front);
        nvgBeginPath(vg); nvgCircle(vg, mm2px(3.0f), mm2px(3.5f), mm2px(1.4f));
        nvgFillColor(vg, active ? nvgRGB(0xd4,0x00,0x1a) : nvgRGBA(0x60,0x30,0x34,0xff)); nvgFill(vg);
        // Name band text (bottom strip).
        nvgFontFaceId(vg, f->handle);
        nvgFontSize(vg, 8.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        std::string label = slot.loaded ? (slot.name.empty() ? "(tuning)" : trunc(slot.name, 12))
                                        : "— empty —";
        nvgFillColor(vg, slot.loaded ? nvgRGB(0xff,0x9a,0x2a) : nvgRGB(0x6a,0x72,0x7c));
        nvgText(vg, box.size.x * 0.5f, box.size.y - mm2px(4.0f), label.c_str(), nullptr);
        // Front number top-centre.
        nvgFontSize(vg, 7.f);
        nvgFillColor(vg, nvgRGB(0x9a,0xa2,0xac));
        nvgText(vg, box.size.x * 0.5f, mm2px(3.5f), std::to_string(front + 1).c_str(), nullptr);
    }
    void onButton(const event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT
            && module && front < module->frontCount()) {
            e.consume(this);
            loadDmtune();
            return;
        }
        OpaqueWidget::onButton(e);
    }
    void loadDmtune() {
        osdialog_filters* filters = osdialog_filters_parse("dot.modular Tuning:dmtune");
        char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
        osdialog_filters_free(filters);
        if (!path) return;
        std::string pathStr(path); std::free(path);
        // Validate n against the module mode (unless empty → first load adopts it, via loadSlot).
        const int wantN = module->list.anyLoaded() ? module->list.degrees() : 0;   // 0 = accept, adopt
        auto p = dotModular::loadTuningPreset(pathStr,
            [wantN](int nn){ return wantN == 0 || nn == wantN; },
            "This .dmtune's degree count doesn't match this Shophouse Micro's mode "
            "(all slots must be the same 12 or 24).");
        if (!p.ok()) { osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, p.errorMessage.c_str()); return; }
        // If unattached/empty, set the front count for the adopted mode before loading.
        if (!module->list.anyLoaded()) module->setMode(p.n);
        std::string nm = p.name;
        if (nm.empty()) {
            size_t slash = pathStr.find_last_of("/\\");
            std::string base = (slash == std::string::npos) ? pathStr : pathStr.substr(slash + 1);
            size_t dot = base.find_last_of('.');
            nm = (dot == std::string::npos) ? base : base.substr(0, dot);
        }
        if (!module->list.loadSlot(front, p.n, p.cents, p.weight, nm))
            osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK,
                "Could not load into this slot (mode mismatch).");
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

        // Per-front cells (name band + lantern + click-to-load).
        for (int f = 0; f < ShophouseMicroIds::MAX_FRONTS; ++f) {
            if (auto* s = findNamed("cell_" + std::to_string(f))) {
                auto* fw = new MicroFrontWidget();
                fw->module = mod; fw->front = f;
                Rect b = boundsOf(s);
                fw->box.pos = b.pos; fw->box.size = b.size;
                addChild(fw);
            }
        }

        bindParam<CKSS>("param_conservation", ShophouseMicroIds::CONSERVATION_PARAM);
        bindInput<PJ301MPort>("input_indexcv", ShophouseMicroIds::INDEX_CV_INPUT);
        bindParam<Trimpot>("param_indexcvatt", ShophouseMicroIds::INDEX_CV_ATT_PARAM);

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
