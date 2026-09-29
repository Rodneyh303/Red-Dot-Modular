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

## Adjacency means different things in clock vs gate mode (Rodney)
The deeper framing: BOTH modes ask the same question — "are these two notes NEIGHBOURS (tie
candidates)?" — but from different information.
- **Clock mode: adjacency is STRUCTURAL.** Neighbour = the next STEP. The step counter defines it by
  construction, so there is no gap to measure, no tolerance, no timing test. `wasHeld` is SAFE here
  because the grid bounds it: a note's hold spans naturally to the next step boundary, and that step
  IS the adjacent one.
- **Gate mode: adjacency is TEMPORAL and EXTERNAL.** You do not own the grid — the incoming gates do —
  so "consecutive" is not defined for you; you INFER it from edge timing. The 1ms threshold is
  precisely reconstructing, from gate edges, the "are these consecutive?" that clock mode gets for
  free from its step counter.

So the BUG is: gate mode used clock mode's MECHANISM (`wasHeld`) without clock mode's GUARANTEE (that
the next event is structurally adjacent). In clock mode `wasHeld` is grid-bounded; in gate mode it is
unbounded because nothing guarantees the next gate is soon. The fix adds, in gate mode, the adjacency
guarantee the step grid provides automatically in clock mode.

### Subgate is the bridge case — adjacency may be STRUCTURAL again
When STEP_GATE is patched, gate mode REGAINS a grid (the subgate clock), so within it adjacency can be
STRUCTURAL (consecutive subgate cells) rather than 1ms-inferred — which is MORE consistent with clock
mode and more robust than measuring gaps. Two options:
- **(chosen for the bug fix) 1ms everywhere in gate mode** — one rule; get bare main-gate mode correct
  now.
- **(worth confirming) structural adjacency on the subgate grid when present; 1ms only for the bare
  main gate** — matches clock mode, at the cost of two code paths.
Recommendation: fix bare gate mode with 1ms now; then CHECK whether the subgate path already resolves
adjacency structurally (consecutive cells) — if so, do not force 1ms onto a grid that already defines
adjacency; let it be structural there, matching clock mode.

### Step 2 (follow-on, Rodney): infer the gap grid from the subgate clock
Once a subgate clock is patched, replace the 1ms TIMING inference with STRUCTURAL adjacency on that
grid: two main gates are neighbours iff no subgate cell lies strictly between them (previous gate
ended in cell K, this gate starts in K+1 -> adjacent; K+2+ -> gap). Grid PRIORITY:
**SUBGATE_RATCHET, or SUBGATE_GHOST if that is the one patched** — ratchet is the natural choice
(finest grid governing in-gate behaviour, so gaps align with how notes are already subdivided); and
since GHOST normals to RATCHET, "ratchet or ghost if patched" collapses to "whatever single clock is
patched" in the common case.
Why this is BETTER than 1ms, not just consistent: it is still decidable BACKWARD (you know the
previous gate's cell when this edge arrives — no lookahead), because the grid discretises the past
into countable cells rather than a continuous gap you would want to wait out. So it sidesteps the
causality constraint entirely. 1ms then survives ONLY for the bare main gate with NO subgate patched
(the one genuinely gridless case).
Order: (1) ship the 1ms bare-gate fix standalone; (2) switch to structural cell-adjacency when a
subgate is patched. Step 1 is shippable alone; step 2 makes the subgate case exact, matching clock mode.


## RESOLUTION (Rodney, after scoping Rampage correctly)
Scoping Rampage with BOTH rising and falling edges patched shows legato fires correctly — the earlier
"no legato" was a PATCH error (falling edge only), not the code. This confirms gate-mode legato is the
OVERLAP / HELD-PREDECESSOR model (the clock-mode slur-forward model applied to gate mode): legato
requires the previous gate still high across the boundary (`wasHeld || hadTail`), which is correct and
working. It is NOT gap-tolerant by design, and should not be made so — a source that drops the gate
with a real gap is not playing legato.

Consequence: **the 1ms gap-tolerance idea is retired.** It was solving a problem the held-predecessor
invariant already solves. Whichever way the redundancy experiment goes, the answer is NO TIMER:
- if the original long-gap bug stays fixed with `gate1Adjacent` forced true -> REMOVE the machinery
  (redundant; and it harmfully clears `slurForward` on gaps);
- if the bug returns -> the fix belongs in the NOTE-LENGTH nullification (stop a long note-value leaking
  held-ness), NOT in a gap timer — because legato is overlap-based, a timer is the wrong tool regardless.
The "keep it, tune tolerance to tens of ms" option (former step 3) is WRONG and dropped: it assumed a
gap-tolerance model that the Rampage result disproves.
