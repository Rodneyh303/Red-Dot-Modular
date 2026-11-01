# SIGN-OFF — per-voice step-advance plan: accept 1 & 3, OVERRIDE 2 & 4 (Rodney)

Good recon. Staging (Stage 1 bit-identical / Stage 2 divergence+tests / Stage 3 display) is correct — and
the Stage-2 divergence unit tests are exactly the coverage that de-risks the later counter removal. Proceed
on that structure. Four decisions:

## DECISION 1 — WRAP: int64 unbounded offset. ACCEPT.
Correct. A mod-pattern-length wrap on the OFFSET is not read-preserving (lengths ∤ 16 jump at the wrap — the
DNA_LCM bug class). The offset is the same KIND as totalStepsElapsed (a net-step count); "wraps at phrase
boundaries" is satisfied at the READ level (every lane mods locally at its own length), NOT by wrapping the
offset. So the offset stays unbounded int64; the VISIBLE wrapping happens per-lane at the phrase. This
IMPLEMENTS "wraps at phrase" correctly, it doesn't contradict it. ACCEPT.

## DECISION 3 — DELEGATION: decision reads own window, draw-production keeps delegation. ACCEPT.
Correct and consistent with D6 SWEEP (draw production ≠ decision structure). The advance DECISION reads the
voice's own variation window; note-length draw production keeps the VAR/LEG delegation rules. ACCEPT.

## DECISION 2 — OVERRIDE: the tie-into-rest FALL advance is PER-VOICE, not mono.
CC's recon MISSED that the fall-advance is already a STOCHASTIC advance decision. Per
CC_PROMPT_TIE_INTO_REST_PROBABILISTIC_ADVANCE.md (OPTION 2, built): when tieAcrossRests && advanceOnTieIntoRest,
the fall-advance is PROBABILISTIC — decide(p,u) with u = the rest step's variation draw, bipolar — the SAME
mechanism as the rise. So it is NOT "the clock moves on the mono decision"; it is its OWN stochastic advance
decision.
THEREFORE: if advance decisions go per-voice, the FALL decision must ALSO go per-voice — each voice makes its
own probabilistic tie-into-rest fall decision (own rest-step variation draw, own advanceOffset update),
consistent with the rise. You CANNOT have per-voice rise-advance and mono fall-advance for the SAME stochastic
mechanism — voices would diverge on rises then snap back together at a tie-into-rest fall = incoherent (same
"one coherent playhead" logic as the bipolar-direction fix).
- pendingCheckpointArrival ARRAY-IFIES per voice (the per-voice "one advance, one arrival" accounting).
- CC's "no designed consumer" dismissal is WRONG: the option-2 probabilistic fall-advance IS the consumer.
- Scope note: heavier than CC proposed, but correct. The fall decision reuses decideStepAdvanceForVoice_
  (the same per-voice decision fn), applied at the fall checkpoint, per voice.

## DECISION 4 — OVERRIDE: NO new perVoiceStepAdvance bool. Reuse the variation-lane delegation machinery.
Step-advance REPURPOSES the variation lane. The variation lane ALREADY has Macro/East delegation + the
per-voice Straits controls we added. Global-vs-per-voice step-advance must FALL OUT of that existing
machinery — do NOT invent a parallel engine bool.
- A voice's step-advance is PER-VOICE iff its variation lane is per-voice (has a per-voice override via
  East / the Straits per-voice variation controls); GLOBAL (follows Macro) iff its variation is delegated
  to Macro.
- So the "switch" is the voice's EXISTING variation delegation state, read at the advance decision — not a
  new bool, not a new menu toggle, not new persistence.
- This is cleaner AND consistent: step-advance inherits the variation lane's per-voice/global routing for
  free, because it reads the variation lane. Reuse, don't invent (the recurring lesson).
- Remove the Decision-4 bool/menu/persistence from Stage 2. Replace "if perVoiceStepAdvance" with "per the
  voice's variation delegation" at the rise AND fall sites.

## NET CHANGES TO THE PLAN
- Stage 2: the switch is the variation-lane delegation (not a bool). The FALL site (legatoCheckpointOnFall)
  gets per-voice treatment (decideStepAdvanceForVoice_ at the fall, per voice, own rest-step draw);
  pendingCheckpointArrival array-ifies. Everything else in Stage 2 stands.
- Decisions 1 & 3, and all of Stage 1 / Stage 3, unchanged.
- The per-voice divergence tests (Stage 2.7) stand — they're the counter-removal safety net.
