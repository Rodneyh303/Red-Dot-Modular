#pragma once
// ── dotModular::Distribution — the probability core of the output-distribution mapping ─────────
// (docs/design/DISTRIBUTION_LAYER.md). Distribution is the GENERAL probability object:
// cdf(x) / pdf(x) / invCDF(u) on [0,1]. Everything downstream (lane F⁻¹ application, PDF/CDF
// display, the effective-distribution visualiser) talks to this interface and never cares
// which concrete type. A drawn PCHIP curve (MonotoneCurve) is just ONE kind of distribution
// — PchipDistribution wraps it; the parametric types have closed-form inverses (NO LUT: the
// direct-eval architecture holds — the selection principle that rejected Bates).
//
// DOUBLE precision throughout, matching the copula math layer it calls into
// (redDot::copula::Phi / phi / PhiInv — reused, not reimplemented) and the double
// MonotoneCurve. The engine's float cast happens at the storage boundary only.
//
// v1 palette (shape-space coverage, ALL cheap inverses, ALL parametric types TUNABLE —
// the select → morph-with-knobs → convert UI needs params; a param-less type is a dead
// end, which is why Arcsine was removed):
//   WARPS (gentle identity-centred tilts): Power(k), ReversedPower(theta) (its upper-tail
//   mirror — frailty in -log(1-x), the Joe-copula corner), Mobius(a) — pure arithmetic,
//   bounded density, never spikes.  SIGMOIDS (pick-your-tails): LogitNormal(mu,sigma) (Phi-squash,
//   zero edges, open support), LogitSigmoid(a,b) (logistic squash — cheaper, heavier
//   tails, no Phi/erf).  SHAPES: Triangular(mode), Kumaraswamy(a,b) (flexible catch-all;
//   (1,1) = uniform), TruncatedNormal(mu,sigma) (bounded bell, NON-ZERO edge density).
//   Anything else: the drawn PchipDistribution (the escape valve — its "params" are the
//   handles).
//
// JSON: the shared .dmcurve v1 schema (docs/design/DMCURVE_EDITOR.md). The type field
// selects the concrete Distribution; params{} holds the parametric parameters; points[] is
// the drawn case (delegated to MonotoneCurve's own serializer/validator — one schema, both
// destinations, the TuningPreset house pattern). Type tags: "drawn", "triangular", "power",
// "reversed-power", "kumaraswamy", "mobius", "logit-normal", "logit-sigmoid",
// "truncated-normal".
//
// Pure module: no Rack/nanovg/UI. jansson included directly (test/jansson.h shim in the
// standalone harness, Rack's bundled jansson in the plugin tree).

#include <jansson.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "GaussianCopula.hpp"   // redDot::copula::{Phi, phi, PhiInv}
#include "MonotoneCurve.hpp"    // dotModular::MonotoneCurve (the drawn case)

namespace dotModular {

// Internal small helpers (kept out of the way of the public surface).
inline double distClamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
// Evaluation floor for pdfs that diverge at an edge when a shape parameter is < 1 (Power
// k<1, Kumaraswamy a<1 or b<1, Arcsine): the TRUE density is unbounded there; we return the
// (finite, large) value at the floor so callers never see inf/NaN. Interior evaluations are
// exact; the floor is far below any editor-visible resolution.
inline constexpr double DIST_EDGE_EPS = 1e-9;

class Distribution {
public:
    virtual ~Distribution() = default;

    // All on [0,1]; inputs clamped. cdf/invCDF return values in [0,1]; pdf is non-negative
    // (large but finite near a divergent edge — see DIST_EDGE_EPS).
    virtual double cdf(double x) const = 0;
    virtual double pdf(double x) const = 0;
    virtual double invCDF(double u) const = 0;

