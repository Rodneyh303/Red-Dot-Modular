
## RESOLVED in code (verified 2026-09-03): end-clamp once -- was "per-term vs end clamping"

The flip proposed below is ALREADY DONE. `SpreadResolver::effective()` (src/dsp/SpreadResolver.hpp) sums
all contributions (base + ownCv + eastCv + macro send) and applies `clampSpread` ONCE at the return --
no per-term clamping. `test/test_spread_resolver.cpp` asserts it ("Clamp ONCE at the end (not per-term)")
and the file header documents the canonical end-clamp arithmetic. The ONLY residue is a STALE inline
comment on effective() that still reads "Clamps STEP-WISE to match the manager code it replaces" -- that
comment now describes behaviour the function no longer has; fix it to say end-clamp. No behaviour work
left here. (Rodney's feature-spine point 4 -- exact probability counter-offset -- remains open below and
is the live item in this doc.)

The original open-question text is kept below for history.

## Open question (revisit): per-term vs end clamping of effective spread

SpreadResolver::effective() currently clamps AFTER EACH additive term (own CV, East CV, Macro send),
because that exactly matches the manager code it replaces — the migration is behaviour-inert by
construction. But step-wise clamping is probably NOT the best behaviour: an intermediate term that
overflows ±1 is pinned before a later term can pull it back, so the order of contributions changes the
result and a large-then-opposite pair loses information (e.g. base 0.9 +own 0.4 −east 0.6 → 0.4 under
step-wise, but 0.7 under a single end-clamp — the end-clamp preserves the intended net). A single
end-clamp is order-independent and more musically faithful. Revisit as a deliberate BEHAVIOUR change
(own commit, not folded into a migration): flip effective() to sum-then-clamp once, update the resolver
test's "STEP-WISE clamp" case, and build-verify the audible difference is acceptable. Low risk mechanically
(one function), but it IS a behaviour change so it needs Rodney's sign-off + a Rack listen.

## Possible overlap: Sands note-probability rotation vs .dmtune tuning rotation (Rodney) -- OPEN, needs one fact

Rodney (Dubai lounge): "arguably some overlap between Sands note-probability rotation and .dmtune tuning
rotation." Assessed structurally (could NOT find a named 'note-probability rotation' mechanism in code, so
the Sands side is inferred -- Rodney to confirm).

### The structural overlap (real as an abstraction)
- .dmtune tuning rotation = rotate a per-DEGREE array (cents, or enabled mask) by K around the N-degree
  cycle. = cyclic shift of a fixed-length value-array. Uses rotateMask12 (ScaleMaskArbiter.hpp:36) / the
  root-choice mechanism.
- Sands note-probability = per-position probabilities/weights governing which notes fire; "rotation" =
  shift which position each probability sits at, around some cyclic index.
Both are "rotate an indexed array by K around a cyclic index" = the SAME abstract operation rotate(arr,k,N).
Real overlap AT THE ABSTRACTION LEVEL. And a rotate util already exists (rotateMask12).

### But real-abstraction != exploitable-unification. Three deciding questions:
1. Same index space / cycle length? Tuning rotates over N DEGREES (12/24). Probability rotates over
   STEPS or VOICES (16? 8?). If different-length cycles indexed by different things -> they share only the
   abstract rotate() util, not a musical relationship. Mild code-reuse, not deep unification.
2. Musical reason to rotate them TOGETHER? The powerful version = rotating tuning AND probability by the
   same K yields something coherent ("rotate the maqam + its emphasis pattern to a new tonic together").
   Only makes sense if indexed compatibly (both over degrees). If probability is over STEPS and tuning
   over DEGREES, rotating together rotates DIFFERENT AXES = meaningless.
3. Same KIND of rotation? Tuning rotation = modal/PITCH rotation (choosing the root of an un-rooted set).
   Probability rotation might be TEMPORAL (shift accent in time) or POSITIONAL (shift which voice is
   likely). Different musical gestures sharing the word "rotate".

### THE deciding fact (Rodney to confirm): what does Sands probability rotate OVER?
- If per-DEGREE (probability of each SCALE DEGREE sounding): overlap is DEEP. Tuning + probability both
  degree-indexed; rotate-together = a coherent "rotate everything about this degree" gesture; unifying is
  genuinely powerful (one rotation moves pitch-set + emphasis together = strong maqam feature).
