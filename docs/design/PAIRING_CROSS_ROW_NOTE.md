# Pairing v1: cross-row placement + horizontal scaling (note for Claude Code)

## Status: ALREADY WORKING in master (v1). This note documents the capability, not new work.
Demonstrated: two IT+Lantern+T3 tuples running simultaneously (screenshot). The pairing mechanism
already supports the "adjacent default, global override" model Rodney wanted.

## The hybrid discovery model (as built)
Consumers (Lantern, Changi T3) resolve their Intertropical two ways, controlled by a followIT /
pair-number field (Lantern: followIT, persisted):
- followIT == 0  -> AUTO: nearest Intertropical either-side via adjacency (findMonsoonEitherSide-style
  expander walk). Zero configuration -- snap modules together and it works. The historical/default
  behaviour.
- followIT > 0   -> PAIR MATCH: the Intertropical whose pairId matches, ANYWHERE in the rack (rack-wide
  scan via APP->engine->getModuleIds()). Works across rows, across the whole patch -- NOT limited to
  adjacency.

So: adjacency is the zero-config default; the pair number is the global override for distant placement.
Best of both -- tactile snap-together for the simple case, rack-wide numbers for multi-row layouts.

## pairId assignment (as built -- thread-safety already handled)
- Intertropical auto-assigns pairId (lowest free) in process(), NOT in the constructor or an
  audio-locked path -- because APP->engine->getModuleIds() re-locks the engine mutex and would
  deadlock/hang if called from the wrong place. process() is the safe spot. (See Intertropical.cpp
  comment ~line 46-61.)
- pairId is immutable once assigned, persists in JSON, gaps allowed (never renumber). Consumers store
  the watched number and rebind on load.
- Clash detection: on load, if two ITs claim the same pairId, one reassigns.

## What this enables: HORIZONTAL SCALING across rows
Because followIT > 0 finds the Intertropical anywhere in the rack, you can:
- Place Monsoon + Straits + Intertropical in one row, and its Lantern + Changi T3 in ANOTHER row
  (set the consumer's follow-pair to the IT's number). Frees the main row of visualiser/breakout width.
- Run MANY tuples stacked across rows -- each IT numbered, each Lantern/T3 following by number. The
  "polyphony of arrangements" scales vertically down the rack as well as horizontally.
- Keep the audio/engine chain compact (Monsoon+Straits+expanders adjacent) while the display and
  breakout modules live wherever there's room.

## Recommended UX pattern (for T3 + future consumers, matching Lantern)
Give every IT consumer (Lantern, Changi T3, and any future one) the SAME followIT-style field:
- context-menu "Follow Intertropical": Auto (nearest) | #1 | #2 | ... (list live IT pair numbers).
- Default Auto so adjacency Just Works; the number override is opt-in for cross-row.
- Pair-colour swatch on both IT and consumer panels so a numbered pairing is visually legible
  (which Lantern follows which IT).

## For Changi T3 specifically
T3 should reuse the exact Lantern followIT mechanism (it's a find-and-read consumer, same as Lantern).
When T3 is built, give it a followIT field + context menu identical to Lantern's, so T3 can also sit
on a separate row from its Intertropical. No new discovery code -- reuse ui/IntertropicalPairing.hpp.

## Cross-instance note (relates to shared-CA idea)
The rack-wide scan is the SAME mechanism that would let two Monsoons on different rows share one
Change Alley (CA_SHARED_EXPANDER_OPTION_A.md) -- a rack-wide pairId/registry lookup rather than
adjacency. When building shared-CA, use the pairing scan model (already proven here), NOT an adjacency
walk, so cross-row sharing works. This is why other Rack developers achieve cross-rack connections:
registry scan (getModuleIds), not expander adjacency.
