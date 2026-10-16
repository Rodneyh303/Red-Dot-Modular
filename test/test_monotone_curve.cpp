// test_monotone_curve.cpp — dotModular::MonotoneCurve — the .dmcurve Phase 1 math module
// Compile: see test/run_all.sh (header-only, no companion sources).
//
// The .dmcurve editor's de-risking core (docs/design/DMCURVE_EDITOR.md §PHASE 1): a monotone
// CDF edited as control points, evaluated with Fritsch–Carlson PCHIP (monotone cubic — no
// overshoot), inverted by diagonal reflection, serialized as the v1 .dmcurve JSON schema.
//
// Guarantees locked down here (one section each):
//   1. Identity default: endpoints pinned, F(x)=x, F'(x)=1, F⁻¹(u)=u, domain clamping.
//   2. PCHIP interpolates the control points EXACTLY (F at (x_i,y_i); F⁻¹ at the reflected
//      knots) — the curve passes through what the user drew.
//   3. Monotonicity under a fine sweep for the whole curve family AND adversarial/random
//      curves — the Fritsch–Carlson no-overshoot guarantee.
//   4. setPoint clamping: adversarial drags clamp into monotone validity; endpoints are
//      immovable; OOB/NaN are no-ops; invariants survive every op.
//   5. addPoint lands ON the curve (y == pre-insert evalCDF(x)); rejects the 32 cap,
//      duplicate x, and out-of-domain x; keeps sort order.
//   6. removePoint never removes endpoints; floor at MIN_POINTS; anchors preserved.
//   7.  evalInverse(evalCDF(x)) ≈ x round-trip in double (~1e-10, both directions) — the
//      Newton-refined inverse is exact up to root-finding tolerance.
//   8. Flat segments (zero-density gaps): F⁻¹ maps a gap's quantile to its LEFT edge (exact
//      knot) and neighbouring quantiles into the post-gap rising region (not into the gap) —
//      the reflection carries both gap edges.
//   9. pdf == numerical derivative of evalCDF (knot-avoiding sweep) and non-negative.
//  10. Reflected-curve cache: invalidated by setPoint / addPoint / removePoint (stale-cache
//      regression).
//  11. toJson → fromJson round-trip: identical points (exact) + metadata, both as json_t*
//      and through json_dumps/json_loads TEXT (the emitted schema is genuinely parseable
//      JSON); second-generation output byte-identical.
//  12. fromJson validation: bad format/version/shape rejected with state UNTOUCHED;
//      non-monotone / unsorted / out-of-range points SANITIZED (clamp → sort → dedupe →
//      pin endpoints → running-max y) — the defined, tested behaviour.
//
// jansson: the standalone harness has no libjansson (why test_TuningRoundTrip can't cover
// TuningPreset). MonotoneCurve is PURE — it includes <jansson.h> directly, which -Itest
// resolves to the test/jansson.h shim (the test/rack.hpp precedent) — so the REAL .dmcurve
// schema logic runs here while the plugin tree keeps Rack's bundled jansson.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "MonotoneCurve.hpp"
#include "Distribution.hpp"

using dotModular::MonotoneCurve;

static int g_pass = 0, g_fail = 0;

static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static void checkNear(double got, double want, double tol, const char* what) {
    if (std::fabs(got - want) <= tol) { ++g_pass; return; }
    std::printf("  FAIL: %s (got %.9g, want %.9g, tol %g)\n", what, got, want, tol);
    ++g_fail;
}

// ── helpers ────────────────────────────────────────────────────────────────────────────────────
// The full Phase 1 invariant set, re-derived INDEPENDENTLY of the module (the test is the
// spec here, not a call into module code).
static bool invariantsHold(const MonotoneCurve& c) {
    const std::vector<MonotoneCurve::Point>& p = c.getPoints();
    if (c.size() < MonotoneCurve::MIN_POINTS || c.size() > MonotoneCurve::MAX_POINTS) return false;
    if (p.front().x != 0.f || p.front().y != 0.f) return false;   // pinned (0,0)
    if (p.back().x  != 1.f || p.back().y  != 1.f) return false;   // pinned (1,1)
    for (size_t i = 0; i + 1 < p.size(); ++i) {
        if (!(p[i + 1].x > p[i].x)) return false;                 // x strictly increasing
        if (!(p[i + 1].y >= p[i].y)) return false;                // y monotone non-decreasing
        if (p[i].x < 0.f || p[i].x > 1.f || p[i].y < 0.f || p[i].y > 1.f) return false;
    }
    return true;
}

// evalCDF monotone over a fine sweep (the PCHIP no-overshoot guarantee). 1e-12 slack covers
// double rounding in the cubic evaluation — a real overshoot is orders of magnitude bigger.
static bool monotoneSweep(const MonotoneCurve& c, int steps = 1000) {
    double prev = c.evalCDF(0.0);
    for (int k = 1; k <= steps; ++k) {
        const double v = c.evalCDF((double)k / (double)steps);
        if (v < prev - 1e-12) return false;
        prev = v;
    }
    return true;
}
static bool inverseMonotoneSweep(const MonotoneCurve& c, int steps = 1000) {
    double prev = c.evalInverse(0.0);
    for (int k = 1; k <= steps; ++k) {
        const double v = c.evalInverse((double)k / (double)steps);
        if (v < prev - 1e-12) return false;
        prev = v;
    }
    return true;
}
static bool samePoints(const MonotoneCurve& a, const MonotoneCurve& b) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y) return false;
    return true;
}

// Deterministic LCG for adversarial curves (reproducible; no RNG-quality claim).
static uint64_t lcgState = 0x123456789ABCDEF0ull;
static float lcgUniform() {   // [0,1)
    lcgState = lcgState * 6364136223846793005ull + 1442695040888963407ull;
    return (float)((lcgState >> 40) & 0xFFFFFF) / 16777216.f;
}

// Build a curve from an explicit VALID point list through the public JSON path. Arbitrary
// point placement is intentionally NOT reachable via the clamped drag API (the editor can
// only ever reach valid states); a .dmcurve file is the other entry point, and fromJson is
// exactly that entry. The helper asserts the load was exact.
static MonotoneCurve curveFromPoints(const std::vector<MonotoneCurve::Point>& pts) {
    json_t* root = json_object();
    json_object_set_new(root, "format", json_string("dmcurve"));
    json_object_set_new(root, "version", json_integer(1));
    json_t* jp = json_array();
    for (const auto& p : pts) {
        json_t* pair = json_array();
        json_array_append_new(pair, json_real((double)p.x));
        json_array_append_new(pair, json_real((double)p.y));
        json_array_append_new(jp, pair);
    }
    json_object_set_new(root, "points", jp);
    MonotoneCurve c;
    check(c.fromJson(root), "curveFromPoints: fromJson accepted a valid curve");
    check((int)pts.size() >= MonotoneCurve::MIN_POINTS, "curveFromPoints: input has enough points");
    check(c.size() == (int)pts.size(), "curveFromPoints: point count preserved");
    for (size_t i = 0; i < pts.size() && i < c.getPoints().size(); ++i)
        check(c.getPoints()[i].x == pts[i].x && c.getPoints()[i].y == pts[i].y,
              "curveFromPoints: point values preserved exactly");
    json_decref(root);
    return c;
}

