# ARTICULATION: "within an envelope" (containment) vs correlation — a design question (Rodney)

NEEDS MORE THOUGHT/DISCUSSION — note only, not decided. The first time "mono not special" has been
considered MUSICALLY (not just mechanically — indices/decisions/timing). Opened by the MONO_SPECIAL_IN_
LOGIC_AUDIT + the EAST_EXTRA_LANES history.

## LINEAGE (why the current behaviour exists)
Articulation rules originally came from Vermona Melodicer + mex3: "All three voices share NOTE VALUE,
VARIATION and LEGATO; but REST is set individually per voice." → poly voices were SUBTRACTIVE from the
MONO (V1) voice — they articulate WITHIN V1's envelope. This carried into dot.modular; then EAST_EXTRA_
LANES progressively made things per-voice-flexible; eventually FULL equality across all random sources
(every lane). But the "voices articulate within V1's envelope" behaviour was never RE-EXAMINED against
the sophistication we now have (CA + copula correlation). It's a Vermona RESIDUE whose V1-specialness was
never chosen for THIS instrument.

## THE KEY DISTINCTION (the crux)
"Voices articulate within V1's ENVELOPE" (CONTAINMENT/nesting) ≠ "voices CORRELATED to V1" (copula).
- CONTAINMENT (nested/subtractive): V1 defines a gate WINDOW; poly voices can only articulate INSIDE it —
  rest/shorten/subdivide WITHIN V1's note, but CANNOT sound when V1 is silent, CANNOT extend past V1's
  gate. A hard SUPPORT constraint. Lets a voice be rhythmically active (ratchet/subdivide) INSIDE V1's
  held note, guaranteed bounded by it = a specific texture (sustained envelope + inner voices dancing
  within it).
- CORRELATION (copula/CA): each voice draws its OWN rest/legato/length; the DRAWS are statistically
  dependent (tight/loose/anti). A ρ≈1 voice TENDS to rest with V1 but CAN sound when V1 is silent, and
  draws its own gate length. A tendency, not a boundary.
ASYMMETRY (why they're NOT redundant): even ρ=1 correlation makes voices AGREE, it does NOT make a voice
UNABLE to sound outside another's gate, nor let it freely subdivide INSIDE while hard-bounded OUTSIDE.
Containment is a support-nesting constraint; correlation operates on the decisions. CORRELATION CANNOT
REPRODUCE CONTAINMENT. They are COMPLEMENTARY, composable axes.

## THE TWO QUESTIONS (Rodney)
1. Keep / flex / remove "within V1 envelope"?
   LEANING (not decided): KEEP the containment CAPABILITY (unique — correlation can't do it), but STRIP
   the V1-SPECIALNESS (the Vermona residue). Don't remove (loses the sustained-envelope+inner-voices
   texture); don't keep-as-is (V1-hardcoded = the mono-special being eradicated). → DECOUPLE the envelope
   from V1.
2. Allow "within ANOTHER envelope" (CA source, arbitrary)?
   LEANING (not decided): YES — make the ARTICULATION ENVELOPE a routable per-voice/per-group SOURCE:
   within V1 (current, now an OPTION) / within a CA-routed source (→ NESTED VOICE GROUPS: voices 2-4
   within v1's envelope, 5-7 within v5's) / within an external CV or another lane's gate / within NONE
   (independent — the full-equality case we have). Same "stop hardcoding V1, make it a routed input"
   pattern as spread/warp/direction.

## RESOLUTION OF "MONO NOT SPECIAL" (the musical meaning)
"Mono not special" MUSICALLY = V1 is not PRIVILEGED, but CAN be CHOSEN as a reference. Vermona made V1
the MANDATORY envelope; equality makes V1 one SELECTABLE envelope among sources (itself / CA / external /
none). So "mono not special" = "the envelope is ROUTABLE, defaulting to V1 for compatibility", NOT "no
envelopes". Equality DE-PRIVILEGES the envelope into a routing choice; it does not abolish it.

## HOW IT FOLDS INTO THE ARCHITECTURE (if pursued)
- Envelope SOURCE = a routing decision (CA-like, or the resolver's ownership/delegation generalized from
  "whose VALUE" to "whose ENVELOPE").
- CONTAINMENT = a DECISION-MANAGER rule: "this voice's gate decision is BOUNDED by {envelope source}'s
  gate window" — fires at step edge, mode-independent; a decide() input:
  decide(prob, draw, lane, ownership, ENVELOPE_SOURCE) → clamp the voice's gate to the envelope's window.
- V1-default = just the default routing (envelope = V1), preserving current behaviour until re-routed.
- Composes with correlation: a voice can be CONTAINED within V1 AND correlated in its within-envelope
  articulation — nested AND statistically related.
So it's NOT new machinery — it's the envelope-source routed through the decision-manager + CA authorities.

## THEORY CONNECTION (for REACHABLE_STRUCTURES_THEORY)
CONTAINMENT is a dependence relation that is NOT a copula — it is a hard SUPPORT-NESTING constraint on the
gate (copulas describe the DECISION dependence; containment constrains the SUPPORT). So it sits OUTSIDE
the copula framework as a separate structural operator. Worth adding to the reachable-structures map as
"the non-copula dependence: support nesting (containment), composable with the copula decision-dependence".

STATUS: note only, needs discussion. Decide (1) keep+generalize vs remove, (2) which envelope sources,
when the decision-manager + CA work is on the table (its natural vehicle). The containment-vs-correlation
distinction is the thing to hold onto: they are complementary, not substitutes.
