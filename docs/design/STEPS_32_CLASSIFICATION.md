# STEPS_32 — the fixed-16 audit classification table

Committed from the PR #310 description per the Stage 1 review (Rodney: the table "lives only in
the PR description. Commit it to docs/design … so the audit record survives the PR").

Method: the CC-prompt grep set (`& 0x0F`, `% 16`, `< 16`, `<= 15`, `[16]`, `16 - 1`, `65535`,
`0xFFFF`) + two straggler sweeps (clamp/comparison forms; float-span forms `1.f, 16.f` /
`0.f, 15.f`) over `src/` excluding `src/deprecated/` — **381 candidate lines, every line
classified, full coverage machine-checked**. `(D) deprecated`: `src/deprecated/` is excluded
from the sweep and untouched (9 raw hits). `kVoiceSlots`-style voice-16s are the VOICE 16
(plan taxonomy class 3) — untouched.

**A = 6 sites (the real work, all changed) · B = 184 · C = 15 · D = 0 (excluded) · E = 176**

Classes, per the STEPS_32_PLAN taxonomy:

- **A** — length-bearing literal-16 sites that Stage 1 changed (now cap-derived).
- **B** — the storage frame + frame landings: the `[16]` step caches/planes, the uint16 mask
  planes, the mod-16/`& 0x0F` ring landings, the 0..15/0..15 param domains. **Stage 2's
  worklist** — widens with `kStepStorage` / the cap flip.
- **C** — display constants that STAY (the 16-lamp ring + bar indicator, D15 settled; Lantern's
  N_STEPS grid is Stage 3).
- **D** — `src/deprecated/`, excluded and untouched.
- **E** — unrelated or invariant 16s: the VOICE 16 (`kVoiceSlots`), the MUSICAL 16 (the clock's
  native 16th), MIDI/Philox/12-bit-scale domains, char buffers, comments.

