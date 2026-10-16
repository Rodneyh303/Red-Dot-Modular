// test_distribution.cpp — dotModular::Distribution — the probability core (Phase 1)
// Compile: see test/run_all.sh (header-only, no companion sources).
//
// The Distribution layer (docs/design/DISTRIBUTION_LAYER.md): the general probability object
// (cdf/pdf/invCDF on [0,1]) with closed-form parametric types, the arcsine/truncated-normal
// palette additions, and PchipDistribution wrapping MonotoneCurve (the drawn case). All
// DOUBLE — matching the copula Phi/PhiInv math it calls into; tests compare in double.
//
// Sections:
//   1. Contract battery (the doc's battery, run against EVERY distribution instance —
//      6 parametric types across many parameterizations + PchipDistribution over several
//      curves): round-trips both directions (~1e-9; the doc's ~1e-4 with huge margin),
//      pdf == central-difference d/dx cdf, cdf(0)=0 / cdf(1)=1 (incl. logit-normal's
//      open-support limits), cdf AND invCDF monotone over fine sweeps (the overshoot guard),
//      outputs within [0,1], input clamping, pdf non-negative and finite.
//   2. Degenerate-to-known (catches formula typos cheaply): Kumaraswamy(1,1)=uniform,
//      Power(1)=uniform, triangular at known mode points, logit-normal median=logistic(mu),
//      arcsine self-dual at 0.5, symmetric truncated-normal centre, and the far-off-support
//      truncated-normal point-mass degenerates.
//   3. Edge behaviour: truncated-normal has NON-ZERO edge density (the clipped bell — the
//      doc's distinction vs logit-normal's zero edges).
//   4. PchipDistribution: pure delegation to MonotoneCurve (same values), the contract
//      battery over drawn curves (knot-avoiding pdf sweep), and the contract SURVIVING
//      edits made through .curve (the editor path).
//   5. JSON dispatch: toJson -> loadDistribution round-trip per type (type preserved,
//      second generation byte-identical, JSON TEXT round-trip), params survive exactly,
//      mode clamped on load, rejection matrix (format/version/unknown type/missing,
//      non-numeric, non-positive params; drawn delegates to MonotoneCurve's validation),
//      and dispatch separation (a parametric object is not a MonotoneCurve and vice versa).
//
// jansson: resolved to the test/jansson.h shim via -Itest (the test/rack.hpp precedent) —
// same as test_monotone_curve.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Distribution.hpp"
#include "MonotoneCurve.hpp"

using dotModular::Distribution;
using dotModular::TriangularDistribution;
using dotModular::PowerDistribution;
using dotModular::KumaraswamyDistribution;
using dotModular::LogitNormalDistribution;
using dotModular::ArcsineDistribution;
using dotModular::TruncatedNormalDistribution;
using dotModular::PchipDistribution;
using dotModular::MonotoneCurve;
using dotModular::loadDistribution;

static int g_pass = 0, g_fail = 0;

static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static void checkNear(double got, double want, double tol, const char* what) {
    if (std::fabs(got - want) <= tol) { ++g_pass; return; }
    std::printf("  FAIL: %s (got %.12g, want %.12g, tol %g)\n", what, got, want, tol);
    ++g_fail;
}

// ── helpers ────────────────────────────────────────────────────────────────────────────────────
// Build a MonotoneCurve from an explicit valid point list via the public JSON path (arbitrary
// placement is only reachable through a .dmcurve-shaped load — the same entry the editor files use).
static MonotoneCurve makeCurve(std::initializer_list<std::pair<double, double>> pts) {
    json_t* root = json_object();
    json_object_set_new(root, "format", json_string("dmcurve"));
    json_object_set_new(root, "version", json_integer(1));
    json_t* jp = json_array();
    for (const auto& p : pts) {
        json_t* pair = json_array();
        json_array_append_new(pair, json_real(p.first));
        json_array_append_new(pair, json_real(p.second));
        json_array_append_new(jp, pair);
    }
    json_object_set_new(root, "points", jp);
    MonotoneCurve c;
    check(c.fromJson(root), "makeCurve: valid curve loads");
    json_decref(root);
    return c;
}
static std::vector<double> knotsOf(const MonotoneCurve& c) {
    std::vector<double> k;
    for (const auto& p : c.getPoints()) k.push_back(p.x);
    return k;
}

