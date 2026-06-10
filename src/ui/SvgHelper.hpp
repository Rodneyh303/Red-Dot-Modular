#pragma once
// dotModular::SvgHelper — a small CRTP mixin that binds widgets to named shapes
// in the module's panel SVG, with optional dev-mode live-reload (poll the file
// mtime, re-parse, reposition bound widgets). Lets the panel SVG own control
// POSITIONS (authored by our data-driven generator, or dragged in Inkscape),
// while the widget owns logic and id arithmetic.
//
// In-house implementation (no external dependency), modelled on the feature set
// of dustinlacewell/vcv-svghelper (MIT). Namespaced dotModular for branding.
//
// Usage:
//   struct MyWidget : ModuleWidget, dotModular::SvgHelper<MyWidget> {
//     MyWidget(MyModule* m) {
//       setModule(m);
//       loadPanel(asset::plugin(pluginInstance, "res/panels/My_dark.svg"));
//       bindParam<RoundBlackKnob>("param_FREQ", MyModule::FREQ_PARAM);
//       forEachMatched("light_ring_(\\d+)", [&](auto cap, NSVGshape* s){
//         int i = std::stoi(cap[0]);
//         bindLight<RedLight>(s, MyModule::RING_LIGHT + i);
//       });
//     }
//     void step() override { ModuleWidget::step(); SvgHelper::step(); }
//   };

#include <sys/stat.h>
#include <map>
#include <regex>
#include <string>
#include <vector>
#include <functional>

#include "rack.hpp"

namespace dotModular {

using namespace rack;

template <typename T>
struct SvgHelper {
   private:
    bool devMode = false;
    bool pollMode = false;
    double prevPollTime = 0.0;
    struct stat prevStatBuf = {};
    std::string svgFileName;

    app::SvgPanel* panel = nullptr;
    std::map<std::string, Widget*> bound;

    ModuleWidget* mw() { return static_cast<ModuleWidget*>(static_cast<T*>(this)); }

   public:
    Vec center(NSVGshape* shape) {
        const float* b = shape->bounds;
        return Vec((b[0] + b[2]) / 2.f, (b[1] + b[3]) / 2.f);
    }

    void setDevMode(bool v) { devMode = v; }
    void setDirty() { if (panel && panel->fb) panel->fb->dirty = true; }

    void loadPanel(const std::string& filename) {
        svgFileName = filename;
        if (!panel) {
            panel = createPanel(filename);
            mw()->setPanel(panel);
        } else {
            reloadPanel();
        }
    }

    void appendContextMenu(Menu* menu) {
        if (!devMode || svgFileName.empty()) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuItem("Reload panel", "", [this]() { reloadPanel(); }));
        menu->addChild(createBoolPtrMenuItem("Poll SVG for reload", "", &pollMode));
    }

    // ── Shape lookup ─────────────────────────────────────────────────────────
    void forEachShape(const std::function<void(NSVGshape*)>& cb) {
        if (!panel || !panel->svg || !panel->svg->handle) return;
        for (NSVGshape* s = panel->svg->handle->shapes; s; s = s->next) cb(s);
    }

    NSVGshape* findNamed(const std::string& name) {
        NSVGshape* r = nullptr;
        forEachShape([&](NSVGshape* s) { if (!r && name == s->id) r = s; });
        return r;
    }

    std::vector<NSVGshape*> findPrefixed(const std::string& prefix) {
        std::vector<NSVGshape*> r;
        forEachShape([&](NSVGshape* s) {
            if (std::string(s->id).rfind(prefix, 0) == 0) r.push_back(s);
        });
        return r;
    }

    std::vector<std::pair<std::vector<std::string>, NSVGshape*>>
    findMatched(const std::string& pattern) {
        std::regex re(pattern);
        std::vector<std::pair<std::vector<std::string>, NSVGshape*>> out;
        forEachShape([&](NSVGshape* s) {
            std::string id(s->id);
            std::smatch m;
            if (std::regex_search(id, m, re)) {
                std::vector<std::string> caps;
                for (unsigned i = 1; i < m.size(); ++i) caps.push_back(m[i]);
                out.emplace_back(caps, s);
            }
        });
        return out;
    }

