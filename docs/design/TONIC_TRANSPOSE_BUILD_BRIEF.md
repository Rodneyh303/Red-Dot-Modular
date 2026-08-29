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

## OPEN INTERACTION (Rodney): transpose vs an attached Sikit tuning -- transpose is 12-only

Question: author a scale-only .dmtune, set tonic, enable transpose, AND attach Sikit (supplies tuning
cents). Does transpose still make sense?

### Code fact: transpose is a 12-SEMITONE mask rotation
rotateMask12 (ScaleMaskArbiter.hpp:16,33-36) is hard-coded to 12: "12-bit, bits 0..11 = semitones C..B",
(root+interval)%12, "semis taken mod 12". The transpose/tonic mechanism lives entirely in the 12-semitone
mask domain -- it ASSUMES a 12-degree equal-step grid. Monsoon.cpp:643 + MonsoonShophouseExpander.cpp:150
both call rotateMask12.

### So the answer depends on what Sikit supplies
- Sikit = 12-degree tuning, EQUAL (12-TET): rotate = genuine transposition. Makes sense as normal.
- Sikit = 12-degree tuning, UNEQUAL (12 non-equal cents): rotateMask12 still works mechanically (12
  slots), BUT rotating across unequal cents changes the INTERVALS -> it's MODAL ROTATION, not
  transposition. Musically correct for maqam-like tunings, but "transpose" is the wrong word.
- Sikit = N != 12 degrees (e.g. 24-degree maqam tuning): MISMATCH. rotateMask12 assumes 12, tuning has N.
  The 12-bit mask and the N-degree tuning DON'T ALIGN -> transpose doesn't just change meaning, it may
  not correctly apply at all. The real gap.

### Honest verdict
- TONIC (designate which degree is home): still meaningful on ANY N. Keep.
- TRANSPOSE (rotate the pattern to a new root): built as 12-only. Composes with a 12-degree tuning (as
  MODE if unequal); does NOT map to a non-12 Sikit tuning as built. So "does transpose make sense" ->
  on 12-degree tunings yes (as mode when unequal); on non-12 tunings NOT as currently built.

