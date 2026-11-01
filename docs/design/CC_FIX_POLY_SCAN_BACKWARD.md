# CC FIX — poly scan looks FORWARD while travelling backward; sign by voiceSign_ (real travel) (Rodney)

SYMPTOM: a poly voice travelling BACKWARD still scans FORWARD for the escape probability. The scan must look
in the DIRECTION OF TRAVEL (backward while travelling backward) — as the mono fix already does.

## ROOT CAUSE — the scan is signed by the THRESHOLD's sign, which poly can't carry
The scan delta is signed by `variationProb < 0`:
- Decision: SequencerEngine.cpp:548 `signedDelta = (p < 0.f) ? -drawTickDelta : drawTickDelta` (p = the slot's
  variationProb / subgate ADVANCE).
- Display: SequencerEngine.cpp:2249 `(voices[bank].variationProb < 0.f) ? -scanOfsV_[v] : scanOfsV_[v]`.
BUT poly voices[v].variationProb is [0,1] — it CANNOT be negative (ModeController.cpp:68-69, comment: "the
poly per-voice VAR knobs are [0,1]... no negative modulation source yet... the clamp is a no-op today"). So
`variationProb < 0` NEVER fires for poly → the scan ALWAYS signs forward, even when the voice travels
backward (the backward motion comes from a delegated/global bipolar value, not the voice's own [0,1] field).

## THE FIX — sign the scan by voiceSign_[v] (the REAL travel direction), not by the threshold's sign
voiceSign_[v] (SequencerEngine.cpp:705-706) is the voice frame's INSTANTANEOUS travel direction, derived from
the ACTUAL per-voice step motion (dstep). It is the ground truth of which way the voice is going, regardless
of HOW backward is sourced (own knob, delegated global, CV). Sign the scan by it:
- DISPLAY (:2249): return (voiceSign_[bank.v] < 0) ? -scanOfsV_[bank.v] : scanOfsV_[bank.v];
  (The chevron then scans in the shown travel direction.)
- DECISION (:548): the scan should LOOK in the direction the voice is travelling. Sign drawTickDelta by
  voiceSign_[vs.v] (last step's real direction), NOT by p's sign. p still provides the MAGNITUDE/threshold
  for decide(p, u); only the scan-LOOK direction comes from voiceSign_. (At the first step voiceSign_ may be
  0 — treat 0 as forward, same as p>=0 today.)
- MONO is unaffected/consistent: mono variationProb IS bipolar, and voiceSign_[0] tracks mono's real
  direction too — so signing by voiceSign_ is correct for mono as well (byte-identical where mono was
  already right: forward travel → +delta).

## RELATED ROOT (the real underlying gap — poly VAR must be bipolar)
This is downstream of the "unipolar poly is a special case" item: poly per-voice VAR is [0,1] while mono is
bipolar [-1,+1]. If poly variationProb were properly BIPOLAR (per CC_PROMPT... poly-bipolar), `variationProb
< 0` would carry the sign. But voiceSign_ is the MORE ROBUST source regardless (it handles delegated/global
backward, where the voice's own threshold is positive but it rides a global negative). So: sign by voiceSign_
NOW (fixes the symptom for all backward sources); the poly-bipolar work (per-voice negative source) is the
separate parity item and both should land.

## VERIFY
- A poly voice travelling backward scans BACKWARD (chevron walks backward looking for the escape value).
- Forward travel unchanged (byte-identical). Mono unchanged.
- Delegated poly voice riding a global backward value scans backward (voiceSign_ catches it; variationProb<0
  would not).
