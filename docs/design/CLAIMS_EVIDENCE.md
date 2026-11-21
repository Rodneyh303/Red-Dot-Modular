# Claims → evidence matrix (draft, 2026-11-21)

Purpose: for each headline claim (docs/design/00_VISION.md, CANONICAL_CLAIM.md), record
(1) the automated test that backs it today, (2) the gap, (3) the new unit test that would close it,
(4) the Rack demonstration that shows it to a user.

**How to read the "Backed by" column.** Test names below come from file names plus the header comments
I read on 2026-11-21 (`test_ca_true_reverse`, `test_PhiloxRng`, `test_slew_copula`, `test_cycle_commit_edge`,
`test_gate_mode_agnostic`, `test_MpeRoundTrip`, `test_12tet_identity`, `test_TuningRoundTrip`, `test_qmix_octave`,
`test_lock_behaviour` …). Rows marked **(verify)** are inferred from the name only — open the file and confirm the
assertions really pin the claim before quoting it publicly. Rows marked ✅ for the three added tests were run on 2026-11-21 (see the commit); the full suite result is in the commit message.

Legend: ✅ pinned by a test · 🟡 partly pinned · ❌ no automated evidence (Rack feel-test only)

---

## A. The canonical technical claim

| # | Claim | Backed by | Status | Gap → proposed new unit test | Rack demo |
|---|-------|-----------|--------|------------------------------|-----------|
| A1 | **Time-reversible**: scrub/reverse is bit-exact (Philox counter-addressed) | `test_PhiloxRng` (counter addressing, std anchor vectors), `test_ca_true_reverse` (replays recorded states in reverse, through a lossy collapse) | ✅ position | **Added `test_engine_reverse_roundtrip`** (200 cases): column, every lane tick and the voice read retrace exactly, with or without a voice window (fixed 2026-11-21 — §H.1). Draw *values* remain pinned by the Philox/CA tests, not here. | Record 8 bars, flip reverse, show the same notes come back backwards; scrub and return to the same state. |
| A2 | **7 lanes × 16 channels of moving-average Gaussian copulas** | `test_GaussianCopula` (also covers MovingAverageCopula), `test_slew_copula`, `test_lane_types`, `test_poly_voices` | ✅ storage / 🟡 streams | **Added `test_lane_voice_matrix_shape`**: 16 slots × 7 lanes — 1792 probability cells and 336 LOR values are each independent (mutation-checked: aliasing VAR onto REST fails it). That the 112 *Philox streams* are distinct is still unpinned (verify `test_lane_voice_uniformity`). | Lantern: show 16 voices × 7 lanes populated and independent. |
| A3 | **Uniform marginals** — correlation changes relationships, never each voice's distribution | `test_distribution`, `test_slew_copula` (stated invariant), `test_kumaraswamy_reparam` (verify) | ✅ copula level | **Already pinned:** `test_GaussianCopula` property 3 — the output marginal stays exactly uniform for every r — and property 4 (empirical lag correlation = analytic weights). Remaining: the same invariant at the *cross-voice / spread* layer (needs the SpreadInterp API; not added yet). | Scope two voices at spread 0 and 1: same histogram, different joint behaviour. |
| A4 | **Slew + mix controls** | `test_slew_copula`, `test_slew_latch_regression`, `test_SpreadInterp` | ✅ (verify mix) | Confirm `mix` (interpolation) is pinned, not just slew. | Turn slew: lanes glide; turn mix: blend. |
| A5 | **Constructible correlation structures** (Change Alley) | `test_change_alley_transforms` (collapse / rotate / scatter), `test_cantonment_*` | 🟡 | Transform tests show the *operations*; a test that a constructed matrix yields the stated measured correlation is missing. **New:** `test_change_alley_realised_correlation` — build a structure, draw N samples, assert empirical pairwise correlation within tolerance of the specified matrix. | Build a hocket / heterophony structure in Change Alley and show the resulting voice relationships. |
| A6 | **Modulate structure AND amount independently** | `test_send_tap`, `test_spread_resolver`, `test_probmod_roundtrip`, `test_warp_*` (verify which cover *structure* modulation) | 🟡 | **New:** `test_structure_vs_amount_independence` — modulating amount leaves topology fixed and vice-versa (observable: which voice pairs are coupled vs how strongly). | CV on topology select vs CV on amount, scoped. |
| A7 | **Two-level time structure**: global length + per-lane LOR + direction → polymetric, drifting, realigning, reversible | `test_sands_grid_windows`, `test_lane_direction`, `test_cycle_commit_edge`, `test_reverse_anchor_ring`, `test_lor_phrase_latch`, `test_abs_cycle_consistent`, `test_abs_window_extent`, `test_unified_addressing`, `test_pattern_length_cap` | ✅ strongest area | **Added `test_polymeter_realignment`** — the literal length-7 vs length-5 example in a 16 global (plus 7 more pairs, 16 and 32 rings): tick = t mod L, realign first at lcm = 35, 35 distinct joint states, reverse retraces. **Observation** in §H (restart-at-edge for other G). | Length-7 melody vs length-5 rhythm in a 16 global; watch it phase and realign; reverse. |

