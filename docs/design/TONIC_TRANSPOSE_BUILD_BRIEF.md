# BUILD BRIEF: transposable scales -- tonic designation + root-relative .dmtune (for CC)

> **STATUS: BUILT — Monsoon + Shophouse only** (Rodney's scope: Monsoon edits 12-TET exclusively;
> the Colonnades/Duo references in this brief are an ERROR — microtonal .dmtune from Colonnades/Duo has
> NO tonic and does not transpose). Shipped: rotateMask12/normaliseToTonic (ScaleMaskArbiter.hpp,
> tested); TuningPreset `transposable` flag (save root-relative / load root-transpose); Monsoon tonic
> via right-click "Set as tonic" on a SEMI fader + red cap + live-root transposition of authoredMask;
> Shophouse transposable-custom slots transpose by the front root (shutter-click), red root shutter,
> persisted. STEP 2's Colonnades right-click is N/A. Awaiting Rodney's Rack build + verify.

Consolidated build brief. Pulls the tonic/transposition rulings from MONSOON_SCALE_AUTHORING_DIRECTION
+ COLONNADES_PANEL_LIFT rounds 10/11 into one build truth. Where those and this differ, THIS is truth.
Post-V1 (part of the Monsoon-scale-authoring arc); self-contained relative to the engine.

## The goal
Monsoon-authored (and Colonnades-authored) scales should be TRANSPOSABLE like built-in scales: a scale
is a root-relative PATTERN, and a root control places it (C Major vs F Major = same pattern, different
root). Achieve this WITHOUT storing an absolute root in the file (option B).

## Model: option B -- root-relative mask + live root control (NOT a stored root)
- The .dmtune (scale-only, 12-TET) stores a ROOT-RELATIVE enabled mask (tonic normalised to degree 0) +
  a "transposable / scale-only" FLAG. It does NOT store an absolute root value.
- Transposition happens via the LIVE root control (Monsoon's scale-context-menu root; Shophouse's
  per-front root) -- exactly how built-in scales transpose. One root, always the live control, for
  built-ins AND user scales -> no two-roots conflict.

## STEP 1 -- a per-module TONIC value
- Add a persisted tonic degree index (int, 0..N-1, or -1 = none) to the authoring modules
  (Colonnades/Duo, and Monsoon when scale-authoring).
- Exclusive: exactly one tonic (or none). Setting a new tonic clears the previous.
- Only an ENABLED, in-tuning degree can be the tonic.

## STEP 2 -- the tonic GESTURE (family-appropriate, not identical everywhere)
- **Colonnades / Duo / Monsoon authoring**: RIGHT-CLICK an enabled fader -> context-menu item
  "Set as tonic" (on the current tonic: "Unset tonic"). No existing gesture to reuse there; right-click
  is space-free + discoverable + doesn't collide with the enable band (round 9).
  - CC: APPEND "Set as tonic" to the fader's PARAM (weight) context menu -- do NOT replace the standard
    param entries (initialize/unmap/etc). Known VCV append pattern.
- **Shophouse (loading a scale-only .dmtune)**: use the EXISTING Shophouse tonic gesture -- click a
  shutter -> sets that front's root (MonsoonShophouseExpander.cpp:16,112-120). Factory scales use this;
  .dmtune scales use the SAME. Do NOT add the right-click to Shophouse. Consistency within Shophouse
  wins.

## STEP 3 -- the tonic INDICATOR
- **Colonnades / Duo / Monsoon**: a small border/cap/tick on the tonic fader, in dot.modular red
  (#d4001a) -- minimal space, a mark on the existing fader, orthogonal to lit/dimmed/greyed states.
- **Shophouse**: already done -- the root shutter renders Singapore red (MonsoonShophouseExpander.cpp:148,
  isRoot -> red "open" shutter). Reuse for .dmtune scales; nothing new.

## STEP 4 -- .dmtune SAVE (normalise to root-relative)
- On save of a scale-only .dmtune: ROTATE the enabled mask so the designated tonic sits at degree 0
  (root-relative), write enabled[N] (root-relative) + the transposable/scale-only FLAG.
- Do NOT write an absolute root.
- If no tonic designated: either default tonic = degree 0 (mask saved as-is) or omit the flag (absolute
  mask, non-transposable). Lean: no tonic -> not flagged transposable (absolute mask).
- (Recall: scale-only .dmtune carries NO cents beyond the implied 12-TET and NO weight -- enabled +
  flag + n only. See TUNING_PRESET_FORMAT / SHOPHOUSE_MICRO_SPEC.)

## STEP 5 -- .dmtune LOAD (transpose via the live root control)
- Load the root-relative mask; the LIVE root control (Monsoon scale-menu root / Shophouse front root)
  transposes it -- identical to a built-in scale. Choosing root F rotates the pattern to F.
- The transposable FLAG tells the loader to treat it as root-relative (apply the root control) vs an
  absolute mask.

## Consistency with the microtonal .dmtune (do NOT reconflate)
- MICROTONAL .dmtune (Colonnades/Duo/Shophouse Micro): absolute cents, root IMPLICIT (degree 0 @ 0c),
  NO transposable flag -- transposing an arbitrary tuning = re-tuning, not scale-transposition. UNCHANGED.
- MONSOON SCALE-ONLY .dmtune: root-relative mask, transposable flag, transposed by the live root control.
- Same word "root", different jobs: microtonal = fixed pitch anchor (implicit); 12-TET scale =
  transposition reference (the live control). Not a contradiction -- 12-TET scales transpose, arbitrary
  tunings don't.

## The payoff
- User scales transpose like built-ins (one file = C Major AND F Major, via the root control).
- Shophouse: one user scale in four fronts at four roots = four-key modulation of the user's OWN scale.
- Reuses the existing root controls; adds only a tonic value + the right-click (Colonnades side) and the
  save-normalise/load-transpose.

## VERIFY
- Author a scale on Colonnades, mark a tonic (right-click), save; the mask saves root-relative (tonic
  at degree 0) + transposable flag; no absolute root in the file.
- Load it, change the root control -> the scale transposes (like a built-in); the tonic fader shows the
  red mark at the current root.
- Load the same .dmtune into a Shophouse front, click a shutter -> front root sets, scale transposes;
  four fronts at four roots = four transpositions of the one user scale.
- Microtonal .dmtune still has no root/flag and does not transpose (unchanged).

## Cross-refs
- MONSOON_SCALE_AUTHORING_DIRECTION.md (option B rationale), COLONNADES_PANEL_LIFT_SPEC rounds 10 (N)
  + 11 (right-click tonic), MonsoonShophouseExpander.cpp:16/112-120/148 (existing shutter-click root +
  red indicator to reuse), TUNING_PRESET_FORMAT / SHOPHOUSE_MICRO_SPEC (scale-only .dmtune = enabled +
  flag, no cents/weight), dot.modular red #d4001a.
