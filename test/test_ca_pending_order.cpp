/**
 * test_ca_pending_order.cpp — chronological pending-transform ordering.
 * Header-lite (no Rack SDK): exercises CAPendingOrder::orderRows directly, and applies the real
 * transforms (ca::collapse / ca::rotateValues) in the produced order to verify the resulting pin
 * matrix matches the ARM order — including that collapse-then-rotate != rotate-then-collapse
 * (collapse is lossy, so order is audible), and that same-sample ties fall back to row-index
 * (verb-major) order = today's behaviour.
 *
 * Compile (see test/run_all.sh for the canonical line):
 *   g++ -std=c++17 -Itest -Isrc/dsp test/test_ca_pending_order.cpp -o /tmp/t_capo && /tmp/t_capo
 */
#include "../src/dsp/CAPendingOrder.hpp"
#include "../src/dsp/ChangeAlleyTransforms.hpp"   // ca::collapse / rotateValues (lossy composition)
#include <cstdio>
#include <cstring>

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)

using redDot::CAPendingOrder;
using dotModular::ca::collapse;
using dotModular::ca::rotateValues;

// Mirror ChangeAlleyV2Ids dims (Monsoon.hpp). rowId = verb*SIDES*TYPES + side*TYPES + type.
static constexpr int N_VERBS = 4, SIDES = 2, TYPES = 3, N_ROWS = N_VERBS * SIDES * TYPES;
static constexpr int V_COLLAPSE = 0, V_ROTATE = 1;
static int rowId(int verb, int side, int type) { return verb*SIDES*TYPES + side*TYPES + type; }

static void ident(uint8_t s[16]) { for (int v = 0; v < 16; ++v) s[v] = (uint8_t)v; }
static bool eq16(const uint8_t a[16], const uint8_t b[16]) { return std::memcmp(a, b, 16) == 0; }

// Apply a sequence of (verb) transforms to a copy of identity, in the given row order, and write
// the result. Only collapse + rotate are exercised (the lossy-composition case the spec calls out).
static void applySeq(const int* rowOrder, int n, uint8_t out[16]) {
    ident(out);
    for (int i = 0; i < n; ++i) {
        int r = rowOrder[i];
        int verb = r / (SIDES * TYPES);
        if (verb == V_COLLAPSE) collapse(out, 16, 4);
        else if (verb == V_ROTATE) rotateValues(out, 16, 4, 1);
    }
}

int main() {
    const int colRow = rowId(V_COLLAPSE, 0, 0);   // collapse, intra, rhythm
    const int rotRow = rowId(V_ROTATE, 0, 0);      // rotate,   intra, rhythm
    CHK(colRow < rotRow, "collapse row < rotate row (verb-major baseline)");

    // ── 1. Spaced arms: collapse first then rotate → order [collapse, rotate]; reverse arm order
    //    differs (collapse is lossy). ─────────────────────────────────────────────────────────
    {
        bool armed[N_ROWS] = {};  uint32_t stamp[N_ROWS] = {};
        armed[colRow] = true; stamp[colRow] = 0;   // armed first
        armed[rotRow] = true; stamp[rotRow] = 1;   // armed a sample later
        int ord[N_ROWS];
        int n = CAPendingOrder::orderRows(N_ROWS, armed, stamp, ord);
        CHK(n == 2, "two armed rows");
        CHK(ord[0] == colRow && ord[1] == rotRow, "collapse-then-rotate arm order preserved");

        uint8_t a[16], b[16];
        int fwd[2] = { colRow, rotRow };  applySeq(fwd, 2, a);   // collapse then rotate
        int rev[2] = { rotRow, colRow };  applySeq(rev, 2, b);   // rotate then collapse
        CHK(!eq16(a, b), "collapse-then-rotate != rotate-then-collapse (lossy: order is audible)");
    }

    // ── 2. Reverse spaced order: rotate first then collapse → order [rotate, collapse]. ───────
    {
        bool armed[N_ROWS] = {};  uint32_t stamp[N_ROWS] = {};
        armed[rotRow] = true; stamp[rotRow] = 0;   // armed first
        armed[colRow] = true; stamp[colRow] = 1;   // armed a sample later
        int ord[N_ROWS];
        int n = CAPendingOrder::orderRows(N_ROWS, armed, stamp, ord);
        CHK(n == 2 && ord[0] == rotRow && ord[1] == colRow, "rotate-then-collapse arm order preserved");
    }

    // ── 3. Same-sample tie: both stamps equal → row-index order (verb-major) = today's behaviour.
    //    Nothing regresses: a CV burst / chord of presses behaves exactly as now. ──────────────
    {
        bool armed[N_ROWS] = {};  uint32_t stamp[N_ROWS] = {};
        armed[colRow] = true; stamp[colRow] = 5;   // same sample → same stamp
        armed[rotRow] = true; stamp[rotRow] = 5;
        int ord[N_ROWS];
        int n = CAPendingOrder::orderRows(N_ROWS, armed, stamp, ord);
        CHK(n == 2 && ord[0] == colRow && ord[1] == rotRow,
            "tie -> row-index order (verb-major) = today's behaviour (no regression)");

        // The tie-broken order [collapse, rotate] must equal the row-index iteration order.
        int rowIdx[2] = { colRow, rotRow };
        uint8_t a[16], b[16];
        applySeq(ord, 2, a);
        applySeq(rowIdx, 2, b);
        CHK(eq16(a, b), "tie result == today's row-order result");
    }

    // ── 4. Re-arm keeps the original stamp (does NOT move later). Simulates latchRow's
    //    `if (!p.armed)` guard: collapse armed first (stamp 0), rotate armed (stamp 1), collapse
    //    re-armed — its stamp stays 0, NOT 2. So order is still [collapse, rotate]. ────────────
    {
        bool armed[N_ROWS] = {};  uint32_t stamp[N_ROWS] = {};
        armed[colRow] = true; stamp[colRow] = 0;   // first arm
        armed[rotRow] = true; stamp[rotRow] = 1;   // second arm
        // re-arm collapse: guard would skip re-stamping → stamp stays 0 (NOT bumped to 2)
        armed[colRow] = true; stamp[colRow] = 0;
        int ord[N_ROWS];
        int n = CAPendingOrder::orderRows(N_ROWS, armed, stamp, ord);
        CHK(n == 2 && ord[0] == colRow && ord[1] == rotRow,
            "re-armed row keeps original stamp (does not move later in the order)");
    }

    // ── 5. Out-of-axis row keeps its stamp and stays in the order (for the later commit). ─────
    //    orderRows includes ALL armed rows; the module's apply loop skips out-of-axis ones. Here
    //    we just confirm an armed row with an old stamp sorts correctly relative to newer arms.
    {
        bool armed[N_ROWS] = {};  uint32_t stamp[N_ROWS] = {};
        armed[colRow] = true; stamp[colRow] = 3;   // armed earlier (e.g. out-of-axis, deferred)
        armed[rotRow] = true; stamp[rotRow] = 7;   // armed later
        int ord[N_ROWS];
        int n = CAPendingOrder::orderRows(N_ROWS, armed, stamp, ord);
        CHK(n == 2 && ord[0] == colRow && ord[1] == rotRow,
            "earlier-stamped (deferred) row stays ahead of later arms (no jump-to-front)");
    }

    std::printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
