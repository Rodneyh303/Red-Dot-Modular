#pragma once
// ── dotModular::MonotoneCurve — the .dmcurve monotone CDF (Phase 1: pure math + serialization) ─
// The edited object of the .dmcurve editor (docs/design/DMCURVE_EDITOR.md): a monotone
// non-decreasing CDF on [0,1] with pinned endpoints (0,0)-(1,1), authored as 2..32 control
// points, evaluated with FRITSCH–CARLSON monotone cubic (PCHIP) interpolation — the standard
// construction that provably cannot overshoot (a plain cubic spline can, which would break
// the CDF invariant). The user draws the CDF F; the runtime transform uses F⁻¹; the density
// display uses the slope.
//
// DOUBLE precision throughout (matches the Distribution layer that wraps it —
// docs/design/DISTRIBUTION_LAYER.md): PchipDistribution is a cast-free double citizen, and
// evalInverse's Newton root-find wants double for clean convergence and a ~1e-12 round-trip
// (the same reason PhiInv is double). The float cast happens only at the engine's storage
// boundary, the existing double-compute/float-store pattern.
//
// PHASE 1 SCOPE (this header): pure math + JSON serialization. No Rack/nanovg/UI — the only
// non-stdlib dependency is jansson's json_t, included DIRECTLY (not via rack.hpp), so the
// unit tests exercise the real .dmcurve schema through the test/jansson.h shim exactly as
// the plugin tree uses Rack's bundled jansson. The widget (Phase 2) and the module
// integration / live preview (Phase 3) build on this.
//
// Invariants (every edit preserves them; fromJson sanitizes toward them):
//   • x strictly increasing (sorted), neighbours at least MIN_SPACING apart
//   • y monotone non-decreasing, all coordinates in [0,1]
//   • endpoints pinned: points.front() == (0,0), points.back() == (1,1)
//   • MIN_POINTS <= size <= MAX_POINTS
//
// F⁻¹: a cached reflection of the control points across the diagonal (x↔y swapped — still
// sorted/monotone; rebuilt only when the points change, via a dirty flag) provides a
// knot-exact, monotone SEED; a few safeguarded Newton iterations on the CDF then pin the
// result to the EXACT inverse of the drawn curve. Reflection alone is only an approximation
// — the inverse of a monotone cubic is not a cubic — and was measured drifting up to ~5%
// CDF-wise on skewed curves, which the Phase-3 live preview ("drag the curve, the bars
// redistribute") would visibly expose. Ties in y (flat CDF segments = zero-density gaps)
// would reflect to duplicate x, so each maximal run of near-equal y collapses to TWO knots:
// (y, x_first) — where F⁻¹(y) lands exactly on the gap's LEFT edge — and (y + nudge,
// x_last) — a near-vertical carry of the gap's extent that keeps the seed (and the Newton
// bracket) on the correct side of the gap.
//
// Serialization mirrors the .dmtune house pattern (tuning/TuningPreset.hpp): jansson json_t*,
// format + version guard on load, optional metadata tolerated when absent.

#include <jansson.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace dotModular {

class MonotoneCurve {
public:
    static constexpr int    MIN_POINTS  = 2;
    static constexpr int    MAX_POINTS  = 32;
    // Minimum horizontal spacing between control points (an EDITOR-scale quantum — sub-pixel
    // at panel resolution, and the dedupe threshold for hand-authored files). Edits and loads
    // keep neighbours at least this far apart: every PCHIP segment stays well-conditioned and
    // the reflected (inverse) point set remains a function.
    static constexpr double MIN_SPACING = 1e-6;
    // u-space width of the near-vertical knot pair a flat run contributes to the reflected
    // (inverse) curve. Small enough that a negligible fraction of quantiles land inside a
    // gap, large enough to be numerically robust.
    static constexpr double REFLECT_NUDGE = 1e-4;

    struct Point { double x = 0.0, y = 0.0; };

