# CC BRIEF — lane-index unification (sibling to V1->voice-0)

Full design: docs/design/LANE_INDEX_UNIFICATION.md. This is the working brief.

## Goal
Collapse the TWO lane orderings (editor/visual vs engine) into ONE canonical index = the VISUAL order,
and DELETE the EL2ENG conversion. Kills the 2nd recurring bug class (lane crosswires, e.g.
melody-knob-moved-rest-spread). Mechanical RELABELLING, not a semantic collapse — ~1/3-1/2 the effort of
V1, bit-verifiable (output identical before/after).

## Start
Create a NEW branch off lane-expander-refactor:
    git checkout lane-expander-refactor && git pull
    git checkout -b lane-index-unification

## The target order (VISUAL = stream-contiguous)
Current ENGINE order (SequencerEngine.hpp:352) is the mismatch:
    PL_REST=0, PL_MELODY=1, PL_OCTAVE=2, PL_ACCENT=3, PL_QMIX=4, PL_VARIATION=5, PL_LEGATO=6
Change to the VISUAL order, which is ALSO stream-contiguous:
    PL_MELODY=0, PL_OCTAVE=1, PL_QMIX=2, PL_REST=3, PL_ACCENT=4, PL_VARIATION=5, PL_LEGATO=6
Streams become contiguous ranges: MELODY stream = 0..1, QMIX stream = 2, RHYTHM stream = 3..6.
(This stream-contiguity is also what the later 4x Philox batching wants — bonus.)

## Why most of the ~278 PL_* refs DON'T change
They use the NAME (PL_MELODY etc.), which stays. Only the enum VALUES reorder. `polyRandom(v, PL_MELODY)`
is identical before/after. So the real edit surface is small:
1. The enum definition (SequencerEngine.hpp:352) — reorder the values.
2. Delete EL2ENG / toEngine / EditorLane / EngineLane: StraitsEastSandsVisual.cpp:93,332 (+uses 334,374);
   StraitsSandsMacroVisual.cpp:202 (+uses 210,216,235). After unification editor==engine, so the lambda
   becomes identity -> remove it, pass the lane directly.
3. The ORDER-DEPENDENT hard-codings below.

## ORDER-DEPENDENT sites that MUST be updated (they encode the OLD order)
- StraitsSandsMacroVisual.hpp:97  `const int blk = (lane==0)?0:(lane==1)?3:(lane==3)?9:6;` — lane->block
  map; recompute for the new order.
- StraitsSandsMacroVisual.hpp:152 `if (lane==0) return SPREAD_REST;` and the SPREAD_* enum (hpp:73,
  SPREAD_REST=0..) — realign SPREAD_* order to the new lane order, or fix the mapping.
- MonsoonChangeAlleyV2.hpp:1286,1295 `(plane==0)?rhythmSrc:...` — CA plane (rhythm/melody/qmix) mapping;
  verify plane<->stream still correct under the new lane grouping.
- SpreadInterp.hpp — lane -> CA pin plane (rhythm/melody/qmix) mapping; same check.
- Any `for (… < PL_LANES)` loops are order-agnostic (fine); but check any PARTIAL-lane loops (`< 3`,
  per-stream slices) match the new groupings (melody 0..1, qmix 2, rhythm 3..6).
- laneNames[] array + static_assert size == PL_LANES (StraitsEastSandsVisual.cpp ~762): reorder laneNames
  to the new order.
- The Philox CURSOR order inside rawDraw*PatternAt (PatternEngine.hpp ~697) is a SEPARATE internal detail
  — it need NOT match the lane index, but CONFIRM the stream<->lane grouping stays consistent (melody
  0..1, qmix 2, rhythm 3..6) so each lane still reads its correct stream.

## Method
- Change the enum FIRST, then build and chase every breakage (the order-dependent sites surface as wrong
  behaviour / failed asserts).
- ONE logical change at a time; build + run test suite + commit + PUSH each. No logging.
- BIT-COMPARE: same seed/pattern -> IDENTICAL output before and after (this is a relabelling; any output
  difference = a missed order-dependent site). This is the key check — unlike V1, there's no subtle
  semantics, so "output identical" fully verifies it.
- Keep ONE `Lane` type (optional) with `.stream`/`.subIndex` accessors so stream grouping is METADATA of
  the single index, not a 2nd index.

## Verify
- Moving MELODY spread affects MELODY bars only (not rest) — the crosswire class is gone.
- Each lane's knob affects its own lane; East and Macro agree.
- Bit-identical output vs before the reorder (same seed).
- Test suite green on all 3 platforms (CI now builds lane branches).

## Scope / ordering note
Per ROADMAP_SEQUENCING.md: ideally do this BEFORE adding the NEW Straits lanes (VAR/LEG expanders, range
lane) so they're born into the one-index layout. Panel art/seam is index-independent. Out of scope: the
4x Philox batching (separate later step — but this unification sets up its stream-contiguity).

---

## CORRECTION (post-review) — two order-dependent maps were MISSED; fixed, but VERIFY
The enum reorder + laneToStrand/laneNames were done correctly. But TWO engine-lane->param maps in
StraitsSandsMacroVisual.hpp were NOT updated and crosswired after the reorder (the exact bug class this
is meant to KILL). Both take the ENGINE lane (caller "store engine lane", .cpp:233) but were hard-coded
to the OLD order (0=REST). FIXED in this review to the new order (MELODY=0, OCTAVE=1, QMIX=2, REST=3,
ACCENT=4):
- `sprId(lane)` — now lane0->SPREAD_MELODY, 1->OCTAVE, 2->QMIX, 3->REST, 4->ACCENT (SPREAD_* enum
  unchanged: REST=0,MELODY=1,OCTAVE=2,ACCENT=3,QMIX=4).
- `globalDnaId(lane,c)` — DNA pool blocks keyed by identity (REST@0,MELODY@3,OCTAVE@6,ACCENT@9); now
  lane0(MEL)->3, 1(OCT)->6, 3(REST)->0, 4(ACC)->9, 2(QMIX)->6.

### STILL TO VERIFY (CC — do these before calling done)
1. **globalDnaId QMIX case:** QMIX (new lane 2) falls through to block 6 (OCTAVE's block) — this preserves
   the OLD 'else' behaviour (old lanes 2=OCTAVE and 4=QMIX both ->6). CONFIRM that's correct: does QMIX
   actually SHARE OCTAVE's DNA block, or should QMIX have no global DNA (only 4 lanes REST/MEL/OCT/ACC
   have it)? If QMIX shouldn't map to a real block, this needs a different fix. CHECK the original intent.
2. **CA planes (MonsoonChangeAlleyV2.hpp:1286-1296):** `(plane==0)?rhythmSrc:(plane==1)?melodySrc:qmixSrc`.
   Planes are STREAM-indexed (rhythm/melody/qmix), NOT lane-indexed — so they're likely FINE (streams
   didn't reorder, only lanes). CONFIRM plane is a stream index, not a lane index. If stream -> no change.
3. **BIT-COMPARE (the gate):** same seed/pattern -> output IDENTICAL to lane-expander-refactor. This
   relabelling MUST be output-identical; any difference = a remaining missed site. RUN IT and report the
   result. It would have caught the two maps above — so run it now and chase any remaining diff.
4. **EL2ENG kept as identity (35 refs):** acceptable only if toEngine is genuinely identity now. CONFIRM,
   and prefer DELETING it (per the brief) so the conversion machinery is gone, not just defused.
