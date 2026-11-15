// STEPS_32 STAGE 1 (CC_PROMPT_32_STEPS_STAGE1_STRUCTURAL deliverable 4) — the length-domain pins.
//
// Stage 1 named the pattern-length ceiling (dotModular::kMaxPatternSteps = 32) and the USABLE
// storage-bound cap (dotModular::kPatternLengthCap — THE ONE STAGE-2 SWITCH), and derives
// every length-bearing control from them. These pins hold the stage to its contract:
//   1. THE WRAP SWEEP — for every L in 1..cap the master window walks exactly its L cells and
//      wraps at L; windowMask carries exactly those L bits; the lane walks wrap at THEIR OWN
//      length (the holders' own-length regime, unchanged).
//   2. THE CLAMP REPORT — a requested length above the bound is CLAMPED to the cap and the
//      engine's effective length reports it. Stage-1 close-out (Rodney's PR #310 review): the
//      length KNOB's range derives from kPatternLengthCap, so the panel never offers an
//      undeliverable length; the clamp remains the defence-in-depth for the CV path.
//   3. THE CV MAP AT THE BOUND — the length CV mapping round(cv/10*(cap-1))+1 is pinned so the
//      rescale is a conscious re-pin, never an accident. STAGE 2B RE-PIN: the cap flipped to 32
//      (the D1 *31 rescale landed — 10V now maps to 32, 5V to round(15.5)+1 = 17).
//   4. THE LANE CLAMP — setStrand clamps the lane length to the cap: the single sanctioned
//      LOR-length authoring path stays inside the storage bound.
//
// STAGE 2B (this re-pin): kPatternLengthCap flipped 16 → 32 per the plan's Stage 2 split
// ("(2b) flip the cap to 32 (behaviour change, its own pins)"). Every bound below moves WITH the
// constant — the file pins the cap's VALUE in section 0 so the next move is conscious too.
//
// Build (run_all.sh): test_pattern_length_cap|$SE $GS $PE $CE
#include <cstdio>
#include <cstdint>
#include <vector>
#include <set>
#include <algorithm>

#include "SequencerEngine.hpp"

using E = SequencerEngine;
namespace dm = dotModular;

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

