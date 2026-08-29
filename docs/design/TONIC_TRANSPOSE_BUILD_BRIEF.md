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

## Sikit "assumes first tuning is C" -- NOT a bug; it's the missing rotate control (Rodney + code)

Rodney's worry: "Sikit is loading scala assuming first tuning is C, not sure what to do." Read the load
path (Sikit.cpp:169-174):
  // Scala lists degrees 1..N; centsFromRoot[i] is degree (i+1). Root (param 0) stays 0.
  for i in 0..12: params[CENTS_PARAM_0 + (i+1)].setValue(sf.centsFromRoot[i]);
So: Root (param 0) = 0 cents (the 1/1), and the .scl's listed intervals fill degrees 1..12 above it.

### This is CORRECT, not a bug
.scl files are relative to 1/1 (the implicit first degree; the file lists degrees 1..N above the unspoken
1/1, the last = the period/octave). Sikit does the textbook-correct thing: 1/1 = root = param 0 = 0 cents
(fixed), file's intervals become degrees 1..12. "Assumes C" is true only as LABELLING: the 1/1 is anchored
at Monsoon's root slot, which is conventionally CALLED C. The .scl's 1/1 -> Sikit's root -> Monsoon's "C".
Standard, expected. No bug. And Monsoon's root is itself movable, so it's not stuck on a literal C.

### The LEGITIMATE gap (what the unease actually is)
Not "assumes C" -- it's that the load path always puts the file's OWN 1/1 on the root. You can't say
"start this tuning from its 3rd degree instead" = you can only load a tuning in its DEFAULT rotation.
That's exactly the rotate-Sikit / modal-rotation capability from the previous section -- and it's an
ENHANCEMENT, not a fix.

### Options (don't panic-fix; it's correct)
1. LEAVE IT -- correct behaviour. Document ".scl's 1/1 maps to the root (called C by convention); root is
   movable." Lowest effort, not wrong.
2. ADD THE ROTATE/TONIC CONTROL -- let the user pick which loaded degree sits on the root (modal rotation
   of the tuning). The rotate-Sikit / unified-tonic idea. The REAL added capability; subsumes the "what if
   I don't want 1/1 on C" concern by making the reference degree a choice. RECOMMENDED.
3. FULL .kbm support -- read a keyboard-map for arbitrary reference placement. Correct+complete Scala
   answer but heavy; overkill for a 12-slot retuner. Rotate (opt 2) covers the musically useful part.

STEER: the load path is doing the right thing; it just needs a COMPANION rotate control (option 2), not a
change to the load path. The "assuming C" feeling = "I can only load in default rotation"; the fix is the
unified tonic-rotates-tuning control (prev section), not touching openScalaFilePicker().

Cross-ref: Sikit.cpp:169-174 (load path, root stays 0, degrees 1..N above), the rotate-Sikit section
above (the companion control), ScalaFile.hpp (.scl rel 1/1), SCALA_FILE_AND_LOAD_UI.md (load UX).

## DECISION (Rodney): add a Sikit ROOT knob -- independent of Shophouse scale root

Chose option 2. Add a dial/knob (fixed-position control) ON SIKIT to set the ROOT = which loaded degree
sits at the root/1/1. A discrete rotary selector 0..11 that rotates the loaded tuning so the chosen degree
becomes the root. The modal-rotation control, on Sikit, in the TUNING domain.

### Sikit root is INDEPENDENT of Shophouse scale root (Rodney: "need not be same")
Two different roots on two different things:
- SIKIT root = which degree of the TUNING sits at 1/1 -> rotates the CENTS (pitch content). Property of
  the tuning.
- SHOPHOUSE scale root = which degree the MASK is rotated to -> rotates MEMBERSHIP (which degrees in
  scale). Property of the scale.
Decoupling them is a real musical freedom: tuning reference on one degree, scale tonic on another (e.g. a
maqam tuning at its natural 1/1, scale tonic on a different degree of it). The tuning keeps its correct
reference while the scale roams.