    // The .dmcurve schema type tag ("triangular", "drawn", ...).
    virtual const char* typeName() const = 0;
    // The .dmcurve v1 schema object for this distribution (caller owns it; json_decref).
    virtual json_t* toJson() const = 0;

protected:
    // Envelope shared by the parametric toJson()s (takes ownership of `params`).
    json_t* paramEnvelope(json_t* params) const {
        json_t* root = json_object();
        json_object_set_new(root, "format", json_string("dmcurve"));
        json_object_set_new(root, "version", json_integer(1));
        json_object_set_new(root, "type", json_string(typeName()));
        json_object_set_new(root, "params", params);
        return root;
    }
};

// ── Triangular(mode) — single linear peak ─────────────────────────────────────────────────────
// cdf: x<c ? x²/c : 1-(1-x)²/(1-c);  pdf: x<c ? 2x/c : 2(1-x)/(1-c);
// inv: u<c ? sqrt(uc) : 1-sqrt((1-u)(1-c)). Well-defined at mode 0/1 by branch
// reachability (x<c is empty when c==0; the right branch is empty when c==1) — the ramp
// limits, with pdf(0)=2 / pdf(1)=2 respectively.
class TriangularDistribution : public Distribution {
public:
    double mode;   // clamped into [0,1] at construction (and on load)
    explicit TriangularDistribution(double mode_) : mode(distClamp01(mode_)) {}

    const char* typeName() const override { return "triangular"; }

    double cdf(double x) const override {
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;
        if (x < mode) return (x * x) / mode;                              // mode==0 unreachable
        return 1.0 - ((1.0 - x) * (1.0 - x)) / (1.0 - mode);              // mode==1 unreachable
    }
    double pdf(double x) const override {
        x = distClamp01(x);
        if (x < mode) return (2.0 * x) / mode;              // mode > x >= 0, so mode > 0
        if (mode >= 1.0) return 2.0;                        // the mode=1 ramp's peak at x==1
        return (2.0 * (1.0 - x)) / (1.0 - mode);            // mode < 1 here
    }
    double invCDF(double u) const override {
        if (u <= 0.0) return 0.0;
        if (u >= 1.0) return 1.0;
        if (u < mode) return std::sqrt(u * mode);
        return 1.0 - std::sqrt((1.0 - u) * (1.0 - mode));
    }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "mode", json_real(mode));
        return paramEnvelope(p);
    }
};

// ── Power(k) — monotone skew; k==1 is the uniform ─────────────────────────────────────────────
// cdf: x^k;  pdf: k·x^(k-1);  inv: u^(1/k).
class PowerDistribution : public Distribution {
public:
    double k;   // > 0 (validated on load)
    explicit PowerDistribution(double k_) : k(k_) {}

    const char* typeName() const override { return "power"; }

    double cdf(double x) const override { return std::pow(distClamp01(x), k); }
    double pdf(double x) const override {
        x = distClamp01(x);
        if (x < DIST_EDGE_EPS) x = DIST_EDGE_EPS;   // divergent at 0 for k<1 (finite proxy)
        return k * std::pow(x, k - 1.0);
    }
    double invCDF(double u) const override { return std::pow(distClamp01(u), 1.0 / k); }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "k", json_real(k));
        return paramEnvelope(p);
    }
};

// ── ReversedPower(theta) — Power's upper-tail mirror ──────────────────────────────────────────
// cdf: 1-(1-x)^theta;  pdf: theta·(1-x)^(theta-1);  inv: 1-(1-u)^(1/theta). The x<->1-x
// mirror of Power (x^theta): frailty in -log(1-x) — the upper-tail counterpart to Power's
// lower-tail, and the only marginal form in the palette that, at r=0 with shared-theta
// modulation, yields the genuine JOE copula (vs Power -> Clayton). theta=1 is the uniform.
// Density diverges at x=1 for theta<1 (the DIST_EDGE_EPS proxy, mirrored from Power's 0-edge).
class ReversedPowerDistribution : public Distribution {
public:
    double theta;   // > 0 (validated on load)
    explicit ReversedPowerDistribution(double t) : theta(t) {}

