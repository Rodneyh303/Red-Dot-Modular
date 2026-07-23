#pragma once
// StoreKnob<TModule> — a knob bound to a STORE FIELD, drawing itself.
//
// The visual/editor expanders must expose NOTHING to host automation
// (PARAM_CLASSIFICATION.md), so their knobs cannot be Rack params.
//
// HISTORY (why this does NOT derive from SvgKnob):
// The first version layered over Controls.hpp's KnobT (i.e. app::SvgKnob) to reuse the RDM
// artwork. It did not render. SvgKnob draws through a FramebufferWidget whose refresh Rack
// drives from inside `if (getParamQuantity())` — and a de-parammed knob has no
// ParamQuantity, so the framebuffer never refreshed. Forcing fb->dirty every frame STILL
// did not render (the knob appeared only on frames the flag was set — it "flashed" while
// dragging), so the FramebufferWidget was not retaining content for a param-less knob at
// all. Rather than keep fighting inherited behaviour that assumes a param exists, this
// draws the SVG directly with its own rotation transform: no framebuffer, no ParamWidget,
// no ParamQuantity. Fully predictable, and it still uses the same res/controls faces.
//
// Undo: one history action per DRAG (StoreEditCoalescer), matching Rack's ParamChange.
// TModule = the module that OWNS the store (the action keeps its id, not a pointer).

#include <rack.hpp>
#include "StoreEditAction.hpp"

// The plugin global, used to resolve control-face assets. Declared here rather than
// pulled in via plugin.hpp, matching Controls.hpp (which does exactly the same).
extern rack::plugin::Plugin* pluginInstance;

namespace redDot {

template <typename TModule>
struct StoreKnob : rack::widget::Widget {
    // ── artwork ──
    std::shared_ptr<rack::window::Svg> svg;
    float minAngle = -0.83f * (float)M_PI;   // matches Controls.hpp KnobT sweep
    float maxAngle =  0.83f * (float)M_PI;

    // ── binding ──
    // storeModule: the module that OWNS the store. For Lantern that is the widget's own
    // module; for the Sands expanders the store lives on MONSOON, which may not be attached
    // when the widget is built and can be attached/detached later. So prefer resolveStore
    // (a lambda evaluated at use time); storeModule is the static fallback.
    TModule*                             storeModule = nullptr;
    std::function<TModule*()>            resolveStore;
    TModule* store() const { return resolveStore ? resolveStore() : storeModule; }
    std::function<float()>               getValue;
    std::function<void(TModule&, float)> setter;
    std::string label = "value";
    float minValue = 0.f, maxValue = 1.f, defaultValue = 0.f;
    bool  snap = false;

    // ── tooltip (de-paramming loses the ParamQuantity tooltip; switch-like knobs need it) ──
    std::vector<std::string>     valueLabels;
    std::function<std::string()> tooltipTextFn;
    rack::ui::Tooltip*           tooltip_ = nullptr;

    // ── internal ──
    StoreEditCoalescer coalesce;
    float dragValue = 0.f;

    void setSvg(std::shared_ptr<rack::window::Svg> s) {
        svg = s;
        if (svg && svg->handle)
            box.size = rack::math::Vec(svg->handle->width, svg->handle->height);
    }

    float norm() const {
        float v = getValue ? getValue() : defaultValue;
        float t = (maxValue > minValue) ? (v - minValue) / (maxValue - minValue) : 0.f;
        return rack::math::clamp(t, 0.f, 1.f);
    }

    void draw(const DrawArgs& args) override {
        if (!svg || !svg->handle) return;
        const float angle = rack::math::rescale(norm(), 0.f, 1.f, minAngle, maxAngle);
        nvgSave(args.vg);
        nvgTranslate(args.vg, box.size.x * 0.5f, box.size.y * 0.5f);
        nvgRotate(args.vg, angle);
        nvgTranslate(args.vg, -box.size.x * 0.5f, -box.size.y * 0.5f);
        rack::window::svgDraw(args.vg, svg->handle);
        nvgRestore(args.vg);
    }

    // ── drag: write the store live, one undo per drag ──
    void onDragStart(const rack::event::DragStart& e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        dragValue = getValue ? getValue() : defaultValue;
        coalesce.begin(dragValue);
        APP->window->cursorLock();
        e.consume(this);
    }

    void onDragMove(const rack::event::DragMove& e) override {
        const float range = maxValue - minValue;
        const int   mods  = APP->window->getMods();   // DragMoveEvent has no mods field
        const float speed = (mods & GLFW_MOD_SHIFT) ? 0.05f : 0.5f;
        dragValue += -e.mouseDelta.y * speed / 200.f * range;   // up = positive
        dragValue = rack::math::clamp(dragValue, minValue, maxValue);
        const float applied = snap ? std::round(dragValue) : dragValue;
        if (auto* m = store()) { if (setter) setter(*m, applied); }
        if (tooltip_) tooltip_->text = tooltipText();
    }