    // Optional .dmcurve library metadata (name/author/description). Never interpreted — the
    // editor carries them so a saved curve stays attributable / shareable.
    std::string name, author, description;

    MonotoneCurve() { reset(); }

    // Identity curve: just the two pinned endpoints (the uniform distribution).
    void reset() {
        points.clear();
        points.push_back(Point{0.0, 0.0});
        points.push_back(Point{1.0, 1.0});
        dirty_ = true;
    }

    // ── Accessors ────────────────────────────────────────────────────────────────────────────
    int   size() const { return (int)points.size(); }
    const std::vector<Point>& getPoints() const { return points; }   // sorted; invariants hold
    const Point& operator[](int i) const { return points[(size_t)i]; }

    // ── Core math ────────────────────────────────────────────────────────────────────────────
    // The CDF at x — PCHIP over the control points. x clamped to [0,1]; F(0)=0 and F(1)=1
    // exactly (pinned endpoints, exact Hermite evaluation at the knots).
    double evalCDF(double x) const {
        ensureCaches();
        x = clamp01(x);
        const int i = segmentFor(points.data(), (int)points.size(), x);
        return evalSegment(points.data(), cdfSlopes_.data(), i, x);
    }
    // The inverse CDF (quantile function) at u — the EXACT inverse of the drawn PCHIP curve:
    // the cached reflected-curve PCHIP provides the seed (knot-exact, monotone, carries the
    // gap edges), then safeguarded Newton iterations on evalCDF/pdf pin it to the true root
    // (bisection whenever a step leaves the bracket or the slope is flat; ~1e-13). u clamped
    // to [0,1]; F⁻¹(0)=0 and F⁻¹(1)=1 exactly.
    double evalInverse(double u) const {
        ensureCaches();
        if (u <= 0.0) return 0.0;   // pinned anchors (also the left-edge convention for a bottom flat)
        if (u >= 1.0) return 1.0;   // pinned anchors (right edge for a top flat)
        const int i = segmentFor(reflPts_.data(), (int)reflPts_.size(), u);
        double x = clamp01(evalSegment(reflPts_.data(), invSlopes_.data(), i, u));
        double lo = 0.0, hi = 1.0;
        for (int it = 0; it < 60; ++it) {
            const double fx = evalCDF(x) - u;
            if (fx == 0.0) return x;                          // exact hit (incl. knots + gap left-edges)
            if (fx < 0.0) lo = x; else hi = x;
            if (hi - lo < 1e-13) break;
            const double df = pdf(x);
            double nx = (df > 1e-15) ? (x - fx / df) : (0.5 * (lo + hi));
            if (!(nx > lo && nx < hi)) nx = 0.5 * (lo + hi);  // NaN / left the bracket -> bisect
            x = nx;
        }
        return 0.5 * (lo + hi);
    }
    // The density at x — the ANALYTIC slope of the same PCHIP segment evalCDF evaluates.
    // x clamped to [0,1]. Non-negative by construction (monotone cubic).
    double pdf(double x) const {
        ensureCaches();
        x = clamp01(x);
        const int i = segmentFor(points.data(), (int)points.size(), x);
        return evalSegmentSlope(points.data(), cdfSlopes_.data(), i, x);
    }

    // ── Editing (every op preserves the invariants) ──────────────────────────────────────────
    // Move interior point i. Values are CLAMPED into validity: y into [points[i-1].y,
    // points[i+1].y] and x into (points[i-1].x, points[i+1].x) with a MIN_SPACING margin on
    // each side (when the neighbours are closer than 2*MIN_SPACING the x stays put). THE
    // ENDPOINTS ARE IMMOVABLE — the pinned (0,0)/(1,1) anchor the CDF; setPoint on index 0,
    // the last index, or any out-of-range index is a no-op. NaN inputs are no-ops.
    void setPoint(int i, double x, double y) {
        if (i <= 0 || i >= (int)points.size() - 1) return;   // endpoints immovable / out of range
        if (!(x == x) || !(y == y)) return;                  // NaN guard
        const double ny = clamp(y, points[i - 1].y, points[i + 1].y);
        double nx = points[i].x;
        const double lo = points[i - 1].x + MIN_SPACING;
        const double hi = points[i + 1].x - MIN_SPACING;
        if (lo < hi) nx = clamp(x, lo, hi);
        points[i] = Point{nx, ny};
        dirty_ = true;
    }