## B. Functional identities

| # | Claim | Backed by | Status | Gap → new test | Rack demo |
|---|-------|-----------|--------|----------------|-----------|
| B1 | **Sequencer–quantiser axis** (q-mix per voice; pitch origin independent of timing mode) | `test_qmix_poly`, `test_qmix_octave`, `test_qmix_rng`, `test_ca_qmix_source_select`, `test_quantize_engine` | ✅ | The two extremes should be pinned: q-mix = 0 is a pure quantiser (output ∈ scale degrees of the input), q-mix = 1 is pure generator. **New:** `test_qmix_extremes`. | Scaler → Monsoon → synth (the patch you just tried). Sweep q-mix. |
| B2 | **Gate mode is mode-agnostic** (one gate path for generator and quantiser) | `test_gate_mode_agnostic` (the §421 invariant), `test_gate_smoke`, `test_mode_b_gate` | ✅ | — | SEQ-8 into GATE mode → ensemble. |
| B3 | **Generative clock/gate modulator** (rests, legato, subdivision) | `test_StepGate`, `test_subgate*`, `test_legato_leading_edge`, `test_rhythm_decision_*`, `test_multistep` | ✅ | — | Steady clock in, probabilistic rhythm out. |
| B4 | **Per-voice articulation with a shared timebase** (mono gate by design) | `test_per_voice_articulation`, `test_poly_voices`, `test_poly_slur_roll` | ✅ (verify shared-step claim) | **New:** `test_shared_master_step` — all 16 voices read the same master draw index per step (the architectural reason for mono GATE IN). | — |
| B5 | **Lock** (freeze selected controls) | `test_lock_behaviour` (73 assertions) | ✅ | — | Lock a take you like; it repeats. |

## C. Microtonal / MPE

| # | Claim | Backed by | Status | Gap → new test | Rack demo |
|---|-------|-----------|--------|----------------|-----------|
| C1 | **12-TET path is bit-identical to the legacy formulas** | `test_12tet_identity` | ✅ | — | — |
| C2 | **Tuning load/round-trip (.scl/.kbm/.dmtune)** | `test_ScalaFile`, `test_TuningTable`, `test_TuningList`, `test_TuningRoundTrip` | ✅ | — | Load a maqam tuning, play. |
| C3 | **MPE out → in is an identity (Keppel ↔ Woodlands)** | `test_MpeTransmitter`, `test_MpeReceiver`, `test_MpeRoundTrip` ("trustworthy without a DAW") | ✅ | Real-device interop (Arturia Juno, Bitwig) is not testable headlessly. | Your Scaler → Rack → Juno chain; record pitch bend range follow. |

## D. Observability & persistence

| # | Claim | Backed by | Status | Gap → new test | Rack demo |
|---|-------|-----------|--------|----------------|-----------|
| D1 | **Lantern shows the real engine state** | `test_lantern_bars` | 🟡 | Widget draw cannot be unit-tested, but the *data it reads* can: **New:** `test_display_equals_engine` (see §F). | Lantern beside the sequencer. |
| D2 | **Patch save/load preserves state** | `test_stage4_persistence`, `test_probmod_roundtrip` | 🟡 | **New:** `test_patch_roundtrip_full_editor` — serialise the whole `editor` struct (lorBase, spread, mask bits, directions), reload, assert byte-equal. Includes retired keys (follow-R) loading harmlessly. | Save, reopen, identical sound. |

