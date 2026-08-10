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

    // 8. CUSTOM variant (MONSOON_SCALE_AUTHORING D4): a slot can hold a loaded .dmtune scale mask.
    ScaleList C(4);
    C.setEntry(0, 0, 0);            // factory
    C.setEntryCustom(1, 0x0AB5);   // custom mask
    chk(!C.entry(0).isCustom, "slot 0 factory (not custom)");
    chk(C.entry(1).isCustom && C.entry(1).customMask==0x0AB5, "slot 1 custom mask stored");
    // setEntry on a custom slot reverts it to factory.
    C.setEntry(1, 2, 3);
    chk(!C.entry(1).isCustom, "setEntry clears custom flag");
    // all-off mask forced non-silent.
    C.setEntryCustom(2, 0x0000);
    chk(C.entry(2).isCustom && C.entry(2).customMask==0x0001, "all-off custom mask forced non-silent");
    // boundary commit + equality across the variant: factory vs custom never compare equal.
    ScaleList V(2);
    V.setEntry(0, 0, 0);           // factory C major-ish
    V.setEntryCustom(1, 0x0FFF);   // custom all-on
    V.setPending(1);
    chk(V.commitAtBoundary(), "commit factory→custom reports changed");
    chk(V.activeEntry().isCustom && V.activeEntry().customMask==0x0FFF, "active is the custom entry");
    // two custom slots with the SAME mask → value-unchanged on commit.
    ScaleList D(2);
    D.setEntryCustom(0, 0x0555);
    D.setEntryCustom(1, 0x0555);
    D.setPending(1);
    chk(!D.commitAtBoundary(), "commit between identical custom masks → value-unchanged");
    chk(D.active()==1, "  (index still advanced)");

    printf(fails? "\n%d FAILED\n" : "\nALL PASS (ScaleList boundary-quantised model + custom variant)\n", fails);
    return fails?1:0;
}