// ── 1. the contract battery ────────────────────────────────────────────────────────────────────
struct Tol {
    double rt  = 1e-8;   // round-trip tolerance (spec says ~1e-4; double closed forms hit ~1e-15,
                         // PhiInv's edge accuracy sets the practical floor at ~1e-9)
    double pdf = 1e-6;   // pdf vs central difference
    double h   = 1e-5;   // central-difference step
};

static void contractBattery(const Distribution& d, const char* label,
                            const std::vector<double>& pdfAvoid = {},
                            const Tol& tol = Tol{}) {
    std::printf("  battery: %s\n", label);
    // endpoints + open-support limits
    checkNear(d.cdf(0.0), 0.0, 1e-9, "cdf(0) == 0");
    checkNear(d.cdf(1.0), 1.0, 1e-9, "cdf(1) == 1");
    checkNear(d.invCDF(0.0), 0.0, 1e-9, "invCDF(0) == 0");
    checkNear(d.invCDF(1.0), 1.0, 1e-9, "invCDF(1) == 1");
    // input clamping
    check(d.cdf(-0.5) == d.cdf(0.0) && d.cdf(1.5) == d.cdf(1.0), "cdf clamps its input");
    check(d.invCDF(-1.0) == 0.0 && d.invCDF(2.0) == 1.0, "invCDF clamps its input");
    check(d.pdf(-1.0) == d.pdf(0.0) && d.pdf(2.0) == d.pdf(1.0), "pdf clamps its input");
    // round trips, both directions. The FORWARD direction is asserted only where the
    // inversion is well-posed: the engine's PhiInv clamps u to its 1e-12 copula domain, and
    // its ~1e-15 p-space accuracy amplifies to a z-error of ~1e-15/phi(z) in the tail — so
    // for x whose CDF value is outside [1e-9, 1-1e-9] (effectively zero mass), F⁻¹(F(x))
    // correctly returns the edge of the effective support, not x (visible on skewed
    // logit-normal / tight truncated-normal tails). The BACK direction sweeps u directly
    // and is well-posed everywhere.
    double maxFwd = 0.0, maxBack = 0.0;
    int fwdChecked = 0;
    for (int k = 0; k <= 400; ++k) {
        const double x = 1e-3 + 0.998 * (double)k / 400.0;
        const double fx = d.cdf(x);
        if (fx > 1e-9 && fx < 1.0 - 1e-9) {
            const double fwd = std::fabs(d.invCDF(fx) - x);
            if (fwd > maxFwd) maxFwd = fwd;
            ++fwdChecked;
        }
        const double u = 1e-3 + 0.998 * (double)k / 400.0;
        const double back = std::fabs(d.cdf(d.invCDF(u)) - u);
        if (back > maxBack) maxBack = back;
    }
    checkNear(maxFwd, 0.0, tol.rt, "invCDF(cdf(x)) == x round-trip (well-posed region)");
    checkNear(maxBack, 0.0, tol.rt, "cdf(invCDF(u)) == u round-trip");
    check(fwdChecked > 100, "forward round-trip sweep exercised enough points");
    // pdf == numerical derivative (avoiding listed knots: the derivative is discontinuous
    // AT Pchip control points and a central difference across a kink mixes both sides)
    int checked = 0;
    for (double x = 0.01; x <= 0.99; x += 0.0037) {
        bool skip = false;
        for (double a : pdfAvoid)
            if (std::fabs(x - a) < 3.0 * tol.h) { skip = true; break; }
        if (skip) continue;
        const double num = (d.cdf(x + tol.h) - d.cdf(x - tol.h)) / (2.0 * tol.h);
        checkNear(d.pdf(x), num, tol.pdf, "pdf == numerical derivative of cdf");
        check(d.pdf(x) >= -1e-12, "pdf non-negative");
        check(d.pdf(x) < 1e6, "pdf finite (bounded)");
        ++checked;
    }
    check(checked > 100, "pdf sweep exercised interior points");
    // monotone sweeps + output range (the no-overshoot guard — critical for Pchip)
    double prev = d.cdf(0.0), prevInv = d.invCDF(0.0);
    bool mono = true, monoInv = true, inRange = true;
    for (int k = 1; k <= 2000; ++k) {
        const double x = (double)k / 2000.0;
        const double v = d.cdf(x), q = d.invCDF(x);
        if (v < prev - 1e-12) mono = false;
        if (q < prevInv - 1e-12) monoInv = false;
        if (v < -1e-9 || v > 1.0 + 1e-9 || q < -1e-9 || q > 1.0 + 1e-9) inRange = false;
        prev = v;
        prevInv = q;
    }
    check(mono, "cdf monotone non-decreasing over a fine sweep");
    check(monoInv, "invCDF monotone non-decreasing over a fine sweep");
    check(inRange, "cdf/invCDF outputs stay inside [0,1]");
}