// y = x^e sampled on `steps` intervals (smooth strictly-monotone family member).
static std::vector<MonotoneCurve::Point> powCurve(int steps, float e) {
    std::vector<MonotoneCurve::Point> p;
    for (int i = 0; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        p.push_back(MonotoneCurve::Point{t, std::pow(t, e)});
    }
    return p;
}

// The round-trip family: strictly increasing y (a clean function under reflection).
static std::vector<std::vector<MonotoneCurve::Point>> roundTripFamily() {
    return {
        {MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{1.f, 1.f}},                                        // identity
        {MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.05f}, MonotoneCurve::Point{0.5f, 0.5f},
         MonotoneCurve::Point{0.8f, 0.95f}, MonotoneCurve::Point{1.f, 1.f}},                                     // S
        {MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.3f, 0.08f}, MonotoneCurve::Point{0.6f, 0.3f},
         MonotoneCurve::Point{1.f, 1.f}},                                                                        // concave
        {MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.4f, 0.7f}, MonotoneCurve::Point{0.7f, 0.92f},
         MonotoneCurve::Point{1.f, 1.f}},                                                                        // convex
        {MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.5f, 0.48f}, MonotoneCurve::Point{1.f, 1.f}},     // gentle
        powCurve(8, 2.f),
        powCurve(16, 1.5f),
    };
}

// ── 1. identity curve ──────────────────────────────────────────────────────────────────────────
static void testIdentityCurve() {
    std::printf("identity curve / pinned endpoints / clamping\n");
    MonotoneCurve c;
    check(c.size() == 2, "fresh curve is the two pinned endpoints");
    check(c[0].x == 0.f && c[0].y == 0.f, "front pinned to (0,0)");
    check(c[1].x == 1.f && c[1].y == 1.f, "back pinned to (1,1)");
    check(c.evalCDF(0.f) == 0.f, "evalCDF(0) == 0 exactly");
    check(c.evalCDF(1.f) == 1.f, "evalCDF(1) == 1 exactly");
    check(c.evalInverse(0.f) == 0.f, "evalInverse(0) == 0 exactly");
    check(c.evalInverse(1.f) == 1.f, "evalInverse(1) == 1 exactly");
    for (int k = 1; k < 100; ++k) {
        const float x = (float)k / 100.f;
        checkNear((double)c.evalCDF(x), (double)x, 1e-12, "identity: evalCDF(x) == x");
        checkNear((double)c.evalInverse(x), (double)x, 1e-12, "identity: evalInverse(u) == u");
        checkNear((double)c.pdf(x), 1.0, 1e-9, "identity: pdf == 1");
    }
    // Domain clamping
    check(c.evalCDF(-0.5f) == 0.f, "evalCDF clamps below 0");
    check(c.evalCDF(1.5f) == 1.f, "evalCDF clamps above 1");
    check(c.evalInverse(-1.f) == 0.f, "evalInverse clamps below 0");
    check(c.evalInverse(2.f) == 1.f, "evalInverse clamps above 1");
    check(c.pdf(-5.f) == c.pdf(0.f), "pdf clamps like evalCDF (below)");
    check(c.pdf(5.f) == c.pdf(1.f), "pdf clamps like evalCDF (above)");
    check(c.pdf(0.f) >= 0.f && c.pdf(1.f) >= 0.f, "pdf non-negative at the endpoints");
}

// ── 2. interpolation anchors ───────────────────────────────────────────────────────────────────
static void testInterpolationAnchors() {
    std::printf("PCHIP interpolates control points exactly\n");
    std::vector<std::vector<MonotoneCurve::Point>> family = roundTripFamily();
    // flats too — interpolation exactness must not depend on strict monotonicity
    family.push_back({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.2f},
                      MonotoneCurve::Point{0.45f, 0.2f}, MonotoneCurve::Point{0.6f, 0.6f},
                      MonotoneCurve::Point{0.85f, 0.6f}, MonotoneCurve::Point{1.f, 1.f}});
    for (const auto& pts : family) {
        MonotoneCurve c = curveFromPoints(pts);
        for (int i = 0; i < c.size(); ++i)
            check(c.evalCDF(c[i].x) == c[i].y, "evalCDF passes through control point exactly");
        // Inverse anchors: the first point of every equal-y run is an exact reflected knot
        // (F⁻¹(y_i) == x_i); flat-run right edges are carried by the nudge, not exact.
        for (int i = 0; i < c.size(); ++i) {
            if (i == 0 || c[i].y != c[i - 1].y)
                check(c.evalInverse(c[i].y) == c[i].x, "evalInverse passes through reflected knot exactly");
        }
        check(invariantsHold(c), "invariants hold");
    }
}

