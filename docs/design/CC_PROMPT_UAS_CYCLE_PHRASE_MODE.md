# CC prompt — UAS "Cycle: Phrase" (default) vs "Cycle: Custom"

Rodney's ruling (2026-11-22). Today the Macro voice range seeds L = kPatternLengthCap (32) (bug 23), so a
16-step pattern sits over a 32-step voice window and its second lap uses different random numbers: the
pattern no longer repeats every lap. That is a change from pre-UAS behaviour and a surprising DEFAULT.
Fix it with a mode, not a magic value.

## The two modes (per Macro row and per voice row; delegation works as today)
- **Cycle: Phrase** (NEW, the DEFAULT everywhere): the voice cycle is locked to the global phrase.
  - L is not authorable: effective L = the transport/global pattern length (`cachedLength`), live.
  - O is not authorable: locked at 0.
  - R (rotation) = the transport/global OFFSET 1:1 (not user-settable; see RULINGS). Effect: the global offset slips the pattern around
    the probabilities, as in pre-UAS (the draw window stays the pattern length; the offset rotates the read).
  - UI: L, O and R shown greyed, not selectable, displaying the live effective values.
- **Cycle: Custom**: today's cycle mode (own L, O, R authorable). The opt-in for drawing over a window
  longer or shorter than the pattern (e.g. 16 over 32), polymeter windows, etc. Existing behaviour unchanged.
Mode is an explicit enum/flag stored per row (persisted, undoable, default Phrase). There is NO sentinel:
do not encode "follow" as a magic L.

## RATIONALE (Rodney, 2026-11-22 — record this in the code comments at the mode and in the UAS doc)
A lane's rotation R sets where the pattern wraps when the voice is at its first step. In Cycle: Phrase the voice
lane is therefore locked as: O = 0, L = transport length, R = transport offset. That makes the lane-level
behaviour (the "level 2" cycle) identical to pre-UAS: before UAS every lane was always at the global offset at
global step 0, i.e. the pattern simply started at the global offset and wrapped at the global length. Phrase mode
reproduces that by construction, with no special-case path. Custom mode is the UAS extension (independent L/O/R,
polymeter, longer-than-pattern draw windows) and is opt-in. Add a test whose oracle is exactly this statement:
"voice at first step reads the cell at the global offset; wraps at the global length" (Phrase mode, offsets and
lengths swept), and cross-check against a pre-UAS reference if the old 16-ring goldens still exist.

> **STATUS (2026-11-23): the pure LAW is DONE — `src/dsp/CycleModeCore.hpp` (resolveCycle,
> isIdentityCycle, readIndex, authoredOnModeSwitch), `test_cycle_mode` registered + green, mutation-
> proven (bug-23 'Phrase uses the cap' fails 68; slide-instead-of-rotate fails 2505). The engine must
> CALL this core, not re-derive the law: feed the CYCLE read the effective (L,O,R) from resolveCycle,
> and rewrite the identity predicates to isIdentityCycle. The core encodes the INTENDED law from the
> rulings — fact #2 below (what the engine reads TODAY at offset k) is still yours to establish; if the
> current code disagrees with the oracle, REPORT it, do not silently bend either side. Still engine-side:
> the wiring, the seeds, delegation, persistence/undo, the UI greying, the goldens, the slew boundary.**

## FACT 2 — SETTLED BY CODE READING (2026-11-23), THE ORACLE IS CONFIRMED
Traced `voiceFrameTickAt`'s CYCLE branch (SequencerEngine.cpp ~:916-920):
    phrasePos = wrapMod(effTick1based - 1, lenG)          // 0-based position WITHIN the global phrase
    wnow      = (walkFold(vd, phrasePos + 1, L) + R) mod L
and `walkFold(Forward, t, L) = laneTickFor(Forward,t,L) + anchorBias(Fwd=0) = t - 1`, so for forward
    **wnow = (phrasePos + R) mod L**
which is EXACTLY CycleModeCore::readIndex(step, L, R) = (step + R) mod L. NO DIVERGENCE — the engine
already rotates WITHIN the phrase by R. The code comment states Rodney's rationale verbatim: "R is the
voice's position when the transport hits step 1 ... the read RETURNS TO R at every global step 1."
TWO CONSEQUENCES THAT SHRINK THE WORK:
1. **O is UNUSED in the cycle read.** The cycle branch sets `rho = R` — NOT `R - O` (only the
   abs1/full-phrase branch uses `R - O`). So the "O locked at 0" ruling has NO read-path consequence;
   O only feeds the window/mask `setWindow` builds ([offset, offset+length), i.e. MASK_DYNAMIC).
2. **Phrase mode needs NO NEW READ PATH.** `voiceFrameTickAt` already calls
   `effectiveVoiceLor(v, L, O, R, vd)` and the cycle branch does the rest. Phrase mode reduces to:
   make `effectiveVoiceLor` (and its macro twin) return `resolveCycle(mode, lenG, offG, authored)`.
   The core engine change is FEEDING IT DIFFERENT VALUES, not rewriting the read.
