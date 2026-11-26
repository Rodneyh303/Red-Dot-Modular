# Retiring the setWindow in-window re-seed — the column-edge result

**Status: headless-proven, feel-test pending (reverse + global-length modulation).**
Candidate branch `experiment/retire-reseed-column-edge`, off the net-steps tip
(#369 `feat/phrase-edge-on-netsteps`).

## The question

Can the `setWindow` **in-window re-SEED** (the else-branch that re-anchors
`netSteps_` to the column phase on every global re-author) retire — the C2 "keep
counting, fold mod the new len" ruling — without reopening the 173
`test_global_dip_differential` failures?

## What the measurement found (diag_global_dip_edge, on debug/reseed-logging)

The re-seed was holding two coordinates in forced congruence:
- the **read** — `readCoord()` / `atPhraseEdge()` = `netSteps_ % len_` (free-running counter);
- the **edge** — the live commit anchor, `phrasePos()` (the column: `col_ − startStep_`).

Through a `16→4→16` global dip with the re-seed off, the counter free-runs (e.g.
`netSteps_=4` at the restore) while the column sits at the phrase entry
(`phrasePos()==0`). `atPhraseEdge()` = `4 % 16 ≠ 0` then **misses** the restored
phrase entry the column is on — which is exactly "no commit off the live anchor,"
the class of all 173 failures. (Reproduce-first: 0 edge-splits with the re-seed, 2
at the restore without it.)

## The fix: move the commit EDGE onto the COLUMN, leave the READ on the counter

Both branches of `voiceFrameEdgeLive_` rode the counter:
- identity row → `pos_.atPhraseEdge()` (counter);
- CYCLE row → `vCoord = readCoord() + advanceOffset` (counter, labelled TRUE-RETRACE).

Changed both to the **column**:
- identity row → `pos_.phrasePos() == 0`;
- CYCLE row → `vCoord = (dir>0 ? phrasePos() : −phrasePos()) + advanceOffset`.

Then deleted the in-window re-SEED else-branch (the PARKED re-ARM stays — an
out-of-window column has no phase to count from). `readCoord()` / `netSteps_` keep
their path-independent role for the **read** only; the edge reads the column, where
commits belong. "Each coordinate where it belongs."

## Result

- Reproduce-first: re-seed off + counter edges → `test_global_dip_differential`
  FAILS (both legs). Identity-edge→column alone fixes LEG2a but LEG2b (mapped/CYCLE)
  still fails — the CYCLE edge rode the counter too. Moving BOTH edges to the column
  closes it.
- With both edges on the column and the re-seed removed: **full headless suite
  green** (163/163 + anchor/bind + plugin-TU smoke + array-guard), including every
  reverse / rotation / problane / LOR / position test:
  `test_global_dip_differential`, `test_reverse_rotation_anchor`,
  `test_reverse_anchor_ring`, `test_engine_reverse_roundtrip`, `test_ca_true_reverse`,
  `test_problane_phase_coupling`, `test_lor_phrase_latch`,
  `test_position_resolver_identity` (203), `test_position_holders` (16).

## Caveats before merge

1. **Reverse feel-test.** The headless reverse battery is green, but the commit edge
   is now column-derived; confirm in Rack that reverse retrace and the crab-canon
   still feel right, and that commits land on the window edge through a live
   global-length modulation.
2. **Other `atPhraseEdge()` consumers untouched.** Sites at `SequencerEngine.cpp`
   ~506 (draw edge), 939 / 1030 (LOR latch / commit gate), 1227 still read
   `atPhraseEdge()` (the counter). The full suite is green with the re-seed removed,
   so they are not regressed by what the tests cover — but a complete "the counter
   never gates a commit" pass should audit whether any of these also wants the column.
3. This supersedes the earlier retracted diagnoses (voice-lane holders / the "third
   face"); it is measured, not reasoned — see `diag_global_dip_edge` and BUG6 doc.
