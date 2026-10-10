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

---

## SCOPE vs V1->voice-0 — similar SIZE, meaningfully EASIER (Rodney)
Measured footprint:
- PL_* lane-enum refs: ~278 across ~12 files. STRAND_*: ~136 across ~8. EL2ENG/EditorLane/EngineLane
  (the conversions to DELETE): ~36 across ~5 files.
- V1->voice-0 took: 33 files, ~845+/432-, multi-session, several overclaim/correct cycles.

So raw ref-count is SIMILAR-OR-LARGER than V1 — BUT the effort is MUCH SMALLER, because it's MECHANICAL
RELABELLING, not a semantic data-model collapse:
- Most of the 278 PL_* refs DON'T CHANGE — they use the NAME (PL_MELODY etc.), which is STABLE; only the
  enum VALUES reorder (PL_MELODY=0 instead of PL_REST=0). `polyRandom(v, PL_MELODY)` is identical before/
  after. So ~278 refs, but most need ZERO edits.
- Real edit surface is bounded: (a) the enum definition (1 line reorder, SequencerEngine.hpp:352);
  (b) delete the ~36 EL2ENG conversion sites (editor-order == engine-order now); (c) the ORDER-DEPENDENT
  hard-codings below.
- Unlike V1, NO subtle semantic preservation (legato timing/spread/ownership) — output is
  IDENTICAL-or-OBVIOUSLY-BROKEN. Bit-compare = clean pass/fail, no subtle-breakage debugging.
Estimate: ~1/3 to 1/2 the EFFORT of V1 despite comparable size; lower-risk.

### ORDER-DEPENDENT spots the reorder MUST update (the real work — prep list)
These encode the CURRENT order and break silently if the enum reorders without fixing them:
- StraitsSandsMacroVisual.hpp:97  `(lane==0)?0 :(lane==1)?3 :(lane==3)?9 :6` — lane->block index map.
- StraitsSandsMacroVisual.hpp:152 `if (lane==0) return SPREAD_REST;` — lane->spread-id map.
- MonsoonChangeAlleyV2.hpp:1286/1295 `(plane==0)?rhythmSrc:...` — CA plane (rhythm/melody/qmix) mapping.
- SpreadInterp.hpp:152 — lane -> CA pin plane (rhythm/melody/qmix) mapping.
- SequencerEngine.cpp:118 `for l < PL_LANES` — order-agnostic loop (fine, but verify any `< 3`/`<N`
  partial-lane loops are updated to the new groupings).
- rawDraw*PatternAt cursor order (PatternEngine.hpp ~697) — the Philox CURSOR order within a stream is a
  SEPARATE internal detail; need NOT match the lane index, but confirm the stream<->lane grouping stays
  consistent (melody 0-1, qmix 2, rhythm 3-6 under the new order).
- static_assert laneNames size == POLY_LANES (StraitsEastSandsVisual.cpp:762) — update laneNames order.
Prep = grep `lane ==`, `== PL_`, `plane ==`, `< 3`/partial-lane loops near PL_ usage; these are the
findable, bounded set. Not a semantic minefield.

### Recommendation
Good "do it on the momentum" candidate after the Straits finish: kills the 2nd recurring bug class
(EL2ENG crosswires), lower-risk than V1 (mechanical + bit-verifiable), and ENABLES the 4x Philox
stream-contiguous batching. Main prep is the order-dependent list above.