    const char* typeName() const override { return "reversed-power"; }

    double cdf(double x) const override {
        x = distClamp01(x);
        return 1.0 - std::pow(1.0 - x, theta);
    }
    double pdf(double x) const override {
        x = distClamp01(x);
        double tail = 1.0 - x;
        if (tail < DIST_EDGE_EPS) tail = DIST_EDGE_EPS;   // divergent at x=1 for theta<1
        return theta * std::pow(tail, theta - 1.0);
    }
    double invCDF(double u) const override {
        u = distClamp01(u);
        return 1.0 - std::pow(1.0 - u, 1.0 / theta);
    }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "theta", json_real(theta));
        return paramEnvelope(p);
    }
};

// ── Kumaraswamy(a,b) — flexible bounded family; (1,1) is the uniform ──────────────────────────
// cdf: 1-(1-x^a)^b;  pdf: a·b·x^(a-1)·(1-x^a)^(b-1);  inv: (1-(1-u)^(1/b))^(1/a).
class KumaraswamyDistribution : public Distribution {
public:
    double a, b;   // > 0 (validated on load)
    KumaraswamyDistribution(double a_, double b_) : a(a_), b(b_) {}

    // ── (mode, concentration) authoring basis ─────────────────────────────────────────────
    // Native (a,b) are a poor CONTROL basis: both knobs pull mode AND spread. The authoring
    // basis (TODO_distribution_palette_and_editor.md #1 / CC_PROMPT_P1 sub-task A) is
    // (mode, concentration) with conc := a (the LEFT shape exponent) and b solved CLOSED-FORM
    // to hold the mode:
    //     forward (abFromModeConc):   a = conc,  b = (a-1+m^a)/(a·m^a)   [mode^a=(a-1)/(ab-1)]
    //     inverse (modeConcFromAb):   conc = a,  mode = ((a-1)/(ab-1))^(1/a)
    // — an exact inverse BOTH ways, no root-finding (conc:=b would be transcendental in a).
    // b-1 = (a-1)(1-m^a)/(a·m^a) > 0 for a>1, m<1, so the forward image always lands in the
    // a>1, b>1 unimodal region, and the conc knob never moves the mode (b is solved to hold
    // it — exact orthogonality). Internal evaluation and the .dmcurve schema STAY (a,b); this
    // is the UI/authoring mapping only, and a parametric never faces the editor's
    // eps-monotonicity gate (only "convert to editable" samples it, at 9 points).
    //
    // Clamps (LOAD-BEARING, verified — do NOT widen): conc ∈ [1, CONC_MAX=5] and mode ∈
    // [MODE_EPS, 1-MODE_EPS] with MODE_EPS=0.02. conc := a drives b ~ mode^(-a), which
    // EXPLODES: conc=5 → b up to ~2.5e8 (invCDF still strictly increasing across a 1000-pt
    // grid in double); conc=8 → b ~1e13 and the invCDF goes FLAT (ties — the numerical
    // atom); conc=100 → b ~1e169, totally flat. conc=5 reaches peak height ~3.7 — musically
    // ample; a needle spike IS a numerical atom. MODE_EPS keeps m^a off the 0/denormal
    // boundary. Proven by test_kumaraswamy_reparam: finite a,b>0, never NaN/inf, invCDF
    // strictly increasing, over the whole clamped box.
    static constexpr double CONC_MAX = 5.0;    // 1 = uniform, 5 = peak height ~3.7
    static constexpr double MODE_EPS = 0.02;   // authoring mode floor/ceiling

    static void abFromModeConc(double mode, double conc, double& a, double& b) {
        if (!(conc >= 1.0)) conc = 1.0;                    // NaN-safe clamps into the box
        else if (conc > CONC_MAX) conc = CONC_MAX;
        if (!(mode >= MODE_EPS)) mode = MODE_EPS;
        else if (mode > 1.0 - MODE_EPS) mode = 1.0 - MODE_EPS;
        if (conc == 1.0) { a = 1.0; b = 1.0; return; }     // uniform; the mode is irrelevant
        a = conc;
        const double ma = std::pow(mode, a);
        b = (a - 1.0 + ma) / (a * ma);
    }

