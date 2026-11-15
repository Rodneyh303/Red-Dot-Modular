# SANDS PROBABILITY-BAR LOR EDITING  bug list, status and the test plan

Focus (Rodney): shake all bugs out of the Sands probability-bar LOR editing. It has never been seamless.

## STATUS RECONCILIATION (2026-11-14)
This ledger had drifted: every regression below was marked OPEN, but the fixes had landed and the doc was
never updated  the inverse of the usual "fixed" lie (it read as broken while the code was fixed). Statuses
below reconciled against the ACTUAL CODE on feat/phase-duals-gaps @ dea7d708 (and, where noted, the
laneSeed fold fix still only on fix/macro-voicelane-shared-frame, pending merge).

HOW VERIFIED  and its limits (so the next reader trusts the right amount):
- Each "FIXED" was confirmed by READING the named code site AND by the headless battery passing. The full
  run_all battery is GREEN  but on ONE leg here (g++ + SIMDe), not Rodney's two-leg + real-SDK re-run.
- "FIXED (code)" on the audio/Macro items (16, 19-21) means the mechanism is in the code and the headless
  pins pass. It is NOT a Rack feel-test. Rodney feel-tests; those stay the human gate, as always.
- Bug 22 is the one partial: the gate-OFF half is in (Lantern records the gated jack), the "distinct dimmed
  cue" half is NOT confirmed (records Inactive = the doc's own fallback). Flagged, not claimed.

ONE-LINE LEDGER (detail in the sections below, which are kept as the record of HOW each was fixed):
  1-6 FIXED (gesture core + named constants) | 7 FIXED | 8 VERIFIED (no bug) | 9-15 FIXED (drag reducer +
  gate suite) | 16 FIXED (code; output-gate composition restored in MonsoonOutputGenerator) | 17-18 FIXED
  (shared lane playhead) | 19-21 FIXED (code; resolver bypass removed) | 22 PARTIAL (gate-off in; dim cue
  unconfirmed) | 23 FIXED (identity sentinel -> kPatternLengthCap).
  SEPARATE, PENDING MERGE: the Macro-lanes-fold-over-global-length SEED bug  FIXED on
  fix/macro-voicelane-shared-frame (one laneSeed helper, test_macro_lane_seed 9/9), NOT yet on trunk.

## BUG LIST (status reconciled against feat/phase-duals-gaps @ dea7d708, 2026-11-14)
| # | Bug | Symptom | Cause (code) | Status |
|---|---|---|---|---|
| 1 | Engine clamps lane offset/rotation to 15 | Range snaps at the midpoint (past 16) | MonsoonSandsManager.cpp:392-393 (and the other sites: 450-451, 703-704, 965-966, 1001-1002), literal 15.f | FIXED  named kLaneLengthMax/kLaneOffsetMax at every clamp site (MonsoonSandsManager.cpp:394-395, 453-454, 712-715); test_range_strip_gesture F/G |
| 2 | Lane drags share the voice-strip drag state | Lane drags can write the voice window / fire onRangeEdit | SandsVisualEditorV4.hpp: rangeStrip is the only drag state (lines 325, 1195, 1220, 1316) | FIXED  one owner per drag in SandsRangeGestureCore; test_LorDrag case 3, test_range_strip_gesture E |
| 3 | Edge handles too small | Presses on an edge fall into the window body | handleWidthF() = min(0.8 cell, 0.5 lane) (line 418) | FIXED  edge zone >= max(1 cell, 8px) each side; test_range_strip_gesture B |
| 4 | Absolute edge assignment | Edge jumps by the cells crossed before engagement | getStepAtX sets the edge from the cursor step (line 1317) | FIXED  delta-from-press edges (delta 0 = no engagement jump); test_range_strip_gesture A, test_LorDrag case 1 |
| 5 | Voice rotation absolute on the strip | Chevron can leave the window | strip ROTATION writes an absolute step, no clamp | FIXED  rotation window-relative, clamped [0, len-1]; test_range_strip_gesture C, test_LorDrag case 5 |
| 6 | Literal bounds and stale comments | Future regressions (the "visible 16" comment at line 297) | magic numbers 15/16 in window code | FIXED  named constants (F/G), bounding literals stripped from the two managers' live window code (test asserts none remain) |
| 7 | Preset bank (vestigial) | Dead code | presetBank / applyPreset | FIXED (removed); test_range_strip_gesture D (grep-in-test) |
| 8 | 32-column display | Data drawn for all 32 columns; interactions buggy | geometry verified consistent | VERIFIED (no display bug) |

