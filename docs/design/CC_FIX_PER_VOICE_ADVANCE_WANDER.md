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

════════════════════════════════════════════════════════════════════════════════
## THE PRINCIPLE (Rodney) — advance must work like EVERY OTHER LANE. No new behaviour.
Every lane decision (rest, legato, accent, qmix) works the SAME way:
- DEFAULT = global control (threshold follows Macro/mono).
- OPT-IN = per-voice control (East/Straits per-voice override).
- EVEN ON GLOBAL CONTROL: each voice reads its OWN window draw → voice-to-voice you see DIFFERENT
  probability outcomes from the SAME control. (Voice 3's rest ≠ voice 7's rest on the same global rest
  threshold, because different own-window draws.)
NO lane has a "delegated → ride the mono decision wholesale / skip the decision" path. Rest doesn't, legato
doesn't, none do. One control (global threshold), many draws (per-voice), many outcomes (per-voice).

Line 623's `if (varlegDelegated) continue;` INVENTED a bespoke advance-only behaviour ("delegated → ride
dMain, don't decide") that NO other lane has — and that invention IS the bug (the recurring invent-vs-reuse
error). The fix is to make advance resolve EXACTLY like rest/legato: delete the advance-specific skip; every
voice always decides on its own draw; delegation selects only the THRESHOLD source, identically to how
rest/legato select their threshold source. Do NOT invent advance-specific gating — COPY the existing lane
delegation pattern.

VERIFY against rest/legato: find how rest resolves per-voice-draw + global-vs-per-voice-threshold, and make
advance do the SAME. If rest's resolution lives in a shared helper, advance should use that helper, not a
parallel path.

════════════════════════════════════════════════════════════════════════════════
## DRY AUDIT (Rodney asked) — the authorities are mostly respected; ONE twin to collapse
Checked the per-voice advance path against the ODR/DRY authorities. RESPECTED:
- Strong types: VoiceResolver::slot()/VoiceSlot throughout (no raw voice identity).
- Draw accessor: pe.warpRow(bank, PL_VARIATION) — the SAME warped-probability accessor rest/legato use.
- Subgate three-way: reads subRes[subLevel][SK::ADVANCE] — the resolver's OUTPUT, not re-resolved.
- Rate manager: no raw dividers/gridPulse in the advance path.
- Delegation: varlegDelegated is the SOLE delegation authority (no parallel check).

### THE ONE DRY VIOLATION — the "*ForVoice TWIN" functions (collapse them)
stepAdvanceStrandIdxForVoice (SequencerEngine.cpp:2266) is literally commented "the per-voice TWIN of
stepAdvanceStrandIdx". TWIN = DUPLICATE. There are now TWO scanned-position expressions (mono +
per-voice), contradicting the mono one's own comment ":2240 THE ONE scanned-position expression". Same for
stepAdvanceDrawAt / stepAdvanceDrawAtForVoice, and decideStepAdvance_ / decideStepAdvanceForVoice_. Each is
a mono fn + a parallel per-voice twin that must be kept in sync — the exact drift the "one expression"
discipline prevents (change the wrap/scan in one, forget the other → mono/poly diverge; the position-holder
refactor and 32 steps WILL change this logic, so the twins WILL drift).
FIX (fold into the per-voice work): collapse each twin pair into ONE function PARAMETERIZED by VoiceSlot:
- stepAdvanceStrandIdx(VoiceSlot, delta)  — mono = stepAdvanceStrandIdx(VoiceResolver::mono(), delta)
- stepAdvanceDrawAt(VoiceSlot, delta)     — mono = …(mono(), delta)
- decideStepAdvance_(VoiceSlot, subLevel, delta) — mono = …(mono(), …)
Delete the *ForVoice twins; every caller passes a VoiceSlot (mono() for the mono path). ONE expression, N
voices — mono is the special case, not a separate implementation. (Same shape as the display single-source
fix: one function, many readers, never a synced twin.)
PIN: byte-identical (it's a pure parameterization — mono(VoiceSlot=mono) == the old mono fn).

════════════════════════════════════════════════════════════════════════════════
## POLY VARIATION MUST BE BIPOLAR TOO — unipolar poly is a SPECIAL CASE (Rodney)
Currently poly per-voice VAR is [0,1] (ModeController.cpp:60-69: "no poly VAR CV... the clamp is a no-op
today"), while MONO VAR is BIPOLAR [-1,+1] (knob + CV2 + Junction). So a poly voice can wander FORWARD/HOLD
but NOT BACKWARD — mono goes backward, poly can't. SAME LANE, DIFFERENT BEHAVIOUR BY VOICE = a special case,
exactly the inconsistency we keep removing.

FIX: poly VAR must be BIPOLAR too, resolved through the SAME path as mono.
- The resolver path is already correct/ready (variationProb comes from getEffectivePolyVariation via the
  SandsModifierResolver); the gap is the poly VAR has no NEGATIVE source, so the clamp is [0,1] in effect.
- Poly VAR must reach negative the SAME way mono does: the per-voice knob maps bipolar ((2*knob−1), so
  knob centre = 0 = stutter, knob down = backward), PLUS the per-voice modulation source (the poly VAR
  CV / Causeway path — the poly analogue of mono's CV2/Junction) when it exists. Same "(2*knob−1) + mod,
  ONE clamp [-1,+1]" formula the mono write uses (:166-167), applied per voice.
- If the poly VAR modulation SOURCE genuinely doesn't exist yet (no poly VAR CV / Causeway lane), then at
  MINIMUM the poly KNOB must be bipolar (knob alone reaches ±1, like mono's knob-alone does), so poly
  voices can go backward by knob even before a poly mod source lands. The CV/mod source is a separate
  add, but the KNOB bipolarity is required now (no mono/poly knob-behaviour split).
- Do NOT leave poly unipolar "for now" — that bakes in the mono/poly split. Poly VAR = bipolar, same
  formula, same resolver, same as every other per-voice control that mirrors its mono.

VERIFY: a poly voice with its VAR knob below centre walks BACKWARD independently (like mono); centre =
stutter; above = forward. Per-voice, different draws → different wander, in BOTH directions.