### Design question to resolve (park; brief was written 12-TET-framed, predates full Sikit picture)
1. Disable/grey transpose when a non-12 tuning is attached (honest: it's a 12-TET op). Simplest.
2. Generalise to degree-space: rotateMaskN (rotate the N-degree mask by degree, any N). Works on any
   tuning -- but then it's ALWAYS modal rotation on unequal tunings; relabel "root/mode" not "transpose".
3. Relabel the control "ROOT" (tuning-agnostic, always true: sets where degree-0 sits; means
   transposition on equal tunings, modal rotation on unequal) -- the brief already built it as a "live
   root control", so "root" is the honest name; "transpose" is the only thing that breaks.
LEAN: (3) name it root + (2) generalise to N-degree rotation, so it's correct on any Sikit tuning and
honestly named. At minimum (1): disable transpose on N!=12 until generalised.

Caveat: I did not find the transpose<->Sikit read-path composition in MicroTuning.cpp (grep empty) --
whether a non-12 Sikit tuning even reaches rotateMask12, or is blocked upstream, is UNVERIFIED. Confirm
the N!=12 path before deciding disable-vs-generalise.

Cross-ref: ScaleMaskArbiter.hpp (rotateMask12, 12-only), Monsoon.cpp:643 + MonsoonShophouseExpander.cpp:150
(callers), SHAREABILITY_ANALYSIS/Sikit (tuning source, may be N!=12), DOC_PRIORITISATION (this makes
TONIC_TRANSPOSE a genuine Tier-1 open item, not just "partial").

## RESOLVED for Sikit (Rodney + code): Sikit is 12-ONLY, so no mismatch -- gap moves to the non-12 path

Sikit only loads 12-note Scala files. Confirmed Sikit.cpp: tt.N = N_DEGREES = 12 (:29 "Phase 1"); :158
"EXACTLY 12 degrees (retunes Monsoon's fixed 12-degree system)"; :161 loader validator
`[](int n){ return n == 12; }` REJECTS non-12 .scl with ":162 'Sikit reads 12-note .scl files only. For
non-12 tunings, use a Micro expander'"; :33 default = 12-TET exactly.

### This collapses the three-way problem to two-way
The N != 12 MISMATCH case CANNOT occur through Sikit -- rotateMask12 (12) and Sikit (always 12) always
agree on N=12. So attaching Sikit, transpose ALWAYS applies correctly. Remaining nuance is only:
- Sikit EQUAL (default 12-TET): transpose = genuine transposition. Correct word.
- Sikit UNEQUAL (12-note non-equal .scl, 12 custom cents): transpose = MODAL ROTATION (rotating across
  unequal cents changes intervals). Mechanically fine (still 12 slots); musically "mode" not "transpose".
  Arguably relabel "root", but NO mechanism gap.

### The N!=12 gap lives on the MICRO/non-12 path, NOT Sikit
Non-12 microtonal tunings are routed by design to Shophouse Micro / Emerald Hill / Colonnades (Sikit's
own error message says so). So the transpose<->tuning interaction splits by WHICH MODULE supplies tuning:
- Via SIKIT -> always 12 -> rotateMask12 always aligns -> transpose works; only name/meaning shifts
  (transposition on equal, mode on unequal). NO mechanism gap. RESOLVED.
- Via MICRO / EMERALD HILL (non-12) -> THIS is where N!=12 lives, where transpose-generalisation
  (rotateMaskN) or disabling matters. The open design question (options 1/2/3 above) applies HERE, not to
  Sikit.

### Net
Rodney's original question ("attach Sikit, does transpose still make sense?") -> YES. Sikit being 12-only
means transpose always applies; the only nuance is modal-vs-transposition wording on an unequal 12-note
tuning. The scary structural mismatch was never reachable via Sikit. The rotateMaskN generalisation
question is real but belongs to the non-12 Micro/Emerald-Hill path.

Supersedes the "OPEN INTERACTION" section above FOR SIKIT (no gap there); that section's design question
(disable vs generalise) is re-homed to the non-12 Micro path.

Cross-ref: Sikit.cpp:29,158,161 (12-only), the OPEN INTERACTION section (now scoped to non-12 Micro/
Emerald Hill), MONSOON_SCALE_AUTHORING_DIRECTION (Sikit tuning-only 12; Micro/Shophouse = non-12/custom),
DOC_PRIORITISATION (TONIC_TRANSPOSE: Sikit path fine, non-12 path is the Tier-1 open bit).

## Rotate-Sikit option + Scala reference-pitch clarification (Rodney)

### Scala files are relative to 1/1, NOT to C (correcting the common assumption)
A .scl file lists INTERVALS relative to the 1/1 (unison) -- ratios/cents ending on the period (2/1). It
defines the scale SHAPE (each degree's distance from the tonic), NOT absolute pitch. The C/absolute-pitch
association lives in a SEPARATE .kbm (keyboard map) file, which pins a reference freq + which key = 1/1.
So: .scl = intervals rel to 1/1 (pitch-neutral); .kbm = where 1/1 lands (ties to C/A/etc).
In Sikit: loading a 12-note .scl = 12 intervals rel to 1/1, degree 0 (the 1/1, the locked root plate) =
Monsoon's root. "C" is just the conventional NAME for the root slot -- a labelling choice, not dictated by
the .scl. Precise: .scl is relative to 1/1 (degree 0), and Sikit maps degree 0 = root.

### Rotate-Sikit option (Rodney's idea) -- musically = MODAL ROTATION of the tuning
Sikit holds 12 cents, degree 0 = 1/1 = root. "Rotate Sikit" = cyclically shift which cents-offset sits on
which degree, so a DIFFERENT degree becomes 1/1/root. Because Sikit is a TUNING (unequal cents), rotating
it = MODAL ROTATION (which intervals fall where) -> a different MODE of the same tuning. Coherent and
musically meaningful: the tuning-domain complement to mask rotation. "Same scale starting on a different
degree" on an unequal tuning = a genuinely different interval sequence = a mode.

### Interaction to design (park): rotate-Sikit vs mask-transpose -- link or independent?
- Mask transpose (rotateMask12): rotates WHICH DEGREES are in the scale (membership).
- Rotate Sikit: rotates WHICH CENTS sit on which degree (the tuning/pitch content).
Different operations. If both exist they could align (rotate both = clean modal shift, mask follows
tuning) or diverge (rotate one only = mask/tuning misaligned). LEAN: LINK them -- setting the tonic
rotates BOTH mask and tuning together, so "set degree K as tonic" gives the mode starting there with
membership + pitch both consistent.
- Simpler framing to consider: is "rotate Sikit" just what SET TONIC should already do, in the tuning
  domain? Setting a new tonic on an unequal tuning IS modal rotation. So rotate-Sikit may NOT be a new
  control -- it may be the tonic control once generalised to rotate the tuning too (not only the mask).
  DECIDE: two controls (mask-tonic + tuning-rotate) vs one unified "tonic = rotate everything to here".
  Lean: one unified tonic that rotates mask + tuning together = simplest + most musical.

Cross-ref: Sikit.cpp (12 cents, degree 0 = root plate), the transpose<->Sikit resolution above (12-only,
modal-on-unequal), ScaleMaskArbiter rotateMask12 (the mask side), ScalaFile.hpp (.scl = intervals rel 1/1).
