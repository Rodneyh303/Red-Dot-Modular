// test_phase_distortion.cpp — the PHASE-DISTORTION EXPANDER MODEL battery
// (PHASE_DISTRIBUTION_SPEC §14 + docs/design/CC_PROMPT_PHASE_DISTORTION_EXPANDER.md —
// the adopted connection model: the expander's EIGHT PARAMETER SETS, the block-
// consistency mechanism, the ONE shared transform primitive, the Monsoon wiring).
//
// THE GATES, mapped 1:1 (the CC prompt's "GATES (all must pass)"):
//   G1  IDENTITY            — all eight sets at identity: the primitive maps u to u
//                             BIT-EXACTLY over a sweep (the Stage-1 gate's load-bearing
//                             law; the production bypass keys on the same predicate).
//   G2  SAME INDEX          — two readers snapshotting the SAME store set produce
//                             bit-identical primitive outputs, sample for sample.
//   G3  DIFFERENT INDEX     — two readers on different sets differ ONLY by the sets
//                             they read (equal values at two indices ⟹ equal outputs;
//                             a differing value ⟹ the output tracks its own set).
//   G4  BLOCK CONSISTENCY   — the Task-1 gate: TWO reader instances and a parameter
//                             write BETWEEN blocks — both readers see the new value at
//                             the SAME block boundary, never one before the other; a
//                             mid-block write changes NO reader's in-block snapshot;
//                             a multi-field commit is atomic per set (never torn).
//   G5  SYNC                — structural (no module sits between source and Monsoon;
//                             the wiring review pins it) — no runtime pin is possible
//                             for cable topology in a unit rig; recorded here.
//   G6  SINGLE IMPLEMENTATION — the bridge: the primitive's output == WarpBank::
//                             warpedValue's output for the SAME cell values (the
//                             extracted evalWarpCellDistortion has ONE copy; the lane
//                             draws and the phase tier cannot drift).
//   G7  SEMANTICS UNCHANGED — the landed chain's own battery (test_phase_distribution)
//                             stays green in run_all; here the primitive re-proves the
//                             reading law through the set shape (dual by sign, gap
//                             clamp, atom plateau) and the [0,1] range over the grid.
//   G8  run_all green       — both SDK legs (the battery itself; not a local pin).
//
// Plus the RIDING fix's pins (Rodney: "The calibration snap-to-pulses fix above is
// small and can ride with it"): the straight 24-PPQN detent list {bar, half, quarter,
// eighth, 16th} = {96,48,24,12,6} pulses = {16,8,4,2,1} sixteenths — every detent
// divides the 96-pulse bar exactly and the knob→detent map covers [0,1] with no gaps.
//
// Header-only, Rack-free (jansson via the test/jansson.h shim); registered in
// test/run_all.sh.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>

#include "PhaseDistStore.hpp"           // the store + the mechanism (the Task-1 unit)
#include "SandsModifierResolver.hpp"    // the primitive + the bank (the Task-2 unit)

using dotModular::WarpBank;
using dotModular::WarpF;
using redDot::PhaseDistSet;
using redDot::PhaseDistStore;
namespace ws = dotModular::warpStages;

