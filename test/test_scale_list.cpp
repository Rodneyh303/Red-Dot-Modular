// test_scale_list.cpp — the pure ScaleList data model (SHOPHOUSE_SPEC.md).
// Verifies boundary-quantised behaviour independent of any driver/UI: pending set freely,
// active changes ONLY on commitAtBoundary; the 1-entry-list = single-scale unification; wrap.
#include "ScaleList.hpp"
#include <cstdio>
static int fails=0;
static void chk(bool c,const char*m){ if(!c){printf("FAIL: %s\n",m);++fails;} }

int main(){
    // Default 4 slots.
    ScaleList L(4);
    chk(L.size()==4, "default 4 slots");
    chk(!L.isSingle(), "4-slot list is not single");
    L.setEntry(0, 0, 0);   // C major (say)
    L.setEntry(1, 0, 2);   // D major
    L.setEntry(2, 3, 9);   // some scale, A root
    L.setEntry(3, 1, 5);   // another, F root

    // 1. Pending can move freely; active does NOT change until boundary commit.
    chk(L.active()==0 && L.pending()==0, "start: active=pending=0");
    L.setPending(2);
    chk(L.pending()==2 && L.active()==0, "setPending(2): pending moves, active stays (boundary-quantised)");
    L.setPending(1);
    chk(L.pending()==1 && L.active()==0, "setPending(1): still not applied mid-phrase");

    // 2. commitAtBoundary applies the pending slot; returns true when the entry changed.
    bool changed = L.commitAtBoundary();
    chk(L.active()==1 && changed, "commit: active←pending(1), reports changed");
    chk(L.activeEntry().scaleIdx==0 && L.activeEntry().root==2, "active entry is D major (slot 1)");

    // 3. Commit with pending==active → no change, returns false (caller skips updateScaleMask).
    chk(!L.commitAtBoundary(), "commit with pending==active → no change");

    // 4. Two identical entries: index differs but VALUE same → commit reports NOT changed.
    L.setEntry(0, 0, 2);         // make slot 0 identical to slot 1 (D major)
    L.setPending(0);
    chk(!L.commitAtBoundary(), "commit to a different slot with identical value → value-unchanged");
    chk(L.active()==0, "  (index still advanced to the pending slot)");

    // 5. stepPending wraps (fwd/back drivers).
    ScaleList W(3);
    W.setPending(2); W.stepPending(1);
    chk(W.pending()==0, "stepPending wraps 2→0 (fwd)");
    W.stepPending(-1);
    chk(W.pending()==2, "stepPending wraps 0→2 (back)");

    // 6. Single scale = 1-entry list (unification + migration seed).
    ScaleList S(4);
    S.seedSingle(3, 7);
    chk(S.isSingle() && S.size()==1, "seedSingle → 1-entry list");
    chk(S.activeEntry().scaleIdx==3 && S.activeEntry().root==7, "seeded active entry correct");
    S.setPending(5);   // out of range → wraps to the only slot
    chk(S.pending()==0, "single list: any pending wraps to slot 0");
    chk(!S.commitAtBoundary(), "single list: nothing to switch to");

    // 7. root normalisation.
    ScaleList R(1);
    R.setEntry(0, 0, 14);   // 14 → 2
    chk(R.entry(0).root==2, "root normalised into 0..11");

    printf(fails? "\n%d FAILED\n" : "\nALL PASS (ScaleList boundary-quantised model)\n", fails);
    return fails?1:0;
}
