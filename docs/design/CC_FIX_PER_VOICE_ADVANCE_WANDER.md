# CC FIX — per-voice variation playhead must wander even on a GLOBAL threshold (Rodney)

SYMPTOM: test #1 fails — each voice's variation playhead does NOT wander independently to find its escape
probability. All voices move together.

## ROOT CAUSE — line 623 `continue` disables the per-voice decision for delegated voices (= all, by default)
foldPerVoiceAdvance_ (SequencerEngine.cpp:623): `if (varlegDelegated(vs, 0)) continue;` — a DELEGATED voice
SKIPS its own decision and rides dMain (the global decision). varlegDelegated (:745-746) returns TRUE unless
the voice is Local-East → so BY DEFAULT every voice is "delegated" → every voice continues → rides dMain →
ZERO divergence. That is why nothing wanders.

## THE CONCEPTUAL ERROR (Rodney) — delegation controls the THRESHOLD, not whether the voice DECIDES
Even with a GLOBAL threshold (all voices share p = variationProb), each voice reads its OWN variation window
draw (u_v), so decide(p, u_v) gives a DIFFERENT result per voice → per-voice wandering. The per-voice
machinery is ALREADY correct: decideStepAdvanceForVoice_ (:588-596) reads u = stepAdvanceDrawAtForVoice(vs,…)
— the voice's own window. The bug is that line 623 SKIPS this for delegated voices.
Delegation should ONLY affect WHERE p comes from (global variationProb vs a per-voice threshold), NOT whether
the voice decides at all. Global threshold + per-voice DRAWS → per-voice WANDERING. That's the feature.

## THE FIX
1. REMOVE the `if (varlegDelegated(vs, 0)) continue;` at :623 (and the analogous skip at :1294). Every voice
   v = 1..numPolyVoices runs decideStepAdvanceForVoice_ on its OWN window draw — ALWAYS.
2. Delegation now only selects the THRESHOLD p inside decideStepAdvanceForVoice_:
   - DELEGATED (follows Macro/global): p = the global variationProb (the shared threshold). The voice still
     reads its OWN draw u_v → still wanders differently per voice.
   - LOCAL-EAST / per-voice: p = the voice's own threshold (voices[vs].variationProb already per-voice, or
     the subgate per-voice ADVANCE). Own threshold AND own draw.
   So decideStepAdvanceForVoice_ already does the right thing — just stop skipping it. (If the threshold-
   source selection needs a tweak so delegated voices read the GLOBAL variationProb while non-delegated read
   their own, do that INSIDE the decision fn, not by skipping.)
3. The pendingCheckpointArrivalV_ latch (a voice that moved at the fall rides 0 this rise) STAYS — that's
   correct per-voice accounting, unrelated to the delegation skip.
4. Lane index note: varlegDelegated(vs, 0) — the `0` is one of the two VAR/LEG delegation lanes
   (varlegLocalEast_[v][0..1]). Once the `continue` is removed this is moot for gating; but if delegation is
   used to pick the THRESHOLD source, confirm it reads the VARIATION lane's delegation (the correct index),
   not legato's.

## VERIFY (test #1)
- GLOBAL threshold (no per-voice overrides, all voices "delegated"): each voice's variation playhead STILL
  wanders to its OWN escape probability at a different scan depth (different per voice, because different
  own-window draws). VISIBLE on each poly voice's tab.
- With all voices locked p=+1: all advance every rise (no wander) = degenerate. With p mid: each wanders
  differently. With p=0: each holds (but still scanning its own window — the chevron scans even while the
  main playhead holds).
- Goldens: the GLOBAL (delegated) case previously rode dMain (no offset). Removing the skip means delegated
  voices now diverge — so the "global = bit-identical to mono" claim CHANGES. Re-examine: is "global
  threshold" meant to be bit-identical (all voices same position) or "same threshold, different wandering"?
  Per Rodney it's the LATTER — so the Stage-1 "global = bit-identical" golden was testing the WRONG thing
  (it locked positions; Rodney wants same-threshold-different-wander). Re-pin accordingly; GOLDEN_A (clock/
  phase) untouched.

## NOTE
This is the "global threshold vs per-voice threshold" distinction — NOT "global position vs per-voice
position". The draws are ALWAYS per-voice (own window); only the threshold follows delegation. The `continue`
conflated threshold-delegation with decision-skipping.
