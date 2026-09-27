// CAPendingOrder.hpp — chronological pending-transform ordering (CA_DICE_COUNTER_MODEL.md
// "Pending transform ORDER: chronological, with rules").
//
// Today the boundary applies armed rows in ROW-INDEX order (verb-major: collapse, rotate,
// reflect, scatter). This refines it to CHRONOLOGICAL order: each row is stamped on its
// unarmed→armed transition with a monotonic counter, and the boundary applies in ascending
// stamp order. Ties (same-sample CV burst / chord of presses) break by ROW INDEX — i.e. exactly
// today's verb-major order — so nothing regresses; the change is only observable when the user
// deliberately SPACED two arms on the same stream.
//
// Rack-free / header-only so the ordering is unit-testable without the SDK. The module supplies
// the per-row armed[] + stamp[] arrays; this struct produces the apply order.
//
// Rules (all matter):
//   1. Stamp on arm (unarmed→armed only); re-arming an armed row keeps its original stamp.
//   2. Ties break by ROW INDEX (ascending) = today's verb-major order.
//   3. Order is only meaningful WITHIN a stream (cross-stream rows touch different src[]).
//   4. Out-of-axis (lock-deferred) rows stay armed and KEEP their stamp, so they commit in the
//      right relative order at the later unlock — they do NOT jump to the front. (The counter
//      therefore must NOT reset while any row remains armed; see the module's reset-when-empty
//      rule. Resetting at every boundary would give new post-boundary arms stamp 0 < the
//      persistent rows' stamps → persistent rows jump to the back = wrong.)
#pragma once
#include <cstdint>

namespace redDot {

struct CAPendingOrder {
    // Fill `out[]` with the indices of every armed row, in ascending-stamp order, ties by
    // ascending row index. Returns the count. `armed[r]` and `stamp[r]` are per-row over
    // [0, nRows). Collection is in ascending row order and the sort is STABLE, so equal stamps
    // retain row-index order = today's verb-major behaviour.
    //
    // Includes ALL armed rows (in-axis AND out-of-axis): the caller's apply loop skips
    // out-of-axis rows (they stay armed) so their stamp survives for the later commit. Including
    // them here keeps the relative order correct when they eventually fire.
    static int orderRows(int nRows, const bool* armed, const uint32_t* stamp, int* out) {
        int n = 0;
        for (int r = 0; r < nRows; ++r)
            if (armed[r]) out[n++] = r;
        // Stable insertion sort by stamp ascending. Ties (stamp equal) keep collection order
        // = ascending row index = the existing verb-major tie-break.
        for (int i = 1; i < n; ++i) {
            int key = out[i];
            int j = i - 1;
            while (j >= 0 && stamp[out[j]] > stamp[key]) {
                out[j + 1] = out[j];
                --j;
            }
            out[j + 1] = key;
        }
        return n;
    }
};

} // namespace redDot