    // Insert a point at x with y = evalCDF(x) — it lands ON the current curve (no
    // discontinuity; the shape is unchanged until the new point is dragged). Rejected
    // (no-op) when: the curve is at MAX_POINTS, x is not in the open interval (0,1) (0 and
    // 1 are the pinned endpoints' x), or x duplicates an existing point within MIN_SPACING.
    void addPoint(double x) {
        if ((int)points.size() >= MAX_POINTS) return;
        if (!(x == x)) return;                               // NaN guard
        if (!(x > 0.0 && x < 1.0)) return;
        for (const Point& p : points)
            if (std::fabs(x - p.x) < MIN_SPACING) return;
        const double y = evalCDF(x);                         // PRE-insert CDF: lands on the curve
        size_t at = 0;
        while (at < points.size() && points[at].x < x) ++at;
        points.insert(points.begin() + (std::ptrdiff_t)at, Point{x, y});
        dirty_ = true;
    }

    // Remove interior point i. NEVER removes an endpoint (index 0 / last); out-of-range
    // indices and MIN_POINTS curves (both indices are endpoints) are no-ops.
    void removePoint(int i) {
        if (i <= 0 || i >= (int)points.size() - 1) return;
        points.erase(points.begin() + i);
        dirty_ = true;
    }

    // ── Serialization — .dmcurve v1 schema (docs/design/DMCURVE_EDITOR.md) ──────────────────
    //   { "format":"dmcurve", "version":1, "type":"drawn", "interp":"pchip",
    //     "endpoints":"pinned", "points":[[x,y],...], "params":{},
    //     "name"/"author"/"description": optional }
    // Caller owns the returned reference (json_decref it).
    json_t* toJson() const {
        json_t* root = json_object();
        json_object_set_new(root, "format",    json_string("dmcurve"));
        json_object_set_new(root, "version",   json_integer(1));
        json_object_set_new(root, "type",      json_string("drawn"));
        json_object_set_new(root, "interp",    json_string("pchip"));
        json_object_set_new(root, "endpoints", json_string("pinned"));
        json_t* jp = json_array();
        for (const Point& p : points) {
            json_t* pair = json_array();
            json_array_append_new(pair, json_real(p.x));
            json_array_append_new(pair, json_real(p.y));
            json_array_append_new(jp, pair);
        }
        json_object_set_new(root, "points", jp);
        json_object_set_new(root, "params", json_object());  // empty for "drawn"
        if (!name.empty())        json_object_set_new(root, "name",        json_string(name.c_str()));
        if (!author.empty())      json_object_set_new(root, "author",      json_string(author.c_str()));
        if (!description.empty()) json_object_set_new(root, "description", json_string(description.c_str()));
        return root;
    }