    static void modeConcFromAb(double a, double b, double& mode, double& conc) {
        if (a == 1.0 && b == 1.0) { mode = 0.5; conc = 1.0; return; }   // uniform: 0/0 sentinel
        if (!(a > 1.0) || !(b > 1.0)) {
            // No interior mode — a J/U/monotone shape authored directly in (a,b) (the forward
            // map never produces these): report the boundary the mass sits against, via the
            // median: cdf(0.5) >= 0.5 means the majority of the mass is left (density
            // decreasing) -> mode 0; otherwise -> mode 1.
            if (!(a >= 1.0)) conc = 1.0;
            else if (a > CONC_MAX) conc = CONC_MAX;
            else conc = a;
            const KumaraswamyDistribution d(a, b);
            mode = (d.cdf(0.5) >= 0.5) ? 0.0 : 1.0;
            return;
        }
        conc = a;   // honest readout (may exceed CONC_MAX for (a,b) authored directly)
        mode = std::pow((a - 1.0) / (a * b - 1.0), 1.0 / a);
    }

    const char* typeName() const override { return "kumaraswamy"; }

    double cdf(double x) const override {
        x = distClamp01(x);
        return 1.0 - std::pow(1.0 - std::pow(x, a), b);
    }
    double pdf(double x) const override {
        x = distClamp01(x);
        if (x < DIST_EDGE_EPS) x = DIST_EDGE_EPS;                  // divergent at 0 for a<1
        double tail = 1.0 - std::pow(x, a);
        if (tail < DIST_EDGE_EPS) tail = DIST_EDGE_EPS;            // divergent at 1 for b<1
        return a * b * std::pow(x, a - 1.0) * std::pow(tail, b - 1.0);
    }
    double invCDF(double u) const override {
        u = distClamp01(u);
        return std::pow(1.0 - std::pow(1.0 - u, 1.0 / b), 1.0 / a);
    }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "a", json_real(a));
        json_object_set_new(p, "b", json_real(b));
        return paramEnvelope(p);
    }
};

// ── Mobius(a) — the rational-linear warp: gentle bounded tilt, identity at a=1 ────────────────
// cdf: x/((1-a)x+a);  inv: ay/(1-(1-a)y);  pdf: a/((1-a)x+a)^2 (the x-terms cancel in the
// quotient rule). PURE ARITHMETIC — one divide each, no transcendentals: the CHEAPEST type
// in the palette. Density bounded in [1/a, a] (never spikes, unlike Power whose density
// diverges at an edge for a<1); a>1 tilts mass toward x=1, 0<a<1 toward x=0, and a=1 is
// the identity — the clean no-op centre. The "subtle, never-extreme tilt" tool.
class MobiusDistribution : public Distribution {
public:
    double a;   // > 0 (validated on load)
    explicit MobiusDistribution(double a_) : a(a_) {}

    const char* typeName() const override { return "mobius"; }

    double cdf(double x) const override {
        x = distClamp01(x);
        return x / ((1.0 - a) * x + a);
    }
    double pdf(double x) const override {
        x = distClamp01(x);
        const double d = (1.0 - a) * x + a;   // > 0 for all x in [0,1] when a > 0
        return a / (d * d);
    }
    double invCDF(double u) const override {
        u = distClamp01(u);
        return (a * u) / (1.0 - (1.0 - a) * u);
    }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "a", json_real(a));
        return paramEnvelope(p);
    }
};

