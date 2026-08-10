# Direction: Monsoon custom-scale authoring + user scales in Shophouse (post-V1)

> **STATUS: BUILT** (see plans/monsoon_scale_authoring.md). Shipped: an explicit-mask arbiter
> (override > authored > factory > all-12, src/dsp/ScaleMaskArbiter.hpp, tested); a Monsoon enable-band
> that authors `authoredMask` (matches Colonnades — fader-dim = membership, no per-degree LED, inert
> when the scale is owned elsewhere); Monsoon Save/Load scale `.dmtune` (round-trips its own files);
> and the regular Shophouse loading a user `.dmtune` per front (variant ScaleListEntry, Option B: ALL
> commits are non-destructive boundary-quantised overrides, the arbiter is the revert cache). Sikit
> untouched. guide/enforce reuses the existing `lockScaleNotes`. Awaiting Rodney's Rack build + verify.

Two ideas (Rodney) that converge into one clean unification: give a plain Monsoon (no Colonnades/Duo)
the ability to AUTHOR custom scales, and let Shophouse load those user-authored scales into slots
alongside its factory scales -- reusing the existing .dmtune + base/override architecture, with Sikit
staying a separate tuning-only concern. Post-microtonal-V1 direction, not a build spec.

## The gap this fills
A plain Monsoon's scale comes from Shophouse (FACTORY scales) or nothing (all 12 active). There is no
way to HAND-AUTHOR a custom scale on the Monsoon, and no way to feed a user scale into Shophouse. These
ideas add both.

## The unification (Rodney's synthesis)
1. **Monsoon authors a .dmtune-WITHOUT-cents** -- just the enabled mask over its 12 fixed semitones.
   A scale, not a tuning: .dmtune with n=12, enabled[], cents = implied 12-TET (ignored/omitted). The
   SCALE-ONLY subset of the format. Authoring UI = the Colonnades enable-band + N control (idea 2),
   ported to the Monsoon note-fader row.
2. **Shophouse optionally loads that user .dmtune into a slot** -- instead of only its built-in factory
   scales. A Shophouse slot can hold EITHER a factory scale (as now) OR a user-authored .dmtune
   scale-mask. Same slot, same boundary-quantised switch, same override-of-Monsoon mechanism -- only
   the SOURCE of the scale widens (factory list -> factory list + user files).
3. **Sikit stays tuning-only** -- a separate concern, untouched. Tuning (Sikit) and scale (Monsoon
   authoring / Shophouse) remain cleanly separated, the same split as everywhere else.

So the pipeline "author on Monsoon -> save .dmtune -> load into Shophouse slot -> modulate" is built
ENTIRELY from parts that already exist. .dmtune becomes the universal scale/tuning interchange artifact
across the whole collection.

## Why this beats the alternative (Sikit growing scale-authoring)
Rodney's idea 1 first form was: Sikit loads 12-note .dmtune, operates on Monsoon faders, gets its own
guide/enforce. REJECTED in that form, because:
- It makes Sikit AND Shophouse both scale-authorities for the Monsoon -> two owners of the scale mask,
  no arbitration -> the "two parallel schemes is where off-by-ones breed" failure mode (cf the accent/
  CV routing lesson).
- It duplicates Colonnades' role and muddies Sikit's one clean job (tuning), raising "why isn't it just
  Colonnades?".
The unification above gets idea 1's benefit (author full tuning+scale on a plain Monsoon = Sikit tuning
+ Monsoon scale-authoring) WITHOUT the conflict: two clean single-owner pieces, not a fused one.

## Ownership / arbitration (already solved -- reused, not reinvented)
The scale mask on the Monsoon has ONE base author and an override, exactly like Colonnades <-> Shophouse
Micro:
- **Monsoon authors the BASE scale** (its enable-band mask), OR all-12 if unauthored.
- **Shophouse OVERRIDES** when a slot/front is active (boundary-quantised), reverts on detach.
- Same base/override/revert model already designed for Colonnades <-> Shophouse Micro. No new
  arbitration -- the Monsoon takes the base-author role Colonnades has; Shophouse is the override layer.
So there is NO two-owners problem here: base (Monsoon) + override (Shophouse) is a solved relationship.

## guide/enforce
Already exists as Shophouse's conservation toggle (guide = mask dims/advises; enforce = mask zeroes
out-of-scale at read). If the Monsoon authors its own scale, it wants the SAME guide/enforce choice for
its own mask -- but that's ONE guide/enforce concept applied to the active mask, a Monsoon setting
consistent with existing conservation, NOT a new per-module switch. Do not invent a second enforce.

