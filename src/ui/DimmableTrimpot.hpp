#pragma once
#include <rack.hpp>
#include <functional>
#include <cmath>

// A Trimpot with three optional behaviours, all driven by lambdas so a consumer can bind them
// to a topology/ownership predicate:
//   dimWhen        — draw at reduced alpha (control unavailable / not the target)
//   lockWhen       — swallow input (inoperable) — for an inherited/delegated/locked lane
//   displayValueFn — RENDER a display value while the bound param (the STORE that save/restore
//                    reads) stays UNTOUCHED. So a ceded lane's knob can SHOW the owner's value
//                    (e.g. Macro's spread) without clobbering the local stored value — reclaiming
//                    the lane reverts to the stored value automatically. Return NaN = no override.
//
// See docs/design/DISPLAY_STORE_ENGINE_SEPARATION.md for the display/store split rationale.
// Originally defined inline in StraitsEastSandsVisual.cpp; extracted here so the Mono/Sands
// visual can share the exact same lockable/display-mirroring trimpot instead of a divergent copy.
//
// NOTE: displayValueFn is implemented by presenting the display value to the base step() (which
// sets the knob rotation from the ParamQuantity) then restoring the store in the SAME step, so the
// store is never observably changed. This is the one piece that needs an in-Rack build to confirm
// (no SDK headers in-container); guarded so a null/!finite case is a plain knob.
struct DimmableTrimpot : rack::componentlibrary::Trimpot {
    std::function<bool()>  dimWhen;
    std::function<bool()>  lockWhen;
    std::function<float()> displayValueFn;
    bool locked() const { return lockWhen && lockWhen(); }

    void step() override {
        rack::engine::ParamQuantity* pq = getParamQuantity();
        if (pq && displayValueFn) {
            float dv = displayValueFn();
            if (std::isfinite(dv)) {
                float stored = pq->getValue();
                if (dv != stored) {
                    pq->setValue(dv);                          // present display value
                    rack::componentlibrary::Trimpot::step();   // base sets rotation from it
                    pq->setValue(stored);                      // restore store (same frame)
                    return;
                }
            }
        }
        rack::componentlibrary::Trimpot::step();
    }
    void onButton(const rack::event::Button& e) override {
        if (locked()) { e.consume(this); return; }
        rack::componentlibrary::Trimpot::onButton(e);
    }
    void onDragStart(const rack::event::DragStart& e) override {
        if (locked()) return;
        rack::componentlibrary::Trimpot::onDragStart(e);
    }
    void onDragMove(const rack::event::DragMove& e) override {
        // The actual value change happens here during a drag — guard it too, or a
        // locked knob can still be moved even though onDragStart was blocked.
        if (locked()) { e.consume(this); return; }
        rack::componentlibrary::Trimpot::onDragMove(e);
    }
    void onHoverScroll(const rack::event::HoverScroll& e) override {
        if (locked()) { e.consume(this); return; }   // scroll-wheel also changes value
        rack::componentlibrary::Trimpot::onHoverScroll(e);
    }
    void draw(const DrawArgs& args) override {
        // Locked no longer forces dim — a locked-but-shown control (e.g. V1 spread
        // mirroring Mono) must stay readable. Only dimWhen dims (truly unavailable).
        bool dim = (dimWhen && dimWhen());
        if (dim) nvgGlobalAlpha(args.vg, 0.4f);
        rack::componentlibrary::Trimpot::draw(args);
        if (dim) nvgGlobalAlpha(args.vg, 1.0f);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        bool dim = (dimWhen && dimWhen());
        if (dim) nvgGlobalAlpha(args.vg, 0.4f);
        rack::componentlibrary::Trimpot::drawLayer(args, layer);
        if (dim) nvgGlobalAlpha(args.vg, 1.0f);
    }
};