int main() {
    const int CAP = dm::kPatternLengthCap;

    // ── 0. the length-domain constants themselves (every move is a conscious change) ──
    check(dm::kMaxPatternSteps == 32,  "kMaxPatternSteps == 32 (the ceiling)");
    check(dm::kPatternLengthCap == 32, "kPatternLengthCap == 32 (STAGE 2B: flipped to the ceiling — was 16 at Stage 1)");
    check(dm::kStepStorage == 32,      "kStepStorage == 32 (Stage 2a widened the rows; storage == cap now)");
    static_assert(dm::kPatternLengthCap <= dm::kMaxPatternSteps, "cap never exceeds the ceiling");

    // ── 1. the master-window wrap sweep: L = 1..cap, offset 0 ───────────────────────
    // setWindow(L,0) → cachedLength == L; the walk visits exactly [0, L) and wraps at L;
    // windowMask bit s is set iff s < L. 2L+3 advances cross at least one full wrap.
    for (int L = 1; L <= CAP; ++L) {
        E e; e.reset();
        e.setWindow(L, 0);
        check(e.cachedLength == L, "setWindow(L,0) reports cachedLength == L");
        std::set<int> seen;
        bool alwaysInWindow = true;
        for (int i = 0; i < 2 * L + 3; ++i) {
            e.advancePlayhead();
            seen.insert(e.stepIndex);
            alwaysInWindow = alwaysInWindow && e.isStepInWindow(e.stepIndex);
        }
        bool exactDomain = true;
        for (int s = 0; s < L; ++s)   exactDomain = exactDomain && seen.count(s) > 0;
        for (int s = L; s < CAP; ++s) exactDomain = exactDomain && seen.count(s) == 0;
        check(alwaysInWindow, "walk stays inside the window");
        check(exactDomain,    "visited set == [0, L) exactly (the wrap-at-L domain)");
        bool maskOk = true;
        for (int s = 0; s < CAP; ++s) maskOk = maskOk && (((e.windowMask >> s) & 1u) == (s < L ? 1u : 0u));
        check(maskOk, "windowMask bits == the L-window (offset 0)");
    }

    // ── 2. the window wrap PERIOD at an offset: the walk repeats with period exactly L ──
    // (contiguous window, offset+len ≤ ring — the GOLDEN LAW domain; the visited set over
    // the first L advances equals the set over 2L+3, and the mask popcount is L. The pairs
    // keep their Stage-1 cases (still valid at ring 32) and gain the NEW bound cases.)
    const int pairs[][2] = { {1,0}, {5,3}, {7,9}, {16,0}, {16,15}, {3,14}, {32,0}, {32,31}, {3,30} };
    for (const auto& p : pairs) {
        const int L = p[0], O = p[1];
        E e; e.reset();
        e.setWindow(L, O);
        int pop = 0;
        for (int s = 0; s < CAP; ++s) pop += (int)((e.windowMask >> s) & 1u);
        check(pop == L, "window popcount == L at an offset");
        std::set<int> firstL, all;
        for (int i = 0; i < L; ++i)     { e.advancePlayhead(); firstL.insert(e.stepIndex); }
        for (int i = L; i < 2 * L + 3; ++i) { e.advancePlayhead(); all.insert(e.stepIndex); }
        check(firstL == all, "the walk's visited set repeats with period exactly L (wraps at L, not the ring)");
        bool inside = true;
        for (int s : all) inside = inside && ((e.windowMask >> s) & 1u);
        check(inside, "every visited cell is a window cell");
    }

    // ── 3. THE CLAMP REPORT — over-range requests deliver the bound ─────────────────
    {
        E e; e.reset();
        e.updateWindow(40.f, 0.f, false, 0.f, 0.f, false);
        check(e.cachedLength == 32, "requested 40 → effective length 32 (the cap reports)");
        e.updateWindow(33.f, 0.f, false, 0.f, 0.f, false);
        check(e.cachedLength == 32, "requested 33 → effective length 32 (one past the bound clamps)");
        e.updateWindow(32.f, 0.f, false, 0.f, 0.f, false);
        check(e.cachedLength == 32, "requested 32 → effective length 32 (the bound itself — STAGE 2B: was clamped to 16)");
        e.updateWindow(20.f, 0.f, false, 0.f, 0.f, false);
        check(e.cachedLength == 20, "requested 20 → 20 (STAGE 2B: in range now)");
        e.updateWindow(17.f, 0.f, false, 0.f, 0.f, false);
        check(e.cachedLength == 17, "requested 17 → 17 (STAGE 2B: in range now)");
        for (int L = 1; L <= CAP; ++L) {
            e.updateWindow((float)L, 0.f, false, 0.f, 0.f, false);
            if (e.cachedLength != L) { check(false, "in-range request passes through unchanged"); break; }
        }
        check(true, "in-range sweep completed (1..32 pass through exactly)");
    }

    // ── 4. THE CV MAP AT THE BOUND — the D1 *31 rescale landed with the flip ────────
    {
        E e; e.reset();
        e.updateWindow(32.f,  0.f, true, 0.f, 0.f, false);
        check(e.cachedLength == 1,  "length CV 0V → 1");
        e.updateWindow(32.f, 10.f, true, 0.f, 0.f, false);
        check(e.cachedLength == 32, "length CV 10V → 32 (D1's *31: round(10/10*31)+1 — the Stage-2 rescale, consciously re-pinned)");
        e.updateWindow(32.f,  5.f, true, 0.f, 0.f, false);
        check(e.cachedLength == 17, "length CV 5V → round(5/10*31)+1 = 17 (the cap-derived mapping at the 32 bound)");
    }

    // ── 5. THE LANE CLAMP — setStrand stays inside the storage bound ────────────────
    {
        E e; e.reset();
        e.beginStrandWriteBlock();
        e.setStrand(StrandWriter::MONO, dm::STRAND_RHYTHM, 40, 0, 0);
        check(e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) == 32, "lane length 40 requested → clamped to the cap 32 (the report)");
        e.setStrand(StrandWriter::MONO, dm::STRAND_RHYTHM, 33, 0, 0);
        check(e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) == 32, "lane length 33 → clamped to 32");
        e.setStrand(StrandWriter::MONO, dm::STRAND_RHYTHM, 32, 0, 0);
        check(e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) == 32, "lane length 32 → 32 (the bound — STAGE 2B: was clamped to 16)");
        e.setStrand(StrandWriter::MONO, dm::STRAND_RHYTHM, 17, 0, 0);
        check(e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) == 17, "lane length 17 → 17 (STAGE 2B: in range now)");
        e.setStrand(StrandWriter::MONO, dm::STRAND_RHYTHM, 7, 0, 0);
        check(e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) == 7,  "lane length 7 → 7 (in-range passes)");
        e.setStrand(StrandWriter::MONO, dm::STRAND_RHYTHM, 1, 0, 0);
        check(e.lorRef(dm::STRAND_RHYTHM, E::LOR_LEN) == 1,  "lane length 1 → 1");
    }

    // ── 6. the LANE wrap sweep: the walk wraps at its OWN length (1..cap) ───────────
    // The holders wrap at the lane's own length (POSITION_HOLDER_MODEL) — at the pass-through
    // default window the mono tick is the walk's bounded phase in [0, len).
    for (int L = 1; L <= CAP; ++L) {
        E e; e.reset();
        e.beginStrandWriteBlock();
        e.setStrand(StrandWriter::MONO, dm::STRAND_RHYTHM, L, 0, 0);
        bool inDomain = true;
        for (int i = 0; i < 2 * L + 3; ++i) {
            e.advancePlayhead();
            const int t = e.laneTick_[dm::STRAND_RHYTHM];
            inDomain = inDomain && (t >= 0 && t < L);
        }
        char msg[96];
        std::snprintf(msg, sizeof(msg), "lane walk (len %d) stays in [0, %d) across a wrap", L, L);
        check(inDomain, msg);
    }

    std::printf("test_pattern_length_cap: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
