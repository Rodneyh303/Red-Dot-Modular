// CycleModeCore — Rack-free law for UAS "Cycle: Phrase" (default) vs "Cycle: Custom"
// (CC_PROMPT_UAS_CYCLE_PHRASE_MODE.md, Rodney's rulings 2026-11-22).
//
// THE PROBLEM IT FIXES: the Macro voice range seeds L = kPatternLengthCap (32), so a
// 16-step pattern sat over a 32-step voice window and its 2nd lap used different random
// numbers — the pattern stopped repeating every lap. A surprising DEFAULT, and a change
// from pre-UAS behaviour.
//
// THE LAW (pure, so it is proven here once and the engine just CALLS it):
//   Cycle::Phrase (DEFAULT) — the voice cycle is LOCKED to the global phrase. The user can
//     set NONE of L/O/R: L = the transport pattern length (live), O = 0, R = the transport
//     OFFSET 1:1. Rodney's rationale: a lane's R sets where the pattern wraps when the voice
//     is at its first step, so O=0 / L=transport / R=transport-offset reproduces pre-UAS
//     lane behaviour BY CONSTRUCTION (every lane was always at the global offset at global
//     step 0) — no special-case path.
//   Cycle::Custom — today's behaviour: own L/O/R authorable (polymeter, longer-than-pattern
//     draw windows). Opt-in.
//
// NOTE FOR THE ENGINE WIRING (CC): this header encodes the INTENDED law from the rulings.
// What the engine reads TODAY at a given offset (prompt "facts to verify" #2) is still CC's
// to establish; if the current code disagrees with this oracle, REPORT the divergence —
// do not silently bend either side.
#pragma once
#include <algorithm>

namespace redDot { namespace cycle {

enum class CycleMode { Phrase = 0, Custom = 1 };

// A resolved (or authored) voice-lane cycle window.
struct Cycle {
    int L = 1;   // window length in steps
    int O = 0;   // window start offset
    int R = 0;   // rotation (where the pattern wraps when the voice is at its first step)
};

// Engine bounds (mirror of PatternEngine's kMaxPatternSteps / usable minimum). Kept local so
// this header stays dependency-free; a static_assert at the call site can pin them together.
constexpr int kMinLen = 1;
constexpr int kMaxLen = 32;

inline int clampLen(int l) { return std::min(std::max(l, kMinLen), kMaxLen); }
// A rotation is only meaningful modulo the window length; normalise to [0, L).
inline int normRot(int r, int l) {
    const int L = clampLen(l);
    int m = r % L;
    if (m < 0) m += L;      // negative-safe (a negative transport offset still normalises)
    return m;
}

// ── THE RESOLVE ──────────────────────────────────────────────────────────────
// The effective window for a row, given its mode, the live transport, and (Custom only)
// its authored values. Phrase IGNORES `authored` entirely — that is the point.
inline Cycle resolveCycle(CycleMode mode, int transportLen, int transportOff, const Cycle& authored) {
    Cycle out;
    if (mode == CycleMode::Phrase) {
        out.L = clampLen(transportLen);          // follows the transport, live
        out.O = 0;                                // locked
        out.R = normRot(transportOff, out.L);     // the transport offset, 1:1
    } else {
        out.L = clampLen(authored.L);
        out.O = authored.O;
        out.R = normRot(authored.R, out.L);
    }
    return out;
}

// ── THE IDENTITY PREDICATE ───────────────────────────────────────────────────
// Replaces the old `L==cap && O==0 && R==0 && Forward` sentinel (which is what made the
// cap a magic value). New law: identity == Phrase mode with no effective rotation, forward.
// Custom keeps the old-style test on its OWN authored values (a Custom row that happens to
// match the phrase is NOT the pass-through identity — the mode is the thing).
inline bool isIdentityCycle(CycleMode mode, const Cycle& resolved, bool forward) {
    return mode == CycleMode::Phrase && resolved.R == 0 && forward;
}

// ── THE READ LAW (the rotate-WITHIN-the-phrase oracle) ───────────────────────
// Rodney's rationale, stated as arithmetic: "the voice at its first step reads the cell at
// the global offset; it wraps at the global length." So the cell read at step s is
// (s + R) mod L — the offset SLIPS THE PATTERN AROUND THE SAME PROBABILITIES (a rotation of
// the same L cells), it does NOT slide the window over a larger ring.
inline int readIndex(int step, int L, int R) {
    const int len = clampLen(L);
    int s = step % len;
    if (s < 0) s += len;
    return (s + normRot(R, len)) % len;
}

// ── MODE SWITCHING ───────────────────────────────────────────────────────────
// Phrase -> Custom must NOT jump: the row adopts what Phrase was producing as its authored
// values, so the sound is continuous across the switch.
// Custom -> Phrase DISCARDS the authored values (Phrase ignores them by definition). They are
// passed through unchanged in storage, but a later Phrase->Custom overwrites them with the
// phrase-effective — i.e. a Custom -> Phrase -> Custom round trip does NOT restore the
// original Custom window. That is the prompt's "discards"; if Rodney later wants
// preserve-and-restore instead, this is the ONE function to change.
inline Cycle authoredOnModeSwitch(CycleMode from, CycleMode to, const Cycle& resolved, const Cycle& authored) {
    if (from == CycleMode::Phrase && to == CycleMode::Custom) return resolved;  // adopt: no jump
    return authored;                                                            // Custom->Phrase: unused
}

}} // namespace redDot::cycle