---

## E. Claims with no automated evidence (Rack feel-test is the only proof)

- The *perceptual* claim that the space between order and chaos is "performable" (musical, not just correct).
- Anything depending on Rack's real clock/threading: deferred expander scan, topology re-resolution after module
  add/remove (the `onExpanderChange` hang fix has no test).
- Widget gestures: lane/strip drag, tab switching, mask painting, undo/redo of store edits.

## F. The class of bug unit tests are currently blind to (and the fix)

The V1-overwrite regression (fixed in `27282771`) lived in `StraitsEastSandsVisual::step()`: a per-frame
`saveSlot(currentSlot())` ran on the frame of a poly→V1 tab change before `loadSlot(0)`, copying the last-viewed
poly voice's windows into V1's store slot. No test could see it because the editor↔store state machine lives
inside a Rack widget.

**Proposal (highest-value new test infrastructure):** extract the load/save/tab-change/`v1Loaded_` logic into a
small header-only core (working name `SandsSlotSync`) that takes the store and the editor state by reference and has
no Rack types. The widget calls it; tests drive it directly. Then pin:

1. Switching poly→V1→poly never changes any slot except the one being edited.
2. Viewing a voice (no edit) never changes the store.
3. V1 edit survives viewing every other voice, in every tab order.
4. Patch load while on V1 does not get overwritten by the next frame's save (the latent stale-editor case noted
   during the hunt).
5. Undo/redo of a LOR commit restores the slot it was made on, regardless of the currently selected tab
   (the undo lambda currently writes the *currently displayed* editor lane — a related latent issue).

This converts the largest remaining blind spot (UI state machines) into the same kind of byte-level pin the engine
already has.

## H. Findings from writing the tests (2026-11-21) — need Rodney's decision before the claims are worded

1. **Reverse transport and the voice-window read — RESOLVED 2026-11-21 (TRUE RETRACE, Rodney).** The read
   coordinate re-anchored to the new direction's phrase entry on a mid-phrase reversal, so the voice read and lane
   ticks came back one step ahead. `PositionResolver::readCoord()` is now anchored to the traversal's ENTRY
   direction: a mid-phrase reversal walks the same coordinate backwards (exact retrace — `test_engine_reverse_roundtrip`
   now ASSERTS column, every lane tick and the read, 200 cases). A reverse that starts FRESH at the phrase entry still
   enters at R (`test_reverse_rotation_anchor` gate 2, unchanged). Forward and fresh-reverse are byte-identical
   (goldens unchanged). **Supersedes** the SLICE-2 "reverse re-enters at R after a direction change" pins
   (`test_reverse_rotation_anchor` gate 3 and `test_unified_addressing` STAGE2), re-aimed to retrace.
   **Needs a Rack feel-test**: forward, flip to reverse mid-phrase with a rotated/windowed voice — the content should
   replay backwards rather than re-landing on R at the right edge.
2. **Polymeter at the global edge.** At global length 16 and 32 the lanes free-run across the global boundary
   (realign at lcm). At G = 8, 10, 12, 15, 24 they restart at 0 at every global boundary. Both are coherent, but
   the wording "drifting and realigning over the global loop" should match the intended behaviour at every G.

## G. Suggested order of work

1. DONE 2026-11-21: `test_polymeter_realignment`, `test_engine_reverse_roundtrip`, `test_lane_voice_matrix_shape`.
2. Decide §H.1 / §H.2, then tighten the two tests.
3. `test_qmix_extremes`, `test_shared_master_step`, the cross-voice marginals test.
4. `SandsSlotSync` extraction + its pins (§F).
5. `test_patch_roundtrip_full_editor`.
6. Walk each **(verify)** row: open the test, confirm it really asserts the claim, and either tighten it or downgrade the row.

Each new test: reproduce-first, register in `test/run_all.sh`, and — where a claim is quantitative — put the
tolerance and sample count in the test header so the claim can cite it.
