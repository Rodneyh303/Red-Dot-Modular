# Probability-lane LOR commits at the wrong step after global transport < voice length

**Reported (Rodney, Rack):** fresh start, global 16 / voice 8 — every LOR modulation
(offset, length, rotation; all lanes) lands when the voice lane wraps to step 1,
as agreed. After setting the **global transport below the voice length** (e.g. 4)
for a time and restoring it to 16, the commits start landing at step **6**, another
time step **7** — global-invariant, "as if 6 became magic."

> **SUPERSEDED — read "## RESOLVED" at the foot first.** The `advanceOffset_` account
> below is a real *lever* (injecting it reproduces the step 6/7), but the same-seed A/B
> differential later showed `advanceOffset_` is a *symptom*, not the root: with static
> modulation the dip changes nothing; only with live modulation does it diverge, and the
> true cause is the cycle-mode nesting not clamping the voice/lanes/scan to the global
> window. The fix directions in this upper section are retained for the audit trail but
> are not the ones to build — the RESOLVED spec is.

## Root cause — first pass (lever, not root; see RESOLVED)

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

---

## RESOLVED — the root cause and the agreed spec (2026-11-18)

### What the evidence actually showed
A same-seed A/B differential (`test/diag_global_dip_advoffset.cpp`, branch
`debug/global-dip-logging`), two engines fed an identical gate stream, one holding
global=16 and one dipping 16→4→16:
- With modulation **static**, A and B are byte-identical — the dip perturbs nothing,
  `advanceOffset_` included. So `advanceOffset_` is not the cause.
- With a **live** variation-lane LOR CV swinging through the window change, the
  modulated+wandering voices diverge permanently from ~2 steps after the restore and
  the gap grows; the centre (un-modulated) voice stays byte-identical A/B — **no
  cross-voice leak**. So the perturbation is confined to voices that are both wandering
  and actively modulated, and it is the window change reaching their reads that does it.

### The root cause
In `SequencerEngine::voiceFrameTickAt`, the CYCLE branch anchors the **read** to the
global phrase — `phrasePos = wrapMod(effTick−1, lenG)`, "returns to R at every global
step 1, restarts its cycle from R each phrase instead of drifting" — but hands back a
**frame phase** on the RAW congruence (`wrapMod(effTick−1, walkPeriod)`, the comment:
"the phase stays the RAW congruence"). `tickHolders` keys the probability-lane commit
edge off that raw congruence (`wrapMod(t0EffNow, walkPeriod)==0`). And the variation
scan wraps by the raw lane length (`SequencerEngine.cpp:629`, "the LOCAL window scan,
never the global clock"). So the **read is anchored to the global; the commit edge and
the scan free-run.** That is the step 6/7 drift and the scan-overrun — one root: cycle
mode does not clamp the frame/commit/scan to the global window the read already honours.

### The agreed semantics (UAS — Rodney)
Cycle mode must align and re-align the voice to the global range; it never free-drifts.

1. **Dynamic clamp.** The effective window the voice cycles is
   `min(committed global, committed voice, committed lane)` — e.g. global 4 / voice 8 →
   the voice visits its first 4 steps; grow global back to 16 → it resumes all 8. The
   `min` is whatever is committed *now*, not latched at voice creation.
2. **Live boundary — NOT a precomputed period** (C1 + the live-boundary refinement,
   `DESIGN_POSITION_UNIFICATION.md:212–261`). Global length + offset are the STEP-rate
   class — the MeloDicer-native "patterns react instantly" controls: they reposition/
   resize immediately, next step. The voice/lane re-anchor boundary is evaluated **live
   each step** — "is my parent at `phrasePos==0` THIS step?" against the parent's
   *current* length — never a countdown or a stored cycle. So global 8→4 at step 8 →
   the voice re-anchors at **12 AND 16**, not 16; C2 (keep-counting) places the next
   boundary after a mid-phrase change. Concretely: the voice position/anchor is a live
   function of the global phrase position — exactly what `voiceFrameTickAt`'s cycle
   *read* already is (`phrasePos = wrapMod(effTick−1, lenG)`, recomputed every step) —
   so immediate global reaction falls out for free and there is no latched clamp to go
   stale. The re-anchor is the **global** phrase boundary (defined by the global via
   C2), not a self-referential voice period, so nothing is ever undefined. Only the LOR
   **values** latch at the boundary (C1b); the window geometry is live. (An
   implementation that precomputes a clamp period gets the step-12 case wrong — the
   doc's explicit warning.)
3. **Frame anchored to the global first step.** Voice-step-zero (where the probability
   lanes commit) coincides with the global first step via rotation R, re-aligning every
   global phrase. The wander lives *inside* the clamped window; it cannot push the
   downbeat off the global grid (Behavior 2 — not the free drift of today, Behavior 1).
4. **The variation scan respects the clamp.** The escape search traverses exactly the
   clamped window, not the raw variation-lane length. Forced by the `variation = 1`
   limiting case: always-advance is clamped to the window, so the scan — which is "where
   variation=1 would have gone" searched probabilistically — must traverse the same
   range, or the two disagree about what "advance" means.

Consequence: `advanceOffset_` can no longer accumulate past the window, so a global dip
leaves no residue — the voice re-anchors at the next *live* global-phrase boundary, which
moves immediately with the (STEP-rate) global length.

### Where it lives (fix surface)
- `voiceFrameTickAt` CYCLE branch — the frame-phase output must track the
  global-anchored position, not the raw congruence.
- `tickHolders` — the commit edge keys off the anchored frame, not `wrapMod(t0EffNow…)`.
- `SequencerEngine.cpp:629` — the scan modulus clamps to the effective window.
- All three are the same clamp; this is the concept face of **CA5** (the phase-holder
  skeleton) and should land as one change, reproduce-first against the differential.

### Reproduction / guard
`test/diag_global_dip_advoffset.cpp` (on `debug/global-dip-logging`) is the failing
differential; turn it into a pass/fail guard (A==B after the dip) once the clamp lands.
`MONSOON_DIP_LOG` on the same branch is the Rack confirmation, now optional.
