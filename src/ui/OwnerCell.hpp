#pragma once
#include <rack.hpp>
#include <functional>

// ── OwnerCell ────────────────────────────────────────────────────────────────
// The per-lane ownership control as a lane-step block (Sands "Option C",
// treatment A): a cell drawn in the lane's own colour —
//   FILLED  = global/Macro owns this lane (value comes from the shared base),
//   OUTLINE = local/per-voice owns it (this surface draws its own pattern).
// Left-click toggles. It reads as a "17th step" of the lane, sitting right of the
// grid in the SRC column. Shared by East (per-voice V2–V16) and Mono (V1).
//
// Convention: param value > 0.5 == "local owns" (East/Mono draws it) → OUTLINE;
//             value <= 0.5 == "global/Macro owns" → FILLED. This matches both
// StraitsEastVisualIds::ownerDispId (1=East owns) and
// SandsMonoVisualIds::ownerDispId (1=Mono owns).
struct OwnerCell : rack::ParamWidget {
    NVGcolor laneCol = nvgRGB(0x80, 0x80, 0x80);
    std::function<bool()> inertWhen;     // e.g. no Macro attached → inert + dimmed
    std::function<bool()> hideWhen;      // e.g. V1 mono-mirror tab on East → hidden
    OwnerCell() { box.size = rack::math::Vec(18.f, 28.f); }  // sane default; reset in config

    bool inert()  const { return inertWhen && inertWhen(); }
    bool hidden() const { return hideWhen && hideWhen(); }
    bool localOwns() {   // value > 0.5 → local (East/Mono) owns → outline (getParamQuantity is non-const)
        return getParamQuantity() && getParamQuantity()->getValue() > 0.5f;
    }
    void toggle() {
        if (!getParamQuantity()) return;
        getParamQuantity()->setValue(localOwns() ? 0.f : 1.f);
    }
    void onButton(const rack::event::Button& e) override {
        if (hidden() || inert()) { if (e.action == GLFW_PRESS) e.consume(this); return; }
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            toggle();
            e.consume(this);
            return;
        }
        rack::ParamWidget::onButton(e);
    }
    void draw(const DrawArgs& args) override {
        if (hidden()) return;
        const float w = box.size.x, h = box.size.y, r = 2.2f;
        const float a = inert() ? 0.4f : 1.0f;
        NVGcolor col = laneCol; col.a = a;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.0f, 1.0f, w - 2.0f, h - 2.0f, r);
        if (localOwns()) {                 // local/per-voice → hollow outline
            nvgStrokeColor(args.vg, col);
            nvgStrokeWidth(args.vg, 1.4f);
            nvgStroke(args.vg);
        } else {                           // global/Macro → solid fill
            NVGcolor fillc = laneCol; fillc.a = a * 0.92f;
            nvgFillColor(args.vg, fillc);
            nvgFill(args.vg);
        }
    }
};

// Sands poly-lane colours by EDITOR lane (0=MEL,1=OCT,2=REST,3=ACC). Matches
// SandsVisualEditorV4 lane colours.
inline NVGcolor sandsLaneColorEditor(int editorLane) {
    switch (editorLane) {
        case 0: return nvgRGB(0xd4, 0xaf, 0x37);  // MELODY gold
        case 1: return nvgRGB(0xb8, 0x86, 0x0b);  // OCTAVE darker gold
        case 2: return nvgRGB(0x50, 0x50, 0x50);  // REST grey
        case 3: return nvgRGB(0xff, 0x95, 0x00);  // ACCENT orange
        default: return nvgRGB(0x80, 0x80, 0x80);
    }
}
