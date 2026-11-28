// test_fold_resize_adjacency.cpp — proves the fold RESIZE keeps the row flush
// (no gaps, no overlaps) for BOTH grow and shrink, across a multi-module chain —
// the bug Rodney feel-tested: growing SandsFold broke the expander chain (Monsoon
// unreachable, Sands/IT/Lantern went dark) and shrinking left a physical gap.
//
// We cannot run Rack here, so we model the ONE contract the widget relies on:
//   setModulePosForce(mod, target): if `target` overlaps no other module, `mod`
//   is placed EXACTLY at target and nothing else moves. (When the target DOES
//   overlap, real Rack shoves the overlapped module — an outcome the plan must
//   AVOID, because a shove desyncs the rigid translation. So the test asserts the
//   stronger invariant: every push lands on empty space.)
//
// The plan under test is redDot::fold::resizeNeighbourOrder + resizeSelfFirst.
// The OLD spike (always getModuleIds()-order, push-then-resize) is reproduced as
// `legacyPlan` and SHOWN to violate the invariant — the red/green proof.
//
// Rack-free. Compile: see test/run_all.sh ("test_fold_resize_adjacency").

#include "FoldPanelWidthCore.hpp"
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>

namespace fw = redDot::fold;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

struct Mod { float x; float w; };   // a row module: left edge x, width w (px)

// Faithful model of the widget's resize against the setModulePosForce contract.
// Returns false if ANY push targeted occupied space (the thing that desyncs the
// rigid translation in real Rack — a cascade shove). `legacy` = the old spike's
// behaviour (push every right-neighbour in ascending-id order, THEN resize self,
// both directions) so the test can show it failing.
static bool runResize(std::vector<Mod>& mods, int self, int newWidthHp,
                      bool legacy, float& minGapOut, float& maxOverlapOut) {
    const float oldW = mods[self].w;
    const float newW = (float)newWidthHp * fw::kGridPx;
    const float shift = newW - oldW;
    const float selfRight = mods[self].x + oldW;

    // collect right-side neighbours (same row is implicit here — one row)
    std::vector<int> nb;
    for (int i = 0; i < (int)mods.size(); ++i)
        if (i != self && mods[i].x >= selfRight - 1.f) nb.push_back(i);

    // processing order + self-resize timing
    std::vector<int> order;
    bool selfFirst;
    if (legacy) {
        order = nb;                       // getModuleIds() order ~ ascending index
        selfFirst = false;                // always push-then-resize
    } else {
        std::vector<float> xs;
        for (int i : nb) xs.push_back(mods[i].x);
        std::vector<int> ord = fw::resizeNeighbourOrder(xs, shift);
        for (int k : ord) order.push_back(nb[k]);
        selfFirst = fw::resizeSelfFirst(shift);
    }

    bool allClean = true;
    auto overlapsAny = [&](int who, float tx) {
        const float a0 = tx, a1 = tx + mods[who].w;
        for (int j = 0; j < (int)mods.size(); ++j) {
            if (j == who) continue;
            const float b0 = mods[j].x, b1 = mods[j].x + mods[j].w;
            if (a1 - 1.f > b0 && b1 - 1.f > a0) return true;   // >1px interpenetration
        }
        return false;
    };

    if (selfFirst) mods[self].w = newW;
    for (int i : order) {
        const float target = mods[i].x + shift;
        if (overlapsAny(i, target)) allClean = false;         // real Rack would SHOVE here
        mods[i].x = target;                                   // contract: clean place
    }
    if (!selfFirst) mods[self].w = newW;

    // measure the final row: sort by x, walk the seam between consecutive modules
    std::vector<int> byX(mods.size());
    for (int i = 0; i < (int)mods.size(); ++i) byX[i] = i;
    std::sort(byX.begin(), byX.end(), [&](int a, int b){ return mods[a].x < mods[b].x; });
    minGapOut = 0.f; maxOverlapOut = 0.f;
    for (int k = 1; k < (int)byX.size(); ++k) {
        const float prevRight = mods[byX[k-1]].x + mods[byX[k-1]].w;
        const float seam = mods[byX[k]].x - prevRight;        // +gap / -overlap
        if (seam >  minGapOut)    minGapOut = seam;
        if (-seam > maxOverlapOut) maxOverlapOut = -seam;
    }
    return allClean;
}

// a flush row of n modules each `hp` wide, starting at x0
static std::vector<Mod> flushRow(int n, int hp, float x0 = 0.f) {
    std::vector<Mod> r; float x = x0;
    for (int i = 0; i < n; ++i) { r.push_back({x, hp * fw::kGridPx}); x += hp * fw::kGridPx; }
    return r;
}

