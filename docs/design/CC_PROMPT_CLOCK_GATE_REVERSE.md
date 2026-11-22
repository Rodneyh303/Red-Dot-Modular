# CC prompt — reverse mode for the CLOCK and GATE timing origins

Branch: `fix/ca5-walkkey-gate` (prompts) → engine work on a fix branch off it. COORDINATE
with the legato-tie work: gate reverse edits `executeModeB`/`executeModeBSubdivided`, the SAME
functions the tie work touches — land this AFTER the tie work merges, or rebase carefully.
Rack behaviour is Rodney's feel-test; the traversal/determinism below is Rack-free testable
(compile against the engine with `CPLUS_INCLUDE_PATH=/home/claude/simde`).

## The mode model (Rodney's correction, verified MonsoonModeController.cpp ~455-470)
There are THREE timing origins, cycled by the one MODE button (Clock-Gate-Phase):
  - CLOCK  = modeId 0, `executeModeA` ("Mode A")
  - GATE   = modeId 1, `executeModeB` ("Mode B")
  - PHASE  = modeId 2, `executeModeE` ("Mode C" to the user; `executeModeE` is just the phase
             origin's CODE PATH — a legacy name, NOT a separate user mode).
Q-MIX (generate vs quantise) is an AXIS engaged per step (beginQuantiserSource_), NOT a mode: the old
C/D/F "quantiser modes" are gone (MODE_COLLAPSE_6_TO_3 — "that timing origin with q-mix up"). So
reverse has to work for exactly two origins, CLOCK and GATE; PHASE already reverses.

## The mechanism that already exists (verify by reading, do not trust this text)
Two reverse axes, both wired TODAY for the PHASE origin and for neither clock nor gate:
1. WITHIN-DRAW PLAYHEAD TRAVERSAL — `executeModeA(rm, noteVal, in, int dir=+1)` passes `dir`
   to `advancePlayhead(dir)` (`dir<0` = reverse traversal within the pattern window,
   SequencerEngine.hpp:1816). The playhead walks the window backward: RETROGRADE.
2. DRAW-COUNTER REVERSE — `PatternEngine::setReverseActive(bool)`: the signed draw index steps
   −1 per boundary (regenerating the PREVIOUS draw) instead of +1. This is the scrub/canon axis.
The PHASE origin drives BOTH from `phase.reverse` (Monsoon.cpp ~1040-1053: `setPhaseReverse(eff)` +
`engine.pe.setReverseActive(eff)`); its code path `executeModeE` passes `phaseReverse ? -1 : +1` to
`executeModeA` (MonsoonModeController.cpp:330). So the phase origin is the working reference — REUSE
its plumbing; do NOT invent a second mechanism. (Note the phase path reuses `executeModeA` as its
executor, which is why the engine already honours `dir` — the clock origin just never passes it.)

## State today
- CLOCK origin: the ENGINE's `executeModeA` accepts `dir`, but `ModeController::executeModeA()`
  (line 337) calls it WITHOUT a dir → always +1. No reverse source, no `setReverseActive`.
- GATE origin: `executeModeB(gate1Rise, gate1High, noteVal, in)` and `executeModeBSubdivided(...)`
  take NO `dir` at all. Reverse needs an ENGINE signature change threading `dir` to
  `advancePlayhead`, plus the gate-specific subtleties below.

## Control source — DECISION for Rodney (no clock/gate reverse input exists today)
Phase reverse derives from the phase ramp; clock and gate have no direction of their own, so reverse
is a USER control. Options (Rodney picks):
  (a) a dedicated REVERSE gate/CV input (high = read backward) — consistent, patchable, the
      canon use wants a jack;
  (b) a reverse button/param;
  (c) reuse an assignable mod (Gate3 target) — cheapest, no new port.
Recommend (a) a reverse gate/CV, since the crab-canon use (one Monsoon forward, one reverse) wants to
drive it from a patch.

RESOLVED (Rodney 2026-11-22) — the DRAW AXIS acts EXACTLY like phase reverse: the control sets BOTH
axes, `setReverseActive(rev)` AND the `dir` passed to the executor. Verified: `rhythmDrawDir()`/
`melodyDrawDir()`/`qmixDrawDir()` use `reverseActive ? -1 : +1`, and `advance*Draw` steps the counter
by it (PatternEngine.hpp ~936-951). So reverse makes the draw counter step −1 per boundary,
REGENERATING THE PREVIOUS DRAW — "forward becomes last draw backwards next": the draw sequence is
retraced, not just the within-draw playhead. Identical to what the phase origin already does (same
`setReverseActive`), so clock/gate reverse is behaviourally the same as phase on the draw axis — no new
draw logic, wire the same call. The Gaussian-copula reversibility makes the retrace exact (forward then
an equal reverse returns to the same draw state); pin it in a test.

## Clock (Mode A) — the small half
- Add a reverse source (per the decision) read in the controller; pass `rev ? -1 : +1` as `dir` to
  `engine.executeModeA(...)` in `ModeController::executeModeA()`; call `engine.pe.setReverseActive(rev)`
  where the phase path does (so the draw axis matches). Keep forward bit-identical when rev=false.

## Gate (Mode B) — the engine half
- Thread `int dir=+1` through `executeModeB` and `executeModeBSubdivided` to `advancePlayhead(dir)`.
- SUBTLETIES to get right (each a test): the BOUNDARY INTERRUPT clear-at-wrap (executeModeA/B comment
  ~2344/2490) must fire at the reverse wrap (window start, not end); the SUBGATE ratchet/ghost edge
  streams walk the SHARED playhead (executeModeBSubdivided) — in reverse they must step it backward
  consistently (the ratchet advance −1, the ghost in-gap read mirrored); `legatoCheckpointOnFall`
  (the fall advanced one step — SequencerEngine.hpp:286) must advance in the reverse direction; the
  per-voice STEP-ADVANCE net offset (advanceOffset_) composes with reverse. Gate mode is externally
  clocked, so "reverse" = each external gate edge advances the READ backward through the pattern,
  time still moving forward.

## Tests (failing first where the behaviour is absent; Rack-free; register in run_all.sh)
Engine-level (compile against SequencerEngine/PatternEngine; the forward path is the oracle):
1. CLOCK retrograde: N forward steps capture the read sequence; N reverse steps (dir=-1 from the same
   seed/window) read that sequence REVERSED; a full reverse lap returns the playhead to the start.
   (The engine supports dir=-1 already — this pins it as the spec and proves the clock engine half;
   the controller wiring is then the only clock gap.)
2. DRAW RETRACE (the draw axis, phase-equal): with setReverseActive(true) the draw counter steps -1
   per boundary — the PREVIOUS draw regenerates. Forward k boundaries then reverse k boundaries returns
   the draw counters (rhythm/melody/qmix) AND the slewed buffers to the starting draw state, bitwise
   (the copula reversibility). Clock and gate origins both; byte-equal to the phase origin's reverse on
   the same seed.
2. CLOCK reverse determinism (canon tie-in): a forward engine and a reverse engine, same seed/length,
   produce MIRRORED reads — ties to CC_PROMPT_SCRUB32_BIDIR_SLEW_PHRASE test 1 (the counter-offset mirror) and the
   crab-canon use.
3. GATE retrograde: executeModeB with dir=-1, each simulated gate rise reads one step backward; the
   reverse read sequence == the forward sequence reversed; the boundary interrupt clears at the reverse
   wrap. Fails today (no dir param) — the reproduce-first compile failure, then green.
4. GATE SUBDIVIDED reverse: a ratchet edge and a ghost edge in reverse walk the shared playhead
   backward correctly (the subgate streams stay consistent); legato checkpoint-on-fall reverses.
5. Forward bit-identity: rev=false everywhere is byte-identical to today (the whole battery + a
   targeted golden). The DEFAULT must not move.
Mutation: gate dir not threaded to advancePlayhead → test 3 fails; boundary interrupt keyed to the
forward wrap only → the reverse-wrap gate-clear test fails; setReverseActive not called for clock →
test 2 (draw-axis mirror) fails.

## Report
Red/green, the control-source choice implemented, whether clock/gate reverse drives the draw counter,
and any gate subtlety that needed a ruling (subgate reverse semantics especially).