    // Parse the v1 schema (BORROWS rootJ — the caller keeps ownership). Returns false (curve
    // UNTOUCHED) for structural failures: wrong format tag, version != 1, missing / short /
    // overlong / malformed points, or a type/interp/endpoints value v1 cannot represent
    // (drawn/pchip/pinned only; absent fields take those defaults). Out-of-shape points are
    // SANITIZED, not rejected — the defined, tested behaviour: coordinates clamped into
    // [0,1], points sorted by x, near-duplicate x dropped (first wins), the pinned corner
    // anchors (0,0)/(1,1) ensured (inserted when missing; a file whose corner insertion
    // would exceed MAX_POINTS is rejected), y lifted to a running maximum. Optional metadata
    // tolerated when absent.
    bool fromJson(json_t* rootJ) {
        if (!json_is_object(rootJ)) return false;
        json_t* jfmt = json_object_get(rootJ, "format");
        if (!json_is_string(jfmt) || std::string(json_string_value(jfmt)) != "dmcurve") return false;
        json_t* jver = json_object_get(rootJ, "version");
        if (jver && !json_is_integer(jver)) return false;            // present but not an integer
        if (jver && (int)json_integer_value(jver) != 1) return false;
        // v1 supports exactly drawn/pchip/pinned; absent fields default (minimal objects load)
        const char* const fixed[][2] = {
            {"type", "drawn"}, {"interp", "pchip"}, {"endpoints", "pinned"},
        };
        for (const auto& kv : fixed) {
            json_t* j = json_object_get(rootJ, kv[0]);
            if (j && (!json_is_string(j) || std::string(json_string_value(j)) != kv[1])) return false;
        }
        json_t* jp = json_object_get(rootJ, "points");
        if (!json_is_array(jp)) return false;
        const size_t n = json_array_size(jp);
        if (n < (size_t)MIN_POINTS || n > (size_t)MAX_POINTS) return false;

        std::vector<Point> loaded;
        loaded.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            json_t* pair = json_array_get(jp, i);
            if (!json_is_array(pair) || json_array_size(pair) != 2) return false;
            json_t* jx = json_array_get(pair, 0);
            json_t* jy = json_array_get(pair, 1);
            if (!json_is_number(jx) || !json_is_number(jy)) return false;
            loaded.push_back(Point{clamp01(json_number_value(jx)),
                                   clamp01(json_number_value(jy))});
        }
        // Sanitize: sort, dedupe, ensure corner anchors, running-max (see the method comment).
        std::stable_sort(loaded.begin(), loaded.end(),
                         [](const Point& a, const Point& b) { return a.x < b.x; });
        std::vector<Point> kept;
        kept.reserve(loaded.size());
        for (const Point& p : loaded) {
            if (!kept.empty() && p.x < kept.back().x + MIN_SPACING) continue;   // duplicate x: first wins
            kept.push_back(p);
        }
        if ((int)kept.size() < MIN_POINTS) return false;                        // everything collapsed
        // Ensure the pinned corners: INSERT the anchors when the extremes aren't already at
        // them (overwriting would silently eat the file's shape); overwrite in place when a
        // point already sits at the corner with its y off it.
        if (kept.front().x > MIN_SPACING) kept.insert(kept.begin(), Point{0.0, 0.0});
        else                              kept.front()  = Point{0.0, 0.0};
        if (kept.back().x < 1.0 - MIN_SPACING) kept.push_back(Point{1.0, 1.0});
        else                                    kept.back()  = Point{1.0, 1.0};
        if ((int)kept.size() > MAX_POINTS) return false;   // inserted corners pushed it over the cap
        double yMax = 0.0;                                                      // lift y monotone
        for (Point& p : kept) { if (p.y > yMax) yMax = p.y; p.y = yMax; }

        points = std::move(kept);                       // commit (state was untouched so far)
        name.clear();
        author.clear();
        description.clear();
        if (json_t* j = json_object_get(rootJ, "name");        json_is_string(j)) name        = json_string_value(j);
        if (json_t* j = json_object_get(rootJ, "author");      json_is_string(j)) author      = json_string_value(j);
        if (json_t* j = json_object_get(rootJ, "description"); json_is_string(j)) description = json_string_value(j);
        dirty_ = true;
        return true;
    }

