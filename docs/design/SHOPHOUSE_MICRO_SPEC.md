# Shophouse Micro -- tuning+scale scene modulation for the Colonnades family (post-V1)

A microtonal sibling of Shophouse: instead of switching (scale, root) slots within 12-TET, it switches
between full .dmtune tuning+scale slots, boundary-quantised, under CV. Makes TUNING ITSELF a modulation
destination. Post-V1 (V1 has the M4/engine-widening long poles; this must not expand that scope).

## The idea
Shophouse already IS the right architecture (MonsoonShophouseExpander.hpp): FOUR fronts, each a slot;
a CV input sampled AT THE PHRASE BOUNDARY selects the active front; the active slot writes to Monsoon's
shared state on the loop edge (ScaleList commitAtBoundary -- changes land on the loop edge, never
mid-phrase). Shophouse Micro widens what a "front" HOLDS: from (scale, root) within fixed 12-TET, to a
full .dmtune (cents[] + weight[] for N degrees).

A .dmtune IS a superset of a Shophouse front: today's front = "scale mask + implicit 12-TET tuning";
a .dmtune front = "scale mask + explicit N-degree tuning". So this is a generalisation of Shophouse,
not a new mechanism.

## Where it writes (the one architectural point)
Today Shophouse writes (scale, root) into Monsoon's ScaleManager. Shophouse Micro writes the active
front's full cents[] + weight[] into the SHARED TuningTable owned by Monsoon -- the SAME destination
the Colonnades/Duo faders write. So:
- Colonnades/Duo = the AUTHORING source (faders write the table).
- Shophouse Micro = the MODULATION source (scene switch writes the table).
- Both write the same shared TuningTable, boundary-quantised. The write-authority / WriteLedger work
  (MICROS_ENGINE_CLAUDE_CODE_GUIDE) already contemplates multiple writers; Shophouse Micro is one.

## ONE module, 12-or-24 chosen by connection target (Rodney)
Do NOT ship two modules. Shophouse Micro is ONE module that auto-configures its mode from what it
feeds:
- Attached to a Colonnades-backed TuningTable (N=12): fronts are 12-tone .dmtunes. FOUR fronts.
- Attached to a Colonnades Duo-backed TuningTable (N=24): fronts are 24-tone .dmtunes. TWO fronts.
- It reads the target's N (the TuningTable's tt.N, already the single source of degree count) and
  configures front count + .dmtune degree count accordingly. Same way the Micros already resolve N.
- Mode is chosen by the CONNECTION, not a user toggle -- if it's feeding a 12-degree table it's in
  12 mode, a 24-degree table -> 24 mode. If unattached, default/last mode; resolve on attach.

Why 4x12 vs 2x24 (forced by the format, not arbitrary): a front's payload is N cents + N weights, so a
24-degree front is twice the data of a 12-degree one. Four fronts of 24 is a lot of state + panel; two
fronts of 24 balances the total and matches the Duo's own 24=2x12 doubling logic. Two 24-tone fronts
also has musical sense: A/B maqam modulation, call-and-response between two 24-tone systems.

## Changes FROM the original Shophouse (Rodney)
1. **DROP the root-note setting.** Root-note was a 12-TET affordance (pick which chromatic pitch is
   degree 1). A .dmtune already encodes the full degree set with degree 0 as the root at 0 cents -- the
   file IS the pitch set, so a separate root offset is redundant and would fight the file. No ROOT_PARAM,
   no shutter-click-to-set-root. A front is just: which .dmtune.
2. **PANEL: alternating-colour cells, NOT piano keys.** The original Shophouse facades resemble a piano
   octave (the shutter-click-root metaphor). That bakes in the 12-TET-scale interpretation -- the SAME
   false interpretation dropped for the Lantern (no keyboard at N!=12) and the Micro panel (no
   primary/inflection split). Replace the piano-key facade with plain ALTERNATING-COLOUR cells (a
   neutral scene-slot strip). Consistent: microtonal surfaces never imply 12-TET structure, from panel
   to roll to this.
3. **Front content = a loaded .dmtune** (cents[] + weight[]) instead of (scale index, root). Loaded into
   the slot (baked into module state, travels with the patch -- self-contained), same as a front holds
   its scale today.

## What stays from Shophouse (the proven mechanism)
- FOUR-front street model (but N-derived: 4 at 12, 2 at 24).
- CV input sampled at the phrase boundary selects the active front.
- Boundary-quantised commit (ScaleList commitAtBoundary equivalent) -- tuning changes land on the loop
  edge, never mid-phrase. THIS IS THE KEY MUSICAL PROPERTY: tuning modulation quantised to phrase edges.
- CONSERVATION toggle (guide vs enforce) -- still meaningful (the weight mask can guide or enforce).
- Menu-free direct panel (the whole point of Shophouse -- faster than the menu dance).

## Open design decisions (leans, confirm at build)
- **Switch vs morph.** Lean SWITCH first (discrete, boundary-quantised, matches Shophouse exactly).
  Interpolating cents between fronts (a tuning that GLIDES from A to B) is musically gorgeous but a much
  bigger feature and raises "what does interpolating a weight mask mean." Add morph as a later option;
  switch is already valuable and is the natural extension.
- **Loaded vs referenced .dmtunes.** Lean LOADED (baked into slot state, patch self-contained), same as
  a Shophouse front holds its scale now.
- **Variant vs mode of Shophouse.** This is a SEPARATE module (Shophouse Micro), not a mode of the
  12-TET Shophouse -- mixing (scale,root) fronts and .dmtune fronts in one module muddies what a front
  is. The 12-TET Shophouse stays as-is.

## The payoff (why this is worth building)
Tuning modulation as a first-class, boundary-quantised musical gesture -- not "switch scales" (note
selection) but "modulate between tuning SYSTEMS" (12-TET -> maqam -> stretched, on the phrase edge,
under CV). Combined with the cross-tuning canon (two Monsoons, differently tuned, shared CA --
PITCH_PATCHABILITY 12/13), tuning itself becomes a modulation dimension in the correlation architecture.
Most microtonal tools treat tuning as static setup; this makes it a modulation target. Novel.

## Cross-refs
- src/MonsoonShophouseExpander.hpp -- the four-front + boundary-commit mechanism this generalises.
- TUNING_PRESET_FORMAT.md -- the .dmtune a front holds.
- MICROS_ENGINE_CLAUDE_CODE_GUIDE.md -- the shared TuningTable + write-authority this writes into.
- PITCH_PATCHABILITY_AND_DISTINCTION.md 12/13 -- cross-tuning canon this composes with.