| Class | File | Lines | Count | Reason |
|---|---|---|---|---|
| A | `src/dsp/engines/SequencerEngine.cpp` | 950, 955 | 2 | CHANGED — updateWindow: the knob clamp 1..kPatternLengthCap (over-range reports as the cap) + the length CV *15 → *(kPatternLengthCap-1) (identical at the bound; D1's *31 lands with the Stage-2 flip) |
| A | `src/dsp/engines/SequencerEngine.hpp` | 1061 | 1 | CHANGED — setStrand: the lane-length clamp 1..kPatternLengthCap (was literal 16) — the single sanctioned LOR-length authoring path |
| A | `src/dsp/managers/MonsoonConfigurator.cpp` | 30 | 1 | CHANGED — the length control's range derives from the ceiling: PATTERN_LENGTH_PARAM 1..kMaxPatternSteps, default 16 stays (the MODE LAW); the offset param :31 stays 0..15 (class B, frame). CLOSE-OUT (PR #310 review): the range re-derived from kPatternLengthCap — the knob never offers an undeliverable length |
| A | `src/dsp/managers/MonsoonSandsManager.cpp` | 443, 388 | 2 | CHANGED — the lane baseLen CV spans + end clamps (mono :388 applyMonoCV + East/poly :443 eastDelta) → kPatternLengthCap (was literal 16.f; the off/rot spans stay 0..15 — class B) |
| B | `src/Monsoon.cpp` | 1149, 1104 | 2 | the within-draw FRAME cap (doc §C: at most one boundary) + the drift-snap phase landing (% MAX_STEPS at the resize, phase lockstep) — Stage 2+ |
| B | `src/Monsoon.hpp` | 566, 567, 955, 966, 968, 969, 970, 971, 972, 1051, 1056, 1065, 1069, 1079, 1083, 1473, 1474, 1496, 1058, 1071 | 20 | the editor mask float rows (65535.f defaults — the float-precision wall, DECIDED uint64) + the mask accessors/column guards backing them + the melody editor mirrors + the float(&)[16] views |
| B | `src/StraitsEastSandsVisual.cpp` | 85, 907, 927, 1366, 1367, 1369, 1370 | 7 | the mask-editor stroke/all-on 16-bit plane + the per-lane step-seen [16] editor arrays — Stage 2 |
| B | `src/StraitsSandsMacroVisual.cpp` | 78, 638, 658, 1279, 1304 | 5 | the mask-editor stroke/all-on 16-bit plane (0xFFFF/65535) + the playhead frame landing & 0x0F (doc §C → 0x1F) — Stage 2/3 |
| B | `src/dsp/engines/PatternEngine.cpp` | 45, 95, 199, 201, 217, 224, 226, 237, 238, 246, 254, 431, 495, 517, 539, 604, 621, 632, 690, 705, 714, 747, 752, 759, 767, 800, 804, 810, 816, 844, 845, 848, 851, 876, 912, 983, 1110, 1163, 1171 | 39 | step loops over the [16] storage (warp-mirror rebuild, draw/source-cache, slew/qmix, melody-cache publishes) + the warped_ cell landing — the LENGTH LAW re-bounds them when Stage 2 widens |
| B | `src/dsp/engines/PatternEngine.hpp` | 427, 434, 468, 492, 569, 705, 706, 785, 787, 952, 965, 975, 1016, 1026, 1034 | 15 | the [16] step caches/loops + the warpRow frame landing — storage; Stage 2 widens the step dim |
| B | `src/dsp/engines/SequencerEngine.cpp` | 203, 220, 221, 222, 224, 225, 245, 252, 256, 261, 304, 321, 425, 426, 428, 429, 434, 435, 437, 438, 858, 934, 943, 944, 951, 959, 1002, 1003, 1043, 1061, 1816, 1817, 1823, 1932, 2338, 2376 | 36 | frame landings on the 16-ring (walk bounds, window build, mask init 0xFFFF, mask-bit clamp 65535, offset domain, rel-light/trail ring math, poly cell landings) — the RING constant; widens with storage at Stage 2 |
| B | `src/dsp/engines/SequencerEngine.hpp` | 660, 661, 664, 665, 667, 668, 670, 672, 674, 678, 683, 690, 692, 729, 732, 765, 781, 784, 790, 794, 798, 800, 1062, 1063, 1070, 1276, 1310, 1429, 1430, 1434, 1435, 682 | 32 | the uint16 mask planes + rotMask16/windowMaskBits/maskWindowContains + uint16 windowMask + off/rot mod-16 landings + the [16]-backed read landings — Stage 2 (DECIDED: uint64 length-masked) |
| B | `src/dsp/managers/MonsoonConfigurator.cpp` | 31 | 1 | the offset param's 0..15 range — the frame domain (offset rides the 16-ring); Stage 2 |
| B | `src/dsp/managers/MonsoonExpanderManager.cpp` | 932, 987, 988 | 3 | the step publish loop + the macroBase off/rot mod-16 landings (frame domain) — Stage 2 |
| B | `src/dsp/managers/MonsoonPersistenceManager.cpp` | 130, 406, 425, 481 | 4 | the 16-BOUNDED loaders (doc §G: legacy fills [0..15]) + the step-i loader loops — stay 16-bounded until the DECIDED fresh-key persistence |
| B | `src/dsp/managers/MonsoonSandsManager.cpp` | 566, 677, 678, 776, 856, 389, 390 | 7 | step publish loops over the [16] stores + the off/rot mod-16 landings (frame domain, Stage 2) |
| B | `src/dsp/managers/SpreadManager.hpp` | 161, 176, 327, 345, 379, 147, 220, 245 | 8 | the avgCache_[5][16] per-step caches + their step guards — storage; Stage 2 |
| B | `src/ui/SandsVisualEditorV4.hpp` | 331, 338, 1105, 1525 | 4 | the 16-bit mask editor plane (maskPattern 0xFFFF) — Stage 2 |
| B | `src/ui/VisualExpanderHelpers.hpp` | 88 | 1 | the mask-editor timeline landing (timelineIdx + off) % 16 — the 16-wide editor timeline, Stage 2 |
| C | `src/Lantern.cpp` | 177, 278, 326, 348, 261, 300, 499, 502, 1074, 1119, 1376 | 11 | display constants that STAY: the twice-around ring is 16 lamps + bar indicator (D15 settled); Lantern's N_STEPS grid is Stage 3 |
| C | `src/Monsoon.cpp` | 1462, 1463 | 2 | display constants that STAY: the twice-around ring is 16 lamps + bar indicator (D15 settled); Lantern's N_STEPS grid is Stage 3 |
| C | `src/MonsoonWidget.cpp` | 472 | 1 | display constants that STAY: the twice-around ring is 16 lamps + bar indicator (D15 settled); Lantern's N_STEPS grid is Stage 3 |
| C | `src/dsp/managers/MonsoonUIManager.cpp` | 100 | 1 | display constants that STAY: the twice-around ring is 16 lamps + bar indicator (D15 settled); Lantern's N_STEPS grid is Stage 3 |
| E | `src/Keppel.cpp` | 77, 81, 107, 164, 170, 235 | 6 | Keppel's own 16 voices (VoiceState/memberOwner) |
| E | `src/Lantern.cpp` | 182, 498 | 2 | the 16-VOICE roll mask (0xFFFF voice bits) + N_VOICES = 16 |
| E | `src/MicroTuning.cpp` | 346 | 1 | char buf[16] |
| E | `src/Monsoon.cpp` | 92 | 1 | voice slot loop |
| E | `src/Monsoon.hpp` | 565, 628, 936, 937, 1006, 1189, 1203, 1231, 1151, 1180, 1235 | 11 | voice guards/loops + polyVoiceActive + the voice-indexed LOR float rows + comments |
| E | `src/MonsoonCantonment.hpp` | 43, 305, 306, 328, 916 | 5 | containment voice words (uint16 per-voice bits) + voice loop + comment |
| E | `src/MonsoonCausewayPolyExpander.cpp` | 46 | 1 | voice init loop |
| E | `src/MonsoonChangeAlleyV2.hpp` | 209, 173 | 2 | CA leader offset 0..15 (voice-domain routing) + the undo ring depth (unrelated) |
| E | `src/MonsoonChangiExpander.cpp` | 34 | 1 | voice init loop |
| E | `src/MonsoonChangiExpander.hpp` | 40 | 1 | voice init loop |
| E | `src/MonsoonChangiT2Expander.cpp` | 34 | 1 | voice init loop |
| E | `src/MonsoonChangiT2Expander.hpp` | 46 | 1 | voice init loop |
| E | `src/MonsoonStraitsExpander.cpp` | 216 | 1 | voice init loop |
| E | `src/MonsoonWidget.cpp` | 1204, 1550, 917 | 3 | voice loops + a brightness comment |
| E | `src/StraitsEastSandsVisual.cpp` | 88, 1128, 1339, 1382, 1386, 1399, 1448, 1483, 1497, 159, 184 | 11 | voice guards/owner loops + MIDI channels (1..16) + char buffer + comment |
| E | `src/StraitsEastSandsVisual.hpp` | 227 | 1 | comment |
| E | `src/StraitsLaneExpander.cpp` | 222 | 1 | voice init loop |
| E | `src/StraitsSandsMacroVisual.cpp` | 81 | 1 | char rangeLabel_[16] buffer |
| E | `src/dsp/CATrajectoryBuffer.hpp` | 3 | 1 | comment (voice routing src[16]) |
| E | `src/dsp/CantonmentGraph.hpp` | 27, 37 | 2 | comment (voice bitmask) + VOICES = 16 (the voice count) |
| E | `src/dsp/CantonmentModuleCore.hpp` | 125, 197, 208, 260, 58 | 5 | the containment voice bitmask domain (parents[16] = voice bits) + the rotate-step verb config (voice-DAG pivot) + the undo ring depth (unrelated) |
| E | `src/dsp/CantonmentTransforms.hpp` | 184, 188 | 2 | comments (voice-DAG 0xFFFF anchors) |
| E | `src/dsp/ChangeAlleyTransforms.hpp` | 126 | 1 | CA correlation page constant (voice/CA domain) |
| E | `src/dsp/MpeMath.hpp` | 36 | 1 | MIDI 16383 — unrelated |
| E | `src/dsp/NoteValues.hpp` | 106 | 1 | MUSICAL — the note-value fraction × 16 assert (the clock's native 16th) |
| E | `src/dsp/PhiloxRng.hpp` | 107, 134, 153, 179, 271 | 5 | Philox 32/64-bit counter words + the 16-word block stride — unrelated |
| E | `src/dsp/SandsTopology.hpp` | 60, 81, 141 | 3 | voice owner tables + guards |
| E | `src/dsp/ScaleMaskArbiter.hpp` | 25, 26, 27, 39 | 4 | 12-bit chromatic scale masks (12 semitones — unrelated) |
| E | `src/dsp/SpreadInterp.hpp` | 62, 135 | 2 | comments (voice-slot collapse notes) |
| E | `src/dsp/VoiceResolver.hpp` | 49, 158, 178, 37 | 4 | the VOICE 16 (kLastVoice/kVoiceSlots asserts + comment) |
| E | `src/dsp/WarpF.hpp` | 430 | 1 | WarpBank's own 16 voices |
| E | `src/dsp/engines/PatternEngine.cpp` | 104, 182, 191, 215, 244, 252 | 6 | voice guards (v < 16 = kVoiceSlots) |
| E | `src/dsp/engines/PatternEngine.hpp` | 123, 170, 330, 350, 360, 384, 574, 650, 654, 655, 656, 663, 692, 702, 786, 933, 988, 1179, 1267 | 19 | voice-domain guards (CA source rows are voice→voice — the audit's correction) + kVoiceSlots + Philox u64 + storage-shape comments |
| E | `src/dsp/engines/PhaseEngine.hpp` | 165 | 1 | MUSICAL — the bar-local step16 % 16 quarter mark (mod-4 invariant; survives verbatim per the plan) |
| E | `src/dsp/engines/RhythmDecisionCore.hpp` | 163 | 1 | voice-domain / unrelated 16 |
| E | `src/dsp/engines/SequencerEngine.cpp` | 93, 132, 145, 146, 160, 187, 229, 554, 821, 822, 831, 1522, 2388, 102, 115, 653, 811, 1882, 2374, 2405, 2421 | 21 | the default-window pass-through identity (len==16&&off==0 — the default stays 16, the REPRODUCTION LAW) + voice loops/guards (kVoiceSlots) + the D7 ACTIVE-PASS landing (tracks the pass, not the frame) + comments |
| E | `src/dsp/engines/SequencerEngine.hpp` | 110, 574, 1210, 405, 409, 819, 1251, 1267, 1274 | 9 |  |
| E | `src/dsp/managers/MonsoonExpanderManager.cpp` | 142, 770, 777, 785, 837, 843, 848 | 7 | poly voice loops (v 1..15 = voices 2..16) |
| E | `src/dsp/managers/MonsoonModeController.cpp` | 418, 425 | 2 | voice loops |
| E | `src/dsp/managers/MonsoonOutputGenerator.cpp` | 90, 91, 92, 93, 94 | 5 | per-VOICE output arrays (poly[16] = voices) |
| E | `src/dsp/managers/MonsoonPersistenceManager.cpp` | 108, 129, 171, 362, 420, 424, 506 | 7 | voice loops + the 12-bit chromatic scale mask (& 0x0FFF — 12 semitones) |
| E | `src/dsp/managers/MonsoonSandsManager.cpp` | 41, 84, 94, 97, 245, 1137, 1144, 352 | 8 | voice loops (CA routing maps, remap staging — voice domain) + :352 the lane-length DEFAULT fallback (defaults stay 16 — D2, settled) |
| E | `src/dsp/managers/MonsoonScaleManager.hpp` | 67, 75, 110, 118 | 4 | 12-bit chromatic scale masks (12 semitones — unrelated) |
| E | `src/ui/DistributionCurveWidget.hpp` | 302, 318, 336 | 3 | char buf[16] format buffers |

TOTAL candidate lines: 381 — A=6, B=184, C=15, D=0, E=176

## How to use this table

- **Stage 2 (storage)** works the class-B rows: `kStepStorage` names the 16-wide row dimension
  and widens it to 32; the mask planes go uint64 length-masked (the DECIDED block in
  STEPS_32_PLAN.md); the frame landings re-derive from the cap. Unrelated 16s (class E) and the
  display ring (class C) never move.
- The class-A rows are Stage 1's delivered changes; the MonsoonConfigurator row carries the
  PR-#310-review close-out (the length param's range re-derived from `kPatternLengthCap`).
- Line numbers are the audit-time positions (feat/steps-32-stage1 @ da494a09); they drift with
  edits — the file+reason is the durable key.
