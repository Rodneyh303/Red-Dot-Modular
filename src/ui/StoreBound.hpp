#pragma once
// StoreBound<Base> — layer a STORE binding (get/set + undo) onto any dot.modular knob.
//
// The visual/editor expanders must expose NOTHING to host automation
// (PARAM_CLASSIFICATION.md: "visual/editor expanders expose nothing"). Their knobs
// therefore must not be Rack params. But the existing Controls.hpp already gives us
// everything EXCEPT the write path:
//   • KnobT<Tag> / ThemedKnobT — the RDM artwork, shadow kill, theme swap.
//   • Dimmable<Base>::displayValueFn — RENDER a value from an arbitrary source while the
//     bound ParamQuantity stays untouched (already used for spread-follow on East/Macro).
// So a store-backed knob = existing artwork + Dimmable's display trick + THIS write path.
//
// StoreBound composes OVER a knob Base (KnobT or Dimmable<KnobT>), the same way Dimmable
// composes over KnobT. It:
//   • reads its displayed value from getValue() (feeds Dimmable::displayValueFn if present,
//     else drives rotation directly),
//   • writes the STORE on drag (live), recording ONE undo action per drag via
//     StoreEditAction (module-id-safe, matches Rack ParamChange),
//   • needs NO configParam / ParamQuantity — the module drops the config call.
//
// TModule = the module that OWNS the store (Monsoon today; the store lives on Monsoon even
// when the knob widget sits on an expander — the action keeps the module id, not a pointer).
//
// NOTE: Controls.hpp is generator-owned ("do not hand-edit"); this behaviour lives here so
// it can be regenerated-around and composed per binding site.

#include <rack.hpp>
#include "StoreEditAction.hpp"

namespace redDot {

template <typename Base, typename TModule>
struct StoreBound : Base {
    TModule*                              storeModule  = nullptr;   // resolves + owns the store
    std::function<float()>               getValue;                  // store -> display value
    std::function<void(TModule&, float)> setter;                    // write store (live + undo)
    std::string label = "value";
    float minValue = 0.f, maxValue = 1.f, defaultValue = 0.f;
    bool  snap = false;

    // Tooltip. De-paramming loses the ParamQuantity tooltip, which for SWITCH-LIKE knobs
    // (configSwitch with value labels: "Notes/Velocity/Prob", "x1/x2/x4", "Grid/Piano roll")
    // is the only thing telling the user which position they are in — a real UX regression,
    // not cosmetic. Supply either:
    //   valueLabels — discrete labels, indexed by the snapped value, or
    //   tooltipTextFn — full control (continuous knobs: format the number + unit).
    // If neither is set, no tooltip is shown (fine for self-evident controls).
    std::vector<std::string>     valueLabels;
    std::function<std::string()> tooltipTextFn;
    rack::ui::Tooltip*           tooltip_ = nullptr;

    std::string tooltipText() {
        if (tooltipTextFn) return tooltipTextFn();
        float v = getValue ? getValue() : defaultValue;
        if (!valueLabels.empty()) {
            int i = rack::math::clamp((int)std::round(v), 0, (int)valueLabels.size() - 1);
            return label + ": " + valueLabels[(size_t)i];
        }
        return "";
    }

    void onEnter(const rack::event::Enter& e) override {
        std::string txt = tooltipText();
        if (!txt.empty() && !tooltip_) {
            tooltip_ = new rack::ui::Tooltip;
            tooltip_->text = txt;
            APP->scene->addChild(tooltip_);
        }
        Base::onEnter(e);
    }
    void onLeave(const rack::event::Leave& e) override {
        if (tooltip_) { APP->scene->removeChild(tooltip_); delete tooltip_; tooltip_ = nullptr; }
        Base::onLeave(e);
    }
    ~StoreBound() { if (tooltip_) { APP->scene->removeChild(tooltip_); delete tooltip_; } }

    StoreEditCoalescer coalesce;
    float dragValue = 0.f;

    StoreBound() {
        // Drive the artwork from the store without a ParamQuantity: Base (via Dimmable)
        // reads displayValueFn if set. If Base is a plain KnobT we override step() below.
    }

    // Map store value -> normalized 0..1 for rotation.
    float norm() const {
        float v = getValue ? getValue() : defaultValue;
        float t = (maxValue > minValue) ? (v - minValue) / (maxValue - minValue) : 0.f;
        return rack::math::clamp(t, 0.f, 1.f);
    }

    // Present the store value as rotation each frame. We set the SvgKnob transform (tw)
    // directly, so no ParamQuantity is required. API note: SvgKnob::tw/fb/shadow are
    // public members in Rack v2 -- Controls.hpp already writes shadow->opacity and
    // fb->dirty from subclasses, and tw is their sibling, so this is consistent with
    // existing access. (Unverifiable in the container: no Rack SDK to compile against.)
    void step() override {
        if (getValue) {
            float angle = rack::math::rescale(norm(), 0.f, 1.f, this->minAngle, this->maxAngle);
            if (this->tw) { this->tw->identity(); this->tw->rotate(angle); }
        }
        Base::step();
    }

    void onDragStart(const rack::event::DragStart& e) override {
        if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        dragValue = getValue ? getValue() : defaultValue;
        coalesce.begin(dragValue);
        APP->window->cursorLock();
        e.consume(this);
    }

    void onDragMove(const rack::event::DragMove& e) override {
        float range = maxValue - minValue;
        // DragMoveEvent has no mods field; use APP->window for modifier state.
        int mods = APP->window->getMods();
        float speed = (mods & GLFW_MOD_SHIFT) ? 0.05f : 0.5f;
        dragValue += -e.mouseDelta.y * speed / 200.f * range;   // up = positive
        dragValue = rack::math::clamp(dragValue, minValue, maxValue);
        float applied = snap ? std::round(dragValue) : dragValue;
        if (storeModule && setter) setter(*storeModule, applied);   // live store write
        if (tooltip_) tooltip_->text = tooltipText();                // keep readout live
    }

    void onDragEnd(const rack::event::DragEnd& e) override {
        APP->window->cursorUnlock();
        if (!storeModule || !setter) { coalesce.cancel(); return; }
        float endV = snap ? std::round(dragValue) : dragValue;
        coalesce.commit<TModule>(storeModule, label, setter, endV);   // ONE undo per drag
    }

    void onDoubleClick(const rack::event::DoubleClick& e) override {
        if (!storeModule || !setter) return;
        float oldV = getValue ? getValue() : defaultValue;
        applyAndPushStoreEdit<TModule>(storeModule, label, setter, oldV, defaultValue);
    }
};

} // namespace redDot
