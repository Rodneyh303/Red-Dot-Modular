# Scales to add + quantizer modes -- TODO (own session)

## Scales to add (MonsoonScaleManager.cpp MONSOON_SCALES -- currently 28)
Priority, with the Singapore East/West / demographic-completeness rationale:

1. SLENDRO -- PRIORITY. Completes the GAMELAN PAIR (Pelog is already in; Slendro is the other gamelan
   tuning -- having one without the other is like Major without Minor). Directly strengthens the
   heterophony/gamelan/Singapore anchor. 12-TET approx: commonly {0,2,5,7,9} or {0,2,4,7,9} (real
   Slendro is ~equipentatonic, ~240c steps, NOT 12-TET -- ships as an approximation, same as Pelog
   already does; label honestly).
2. CHINESE PENTATONIC (Gong) -- Singapore's largest cultural group is Chinese, and the set has
   Japanese + Indonesian + Indian but nothing explicitly Chinese. Note: {0,2,4,7,9} = same intervals
   as Pentatonic Major already present -- so this is a NAMED/framing add (the cultural label carries
   the story), not new pitch content. Legit for the demographic-completeness statement; be aware it's
   a relabel not a new sound. (Could add other Chinese modes -- Zhi/Yu etc. -- for actual variety.)
3. A CARNATIC (S. Indian) raga -- Bhairav is N. Indian/Hindustani; a Carnatic scale complements it and
   matches Singapore's Tamil population. Rounds the Indian representation.

Result = a scale set whose STRUCTURE mirrors Singapore's cultural composition (Chinese / Malay-
Indonesian / Indian / Western) -- the scale list itself tells the Singapore story, like the module
names do. Curate: prefer scales that extend the East/West span or complete a pair; resist bloating to
a generic 40+ "scale library" (curation is the pitch, not exhaustiveness).

HONEST BOUND (applies to Pelog, Slendro, and the Japanese/Indian scales): these are 12-TET
approximations of non-12-TET tunings. Correct + standard for a semitone-mask system; the REAL tunings
would need microtonal / Scala / per-note-cents support (see quantizer note below).

## Quantizer modes -- COME BACK TO (Rodney flagged)
Revisit the quantiser/scale-enforcement modes. Open questions to work through in that session:
- Current model: scale MASK gates the READ (getSemitoneWeight returns 0 for out-of-scale when locked);
  non-destructive (faders keep values, dim in UI). That's the LOCK behaviour. What other modes?
- Candidate modes to consider: (a) hard quantise (snap out-of-scale to nearest in-scale) vs current
  (b) probabilistic mask (zero the weight, current). (c) "nudge to scale" -- redistributeWeights() is
  ALREADY retained as a static helper for a possible USER-INVOKED nudge (not auto-applied) -- wire it
  to a menu/gesture? (d) nearest-up / nearest-down / nearest bias for a hard-quantise mode.
- Microtonal / true tunings: the 12-TET-approximation bound above is the real limit. A future
  quantiser mode could support per-note cents / Scala import so Pelog/Slendro are the ACTUAL tunings,
  not approximations -- would make the East/West gamelan claim literal rather than approximate. Big
  feature; note as a direction, not near-term.
- Decide interaction with Shophouse conservation + the transpose escape-hatch (transpose is
  deliberately NON-key-aware; a quantiser mode must not accidentally re-conform transposed output --
  see INTERTROPICAL transpose DECIDED note).
