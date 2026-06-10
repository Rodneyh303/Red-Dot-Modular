#pragma once
// dotModular::SvgPanelKit — a VARIADIC-TEMPLATE, composable alternative to the
// CRTP SvgHelper. Same job (bind widgets to named SVG shapes, optional dev
// live-reload), but the feature set is assembled from independent mixin pieces
// via a variadic chain, so a module opts into exactly the capabilities it wants:
//
//   struct MyWidget : ModuleWidget,
//       dotModular::Compose<MyWidget, ShapeQuery, Bind, Reload> { ... };
//
// vs the monolithic CRTP base. This is an EXPERIMENT to compare ergonomics and
// readability against src/ui/SvgHelper.hpp — not a drop-in replacement yet.
//
// ── USAGE COMPARISON ────────────────────────────────────────────────────────
// CRTP (current SvgHelper.hpp):
//   struct MonsoonWidget : ModuleWidget, dotModular::SvgHelper<MonsoonWidget> {
//     ...
//     bindInput<PJ301MPort>("input_RUN_GATE_INPUT", RUN_GATE_INPUT);
//     bindInput<PJ301MPort>("input_CLK_INPUT",      CLK_INPUT);   // one per line
//     void step() override { ModuleWidget::step(); SvgHelper::step(); }
//   };
//
// Composed variadic (this file):
//   struct MonsoonWidget : ModuleWidget,
//       dotModular::Compose<MonsoonWidget, ShapeQuery, Bind, Reload> {
//     ...
//     // opt into only ShapeQuery+Bind+Reload; a tiny module could take fewer.
//     // pack form binds a whole prefixed row in one call:
//     bindParams<Trimpot>("atten_", A0,A1,A2,A3,A4,A5,A6,A7,A8,A9,A10,A11);
//     void step() override { ModuleWidget::step(); kitStep(); }
//   };
//
// Trade-off summary (see commit message): the composed form adds the ability to
// opt into feature subsets and to bind prefixed rows in one variadic call, at
// the cost of diamond/virtual-inheritance plumbing and a separate state struct.
// ────────────────────────────────────────────────────────────────────────────
//
// Design/interface derived from dustinlacewell/vcv-svghelper (MIT); see the MIT
// attribution block in src/ui/SvgHelper.hpp. This file restructures that feature
// set as composable mixins rather than copying it.
#pragma once

#include <sys/stat.h>
#include <map>
#include <regex>
#include <string>
#include <vector>
#include <functional>
#include "rack.hpp"

namespace dotModular {

using namespace rack;

// ── Shared state every feature mixin can see (held by the Compose base) ──────
struct SvgKitState {
    app::SvgPanel* panel = nullptr;
    std::string svgFile;
    std::map<std::string, Widget*> bound;
    bool devMode = false, pollMode = false;
    double prevPoll = 0.0;
    struct stat prevStat = {};
};

// CRTP access helpers: Holds common utilities. Inherited non-virtually via Compose.
template <class T>
struct KitAccess {
    T* self()  { return static_cast<T*>(this); }
    ModuleWidget* mw()    { return static_cast<ModuleWidget*>(self()); }
    SvgKitState&  state() { return self()->kit_; }
    
    Vec centerOf(NSVGshape* s) {
        const float* b = s->bounds;
        // Restored the missing array indices here:
        return Vec((b[0] + b[2]) / 2.f, (b[1] + b[3]) / 2.f);
    }
};
// ── Feature mixin: panel load + shape queries ────────────────────────────────
template <class T>
struct ShapeQuery { 
    T* self() { return static_cast<T*>(this); }

    void loadPanel(const std::string& file) {
        auto& st = this->self()->state(); 
        st.svgFile = file;
        if (!st.panel) { st.panel = createPanel(file); this->self()->mw()->setPanel(st.panel); } 
    }
    void forEachShape(const std::function<void(NSVGshape*)>& cb) {
        auto& st = this->self()->state();
        if (!st.panel || !st.panel->svg || !st.panel->svg->handle) return;
        for (NSVGshape* s = st.panel->svg->handle->shapes; s; s = s->next) cb(s);
    }
    NSVGshape* findNamed(const std::string& name) {
        NSVGshape* r = nullptr;
        forEachShape([&](NSVGshape* s){ if (!r && name == s->id) r = s; });
        return r;
    }
    void forEachMatched(const std::string& pat,
                        const std::function<void(std::vector<std::string>, NSVGshape*)>& cb) {
        std::regex re(pat);
        forEachShape([&](NSVGshape* s){
            std::string id(s->id); std::smatch m;
            if (std::regex_search(id, m, re)) {
                std::vector<std::string> caps;
                for (unsigned i=1;i<m.size();++i) caps.push_back(m[i]);
                cb(caps, s);
            }
        });
    }
};

// ── Feature mixin: binding (incl. VARIADIC multi-id binds) ───────────────────
template <class T>
struct Bind {
    T* self() { return static_cast<T*>(this); }