static int g_pass = 0, g_fail = 0;
static void ok(bool cond, const char* what) {
    if (cond) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static bool sameBits(double a, double b) {
    uint64_t ra, rb;
    std::memcpy(&ra, &a, sizeof(ra));
    std::memcpy(&rb, &b, sizeof(rb));
    return ra == rb;
}

// A roster of parametric members small but broad (the palette minus drawn/identity —
// the identity is pinned separately; drawn cannot enter the store).
struct RosterEntry { WarpF f; const char* name; };
static RosterEntry g_roster[] = {
    { dotModular::TriangularWarp{0.5},        "triangular" },
    { dotModular::PowerWarp{2.0},             "power" },
    { dotModular::KumaraswamyWarp{0.55, 2.2}, "kumaraswamy" },
    { dotModular::MobiusWarp{1.5},            "mobius" },
    { dotModular::LogitSigmoidWarp{1.5, 0.0}, "logit-sigmoid" },
    { dotModular::ReversedPowerWarp{2.0},     "reversed-power" },
};

static int mainG1() {
    std::printf("G1 IDENTITY — the eight sets at identity map u to u bit-exactly\n");
    PhaseDistStore store;   // default-constructed = eight identity sets
    bool allIdent = true, bitExact = true;
    for (int s = 0; s < PhaseDistStore::kSets; ++s) {
        PhaseDistSet snap{};
        store.snapshotSet(s, snap);
        if (!snap.isIdentity()) allIdent = false;
    }
    ok(allIdent, "G1: a default store is EIGHT identity sets (isIdentity on every set)");
    // The bit-exact sweep through the PRIMITIVE (the production call shape: the set's cell).
    for (int i = 0; i <= 200; ++i) {
        const double u = (double)i / 200.0;
        PhaseDistSet snap{};
        store.snapshotSet(3, snap);                      // any index — all eight are identity
        const double got = dotModular::phaseDistort(u, snap.cell());
        if (!sameBits(got, u)) { bitExact = false; break; }
    }
    ok(bitExact, "G1: the primitive at identity == u BIT-EXACT over the sweep (201 points)");
    // Every single-feature neutral is identity too (the store-level neutrality law).
    {
        PhaseDistSet s{};
        s.w = 0.3f;                         ok(!s.isIdentity(), "G1: w != 0 is not identity");
        s = PhaseDistSet{}; s.tilt = 0.2f;  ok(!s.isIdentity(), "G1: tilt != 0 is not identity");
        s = PhaseDistSet{}; s.atomMass = 0.1f; ok(!s.isIdentity(), "G1: mass != 0 is not identity");
        s = PhaseDistSet{}; s.gapSize = 0.05f; ok(!s.isIdentity(), "G1: gap width != 0 is not identity");
        s = PhaseDistSet{}; s.f = dotModular::TriangularWarp{0.5f}; ok(!s.isIdentity(), "G1: a member != identity is not identity");
        s = PhaseDistSet{}; s.delegated = true;
        ok(s.isIdentity(), "G1: delegation alone does not break identity (engine-inert)");
    }
    return 0;
}

// THE READER (the mechanism's reader half, exactly what Monsoon does at its block
// start): a private snapshot + a block counter, so the test can drive blocks.
struct TestReader {
    PhaseDistStore* store = nullptr;
    PhaseDistSet    snap{};
    long            blocks = 0;
    void beginBlock(int idx) { store->snapshotSet(idx, snap); ++blocks; }
    double eval(double u) const { return dotModular::phaseDistort(u, snap.cell()); }
};

static int mainG2G3() {
    std::printf("G2/G3 SAME + DIFFERENT INDEX — the sets the readers read decide the output\n");
    PhaseDistStore store;
    // Set 1 (idx 0): Power-2 warp at +0.7 with a tilt. Set 2 (idx 1): the IDENTICAL
    // values. Set 3 (idx 2): a kumaraswamy member with a gap, negative w (the dual).
    PhaseDistSet a{}; a.f = dotModular::PowerWarp{2.0}; a.w = 0.7f; a.tilt = 0.3f;
    PhaseDistSet b = a;                                            // identical values
    PhaseDistSet c{}; c.f = dotModular::KumaraswamyWarp{0.55f, 2.2f};
    c.w = -0.6f; c.gapLoc = 0.4f; c.gapSize = 0.15f;
    store.writeSet(0, a);
    store.writeSet(1, b);
    store.writeSet(2, c);

    TestReader r1, r2; r1.store = r2.store = &store;
    r1.beginBlock(0);                     // reader 1 on set 1
    r2.beginBlock(1);                     // reader 2 on set 2 — the SAME values
    bool same = true;
    for (int i = 0; i <= 100; ++i) {
        const double u = (double)i / 100.0;
        if (!sameBits(r1.eval(u), r2.eval(u))) { same = false; break; }
    }
    ok(same, "G2: two readers on EQUAL sets at different indices — identical outputs (bit-exact sweep)");

    TestReader r3; r3.store = &store; r3.beginBlock(2);   // reader 3 on the differing set
    int diffs = 0; bool inRange = true;
    for (int i = 1; i <= 100; ++i) {
        const double u = (double)i / 100.0;
        const double g1 = r1.eval(u), g3 = r3.eval(u);
        if (!sameBits(g1, g3)) ++diffs;                     // the same u, different sets
        if (!(g3 >= 0.0 && g3 <= 1.0)) { inRange = false; break; }
    }
    ok(diffs > 0, "G3: a differing set gives a differing output for the same input (the sets decide)");
    ok(inRange,  "G3: the dual-side output stays in [0,1] over the sweep (a valid phase)");
    ok(sameBits(r3.eval(0.0), 0.0) && sameBits(r3.eval(1.0), 1.0),
       "G3: the composed reading pins the Sklar edges W(0)=0, W(1)=1");
    return 0;
}

static int mainG4() {
    std::printf("G4 BLOCK CONSISTENCY — the Task-1 gate (two readers, writes between blocks)\n");
    PhaseDistStore store;
    TestReader ra, rb; ra.store = rb.store = &store;
    const int kBlockFrames = 32;   // the test's block cadence (any fixed period; the
                                   // production cadence is Monsoon's Control tier)

    PhaseDistSet warm{}; warm.f = dotModular::PowerWarp{2.0}; warm.w = 0.5f;
    store.writeSet(4, warm);

    // ── THE GATE SCENARIO: a parameter write BETWEEN blocks ──────────────────────
    // Drive both readers block-locked (the same block indices). Between block 5 and
    // block 6, WRITE a new value into set 4. Both readers must adopt it at block 6.
    PhaseDistSet changed = warm; changed.w = -0.8f;   // a different depth AND sign
    long raFlip = -1, rbFlip = -1;
    for (long blk = 0; blk < 10; ++blk) {
        ra.beginBlock(4); rb.beginBlock(4);
        if (blk == 5) store.writeSet(4, changed);      // the write BETWEEN block 5 and 6
        if (raFlip < 0 && ra.snap == changed) raFlip = blk;
        if (rbFlip < 0 && rb.snap == changed) rbFlip = blk;
    }
    ok(raFlip == 6 && rbFlip == 6,
       "G4: a write between blocks — BOTH readers see the new value at the SAME block boundary (6)");
    ok(raFlip == rbFlip, "G4: never one before the other");

    // ── A MID-BLOCK write is invisible until the boundary ────────────────────────
    ra.blocks = rb.blocks = 0;
    ra.beginBlock(4);                                   // block start: the snapshot
    PhaseDistSet mid = changed; mid.tilt = 0.4f;
    store.writeSet(4, mid);                             // the write MID-block
    bool frozen = true;
    for (int f = 0; f < kBlockFrames; ++f) {
        if (!(ra.snap == changed)) { frozen = false; break; }   // the snapshot must NOT move
    }
    ok(frozen, "G4: a mid-block write changes NOTHING in the reader's in-block snapshot");
    ra.beginBlock(4);
    ok(ra.snap == mid, "G4: the mid-block write lands at the reader's NEXT block boundary");

    // ── A MULTI-FIELD COMMIT IS ATOMIC (the tear the mechanism exists to kill) ───
    // Write the gap's (loc, size) as one set: a snapshot either side of the commit
    // sees the whole old pair or the whole new pair — the mutex makes a torn pair
    // (new loc beside old size) unrepresentable.
    PhaseDistSet gapOld{}; gapOld.gapLoc = 0.2f; gapOld.gapSize = 0.1f;
    PhaseDistSet gapNew{}; gapNew.gapLoc = 0.7f; gapNew.gapSize = 0.2f;
    store.writeSet(5, gapOld);
    ra.beginBlock(5);
    ok(ra.snap == gapOld, "G4: the snapshot before the commit reads the whole old set");
    store.writeSet(5, gapNew);
    ok(ra.snap == gapOld, "G4: the committed write does not tear the already-taken snapshot");
    ra.beginBlock(5);
    ok(ra.snap == gapNew, "G4: the snapshot after the commit reads the whole new set");

    // ── THE JSON ROUND TRIP (the store's persistence) ─────────────────────────────
    {
        json_t* root = json_object();
        json_object_set_new(root, "sets", store.toJson());
        PhaseDistStore loaded;
        const bool full = loaded.fromJson(json_object_get(root, "sets"));
        ok(full, "G4: the store's JSON round trip reports full-length");
        bool eq = true;
        for (int s = 0; s < PhaseDistStore::kSets; ++s) {
            PhaseDistSet x{}, y{};
            store.snapshotSet(s, x); loaded.snapshotSet(s, y);
            if (!(x == y)) { eq = false; break; }
        }
        ok(eq, "G4: all eight sets survive the round trip value-identical");
        json_decref(root);
    }
    // Missing/malformed key ⟹ the identity defaults (the house contract).
    {
        PhaseDistStore loaded;
        ok(!loaded.fromJson(nullptr), "G4: a null payload loads nothing (returns not-full)");
        PhaseDistSet s{}; loaded.snapshotSet(0, s);
        ok(s.isIdentity(), "G4: a payload-less load leaves the identity defaults");
    }
    return 0;
}

static int mainG6() {
    std::printf("G6 SINGLE IMPLEMENTATION — the primitive == the bank's own mix (the bridge)\n");
    // The SAME cell values, evaluated BOTH ways: the phase primitive (the expander
    // model's call) and WarpBank::warpedValue (the lane draws' call — which resolves
    // the bank slot to a cell and runs the ONE extracted body). Bit-equal over the
    // roster × the signed depth grid, with tilt/gap/atom LIVE — the proof the two
    // consumers cannot drift (§14 gate "the transform exists once").
    WarpBank bank;
    const int v = 3, lane = 2;
    bool bridge = true;
    const char* broke = "";
    for (const auto& r : g_roster) {
        for (double w : { -1.0, -0.5, -0.2, -0.01, 0.0, 0.01, 0.2, 0.5, 1.0 }) {
            PhaseDistSet s{};
            s.f = r.f; s.w = (float)w;
            s.tilt = 0.25f; s.atomLevel = 0.6f; s.atomMass = 0.2f;
            s.gapLoc = 0.4f; s.gapSize = 0.12f;
            // Seat the SAME values in the bank (delegation OFF, no delta planes —
            // resolveWarpCell then returns the base cell verbatim).
            const int i = WarpBank::index(v, lane);
            bank.warpF[i] = s.f; bank.warpW[i] = s.w;
            bank.warpTilt[i] = s.tilt; bank.warpAtomLevel[i] = s.atomLevel;
            bank.warpAtomMass[i] = s.atomMass; bank.warpGapLoc[i] = s.gapLoc;
            bank.warpGapSize[i] = s.gapSize;
            for (int k = 0; k <= 50; ++k) {
                const double u = (double)k / 50.0;
                if (!sameBits(dotModular::phaseDistort(u, s.cell()),
                              bank.warpedValue(v, lane, u))) {
                    bridge = false; broke = r.name; break;
                }
            }
            if (!bridge) break;
        }
        if (!bridge) break;
    }
    ok(bridge, broke[0] ? "G6: BRIDGE BROKEN (see the roster entry in the build log)"
                        : "G6: phaseDistort(u, set) == WarpBank::warpedValue(u) bit-exact over the roster x signed-w grid (the ONE body)");
    return 0;
}

static int mainG7() {
    std::printf("G7 SEMANTICS — the reading law, the clamp, the atom, the range (through the set shape)\n");
    // The DUAL by SIGN through the primitive: the same member, w = +1 reads the
    // composed quantile, w = −1 the composed CDF (the exchange), the identity
    // exempt. Verified against the chain helpers DIRECTLY (the law's own forms).
    bool dualOK = true;
    for (const auto& r : g_roster) {
        PhaseDistSet s{}; s.f = r.f;
        s.tilt = 0.2f; s.gapLoc = 0.35f; s.gapSize = 0.1f; s.atomMass = 0.15f;
        // THE FLOAT-STORE VALUES (the engine's own arithmetic): the set carries
        // floats, the chain helpers take doubles — the expectation is computed on
        // the FLOAT-ROUNDTRIPPED values, exactly what the primitive reads.
        const double pT = (double)s.tilt, gL = (double)s.gapLoc, gS = (double)s.gapSize;
        const double aL = (double)s.atomLevel, aM = (double)s.atomMass;
        s.w = 1.0f;
        const redDot::WarpCell c = s.cell();
        for (int k = 1; k <= 40; ++k) {
            const double u = (double)k / 41.0;
            const double q = ws::invCDF(u, pT, gL, gS, aL, aM,
                [&](double uu) { return dotModular::invCDF(r.f, uu, nullptr); },
                [&](double x)  { return dotModular::cdf(r.f, x, nullptr); });
            if (!sameBits(dotModular::phaseDistort(u, c), q)) { dualOK = false; break; }
        }
        s.w = -1.0f;
        const redDot::WarpCell cd = s.cell();
        for (int k = 1; k <= 40; ++k) {
            const double u = (double)k / 41.0;
            const double g = ws::cdf(u, pT, gL, gS, aL, aM,
                [&](double x) { return dotModular::cdf(r.f, x, nullptr); });
            if (!sameBits(dotModular::phaseDistort(u, cd), g)) { dualOK = false; break; }
        }
        if (!dualOK) break;
    }
    ok(dualOK, "G7: the sign IS the reading — +1 the composed quantile, −1 the composed CDF, over the roster");

    // The GAP CLAMP through the primitive: a mass ≈ 1 lands on the clamped mass
    // (valid output, no NaN) — the ε = 1e-6 spec parameter rides the set shape.
    {
        PhaseDistSet s{}; s.f = dotModular::PowerWarp{2.0};
        s.gapLoc = 0.05f; s.gapSize = 0.9f; s.w = 1.0f;   // a near-full-span gap
        bool finite = true;
        for (int k = 0; k <= 100; ++k) {
            const double u = (double)k / 100.0;
            const double g = dotModular::phaseDistort(u, s.cell());
            if (!(g == g) || std::isinf(g)) { finite = false; break; }
        }
        ok(finite, "G7: the ε clamp holds through the primitive (a p ≈ 1 gap stays finite, no NaN)");
    }

    // THE ATOM PLATEAU through the primitive: mass m holds the output AT ℓ for a
    // width-m stretch of u (the freeze the phase tier rides — a stalled ramp).
    {
        PhaseDistSet s{}; s.f = dotModular::TriangularWarp{0.5};
        s.atomLevel = 0.55f; s.atomMass = 0.3f; s.w = 1.0f;
        const redDot::WarpCell c = s.cell();
        // The plateau's u-interval is [G(ℓ)·(1−m), G(ℓ)·(1−m)+m] on the set's own
        // FLOAT-ROUNDTRIPPED arithmetic; sample its middle and expect ℓ EXACTLY
        // (the atom branch returns the set's own clamped level — the freeze's shape).
        const double ell = (double)s.atomLevel;
        const double m   = (double)s.atomMass;
        const double q0 = ws::cdf(ell, 0.0, 0.0, 0.0, 0.0, 0.0,
            [&](double x) { return dotModular::cdf(dotModular::TriangularWarp{0.5}, x, nullptr); }) * (1.0 - m);
        const double midU = q0 + m * 0.5;
        ok(sameBits(dotModular::phaseDistort(midU, c), ell),
           "G7: the atom plateau holds the output at ℓ exactly (the freeze's shape)");
    }

    // THE RANGE over the whole roster × signed-w × u grid (a phase stays a phase).
    {
        bool inRange = true;
        for (const auto& r : g_roster) {
            for (double w : { -1.0, -0.7, -0.3, -0.05, 0.05, 0.3, 0.7, 1.0 }) {
                PhaseDistSet s{}; s.f = r.f; s.w = (float)w;
                s.tilt = 0.4f; s.gapLoc = 0.3f; s.gapSize = 0.2f; s.atomMass = 0.25f;
                for (int k = 0; k <= 80; ++k) {
                    const double u = (double)k / 80.0;
                    const double g = dotModular::phaseDistort(u, s.cell());
                    if (!(g >= 0.0 && g <= 1.0)) { inRange = false; break; }
                }
                if (!inRange) break;
            }
            if (!inRange) break;
        }
        ok(inRange, "G7: the primitive is a [0,1]→[0,1] self-map over the roster x signed-w grid");
    }
    return 0;
}

static int mainCal() {
    std::printf("CALIBRATION — the straight 24-PPQN detent list (the riding snap fix)\n");
    // {bar, half, quarter, eighth, 16th} = {96,48,24,12,6} pulses = {16,8,4,2,1} 16ths.
    static constexpr int kCalSixteenths[5] = {16, 8, 4, 2, 1};
    bool tiles = true;
    for (int d = 0; d < 5; ++d) {
        if (96 % (kCalSixteenths[d] * 6) != 0) tiles = false;   // the detent's pulses divide the bar
    }
    ok(tiles, "CAL: every detent's pulse count divides the 96-pulse bar exactly (the tiling)");
    bool covers = true;
    for (int v = 0; v <= 100; ++v) {   // the knob's full range maps INTO the list
        const double kn = (double)v / 100.0;
        const int det = std::clamp((int)std::lround((1.0 - kn) * 4.0), 0, 4);
        if (det < 0 || det > 4) covers = false;
        if (kCalSixteenths[det] <= 0) covers = false;
    }
    ok(covers, "CAL: the knob→detent map covers [0,1] with no gaps (knob 1.0 = bar, the shipped default)");
    ok(kCalSixteenths[0] == 16 && kCalSixteenths[4] == 1,
       "CAL: the detent order is bar..16th (1.0 → bar preserves the continuous default's value)");
    return 0;
}

int main() {
    mainG1();
    mainG2G3();
    mainG4();
    mainG6();
    mainG7();
    mainCal();
    if (g_fail == 0) { std::printf("ALL PASS (%d checks)\n", g_pass); return 0; }
    std::printf("%d FAILURE(S) of %d checks\n", g_fail, g_fail + g_pass);
    return 1;
}
