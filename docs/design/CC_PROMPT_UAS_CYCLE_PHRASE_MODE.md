# CC prompt — UAS "Cycle: Phrase" (default) vs "Cycle: Custom"

Rodney's ruling (2026-11-22). Today the Macro voice range seeds L = kPatternLengthCap (32) (bug 23), so a
16-step pattern sits over a 32-step voice window and its second lap uses different random numbers: the
pattern no longer repeats every lap. That is a change from pre-UAS behaviour and a surprising DEFAULT.
Fix it with a mode, not a magic value.

## The two modes (per Macro row and per voice row; delegation works as today)
- **Cycle: Phrase** (NEW, the DEFAULT everywhere): the voice cycle is locked to the global phrase.
  - L is not authorable: effective L = the transport/global pattern length (`cachedLength`), live.
  - O is not authorable: locked at 0.
  - R (rotation) follows the transport/global OFFSET. Effect: the global offset slips the pattern around
    the probabilities, as in pre-UAS (the draw window stays the pattern length; the offset rotates the read).
  - UI: L, O (and R, see Q1) shown greyed, not selectable, displaying the live effective values.
- **Cycle: Custom**: today's cycle mode (own L, O, R authorable). The opt-in for drawing over a window
  longer or shorter than the pattern (e.g. 16 over 32), polymeter windows, etc. Existing behaviour unchanged.
Mode is an explicit enum/flag stored per row (persisted, undoable, default Phrase). There is NO sentinel:
do not encode "follow" as a magic L.

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
5. Phrase mode with user R (if Q1 says additive): rotation = global offset + user R, modulo L.
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

## Open questions (report, default stated)
Q1. In Phrase mode is R fully slaved to the global offset (greyed), or does the user's R ADD to it? Default:
    additive (user R + global offset) — Rodney said both "can set rotation" and "rotation follows the
    transport offset". Make it one constant/branch so it is trivial to flip; tell Rodney which you chose.
Q2. Should the global offset in Phrase mode rotate WITHIN the phrase cells (pre-UAS) or slide over the 32
    ring? Intent above is rotate-within; fact 2 decides what current code does.
Q3. Where is the mode stored for the Macro row vs each voice (editor state `voiceLorMacro` / `voiceLorEast`)?
Not in scope: scrub/slew (separate prompt), visual redesign of the range strips.
