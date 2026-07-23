#pragma once

/*
// Inspried by dustinlacewell/vcv-svghelper
// vcv-svghelper by Dustin Lacewell (https://github.com/dustinlacewell/vcv-svghelper),
This variadic composition approach is a architecturally different 
compared to standard CRTP mixins, allowing for clean feature opt-ins and variadic binding.

*/



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
#include <type_traits> // <--- MAKE SURE THIS IS HERE

namespace dotModular {

using namespace rack;

// ── Shared state every feature mixin can see (held by the Compose base) ──────
struct SvgKitState {
    app::SvgPanel* panel = nullptr;
    std::string svgFile;
    std::map<std::string, NSVGshape*> shapeCache;
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
    // Full bounding rect (px) of a shape marker — lets the widget read geometry (position AND size)
    // straight from a panel <rect> marker, so panel art is the single source of geometry truth.
    Rect boundsOf(NSVGshape* s) {
        const float* b = s->bounds;   // [minx, miny, maxx, maxy]
        return Rect(Vec(b[0], b[1]), Vec(b[2] - b[0], b[3] - b[1]));
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
        
        // Cache shapes for O(1) lookups during binding
        st.shapeCache.clear();
        if (st.panel->svg && st.panel->svg->handle) {
            for (NSVGshape* s = st.panel->svg->handle->shapes; s; s = s->next) {
                if (s->id[0] != '\0') st.shapeCache[s->id] = s;
            }
        }
    }
    void forEachShape(const std::function<void(NSVGshape*)>& cb) {
        auto& st = this->self()->state();
        if (!st.panel || !st.panel->svg || !st.panel->svg->handle) return;
        for (NSVGshape* s = st.panel->svg->handle->shapes; s; s = s->next) cb(s);
    }
    NSVGshape* findNamed(const std::string& name) {
        auto& st = this->self()->state();
        auto it = st.shapeCache.find(name);
        return (it != st.shapeCache.end()) ? it->second : nullptr;
    }
    std::vector<NSVGshape*> findPrefixed(const std::string& prefix) {
        std::vector<NSVGshape*> r;
        forEachShape([&](NSVGshape* s){
            if (std::string(s->id).rfind(prefix, 0) == 0) r.push_back(s);
        });
        return r;
    }
    void forEachPrefixed(const std::string& prefix,
                         const std::function<void(unsigned, NSVGshape*)>& cb) {
        auto v = findPrefixed(prefix);
        for (unsigned i = 0; i < v.size(); ++i) cb(i, v[i]);
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

    // Core binding logic with optional configuration lambda
    template <class W> void bindParam(const std::string& n, int id, std::function<void(W*)> config = nullptr) {
        if (auto* s = this->self()->findNamed(n)) { 
            auto* w = createParamCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, id); 
            if (config) config(w);
            this->self()->mw()->addParam(w); this->self()->state().bound[s->id] = w; 
        } else WARN("[SvgKit] param shape not found: %s", n.c_str());
    }
    // Bind a NON-PARAM widget (e.g. redDot::StoreKnob) to a named panel shape.
    // De-parammed controls are not ParamWidgets, so createParamCentered/addParam cannot be
    // used: this creates the widget, lets `config` run FIRST (setSvg establishes box.size),
    // then centres it on the shape and addChild()s it. Returns the widget so the caller can
    // finish wiring it. See PARAM_CLASSIFICATION.md.
    template <class W> W* bindWidget(const std::string& n, std::function<void(W*)> config = nullptr) {
        auto* s = this->self()->findNamed(n);
        if (!s) { WARN("[SvgKit] widget shape not found: %s", n.c_str()); return nullptr; }
        auto* w = createWidget<W>(rack::math::Vec(0, 0));
        if (config) config(w);                       // must precede centring: sets box.size
        w->box.pos = this->self()->centerOf(s).minus(w->box.size.div(2));
        this->self()->mw()->addChild(w);
        this->self()->state().bound[s->id] = w;
        return w;
    }