// ── LogitSigmoid(a, b) — the logistic squash: logit-normal's cheaper, heavier-tailed sibling
// logit(x)=ln(x/(1-x)); sigma(z)=1/(1+e^-z);
// cdf: sigma(a·logit(x)+b);  inv: sigma((logit(y)-b)/a);  pdf: a·f(x)·(1-f(x))/(x·(1-x)).
// exp/log only — NO Phi/erf (kept ALONGSIDE logit-normal: similar shape space, distinct
// tails + cost). Identity at a=1, b=0 (another gentle-warp centre). The pdf self-limits at
// the edges when evaluated at the DIST_EDGE_EPS floor (a>=1 lands on its true limit, a<1
// gives the same finite divergent-edge proxy as Power/Kumaraswamy).
class LogitSigmoidDistribution : public Distribution {
public:
    double a;   // > 0 steepness (validated on load)
    double b;   // shift
    LogitSigmoidDistribution(double a_, double b_) : a(a_), b(b_) {}

    const char* typeName() const override { return "logit-sigmoid"; }

    static double sigmoid(double z) { return 1.0 / (1.0 + std::exp(-z)); }

    double cdf(double x) const override {
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;
        if (x < DIST_EDGE_EPS) x = DIST_EDGE_EPS;
        else if (x > 1.0 - DIST_EDGE_EPS) x = 1.0 - DIST_EDGE_EPS;
        return sigmoid(a * std::log(x / (1.0 - x)) + b);
    }
    double pdf(double x) const override {
        x = distClamp01(x);
        if (x < DIST_EDGE_EPS) x = DIST_EDGE_EPS;
        else if (x > 1.0 - DIST_EDGE_EPS) x = 1.0 - DIST_EDGE_EPS;
        const double f = sigmoid(a * std::log(x / (1.0 - x)) + b);
        return a * f * (1.0 - f) / (x * (1.0 - x));
    }
    double invCDF(double u) const override {
        if (u <= 0.0) return 0.0;
        if (u >= 1.0) return 1.0;
        return sigmoid((std::log(u / (1.0 - u)) - b) / a);
    }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "a", json_real(a));
        json_object_set_new(p, "b", json_real(b));
        return paramEnvelope(p);
    }
};

// ── LogitNormal(mu, sigma) — squashed bell on open support (zero edge density) ─────────────────
// logit(x)=ln(x/(1-x)); logistic(z)=1/(1+e^-z);
// cdf: Phi((logit(x)-mu)/sigma);  pdf: phi((logit(x)-mu)/sigma)/(sigma·x·(1-x));
// inv: logistic(mu + sigma·PhiInv(u)). REUSES redDot::copula::{Phi, phi, PhiInv} — the
// engine's exact normal machinery, no reimplementation. x,u clamped off exact {0,1}.
class LogitNormalDistribution : public Distribution {
public:
    double mu;
    double sigma;   // > 0 (validated on load)
    LogitNormalDistribution(double mu_, double sigma_) : mu(mu_), sigma(sigma_) {}

    const char* typeName() const override { return "logit-normal"; }

    double cdf(double x) const override {
        if (x <= 0.0) return 0.0;                    // open-support limit
        if (x >= 1.0) return 1.0;                    // open-support limit
        if (x < DIST_EDGE_EPS) x = DIST_EDGE_EPS;
        else if (x > 1.0 - DIST_EDGE_EPS) x = 1.0 - DIST_EDGE_EPS;
        return distClamp01(redDot::copula::Phi((std::log(x / (1.0 - x)) - mu) / sigma));
    }
    double pdf(double x) const override {
        if (x <= 0.0 || x >= 1.0) return 0.0;        // the density vanishes at the edges
        if (x < DIST_EDGE_EPS) x = DIST_EDGE_EPS;
        else if (x > 1.0 - DIST_EDGE_EPS) x = 1.0 - DIST_EDGE_EPS;
        const double z = (std::log(x / (1.0 - x)) - mu) / sigma;
        return redDot::copula::phi(z) / (sigma * x * (1.0 - x));
    }
    double invCDF(double u) const override {
        if (u <= 0.0) return 0.0;
        if (u >= 1.0) return 1.0;
        const double z = mu + sigma * redDot::copula::PhiInv(u);
        return 1.0 / (1.0 + std::exp(-z));           // logistic(z)
    }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "mu", json_real(mu));
        json_object_set_new(p, "sigma", json_real(sigma));
        return paramEnvelope(p);
    }
};