// The instance list — every distribution the layer ships, across parameterizations.
struct Instance {
    std::unique_ptr<Distribution> d;
    const char* label;
    std::vector<double> avoid;   // pdf-sweep knots to keep clear of (Pchip control points)
};
static std::vector<Instance> allInstances() {
    std::vector<Instance> v;
    auto add = [&](Distribution* d, const char* label, std::vector<double> avoid = {}) {
        v.push_back(Instance{std::unique_ptr<Distribution>(d), label, std::move(avoid)});
    };
    add(new TriangularDistribution(0.2), "triangular mode=0.2");
    add(new TriangularDistribution(0.5), "triangular mode=0.5");
    add(new TriangularDistribution(0.8), "triangular mode=0.8");
    add(new TriangularDistribution(0.0), "triangular mode=0 (right ramp)");
    add(new TriangularDistribution(1.0), "triangular mode=1 (left ramp)");
    add(new PowerDistribution(0.5), "power k=0.5");
    add(new PowerDistribution(1.0), "power k=1");
    add(new PowerDistribution(2.0), "power k=2");
    add(new PowerDistribution(3.7), "power k=3.7");
    add(new KumaraswamyDistribution(1.0, 1.0), "kumaraswamy (1,1)");
    add(new KumaraswamyDistribution(2.0, 3.0), "kumaraswamy (2,3)");
    add(new KumaraswamyDistribution(0.5, 0.5), "kumaraswamy (0.5,0.5) bathtub");
    add(new KumaraswamyDistribution(3.0, 0.5), "kumaraswamy (3,0.5)");
    add(new KumaraswamyDistribution(2.0, 0.8), "kumaraswamy (2,0.8)");
    add(new LogitNormalDistribution(0.0, 1.0), "logit-normal (0,1)");
    add(new LogitNormalDistribution(1.0, 0.8), "logit-normal (1,0.8)");
    add(new LogitNormalDistribution(-1.0, 0.5), "logit-normal (-1,0.5)");
    add(new LogitNormalDistribution(0.5, 0.25), "logit-normal (0.5,0.25)");
    add(new ArcsineDistribution(), "arcsine (bathtub/extremes)");
    add(new TruncatedNormalDistribution(0.5, 0.2), "truncated-normal (0.5,0.2)");
    add(new TruncatedNormalDistribution(0.25, 0.12), "truncated-normal (0.25,0.12)");
    add(new TruncatedNormalDistribution(0.85, 0.15), "truncated-normal (0.85,0.15)");
    add(new TruncatedNormalDistribution(0.5, 0.05), "truncated-normal (0.5,0.05) tight");
    add(new PchipDistribution(), "pchip identity");
    add(new PchipDistribution(makeCurve({{0.0, 0.0}, {0.2, 0.05}, {0.5, 0.5}, {0.8, 0.95}, {1.0, 1.0}})),
        "pchip S", knotsOf(makeCurve({{0.0, 0.0}, {0.2, 0.05}, {0.5, 0.5}, {0.8, 0.95}, {1.0, 1.0}})));
    add(new PchipDistribution(makeCurve({{0.0, 0.0}, {0.3, 0.08}, {0.6, 0.3}, {1.0, 1.0}})),
        "pchip concave", knotsOf(makeCurve({{0.0, 0.0}, {0.3, 0.08}, {0.6, 0.3}, {1.0, 1.0}})));
    add(new PchipDistribution(makeCurve({{0.0, 0.0}, {0.4, 0.7}, {0.7, 0.92}, {1.0, 1.0}})),
        "pchip convex", knotsOf(makeCurve({{0.0, 0.0}, {0.4, 0.7}, {0.7, 0.92}, {1.0, 1.0}})));
    add(new PchipDistribution(makeCurve({{0.0, 0.0}, {0.5, 0.48}, {1.0, 1.0}})),
        "pchip gentle", knotsOf(makeCurve({{0.0, 0.0}, {0.5, 0.48}, {1.0, 1.0}})));
    return v;
}