## ROOT ISSUE: THE DRAG LOGIC IS NOT TESTABLE
The press, drag and release logic lives inside the GLFW event handlers of the editor widget. It can only be exercised in Rack,
which is why each fix has been checked by eye. Extract it.

## PLAN
1. EXTRACT A PURE DRAG REDUCER. A function dragReduce(state, event) -> state, with no Rack or GLFW types. events are
   press(step, zone), move(step), release. state holds the owner (strip or lane), the press cell, the grab values
   (grabOff, grabLen, grabEdge), and the current window. The widget only translates GLFW events into these calls.
2. FIX 1-5 INSIDE THE REDUCER: named bounds (kLaneOffsetMax, kLaneLengthMax), one owner per drag, delta-from-press
   edges, a minimum edge zone of one cell each side, and rotation clamped to the lane length minus one.
3. A HEADLESS TEST SUITE (test/test_LorDrag.cpp, header-only, in run_all). Scripted sequences assert state after each
   event. Required cases:
   - Edge drag one cell at a time moves the edge one cell at a time, with no jump at engagement.
   - Drag past step 16 and past step 31: no snap at 16, no wrap.
   - A press in a lane body never changes the voice window; a press in the strip band never changes a lane.
   - Press within one cell of an edge grabs the edge, not the body.
   - Rotation stays in [0, length-1] for every length, drag and clamp.
   - Window body drag moves the window with the grab offset held.
4. DELETE THE MAGIC NUMBERS in the window path (G), and fix the stale comments.
5. THEN a short Rack pass to confirm the feel, not the correctness (the tests carry correctness).

## ORDER OF WORK
Extract the reducer first (no behaviour change, with the existing behaviour captured as tests), then fix 1-5 one at a time,
each with its test failing first. This is the only way to be sure the bugs are gone rather than moved.

## DO NOT
- Change the engine arithmetic, the store, or the 32-column geometry.
- Change the audio path.

## NEW BUGS (Rodney's Rack report, after the lane-editing fix)
| # | Bug | Symptom | Likely cause (located) | Status |
|---|---|---|---|---|
| 9 | Range dragged past step 1 or 32 cannot be picked up after release | The window is stuck; it can't be grabbed to move again | A wrapped window (start after end) is not handled by the body hit test (SandsRangeGestureCore inBody / classifyBand with startBoundX > endBoundX). Wrap at the release point is not normalised. | FIXED  window-relative classifier handles the wrap; test_LorDrag case 7 |
| 10 | Body hit area is the interior bars only | A window of length 1 has no body to grab; accent (length 1) cannot be moved | edgeZoneW (START and END zones, one cell each side) takes the whole cell for length 1; inBody returns false. | FIXED  length-1 bar moves the window, outside bands resize; test_LorDrag case 8 |
| 11 | Voice lane has no faint range highlight | The voice strip does not show the window the probability lanes show | drawRangeStrip (SandsVisualEditorV4.hpp:619) draws no body fill for the window; the lanes draw ribbons and a body fill. | FIXED  strip tint from the one windowContains law; test_LorDrag case 9 |
| 12 | Rotation chevrons wider than the probability bars at 32 steps | Chevrons do not match the 32-column grid | drawRotationIndicator (line 1027) uses a fixed chevron size, not stepWidthF(). | FIXED  chevron x-extent = one step width; test_LorDrag case 10 |

Which lanes can be selected: the voice lane, rest, variation and legato work; melody, octave and qmix fail after they wrap
(bug 9); accent fails because it is length 1 (bug 10).

## TESTS TO ADD (test/test_LorDrag.cpp, headless, with the reducer)
- Drag the window past step 32 and release; then press in the body and move: the window moves (bug 9).
- Length 1 window: press on the bar, move one step: the window moves by one step (bug 10).
- Length 1 window: press on the edge, move: the edge moves, not the body (bug 10).
- Voice-lane band draws the same window highlight as a lane, for the same offset and length (bug 11, a render-state test).
- Rotation chevron x-extent equals one step width at 32 steps (bug 12, a geometry test).

