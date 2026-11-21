# CC PROMPT — FIX Sands subpanel snap (doesn't eject intruders / move with host like Straits)

SYMPTOM (Rodney): Sands subpanels don't snap to the host's right, don't move with it, and don't eject a
module inserted between host and subpanel — unlike StraitsBase + lanes which do all three.

## ROOT CAUSE — the Sands snap abstraction (ui/SandsSnap.hpp) DIVERGED from the two Straits invariants
CC refactored the Straits resnap into a new abstraction (SandsSnap.hpp / sandsSnapStep / SandsSnapHostState)
instead of reusing the Straits mechanism, and the abstraction lost the TWO properties that make Straits work:

### Divergence 1 — runIds is REFRESHED FROM THE LIVE CHAIN (SandsSnap.hpp:25, :121-125)
Straits keeps orderedLaneIds_ as a STABLE OWNED LIST — recorded when a lane is added, members removed only
on deliberate delete, NEVER re-derived from the live chain. The Sands runIds is "refreshed from the live
chain" — so when a foreign module breaks the rightExpander chain, re-snapshotting runIds from the broken
chain DROPS the subpanels beyond the break. The host then forgets its own subpanels. FIX: make runIds a
STABLE owned list like orderedLaneIds_ — add on attach, remove only on deliberate delete/park, and do NOT
rebuild it from the live chain when the chain is broken. The live chain is used to DETECT breakage, never
to redefine ownership.

### Divergence 2 — reattach scans by SPATIAL PROXIMITY (sandsNearMiss), not by owned ID (SandsSnap.hpp:160-183)
Straits repositions its lanes BY STORED ID, UNCONDITIONALLY — it walks orderedLaneIds_ and setModulePosForce
each to its seat regardless of where it currently is. The Sands reattach scans all modules for ones that are
isOwnFamily AND within the sandsNearMiss window — so a subpanel shoved FAR (dragged away, or pushed outside
the near-miss window by the intruder) is NOT grabbed and is abandoned. FIX: reposition the owned subpanels
BY ID from runIds, UNCONDITIONALLY (setModulePosForce each owned ID to its computed seat), exactly like
resnapLanes() walks orderedLaneIds_. Do not gate reattach on spatial proximity — an owned subpanel is
reclaimed wherever it is.

## THE FIX IN ONE LINE
Make SandsSnap match the two StraitsBase invariants: (1) runIds is a STABLE owned list (not re-derived from
the live chain), (2) resnap repositions every owned ID UNCONDITIONALLY by setModulePosForce (not gated on
near-miss proximity). The eject-intruder behaviour falls out: repositioning owned subpanels onto their seats
via setModulePosForce shoves the intruder off (Rack's force-pos shoves bystanders), exactly as Straits does.

## BETTER OPTION — just REUSE StraitsBase's mechanism directly
The spec (CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md) said reuse the Straits snapping, don't reinvent.
The cleanest fix may be to factor StraitsBase's ACTUAL resnap (orderedLaneIds_ + resnapLanes() +
needsResnap + the drag-end/poll triggers) into a SHARED helper both StraitsBase and the Sands hosts call,
rather than maintaining a parallel SandsSnap that must be kept in sync. One snap implementation, two hosts
— the same "one mechanism, not two twins" principle used elsewhere. If that factoring is clean, prefer it.

## VERIFY (the three behaviours, as on Straits)
- Snap to right: attach a Distribution expander → it seats flush right of the host.
- Move with host: drag the host → the subpanel follows (repositioned by ID).
- Eject intruder: drag any module (own or foreign) between host and subpanel → within ~0.1 s it's shoved
  out and the subpanel re-seats. Test with the subpanel shoved FAR (divergence 2's failure case) — it must
  still be reclaimed.
- The existing Straits snap still works (if the shared-helper refactor is taken, Straits must stay green).
