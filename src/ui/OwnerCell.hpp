#pragma once
#include <rack.hpp>
#include <functional>

// ── OwnerCell ────────────────────────────────────────────────────────────────
// The per-lane ownership control as a lane-step block (Sands "Option C",
// treatment A): a cell drawn in the lane's own colour —
//   FILLED  = global/Macro owns this lane (value comes from the shared base),
//   OUTLINE = local/per-voice owns it (this surface draws its own pattern).
// Left-click toggles (when not locked). It reads as a "17th step" of the lane,
// sitting right of the grid in the SRC column. Shared by East (V2–V16) and Mono (V1).
//
// THREE conditions (see SANDS_OWNERSHIP_SPEC.md):
//   1. outline + operable  — local owns, lane editable, cell toggleable.
//   2. outline + LOCKED    — no Macro to delegate to; cell can't toggle. A small
//                            lock glyph distinguishes it from condition 1.
//   3. filled  (+/- LOCKED)— delegated to Macro; lane tracks Macro & is inoperable.
//                            The CELL stays toggleable (click to un-delegate) UNLESS
//                            lockWhen also holds (e.g. East's V1 can never delegate).
//
// Convention: param value > 0.5 == "local owns" → OUTLINE; <= 0.5 == "Macro owns"
// → FILLED. Matches StraitsEastVisualIds::ownerDispId and SandsMonoVisualIds::ownerDispId
// (1 = local owns).
struct OwnerCell : rack::ParamWidget {
    NVGcolor laneCol = nvgRGB(0x80, 0x80, 0x80);
    std::function<bool()> lockWhen;      // cell can't be toggled (inoperable) — shows a lock glyph
    std::function<bool()> hideWhen;      // (P1: unused now; kept for compatibility)
    OwnerCell() { box.size = rack::math::Vec(18.f, 28.f); }  // sane default; reset in config

    bool locked() const { return lockWhen && lockWhen(); }
    bool hidden() const { return hideWhen && hideWhen(); }
    bool localOwns() {   // value > 0.5 → local (East/Mono) owns → outline (getParamQuantity is non-const)
        return getParamQuantity() && getParamQuantity()->getValue() > 0.5f;
    }
    void toggle() {
        if (!getParamQuantity()) return;
        getParamQuantity()->setValue(localOwns() ? 0.f : 1.f);
    }
    void onButton(const rack::event::Button& e) override {
        if (hidden() || locked()) { if (e.action == GLFW_PRESS) e.consume(this); return; }
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
        const bool lock = locked();
        const float a = lock ? 0.45f : 1.0f;   // locked cells dimmed
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
        if (lock) drawLockGlyph(args, col);
    }
    // Tiny padlock glyph centred in the cell, to mark a locked (inoperable) cell.
    void drawLockGlyph(const DrawArgs& args, NVGcolor base) {
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float bw = std::min(box.size.x, box.size.y) * 0.30f;   // body width
        const float bh = bw * 0.78f;                                  // body height
        const float sh = bh * 0.62f;                                  // shackle height
        NVGcolor g = localOwns() ? base : nvgRGBA(0x16,0x18,0x1c,(unsigned char)(base.a*255));
        // shackle (arc)
        nvgBeginPath(args.vg);
        nvgArc(args.vg, cx, cy - bh*0.35f, bw*0.32f, M_PI, 2.f*M_PI, NVG_CW);
        nvgStrokeColor(args.vg, g);
        nvgStrokeWidth(args.vg, std::max(0.8f, bw*0.16f));
        nvgStroke(args.vg);
        // body
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, cx - bw*0.5f, cy - bh*0.1f, bw, bh, bw*0.16f);
        nvgFillColor(args.vg, g);
        nvgFill(args.vg);
        (void)sh;
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

// ── DirCell ──────────────────────────────────────────────────────────────────
// The per-lane direction control: a 4-state cell cycling Forward → Reverse →
// Pendulum → Ping-pong → Forward on left-click. Draws a direction glyph (→ ← ↔ «»)
// in the lane's colour. Value: 0=Forward, 1=Reverse, 2=Pendulum, 3=PingPong.
// Mirrors the OwnerCell pattern (ParamWidget, click to cycle, lockWhen to disable).
struct DirCell : rack::ParamWidget {
    NVGcolor laneCol = nvgRGB(0x80, 0x80, 0x80);
    std::function<bool()> lockWhen;
    DirCell() { box.size = rack::math::Vec(18.f, 28.f); }

