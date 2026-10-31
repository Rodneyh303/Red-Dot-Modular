# PLAN — per-voice step-advance (poly stuttering), sequenced AFTER UAS (Rodney)

## SEQUENCING (decided)
1. FINISH mono step-advance now (freeze-fix + bipolar + mono-chevron scan display) on
   fix/subgate-merge-onto-straits-base. Ship mono-only advance (one shared playhead, mono decides).
2. MERGE fix/subgate-merge → trunk.
3. MERGE / rebase unified-addressing (UAS) → it settles the clock↔laneTickV_ model.
4. THEN make step-advance PER-VOICE (poly stuttering) — built on the settled UAS clock model.
Per-voice advance touches the same clock↔per-voice-tick relationship UAS is resettling, so do it AFTER UAS,
not on a clock model about to change.

## WHY PER-VOICE IS THE TARGET (Rodney)
Everything designed is for poly voices making their OWN gate decisions (Cantonment, per-voice subgate
decisions, the delegation authorities). Voices ALREADY sit at different positions (per-voice LOR / UAS
per-voice addressing, laneTickV_). Mono-only advance is the ANOMALY; per-voice stuttering is the CONSISTENT
design — voices advancing independently (anti-correlated advance = the cross-voice sequence-position scatter
the reachable-structures theory predicted).

## THE KEY DISTINCTION — LOR is USER-set; scanOfs is ENGINE-computed (Rodney)
Two different kinds of offset into the SAME variation window, composed:
- LOR (length / offset / rotation): USER-SET and modulated — where the user POSITIONS the window and the
  phase into it. User intent.
- scanOfs: ENGINE-COMPUTED — the engine's internal search cursor within the user-positioned window ("how
  far I've scanned for the advance-escape value"). NOT a user control; an engine mechanism (0..varLen-1,
  phrase wrap).
Read position = user's LOR view, THEN the engine's scanOfs on top. For POLY: each voice has its own USER-set
LOR (exists) AND its own ENGINE-computed scanOfs[v]. User controls WHERE (per-voice LOR); engine controls
the SEARCH WITHIN (per-voice scanOfs). Clean separation.

## SCOPE OF THE PER-VOICE CHANGE (bounded — NOT "N new clocks")
- scanOfs → scanOfs[v] (an ARRAY of the existing bounded offset; trivial).
- The advance DECISION becomes per-voice: stepAdvanceDrawAt(voice, delta) reads laneTickV_[voice]
  [STRAND_VARIATION] + delta and warpRow(voice, …) — the per-voice variation window that ALREADY exists
  (cf. SequencerEngine.cpp:1788). Currently stepAdvanceDrawAt is mono-only (hardcoded VoiceResolver::mono()
  / laneTick_).
- THE REAL WORK: each voice's laneTickV_ must advance on ITS OWN decision, instead of all recomputing from
  the one totalStepsElapsed (today laneTickV_ is "a CACHE recomputed from totalStepsElapsed", :394). So
  voices become INDEPENDENT advancers. This touches the one-clock→all-laneTickV_ invariant (the same thing
  UAS/DNA_LCM touch) — hence sequencing AFTER UAS.
- Display: each voice's variation chevron shows ITS OWN scan (reading its own stepAdvanceStrandIdx(voice,
  scanOfs[v]) — the display-reads-engine single-source rule, per voice).

## NOT A MAJOR ARCHITECTURE CHANGE (correcting an earlier overstatement)
The scan was never a clock — it is a bounded offset. Per-voice is "an array of that offset + per-voice
advance of the existing per-voice ticks", not "N new clocks". It IS bounded; it just touches the clock↔tick
relationship UAS is resettling, so it lands cleanly AFTER UAS.

## STRONGER REASON TO SEQUENCE AFTER UAS — wrap behaviour (Rodney)
The scanOfs (and per-voice scanOfs[v]) WRAPS at the phrase/variation-window boundary. UAS is precisely the
thing that REDEFINES wrap: the nested maps, the global-length-wraps-the-read fix, the direction/wrap
composition across global/voice/lane levels, and the parked DNA_LCM-kill (which changes the wrap MODULUS
entirely). So building poly stutter BEFORE UAS would wrap scanOfs[v] against the CURRENT wrap semantics,
then UAS changes them → rework or silent-wrong-wrap. Building it AFTER UAS means scanOfs[v] wraps against
the SETTLED final wrap model — one correct implementation, no rework. The scan is "local window scan,
wrapping at the window boundary", and WHAT that boundary is / HOW it wraps is UAS's domain. Poly stutter
consumes UAS's wrap model; it must not predate it.