    template <class W> void bindInput(const std::string& n, int id, std::function<void(W*)> config = nullptr) {
        if (auto* s = this->self()->findNamed(n)) { 
            auto* w = createInputCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, id); 
            if (config) config(w);
            this->self()->mw()->addInput(w); this->self()->state().bound[s->id] = w; 
        } else WARN("[SvgKit] input shape not found: %s", n.c_str());
    }
    template <class W> void bindOutput(const std::string& n, int id, std::function<void(W*)> config = nullptr) {
        if (auto* s = this->self()->findNamed(n)) { 
            auto* w = createOutputCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, id); 
            if (config) config(w);
            this->self()->mw()->addOutput(w); this->self()->state().bound[s->id] = w; 
        } else WARN("[SvgKit] output shape not found: %s", n.c_str());
    }
    template <class W> void bindLight(const std::string& n, int id, std::function<void(W*)> config = nullptr) {
        if (auto* s = this->self()->findNamed(n)) {
            auto* w = createLightCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, id);
            if (config) config(w);
            this->self()->mw()->addChild(w); this->self()->state().bound[s->id] = w;
        } else WARN("[SvgKit] light shape not found: %s", n.c_str());
    }

    // Support for widgets with integrated lights (e.g. LightSliders)
    template <class W> void bindLightParam(const std::string& n, int paramId, int lightId, std::function<void(W*)> config = nullptr) {
        if (auto* s = this->self()->findNamed(n)) {
            auto* w = createLightParamCentered<W>(this->self()->centerOf(s), this->self()->mw()->module, paramId, lightId);
            if (config) config(w);
            this->self()->mw()->addParam(w); this->self()->state().bound[s->id] = w;
        } else WARN("[SvgKit] lightParam shape not found: %s", n.c_str());
    }

    // Bind an arbitrary Widget (e.g. a custom Sands lane display) to a named
    template <class W> W* bindChild(const std::string& n, std::function<void(W*)> config = nullptr) {
        if (auto* s = this->self()->findNamed(n)) {
            auto* w = createWidgetCentered<W>(this->self()->centerOf(s));
            if (config) config(w);
            this->self()->mw()->addChild(w); this->self()->state().bound[s->id] = w;
            return w;
        }
        WARN("[SvgKit] child shape not found: %s", n.c_str());
        return nullptr;
    }

    template <class W, class... Ids>
    void bindParams(const std::string& prefix, Ids... ids) {
        int i = 0;
        (bindParam<W>(prefix + std::to_string(i++), ids), ...);
    }

    template <class W, class... Ids>
    void bindInputs(const std::string& prefix, Ids... ids) {
        int i = 0;
        (bindInput<W>(prefix + std::to_string(i++), ids), ...);
    }

        template <class W, class... Ids>
    void bindOutputs(const std::string& prefix, Ids... ids) {
        int i = 0;
        (bindOutput<W>(prefix + std::to_string(i++), ids), ...);
    }

    template <class W, class... Ids>
    void bindLights(const std::string& prefix, Ids... ids) {
        int i = 0;
        (bindLight<W>(prefix + std::to_string(i++), ids), ...);
    }

// bindLightParamsContiguous<LightSlider>("fader_", P_FADER_START, L_LED_START,
//     [](LightSlider* w) { /* Custom behavior for fader_0 */ },
//     [](LightSlider* w) { /* Custom behavior for fader_1 */ },
//     [](LightSlider* w) { /* Custom behavior for fader_2 */ }
// );


template <class W, class... LambdaConfigs>
void bindLightParamsContiguous(const std::string& prefix, 
                               int startParamId, 
                               int startLightId, 
                               LambdaConfigs... configs) {
    // Compile-time check to ensure at least one lambda was provided
    static_assert(sizeof...(LambdaConfigs) > 0, "Must provide at least one configuration lambda.");

    int i = 0;
    // C++17 Fold Expression over the variadic lambdas pack.
    // It steps through every lambda, auto-incrementing the string names and both ID tracks.
    ((bindLightParam<W>(prefix + std::to_string(i), startParamId + i, startLightId + i, configs), i++), ...);
}


// Binds fader_1 to (PARAM_1, LIGHT_1), fader_2 to (PARAM_2, LIGHT_2), etc.
// bindLightParams<LightSlider>({"fader_1", "fader_2", "fader_3"}, P_FADER_1, L_FADER_1);


template <class W>
void bindLightParams(std::initializer_list<std::string> names, 
                     int startParamId, 
                     int startLightId, 
                     std::function<void(W*)> config = nullptr) {
    
    std::vector<std::string> namesVec(names);
    int i = 0;
    
    // We can use a standard fold expression over a generated index sequence, 
    // or a simple runtime loop since the count is determined by the names list:
    for (const auto& name : namesVec) {
        bindLightParam<W>(name, startParamId + i, startLightId + i, config);
        i++;
    }
}


// Explicitly map independent IDs to their SVG shapes in a single batch call
// bindLightParamsCustom<LightSlider>({"fader_L", "fader_R"}, nullptr,
//     LightParamIds{P_VOL_L, L_VOL_LEDS_L},
//     LightParamIds{P_VOL_R, L_VOL_LEDS_R}
// );

// 1. A tiny pairing structure to hold the tied IDs
struct LightParamIds {
    int paramId;
    int lightId;
};

// 2. The variadic binder
template <class W, class... Args>
void bindLightParamsCustom(std::initializer_list<std::string> names, 
                           std::function<void(W*)> config, 
                           Args... pairs) {
    
    static_assert(sizeof...(Args) > 0, "Must provide at least one ID pair.");
    static_assert((std::is_same<Args, LightParamIds>::value && ...), "All trailing arguments must be LightParamIds.");

    std::vector<std::string> namesVec(names);
    int i = 0;

    // C++17 unary fold expression unpacking the struct members side-by-side
    ((bindLightParam<W>(namesVec.at(i++), pairs.paramId, pairs.lightId, config)), ...);
}


// bindLightParamsCustom<LightSlider>(
//     {"fader_left", "fader_right", "fader_master"},
    
//     // Fader 1: Left channel customization
//     LightParamConfig<LightSlider>{P_LEFT, L_LEFT_LIGHT, [](LightSlider* w) {
//         w->box.size = Vec(15, 80); // Custom narrow bounding box
//     }},
    
