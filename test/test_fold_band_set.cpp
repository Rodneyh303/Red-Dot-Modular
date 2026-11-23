// test_fold_band_set.cpp — SEAMLESS_FOLD_PLAN.md (the 2026-11-22 rulings): width is
// DERIVED from the popped-out bands, subpanels re-orderable, jack panels edge-pinned,
// block/warn on removing a cabled jack panel. Pure core (FoldBandSetCore.hpp).
//
// INDEPENDENT ORACLES ONLY: each property is cross-checked against a from-scratch
// recomputation of the SPEC, never the function under test. Compile: see run_all.sh.
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "ui/FoldBandSetCore.hpp"

using namespace redDot::fold;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

// ── Independent oracle: module width in HP from a band set ──
// Recompute the SPEC: base + sum of positive finite extents, px->HP ceil, then the
// allowed-width law (floor kMinHp, even step, cap kMaxHp) — written WITHOUT calling
// the core's clampHp/widthHpForBands.
static int oracleWidthHp(float basePx, const std::vector<FoldBand>& bands) {
    double total = (std::isfinite(basePx) && basePx > 0.f) ? basePx : 0.0;
    for (const FoldBand& b : bands)
        if (std::isfinite(b.extentPx) && b.extentPx > 0.f) total += b.extentPx;
    int hp = (int)std::ceil(total / 15.0);        // kGridPx literal
    if (hp < 12) hp = 12;                          // kMinHp
    if (hp > 120) return 120;                       // kMaxHp (always allowed, grid ignored)
    if (((hp - 12) % 2) != 0) hp += 1;             // snap up to the 2-HP grid from the floor
    if (hp > 120) hp = 120;
    return hp;
}

// Independent oracle: left->right order = Left group, Interior group, Right group,
// each sorted by (rank, id). A stable partition written from scratch.
static std::vector<int> oracleOrder(const std::vector<FoldBand>& bands) {
    auto grp = [&](BandEdge e) {
        std::vector<FoldBand> g;
        for (const FoldBand& b : bands) if (b.edge == e) g.push_back(b);
        std::sort(g.begin(), g.end(), [](const FoldBand& a, const FoldBand& b) {
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.id < b.id;
        });
        return g;
    };
    std::vector<int> out;
    for (BandEdge e : {BandEdge::Left, BandEdge::Interior, BandEdge::Right})
        for (const FoldBand& b : grp(e)) out.push_back(b.id);
    return out;
}

static FoldBand mk(int id, float ext, BandEdge e, int rank, bool jack) {
    FoldBand b; b.id = id; b.extentPx = ext; b.edge = e; b.rank = rank; b.isJackPanel = jack; return b;
}

int main() {
    std::printf("test_fold_band_set\n");

    // 1. WIDTH follows the band set, matches the independent oracle, across sets.
    const float base = 120.f;   // base panel px (8 HP) — arbitrary, swept against the oracle
    std::vector<std::vector<FoldBand>> sets = {
        {},
        { mk(1, 90.f, BandEdge::Interior, 0, false) },
        { mk(1, 90.f, BandEdge::Interior, 0, false), mk(2, 150.f, BandEdge::Interior, 1, false) },
        { mk(1, 45.f, BandEdge::Left, 0, true), mk(2, 300.f, BandEdge::Interior, 0, false) },
    };
    for (size_t i = 0; i < sets.size(); ++i) {
        int got = widthHpForBands(base, sets[i]);
        int want = oracleWidthHp(base, sets[i]);
        CHECK(got == want, "width matches oracle");
        CHECK(got >= 12 && got <= 120 && (got == 120 || (got - 12) % 2 == 0), "width is an allowed HP");
    }

    // 2. WIDTH is ORDER-INDEPENDENT (a sum): permuting the band list gives the same HP.
    {
        std::vector<FoldBand> a = {
            mk(3, 200.f, BandEdge::Right, 0, true),
            mk(1, 90.f,  BandEdge::Left, 0, true),
            mk(2, 150.f, BandEdge::Interior, 0, false),
        };
        std::vector<FoldBand> b(a.rbegin(), a.rend());
        CHECK(widthHpForBands(base, a) == widthHpForBands(base, b), "width is order-independent");
    }

    // 3. Negative / non-finite extents never shrink the module (treated as 0).
    {
        std::vector<FoldBand> good = { mk(1, 90.f, BandEdge::Interior, 0, false) };
        std::vector<FoldBand> junk = good;
        junk.push_back(mk(2, -500.f, BandEdge::Interior, 1, false));
        junk.push_back(mk(3, std::nanf(""), BandEdge::Interior, 2, false));
        junk.push_back(mk(4, INFINITY, BandEdge::Interior, 3, false));
        CHECK(widthHpForBands(base, good) == widthHpForBands(base, junk), "junk extents are no-ops");
    }

    // 4. ORDER: jack (edge) panels pinned outside, interior re-orderable; matches oracle.
    {
        std::vector<FoldBand> bands = {
            mk(10, 50.f, BandEdge::Interior, 2, false),
            mk(11, 50.f, BandEdge::Left,     0, true),
            mk(12, 50.f, BandEdge::Interior, 0, false),
            mk(13, 50.f, BandEdge::Right,    0, true),
            mk(14, 50.f, BandEdge::Interior, 1, false),
            mk(15, 50.f, BandEdge::Left,     1, true),
        };
        std::vector<int> got = layoutOrder(bands);
        std::vector<int> want = oracleOrder(bands);
        CHECK(got == want, "order matches oracle");
        // Explicit property: the first and last ids are jack panels; no jack id sits
        // between two interior ids.
        auto isJack = [&](int id){ for (auto& b : bands) if (b.id==id) return b.isJackPanel; return false; };
        CHECK(isJack(got.front()) && isJack(got.back()), "edges are jack panels");
        bool seenInterior = false, jackAfterInterior = false, interiorAfterRight = false;
        // walk: once we pass the left jacks, interior; a jack after interior must be a RIGHT jack (tail)
        size_t k = 0; while (k < got.size() && isJack(got[k])) ++k;      // skip left jacks
        for (; k < got.size(); ++k) {
            if (!isJack(got[k])) { seenInterior = true; if (interiorAfterRight) jackAfterInterior = true; }
            else { /* right-tail jack */ interiorAfterRight = true; }
        }
        (void)seenInterior;
        CHECK(!jackAfterInterior, "no interior band after a right-edge jack");
    }

    // 5. CAP: content wider than kMaxHp caps at 120 and bandWouldExceedCap flags the pop-out.
    {
        std::vector<FoldBand> huge = { mk(1, 120.f*15.f, BandEdge::Interior, 0, false) }; // 120 HP of content
        CHECK(widthHpForBands(0.f, huge) == 120, "content capped at kMaxHp");
        CHECK(bandWouldExceedCap(0.f, huge, 15.f) == true, "one more HP over the cap is refused");
        std::vector<FoldBand> small = { mk(1, 90.f, BandEdge::Interior, 0, false) };
        CHECK(bandWouldExceedCap(120.f, small, 150.f) == false, "a fitting pop-out is allowed");
    }

    // 6. REMOVAL GUARD: block/warn only for a cabled jack panel.
    CHECK(removalNeedsGuard(true,  true)  == true,  "cabled jack panel is guarded");
    CHECK(removalNeedsGuard(true,  false) == false, "uncabled jack panel is free");
    CHECK(removalNeedsGuard(false, true)  == false, "a cabled knob band is not a jack-panel guard");
    CHECK(removalNeedsGuard(false, false) == false, "uncabled knob band is free");

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
