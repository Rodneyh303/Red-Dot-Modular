# BACKLOG — designed-but-unbuilt items (Rodney)

These are NOT started in CODE, but all three are already DESIGNED and DOCUMENTED. The design work is done;
what remains is implementation. Not urgent; captured so they're not lost. No crisis.

## Unified addressing system — FULLY DESIGNED (docs/design/UNIFIED_ADDRESSING_SYSTEM.md, 489 lines)
Status in that doc: design direction, POST-RELEASE, "major but conceptually clean — the next natural
consolidation" (like copula unified seq/quant, correlation unified per-voice value; pattern ADDRESSING is
the subsystem not yet unified). Has AUTHORITATIVE LOR definitions (L=length/#steps, O=offset/which step it
starts from = the window, R=rotation/phase-into-window-at-phrase-start) and the full global/voice/lane
NESTING semantics resolved (lane R referenced to voice step 1, voice R to global step 1). Worked through so
it isn't re-derived. NOT a "needs scoping session" item — it's specced; it needs BUILDING.

## Crab canon — FULLY DESIGNED (docs/design/CRAB_CANON_RECIPE.md, 260 lines)
Two Monsoons, shared Philox seed + shared CA, one forward one backward through the SAME material, crossing
at the midpoint = canon cancrizans from one stochastic source. Doc has BOTH:
- BUILDABLE TODAY: crab via dice-scrub crossfade (complementary scrub B=1-A, shared seed+CA). No new code.
- THE IDEAL (the feature gap): a user-facing COUNTER-ADDRESS primitive ("set counter=N") — the Philox
  counter is addressable internally but no control SETS it (only dice/dice-scrub MOVE it). The
  "Philox offset" feature = exposing counter-address so "fwd 0->N while back N->0" can be dialled directly.
That counter-address primitive is the real unbuilt feature; the dice-scrub recipe is the workaround today.

## Dotted notes — FULLY DESIGNED (docs/design/DOTTED_NOTES_IDEA.md, 42 lines)
Add dotted note values (dotted 1/2,1/4,1/8,1/16) to the NOTE VALUE set, gated in VARIATION like triplets
(noteVariationMask bits, base note always active). Table grows 8->12, ~4 new positions, low crowding.
Mirrors the triplet decision (added explicitly + gated rather than left emergent). Emergent dotted (1/8
legato-tied into 1/16) already exists as a bonus; this adds DETERMINISTIC dotted. Low-risk, fits
PatternEngine::varyNoteIndex exactly. "Try on a branch one day."

## Context
Surfaced at the end of the dynamic-MO / subpanel / Cantonment weekend. The big uncertainties (atom purpose
= dynamic-MO tail dependence; panel pattern = base+subpanels proven) are resolved. These three are
designed features awaiting build — the path is implementation, not design.
