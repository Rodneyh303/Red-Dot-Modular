#pragma once
#include <cstddef>
#include <span>

namespace redDot {

// ── RingView — the wrapping-window STORAGE read (CA5's lane-addressing idiom) ──
// CC_PROMPT_REVERSE_COMPLETIONS_THEN_CA5 Part 2, the span/RingView sub-step
// (Rodney, 2026-11-21): a window that wraps the ring (O=30, L=4 on the 32-ring)
// cannot be one span — C++20 has no cyclic view (views::concat is C++26) — so this
// is the ONE named law for the arithmetic the masked row reads already do:
//
//     operator[](i) = data[(O + i) & (ring - 1)]        // ring == data.size()
//
// (frameReadCell's fold and PatternEngine's `step & (kPatternLengthCap - 1)`
// row read are the same expression). STORAGE ACCESS ONLY: the position math
// (walkFold / laneTickFor / walkKeyOf / the rho/wnow fold) stays integer
// arithmetic — the doc's NOT-in-scope (the abs1 extent bug lived there, which a
// storage view would not have caught). Bounds: span::operator[] is unchecked in
// C++20 — the test builds carry -D_GLIBCXX_ASSERTIONS.
//
// NOTE the ring is the STORAGE size, NOT kMaskRing: the engine's scattered
// `& (kMaskRing - 1)` sites are 16-folds on the 32-wide storage (domain
// reductions, not ring wraps) and are deliberately NOT routed here — a view
// would mask with 31 where they mask with 15, a behaviour change.
template<typename T>
struct RingView {
    std::span<T> data;   // the ring storage (size() is the power-of-two ring)
    int off = 0;         // the window's offset O
    int len = 0;         // the window's length L

    size_t size() const { return (size_t)len; }
    bool   empty() const { return len <= 0; }
    // The wrap law. Negative i is safe the same way the raw mask is: (O+i) & (ring-1)
    // on two's-complement integers equals the double-mod for ANY i.
    T& operator[](size_t i) const {
        return data[(size_t)((off + (int)i) & ((int)data.size() - 1))];
    }
};

}  // namespace redDot