static void testContractBattery() {
    std::printf("contract battery (every distribution)\n");
    for (const Instance& inst : allInstances())
        contractBattery(*inst.d, inst.label, inst.avoid);
}

// ── 2. degenerate-to-known ─────────────────────────────────────────────────────────────────────
static void testDegenerateToKnown() {
    std::printf("degenerate-to-known\n");
    // Kumaraswamy(1,1) == uniform
    {
        KumaraswamyDistribution k(1.0, 1.0);
        for (int i = 1; i < 20; ++i) {
            const double x = (double)i / 20.0;
            checkNear(k.cdf(x), x, 1e-12, "Kumaraswamy(1,1): cdf(x) == x (uniform)");
            checkNear(k.invCDF(x), x, 1e-12, "Kumaraswamy(1,1): invCDF(u) == u");
            checkNear(k.pdf(x), 1.0, 1e-12, "Kumaraswamy(1,1): pdf == 1");
        }
    }
    // Power(1) == uniform
    {
        PowerDistribution p(1.0);
        for (int i = 1; i < 20; ++i) {
            const double x = (double)i / 20.0;
            checkNear(p.cdf(x), x, 1e-12, "Power(1): cdf(x) == x (uniform)");
            checkNear(p.invCDF(x), x, 1e-12, "Power(1): invCDF(u) == u");
            checkNear(p.pdf(x), 1.0, 1e-12, "Power(1): pdf == 1");
        }
    }
    // Triangular at known mode points: cdf(mode)=mode, pdf(mode)=2 (the peak), inv(mode)=mode
    for (double c : {0.2, 0.5, 0.8}) {
        TriangularDistribution t(c);
        checkNear(t.cdf(c), c, 1e-12, "triangular: cdf(mode) == mode");
        checkNear(t.pdf(c), 2.0, 1e-12, "triangular: pdf(mode) == 2 (peak)");
        checkNear(t.invCDF(c), c, 1e-12, "triangular: invCDF(mode) == mode");
    }
    // Triangular ramps: mode=0 is the falling ramp 1-(1-x)^2; mode=1 the rising ramp x^2
    {
        TriangularDistribution r0(0.0), r1(1.0);
        checkNear(r0.cdf(0.5), 0.75, 1e-12, "triangular mode=0: cdf(0.5) == 0.75 (right ramp)");
        checkNear(r0.pdf(0.0), 2.0, 1e-12, "triangular mode=0: pdf(0) == 2");
        checkNear(r1.cdf(0.5), 0.25, 1e-12, "triangular mode=1: cdf(0.5) == 0.25 (left ramp)");
        checkNear(r1.pdf(1.0), 2.0, 1e-12, "triangular mode=1: pdf(1) == 2");
    }
    // LogitNormal median == logistic(mu)
    for (double mu : {-1.0, 0.0, 0.5, 1.0}) {
        LogitNormalDistribution l(mu, 0.8);
        checkNear(l.invCDF(0.5), 1.0 / (1.0 + std::exp(-mu)), 1e-9,
                  "logit-normal: invCDF(0.5) == logistic(mu) (median)");
    }
    // Arcsine is self-dual at the centre
    {
        ArcsineDistribution a;
        checkNear(a.cdf(0.5), 0.5, 1e-12, "arcsine: cdf(0.5) == 0.5");
        checkNear(a.invCDF(0.5), 0.5, 1e-12, "arcsine: invCDF(0.5) == 0.5");
        checkNear(a.pdf(0.5), 2.0 / 3.14159265358979323846, 1e-12, "arcsine: pdf(0.5) == 2/pi");
    }
    // Symmetric truncated-normal centres at 0.5
    for (double s : {0.05, 0.2, 0.5}) {
        TruncatedNormalDistribution t(0.5, s);
        checkNear(t.cdf(0.5), 0.5, 1e-9, "truncated-normal: symmetric centre cdf(0.5) == 0.5");
    }
    // Far-off-support truncated-normal degenerates to edge point masses (documented limit:
    // the window is many sigmas off [0,1], so the clipped mass sits at the nearest edge)
    {
        TruncatedNormalDistribution left(-3.0, 0.1);
        check(left.cdf(0.25) == 1.0, "far-left truncated-normal: all mass at 0 (cdf == 1 on (0,1])");
        check(left.invCDF(0.5) == 0.0, "far-left: invCDF -> 0");
        check(left.pdf(0.5) == 0.0, "far-left: pdf == 0");
        check(std::isfinite(left.cdf(0.3)) && std::isfinite(left.pdf(0.3)) && std::isfinite(left.invCDF(0.3)),
              "far-left: no NaN/inf anywhere");
        TruncatedNormalDistribution right(4.0, 0.1);
        checkNear(right.cdf(0.75), 0.0, 1e-9, "far-right truncated-normal: cdf == 0 on [0,1)");
        check(right.invCDF(0.5) == 1.0, "far-right: invCDF -> 1");
        checkNear(right.pdf(0.5), 0.0, 1e-9, "far-right: pdf == 0");
        check(std::isfinite(right.cdf(0.3)) && std::isfinite(right.pdf(0.3)) && std::isfinite(right.invCDF(0.3)),
              "far-right: no NaN/inf anywhere");
    }
}

