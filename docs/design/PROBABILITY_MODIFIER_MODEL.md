
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