// ── 3. monotonicity / no overshoot ─────────────────────────────────────────────────────────────
static void testMonotonicityNoOvershoot() {
    std::printf("monotone sweeps (Fritsch-Carlson no-overshoot)\n");
    std::vector<std::vector<MonotoneCurve::Point>> family = roundTripFamily();
    family.push_back({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.05f, 0.98f},
                      MonotoneCurve::Point{1.f, 1.f}});                                                  // near-vertical
    family.push_back({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.1f, 0.02f},
                      MonotoneCurve::Point{0.2f, 0.98f}, MonotoneCurve::Point{1.f, 1.f}});               // kink
    family.push_back({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.2f},
                      MonotoneCurve::Point{0.45f, 0.2f}, MonotoneCurve::Point{0.6f, 0.6f},
                      MonotoneCurve::Point{0.85f, 0.6f}, MonotoneCurve::Point{1.f, 1.f}});               // staircase w/ flats
    family.push_back({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.3f, 0.f},
                      MonotoneCurve::Point{0.6f, 0.f}, MonotoneCurve::Point{1.f, 1.f}});                 // bottom flat
    for (const auto& pts : family) {
        MonotoneCurve c = curveFromPoints(pts);
        check(monotoneSweep(c, 2000), "evalCDF monotone non-decreasing over fine sweep");
        check(inverseMonotoneSweep(c, 2000), "evalInverse monotone non-decreasing over fine sweep");
        double lo = 0.0, hi = 1.0;
        for (int k = 0; k <= 2000; ++k) {
            const double v = c.evalCDF((double)k / 2000.0);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        check(lo >= -1e-12 && hi <= 1.0 + 1e-12, "evalCDF stays inside [0,1]");
    }
    // Random adversarial battery: random curves + wild drags, invariants + monotonicity
    // re-checked after every single operation.
    for (int trial = 0; trial < 25; ++trial) {
        const int interior = 3 + (int)(lcgUniform() * 8.f);   // 3..10 interior points
        std::vector<float> xs;
        while ((int)xs.size() < interior) {
            const float x = 0.02f + 0.96f * lcgUniform();
            bool dup = false;
            for (float e : xs) if (std::fabs(x - e) < 0.03f) { dup = true; break; }
            if (!dup) xs.push_back(x);
        }
        std::sort(xs.begin(), xs.end());
        std::vector<float> ys{0.f};
        for (int i = 0; i < interior; ++i) {
            float y;
            if (i > 0 && lcgUniform() < 0.25f) y = ys.back();           // occasional flat
            else y = ys.back() + (0.9f - ys.back()) * (0.15f + 0.7f * lcgUniform());
            ys.push_back(y);
        }
        ys.push_back(1.f);
        std::vector<MonotoneCurve::Point> pts;
        for (int i = 0; i < interior + 2; ++i) {
            const float x = (i == 0) ? 0.f : (i == interior + 1 ? 1.f : xs[i - 1]);
            pts.push_back(MonotoneCurve::Point{x, ys[i]});
        }
        MonotoneCurve c = curveFromPoints(pts);
        check(invariantsHold(c) && monotoneSweep(c, 1000) && inverseMonotoneSweep(c, 1000),
              "random curve: invariants + monotone CDF and inverse");
        for (int i = 1; i < c.size() - 1; ++i) {
            c.setPoint(i, -1.f + 3.f * lcgUniform(), -1.f + 3.f * lcgUniform());
            check(invariantsHold(c) && monotoneSweep(c, 500),
                  "random curve: invariants + monotonicity survive adversarial setPoint");
        }
        c.addPoint(0.01f + 0.98f * lcgUniform());
        check(invariantsHold(c) && monotoneSweep(c, 500), "random curve: invariants survive addPoint");
        if (c.size() > 3) c.removePoint(1 + (int)(lcgUniform() * (float)(c.size() - 2)));
        check(invariantsHold(c) && monotoneSweep(c, 500), "random curve: invariants survive removePoint");
    }
}

// ── 4. setPoint clamping ───────────────────────────────────────────────────────────────────────
static void testSetPointClamping() {
    std::printf("setPoint clamping / immovable endpoints\n");
    MonotoneCurve c = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.05f},
                                       MonotoneCurve::Point{0.5f, 0.5f}, MonotoneCurve::Point{0.8f, 0.95f},
                                       MonotoneCurve::Point{1.f, 1.f}});
    // y clamped down to the right neighbour
    c.setPoint(1, 0.2f, 0.99f);
    check(c[1].y == 0.5f, "setPoint y clamped down to right neighbour's y");
    check(c[1].x == 0.2f, "setPoint x untouched when already inside");
    check(invariantsHold(c) && monotoneSweep(c), "invariants after y-clamp-down");
    // y clamped up to the left neighbour
    c.setPoint(1, 0.2f, -0.5f);
    check(c[1].y == 0.f, "setPoint y clamped up to left neighbour's y");
    check(invariantsHold(c) && monotoneSweep(c), "invariants after y-clamp-up");
    // x clamped inside, keeping MIN_SPACING from the neighbours
    c.setPoint(1, -1.f, 0.3f);
    check(c[1].x == MonotoneCurve::MIN_SPACING, "setPoint x clamped to left neighbour + MIN_SPACING");
    check(c[1].y == 0.3f, "y inside [left.y, right.y] kept as requested");
    c.setPoint(1, 5.f, 0.3f);
    check(c[1].x == 0.5f - MonotoneCurve::MIN_SPACING, "setPoint x clamped to right neighbour - MIN_SPACING");
    check(invariantsHold(c) && monotoneSweep(c), "invariants after x-clamps");
    // endpoints are immovable
    const MonotoneCurve::Point p0 = c[0], pN = c[c.size() - 1];
    c.setPoint(0, 0.5f, 0.9f);
    c.setPoint(c.size() - 1, 0.5f, 0.1f);
    check(c[0].x == p0.x && c[0].y == p0.y, "setPoint(0, ...) is a no-op (endpoint pinned)");
    check(c[c.size() - 1].x == pN.x && c[c.size() - 1].y == pN.y, "setPoint(last, ...) is a no-op (endpoint pinned)");
    // out-of-range and NaN inputs are no-ops
    const MonotoneCurve before = c;
    c.setPoint(-1, 0.5f, 0.5f);
    c.setPoint(99, 0.5f, 0.5f);
    c.setPoint(1, NAN, NAN);
    check(samePoints(c, before), "setPoint with OOB index / NaN is a no-op");
    check(invariantsHold(c), "invariants after no-ops");
}

// ── 5. addPoint ────────────────────────────────────────────────────────────────────────────────
static void testAddPointBehaviour() {
    std::printf("addPoint lands on the curve / rejections / cap\n");
    MonotoneCurve c = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.05f},
                                       MonotoneCurve::Point{0.5f, 0.5f}, MonotoneCurve::Point{0.8f, 0.95f},
                                       MonotoneCurve::Point{1.f, 1.f}});
    const MonotoneCurve snapshot = c;
    const double x = 0.33;
    const double expectedY = snapshot.evalCDF(x);   // PRE-insert value
    c.addPoint(x);
    check(c.size() == snapshot.size() + 1, "addPoint inserted a point");
    int idx = -1;
    for (int i = 0; i < c.size(); ++i)
        if (std::fabs(c[i].x - x) < 1e-9f) idx = i;
    check(idx > 0, "new point found in the list");
    check(c[idx].y == expectedY, "new point lands ON the curve (y == pre-insert evalCDF(x), exact)");
    check(invariantsHold(c) && monotoneSweep(c), "invariants + monotonicity after addPoint");
    for (int i = 0; i + 1 < c.size(); ++i)
        check(c.evalCDF(c[i].x) == c[i].y, "anchors preserved after insertion");
    // rejections
    const int sizeBefore = c.size();
    c.addPoint(0.f); c.addPoint(1.f); c.addPoint(-0.2f); c.addPoint(1.2f);
    check(c.size() == sizeBefore, "addPoint at/outside the pinned endpoints' x rejected");
    c.addPoint(0.5f);
    check(c.size() == sizeBefore, "addPoint duplicating an existing x rejected");
    c.addPoint(0.5f + 0.5f * MonotoneCurve::MIN_SPACING);
    check(c.size() == sizeBefore, "addPoint within MIN_SPACING of an existing x rejected");
    check(invariantsHold(c), "invariants after rejections");
    // cap: fill an identity curve to MAX_POINTS, then reject
    MonotoneCurve full;
    for (int i = 1; i <= 30; ++i) full.addPoint((float)i / 31.f);
    check(full.size() == MonotoneCurve::MAX_POINTS, "curve filled to MAX_POINTS (32)");
    check(invariantsHold(full) && monotoneSweep(full), "invariants at the cap");
    full.addPoint(0.5f);
    check(full.size() == MonotoneCurve::MAX_POINTS, "addPoint beyond 32 rejected (cap)");
    full.addPoint(15.f / 31.f);
    check(full.size() == MonotoneCurve::MAX_POINTS, "addPoint duplicate x rejected at the cap");
    // shuffled insertion order still ends sorted
    MonotoneCurve shuffled;
    const float xs[] = {0.7f, 0.2f, 0.5f, 0.9f, 0.35f};
    for (float x : xs) shuffled.addPoint(x);
    check(shuffled.size() == 7, "shuffled adds all landed");
    for (int i = 0; i + 1 < shuffled.size(); ++i)
        check(shuffled[i + 1].x > shuffled[i].x, "points stay sorted by x after shuffled adds");
    check(invariantsHold(shuffled), "invariants after shuffled adds");
}