    void forEachPrefixed(const std::string& prefix,
                         const std::function<void(unsigned, NSVGshape*)>& cb) {
        auto v = findPrefixed(prefix);
        for (unsigned i = 0; i < v.size(); ++i) cb(i, v[i]);
    }

    void forEachMatched(const std::string& pattern,
                        const std::function<void(std::vector<std::string>, NSVGshape*)>& cb) {
        for (auto& m : findMatched(pattern)) cb(m.first, m.second);
    }

    // ── Binding (records the widget so reload can reposition it) ──────────────
    template <class W> void bindParam(NSVGshape* s, int id) {
        auto* w = createParamCentered<W>(center(s), mw()->module, id);
        mw()->addParam(w); bound[s->id] = w;
    }
    template <class W> void bindParam(const std::string& name, int id) {
        if (auto* s = findNamed(name)) bindParam<W>(s, id);
        else WARN("[SvgHelper] param shape not found: %s", name.c_str());
    }
    template <class W> void bindInput(NSVGshape* s, int id) {
        auto* w = createInputCentered<W>(center(s), mw()->module, id);
        mw()->addInput(w); bound[s->id] = w;
    }
    template <class W> void bindInput(const std::string& name, int id) {
        if (auto* s = findNamed(name)) bindInput<W>(s, id);
        else WARN("[SvgHelper] input shape not found: %s", name.c_str());
    }
    template <class W> void bindOutput(NSVGshape* s, int id) {
        auto* w = createOutputCentered<W>(center(s), mw()->module, id);
        mw()->addOutput(w); bound[s->id] = w;
    }
    template <class W> void bindOutput(const std::string& name, int id) {
        if (auto* s = findNamed(name)) bindOutput<W>(s, id);
        else WARN("[SvgHelper] output shape not found: %s", name.c_str());
    }
    template <class W> void bindLight(NSVGshape* s, int id) {
        auto* w = createLightCentered<W>(center(s), mw()->module, id);
        mw()->addChild(w); bound[s->id] = w;
    }
    template <class W> void bindLight(const std::string& name, int id) {
        if (auto* s = findNamed(name)) bindLight<W>(s, id);
        else WARN("[SvgHelper] light shape not found: %s", name.c_str());
    }
    template <class W> void bindParamLight(NSVGshape* s, int paramId, int lightId) {
        auto* w = createLightParamCentered<W>(center(s), mw()->module, paramId, lightId);
        mw()->addParam(w); bound[s->id] = w;
    }

    // ── Dev-mode live reload (poll mtime once a second) ───────────────────────
    void step() {
        if (!pollMode || svgFileName.empty()) return;
        double now = system::getTime();
        if (now - prevPollTime < 1.0) return;
        prevPollTime = now;
        struct stat sb;
        if (stat(svgFileName.c_str(), &sb) == 0 &&
            memcmp(&sb.st_mtime, &prevStatBuf.st_mtime, sizeof(sb.st_mtime)) != 0) {
            prevStatBuf = sb;
            reloadPanel();
        }
    }

   private:
    void reloadPanel() {
        if (svgFileName.empty() || !panel || !panel->svg) return;
        NSVGimage* repl = nsvgParseFromFile(svgFileName.c_str(), "px", SVG_DPI);
        if (!repl) { WARN("[SvgHelper] cannot parse %s", svgFileName.c_str()); return; }
        if (panel->svg->handle) nsvgDelete(panel->svg->handle);
        panel->svg->handle = repl;
        forEachShape([&](NSVGshape* s) {
            auto it = bound.find(s->id);
            if (it != bound.end()) {
                Widget* w = it->second;
                w->box.pos = center(s).minus(w->box.size.div(2));
            }
        });
        setDirty();
    }
};

}  // namespace dotModular