// ── 3. edge behaviour ──────────────────────────────────────────────────────────────────────────
static void testEdgeBehaviour() {
    std::printf("edge behaviour (the closest-pair distinction)\n");
    // Truncated-normal: NON-ZERO density at the edges (clipped bell — can pile mass at an edge)
    TruncatedNormalDistribution tn(0.5, 0.2);
    check(tn.pdf(0.0) > 0.01 && tn.pdf(1.0) > 0.01,
          "truncated-normal: non-zero edge density (clipped, not squashed)");
    checkNear(tn.pdf(0.0), tn.pdf(1.0), 1e-12, "truncated-normal: symmetric edges");
    // Logit-normal: density VANISHES at the edges (squashed bell, open support)
    LogitNormalDistribution ln(0.0, 1.0);
    check(ln.pdf(0.0) == 0.0 && ln.pdf(1.0) == 0.0, "logit-normal: zero edge density");
    // Divergent-edge pdfs stay finite (the DIST_EDGE_EPS proxy): Power k<1 at 0, arcsine both edges
    PowerDistribution p(0.5);
    check(std::isfinite(p.pdf(0.0)) && p.pdf(0.0) > 1.0, "power k<1: finite (large) pdf at 0");
    ArcsineDistribution a;
    check(std::isfinite(a.pdf(0.0)) && std::isfinite(a.pdf(1.0)) && a.pdf(0.0) > 1.0,
          "arcsine: finite (large) pdf at both edges");
    KumaraswamyDistribution kb(0.5, 0.5);
    check(std::isfinite(kb.pdf(0.0)) && std::isfinite(kb.pdf(1.0)),
          "kumaraswamy (a,b)<1: finite pdf at both edges");
}