// ── TruncatedNormal(mu, sigma) — bounded bell with NON-ZERO edge density ──────────────────────
// Normal clipped to [0,1]: a=(0-mu)/sigma, b=(1-mu)/sigma, Z=Phi(b)-Phi(a);
// cdf: (Phi((x-mu)/sigma)-Phi(a))/Z;  pdf: phi((x-mu)/sigma)/(sigma·Z);
// inv: clamp(mu + sigma·PhiInv(Phi(a) + u·Z), 0, 1). Unlike the logit-normal, the clipped
// bell keeps positive density AT the edges (can pile mass at an edge). When the window is
// so far off support that Z underflows to exactly 0 in double (both Phi(a), Phi(b) round to
// the same saturated value), the mass sits at the NEAREST edge — the documented degenerate:
// a point mass at 0 (mu << 0) or 1 (mu >> 1), with cdf a step, invCDF the edge, pdf 0.
class TruncatedNormalDistribution : public Distribution {
public:
    double mu;
    double sigma;   // > 0 (validated on load; direct construction trusts the caller)

    TruncatedNormalDistribution(double mu_, double sigma_)
        : mu(mu_), sigma(sigma_),
          a_((0.0 - mu_) / sigma_),
          b_((1.0 - mu_) / sigma_),
          Pa_(redDot::copula::Phi(a_)),
          Pb_(redDot::copula::Phi(b_)),
          Z_(Pb_ - Pa_),
          degenerate_(Z_ <= 0.0),
          left_(mu_ < 0.5) {}

    const char* typeName() const override { return "truncated-normal"; }

    double cdf(double x) const override {
        if (degenerate_) return left_ ? 1.0 : (x >= 1.0 ? 1.0 : 0.0);
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;
        return distClamp01((redDot::copula::Phi((x - mu) / sigma) - Pa_) / Z_);
    }
    double pdf(double x) const override {
        if (degenerate_) return 0.0;
        x = distClamp01(x);
        return redDot::copula::phi((x - mu) / sigma) / (sigma * Z_);
    }
    double invCDF(double u) const override {
        if (u <= 0.0) return 0.0;
        if (u >= 1.0) return 1.0;
        if (degenerate_) return left_ ? 0.0 : 1.0;
        return distClamp01(mu + sigma * redDot::copula::PhiInv(Pa_ + u * Z_));
    }
    json_t* toJson() const override {
        json_t* p = json_object();
        json_object_set_new(p, "mu", json_real(mu));
        json_object_set_new(p, "sigma", json_real(sigma));
        return paramEnvelope(p);
    }

private:
    const double a_, b_, Pa_, Pb_, Z_;
    const bool degenerate_;
    const bool left_;
};

// ── PchipDistribution — the DRAWN case: a MonotoneCurve viewed as a Distribution ───────────────
// cdf=curve.evalCDF, invCDF=curve.evalInverse, pdf=curve.pdf — cast-free (the curve is
// double). The curve is public: the .dmcurve editor widget manipulates it; a Pchip
// distribution is NOT special, just the flexible escape valve of the palette.
class PchipDistribution : public Distribution {
public:
    MonotoneCurve curve;

    PchipDistribution() = default;
    explicit PchipDistribution(const MonotoneCurve& c) : curve(c) {}
    explicit PchipDistribution(MonotoneCurve&& c) : curve(std::move(c)) {}

    const char* typeName() const override { return "drawn"; }