    void onDragEnd(const rack::event::DragEnd& e) override {
        APP->window->cursorUnlock();
        auto* m = store();
        if (!m || !setter) { coalesce.cancel(); return; }
        const float endV = snap ? std::round(dragValue) : dragValue;
        coalesce.commit<TModule>(m, label, setter, endV);
    }

    // A plain Widget does not become the hovered widget unless it CONSUMES onHover, and
    // without that onEnter/onLeave never fire -> no tooltip. (SvgKnob got this free from
    // ParamWidget; we do not.)
    void onHover(const rack::event::Hover& e) override { e.consume(this); }

    void onButton(const rack::event::Button& e) override {
        // Claim the press so the drag events reach us.
        if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) e.consume(this);
        Widget::onButton(e);
    }

    void onDoubleClick(const rack::event::DoubleClick& e) override {
        auto* m = store();
        if (!m || !setter) return;
        const float oldV = getValue ? getValue() : defaultValue;
        applyAndPushStoreEdit<TModule>(m, label, setter, oldV, defaultValue);
    }

    // ── tooltip ──
    std::string tooltipText() {
        if (tooltipTextFn) return tooltipTextFn();
        const float v = getValue ? getValue() : defaultValue;
        if (!valueLabels.empty()) {
            int i = rack::math::clamp((int)std::round(v), 0, (int)valueLabels.size() - 1);
            return label + ": " + valueLabels[(size_t)i];
        }
        return "";
    }

    void onEnter(const rack::event::Enter& e) override {
        const std::string txt = tooltipText();
        if (!txt.empty() && !tooltip_) {
            tooltip_ = new rack::ui::Tooltip;
            tooltip_->text = txt;
            APP->scene->addChild(tooltip_);
        }
        Widget::onEnter(e);
    }
    void onLeave(const rack::event::Leave& e) override {
        if (tooltip_) { APP->scene->removeChild(tooltip_); delete tooltip_; tooltip_ = nullptr; }
        Widget::onLeave(e);
    }
    ~StoreKnob() { if (tooltip_) { APP->scene->removeChild(tooltip_); delete tooltip_; } }
};

// ── ONE configuration path for every de-parammed control ─────────────────────
// CONSISTENCY RULE (Rodney): every de-parammed control — already migrated or not, mono or
// poly, kit-panel or hand-placed — is wired the SAME way. Differences may come only from
// genuine differences (where the store lives, how the panel places widgets), never from
// the order modules happened to be migrated.
//
// Face is a generator-emitted Tag (Controls.hpp `Tag_<Palette>_<Size>_<Style>`), not a
// path string: a mis-named face becomes a COMPILE error rather than a knob that silently
// fails to render.
//
// The store owner is ALWAYS supplied as a resolver lambda, even when it is trivially the
// widget's own module — so no call site differs merely because its store happens to be local.
template <class M, class Tag, class W>
void configureStoreKnob(W* w,
                        std::function<M*()> resolve,
                        float lo, float hi, float def,
                        std::string label,
                        std::function<float(M&)> get,
                        std::function<void(M&, float)> set,
                        bool snap = false,
                        std::vector<std::string> labels = {}) {
    w->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, Tag::path())));
    w->resolveStore = resolve;
    w->minValue = lo; w->maxValue = hi; w->defaultValue = def;
    w->label = label;
    w->snap = snap;
    w->valueLabels = labels;
    // getter and setter sit ADJACENT sharing their captured indices: a mismatch between
    // them is the characteristic de-param bug (compiles, silently wrong value), and this
    // makes it visible on one line.
    w->getValue = [resolve, get]() { M* m = resolve(); return m ? get(*m) : 0.f; };
    w->setter   = set;
}

// Placement A — panels using the SVG kit (named shapes). Returns the widget.
template <class M, class Tag, class Self>
StoreKnob<M>* bindStoreKnob(Self* self, const std::string& shape,
                            std::function<M*()> resolve,
                            float lo, float hi, float def, std::string label,
                            std::function<float(M&)> get, std::function<void(M&, float)> set,
                            bool snap = false, std::vector<std::string> labels = {}) {
    using SK = StoreKnob<M>;
    return self->template bindWidget<SK>(shape, std::function<void(SK*)>([=](SK* w) {
        configureStoreKnob<M, Tag>(w, resolve, lo, hi, def, label, get, set, snap, labels);
    }));
}

// Placement B — panels placing widgets by explicit mm coordinates (no named shapes).
// Same configuration path; only the positioning differs.
template <class M, class Tag, class Self>
StoreKnob<M>* placeStoreKnob(Self* self, rack::math::Vec centreMm,
                             std::function<M*()> resolve,
                             float lo, float hi, float def, std::string label,
                             std::function<float(M&)> get, std::function<void(M&, float)> set,
                             bool snap = false, std::vector<std::string> labels = {}) {
    auto* w = rack::createWidget<StoreKnob<M>>(rack::math::Vec(0, 0));
    configureStoreKnob<M, Tag>(w, resolve, lo, hi, def, label, get, set, snap, labels);
    w->box.pos = rack::mm2px(centreMm).minus(w->box.size.div(2));   // config first: sets box.size
    self->addChild(w);
    return w;
}

} // namespace redDot