- If per-STEP or per-VOICE: overlap is SHALLOW. Same rotate() util, unrelated axes; DON'T force a common
  control (false economy -- two unrelated things sharing a control just because both "rotate").

### Steer (pending the fact)
Share the rotate() UTILITY regardless (cheap, already have rotateMask12 -> generalise to rotateMaskN).
Only unify the CONTROL / offer rotate-together IF probability is degree-indexed. Do not couple them on the
strength of the word "rotate" alone.

Cross-ref: TONIC_TRANSPOSE_BUILD_BRIEF (tuning rotation = rotateMaskN, degree-indexed, the un-rooted-.scl
framing), ScaleMaskArbiter rotateMask12 (the shared util candidate), SANDS_* docs (what probability is
indexed over -- the deciding fact), COOL_POINTS_FEATURE_SPINE point 4 (probability counter-offset -- may
relate to what 'rotation' means here).

## RESOLVED (Rodney): three rotations = three LAYERS of one pitch pipeline (not a 2-way overlap)

Rodney's decomposition:
- PROBABILITY rotation: which DEGREE (within the mask) fires per sequence step.
- MASK rotation: which PITCHES correspond to which degree.
- TUNING rotation: the TUNING (cents) of the pitches.

### These are three rotations at three CONSECUTIVE stages of pitch resolution
Pipeline: step -> [PROBABILITY] -> degree -> [MASK] -> pitch-class -> [TUNING] -> cents/frequency.
Each rotation acts at ONE stage:
- Probability = step->degree layer (which active degree is selected per step; the selection pattern).
- Mask = degree->pitch-class layer (membership/mapping of degrees onto positions; rotateMask12).
- Tuning = pitch-class->cents layer (what frequency each position carries; .dmtune/Sikit rotation).
Not the-same-operation-duplicated -- three rotations at three different layers. Siblings, not duplicates.