int main() {
    std::printf("== test_fold_resize_adjacency (the chain-break / shrink-gap proof) ==\n");

    // ── G1 GROW keeps the row flush (the plan) ──────────────────────────────────
    // [fold=20][A=8][B=8][Mon=30] ; fold grows 20 -> 40. Chain must stay flush.
    {
        auto mods = flushRow(4, 8);  mods[0] = {0.f, 20.f * fw::kGridPx};
        // rebuild flush with fold=20 at slot 0
        mods = { {0.f, 20.f*fw::kGridPx}, {0.f,0}, {0.f,0}, {0.f,0} };
        float x = 20.f*fw::kGridPx;
        mods[1] = {x, 8*fw::kGridPx}; x += 8*fw::kGridPx;
        mods[2] = {x, 8*fw::kGridPx}; x += 8*fw::kGridPx;
        mods[3] = {x, 30*fw::kGridPx};
        float gap, ov;
        bool clean = runResize(mods, 0, 40, /*legacy=*/false, gap, ov);
        CHECK(clean, "G1 grow: every push lands on empty space (no cascade shove)");
        CHECK(gap < 1.f && ov < 1.f, "G1 grow: row stays flush (no gap, no overlap)");
    }

    // ── G2 SHRINK keeps the row flush (the plan) ────────────────────────────────
    {
        std::vector<Mod> mods = { {0.f, 40.f*fw::kGridPx}, {0,0}, {0,0}, {0,0} };
        float x = 40.f*fw::kGridPx;
        mods[1] = {x, 8*fw::kGridPx};  x += 8*fw::kGridPx;
        mods[2] = {x, 8*fw::kGridPx};  x += 8*fw::kGridPx;
        mods[3] = {x, 30*fw::kGridPx};
        float gap, ov;
        bool clean = runResize(mods, 0, 20, /*legacy=*/false, gap, ov);
        CHECK(clean, "G2 shrink: every pull lands on empty space (self resized first)");
        CHECK(gap < 1.f && ov < 1.f, "G2 shrink: row stays flush (NO physical gap — the bug)");
        // specifically: Monsoon (slot 3) must end adjacent to the chain, 20HP left of before
        CHECK(std::fabs(mods[3].x - (20.f*fw::kGridPx + 16.f*fw::kGridPx)) < 1.f,
              "G2 shrink: Monsoon pulled flush to the new right edge (stays adjacent)");
    }

    // ── G3 the LEGACY plan is the bug: RED where the new plan is GREEN ───────────
    {
        // grow: legacy re-shifts already-shoved modules -> targets hit occupied space
        std::vector<Mod> g = { {0.f, 20.f*fw::kGridPx}, {0,0}, {0,0}, {0,0} };
        float x = 20.f*fw::kGridPx;
        g[1] = {x, 8*fw::kGridPx}; x += 8*fw::kGridPx;
        g[2] = {x, 8*fw::kGridPx}; x += 8*fw::kGridPx;
        g[3] = {x, 30*fw::kGridPx};
        float gap, ov;
        bool cleanLegacyGrow = runResize(g, 0, 40, /*legacy=*/true, gap, ov);
        CHECK(!cleanLegacyGrow, "G3 legacy GROW cascades (a push hits occupied space) — the chain break");

        // shrink: legacy pushes BEFORE resizing self -> the nearest pull targets a spot
        // still inside our wide box (occupied). Real Rack refuses/blocks it there -> the
        // module stops short -> the physical gap. The model flags the occupied target.
        std::vector<Mod> s = { {0.f, 40.f*fw::kGridPx}, {0,0}, {0,0}, {0,0} };
        x = 40.f*fw::kGridPx;
        s[1] = {x, 8*fw::kGridPx}; x += 8*fw::kGridPx;
        s[2] = {x, 8*fw::kGridPx}; x += 8*fw::kGridPx;
        s[3] = {x, 30*fw::kGridPx};
        bool cleanLegacyShrink = runResize(s, 0, 20, /*legacy=*/true, gap, ov);
        CHECK(!cleanLegacyShrink,
              "G3 legacy SHRINK pulls into our still-wide box (occupied) — Rack blocks it -> the gap");
    }

    // ── G4 order + timing primitives ────────────────────────────────────────────
    {
        CHECK(fw::resizeSelfFirst(-30.f) && !fw::resizeSelfFirst(+30.f),
              "G4 shrink resizes self first; grow does not");
        CHECK(fw::resizeOrderBefore(100.f, 50.f, +30.f), "G4 grow: farther (100) before nearer (50)");
        CHECK(fw::resizeOrderBefore(50.f, 100.f, -30.f), "G4 shrink: nearer (50) before farther (100)");
        std::vector<float> xs = {10.f, 40.f, 25.f};
        auto grow = fw::resizeNeighbourOrder(xs, +30.f);   // descending x
        CHECK(xs[grow[0]] == 40.f && xs[grow[2]] == 10.f, "G4 grow order is descending x");
        auto shr = fw::resizeNeighbourOrder(xs, -30.f);    // ascending x
        CHECK(xs[shr[0]] == 10.f && xs[shr[2]] == 40.f, "G4 shrink order is ascending x");
    }

    if (g_fail == 0) std::printf("ALL PASS\n");
    else             std::printf("%d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