//     // Fader 2: Right channel customization
//     LightParamConfig<LightSlider>{P_RIGHT, L_RIGHT_LIGHT, [](LightSlider* w) {
//         w->box.size = Vec(15, 80);
//     }},
    
//     // Fader 3: Master channel customization (e.g., changing knob colors or ranges)
//     LightParamConfig<LightSlider>{P_MASTER, L_MASTER_LIGHT, [](LightSlider* w) {
//         w->box.size = Vec(25, 100); // Make the master fader larger
//     }}
// );


// A structural bundle to tie an item's IDs and its specific configuration lambda together
template <class W>
struct LightParamConfig {
    int paramId;
    int lightId;
    std::function<void(W*)> config = nullptr;
};

// The modern C++17 variadic binder
template <class W, class... Configs>
void bindLightParamsCustom(std::initializer_list<std::string> names, Configs... items) {
    // Compile-time sanity checks
    static_assert(sizeof...(Configs) > 0, "Must provide at least one configuration bundle.");
    static_assert((std::is_same<Configs, LightParamConfig<W>>::value && ...), 
                  "All trailing arguments must match the LightParamConfig type for the chosen widget.");

    std::vector<std::string> namesVec(names);
    int i = 0;

    // C++17 Unary Fold Expression: Unpacks every structural item and calls your core engine function
    ((bindLightParam<W>(namesVec.at(i++), items.paramId, items.lightId, items.config)), ...);
}






    //     template <class W, class... Ids>
    // void bindLightParams(const std::string& prefix, Ids... ids) {
    //     int i = 0;
    //     (bindLight<W>(prefix + std::to_string(i++), ids), ...);
    // }
    // Explicitly pass the string names as template arguments, and let the IDs deduce naturally.
    // Usage: bindParams<Trimpot, {"shape_A", "shape_B", "shape_C"}>(A0, A1, A2);
    //bindParams<Trimpot>({"atten_left", "atten_right", "atten_cv"}, A0, A1, A2);
    
    template <class W, class... Ids>
    void bindParams(std::initializer_list<std::string> names, Ids... ids) {
        // Compile-time assertion to make sure you passed at least one ID
        static_assert(sizeof...(Ids) > 0, "Must provide at least one ID to bind.");
        
        // Explicitly defining the template argument to eliminate compilation edge cases
        std::vector<std::string> namesVec(names); 
        
        int i = 0;
        // Pure C++17 Fold Expression 
        ((bindParam<W>(namesVec.at(i++), ids)), ...);
    }

// Set up a row of attenuators, scaling down their visual size or customizing behavior via a shared lambda
// bindParams<Trimpot>({"att_1", "att_2", "att_3"}, [](Trimpot* w) {
//     w->box.size = Vec(20, 20); // shrink all of them
// }, ATT_1, ATT_2, ATT_3);
    template <class W, class... Ids>
    void bindParams(std::initializer_list<std::string> names, std::function<void(W*)> config, Ids... ids) {
        static_assert(sizeof...(Ids) > 0, "Must provide at least one ID to bind.");
        
        std::vector<std::string> namesVec(names); 
        int i = 0;
        
        // C++17 fold expression passes the config lambda down to every individual bindParam
        ((bindParam<W>(namesVec.at(i++), ids, config)), ...);
    }

    // 1. A helper container to link an ID to its specific customization setup
    template <class W>
    struct ConfigPair {
        int id;
        std::function<void(W*)> config;
    };

// Customize them completely individually while still maintaining a single-line layout code structure
// bindParamsCustom<Trimpot>({"att_1", "att_2"},
//     ConfigPair<Trimpot>{ATT_1, [](Trimpot* w) { /* config 1 */ }},
//     ConfigPair<Trimpot>{ATT_2, [](Trimpot* w) { /* config 2 */ }}
// );
// 2. The batch binder variation
    template <class W, class... Pairs>
    void bindParamsCustom(std::initializer_list<std::string> names, Pairs... pairs) {
        static_assert(sizeof...(Pairs) > 0, "Must provide at least one configuration pair.");
        
        std::vector<std::string> namesVec(names); 
        int i = 0;
        
        // Fold across the pairs pack, unpacking the id and config lambda side-by-side
        ((bindParam<W>(namesVec.at(i++), pairs.id, pairs.config)), ...);
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
struct Compose : Features<T>..., KitAccess<T> { // KitAccess<T> is a single non-virtual base
    SvgKitState kit_;

    // No per-method using-block: each kit method (bindParam, loadPanel, bindLight,
    // forEachMatched, kitStep, ...) is defined in exactly ONE feature mixin, so an
    // unqualified call from the widget resolves by ordinary base-class lookup.
    // This is what makes the variadic opt-in real: composing a SUBSET (e.g.
    // Compose<W, ShapeQuery, Bind> without Reload) still compiles — there is no
    // hard-coded reference to a mixin that might be absent. (The internal self()
    // each mixin declares is only ever used as this->self() inside that mixin, so
    // its duplication across mixins never causes ambiguity at a call site.)
};

}  // namespace dotModular