    template <class W> void bindParam(const std::string& n, int id) {
        if (auto* s = this->self()->findNamed(n)) { 
            auto* w = createParamCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, id); 
            this->self()->mw()->addParam(w); this->self()->state().bound[s->id] = w; 
        } else WARN("[SvgKit] param shape not found: %s", n.c_str());
    }
    template <class W> void bindInput(const std::string& n, int id) {
        if (auto* s = this->self()->findNamed(n)) { 
            auto* w = createInputCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, id); 
            this->self()->mw()->addInput(w); this->self()->state().bound[s->id] = w; 
        } else WARN("[SvgKit] input shape not found: %s", n.c_str());
    }
    template <class W> void bindOutput(const std::string& n, int id) {
        if (auto* s = this->self()->findNamed(n)) { 
            auto* w = createOutputCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, id); 
            this->self()->mw()->addOutput(w); this->self()->state().bound[s->id] = w; 
        } else WARN("[SvgKit] output shape not found: %s", n.c_str());
    }

    template <class W>
    void bindInputs(const std::string& prefix, std::initializer_list<int> ids) {
        int i = 0;
        for (int id : ids) bindInput<W>(prefix + std::to_string(i++), id);
    }
    
    template <class W, class... Ids>
    void bindParams(const std::string& prefix, Ids... ids) {
        int i = 0;
        using expander = int[];
        (void)expander{0, ((void)bindParam<W>(prefix + std::to_string(i++), ids), 0)...};
    }
};

// ── Feature mixin: dev-mode live reload + context menu ───────────────────────
template <class T>
struct Reload {
    T* self() { return static_cast<T*>(this); }

    void setDevMode(bool v) { this->self()->state().devMode = v; }
    void setDirty() { auto& st = this->self()->state(); if (st.panel && st.panel->fb) st.panel->fb->dirty = true; }

    void appendKitMenu(Menu* menu) { 
        auto& st = this->self()->state(); 
        if (!st.devMode || st.svgFile.empty()) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reload panel", "", [this](){ reload(); }));
        menu->addChild(createBoolPtrMenuItem("Poll SVG for reload", "", &st.pollMode));
    }
    void kitStep() {
        auto& st = this->self()->state(); 
        if (!this->self()->state().pollMode || this->self()->state().svgFile.empty()) return;
        double now = system::getTime();
        if (now - st.prevPoll < 1.0) return;
        st.prevPoll = now;
        struct stat sb;
        if (stat(st.svgFile.c_str(), &sb)==0 &&
            memcmp(&sb.st_mtime, &st.prevStat.st_mtime, sizeof(sb.st_mtime))!=0) {
            st.prevStat = sb; reload();
        }
    }
    void reload() {
        auto& st = this->self()->state();
        if (st.svgFile.empty() || !st.panel || !st.panel->svg) return;
        NSVGimage* repl = nsvgParseFromFile(st.svgFile.c_str(), "px", SVG_DPI);
        if (!repl) { WARN("[SvgKit] cannot parse %s", st.svgFile.c_str()); return; }
        if (st.panel->svg->handle) nsvgDelete(st.panel->svg->handle); 
        st.panel->svg->handle = repl;
        this->self()->forEachShape([&](NSVGshape* s){ 
            auto it = this->self()->state().bound.find(s->id);
            if (it != st.bound.end())
                it->second->box.pos = this->self()->centerOf(s).minus(it->second->box.size.div(2)); 
        });
        setDirty();
    }
};

// ── The variadic composer: pulls in the chosen feature mixins + owns state ───
template <class T, template <class> class... Features>
struct Compose : Features<T>..., KitAccess<T> { // Added KitAccess<T> directly here non-virtually
    SvgKitState kit_;   

    using ShapeQuery<T>::loadPanel;
    using ShapeQuery<T>::forEachShape;
    using ShapeQuery<T>::findNamed;
    using ShapeQuery<T>::forEachMatched;

    using Bind<T>::bindParam;
    using Bind<T>::bindInput;
    using Bind<T>::bindOutput;
    using Bind<T>::bindInputs;
    using Bind<T>::bindParams;

    using Reload<T>::setDevMode;
    using Reload<T>::setDirty;
    using Reload<T>::appendKitMenu;
    using Reload<T>::kitStep;
    using Reload<T>::reload;
};

}  // namespace dotModular