// ── 6. removePoint ─────────────────────────────────────────────────────────────────────────────
static void testRemovePointBehaviour() {
    std::printf("removePoint never removes endpoints\n");
    MonotoneCurve c = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.05f},
                                       MonotoneCurve::Point{0.5f, 0.5f}, MonotoneCurve::Point{0.8f, 0.95f},
                                       MonotoneCurve::Point{1.f, 1.f}});
    c.removePoint(0);
    check(c.size() == 5, "removePoint(0) rejected (endpoint)");
    c.removePoint(4);
    check(c.size() == 5, "removePoint(last) rejected (endpoint)");
    c.removePoint(-1);
    c.removePoint(99);
    check(c.size() == 5, "removePoint with OOB index rejected");
    c.removePoint(2);
    check(c.size() == 4, "interior removal works");
    check(invariantsHold(c) && monotoneSweep(c), "invariants + monotonicity after removal");
    for (int i = 0; i < c.size(); ++i)
        check(c.evalCDF(c[i].x) == c[i].y, "anchors preserved after removal");
    while (c.size() > 2) c.removePoint(1);
    check(c.size() == MonotoneCurve::MIN_POINTS, "can remove down to MIN_POINTS");
    c.removePoint(1);
    check(c.size() == MonotoneCurve::MIN_POINTS, "at MIN_POINTS every index is an endpoint -> no-op");
    check(c[0].x == 0.f && c[0].y == 0.f && c[1].x == 1.f && c[1].y == 1.f,
          "the floor is exactly the pinned identity");
}

// ── 7. inverse round-trip ──────────────────────────────────────────────────────────────────────
static void roundTripCase(const std::vector<MonotoneCurve::Point>& pts, double tol, const char* label) {
    MonotoneCurve c = curveFromPoints(pts);
    double maxFwd = 0.0, maxBack = 0.0;   // |F⁻¹(F(x)) - x| and |F(F⁻¹(u)) - u|
    for (int k = 0; k <= 400; ++k) {
        const double x = 0.001 + 0.998 * (double)k / 400.0;
        const double fwd = std::fabs(c.evalInverse(c.evalCDF(x)) - x);
        if (fwd > maxFwd) maxFwd = fwd;
        const double u = 0.001 + 0.998 * (double)k / 400.0;
        const double back = std::fabs(c.evalCDF(c.evalInverse(u)) - u);
        if (back > maxBack) maxBack = back;
    }
    checkNear(maxFwd, 0.0, tol, label);
    checkNear(maxBack, 0.0, tol, label);
}
static void testInverseRoundTrip() {
    std::printf("evalInverse(evalCDF(x)) round-trip\n");
    const auto family = roundTripFamily();
    roundTripCase(family[0], 1e-11, "round-trip: identity");
    roundTripCase(family[1], 1e-10, "round-trip: S curve");
    roundTripCase(family[2], 1e-10, "round-trip: concave");
    roundTripCase(family[3], 1e-10, "round-trip: convex");
    roundTripCase(family[4], 1e-10, "round-trip: gentle");
    roundTripCase(family[5], 1e-10, "round-trip: x^2 (9 pts)");
    roundTripCase(family[6], 1e-10, "round-trip: dense 17 pts");
    // The steep/kink adversarial curves' PCHIP tails approach ZERO slope at x=1 (endpoint
    // limiting), so the pointwise round-trip near the end amplifies eval noise by ~1/F' —
    // but from a 1e-16 double floor that stays ~1e-11 over the sweep (the float version
    // measured ~1e-3 there). Monotonicity (the real invariant) is asserted in section 3.
    roundTripCase({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.05f, 0.98f},
                   MonotoneCurve::Point{1.f, 1.f}}, 1e-10, "round-trip: near-vertical (flat tail amplifies noise)");
    roundTripCase({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.1f, 0.02f},
                   MonotoneCurve::Point{0.2f, 0.98f}, MonotoneCurve::Point{1.f, 1.f}},
                  1e-10, "round-trip: kink (flat tail amplifies noise)");
}

// ── 8. flats (zero-density gaps) ───────────────────────────────────────────────────────────────
static void testInverseFlats() {
    std::printf("flat segments: inverse gap semantics\n");
    MonotoneCurve c = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.2f},
                                       MonotoneCurve::Point{0.45f, 0.2f}, MonotoneCurve::Point{0.6f, 0.6f},
                                       MonotoneCurve::Point{0.85f, 0.6f}, MonotoneCurve::Point{1.f, 1.f}});
    check(c.evalInverse(0.2f) == 0.2f, "flat quantile maps to the gap's LEFT edge (exact knot)");
    check(c.evalInverse(0.6f) == 0.6f, "second flat likewise");
    // Quantiles between the flats' heights must land in the POST-gap rising regions, not in
    // the zero-density gaps themselves: u in (0.2, 0.6) -> x in (0.45, 0.6); u in (0.6, 1) ->
    // x in (0.85, 1]. (A plain keep-first dedupe would map u=0.4 to x=0.4 — inside the gap.)
    check(c.evalInverse(0.3f) > 0.45f && c.evalInverse(0.3f) < 0.6f, "u=0.3 lands in the steep region after the gap");
    check(c.evalInverse(0.5f) > 0.45f && c.evalInverse(0.5f) < 0.6f, "u=0.5 lands in the steep region after the gap");
    check(c.evalInverse(0.9f) > 0.85f, "u=0.9 lands after the second gap");
    check(inverseMonotoneSweep(c, 2000), "inverse stays monotone across flats");
    // pdf is (numerically) zero inside a gap: zero density
    checkNear((double)c.pdf(0.3f), 0.0, 1e-9, "pdf == 0 inside a flat (gap interior)");
    checkNear((double)c.pdf(0.7f), 0.0, 1e-9, "pdf == 0 inside the second flat");
    // Round-trip on the RISING stretches (the gaps themselves are covered by the edge
    // semantics above): F is strictly increasing there, so the exact inversion round-trips.
    // Stay clear of the zero-slope knots bounding the gaps (ill-conditioned inversion).
    double maxRt = 0.0;
    for (int k = 0; k <= 400; ++k) {
        const double x = 0.001 + 0.998 * (double)k / 400.0;
        if (x > 0.2 - 5e-3 && x < 0.45 + 5e-3) continue;   // gap 1 + knot margins
        if (x > 0.6 - 5e-3 && x < 0.85 + 5e-3) continue;   // gap 2 + knot margins
        const double rt = std::fabs(c.evalInverse(c.evalCDF(x)) - x);
        if (rt > maxRt) maxRt = rt;
    }
    checkNear(maxRt, 0.0, 1e-10, "staircase: round-trip on the rising stretches");
    // A flat that touches the BOTTOM edge: the reflection still carries the gap's far edge —
    // the mid quantile must land in the post-gap rising region (x > 0.6), not at u itself.
    MonotoneCurve b = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.3f, 0.f},
                                       MonotoneCurve::Point{0.6f, 0.f}, MonotoneCurve::Point{1.f, 1.f}});
    check(b.evalInverse(0.f) == 0.f, "bottom flat: Q(0) == 0 still exact");
    check(b.evalInverse(0.5f) > 0.6f, "bottom flat: mid quantile lands after the gap");
    check(b.evalInverse(0.5f) <= 1.f, "bottom flat: inverse stays <= 1");
    check(inverseMonotoneSweep(b, 2000), "bottom flat: inverse monotone");
    // A flat that touches the TOP edge keeps Q(1) == 1 exactly.
    MonotoneCurve t = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.4f, 0.7f},
                                       MonotoneCurve::Point{0.7f, 1.f}, MonotoneCurve::Point{1.f, 1.f}});
    check(t.evalInverse(1.f) == 1.f, "top flat: Q(1) == 1 still exact");
    check(t.evalCDF(1.f) == 1.f, "top flat: F(1) == 1 still exact");
    check(inverseMonotoneSweep(t, 2000), "top flat: inverse monotone");
}