### Honest note: this reverses the earlier "unify them" lean -- and that's fine, but adds a UX obligation
Two sections up I leaned "one unified tonic rotates mask+tuning together" (can't desync). Rodney chose
INDEPENDENT (more expressive). Tradeoff: they CAN now diverge, so they must be VISIBLY DISTINCT so nobody
expects one to move the other. Label for what they are: Sikit = "tuning root" (rotates the tuning);
Shophouse = "scale root" (rotates the mask). Independence is the feature; the only risk is they LOOK like
they should be linked. Name them apart -> clean.

### Design specifics
- DISCRETE 0..11, snap to integer degrees -- a degree selector, not a continuous cents offset. 12-position
  (stepped) rotary.
- FIXED-POSITION control (holds its value, persists in patch), not momentary/gesture.
- Make it a PARAM (automatable, saved) like the cents knobs -- it's a performable/automatable musical
  choice, not just a menu item.
- DEFAULT = 0 (file's own 1/1 on root = current correct load behaviour). Not touching it preserves
  exactly today's semantics; the knob is purely ADDITIVE -> backward-compatible, non-surprising. Existing
  patches + default load unchanged.

### Mechanism
On load, cents fill degrees 1..12 above root=0 (unchanged, Sikit.cpp:169-174). The root knob applies a
degree rotation on top: rotate the 12 cents so the selected degree's cents becomes 0 (the new 1/1) and the
rest follow cyclically. (This is rotateSikit in the tuning domain; the mask-side rotateMask12 is separate,
driven by Shophouse's own scale root.)

Cross-ref: Sikit.cpp:169-174 (load path, default root=0 preserved), the rotate-Sikit section (this IS
that control), the Sikit-12-only resolution (12 degrees so 0..11 selector), ScaleMaskArbiter rotateMask12
(the SEPARATE mask-side root on Shophouse), DOC_PRIORITISATION (TONIC_TRANSPOSE Tier-1 build item).

## Does tuning-rotation apply to Colonnades / Duo? YES -- and Duo is the rotateMaskN case (Rodney)

Sikit param check (Sikit.hpp:20-23): params are CENTS_PARAM_0..11 ONLY (12 cents); degree 0 root LOCKED
at 0 cents, no interactive knob. So the proposed Sikit root DIAL is a NEW control (a rotation index
0..11), not an existing param -- it rotates which loaded degree sits on the (locked-0) root. Default 0 =
current behaviour (1/1 on root); opt-in rotation from there; show when non-zero so an offset is deliberate.

### Colonnades / Duo DO carry tuning -> rotation applies (code)
Colonnades.hpp: "tuning + scale AUTHORING expander... claim/publish cents[]+weight[]+maskAuthored". So
Colonnades carries cents (tuning) + weight + an authored MASK -- a FULLER source than Sikit (Sikit = cents
only; its mask lives on Monsoon/Shophouse). Colonnades = 12 degrees; Colonnades Duo = 24 degrees.

Differences from Sikit that shape HOW rotation applies:
1. Colonnades AUTHORS (build cents fader-by-fader) vs Sikit LOADS a .scl. So "rotate" on Colonnades
   rotates the AUTHORED cents + mask together = the UNIFIED rotation (cents+mask) -- Colonnades is
   actually the CLEANER home for the unified-tonic idea, because it owns BOTH layers in one place (Sikit
   owns only cents).
2. Colonnades Duo is 24-degree = the N != 12 case. This IS "the Micro/Emerald-Hill non-12 path" the
   transpose gap was re-homed to. So rotation on Duo needs rotateMaskN (rotate over 24), NOT rotateMask12.
   Sikit + Colonnades (12) can use the 12-domain rotation; Duo (24) forces the degree-space
   generalisation = the open item from the transpose thread.

### The family picture
- Sikit (12, loads .scl): rotation applies, rotateMask12-compatible, rotates the loaded tuning.
- Colonnades (12, authors cents+weight+mask): rotation applies, UNIFIED (cents+mask), 12 so rotateMask12
  works.
- Colonnades Duo (24, authors): rotation applies, needs rotateMaskN (N=24) -- the module that REQUIRES
  the generalisation; others can ship 12-only first.

### Consistency principle (Rodney's Colonnades question surfaces it)
If all three tuning sources get a root/rotation control, keep it the SAME CONCEPT on each (same meaning:
"which degree is the 1/1"), even if the GESTURE differs per module idiom (Sikit = a dial; Colonnades =
maybe the existing root cents-lock extended, or a matching dial). Uniform concept -> users learn it once.
And the "need not equal the Shophouse/Monsoon mask root" INDEPENDENCE applies to ALL of them: each tuning
source says "my 1/1 sits here", the consuming Monsoon says "my scale is rooted here", the optional gap is
expressive. Consistent across the family.

### Build order implication
Ship the 12-degree root rotation first (Sikit + Colonnades, rotateMask12-domain). Colonnades Duo's 24-deg
rotation waits on rotateMaskN -- bundle it with the non-12 transpose generalisation (same work).

Cross-ref: Sikit.hpp:20-23 (cents-only params, root locked), Colonnades.hpp (cents+weight+mask authoring),
COLONNADES_DUO_PANEL_SPEC (24 degrees), the transpose non-12 gap above (rotateMaskN, now clearly needed
for Duo), the rotate-Sikit + unified-tonic sections (the concept this generalises)." 

## Capacity correction + independent-rotate question (Rodney)

### Colonnades up-to-12, Duo up-to-24 -> rotation is rotateMaskN over the LIVE N
Correction: Colonnades holds UP TO 12, Duo UP TO 24 (variable-N, not fixed). So the rotation domain is
the current authored degree count, not a constant. Rotation = rotateMaskN where N = live degree count.
This UNIFIES the family: Sikit (fixed 12), Colonnades (<=12), Duo (<=24) are all "rotate over current N";
rotateMask12 is just the special case N=12. So rotateMaskN is the GENERAL operation every tuning source
wants -- not a Duo-specific need; 12 is where it reduces to the existing function.

### Independent rotate-tuning vs rotate-mask -- DO they make sense separately?
The two rotations are DIFFERENT musical operations:
- Rotate MASK = shift which degrees are IN the scale (membership / "which notes").
- Rotate TUNING = shift which cents sit on which degree (pitch content / "tuned how").

Case for LINKED (one "tonic"): most "change the tonic" intent wants both to move together = a coherent
mode at the new degree. Independent could confuse a naive user (mask moves, pitch doesn't). Simpler.

Case for INDEPENDENT (two controls) -- stronger than it first looks:
- Tuning fixed, MASK rotated = play different modes of a FIXED tuning without re-tuning. This is the
  PRIMARY maqam/modal gesture (fixed-tuned instrument, select ajnas/modes by choosing degrees). Extremely
  common and central. Forcing linked would DESTROY this workflow (every mode change would re-tune).
- Mask fixed, TUNING rotated = same active degrees, re-intonated underneath (intonational recolouring of
  a fixed pattern). Rarer, but a real microtonal gesture.

### Resolution: INDEPENDENT, because they're ORTHOGONAL musical dimensions (flips the earlier "lean linked")
Mask answers "which notes" (mode selection -- COMMON, should be easy + independent). Tuning answers
"tuned how" (re-intonation -- RARE, deliberate, opt-in). They're independent BY NATURE because they live
on different layers (mask on Monsoon/Shophouse/Colonnades-authored-mask; tuning-rotation on the tuning
source). That's CORRECT, not a flaw. The only risk is a user not realising there are two -> solved by
labelling + defaults, NOT by force-linking:
- Tuning rotation DEFAULTS to 0 (unrotated). So unless deliberately touched, "rotate tonic" moves only
  the mask = the expected common behaviour (mode selection over fixed tuning).
- Tuning rotation is the opt-in "re-intonate" control, clearly labelled, shown when non-zero.

Do NOT force-link them: linking breaks the primary modal workflow (mode over fixed tuning). Keep
independent, default tuning-rotation off. Optionally offer a convenience "rotate both together" as a
COMPOUND gesture for when the user does want a full modal shift -- but built ON TOP of the two independent
controls, not instead of them.

Supersedes the earlier "LEAN: link them / unified tonic rotates both" -- refined: independent by nature
(orthogonal dimensions), tuning-rotation opt-in default-0, optional compound "rotate both" convenience.

Cross-ref: rotate-Sikit + Colonnades/Duo sections above (the controls), rotateMaskN (now the general
operation over live N), the maqam/modal use (why mask-over-fixed-tuning must stay independent).

## Are preset .dmtune files mask rotations? NO -- they're tuning+mask PAIRS (the un-rotated reference)

Checked the .dmtune format (TuningList.hpp + a shipped preset). A .dmtune contains:
  { format:"dotmodular.tuning", version:2, n:15, cents:[...N...], enabled:[...N...] }
= cents[] (the TUNING) AND enabled[] (the MASK), for N degrees. Header: "a .dmtune front carries cents +
enabled, NOT weight". A tuning+mask PAIR, not a rotation.

### So "are they mask rotations?" -- NO, two senses
1. A .dmtune stores an ABSOLUTE (cents, enabled) pair, not a rotation OPERATION. Rotation is a live
   transform applied at load/runtime (rotate mask or tuning by a root offset). The preset is the THING
   transformed, not the transform. Preset = stored (cents, enabled); rotation = what the root/tonic
   control DOES to it.
2. Not even purely masks -- each carries its OWN tuning (cents), not just a membership pattern over a
   shared tuning. The maqam presets are self-contained pitch-sets: e.g. Maqam_Rast_24EDO = Rast's cents +
   Rast's mask. "Here is a complete tuning and which of its degrees form this jins/maqam."

### What this means for the rotation design
The presets are the FIXED REFERENCE (authored tuning+mask, at their authored rotation, degree 0 = authored
tonic); the rotation controls (mask-root, tuning-root) are LIVE TRANSFORMS on top. Load Rast -> get Rast's
cents+mask un-rotated; THEN rotation can shift it (mask-rotate = different mode of Rast's degrees;
tuning-rotate = re-intonate). Preset defines the un-rotated HOME state; rotation is the DEPARTURE from it.
Complementary, not the same. The preset isn't a rotation; it's what rotation acts UPON.

### Subtlety (connects to root-relative storage)
A scale-only/transposable .dmtune stores enabled[] ROOT-RELATIVE (tonic normalised to degree 0, per the
tonic brief) so the live root control can rotate it into place. So rotation touches the preset FORMAT only
in that the mask may be stored PRE-NORMALISED (degree-0 tonic) to allow clean rotation -- NOT that the
preset IS a rotation. Note: root-suffixed jins presets (Jins_*_G, _C) look authored AT a specific root;
24EDO maqam presets may be root-relative. Check per-preset if it matters; headline stands: preset = pair,
not rotation.

Cross-ref: TuningList.hpp:28-31 (cents+enabled slot), presets/maqam/*.dmtune (tuning+mask pairs), the
rotation sections above (rotation = live transform on the loaded pair), STEP 4 of this brief (root-relative
mask save -- the one place normalisation-for-rotation touches the format).