## What each piece needs (direction, not spec)
- **Monsoon scale-authoring**: port the Colonnades enable-band (round 9) + N control (round 10) onto
  the Monsoon note-fader row. N=12 fixed (Monsoon is 12-TET), so N is really just the enable mask here
  (no tuning-size variation -- the "greyed beyond N" state may collapse to "all 12 present, some
  disabled"). Output: an enabled[12] mask the Monsoon applies to its own faders (base scale).
- **Save**: Monsoon writes a scale-only .dmtune (n=12, enabled[], no cents / implied 12-TET).
- **Shophouse slot load**: extend Shophouse slots to accept a user .dmtune (scale-only) as an
  alternative to a factory scale. For scale-only files, only enabled[] is used (Shophouse is 12-TET,
  ignores cents). Boundary-quantised switch unchanged.
- **Sikit**: untouched. Tuning only.

## The payoff
- Plain Monsoon gains custom-scale authoring (hand-authored OR loaded), which it never had.
- Regular Shophouse gains USER scales, not just factory presets -- a real capability, for free, by
  loading the scale-only .dmtunes the Monsoon can now write.
- .dmtune becomes the one interchange artifact for scales AND tunings across the collection (scale-only
  subset for 12-TET Monsoon/Shophouse; full cents+enabled for Colonnades/Duo/Shophouse Micro).
- The three-tier identity stays legible: Sikit = tuning, Colonnades/Duo = tuning+scale+weight,
  Monsoon = weight + (now) scale-authoring; Shophouse = scale MODULATION (factory + user).

## Scope / sequencing
Post-microtonal-V1. The microtonal build (Colonnades/Duo/Sikit/Shophouse Micro/.scl/.dmtune/enabled/N)
comes first. This is a natural NEXT arc that reuses that architecture on the 12-TET side. Captured as
direction; not a build spec.

Cross-ref: COLONNADES_PANEL_LIFT_SPEC rounds 9 (enable band) + 10 (N), SHOPHOUSE_MICRO_SPEC (base/
override arbitration, .dmtune slot loading, the format), TUNING_PRESET_FORMAT (.dmtune, scale-only =
the no-cents subset), SHOPHOUSE conservation (guide/enforce).

## Transposable Monsoon scales: option B (root-relative mask + existing root control), not stored root

Built-in scales are transposable: a scale is an interval PATTERN, and the root-note control (already in
the scale context menu) places it (C Major vs F Major = same pattern, transposed). A Monsoon-authored
.dmtune scale should behave IDENTICALLY. Two ways to store this:

- **A**: the .dmtune stores its own root; the file remembers "this was F Major".
- **B**: the .dmtune stores a ROOT-RELATIVE mask (tonic at degree 0) + a transposable FLAG; the existing
  live root control transposes it at load, exactly like a built-in.

**DECISION: B.** Rodney's setup makes B fit: you already have a live root control (scale context menu)
chosen at load time -- that IS the built-in transposition mechanism. Storing a root in the file (A)
would DUPLICATE that control and create a two-roots conflict (which wins on load -- file or control?).
B has ONE root, always the live control, for built-ins AND loaded user scales alike. No conflict, no
duplication, consistent with built-ins.

### At CREATE (author the scale on Monsoon)
- Author the mask (enable-band).
- DESIGNATE THE TONIC: a "mark as tonic" gesture on one enabled degree. This is the one new authoring
  affordance. It does NOT bake in an absolute pitch -- it ORIENTS the pattern.
- SAVE: normalise the mask to ROOT-RELATIVE (rotate so the marked tonic sits at degree 0), write
  enabled[12] (root-relative) + flag "monsoon scale-only, transposable". NO absolute root stored.

### At LOAD (into Monsoon, or a Shophouse slot)
- The mask loads as a root-relative pattern.
- The EXISTING scale-context-menu root control places/transposes it -- identical to how it transposes a
  built-in. Choose F -> pattern transposes to F. No new control, no file-stored root, no conflict.

### The flag (needed; the stored root is NOT)
- FLAG: mark the .dmtune as "scale-only, root-relative, transposable" so the loader treats it like a
  built-in (apply the live root control) rather than an absolute mask.
- A microtonal .dmtune does NOT have this flag -- its cents are ABSOLUTE pitches, not a transposable
  12-TET pattern (transposing an arbitrary tuning = re-tuning, not scale-transposition). So:
  - microtonal .dmtune: absolute cents, no root, no transposable flag (root implicit = degree 0 @ 0c,
    per the earlier Shophouse Micro / Colonnades ruling -- UNCHANGED).
  - Monsoon scale-only .dmtune: root-relative mask, transposable flag, transposed by the live root
    control (NEW, this section).
  Same word "root" plays genuinely different jobs: microtonal = fixed pitch anchor (implicit);
  Monsoon-scale = transposition reference (the live control). Not a contradiction -- 12-TET scales are
  transposable in a way arbitrary tunings are not.

### Summary
- CREATE: author mask + mark tonic -> save root-relative mask + transposable flag.
- LOAD: existing root control transposes, like a built-in.
- File stores: enabled[12] (root-relative) + transposable flag. NOT a root value.
- Reuses the existing root control; adds only a "mark tonic" authoring gesture. No two-roots conflict.

Cross-ref: the scale context-menu root control (existing, the transposer), the enable-band authoring
(Colonnades round 9 ported to Monsoon), the microtonal .dmtune no-root ruling (SHOPHOUSE_MICRO_SPEC --
unchanged; this is the 12-TET-scale-only complement).