CAVEAT ON THE CORE: `CycleModeCore::readIndex` encodes the FORWARD fold only. Reverse / Pendulum /
PingPong use different `walkFold` laws (Reverse also carries anchorBias=1), so the oracle covers
forward rows. `resolveCycle` / `isIdentityCycle` / `authoredOnModeSwitch` are direction-agnostic.
OPEN FOR RODNEY: is DIRECTION still authorable in Phrase mode? The rulings lock L/O/R but say nothing
about dir, and `isIdentityCycle` includes Forward — implying dir stays settable. If dir IS settable in
Phrase, a Phrase row can be Reverse and the read uses the reverse fold (still correct — it is the same
existing cycle path; only the core's forward-only readIndex oracle does not model it).

## Facts to verify BEFORE coding (read the code, do not trust this text)
1. `voiceFrameReadsFullPhrase` / `kRangeBaseReadsFullPhrase=false` (SequencerEngine.hpp ~936) is the retired
   ABS1 "full phrase" read (L = cachedLength, rho = R-O). Decide reuse vs build on the CYCLE branch
   (`voiceFrameTickAt` ~875-920: `phrasePos`, `rho=R`, `walkFold`). Preferred: feed the existing CYCLE read
   the effective (L,O,R) — no new read path.
2. `setWindow(length, offset)`: `startStep = offset`, window [offset, offset+length) on the 32 ring. Establish
   exactly which random cells a 16-step pattern with offset k reads TODAY, and what pre-UAS did (offset
   rotating within the 16 cells vs sliding over the ring). Rodney's intent: offset slips the pattern
   around the SAME probabilities. Write the oracle from that intent and report any disagreement with current
   behaviour; do not silently pick.
3. `voiceLorIdentity` / `macroRowIdentity` (SequencerEngine.hpp ~1187-1203) are `L==cap && O==0 && R==0 &&
   Forward`. They gate the pass-through fast path (UA_NONDIV goldens). Rewrite: identity = Phrase mode with
   R_eff == 0 (and Forward). Custom keeps the old test. The raw-tick polymeter law in `driverTickAt` /
   `voiceFrameTickAt` (`passG` arms for 16 and cap) is separate and must not change.
4. Seeds: SequencerEngine.cpp:236, Monsoon.hpp:1035/1040, Monsoon.cpp:96 (`editor.lorBase` stride 21), the
   StraitsEastSandsVisual menus (the "bug 23" cap text ~935/976), persistence, `lorInt` clamps (~339-344).

## Behaviour contract (tests, failing first, Rack-free, registered in run_all.sh)
1. Default engine, global LEN=16: the pattern repeats every lap (bitwise-equal slewed reads across lap 1 and
   lap 2 for all strands/voices). Fails today (second lap differs under L=32).
2. LEN=32: identical to today's default (full 32 draw).
3. Change LEN live 16->12->32: effective L tracks it with no stale latch; no step gap/double.
4. Global OFFSET=k in Phrase mode: the pattern is the unshifted pattern rotated by k within the phrase
   (oracle: recompute the unshifted read, rotate), same probabilities, same count of onsets. R_eff = k.
5. (void — Phrase mode has no user R; kept as a number so references stay stable.) Instead: the user cannot author L/O/R in Phrase mode (setters are no-ops or the UI never offers them) and the stored Custom values are untouched by Phrase operation.
6. Custom mode: existing goldens bit-identical (UA_NONDIV, the Stage tests); switching Phrase->Custom keeps
   the current effective (L,O,R) as the authored values (no jump), Custom->Phrase discards them.
7. Delegation: a voice row following Macro inherits Macro's mode; a localEast row has its own.
8. Persistence round-trip + undo of the mode change; old saves without the field load as Custom with their
   stored L/O/R (Rodney does not care about old patches; but loading must not crash or reseed).
9. Slew/draw boundary: in Phrase mode the draw cycle == global phrase wrap, so `result.wrapped` is the right
   boundary; in Custom with L != global length the boundary for slew is the wrap of the effective draw
   window (see CC_PROMPT_SCRUB32_BIDIR_SLEW_PHRASE.md Part A, amended).
Mutation-check: make Phrase read L=cap again -> test 1 fails; ignore the offset -> test 4 fails; drop the
identity rewrite -> the pass-through goldens or test 2 fail.

## UI (Straits East / Sands Macro lane-range editor)
Mode selector "Cycle: Phrase / Cycle: Custom" per row; L and O greyed and read-only in Phrase and showing the
live global values; R per Q1. Menu text must not say "cap" for the Phrase default. Needs Rack feel-test.

## RULINGS (Rodney, 2026-11-22 — these replace the open questions)
Q1. In Cycle: Phrase the user can set NONE of voice-lane L, O, R. L = transport (global) length, O = 0,
    R = transport (global) offset, 1:1. All three greyed and display the live effective values. NOT additive
    (the earlier "additive" default and test 5 are void: there is no user R in Phrase mode; delete test 5).
Q2. The global offset drives R 1:1 and R is where the pattern starts: the offset rotates the pattern within
    the phrase cells (same probabilities, shifted start). Fact 2 (what the current code reads) is still to be
    reported; the oracle follows this ruling.
Q3. The mode is a USER SELECTION: one on the Macro row, and per voice on East (a localEast row has its own,
    a following row inherits Macro's). Default Cycle: Phrase everywhere. Persisted + undoable.
Not in scope: scrub/slew (separate prompt), visual redesign of the range strips.
