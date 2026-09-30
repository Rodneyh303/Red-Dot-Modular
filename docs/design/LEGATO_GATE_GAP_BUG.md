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


## EXPERIMENT RESULT — cause found, fix decided (evidence-led)
Ran the redundancy experiment. Table (Bridge = the IMPL 2b slurForward term in gateHeld; Timer =
gate1Adjacent):

| Variant | Bridge | Timer | Input | Result | Meaning |
|---|---|---|---|---|---|
| A | active  | OFF | long gap | Tie     | bug REPRODUCES — bridge keeps gateHeld/slurForward high across the gap -> wasHeld true -> tie |
| B | removed | OFF | long gap | NewNote | held-predecessor invariant ALONE fixes it (no timer) |
| C | removed | OFF | overlap  | Tie     | overlap still ties without the bridge (held-across works) |
| D | active  | ON  | long gap | NewNote | the timer is a BAND-AID over the bridge (and wipes slurForward -> clips phrasing) |

**True cause:** the `slurForward` term in Mode B IMPL 2b's `gateHeld = gate1High || ghostSounding ||
slurForward` re-asserts gateHeld across the gap, defeating the held-predecessor guard. The gate1Adjacent
timer was a band-aid for that and is harmful (clears slurForward, the legato carrier, on any >=1ms gap).

**FIX (decided): remove BOTH.**
- IMPL 2b: `gateOpen = !isRest && (gate1High || ghostSounding)` — drop the `slurForward` term, KEEP the
  ghost term.
- Remove the gate1Adjacent timer + gate1LowSamples accumulator entirely (Monsoon.cpp/.hpp, the
  ModeController + SequencerEngine parameter, the `if (!gate1Adjacent)` clear).
Result: gap -> fresh (B), overlap -> tie (C), no timer, no phrasing suppression. Overlap-only /
held-predecessor model, intact.

**Confirm before closing (the bridge existed for a reason — check it is gone, not masked):**
1. CLOCK-mode legato still works — a multi-step slur (note slurring across several steps) still holds.
   If IMPL 2b is gate-mode-only this is moot; if shared, verify (the bridge may have protected the
   clock-mode abutting-notes case).
2. Ghost legato across the gate->gap boundary still ties (the kept ghostSounding term) — a legato note
   carries into the first ghost cell.
Both green + forward/reverse bitwise green => closed.

## FINAL MODEL (Rodney) — sample-accurate overlap / <=1-sample tie; supersedes the timer thread
The timer/tolerance thread above is SUPERSEDED. Decision and the reasoning that forces it:

### Why not a gap tolerance (causal + accuracy, not taste)
To tie across a real gap you must HOLD the slur-candidate gate open and WAIT to see if a rise arrives.
That waiting is fatal twice over:
1. If you ultimately DON'T tie, you have held the gate open past its true falling edge -> the gate END
   is smeared by the wait -> NOT sample-accurate (a downstream envelope release starts late).
2. Waiting N ms to "think about" legato also DELAYS the next note's onset by N ms -> you corrupt the
   NEXT gate's start too. You throw out two edges' timing to maybe save one.
Both break the reversible/seed-deterministic contract. So: **sample accuracy is non-negotiable; no
wait-and-see gap tolerance.**

### The model
Legato is committed at the LEAD (slurForward, as today) but RESOLVED in real time by edges, because in
gate mode the note's true length is not known until it ends (unlike clock mode, where the grid
guarantees the landing). Resolve with NO lookahead:
- **Rise while the previous gate is still HIGH (overlap)** -> TIE. Zero latency, decided at the rise.
- **Fall then Rise on the very NEXT sample (<=1-sample gap)** -> TIE. At most a 1-sample gate
  perturbation, which is ~50x shorter than VCV's 1ms minimum-detectable gate, so it is below audibility
  AND below every downstream module's edge threshold — effectively free, no meaningful smear.
- **Fall then Rise >=2 samples later** -> FRESH note. This is exactly the boundary where "decide now"
  becomes "decide by waiting", and waiting is what breaks accuracy. So the cutoff is not a tuned
  threshold — it is the largest gap resolvable WITHOUT waiting (1 sample).
- **Fall then no Rise** -> the note just ends (gate closes sample-accurately).
Edge detection itself is 1 sample (Schmitt, already used: getGate1SchmittHigh), so none of this adds
latency.

This is Rodney's ORIGINAL instinct, restored: "gate high + another rising edge (overlap), or gate low
for one sample then a rise -> tie candidate; any gap >=2 samples -> fresh note."

### Why NOT the earlier "overlap-only" push, and why NOT the timer
- Strict overlap-only (rise-before-fall ONLY) was slightly too strict: the <=1-sample-gap case is also
  sample-accurate (1 sample is below every threshold) and should tie. Include it.
- The 1ms timer / any ms tolerance is too GENEROUS and non-causal (it waits, it smears, and it wrongly
  cleared slurForward). Removed.

### What this means for the USER (honest, document it)
A source whose consecutive notes have a MULTI-SAMPLE gap (most sequencers/Rampage phrasing gaps are
ms = tens-to-hundreds of samples) will NOT legato — correctly, because it cannot be done
sample-accurately. To get legato, the source must present overlapping gates or a <=1-sample gap, i.e.
genuinely hold/abut the gate across the boundary. NOTE: a Count Modula (or similar) TIE is NOT this — a
tie there is ONE continuous high gate = ONE note (no second onset), so it does not exercise the
two-distinct-onsets case at all. The test source must emit TWO distinct rising edges with a 0-1 sample
low between them (e.g. a high-duty square / a period-minus-one-sample pulse), not a held gate.

### Fix (both layers), superseding "remove both"
- Mode B IMPL 2b (Monsoon.cpp): tie candidacy = overlap (prev gate still high at rise) OR the
  fall-was-exactly-1-sample-ago case; else fresh. NO ms timer, NO unconditional slurForward bridge.
  slurForward remains the LEAD commitment (Lantern/SLEG) but does not hold the gate open across a
  multi-sample gap.
- Remove the gate1Adjacent ms-timer/accumulator (the ms version); if a <=1-sample check is cleanest as
  a tiny 1-sample-memory flag, that is fine — it is not a ms timer and does not wait.
- Reverse-safe: the 1-sample decision is edge-timed, direction-agnostic. Keep forward/reverse bitwise.

### SAME MODEL AT SUBGATE LEVEL (the reason to get it right now)
The subgate grid is NOT guaranteed regular (user may feed wonky or steady subgates). So subgate
adjacency uses the SAME edge rule, judged per event, NOT an assumed grid spacing: a subgate cell ties
to the next iff overlap or <=1-sample gap between the cell's own fall and the next rise. No grid-
regularity assumption anywhere — robust to wonky subgate input by construction.

### Test (needs a real 2-onset source — Rodney to supply)
Source emitting two distinct rising edges with 0-1 sample low between (high-duty square / pulse =
period-1). Assert: overlap -> tie; 1-sample gap -> tie; >=2-sample gap -> fresh; long gap -> fresh (the
original bug); held-single-gate (Count Modula tie) -> ONE note (not a legato pair). Forward/reverse
bitwise green.

## DECISION (Rodney): TWO modes via context menu — neither inference is universally right
Inference from gate timing cannot be BOTH sample-accurate AND gap-tolerant (needs the future). So do not
pick one globally — expose both as a context-menu choice, matched to the source:

- **Sample-accurate legato** — overlap / <=1-sample tie (the FINAL MODEL above). Correct, no gate-end
  smear, fully seed-REPRODUCIBLE. Requires a source that holds/abuts the gate across the boundary
  (e.g. Impromptu and similar can emit sample-accurate gates). A gapped source simply won't legato in
  this mode — correctly.
- **Tie across gaps** — the slurForward bridge to the next rise whenever it arrives. Works BROADLY
  (any gate sequencer), at the cost of: gate-END smear on a landed slur (the gate is held open to the
  next onset), and NOT seed-reproducible (the slur landing depends on live gate timing).

Justification for two: **not all sequencers can generate a 1-sample rise held high until the next rise
without dropping.** A sample-accurate-only instrument cannot legato with those; a bridge-only instrument
smears. Neither is wrong — they suit different sources.

### Default: TIE ACROSS GAPS
Most sources a user first patches will NOT produce sample-accurate gates, so a sample-accurate DEFAULT
makes the out-of-box experience "legato is broken" (the failure mode to avoid). Default to the mode that
works broadly; expose sample-accurate for users whose source is clean and who want determinism.
Per-mode caveat stated in the menu/manual: sample-accurate = reproducible; tie-across-gaps = broad but
not seed-exact and slightly smears the held gate end.

### Still true regardless of mode
- **No upper bound / time cap is needed (Rodney): the ARRIVING NOTE decides.** In tie-across-gaps mode
  the bridge keeps slurForward alive across the gap, but a tie only FORMS if the arriving note's own
  legato check (prevSlur + its legato roll) passes — it must NOT connect unconditionally just because
  the bridge was up. A note after a long silence won't roll legato -> fresh note. So the "long-gap bug"
  was really the JOIN connecting unconditionally, not the bridge lasting too long; re-consulting the
  arriving note's decision bounds it for free at any gap length. The bridge proposes; the next note
  disposes.
- Subgate uses the SAME chosen mode, judged per event, no grid-regularity assumption.
- slurForward stays the LEAD commitment either way; only its gate-bridging differs by mode.

### Supersedes
The "remove both / overlap-only-forever" conclusion is replaced by this two-mode choice. The FINAL MODEL
section above defines the sample-accurate mode; the bridge (pre-removal behaviour) defines tie-across-gaps.


## FINDING (Rodney, verified) — TWO kinds of "rest" are already structurally separate; gap has no representation
Checked the engine: executeModeB is **edge-driven** — its body runs under `if (gate1Rise)`. In a GAP
(gate low, no rise) it does nothing: no executeStep, no MonoDecision::Rest, no event. So:
- **Generated rest** = a real MonoDecision::Rest produced by executeStep AT A RISE when the rest lane
  rolls. It exists, is a decision, and feeds `restBeatsLegato`. (Clock and gate mode.)
- **Structural gap** = the silence between rises. NOT represented at all — no decision, no Rest. Just
  absence of a rise.
So the two are ALREADY not conflated (a gap never becomes a Rest, so restBeatsLegato never sees a gap).
Nothing to untangle there — the concern was that a gap might be synthesised into a Rest; it is not.

### Consequence for the design (clean separation, two controls)
Because executeModeB only fires at rises, the ONLY thing that carries a slur across a gap is the
MODULE-LAYER gate-hold bridge (gateOpen/slurForward in Monsoon.cpp) keeping the gate "held" across the
gap so the next rise sees wasHeld. Remove the bridge -> the gap is dead time and the next rise is an
unconditional fresh executeModeB call with no pending-slur memory (why removal killed gapped legato).
Therefore:
- **`restBeatsLegato` governs GENERATED RESTS only** (rolled at a rise). Unchanged.
- **The gap-handling MODE (sample-accurate vs tie-across-gaps) governs the BRIDGE only** (does the gate
  stay held across a structural gap). Separate control, separate concept.
Do NOT recruit restBeatsLegato to police gaps (an earlier tempting idea) — it would conflate a
generated-rest rule with structural-gap handling and couple two unrelated musical choices. Keep them
independent: generated-rest behaviour and gap behaviour are set by different controls.

## RESOLVED (Rodney) — two honest modes; the "advance on falling edge" idea is retired
Explored (thought experiment): a False mode where the FALLING edge holds the gate high AND advances the
playhead to the next step, re-drawing the slur there, so a slur is not blindly carried to the next rise.
REJECTED — it cannot distinguish a long pause from a fall-then-rise-1-sample-later, so a tight fall/rise
double-advances the playhead (fall advances, then the near-immediate rise advances again) -> spurious
playhead JUMPS. Root cause is the SAME causality wall as the ms timer: a falling edge cannot know whether
a rise is imminent (that is 1 sample of lookahead), so it cannot classify itself as "gap" vs "half of a
tight boundary". Every clever middle option needs lookahead and fails identically.

**Decision — the only two options that need no lookahead, exposed as a context-menu toggle
("Falling edge beats gate legato"):**
- **TRUE = abutting-gates-only (sample-accurate).** A falling edge ends note and slur. Ties only when the
  source holds/abuts the gate to the next rise (overlap / <=1-sample). Deterministic, no smear, needs a
  clean source (Impromptu, or logic/latch to hold the gate to the next rise). Never ties across a rest or
  a gap.
- **FALSE = tie across gaps into the next gate.** A slur-forward-COMMITTED note whose gate ends holds
  the gate high across the gap (bridge); when the next gate arrives, **legato continues INTO it** — the
  tie forms, because the PREDECESSOR committed. The lead-commits handshake (slurForward -> prevSlur) is
  applied unchanged across the gap; the gap only means the gate was held high in between.
  Chain rules at the arriving gate:
    1. if the arriving gate is decided a REST -> silenced, chain ENDS (a rest breaks it);
    2. else the note plays (tied IN from the predecessor) and REDRAWS its OWN legato to decide whether
       IT slurs forward to the NEXT gate. Commits -> chain continues; does not -> chain ENDS WITH this
       gate (it still tied in).
  So the arriving note's redraw governs tying OUT, NOT tying in — the incoming tie is already earned by
  the predecessor. **CORRECTION of an earlier note:** FALSE mode does NOT self-bound the gap. A committed
  slur ties into the next gate REGARDLESS of gap length; the ONLY brake on "tie across a bar of silence"
  is a REST decision on the arriving note. That is intended in FALSE mode. Not sample-accurate / not
  seed-exact (depends on live gate timing).
Default FALSE (works out-of-box with the common gapped source; TRUE for clean sources wanting
determinism). Same chosen mode governs subgate adjacency, per event, no grid assumption.
This SUPERSEDES all earlier framings in this doc (timer, remove-both, overlap-only-forever,
advance-on-fall). The two modes above are final.