## NEW BUGS (Rodney's Rack report, after the wrap and length-1 work)
| # | Bug | Symptom | Likely cause (located) | Status |
|---|---|---|---|---|
| 13 | A wrapped range with one step left on one side cannot be dragged back | Grabbing from the step at the edge only drags away from the edge | The edge zone and span maths (SandsVisualEditorV4.hpp ~899-986) use the start and end cells and a wrapped length; the drag does not handle a remainder on the other side of the wrap. | FIXED  raw cell index crosses the seam continuously; test_LorDrag case 11 |
| 14 | Length-1 handle markers overlap the panel at the grid edge | When the single step is at step 1 or 32, the outside handle bands are drawn past the grid | drawHandles (~1042-1046) draws the outside bands at cell.pos.x - edgeW and cell.pos.x + cell.size.x with no clamp to the grid rect. | FIXED  outside bands clamp into the grid rect; test_LorDrag case 12 |
| 15 | Voice-lane range highlight is invisible | The grab range is grey on the grey background | drawRangeStrip (~619-640) uses a low-contrast fill; it needs a lane colour or a contrast check. | FIXED  strip figure meets the minimum contrast ratio; test_LorDrag case 13 |

## TESTS TO ADD
- Wrapped window with one step on one side: press on the edge step and move toward the edge; the window moves toward the edge (bug 13).
- Length 1 at step 1 and at step 32: the outside handle bands are inside the grid rect (bug 14, a geometry test).
- Voice-lane fill contrast against the strip background meets a minimum ratio (bug 15, a colour test).

## REGRESSIONS (Rodney, after the editor improvements)  AUDIO BUG FIRST
| # | Bug | Symptom | Cause (located) | Priority |
|---|---|---|---|---|
| 16 | Output mask has NO effect | Painting the mask does not gate the output | maskPainted_ is WRITTEN (SequencerEngine.cpp:220, 388) but never READ. The output path (voiceRead_ at :916-937) does not AND the painted mask into the gate. The composition step is gone. | FIXED (code)  output-gate composition restored; the gate now ANDs outputMaskPasses in MonsoonOutputGenerator.cpp (mono :55, per-voice :130). The write-only echo cause was disproven on-branch and deleted (commit 84229f7e). Rack feel-test pending. |
| 17 | Voice (GLOBAL) lane playhead outside its range in cycle mode | The voice-lane playhead leaves the window | The voice-lane playhead draws from the global walk, not the voice window's read position (voiceReadPosition / the cycle fold). | FIXED  shared lanePlayheadBar, ring fold; test_LorDrag cases 14-15 |
| 18 | VARIATION lane playhead outside its range | The VAR playhead leaves the window | The editor draws variationLanePlayhead(eng) for the VAR lane (StraitsEastSandsVisual.cpp:1043), which is the engine's GLOBAL variation scan, not the lane window. | FIXED  the VAR global-scan exception retired; every lane draws the shared window playhead; test_LorDrag case 14 |

