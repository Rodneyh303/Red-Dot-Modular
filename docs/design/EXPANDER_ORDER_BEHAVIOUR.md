# Expander discovery — order behaviour (review notes)

MonsoonExpanderManager::update() walks the expander chain each frame to cache one pointer per
expander type. This documents exactly how ORDER and ADJACENCY affect what gets found, so
"occasional order differences" are understood as intentional rules, not surprises.

## The walk
- From the parent Monsoon, scan LEFT fully (up to depth 12), then scan RIGHT fully.
- Both sides are ALWAYS scanned (no early-out). Discovery is therefore ORDER-STABLE: the result
  does not depend on which side happened to satisfy some "found everything" condition first.
  (Previously an early-out gated on allTypesFound(), which referenced two never-set deprecated
  pointers so it was permanently false — dead code that made the walk always-full-depth by
  accident. Removed; the walk is now full-depth by DESIGN.)

## The two intentional order/adjacency rules
1. **Adjacency — stop at first foreign module (Rule 1).** The scan stops on the FIRST module that
   isn't a recognised Monsoon expander. So an expander only attaches if it sits in an UNBROKEN
   chain of recognised modules out from Monsoon. Placing a non-suite module (mult, VCA, blank)
   BETWEEN Monsoon and an expander makes that expander — and everything beyond it — invisible.
   This is standard Rack expander adjacency; it's a constraint to be aware of, not a bug.
2. **First-match-wins per type (left before right).** Each type caches the FIRST instance found.
   The left side is scanned before the right, so if TWO expanders of the same type are attached
   (one left, one right, or two on a side), the LEFTMOST-reachable one is authoritative; the
   other is ignored. Reordering which side/position a duplicate sits on changes which instance
   wins. This is the most likely source of "expander order behaviour differences" when duplicates
   exist. Single-instance-per-type setups are fully order-independent.

## Practical guidance
- Keep each expander type to ONE instance per Monsoon (duplicates are resolved by position, which
  surprises).
- Keep expanders in a contiguous chain (no foreign modules interrupting) or they drop off.
- Otherwise, order is stable and side-independent for the single-instance case.

## Not order-dependent
- Which SIDE (left/right) a single expander sits on does not matter — both sides are scanned and a
  single instance is found either way.
- The per-frame re-scan means hot-plugging/reordering is picked up next frame.