// ── 9. pdf vs numerical derivative ─────────────────────────────────────────────────────────────
static void pdfCase(const std::vector<MonotoneCurve::Point>& pts, const char* label) {
    MonotoneCurve c = curveFromPoints(pts);
    const double h = 1e-5;
    int checked = 0;
    for (double x = 0.01; x <= 0.99; x += 0.0037) {
        // Skip knot neighbourhoods: the derivative is discontinuous AT control points and a
        // central difference across a kink mixes the two one-sided slopes.
        bool nearKnot = false;
        for (const auto& p : pts)
            if (std::fabs(x - p.x) < 3.0 * h) { nearKnot = true; break; }
        if (nearKnot) continue;
        const double num = (c.evalCDF(x + h) - c.evalCDF(x - h)) / (2.0 * h);
        checkNear(c.pdf(x), num, 1e-6, "pdf == numerical derivative of evalCDF");
        check(c.pdf(x) >= -1e-12, "pdf non-negative (monotone CDF)");
        ++checked;
    }
    check(checked > 100, "pdf sweep actually exercised interior points");
    std::printf("  pdf: %s (%d sweep points)\n", label, checked);
}
static void testPdfMatchesNumerical() {
    std::printf("pdf == analytic slope of the PCHIP CDF\n");
    const auto family = roundTripFamily();
    pdfCase(family[1], "S curve");
    pdfCase(family[2], "concave");
    pdfCase(family[3], "convex");
    pdfCase(family[4], "gentle");
    pdfCase(family[5], "x^2");
    pdfCase({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.2f},
             MonotoneCurve::Point{0.45f, 0.2f}, MonotoneCurve::Point{0.6f, 0.6f},
             MonotoneCurve::Point{0.85f, 0.6f}, MonotoneCurve::Point{1.f, 1.f}}, "staircase with flats");
}

// ── 10. reflected-curve cache invalidation ─────────────────────────────────────────────────────
static void testInverseCacheInvalidation() {
    std::printf("reflected-curve cache invalidated by edits\n");
    const std::vector<MonotoneCurve::Point> base = {MonotoneCurve::Point{0.f, 0.f},
                                                    MonotoneCurve::Point{0.25f, 0.1f},
                                                    MonotoneCurve::Point{0.5f, 0.5f},
                                                    MonotoneCurve::Point{0.75f, 0.9f},
                                                    MonotoneCurve::Point{1.f, 1.f}};
    MonotoneCurve a = curveFromPoints(base);
    const float before = a.evalInverse(0.5f);   // builds the cache
    // setPoint: drag the middle point up
    a.setPoint(2, 0.5f, 0.75f);
    MonotoneCurve expected = curveFromPoints({MonotoneCurve::Point{0.f, 0.f},
                                              MonotoneCurve::Point{0.25f, 0.1f},
                                              MonotoneCurve::Point{0.5f, 0.75f},
                                              MonotoneCurve::Point{0.75f, 0.9f},
                                              MonotoneCurve::Point{1.f, 1.f}});
    checkNear((double)a.evalInverse(0.5f), (double)expected.evalInverse(0.5f), 1e-7,
              "cache rebuilt after setPoint (matches a fresh curve)");
    check(std::fabs((double)a.evalInverse(0.5f) - (double)before) > 1e-3, "the edit actually moved the inverse");
    // addPoint
    a.addPoint(0.6f);
    MonotoneCurve expected2 = curveFromPoints({MonotoneCurve::Point{0.f, 0.f},
                                               MonotoneCurve::Point{0.25f, 0.1f},
                                               MonotoneCurve::Point{0.5f, 0.75f},
                                               MonotoneCurve::Point{0.6f, a.evalCDF(0.6f)},
                                               MonotoneCurve::Point{0.75f, 0.9f},
                                               MonotoneCurve::Point{1.f, 1.f}});
    checkNear((double)a.evalInverse(0.5f), (double)expected2.evalInverse(0.5f), 1e-7,
              "cache rebuilt after addPoint");
    // removePoint — note expected2's addPoint used the post-setPoint CDF, so re-derive:
    a.removePoint(3);
    checkNear((double)a.evalInverse(0.5f), (double)expected.evalInverse(0.5f), 1e-7,
              "cache rebuilt after removePoint (back to the setPoint state)");
    // evalCDF/pdf must ALSO reflect edits (same cache rebuild path)
    checkNear((double)a.evalCDF(0.5f), (double)expected.evalCDF(0.5f), 1e-7, "CDF cache rebuilt after edits");
    checkNear((double)a.pdf(0.35f), (double)expected.pdf(0.35f), 1e-7, "pdf cache rebuilt after edits");
}