## BUG 16  ✓ FIXED IN CODE (was the audio priority)  THE OUTPUT-GATE COMPOSITION, RESTORED
(2026-11-14: the output-gate composition now lives in MonsoonOutputGenerator.cpp and reads outputMaskPasses;
the located "written-but-never-read" cause was disproven on-branch and the dead echo deleted, commit 84229f7e.
The original diagnosis is kept below as the record. Rack feel-test still Rodney's.)

### (original report)  THE WHOLE OUTPUT-GATE COMPOSITION WAS LOST
Confirmed: the mask worked before. The last commit to READ maskPainted_ is b134490 ("unified ADDRESSING Stage 4  the
OUTPUT-MASK hierarchy, ONE mechanism (the containment concept)"). Since then the composition at the output gate was
dropped: maskPainted_ is written (SequencerEngine.cpp:220, 388) and never read, AND the abs1 silent-outside-LOR gate is
also gone (the abs1 read at :916-937 computes voiceRead_ over the full ring but nothing silences steps outside the LOR).

THE MODEL (Rodney + UNIFIED_ADDRESSING_SYSTEM.md). ONE AND gate at the OUTPUT, keyed on the GLOBAL step:
- BASE READ: cycle reads WITHIN the LOR; abs1 reads the FULL main-transport length.
- AND the LOR WINDOW at the global step: in abs1 this SILENCES output when the global step is OUTSIDE the LOR (the missing
  behaviour); in cycle the read is already inside, so it is a no-op.
- AND the PAINTED MASK at the global step.
All three compose at the output, on the global step. This is exactly what b134490 added and what regressed.

FIX
1. Restore the output-gate composition from b134490: gate(v, s_global) = base_gate AND inLor(v, s_global) AND
   maskBit(maskComposed_[v], s_global). Find what b134490 added at the gate site and why it stopped being called (git show
   b134490; diff the gate path to HEAD).
2. maskComposed_ = the painted mask composed with the base-mode window (the Stage 4 composition), read at the global step.
3. PINS (restore the Stage 4 / GATE-1 tests that regressed):
   - abs1 + a non-full LOR: output is SILENT at global steps outside the LOR, sounds inside it. (The missing abs1 gate.)
   - cycle + painted mask bits off: exactly those steps silenced; all-on is bit-identical to today.
   - painted mask changed -> output changes (the written-but-never-read smell becomes a test).
4. ≤16 identity/abs1 goldens stay bit-identical.

## PLAYHEAD FIX (bugs 17, 18 together)  ✓ FIXED (shared lanePlayheadBar; the VAR global-scan exception retired; test_LorDrag 14-15)
One shared playhead function: given the lane/voice window (offset, length, rotation, direction) and the engine tick,
return the absolute step INSIDE the window, equal to the engine's read position. The VAR lane and the voice lane both
use it. No lane draws the global scan when it has a window. Test: for every (offset, length, rotation, tick) the drawn
playhead is within [offset, offset+length) and equals the engine read position.

## DO NOT regress the editing improvements while fixing these.

## MACRO REGRESSIONS (Rodney, Rack)  ✓ RESOLVER BYPASS REMOVED (bugs 19-21 FIXED in code; Rack feel-test pending)
(2026-11-14: the located bypass  the raw v==0 loop at SequencerEngine.cpp:876-890  is gone; the loop now
routes through VoiceResolver::isMono/isPoly (:906) and the demanded Macro-follower coverage was added to the
battery. SEPARATE but related: the Macro-lanes-fold-over-global-length SEED bug is FIXED on
fix/macro-voicelane-shared-frame via one shared laneSeed helper (East mono/poly + Macro), test_macro_lane_seed
9/9  NOT yet merged to trunk. See CC_FIX_MACRO_LANE_SEED_AND_CONSOLIDATION.md. The report below is the record.)

East is mostly correct. Macro is broken, and the 109-suite battery is green, so the suites are NOT distinguishing Macro
from East. The model: on the MACRO module everything is GLOBAL  ONE shared window and ONE output mask, common to ALL
voices. The engine has the shared state (macroVoiceLor_, maskMacro*), followers read it via effectiveVoiceLor and
outputMaskPasses. The breakage is per-voice, which a shared-state-only test misses.

| # | Bug | Symptom (Rack) | Suspected cause | Status |
|---|---|---|---|---|
| 19 | Macro voice 1 ignores the window AND emits no gates | V1 cycles ALL steps (cycle and abs), no gates out | The mono slot 0 path (SequencerEngine.cpp:881-884) folds %16 and does not read the Macro window or the Macro mask; the output gate for slot 0 is off. | FIXED (code)  the voice-frame loop routes through VoiceResolver::isMono/isPoly (SequencerEngine.cpp:906, no raw v==0); every voice resolves the shared Macro window + mask. Rack feel-test pending. |
| 20 | Macro voice 2 cycles a small unrelated range | V2's window is not the shared Macro window | A low-index voice reads stale/per-voice LOR instead of macroVoiceLor_; effectiveVoiceLor is not returning the Macro window for this voice. | FIXED (code)  same resolver routing; effectiveVoiceLor returns the Macro window for every follower. Rack feel-test pending. |
| 21 | Macro output mask does not gate at all | Mask on everywhere, no gates out (picture 3) | The follower output gate (outputMaskPasses) is not reaching the voices that play, or the mono/low-index voices bypass it. | FIXED (code)  outputMaskPasses applied per voice incl. the mono slot in MonsoonOutputGenerator (no bypass). Rack feel-test pending. |
| 22 | Lantern not true to the output mask | Fully masked-out pattern: scope shows silence, Lantern still shows steps (picture 1) | Lantern's write path does not apply outputMaskPasses; it records the read, not the gated output. | PARTIAL  GATE-OFF in: Lantern records the gated jack (jackGate < 1 => Inactive, Lantern.cpp:260), so a masked-out step records OFF. The "distinct dimmed-content cue" (resolved-design section below) is NOT confirmed landed  current write records Inactive (= the doc's fallback). Rack confirm pending. |
| V3 | Voice 3 is CORRECT | cycles its range in cycle, all steps in abs | reference behaviour | OK (reference) |

ROOT PATTERN: the behaviour differs BY VOICE INDEX (V3 right, V1/V2 wrong). This is the mono-slot (slot 0) and
low-index special-case path, not the shared Macro mechanism. The mono/early-voice special cases (flagged earlier as a
separate design question) now cause real divergence under Macro.

## THE TEST GAP (fix the tests first, or the fix will regress again)
The suites assert the shared mechanism but never drive MACRO followers across ALL 16 voices including the mono slot.
Required new coverage (test_output_mask_gate.cpp and the voice-frame tests):
1. MACRO SHARED WINDOW: set a Macro window (e.g. O=16, L=8); assert EVERY follower voice v in 0..15 (slot 0 included)
   reads the SAME window  voiceRead_ in [O, O+L), identical period, for V1..V16. Today V1/V2 would fail.
2. MACRO SHARED MASK: paint a Macro mask with bits off; assert outputMaskPasses is identical for every follower voice at
   every global step, and that masked steps are silent for ALL voices including slot 0.
3. MONO SLOT NOT SPECIAL: slot 0 under Macro reads the Macro window and mask exactly as slots 1..15. A direct pin on the
   :881-884 mono path.
4. GATE OUTPUT PER VOICE: with the mask all-on, every follower emits gates; with it all-off, none do  checked per voice,
   not just on the aggregate.
5. LANTERN REFLECTS THE GATE: Lantern's recorded cell for a masked-out step is OFF (bug 22).

## FIX ORDER
Write tests 1-4 FIRST (they must FAIL on this branch, reproducing V1/V2). Then fix the mono/low-index path so slot 0 and
all followers read the shared Macro window and mask. Then bug 22 (Lantern). Do NOT special-case any voice index on the
Macro module  one window, one mask, all voices.

## CORRECTION (Rodney: voice indexing is fixed via a resolver with C++ constraints)
VoiceResolver (src/dsp/VoiceResolver.hpp) is well-formed: strong indices (VoiceSlot), kMonoVoice=1, isMono/isPoly,
voiceSlot/slotToVoice, constexpr. Most of the engine routes through it. The Macro bug is NOT the resolver permitting
divergence  it is a LOOP THAT BYPASSES IT.

LOCATED: SequencerEngine.cpp:876-890, the voice-frame loop, uses a RAW int v and a LITERAL branch:
  const bool live = (v == 0) || (v >= 1 && v <= numPolyVoices);
This is the one place not going through VoiceResolver::isMono / isPoly. Slot 0 and the low-index voices therefore do not
resolve the shared Macro window and mask the way V3 does. (The identity ring-fold here is already the bug-17 fix and is
correct; the problem is the raw indexing around it.)

REVISED FIX (supersedes "fix the mono special case"):
1. Route this loop through VoiceResolver: replace the raw v==0 / v>=1 test with isMono/isPoly, and resolve each voice's
   window and mask through effectiveVoiceLor / outputMaskPasses exactly as the working voices do. No voice index is
   special; the resolver is the single interface, as everywhere else in the engine.
2. The fix is to REMOVE the bypass, not to add a mono branch. If a mono-specific value is needed, it comes from
   VoiceResolver::mono()/kMonoSlot, not a literal 0.
3. TEST that fails on this branch: drive Macro followers THROUGH THE RESOLVER for all v in [kMonoVoice..kLastVoice];
   assert V1..V16 share the Macro window and mask. V1/V2 fail today precisely because this loop bypasses the resolver.
4. Audit: grep the engine for raw per-voice indexing (v==0, %16 on a voice, bare int v loops over voices) and route each
   through VoiceResolver or justify it. The array-guard / anchor-audit style, applied to voice indexing.

## BUG 23  ✓ FIXED IN CODE  identity window sentinel is the literal 16 (vestigial from the 16->32 migration)
(2026-11-14: voiceLorIdentity now returns L == kPatternLengthCap && O==0 && R==0 && Forward, SequencerEngine.hpp:837;
the raw-tick polymeter pass-through 16 is DELIBERATELY preserved and commented distinct, SequencerEngine.cpp. Spec below kept as the record.)
Decided (Rodney): the voice range is its own window, independent of the global pattern length. Identity = the FULL voice
window = kPatternLengthCap (32). Longer than the global pattern: the pattern phases within the window. Shorter: the voice
cycles more than once. Correct for UAS cycle mode. Patch compatibility is NOT a concern yet (no migration needed).

LOCATED  the literal 16 is the identity sentinel in three load-bearing places:
- voiceLorIdentity: returns L==16 && O==0 && R==0 && Forward (SequencerEngine.hpp:834).
- window pass-through: len==16 && off==0 (SequencerEngine.cpp:102 and :115).
- the reset menu items: "Reset range to identity (16/0/0, fwd)" (StraitsEastSandsVisual.cpp:944, :981) and
  "Reset global range to identity (16/0/0, fwd)" (StraitsSandsMacroVisual.cpp:585); engine defaults len=16
  (Monsoon.cpp:92, Monsoon.hpp:1013, :1115).

FIX (no migration):
1. Replace the literal 16 identity length with kPatternLengthCap at ALL sites: voiceLorIdentity, the two pass-through
   checks, the engine defaults, and the three menu items. Use the named constant, not 32.
2. Reset sets 32/0/0/Forward. The menu text reads the cap ("Reset ... to identity (32/0/0, fwd)").
3. Keep the pass-through behaviour: at any length the identity window spans the whole voice ring (kPatternLengthCap), so
   it imposes no restriction.

GATES:
- A reset gives a full 32-step voice window; the voice imposes no range restriction.
- <=16 goldens bit-identical: at pattern length 16 a 32-wide identity window reads the same cells as the old 16-wide
  pass-through (verify the pass-through fires for L==kPatternLengthCap).
- No literal 16 remains as an identity/length sentinel in the voice-window path (grep; part of the magic-number audit, G).
- Global pattern length changing during playback leaves an identity voice reading the full window (the window does not
  depend on the global length).

## BUG 22  ~ PARTIAL (resolved design; gate-off landed, dim cue unconfirmed)  Lantern shows the gated output; masked-out steps DIMMED
(2026-11-14: the GATE-OFF half is in  Lantern records the gated jack, so a masked-out step records OFF, Lantern.cpp:260.
The "distinct dimmed-content cue" half is NOT confirmed in code  the write currently records Inactive, which is this
section's own stated FALLBACK. Needs a Rack look to decide if the dim cue is wanted. Design below unchanged.)
Decided (Rodney): Lantern is the final view of the voice pattern generation, so it reflects the OUTPUT MASK. A masked-out
step does NOT show as a lit gate (picture 1's bug: scope silent, Lantern still lit).
Rule:
- The GATE is OFF for a masked-out step: no lit gate, so Lantern agrees with the scope and the audio.
- The step's CONTENT is shown DIMMED, not blank: Lantern is observability/debugging, and the useful signal is "nothing
  plays here, and this is what it would have been". Matches the Sands grid's language (full = live, dim = present-not-
  sounding).
- The mask dim MUST be visually distinct from the window-dim Lantern already uses (out-of-window), or a masked step and an
  out-of-window step look identical. Give the mask dim its own level or cue.
Fix:
1. Lantern's write/record path applies outputMaskPasses: a masked-out step records gate OFF and a dimmed-content flag.
2. The draw distinguishes three states: live (full), out-of-window (existing dim), masked-out (a distinct dim).
Gate:
- A fully masked-out pattern: scope silent, Lantern shows no lit gates (dimmed content only).
- A masked step and an out-of-window step are visually distinguishable.
- Unmasked output unchanged.
Fallback: if the two dim levels are not readable apart, a masked step goes fully blank; but start with dimmed.