    bool locked() const { return lockWhen && lockWhen(); }
    int state() {   // 0..3
        return getParamQuantity() ? (int)std::round(getParamQuantity()->getValue()) : 0;
    }
    void cycle() {
        if (!getParamQuantity()) return;
        getParamQuantity()->setValue((state() + 1) % 4);
    }
    void onButton(const rack::event::Button& e) override {
        if (locked()) { if (e.action == GLFW_PRESS) e.consume(this); return; }
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            cycle();
            e.consume(this);
            return;
        }
        rack::ParamWidget::onButton(e);
    }
    void draw(const DrawArgs& args) override {
        const float w = box.size.x, h = box.size.y, r = 2.2f;
        const bool lock = locked();
        const float a = lock ? 0.45f : 1.0f;
        NVGcolor col = laneCol; col.a = a;
        // Cell background: rounded rect outline in the lane colour.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 1.0f, 1.0f, w - 2.0f, h - 2.0f, r);
        nvgStrokeColor(args.vg, col);
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
        // Direction glyph centred in the cell.
        drawDirGlyph(args, col);
        if (lock) {
            // Dim overlay for locked cells (simpler than OwnerCell's padlock — just dim the glyph).
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, 1.0f, 1.0f, w - 2.0f, h - 2.0f, r);
            nvgFillColor(args.vg, nvgRGBA(0x16, 0x18, 0x1c, 0xb0));
            nvgFill(args.vg);
        }
    }
    void drawDirGlyph(const DrawArgs& args, NVGcolor col) {
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float s = std::min(box.size.x, box.size.y) * 0.28f;
        NVGcolor g = col;
        nvgStrokeColor(args.vg, g);
        nvgStrokeWidth(args.vg, std::max(1.0f, s * 0.35f));
        nvgLineCap(args.vg, NVG_ROUND);
        switch (state()) {
            case 0:  // Forward →
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx - s, cy);
                nvgLineTo(args.vg, cx + s * 0.7f, cy);
                nvgStroke(args.vg);
                // arrowhead
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx + s * 0.7f, cy);
                nvgLineTo(args.vg, cx + s * 0.3f, cy - s * 0.35f);
                nvgMoveTo(args.vg, cx + s * 0.7f, cy);
                nvgLineTo(args.vg, cx + s * 0.3f, cy + s * 0.35f);
                nvgStroke(args.vg);
                break;
            case 1:  // Reverse ←
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx + s, cy);
                nvgLineTo(args.vg, cx - s * 0.7f, cy);
                nvgStroke(args.vg);
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx - s * 0.7f, cy);
                nvgLineTo(args.vg, cx - s * 0.3f, cy - s * 0.35f);
                nvgMoveTo(args.vg, cx - s * 0.7f, cy);
                nvgLineTo(args.vg, cx - s * 0.3f, cy + s * 0.35f);
                nvgStroke(args.vg);
                break;
            case 2:  // Pendulum ↔
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx - s * 0.6f, cy);
                nvgLineTo(args.vg, cx + s * 0.6f, cy);
                nvgStroke(args.vg);
                // left arrowhead
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx - s * 0.6f, cy);
                nvgLineTo(args.vg, cx - s * 0.3f, cy - s * 0.35f);
                nvgMoveTo(args.vg, cx - s * 0.6f, cy);
                nvgLineTo(args.vg, cx - s * 0.3f, cy + s * 0.35f);
                // right arrowhead
                nvgMoveTo(args.vg, cx + s * 0.6f, cy);
                nvgLineTo(args.vg, cx + s * 0.3f, cy - s * 0.35f);
                nvgMoveTo(args.vg, cx + s * 0.6f, cy);
                nvgLineTo(args.vg, cx + s * 0.3f, cy + s * 0.35f);
                nvgStroke(args.vg);
                break;
            case 3:  // Ping-pong «» (double arrows)
                // left pair
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx - s * 0.2f, cy - s * 0.4f);
                nvgLineTo(args.vg, cx - s * 0.7f, cy);
                nvgLineTo(args.vg, cx - s * 0.2f, cy + s * 0.4f);
                // right pair
                nvgMoveTo(args.vg, cx + s * 0.2f, cy - s * 0.4f);
                nvgLineTo(args.vg, cx + s * 0.7f, cy);
                nvgLineTo(args.vg, cx + s * 0.2f, cy + s * 0.4f);
                nvgStroke(args.vg);
                break;
        }
    }
};