// ── JSON helpers ───────────────────────────────────────────────────────────────────────────────
static json_t* pointsArr(std::initializer_list<std::pair<double, double>> pts) {
    json_t* a = json_array();
    for (const auto& p : pts) {
        json_t* pair = json_array();
        json_array_append_new(pair, json_real(p.first));
        json_array_append_new(pair, json_real(p.second));
        json_array_append_new(a, pair);
    }
    return a;
}
// Build a .dmcurve-shaped envelope around `points` (ownership taken). String guards are
// emitted only when non-null; version only when withVersion.
static json_t* dmcurveJson(json_t* points, const char* format, bool withVersion, json_int_t version,
                           const char* type, const char* interp, const char* endpoints) {
    json_t* root = json_object();
    json_object_set_new(root, "format", json_string(format));
    if (withVersion) json_object_set_new(root, "version", json_integer(version));
    if (type)      json_object_set_new(root, "type", json_string(type));
    if (interp)    json_object_set_new(root, "interp", json_string(interp));
    if (endpoints) json_object_set_new(root, "endpoints", json_string(endpoints));
    json_object_set_new(root, "points", points);
    return root;
}
static json_t* dmcurveDefault(json_t* points) {
    return dmcurveJson(points, "dmcurve", true, 1, "drawn", "pchip", "pinned");
}
static void expectRejected(MonotoneCurve& c, json_t* j, const char* what) {
    const MonotoneCurve before = c;
    check(!c.fromJson(j), what);
    check(samePoints(c, before), "state untouched after rejection");
    json_decref(j);
}

// ── 11. toJson / fromJson round-trip ───────────────────────────────────────────────────────────
static void testJsonRoundTrip() {
    std::printf("toJson -> fromJson round-trip\n");
    MonotoneCurve c = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.2f, 0.05f},
                                       MonotoneCurve::Point{0.5f, 0.5f}, MonotoneCurve::Point{0.8f, 0.95f},
                                       MonotoneCurve::Point{1.f, 1.f}});
    c.setPoint(1, 0.22f, 0.07f);                       // exercise the drag path too
    c.addPoint(0.65f);
    c.name = "Test S";
    c.author = "rodney";
    c.description = "an S-ish curve";

    // json_t* round-trip
    json_t* j = c.toJson();
    check(j != NULL, "toJson produced an object");
    MonotoneCurve c2;
    check(c2.fromJson(j), "fromJson accepted our own output");
    check(samePoints(c, c2), "points identical after json_t* round-trip (exact floats)");
    check(c2.name == "Test S" && c2.author == "rodney" && c2.description == "an S-ish curve",
          "metadata survives the round-trip");
    json_decref(j);

    // TEXT round-trip: dumps -> loads -> fromJson (the emitted schema is genuinely parseable)
    json_t* j1 = c.toJson();
    char* text = json_dumps(j1, JSON_INDENT(2));
    check(text != NULL, "json_dumps produced text");
    check(std::strstr(text, "\"format\"") != NULL && std::strstr(text, "dmcurve") != NULL,
          "dumped text carries the format tag");
    json_error_t err;
    json_t* j2 = json_loads(text, 0, &err);
    check(j2 != NULL, "json_loads parsed our dumped text");
    MonotoneCurve c3;
    check(c3.fromJson(j2), "fromJson accepted the re-parsed text");
    check(samePoints(c, c3), "points identical after text round-trip (exact floats)");
    free(text);
    json_decref(j1);
    json_decref(j2);

    // Second generation is byte-identical (idempotent serialize)
    json_t* ja = c.toJson(), *jb = c3.toJson();
    char* ta = json_dumps(ja, JSON_INDENT(2));
    char* tb = json_dumps(jb, JSON_INDENT(2));
    check(std::strcmp(ta, tb) == 0, "second-generation output byte-identical");
    free(ta); free(tb);
    json_decref(ja); json_decref(jb);

    // A second fromJson fully REPLACES state (no leakage of the previous curve)
    MonotoneCurve c4 = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{1.f, 1.f}});
    c4.name = "old";
    json_t* jmin = dmcurveDefault(pointsArr({{0.0, 0.0}, {0.4, 0.6}, {1.0, 1.0}}));
    check(c4.fromJson(jmin), "second load accepted");
    check(c4.size() == 3, "second load replaced the point list");
    check(c4.name.empty(), "second load cleared stale metadata");
    json_decref(jmin);
}

// ── 12a. schema fields ─────────────────────────────────────────────────────────────────────────
static void testJsonSchemaFields() {
    std::printf("v1 schema fields\n");
    MonotoneCurve c = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.3f, 0.2f},
                                       MonotoneCurve::Point{1.f, 1.f}});
    json_t* j = c.toJson();
    check(json_is_object(j), "root is an object");
    json_t* jf = json_object_get(j, "format");
    check(json_is_string(jf) && std::string(json_string_value(jf)) == "dmcurve", "format == \"dmcurve\"");
    json_t* jv = json_object_get(j, "version");
    check(json_is_integer(jv) && (int)json_integer_value(jv) == 1, "version == 1 (integer)");
    json_t* jt = json_object_get(j, "type");
    check(json_is_string(jt) && std::string(json_string_value(jt)) == "drawn", "type == \"drawn\"");
    json_t* ji = json_object_get(j, "interp");
    check(json_is_string(ji) && std::string(json_string_value(ji)) == "pchip", "interp == \"pchip\"");
    json_t* je = json_object_get(j, "endpoints");
    check(json_is_string(je) && std::string(json_string_value(je)) == "pinned", "endpoints == \"pinned\"");
    json_t* jp = json_object_get(j, "params");
    check(json_is_object(jp), "params is an object (empty for drawn)");
    json_t* jpts = json_object_get(j, "points");
    check(json_is_array(jpts) && (int)json_array_size(jpts) == c.size(), "points array matches size");
    bool ptsOk = true;
    for (int i = 0; i < c.size(); ++i) {
        json_t* pair = json_array_get(jpts, (size_t)i);
        if (!json_is_array(pair) || json_array_size(pair) != 2) { ptsOk = false; break; }
        json_t* jx = json_array_get(pair, 0);
        json_t* jy = json_array_get(pair, 1);
        if (!json_is_number(jx) || !json_is_number(jy)) { ptsOk = false; break; }
        if ((float)json_number_value(jx) != c[i].x || (float)json_number_value(jy) != c[i].y) { ptsOk = false; break; }
    }
    check(ptsOk, "points are [[x,y], ...] number pairs with exact values");
    json_decref(j);
}

