# Bug: legato bridges across LONG gaps between main gates (old, pre-subgate)

STATUS: bug present in plain gate mode (no subgate needed); an attempted fix on
feat/subgate-poly-width over-corrected and killed ALL legato. This note pins cause, the failed fix,
and the correct fix with a sourced threshold.

## Symptom
In gate mode, two main gates separated by a LARGE gap still tie/legato — a held note bridges a clearly
open gap. Not subgate-related; reproduces with only the main gate patched.

## Cause
Legato keys off `wasHeld` (= `gs.gateHeld || holdRemain > 0`), which is NOTE-DURATION, not gate
adjacency. A long note-value keeps `holdRemain > 0` so the previous note is still nominally sounding
when a much later gate arrives -> `wasHeld` true -> the legato branch
(`legatoConnects && (wasHeld || hadTail) && prevPlayedSounded`, SequencerEngine.cpp ~512 and ~538)
fires across the gap. The comment at ~481 says legato is "temporal, connected to the predecessor" —
that INTENT is right, but the code proxies temporal adjacency through note duration, which is the bug.

## Why a wide tolerance is impossible (causal, not tuning)
You cannot look into the future. At a rising edge you cannot WITHHOLD the note to see whether a near
edge arrives to tie to — that is latency a real-time module cannot take. So adjacency can only be
judged from the PAST: was the previous gate still high (overlap), or was it low only very briefly. A
"gap <= 25% of the interval" rule is unimplementable because by the time you know the gap size you have
already had to emit the note. Backward-only.

## Threshold: 1ms (sourced)
Andrew Belt / VCV: **"1ms is the minimum gate/trigger length guaranteed to be detected by all standard
modules' gate inputs."** (community.vcvrack.com/t/.../11303/3). Use 1ms as the gap threshold:
- gate low for **< 1ms** before this rising edge (or overlap/abut) -> ADJACENT -> legato candidate;
- gate low for **>= 1ms** -> a real, intended separation -> FRESH note.
1ms = `sampleRate/1000` samples, computed at runtime (tempo/SR-honest), and it makes Monsoon agree with
the whole ecosystem about what counts as a gap. Overlap (prevGate1High true at the edge) short-circuits
to adjacent with no measurement — this is what makes a 99%-duty LFO and Rampage's back-to-back falling
edges tie correctly.

## The FAILED fix (what went wrong on d0b8ea4)
executeModeB added an UNCONDITIONAL clear at a gate rise (`gs.gateHeld=false; gs.holdRemain=0; ...`,
"Applied before wasHeld capture"). Because `wasHeldMono` is captured JUST AFTER (from
`gateHeld || holdRemain>0`), zeroing the held state on EVERY rise makes `wasHeld` ALWAYS false at a
rising edge -> legato can never fire -> even overlapping LFO/Rampage got no legato. It broke adjacent
ties (baby) to stop distant ones (bathwater). Two problems: the clear is UNCONDITIONAL (no adjacency
test) and it is PRE-CAPTURE (so any clear destroys the legato signal).

## The CORRECT fix
Make the held-state clear fire ONLY on a genuine gap, leaving overlap/sub-1ms untouched:
- At a rising edge, compute the low-duration since the gate last fell (accumulate samples while
  `!gate1High`; overlap = `prevGate1High` still true -> low-duration 0).
- If low-duration **>= 1ms** -> clear `gateHeld`/`holdRemain` (break the stale bridge) so `wasHeld` is
  false -> fresh note.
- If low-duration **< 1ms** or overlap -> DO NOT clear -> `wasHeld` stays true -> legato candidate as
  before (still subject to legatoProb / prevSlur).
- Add the adjacency term to BOTH sites (~512 `slurReachesHere` and ~538 the commit branch), consistently.

## Reverse-safety
Measure the gap from edge TIMING (a low-duration accumulator reset on the falling edge), not a
forward-only sample counter, so it is direction-agnostic like the rest of the legato model (see the
reverse caveat at ~484). Verify a bitwise forward/reverse test still passes with the gap logic in.

## Tests
- long gap (>= 1ms low) between two gates -> FRESH note (the bug);
- overlap (99% duty: gate high when next rises) -> TIE candidate;
- back-to-back / <1ms gap (Rampage consecutive falls) -> TIE candidate;
- normal adjacent phrasing -> unchanged;
- forward vs reverse bitwise identical with gap logic active.
