# Lane-index unification — ONE canonical index (visual order), kill the EL2ENG crosswire bug class

## The recurring bug class
Lane-indexing bugs keep recurring (melody-moves-rest crosswire; the scattered EL2ENG conversions; the
"4 mono + 15 poly" vs "7 poly lanes" confusions). Root: TWO lane orderings with a conversion between them:
- **Editor/visual order:** MELODY, OCTAVE, QMIX, REST, ACCENT, VARIATION, LEGATO (Sands top-to-bottom).
- **Engine order (PolyLane, SequencerEngine.hpp:347):** PL_REST=0, PL_MELODY=1, PL_OCTAVE=2, PL_ACCENT=3,
  PL_QMIX=4, PL_VARIATION=5, PL_LEGATO=6.
Every lane read/write converts (EL2ENG = toEngine(EditorLane(el)), StraitsEastSandsVisual.cpp:327). Miss
one conversion -> crosswire (e.g. the bars-read that made melody move rest spread). Sibling to the
V1->voice-0 debt: both are "collapse parallel representations into one."

## The key fact (Rodney): the VISUAL order IS stream-contiguous
Stream grouping (which Philox stream each lane draws from, SequencerEngine.hpp:672-678):
- MELODY stream: MELODY, OCTAVE
- QMIX stream: QMIX (the melody-twin; sits adjacent to melody)
- RHYTHM stream: REST, ACCENT, VARIATION, LEGATO
The proposed VISUAL order **MELODY(0), OCTAVE(1), QMIX(2), REST(3), ACCENT(4), VARIATION(5), LEGATO(6)**
makes every stream a CONTIGUOUS RANGE:
- melody = 0..1, qmix = 2, rhythm = 3..6.
So display order and stream grouping COINCIDE — no conflict. (The current ENGINE order PL_REST=0.. is the
mismatch; the visual order is already stream-contiguous.)

## The unification
Make the ONE canonical lane index = the VISUAL order above. Then:
- One index for display AND engine AND stores -> DELETE EL2ENG and the EditorLane/EngineLane split ->
  the crosswire bug class dies (no conversions to get wrong).
- Each STREAM is a contiguous range of the one index (melody 0..1, qmix 2, rhythm 3..6) -> draw batching
  (fillBlock/drawBlock per stream) reads contiguous ranges, no remapping.
- The Philox CURSOR order inside a stream (rawDrawRhythmPatternAt:697 = rhythm,variation,legato,accent)
  is a SEPARATE internal detail; the lane reorder need not change it — it changes the PL_* enum values and
  the storage/index layout to the visual order.

## Work (reorder to the canonical visual order)
1. Set PolyLane enum to the visual order: MELODY=0, OCTAVE=1, QMIX=2, REST=3, ACCENT=4, VARIATION=5,
   LEGATO=6. Update STRAND_* / NUM_STRANDS consumers to match.
2. Reorder the engine storage + all [lane] indexing to this order (the stores are engine-indexed today;
   make engine-index == visual-index).
3. Delete EL2ENG / toEngine / EditorLane / EngineLane — every lane is now the single index. Grep for all
   conversion call sites and remove.
4. Update the stream-range definitions (melody 0..1, qmix 2, rhythm 3..6) for the draw batching.
5. Keep ONE strong `Lane` type (optional) with `.stream` / `.subIndex` accessors (lookup), so the stream
   grouping is METADATA of the single index, not a second index.
6. Bit-compare: output identical before/after the reorder (it's a relabelling, not a behaviour change) —
   same seed/pattern.

## Pairs with
- V1->voice-0 (sibling: collapse parallel V1 representations). Same spirit, lane axis.
- The 4x Philox batching (fillBlock/drawBlock per stream): WANTS stream-contiguous indices -> the visual
  order provides them. So this unification ENABLES the clean batching.

## Payoff
One lane index everywhere -> the EL2ENG crosswire bug class is GONE (no conversions), display order stays
the sensible visual one (and is stream-contiguous), draw batching reads contiguous stream ranges. Reorder
is a relabelling refactor (bit-identical output), scoped and verifiable.