// ── 4. PchipDistribution ───────────────────────────────────────────────────────────────────────
static void testPchipWrapper() {
    std::printf("PchipDistribution wraps MonotoneCurve\n");
    // Identity default
    PchipDistribution p;
    checkNear(p.cdf(0.37), 0.37, 1e-12, "pchip identity: cdf(x) == x");
    checkNear(p.invCDF(0.37), 0.37, 1e-12, "pchip identity: invCDF(u) == u");
    checkNear(p.pdf(0.37), 1.0, 1e-9, "pchip identity: pdf == 1");
    // Pure delegation: the Distribution methods return the curve's own values exactly
    MonotoneCurve c = makeCurve({{0.0, 0.0}, {0.2, 0.05}, {0.5, 0.5}, {0.8, 0.95}, {1.0, 1.0}});
    PchipDistribution q(c);
    check(q.cdf(0.33) == c.evalCDF(0.33), "cdf delegates to curve.evalCDF (identical values)");
    check(q.invCDF(0.33) == c.evalInverse(0.33), "invCDF delegates to curve.evalInverse");
    check(q.pdf(0.33) == c.pdf(0.33), "pdf delegates to curve.pdf");
    // The contract SURVIVES edits made through .curve (the editor path)
    q.curve.setPoint(1, 0.22, 0.07);
    q.curve.addPoint(0.65);
    q.curve.removePoint(2);
    contractBattery(q, "pchip after edits", knotsOf(q.curve));
}

// ── 5. JSON dispatch ───────────────────────────────────────────────────────────────────────────
static json_t* paramsObj(std::initializer_list<std::pair<const char*, double>> kv) {
    json_t* p = json_object();
    for (const auto& e : kv) json_object_set_new(p, e.first, json_real(e.second));
    return p;
}
// .dmcurve envelope around a parametric type (params ownership taken; nullptr -> empty object)
static json_t* envelope(const char* type, json_t* params) {
    json_t* root = json_object();
    json_object_set_new(root, "format", json_string("dmcurve"));
    json_object_set_new(root, "version", json_integer(1));
    json_object_set_new(root, "type", json_string(type));
    json_object_set_new(root, "params", params ? params : json_object());
    return root;
}
// dumps(toJson(d)) — caller free()s
static char* dumpText(const Distribution& d) {
    json_t* j = d.toJson();
    char* t = json_dumps(j, JSON_INDENT(2));
    json_decref(j);
    return t;
}

static void testJsonRoundTripAll() {
    std::printf("json: round-trip every type\n");
    for (const Instance& inst : allInstances()) {
        json_t* j = inst.d->toJson();
        check(j != nullptr, "toJson produced an object");
        std::unique_ptr<Distribution> loaded = loadDistribution(j);
        check(loaded != nullptr, "loadDistribution accepted our own output");
        check(loaded && std::string(loaded->typeName()) == std::string(inst.d->typeName()),
              "type preserved through the round-trip");
        // second generation byte-identical
        char* t1 = dumpText(*inst.d);
        char* t2 = loaded ? dumpText(*loaded) : nullptr;
        check(t1 && t2 && std::strcmp(t1, t2) == 0, "second-generation JSON byte-identical");
        // TEXT round-trip: the emitted schema is genuinely parseable JSON
        json_error_t err;
        json_t* j2 = t1 ? json_loads(t1, 0, &err) : nullptr;
        check(j2 != nullptr, "emitted schema parses as JSON text");
        std::unique_ptr<Distribution> loaded2 = j2 ? loadDistribution(j2) : nullptr;
        check(loaded2 != nullptr, "factory accepts the re-parsed text");
        char* t3 = loaded2 ? dumpText(*loaded2) : nullptr;
        check(t1 && t3 && std::strcmp(t1, t3) == 0, "text round-trip byte-identical");
        // the loaded instance still honours the full contract
        if (loaded) contractBattery(*loaded, inst.label, inst.avoid);
        free(t1);
        free(t2);
        free(t3);
        json_decref(j);
        if (j2) json_decref(j2);
    }
}

