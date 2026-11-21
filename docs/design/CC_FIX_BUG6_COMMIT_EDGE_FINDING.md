# BUG 6 — commit-edge investigation: reproduce-first result

**Status: the engine edge is CORRECT. Do NOT apply the planned "ordering fix."**
The remaining discrepancy Rodney sees in Rack is NOT the engine commit edge.

## What BUG 6 claimed

After the bug6 change (`b3ba2ea8`) the probability-lane offset CV was reported to
commit at *irregular* points: "modulated offset range doesn't move always at voice
wrap; sometimes yes, sometimes no, sometimes at other points in voice travel", and
visually "voice lane wraps, advances 1 step, THEN the modulation impacts the lane".

The suspicion (carried into this session) was that the new edge
`wrapMod(t0EffNow, walkPeriod(eD, eL)) == 0` — computed from the LIVE-modulated
period — scatters the commit, and the fix was to re-derive the commit from the
stably-maintained `voiceFramePhase_` via an ordering change.

## Reproduce-first — and it did NOT reproduce

`test/diag_bug6_commit_points.cpp` drives the exact scene (8-voice frame, a
3-length rhythm probability lane — the 3-against-8 tresillo) and prints, per tick,
the TRUE voice-frame wrap (`laneTickFor(dir, phase+1, L) == 0`) against the
committed lane offset. Four scenes:

| Scene | Setup | Result |
|-------|-------|--------|
| A | static global=8, windowed voice len 8 | offset CV commits **once, exactly at the wrap** (t=8) |
| B | global length modulated 8↔7, windowed voice len 8 | commits **once at the wrap**; the global flip never moves the windowed voice's period |
| C | global length modulated 8↔6, **MASK_DYNAMIC** full-phrase voice | commits at the **32-ring wrap**; global range does NOT move the full-phrase period either (`kMaskRing = 32`, a constant) |
| D | the voice's **OWN length** CV'd 8↔5 | commit **follows the moved frame**, always landing at the true wrap of the CURRENT period — never mid-travel |

**No off-wrap scatter in any scene.** `test_lor_phrase_latch` (P1–P12) is green,
P12 included: the window moves ON the wrap step (t=8), not one later.

Why the "unstable recompute" is in fact stable: `wrapMod(t0EffNow, period)` and the
maintained `voiceFramePhase_` are congruent (`phase + kV ≡ t0EffNow`), and the
re-seed on key-change (`frameKey_[v] != key`) re-aligns them the instant the period
moves. So even under Scene D's period modulation both agree, and both land on the
true wrap.

## The real coupling (answers Rodney's "global range → voice range" question)

The probability-lane latch period is:
- a **windowed** voice → the voice's OWN length (`effectiveVoiceLor` L); or
- a **full-phrase** (`MASK_DYNAMIC`, i.e. `voiceFrameReadsFullPhrase == true`) voice
  → `kMaskRing = 32`, a CONSTANT.

Neither is the live global window length. **Global range never moves the
probability-lane commit edge.** The only input that relocates it is the voice's own
length/direction — and when that moves, the commit correctly chases the true wrap
of the new period (Scene D).

Consequence worth flagging to Rodney: for a **full-phrase voice**, the lane latches
on the 32-ring, NOT the visual window. So the commit lands every 32 steps at a point
that visually drifts against an 8-window — which reads exactly as "sometimes at the
wrap, sometimes not." If that is the scene he was testing, the behaviour is the
design (full-phrase = abs1 = the ring is the frame), not a bug. Open design question
if undesired: should a full-phrase voice latch its lanes on the ring or on its
window?

## Why the UI looked "more off" after bug6

bug6 removed a real one-step lag (the old entry-phase edge committed at wrap+1;
P12 proves wrap is correct). The ENGINE got better. If the Rack display half still
reads on the old wrap+1 convention (or samples the post-advance
`voiceFramePhaseEdgeSnap_` a frame late), it is now out of step with the corrected
engine by one step — which presents as "the UI is more off". That is display-side
(Rodney's feel-test half), not the engine edge.

## Recommendation

1. **Do not** reorder/replace the engine commit edge. Reproduce-first failed to
   reproduce scatter, the battery is green, and the change would risk regressing the
   now-correct true-wrap timing (P12).
2. The residual is one of: (a) full-phrase lane latching on the 32-ring (design
   question above), or (b) a display-side one-step mismatch now that the engine
   commits at the true wrap. Both are Rack feel-test findings, not headless-fixable
   from the engine edge.
3. Guard the finding: `test_bug6_commit_wrap_guard` (converted from the diagnostic)
   pins "lane CV commits exactly once per voice frame at the true wrap, under global
   modulation and full-phrase" so a future change can't silently re-break it.
