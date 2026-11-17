# Probability-lane LOR commits at the wrong step after global transport < voice length

**Reported (Rodney, Rack):** fresh start, global 16 / voice 8 — every LOR modulation
(offset, length, rotation; all lanes) lands when the voice lane wraps to step 1,
as agreed. After setting the **global transport below the voice length** (e.g. 4)
for a time and restoring it to 16, the commits start landing at step **6**, another
time step **7** — global-invariant, "as if 6 became magic."

## Root cause (reproduced headlessly, deterministic)

The probability-lane commit fires correctly at `voiceFramePhase_[v] == 0`. But the
voice-frame phase is `wrapMod(t0Now + advanceOffset_[v], walkPeriod(dir, L))`, so a
non-zero **`advanceOffset_[v]`** slides where phase 0 lands relative to the transport
the panel displays:

> **displayed commit step = voicePeriod − advanceOffset_[v]**

Injecting the divergence directly (`diag`, `advancePlayhead` harness):

| injected `advanceOffset_[v]` | commit lands at transport step |
|---|---|
| 0 | 0 (step 1, correct) |
| 1 | 7 |
| 2 | 6 |

→ exactly Rodney's 7 and 6.

`advanceOffset_[v]` is the per-voice advance **divergence** accumulator:
`SequencerEngine.cpp:632  advanceOffset_[i] += (dStep − dMainStep)` inside
`foldPerVoiceAdvance_`. Setting the **global length below the voice length** makes the
per-voice advance decision diverge from the main transport (the global wraps several
times per voice cycle), so `advanceOffset_` accumulates. Restoring the global length
does **not** clear it, and the voice-frame phase only re-seeds on the voice's **own**
key — `walkKeyOf(dir, len) = dir*64 + len` (`SequencerEngine.hpp`), which does **not**
include the global length. So the drift is latched: the commit is permanently offset
by the accumulated `advanceOffset_`, and only re-authoring the voice's own length or
direction (which changes its key → re-seed) would clear it.

This is NOT the bug6 commit-edge (that fires correctly at the true wrap — see
CC_FIX_BUG6_COMMIT_EDGE_FINDING.md). This is a separate, state-corruption bug upstream:
the voice-frame phase's transport alignment drifts and never re-anchors.

## What is reproduced vs still to confirm

- **Reproduced (deterministic):** the *lever* — any non-zero `advanceOffset_[v]`
  relocates the commit to step `voicePeriod − advanceOffset_`. Both of Rodney's
  numbers (6, 7) reproduce from `advanceOffset_` = 2, 1.
- **Not yet reproduced headlessly:** the *accumulation* itself — i.e. that global <
  voice drives `dStep ≠ dMainStep` in `foldPerVoiceAdvance_`. That needs the real
  per-voice advance path (`advancePlayhead` with a live `PatternInput`), which the
  minimal harness passes as `nullptr`, so `advanceOffset_` stays 0 there. One Rack log
  confirms it: print `advanceOffset_[v]` before the global dip, during global=4, and
  after restore — it should be 0 → non-zero → stay non-zero.

## Fix directions (pick after the accumulation is confirmed)

1. **Root — stop the spurious accumulation.** In `foldPerVoiceAdvance_` /
   `decideStepAdvance_`, a plain forward voice should not diverge from the main just
   because the global shrank below its length. Needs tracing why global<voice makes
   `dStep ≠ dMainStep` for a non-wandering voice.
2. **Re-anchor on the global transition.** When `cachedLength` changes, re-anchor
   `advanceOffset_[v]` for non-wandering voices (or fold a global-generation counter
   into `frameKey_` so the phase re-seeds). NB: merely re-seeding `voiceFramePhase_`
   does NOT fix it — `t0EffNow` already includes `advanceOffset_`, so the re-seed
   reproduces the same shifted phase. The accumulated `advanceOffset_` itself must be
   corrected.

Engine-only, headless-testable once the accumulation path is driven. The display half
(voice playhead vs commit step) is consistent with the engine here — the fix is the
latched `advanceOffset_`, not the panel.