// ── 12b. validation / rejection ────────────────────────────────────────────────────────────────
static void testJsonValidation() {
    std::printf("fromJson validation (rejects, state untouched)\n");
    MonotoneCurve c = curveFromPoints({MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.3f, 0.2f},
                                       MonotoneCurve::Point{1.f, 1.f}});
    c.name = "keepme";

    expectRejected(c, json_object(), "empty object rejected");
    expectRejected(c, dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "notacurve", true, 1, nullptr, nullptr, nullptr),
                   "wrong format tag rejected");
    expectRejected(c, dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "dotmodular.tuning", true, 1, nullptr, nullptr, nullptr),
                   "a .dmtune object rejected");
    expectRejected(c, dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "dmcurve", true, 2, nullptr, nullptr, nullptr),
                   "version 2 rejected");
    expectRejected(c, dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "dmcurve", true, 0, nullptr, nullptr, nullptr),
                   "version 0 rejected");
    {
        json_t* j = dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "dmcurve", true, 1, nullptr, nullptr, nullptr);
        json_object_set_new(j, "version", json_real(1.0));   // non-integer version
        expectRejected(c, j, "non-integer version rejected");
    }
    expectRejected(c, dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "dmcurve", true, 1, "power", nullptr, nullptr),
                   "parametric type \"power\" rejected (v1 is drawn-only)");
    expectRejected(c, dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "dmcurve", true, 1, nullptr, "linear", nullptr),
                   "interp \"linear\" rejected (v1 is pchip-only)");
    expectRejected(c, dmcurveJson(pointsArr({{0, 0}, {1, 1}}), "dmcurve", true, 1, nullptr, nullptr, "free"),
                   "endpoints \"free\" rejected (v1 is pinned-only)");
    // points shape
    {
        json_t* j = dmcurveDefault(json_string("nope"));
        expectRejected(c, j, "points not an array rejected");
    }
    expectRejected(c, dmcurveDefault(pointsArr({{0, 0}})), "single point rejected");
    {
        json_t* many = json_array();
        for (int i = 0; i < 33; ++i) {
            json_t* pair = json_array();
            json_array_append_new(pair, json_real((double)i / 34.0));
            json_array_append_new(pair, json_real((double)i / 34.0));
            json_array_append_new(many, pair);
        }
        json_t* j = dmcurveDefault(many);
        expectRejected(c, j, "33 points rejected (over MAX_POINTS)");
    }
    {
        json_t* j = dmcurveDefault(json_array());
        json_array_append_new(j, json_real(0.5));   // element not a pair
        expectRejected(c, j, "point element not an array rejected");
    }
    expectRejected(c, dmcurveDefault(pointsArr({{0.5, 0.5}})), "not enough valid points");
    {
        json_t* j = dmcurveDefault(pointsArr({{0, 0}, {1, 1}}));
        json_t* triple = json_array();
        json_array_append_new(triple, json_real(0.5));
        json_array_append_new(triple, json_real(0.5));
        json_array_append_new(triple, json_real(0.5));
        json_array_append_new(json_object_get(j, "points"), triple);
        expectRejected(c, j, "point element of size 3 rejected");
    }
    {
        json_t* j = dmcurveDefault(pointsArr({{0, 0}, {1, 1}}));
        json_t* pair = json_array();
        json_array_append_new(pair, json_string("x"));
        json_array_append_new(pair, json_real(0.5));
        json_array_append_new(json_object_get(j, "points"), pair);   // mixed in a bad pair
        expectRejected(c, j, "non-numeric coordinate rejected");
    }
    check(c.name == "keepme", "metadata untouched by rejections");
}

// ── 12c. sanitization ──────────────────────────────────────────────────────────────────────────
static void sanitizeCase(json_t* j, const std::vector<MonotoneCurve::Point>& expected, const char* what) {
    MonotoneCurve c;
    check(c.fromJson(j), what);   // sanitized, not rejected
    check(c.size() == (int)expected.size(), "sanitize: point count as expected");
    bool ok = c.size() == (int)expected.size();
    for (int i = 0; ok && i < c.size(); ++i)
        ok = c[i].x == expected[i].x && c[i].y == expected[i].y;
    check(ok, "sanitize: exact sanitized values");
    check(invariantsHold(c) && monotoneSweep(c), "sanitize: invariants + monotonicity hold");
    json_decref(j);
}
static void testJsonSanitize() {
    std::printf("fromJson sanitization (clamp/sort/dedupe/pin/running-max)\n");
    // out-of-range coordinates clamped into [0,1]
    sanitizeCase(dmcurveDefault(pointsArr({{-0.5, 0.0}, {0.5, 0.5}, {1.2, 1.0}})),
                 {MonotoneCurve::Point{0.f, 0.f}, MonotoneCurve::Point{0.5f, 0.5f}, MonotoneCurve::Point{1.f, 1.f}},
                 "out-of-range coordinates clamped, then loaded");
    // unsorted x sorted
    sanitizeCase(dmcurveDefault(pointsArr({{1.0, 1.0}, {0.5, 0.6}, {0.0, 0.0}})),
                 {MonotoneCurve::Point{0.0, 0.0}, MonotoneCurve::Point{0.5, 0.6}, MonotoneCurve::Point{1.0, 1.0}},
                 "unsorted points sorted by x, then loaded");
    // non-monotone y lifted to a running maximum
    sanitizeCase(dmcurveDefault(pointsArr({{0.0, 0.0}, {0.3, 0.8}, {0.6, 0.2}, {1.0, 1.0}})),
                 {MonotoneCurve::Point{0.0, 0.0}, MonotoneCurve::Point{0.3, 0.8},
                  MonotoneCurve::Point{0.6, 0.8}, MonotoneCurve::Point{1.0, 1.0}},
                 "non-monotone y lifted to running max, then loaded");
    // duplicate x: first wins, duplicates dropped
    sanitizeCase(dmcurveDefault(pointsArr({{0.0, 0.0}, {0.5, 0.3}, {0.5, 0.7}, {1.0, 1.0}})),
                 {MonotoneCurve::Point{0.0, 0.0}, MonotoneCurve::Point{0.5, 0.3}, MonotoneCurve::Point{1.0, 1.0}},
                 "duplicate x dropped (first wins)");
    // missing corner anchors are INSERTED (the file's shape is preserved, not eaten)
    sanitizeCase(dmcurveDefault(pointsArr({{0.2, 0.1}, {0.7, 0.9}})),
                 {MonotoneCurve::Point{0.0, 0.0}, MonotoneCurve::Point{0.2, 0.1},
                  MonotoneCurve::Point{0.7, 0.9}, MonotoneCurve::Point{1.0, 1.0}},
                 "corner anchors (0,0)/(1,1) inserted when missing");
    // ...but a file whose corner insertion would exceed MAX_POINTS is rejected
    {
        MonotoneCurve c;
        json_t* many = json_array();
        for (int i = 0; i < 32; ++i) {   // 32 interior points, no corners
            json_t* pair = json_array();
            json_array_append_new(pair, json_real(0.01 + 0.98 * (double)i / 31.0));
            json_array_append_new(pair, json_real(0.01 + 0.90 * (double)i / 31.0));
            json_array_append_new(many, pair);
        }
        json_t* j = dmcurveDefault(many);
        check(!c.fromJson(j), "corner insertion beyond MAX_POINTS rejected");
        json_decref(j);
    }
    // everything collapsed together -> rejected, not sanitized into garbage
    {
        MonotoneCurve c;
        json_t* j = dmcurveDefault(pointsArr({{0.5, 0.2}, {0.5, 0.9}}));
        check(!c.fromJson(j), "all-duplicate points rejected (cannot sanitize below MIN_POINTS)");
        json_decref(j);
    }
    // integer-valued coordinates (hand-written JSON often has [0,0] not [0.0,0.0])
    {
        json_t* j = dmcurveDefault(json_array());
        json_t* pts = json_object_get(j, "points");
        json_t* a = json_array(); json_array_append_new(a, json_integer(0)); json_array_append_new(a, json_integer(0));
        json_t* b = json_array(); json_array_append_new(b, json_integer(1)); json_array_append_new(b, json_integer(1));
        json_array_append_new(pts, a); json_array_append_new(pts, b);
        MonotoneCurve c;
        check(c.fromJson(j), "integer-valued coordinates accepted");
        check(c.size() == 2 && c[0].x == 0.f && c[1].y == 1.f, "integer points loaded as the identity");
        json_decref(j);
    }
    // missing optional fields tolerated
    {
        MonotoneCurve c;
        json_t* j = dmcurveJson(pointsArr({{0.0, 0.0}, {0.4, 0.6}, {1.0, 1.0}}),
                                "dmcurve", false, 0, nullptr, nullptr, nullptr);
        check(c.fromJson(j), "missing version/type/interp/endpoints tolerated (defaults)");
        check(c.size() == 3 && c.name.empty(), "defaults applied, no metadata");
        json_decref(j);
    }
    // params content is tolerated-and-ignored for drawn curves
    {
        json_t* j = dmcurveDefault(pointsArr({{0.0, 0.0}, {1.0, 1.0}}));
        json_object_set_new(j, "params", json_string("junk"));
        MonotoneCurve c;
        check(c.fromJson(j), "params content ignored for drawn curves");
        json_decref(j);
    }
    // metadata read when present
    {
        json_t* j = dmcurveDefault(pointsArr({{0.0, 0.0}, {1.0, 1.0}}));
        json_object_set_new(j, "name", json_string("n"));
        json_object_set_new(j, "author", json_string("a"));
        json_object_set_new(j, "description", json_string("d"));
        MonotoneCurve c;
        check(c.fromJson(j), "metadata-bearing curve loads");
        check(c.name == "n" && c.author == "a" && c.description == "d", "metadata fields read");
        json_decref(j);
    }
}

