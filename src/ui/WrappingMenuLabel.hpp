#pragma once
// ── redDot::WrappingMenuLabel — a MenuLabel that WORD-WRAPS to a fixed width ─────────────────────
// Fixes SCALA_FILE_AND_LOAD_UI.md §"BUG + FIX: long .scl description blows out the context menu width".
// Standard rack::ui::MenuLabel sizes the whole menu to its widest label, so a long .scl description
// (user file content, unbounded) forces an absurdly wide menu. This subclass caps the width and wraps
// the text across as many lines as needed — the menu width is bounded by maxWidth, the full text stays
// visible. Reusable across the module family (built for the Colonnades/Duo "Loaded: <desc>" label).
//
// Word-wrap is greedy by whitespace, measured with bndLabelWidth (the same metric MenuLabel::step uses),
// so it matches the menu font. A single over-long word (no spaces) is hard-broken so it can't overflow.

#include <rack.hpp>
#include <string>
#include <vector>
#include <sstream>

namespace redDot {

struct WrappingMenuLabel : rack::ui::MenuLabel {
    float maxWidth = 220.f;              // px cap (menu won't grow past this + padding)
    std::vector<std::string> lines_;     // wrapped output, rebuilt in step()
    std::string lastText_;               // rebuild only when text changes
    float lineH_ = 0.f;

    // Measure a string in the menu label font.
    static float measure(const std::string& s) {
        return bndLabelWidth(APP->window->vg, -1, s.c_str());
    }

    void rewrap() {
        lines_.clear();
        lineH_ = std::max(1.f, (float)BND_WIDGET_HEIGHT * 0.7f);   // compact line spacing for wrapped text
        std::istringstream iss(text);
        std::string word, line;
        auto flush = [&]{ if (!line.empty()) { lines_.push_back(line); line.clear(); } };
        while (iss >> word) {
            // Hard-break a single word longer than maxWidth (no whitespace to wrap on).
            while (measure(word) > maxWidth && word.size() > 1) {
                std::string head;
                for (char c : word) {
                    if (measure(head + c) > maxWidth && !head.empty()) break;
                    head += c;
                }
                flush();
                lines_.push_back(head);
                word = word.substr(head.size());
            }
            std::string cand = line.empty() ? word : (line + " " + word);
            if (!line.empty() && measure(cand) > maxWidth) { flush(); line = word; }
            else                                            { line = cand; }
        }
        flush();
        if (lines_.empty()) lines_.push_back("");
    }

    void step() override {
        if (text != lastText_) { lastText_ = text; rewrap(); }
        const float rightPadding = 10.f;
        float w = 0.f;
        for (const auto& l : lines_) w = std::max(w, measure(l));
        box.size.x = std::min(w, maxWidth) + rightPadding;
        box.size.y = std::max((float)BND_WIDGET_HEIGHT, (float)lines_.size() * lineH_);
        rack::widget::Widget::step();    // NOT MenuLabel::step (it would resize to the full string)
    }

    void draw(const DrawArgs& args) override {
        float y = 0.f;
        for (const auto& l : lines_) {
            bndMenuLabel(args.vg, 0.f, y, box.size.x, lineH_, -1, l.c_str());
            y += lineH_;
        }
    }
};

// Convenience: build a wrapping label carrying `str`, ready to menu->addChild().
inline WrappingMenuLabel* makeWrappingMenuLabel(const std::string& str, float maxWidth = 220.f) {
    auto* w = new WrappingMenuLabel();
    w->text = str;
    w->maxWidth = maxWidth;
    return w;
}

} // namespace redDot