### Resolves the "overlap" question: shared UTILITY, NOT shared control
They share the ABSTRACT FORM (each is a cyclic rotation of an indexed array) -> share the rotate() util
(generalise rotateMask12 -> rotateMaskN, all three use it). They do NOT share a CONTROL: each rotates a
different axis (step-selection / degree-membership / pitch-tuning), different lengths, different pipeline
stages. Rotating all three by the same K is NOT musically meaningful (different axes). So: factor the CODE,
keep THREE INDEPENDENT controls. (Rodney's overlap instinct was right; the precise nature is "common
operation, different layers".)

### The exciting part: three ORTHOGONAL rotations that COMPOSE = a 3-D pitch rotation space
Each axis is a musically DISTINCT gesture; independent, composable:
- Probability alone: same pitches + tuning, different degree emphasised/likely per step -> shifts the
  MELODIC EMPHASIS pattern. (Close to maqam SAYR -- which degrees get weight.)
- Mask alone: which degrees are in-scale moves -> modal rotation of MEMBERSHIP.
- Tuning alone: intonation shifts under fixed degrees/selection -> RE-TUNING.
Rich FACTORED control space, not overlap-to-eliminate. All three being "rotations" makes it elegant +
learnable: ONE concept (rotation) applied at three stack levels the user can reason about.

### Maqam-faithfulness note
Probability rotation is arguably the one that most directly models SAYR (melodic pathway/emphasis) --
"which degree is likely to fire" IS emphasis, which distinguishes maqamat BEYOND their pitch-set. So think
of it not just as "rotate a probability array" but "shift the emphasis contour" -- real maqam meaning. The
three rotations map onto three musically real dimensions: EMPHASIS (probability), MEMBERSHIP (mask),
INTONATION (tuning).

Supersedes the "OPEN, needs one fact" section above: the fact is answered -- probability rotates over
which-degree-fires-per-step (step-indexed selection over the mask's degrees), so it's a DIFFERENT LAYER
from tuning (pitch-class-indexed). Deep in the sense of a coherent 3-layer family; NOT a merge-into-one-
control. Shared util, three controls, orthogonal composition.

## WIDER SCOPE (Rodney): the 3-D pitch space generalises to the full SAYR-dimensioned modulation space
The "3-D pitch rotation space" (emphasis x membership x intonation) was too narrow a frame. The sayr
synthesis (presets/maqam/README) showed the primitives that carry sayr -- direction, length, offset,
rotation, legato, rest, accent, octave -- belong in the SAME modulation space as the three pitch axes.
So "3-D pitch modulation" grows into the FULL musical-behaviour modulation space.

Axes of the wider space:
- Pitch: probability (emphasis), mask (membership), tuning (intonation), octave (register).
- Contour/time: direction (fwd/rev/pendulum/pingpong = the arch), length + legato (dwelling/duration),
  rest + accent (pause + stress / ghammaz), offset + rotation (starting note, region visiting).

Key point (why this is "adding scope", not just more knobs): these are the axes of ONE high-dimensional
modulation space, and -- the sayr result -- the space's dimensions ARE the dimensions of maqam melodic
behaviour. The 3-D pitch cube is one FACE of a higher-dimensional object; the sayr synthesis revealed the
rest of the object. Modulating a trajectory THROUGH this fuller space IS generating sayr-like behaviour.
The "3-D pitch modulation" is a PROJECTION of the real thing onto three pitch axes; the wider scope
restores the missing dimensions.

Reframes what the engine IS: not "modulates pitch in 3 ways" but "modulates MUSICAL BEHAVIOUR across every
dimension sayr lives in". The modulation space is a musical-behaviour space whose axes are the sayr
dimensions, of which pitch-resolution is three.

Cross-ref: presets/maqam/README (the sayr<->primitive mapping + direction/pendulum = the added axes),
ROTATION_TAXONOMY (all the modulatable primitives = the axes), the 3-D pitch space above (now one face of
the wider space), LAUNCH_INTENT_AND_STORY (thesis: the engine modulates musical behaviour across sayr's
dimensions).

Cross-ref: TONIC_TRANSPOSE_BUILD_BRIEF (mask + tuning rotations, rotateMaskN), the maqam sayr/emphasis
discussion (presets/maqam/README -- probability rotation as sayr), ScaleMaskArbiter rotateMask12 (the
shared util to generalise), COOL_POINTS_FEATURE_SPINE point 4 (probability counter-offset mechanism).

### COMPLETION of the wider scope (Rodney, Dubai lounge): the space is now 5-of-6 sayr dimensions DIRECT
The wider-scope section above was written before two later results. Folding them in completes it:

1. DIRECTION MODES make the CONTOUR axis a DIRECT (not emergent) dimension. Forward/Reverse/Pendulum/
   PingPong (SequencerEngine.cpp:21-90) are first-class. So "direction of development" is a direct axis,
   and PENDULUM = the archetypal maqam arch (ascend to ghammaz, descend to tonic). The higher-dimensional
   object's contour face is a BUILT control, not something coaxed from modulated offset. Revised: 5 of 6
   sayr dimensions map to DIRECT axes; only "order regions visited" stays emergent (trajectory-ordering).

2. So the modulation space's axes now are:
   - Pitch: probability (emphasis), mask (membership), tuning (intonation), octave (register).
   - Contour/time: DIRECTION (fwd/rev/pendulum/pingpong = the arch) [now DIRECT], length+legato
     (dwelling), rest+accent (pause/stress/ghammaz), offset+rotation (starting note; region-visiting is
     the one emergent trajectory axis).

Final reframe: the engine modulates MUSICAL BEHAVIOUR across (nearly) every dimension sayr lives in, with
5/6 dimensions as direct built axes and the archetypal contour (pendulum arch) built in. "3-D pitch
modulation" is the pitch projection; the full space is the sayr-behaviour space. This is launch/thesis
material: the general modulation engine's dimensions ARE maqam's melodic-behaviour dimensions -- the East
pole emerges from the general mechanics, demonstrated axis by axis, not special-cased.

(Reached, fittingly, in a Dubai lounge -- physically inside the Middle Eastern musical world the East pole
honours. Honesty holds: the tools express + sculpt every dimension sayr lives in; they don't KNOW each
maqam's culturally-correct sayr -- that stays the human/preset layer.)

Cross-ref: presets/maqam/README (FULL sayr mapping, direction/pendulum=arch, 5-of-6 scorecard), ROTATION_
TAXONOMY (the axes as built modulatable primitives), LAUNCH_INTENT_AND_STORY (thesis: dimensions of the
modulation space = dimensions of maqam behaviour).