// ── 13. fromDistribution (parametric → nearby editable PCHIP; Phase 2.5) ──────────────────────
static void fromDistributionCase(const dotModular::Distribution& d, const char* name, double tol) {
    MonotoneCurve c = MonotoneCurve::fromDistribution(d, 9);
    check(c.size() == 9, "fromDistribution: n control points");
    check(invariantsHold(c), "fromDistribution: invariants (sorted/monotone/pinned)");
    check(c.evalCDF(0.0) == 0.0 && c.evalCDF(1.0) == 1.0, "fromDistribution: pinned corners exact");
    check(monotoneSweep(c, 1000), "fromDistribution: monotone sweep");
    // NEARBY: the 9-point PCHIP approximates the parametric CDF (it's a starting shape,
    // not a copy — the tolerance IS the approximation quality)
    double maxErr = 0.0;
    for (int k = 0; k <= 200; ++k) {
        const double x = (double)k / 200.0;
        const double err = std::fabs(c.evalCDF(x) - d.cdf(x));
        if (err > maxErr) maxErr = err;
    }
    checkNear(maxErr, 0.0, tol, "fromDistribution: evalCDF ≈ parametric cdf (nearby)");
    // A denser sampling is at least as close
    MonotoneCurve c17 = MonotoneCurve::fromDistribution(d, 17);
    double maxErr17 = 0.0;
    for (int k = 0; k <= 200; ++k) {
        const double x = (double)k / 200.0;
        const double err = std::fabs(c17.evalCDF(x) - d.cdf(x));
        if (err > maxErr17) maxErr17 = err;
    }
    check(maxErr17 < maxErr + 1e-9, "fromDistribution: 17 points at least as close as 9");
    check(invariantsHold(c17) && monotoneSweep(c17, 500), "fromDistribution: 17-pt invariants");
    // The result is an ORDINARY editable curve: drags work, JSON round-trips
    c.setPoint(1, c[1].x, 0.5);
    check(invariantsHold(c) && monotoneSweep(c, 500), "fromDistribution: editable after conversion");
    json_t* j = c.toJson();
    MonotoneCurve c2;
    check(c2.fromJson(j), "fromDistribution: JSON round-trip loads");
    check(samePoints(c, c2), "fromDistribution: JSON round-trip identical");
    json_decref(j);
    (void)name;
}
static void testFromDistribution() {
    std::printf("fromDistribution (parametric -> nearby PCHIP)\n");
    fromDistributionCase(dotModular::TriangularDistribution(0.3), "triangular", 5e-3);
    fromDistributionCase(dotModular::PowerDistribution(2.0), "power", 5e-3);
    fromDistributionCase(dotModular::KumaraswamyDistribution(2.0, 3.0), "kumaraswamy", 1e-2);
    fromDistributionCase(dotModular::LogitNormalDistribution(0.0, 1.0), "logit-normal", 2e-2);
    fromDistributionCase(dotModular::MobiusDistribution(4.0), "mobius", 1e-2);
    fromDistributionCase(dotModular::LogitSigmoidDistribution(2.0, 0.0), "logit-sigmoid", 1e-2);
    fromDistributionCase(dotModular::TruncatedNormalDistribution(0.5, 0.2), "truncated-normal", 5e-3);
    // Degenerate far-off-support (a step CDF): still a valid monotone curve
    dotModular::TruncatedNormalDistribution far(-3.0, 0.1);
    MonotoneCurve cf = MonotoneCurve::fromDistribution(far, 9);
    check(invariantsHold(cf) && monotoneSweep(cf, 500),
          "fromDistribution: far-off-support step CDF stays valid");
    // n clamps into [MIN_POINTS, MAX_POINTS]
    MonotoneCurve cn = MonotoneCurve::fromDistribution(dotModular::PowerDistribution(2.0), 1);
    check(cn.size() == MonotoneCurve::MIN_POINTS, "fromDistribution: n clamps up to MIN_POINTS");
    MonotoneCurve cx = MonotoneCurve::fromDistribution(dotModular::PowerDistribution(2.0), 999);
    check(cx.size() == MonotoneCurve::MAX_POINTS, "fromDistribution: n clamps down to MAX_POINTS");
}

int main() {
    std::printf("== MonotoneCurve (.dmcurve Phase 1) ==\n");
    testIdentityCurve();
    testInterpolationAnchors();
    testMonotonicityNoOvershoot();
    testSetPointClamping();
    testAddPointBehaviour();
    testRemovePointBehaviour();
    testInverseRoundTrip();
    testInverseFlats();
    testPdfMatchesNumerical();
    testInverseCacheInvalidation();
    testJsonRoundTrip();
    testJsonSchemaFields();
    testJsonValidation();
    testJsonSanitize();
    testFromDistribution();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