static void testJsonParamSurvival() {
    std::printf("json: params survive / mode clamped\n");
    // triangular
    json_t* j = envelope("triangular", paramsObj({{"mode", 0.3}}));
    std::unique_ptr<Distribution> d = loadDistribution(j);
    json_decref(j);
    check(d && std::string(d->typeName()) == "triangular", "triangular loads");
    checkNear(static_cast<TriangularDistribution&>(*d).mode, 0.3, 0.0, "mode survives exactly");
    j = envelope("triangular", paramsObj({{"mode", 1.5}}));
    d = loadDistribution(j);
    json_decref(j);
    checkNear(static_cast<TriangularDistribution&>(*d).mode, 1.0, 0.0, "mode clamped down to 1");
    j = envelope("triangular", paramsObj({{"mode", -0.2}}));
    d = loadDistribution(j);
    json_decref(j);
    checkNear(static_cast<TriangularDistribution&>(*d).mode, 0.0, 0.0, "mode clamped up to 0");
    // power / kumaraswamy / logit-normal / truncated-normal
    j = envelope("power", paramsObj({{"k", 2.5}}));
    d = loadDistribution(j);
    json_decref(j);
    checkNear(static_cast<PowerDistribution&>(*d).k, 2.5, 0.0, "k survives exactly");
    j = envelope("kumaraswamy", paramsObj({{"a", 0.5}, {"b", 3.0}}));
    d = loadDistribution(j);
    json_decref(j);
    check(static_cast<KumaraswamyDistribution&>(*d).a == 0.5
          && static_cast<KumaraswamyDistribution&>(*d).b == 3.0, "a and b survive exactly");
    j = envelope("logit-normal", paramsObj({{"mu", -0.7}, {"sigma", 0.9}}));
    d = loadDistribution(j);
    json_decref(j);
    check(static_cast<LogitNormalDistribution&>(*d).mu == -0.7
          && static_cast<LogitNormalDistribution&>(*d).sigma == 0.9, "mu/sigma survive exactly");
    j = envelope("truncated-normal", paramsObj({{"mu", 0.4}, {"sigma", 0.11}}));
    d = loadDistribution(j);
    json_decref(j);
    check(static_cast<TruncatedNormalDistribution&>(*d).mu == 0.4
          && static_cast<TruncatedNormalDistribution&>(*d).sigma == 0.11,
          "truncated-normal mu/sigma survive exactly");
    // arcsine: parameterless — absent params fine, junk content tolerated-and-ignored
    j = envelope("arcsine", nullptr);
    d = loadDistribution(j);
    json_decref(j);
    check(d && std::string(d->typeName()) == "arcsine", "arcsine loads without params");
    j = envelope("arcsine", paramsObj({{"junk", 1.0}}));
    d = loadDistribution(j);
    json_decref(j);
    check(d != nullptr, "arcsine tolerates junk params content (nothing to read)");
}