    double cdf(double x) const override { return curve.evalCDF(x); }
    double pdf(double x) const override { return curve.pdf(x); }
    double invCDF(double u) const override { return curve.evalInverse(u); }
    json_t* toJson() const override { return curve.toJson(); }   // the drawn schema, verbatim
};

// ── JSON dispatch ─────────────────────────────────────────────────────────────────────────────
// Load a Distribution from the .dmcurve v1 schema (BORROWS rootJ — the caller keeps
// ownership). Returns nullptr on any structural failure: wrong format/version, unknown
// type, missing / non-numeric / non-finite params, non-positive shape params (k, theta,
// sigma, a, b — they are rejected, not clamped: a negative scale is a malformed file), or bad
// points for the drawn case (MonotoneCurve's own validation). mode is clamped into [0,1]
// (sanitize, like the curve's point sanitizer). An absent type defaults to "drawn" (the
// minimal {format, points} object loads, matching MonotoneCurve). Params content is
// tolerated-and-ignored for parameterless types (arcsine). House pattern: TuningPreset /
// MonotoneCurve.
inline std::unique_ptr<Distribution> loadDistribution(json_t* rootJ) {
    if (!json_is_object(rootJ)) return nullptr;
    json_t* jfmt = json_object_get(rootJ, "format");
    if (!json_is_string(jfmt) || std::string(json_string_value(jfmt)) != "dmcurve") return nullptr;
    json_t* jver = json_object_get(rootJ, "version");
    if (jver && !json_is_integer(jver)) return nullptr;
    if (jver && (int)json_integer_value(jver) != 1) return nullptr;
    json_t* jtype = json_object_get(rootJ, "type");
    const std::string type = json_is_string(jtype) ? json_string_value(jtype) : "drawn";

    // required finite param from params{}
    auto numParam = [&](const char* key, double& out) -> bool {
        json_t* jp = json_object_get(rootJ, "params");
        if (!json_is_object(jp)) return false;
        json_t* jv = json_object_get(jp, key);
        if (!json_is_number(jv)) return false;
        out = json_number_value(jv);
        return std::isfinite(out);
    };
    auto posParam = [&](const char* key, double& out) -> bool {
        return numParam(key, out) && out > 0.0;
    };

    if (type == "drawn") {
        auto d = std::make_unique<PchipDistribution>();
        if (!d->curve.fromJson(rootJ)) return nullptr;   // MonotoneCurve's validation/sanitize
        return d;
    }
    if (type == "triangular") {
        double mode;
        if (!numParam("mode", mode)) return nullptr;
        return std::make_unique<TriangularDistribution>(mode);
    }
    if (type == "power") {
        double k;
        if (!posParam("k", k)) return nullptr;
        return std::make_unique<PowerDistribution>(k);
    }
    if (type == "reversed-power") {
        double th;
        if (!posParam("theta", th)) return nullptr;
        return std::make_unique<ReversedPowerDistribution>(th);
    }
    if (type == "kumaraswamy") {
        double a, b;
        if (!posParam("a", a) || !posParam("b", b)) return nullptr;
        return std::make_unique<KumaraswamyDistribution>(a, b);
    }
    if (type == "logit-normal") {
        double mu, sg;
        if (!numParam("mu", mu) || !posParam("sigma", sg)) return nullptr;
        return std::make_unique<LogitNormalDistribution>(mu, sg);
    }
    if (type == "mobius") {
        double a;
        if (!posParam("a", a)) return nullptr;
        return std::make_unique<MobiusDistribution>(a);
    }
    if (type == "logit-sigmoid") {
        double a, b;
        if (!posParam("a", a) || !numParam("b", b)) return nullptr;
        return std::make_unique<LogitSigmoidDistribution>(a, b);
    }
    if (type == "truncated-normal") {
        double mu, sg;
        if (!numParam("mu", mu) || !posParam("sigma", sg)) return nullptr;
        return std::make_unique<TruncatedNormalDistribution>(mu, sg);
    }
    return nullptr;   // unknown type
}

} // namespace dotModular