private:
    std::vector<Point> points;               // the CDF's control points (source of truth)
    // PCHIP caches — rebuilt lazily on the first read after any edit (dirty flag); pure
    // reads afterwards (safe to call evalCDF/evalInverse/pdf from the audio thread).
    mutable std::vector<double> cdfSlopes_;  // Fritsch–Carlson derivatives of the CDF
    mutable std::vector<Point>  reflPts_;    // reflected control points (the F⁻¹ knots)
    mutable std::vector<double> invSlopes_;  // Fritsch–Carlson derivatives of the reflection
    mutable bool dirty_ = true;

    static double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static double clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // ── Fritsch–Carlson monotone cubic (PCHIP) ──────────────────────────────────────────────
    // Derivatives d[i] at every knot. Interior knots where the adjoining secant slopes
    // disagree in sign (or either is flat) get 0; otherwise the weighted harmonic mean
    // (w1 = 2h[i] + h[i-1], w2 = h[i] + 2h[i-1]) — which lies between the neighbouring
    // secants and keeps the cubic inside Fritsch–Carlson's monotone region: no overshoot.
    // Endpoints use the one-sided three-point estimate, zeroed on sign mismatch and limited
    // to 3x the adjoining secant (the MATLAB pchip / scipy PchipInterpolator construction).
    static void fritschCarlson(const std::vector<Point>& pts, std::vector<double>& d) {
        const int n = (int)pts.size();
        d.assign((size_t)(n < 1 ? 1 : n), 0.0);
        if (n < 2) return;
        if (n == 2) {   // single segment: both derivatives are the secant (a straight line)
            d[0] = d[1] = (pts[1].y - pts[0].y) / (pts[1].x - pts[0].x);
            return;
        }
        std::vector<double> h((size_t)n - 1), delta((size_t)n - 1);
        for (int i = 0; i < n - 1; ++i) {
            h[(size_t)i]     = pts[i + 1].x - pts[i].x;                  // > MIN_SPACING (invariant)
            delta[(size_t)i] = (pts[i + 1].y - pts[i].y) / h[(size_t)i]; // >= 0 (monotone y)
        }
        for (int i = 1; i < n - 1; ++i) {
            const double dl = delta[(size_t)i - 1], dr = delta[(size_t)i];
            if (dl * dr <= 0.0) { d[(size_t)i] = 0.0; continue; }        // sign change / flat: stay flat
            const double w1 = 2.0 * h[(size_t)i] + h[(size_t)i - 1];
            const double w2 = h[(size_t)i] + 2.0 * h[(size_t)i - 1];
            d[(size_t)i] = (w1 + w2) / (w1 / dl + w2 / dr);
        }
        d[0] = ((2.0 * h[0] + h[1]) * delta[0] - h[0] * delta[1]) / (h[0] + h[1]);
        if (d[0] * delta[0] <= 0.0) d[0] = 0.0;
        else if (delta[0] * delta[1] <= 0.0 && std::fabs(d[0]) > 3.0 * delta[0]) d[0] = 3.0 * delta[0];
        const int L = n - 1;
        d[(size_t)L] = ((2.0 * h[(size_t)L - 1] + h[(size_t)L - 2]) * delta[(size_t)L - 1]
                        - h[(size_t)L - 1] * delta[(size_t)L - 2]) / (h[(size_t)L - 1] + h[(size_t)L - 2]);
        if (d[(size_t)L] * delta[(size_t)L - 1] <= 0.0) d[(size_t)L] = 0.0;
        else if (delta[(size_t)L - 1] * delta[(size_t)L - 2] <= 0.0
                 && std::fabs(d[(size_t)L]) > 3.0 * delta[(size_t)L - 1])
            d[(size_t)L] = 3.0 * delta[(size_t)L - 1];
    }

    // Cubic Hermite evaluation on segment i at x (t = (x - x_i)/h in [0,1]). Exact at the
    // knots: t=0 returns y_i, t=1 returns y_{i+1}.
    static double evalSegment(const Point* pts, const double* d, int i, double x) {
        const double h  = pts[i + 1].x - pts[i].x;
        const double t  = (x - pts[i].x) / h;
        const double t2 = t * t, t3 = t2 * t;
        return pts[i].y     * (2.0 * t3 - 3.0 * t2 + 1.0)
             + pts[i + 1].y * (-2.0 * t3 + 3.0 * t2)
             + h * d[i]     * (t3 - 2.0 * t2 + t)
             + h * d[i + 1] * (t3 - t2);
    }
    // Analytic d/dx of evalSegment (the density). Exact at the knots: d[i] and d[i+1].
    static double evalSegmentSlope(const Point* pts, const double* d, int i, double x) {
        const double h  = pts[i + 1].x - pts[i].x;
        const double t  = (x - pts[i].x) / h;
        const double t2 = t * t;
        const double dH = pts[i].y     * (6.0 * t2 - 6.0 * t)
                       + pts[i + 1].y * (-6.0 * t2 + 6.0 * t)
                       + h * d[i]     * (3.0 * t2 - 4.0 * t + 1.0)
                       + h * d[i + 1] * (3.0 * t2 - 2.0 * t);
        return dH / h;
    }
    // Largest i in [0, n-2] with pts[i].x <= x (binary search over the sorted knots; x
    // assumed clamped to [pts[0].x, pts[n-1].x]).
    static int segmentFor(const Point* pts, int n, double x) {
        int lo = 0, hi = n - 2;
        while (lo < hi) {
            const int mid = (lo + hi + 1) >> 1;
            if (pts[mid].x <= x) lo = mid; else hi = mid - 1;
        }
        return lo;
    }

    // Rebuild both PCHIP caches if the points changed since the last read.
    void ensureCaches() const {
        if (!dirty_) return;
        fritschCarlson(points, cdfSlopes_);
        // F⁻¹ knots: reflect (x,y) -> (y,x). Ties in y (flat CDF segments = zero-density
        // gaps) would reflect to duplicate x, so each maximal run of near-equal y collapses
        // to TWO knots: (y, x_first) — the gap's left edge, where F⁻¹(y) lands — and
        // (y + nudge, x_last) — the near-vertical carry of the gap's extent, so quantiles
        // just above the gap land just right of it, not inside it. The nudge never overruns
        // the next run (half-way clamp) and is dropped outright if float spacing degenerates.
        // The first/last runs naturally anchor the pinned (0,0)/(1,1) (forced below as a
        // guard, which also repairs a fully-collapsed top run).
        reflPts_.clear();
        const int n = (int)points.size();
        int i = 0;
        while (i < n) {
            int j = i;
            while (j + 1 < n && points[j + 1].y <= points[i].y + MIN_SPACING) ++j;  // maximal run [i..j]
            const double u0 = points[i].y;
            reflPts_.push_back(Point{u0, points[i].x});
            if (j > i) {                                   // multi-point run: a gap — carry its far edge
                double u1;
                if (j + 1 < n) {
                    const double uNext = points[j + 1].y;   // next run's height (strictly above u0)
                    u1 = u0 + REFLECT_NUDGE;
                    const double mid = 0.5 * (u0 + uNext);
                    if (u1 > mid) u1 = mid;                 // never overrun the next run
                } else {
                    u1 = 1.0;                               // the run owns the pinned top (1,1)
                }
                const bool fits = u1 > u0                                   // strictly past the left edge...
                                  && (j + 1 >= n || u1 < points[j + 1].y);  // ...and strictly before the next
                if (fits) reflPts_.push_back(Point{u1, points[j].x});
                // (degenerate spacing: the gap collapses to its left edge — still valid)
            }
            i = j + 1;
        }
        if (!reflPts_.empty()) {
            reflPts_.front() = Point{0.0, 0.0};             // Q(0) == 0 exactly (always true; guarded)
            reflPts_.back()  = Point{1.0, 1.0};             // Q(1) == 1 exactly (repairs a collapsed top)
        }
        fritschCarlson(reflPts_, invSlopes_);
        dirty_ = false;
    }
};

} // namespace dotModular
