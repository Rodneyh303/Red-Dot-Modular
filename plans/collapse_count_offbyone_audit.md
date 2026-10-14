# COLLAPSE TAIL — the `numPolyVoices`-as-bound count off-by-ones (voice-6-zero + siblings)

## Progress so far (good)
The [16][16] storage collapse + voiceSlot convention unification FIXED the original voice-2 off-by-one
(V2 no longer special). Remaining issues are a DIFFERENT, cleaner class: COUNT off-by-ones introduced BY
the collapse.

## The class (root)
Before: poly arrays were [15] (0-based, 0=V2), bounded by `numPolyVoices` (poly count, EXCLUDES V1).
After collapse: arrays are [16] V1-INCLUSIVE (0=V1, 1..=V2..). Any loop/bound/check still using
`numPolyVoices` (which excludes V1) is now SHORT BY ONE on the unified arrays -> it covers one voice fewer
than selected. The LAST selected voice (e.g. voice 6 of 6) falls just outside -> unpopulated/unrefreshed
-> reads ZERO.
Correct bound on a V1-inclusive array = `activeVoiceCount()` = `numPolyVoices + 1` (VoiceResolver.hpp:106).
The engine FILL loops already use the right form: `for (v=1; v<=numPolyVoicesHint; v++)` (+ V1 at 0) — these
are CORRECT, cover V1..V6. The BUGS are the sites that DIDN'T get updated.

## THIS bug — voice-6 shows zero Sands lane-prob (select 6, roll dice; v1-5 data, v6 zero)
CONFIRMED NOT the cause: engine fill (correct), voice tabs (setActiveCount=activeVoiceCount=6, correct),
prob-OUT CV (nCh=1+nV, voices 1-6, correct).
SO the bug is in the Sands BAR-DISPLAY per-voice fetch / SPREAD-REFRESH for the selected voice. Suspect a
`numPolyVoices` (5) used as a bound/index-limit where the unified [16] array needs activeVoiceCount (6):
e.g. cachedPolySpread refresh, polyVoiceActive[], refreshPolyRandomCell bounds, or the bar-fetch loop —
voice 6's display slot (or its per-step spread refresh) is bounded one short -> never populated -> zero.

## FIX = systematic audit (not one-at-a-time)
Audit EVERY `numPolyVoices` / `< nV` / `<= numPolyVoices` / `polyVoiceActive[...]` used as a BOUND or
INDEX LIMIT. For each, ask: does the array/loop it bounds now INCLUDE V1 at index 0 (post-collapse)?
- If YES (V1-inclusive [16] array, or a loop that should cover all active incl V1) -> change to
  `activeVoiceCount()` (= numPolyVoices + 1), or make it `for (v=1; v<=numPolyVoices; v++)` + handle V1.
- If NO (genuinely poly-only, e.g. a poly-param bank that still excludes V1) -> leave as numPolyVoices.
Grep targets: `< numPolyVoices`, `<= numPolyVoices`, `< nV`, `1 + nV`, `numPolyVoices)`, `polyVoiceActive[`,
`cachedPolySpread`, `refreshPolyRandomCell`. Especially the Sands display + spread-refresh paths (this bug)
and the East/expander managers.

## Verify
Select N voices (try N=2,6,15), roll dice: ALL N show nonzero Sands lane data (no zero on the Nth). The
displayed voice's bars match its prob-OUT CV. Bit-compare unaffected voices. 3 platforms.
Note: these count off-by-ones are the PREDICTABLE TAIL of the collapse (count excluded V1, array now
includes V1). A `numPolyVoices`-as-bound audit flushes them all in one sweep.
