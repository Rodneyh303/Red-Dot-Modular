# CC BRIEF — slew consumed at PHRASE BOUNDARY (engine AND Sands bars, in lockstep)

Supersedes the control-rate divider (RECOMPUTE_DIV=32, "Rate-limit modulated-slew recompute"). That was
the right instinct (recompute less) but the wrong RATE. Slew belongs at PHRASE boundary, not control rate.

## Why phrase boundary (not control rate)
1. DISTRIBUTIONAL CORRECTNESS: slew is a MOVING-AVERAGE (marginal-shaping) parameter. Per-step /
   control-rate-varying slew means a DIFFERENT marginal per step -> breaks the uniform output
   distribution. Slew must be CONSTANT across a phrase (see AB_MIX_COPULA_FIX.md / ROADMAP_SEQUENCING.md:
   MA params = phrase-locked by necessity).
2. CPU: no mid-phrase recompute AT ALL (vs 1.4kHz) -> the 80%@16-voice modulated-slew spike goes away.
3. The engine ALREADY consumes slew at the roll (redrawRhythm:205-211 "slew blends A<->B at the roll").
   So the roll IS the phrase-boundary slew consumption — the fix is to STOP the mid-phrase recompute, not
   add machinery.

## DISPLAY TRUTHFULNESS (the key principle — Rodney)
Sands must show the ACTUAL OUTPUT probability, never a PREVIEW. If the bars updated at video rate while
the engine applied slew only at the phrase, the DISPLAY WOULD LIE (bars show "what slew would do" while
audio still plays the previous phrase's slew). So: refresh the slew probability bars at the PHRASE
BOUNDARY too — in LOCKSTEP with the engine. Mid-phrase slew-knob changes: bars do NOT move (correct — slew
isn't applied yet); at the phrase, engine AND bars update together -> display always matches output. The
phrase-boundary snap IS the feedback ("it applied on the downbeat"). Do NOT add a video-rate slew visual.
PRINCIPLE (applies beyond slew): the Sands display shows what's ACTUALLY being output, never what's pending.

## Changes
1. REMOVE the slew-change trigger from the control-rate ENGINE recompute:
   - latchMix (PatternEngine.cpp:~485+): drop the `... || rhythmSlewLatched != rhythmSlewApplied ...`
     (and melody/qmix twins) that call recomputeEffective*. LATCH the slew value (keep storing it); do NOT
     recompute mid-phrase.
   - refreshVisualCache (~520+): same — drop the `... || rSlew != rhythmSlewApplied ...` recompute trigger.
   - The ROLL (redrawRhythm/Melody/Qmix) already consumes the latched slew at the phrase boundary — leave
     that; it IS the phrase-boundary slew application.
2. The recomputeDivider_ / RECOMPUTE_DIV control-rate gate: no longer needed FOR SLEW. Remove it for slew.
   (If MIX/scrub still needs a rate-limit, that's SEPARATE — see the scrub/OU-bridge work; don't conflate.
   For now, slew just stops triggering mid-phrase recompute.)
3. Sands bars: ensure the displayed probability is refreshed at the phrase boundary (when the roll applies
   slew) — in lockstep with the engine. NOT video rate. If refreshVisualCache currently drives the bars,
   it should reflect the post-roll slewed values (phrase-rate), not mid-phrase knob positions.

## Verify
- Modulate slew mid-phrase: NO engine recompute -> CPU flat (the 80%@16-voice spike GONE). Slew effect
  still applies at each roll (audition A<->B). Sands bars do NOT move mid-phrase; snap at the phrase.
- Fixed slew: BIT-IDENTICAL.
- Measure: 3 slews modulated @ 16 voices -> CPU near-flat vs old 80%.
- Build + bit-compare + commit + PUSH. Watch CI (3 platforms).

## NOT in scope (later)
MIX/scrub rate + the OU/AR(1)-bridge continuous scrub (AB_MIX_COPULA_FIX.md). The full per-step-audio /
displayed-page decomposition (slew_recompute_decomposition_cc_brief.md) — this phrase-boundary slew fix is
the correct, simpler completion of the slew part; the decomposition is for the remaining per-step compute.
