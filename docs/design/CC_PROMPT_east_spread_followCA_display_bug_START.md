# CC PROMPT — FIX: East spread bars ignore follow-CA (anchor to V1, wrong base) — display≠audio

CONTEXT: Rodney, in Rack — East, spread target = FOLLOW CA, voice 2 sourced from voice 3 (identity
otherwise). TWO bugs in the BARS (display):
- At spread +1: voices 2 AND 3 bars both show VOICE 1's probabilities (should show the CA TARGET = v3).
- At spread 0: voices 2 and 3 show the SAME bars (v3's), should show each voice's OWN original.
So the display's spread (a) anchors to V1 not the CA source, and (b) its spread-0 "own" is already the
CA-remapped value. The AUDIO path is correct (follows CA); only the DISPLAY diverges — the UI-recomputes-
differently disease (cf. MONO_SPECIAL_IN_LOGIC_AUDIT / the decision authority).

## ROOT CAUSE (pinned — SequencerEngine.hpp:865-867, effectiveProbability, the display accessor)
    const float base = redDot::SpreadInterp::polySlewedPub(pe, l, v, step & 0x0F);
    const float disp = (v == 0) ? interpolate(base, base, sp)                       // V1 self
                                : applyAnchorV1Only(pe, l, step & 0x0F, base, sp);   // ← BUG
BUG A (target): poly voices use applyAnchorV1Only — interpolates toward V1's slewed value, HARDCODED,
regardless of spreadTargetMode. It never checks follow-CA, never reads the CA source. So spread→+1 pulls
every voice toward V1 (Rodney's "both show lane 1"). The AUDIO path uses SpreadInterp::applyMono/applyPoly
(which read pe.spreadTargetMode[lane]==1 → follow-CA → target = CA source, via monoPreRemap/caSrc). The
DISPLAY accessor was built with the V1-anchor shortcut and never taught follow-CA.
BUG B (base): base = polySlewedPub = the POST-CA-remap published slew. For a CA-sourced voice (v2←v3),
its base is ALREADY v3's material → spread-0 "own" shows v3, not v2's original. The audio path's follow-CA
uses own = monoPreRemap (PRE-remap = the voice's TRUE own) vs target = post-remap (leader). The display
uses post-remap for BOTH endpoints → collapsed.

## THE FIX — effectiveProbability must use the SAME spread resolution as the audio path
Replace the applyAnchorV1Only shortcut with the real spread resolution that reads spreadTargetMode:
- follow-CA mode (spreadTargetMode[lane]==1): own = PRE-remap (the voice's true own), target = the CA
  SOURCE's material (monoPreRemap/caSrc per applyMono/applyPoly), NOT V1. So spread 0 = voice's own
  (pre-remap), spread +1 = the CA source.
- anchor-V1 mode (spreadTargetMode[lane]==0): own = base, target = V1 — the current applyAnchorV1Only
  behaviour, correct ONLY in this mode.
i.e. the display must branch on spreadTargetMode exactly like applyMono/applyPoly do — ideally CALL the
same SpreadInterp entry the audio uses (applyMono for v==0, applyPoly for v>0), not the V1-only display
twin. The whole point: display spread == audio spread by construction. applyAnchorV1Only should be used
ONLY where V1-anchor is actually the mode (and arguably renamed / its uses audited — it's a foot-gun).

## WHY THIS IS THE UI-DIVERGENCE DISEASE (and a live argument for the decision authority)
effectiveProbability is the "one accessor" the bars read — but it reimplements spread with a V1-anchor
shortcut DIFFERENT from the audio's applyMono/applyPoly. That's a second spread computation that drifted
from the engine's — exactly the pattern the decision-authority note targets. The structural fix is that
the display computes spread via the SAME function as audio (or reads the audio's result). This bug is the
concrete instance; fix it by unifying the spread call.

## CONTRACT / VERIFY (Rack + test)
- follow-CA, v2←v3: spread 0 → v2 shows v2's OWN bars, v3 shows v3's OWN (DIFFERENT); spread +1 → v2 and
  v3 both show their CA SOURCE (v3's material for v2), NOT V1. Intermediate = interpolation own→source.
- anchor-V1 mode unchanged (spread→V1).
- DISPLAY == AUDIO: a test asserting effectiveProbability(voice,lane,step,Owner) equals the audio-path
  spread result (applyMono/applyPoly) for the same inputs, in BOTH spreadTargetMode values. RED on current
  (display uses applyAnchorV1Only always) → GREEN when unified.
- run_all.sh green.

## DONE =
- effectiveProbability branches on spreadTargetMode (follow-CA → CA source target, pre-remap own; anchor →
  V1), matching applyMono/applyPoly. No more unconditional applyAnchorV1Only for poly.
- Display==audio test (both modes) RED→GREEN. Rack: follow-CA bars show own@0, CA-source@+1, never V1.
- run_all.sh green. PR: the two bugs, the unified spread call, Rack confirmation.

## NOT THIS PROMPT
- The broader decision-manager (this is one instance; the authority is later). Rate-manager follow-ups.

────────────────────────────────────────────────────────────────────────────────
## REFINEMENT (Rodney + CC) — pub-reading approved; ONE shared decision, enforced agreement

CC correctly REJECTED the prompt's naive "just call applyMono/applyPoly in the display": those read the
LIVE slewed* buffers, and UI reading live slewed IS the V16-flicker bug (torn reads during the ~116µs
recomputes; the pub snapshots exist precisely to prevent that). So:

### APPROVED
- PUB-READING, mode-aware display (not live) — do NOT reintroduce the flicker. ✓
- Anchor mode BIT-IDENTICAL to today (the regression guard). ✓
- Rejecting 7 new pubPreRemap arrays: the one live preRemapSlewed* read is in the pin-change-GATED
  snapshot — a µs window on a user action, self-healing next frame; 7 arrays to cover a self-healing µs
  transient is a cost-justification mismatch. ✓ (good reasoning.)

### THE REFINEMENT — this is the probability-modifier-resolver disease (Rodney); fix it structurally
Rodney's framing: this is "different UI and engine view of CA-MODIFIED probability" — the modifier-
resolver / decision-authority disease, in the CA-spread domain. CA-SPREAD TARGET SELECTION (own vs
CA-source vs V1-anchor, + polarity) was NOT routed through a single resolver — it lives in
SpreadInterp::applyMono/applyPoly (audio) and was NOT taught to effectiveProbability (display). That gap
IS the bug.
CC's twin approach (applyMonoDisplay/applyPolyDisplay copying the branch structure) FIXES this instance
but re-creates the duplication: the follow-CA/target/polarity DECISION logic would exist TWICE (audio +
display twins), which can DRIFT later (change applyMono's CA logic, forget the display twin). PREFER:
- Factor the DECISION — "given spreadTargetMode[lane], which TARGET (own/CA-source/V1) and which
  POLARITY/endpoints" — into ONE shared helper that BOTH audio and display call, differing ONLY in the
  READ SOURCE (audio passes live slewed; display passes pub snapshots). One follow-CA decision, two
  read-sources. That is the "one computation, two read-rates" model (the resolver principle), not two
  twins kept in sync.
- IF the live-vs-pub read is too entangled to factor cleanly, CC's twins are an ACCEPTABLE fallback —
  but ONLY WITH the mandatory equivalence test below (non-negotiable; it is what converts "two twins that
  happen to agree" into "two twins that MUST agree or CI fails").

### MANDATORY TEST (the drift guard — required either way)
Assert the display spread result EQUALS the audio spread result for the same inputs, in BOTH
spreadTargetMode values (anchor AND follow-CA), in STEADY STATE (pub == live when no recompute in flight).
RED on current (display uses applyAnchorV1Only always) → GREEN when unified/agreeing. This test is the
structural enforcement: display CANNOT silently diverge from audio for the spread modifier.

### DOWN-PAYMENT ON THE MODIFIER RESOLVER
Doing it as ONE shared decision (not twins) makes this fix a down-payment on the probability-modifier /
decision authority: spread-target-selection becomes a single resolved decision both views read. That is
the structural end state; this bug is the concrete first case. (CA-spread was a GAP in the resolver scope
— the SandsModifierResolver handles spread/warp/LOR/direction DELEGATION, but follow-CA TARGET SELECTION
stayed in SpreadInterp outside it. Fold it toward the one-decision model here.)

PROCEED with: pub-reading (approved); ONE shared follow-CA decision if cleanly factorable, else twins +
the mandatory equivalence test; anchor bit-identical; no new pub arrays.