static void testJsonValidation() {
    std::printf("json: validation (rejects)\n");
    auto expectNull = [](json_t* j, const char* what) {
        std::unique_ptr<Distribution> d = loadDistribution(j);
        check(d == nullptr, what);
        json_decref(j);
    };
    expectNull(json_object(), "empty object rejected");
    {
        json_t* j = envelope("power", paramsObj({{"k", 2.0}}));
        json_object_set_new(j, "format", json_string("notacurve"));
        expectNull(j, "wrong format tag rejected");
    }
    {
        json_t* j = envelope("power", paramsObj({{"k", 2.0}}));
        json_object_set_new(j, "version", json_integer(2));
        expectNull(j, "version 2 rejected");
    }
    {
        json_t* j = envelope("power", paramsObj({{"k", 2.0}}));
        json_object_set_new(j, "version", json_real(1.0));
        expectNull(j, "non-integer version rejected");
    }
    expectNull(envelope("bates", nullptr), "unknown type rejected (Bates was deliberately skipped)");
    expectNull(envelope("power", nullptr), "missing params rejected");
    expectNull(envelope("power", json_string("junk")), "params not an object rejected");
    expectNull(envelope("power", paramsObj({{"j", 2.0}})), "wrong param name rejected");
    expectNull(envelope("power", paramsObj({{"k", 0.0}})), "k == 0 rejected");
    expectNull(envelope("power", paramsObj({{"k", -1.0}})), "k < 0 rejected");
    expectNull(envelope("kumaraswamy", paramsObj({{"a", 2.0}})), "missing b rejected");
    expectNull(envelope("kumaraswamy", paramsObj({{"a", 0.0}, {"b", 1.0}})), "a == 0 rejected");
    expectNull(envelope("kumaraswamy", paramsObj({{"a", 1.0}, {"b", -2.0}})), "b < 0 rejected");
    expectNull(envelope("logit-normal", paramsObj({{"mu", 0.0}})), "missing sigma rejected");
    expectNull(envelope("logit-normal", paramsObj({{"mu", 0.0}, {"sigma", 0.0}})), "sigma == 0 rejected");
    expectNull(envelope("logit-normal", paramsObj({{"mu", 0.0}, {"sigma", -1.0}})), "sigma < 0 rejected");
    expectNull(envelope("truncated-normal", paramsObj({{"mu", 0.0}, {"sigma", 0.0}})),
               "truncated-normal sigma == 0 rejected");
    {
        json_t* j = envelope("power", paramsObj({{"k", 2.0}}));
        json_object_set_new(json_object_get(j, "params"), "k", json_string("x"));
        expectNull(j, "non-numeric param rejected");
    }
    // drawn (Pchip) delegates to MonotoneCurve's validation
    expectNull(envelope("drawn", nullptr), "drawn without points rejected");
    {
        json_t* j = envelope("drawn", nullptr);
        json_t* pts = json_array();
        json_t* pair = json_array();
        json_array_append_new(pair, json_real(0.0));
        json_array_append_new(pair, json_real(0.0));
        json_array_append_new(pts, pair);
        json_object_set_new(j, "points", pts);
        expectNull(j, "drawn with a single point rejected");
    }
    // dispatch separation: a parametric object is NOT a valid MonotoneCurve
    {
        json_t* j = envelope("triangular", paramsObj({{"mode", 0.3}}));
        MonotoneCurve curveOnly;
        check(!curveOnly.fromJson(j), "MonotoneCurve rejects a parametric object (type mismatch)");
        json_decref(j);
    }
}

static void testJsonPchip() {
    std::printf("json: drawn (PchipDistribution)\n");
    MonotoneCurve c = makeCurve({{0.0, 0.0}, {0.2, 0.05}, {0.5, 0.5}, {0.8, 0.95}, {1.0, 1.0}});
    c.name = "S test";
    PchipDistribution p(c);
    json_t* j = p.toJson();
    check(json_is_object(j), "drawn toJson produced an object");
    json_t* jt = json_object_get(j, "type");
    check(json_is_string(jt) && std::string(json_string_value(jt)) == "drawn", "type tag is \"drawn\"");
    json_t* jpts = json_object_get(j, "points");
    check(json_is_array(jpts) && (int)json_array_size(jpts) == c.size(), "points array matches size");
    std::unique_ptr<Distribution> loaded = loadDistribution(j);
    check(loaded && std::string(loaded->typeName()) == "drawn", "drawn loads as PchipDistribution");
    PchipDistribution& pc = static_cast<PchipDistribution&>(*loaded);
    check(pc.curve.size() == c.size(), "points count preserved");
    bool same = pc.curve.size() == c.size();
    for (int i = 0; same && i < c.size(); ++i)
        same = pc.curve[i].x == c[i].x && pc.curve[i].y == c[i].y;
    check(same, "points identical (exact doubles)");
    check(pc.curve.name == "S test", "metadata carried by the curve");
    json_decref(j);
}

int main() {
    std::printf("== Distribution layer ==\n");
    testContractBattery();
    testDegenerateToKnown();
    testEdgeBehaviour();
    testPchipWrapper();
    testJsonRoundTripAll();
    testJsonParamSurvival();
    testJsonValidation();
    testJsonPchip();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
