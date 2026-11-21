# CC PROMPT — reverse completions, then the CA5 holder migration (one sequence)

Do these in order. Each part is its own commit, each gated on the full suite
green AND the STEPS_32 goldens byte-identical. Stop and report if a golden moves.

## BASE
Rebase/branch on `position-resolver-consolidation` (merged into
`fix/bug6-stable-edge`). It must carry `walkFold` / `signedPhraseCoord` /
`walkEdge` / `playheadSlotBar` + the `WalkPolicy` concept. Do NOT start from a
pre-consolidation tip.

## GLOBAL GUARDRAILS (the hard-won lessons — every part obeys these)
- **Reproduce-first, always.** A test that FAILS before and PASSES after. No
  exception, no "obviously correct" skips.
- **R==0 is DEGENERATE** — at R=0 rotation IS the left edge, so a bug hides.
  SWEEP R across 0..L-1 (and use non-zero R in modulated pairs) in EVERY
  reverse/rotation test. This is why the commit-edge and playhead bugs hid for
  days.
- **Do NOT touch `laneTickFor`** (goldens depend on it) or the definition of
  `walkFold` (the bridge `test_position_consolidation_bridge` pins it bit-exact).
- **Do NOT re-litigate commit timing that is already correct** — the cycle
  window-edge (`voiceFrameEdgeLive_` cycle arm, `wnow == walkEdge`, 638879aa) and
  the voice/macro range-strip slot (e2a450ab) are correct. Part 1 EXTENDS the
  same property to the surfaces that still miss it; it does not change the ones
  that have it.
- **Byte-identical first for any extraction/migration** (Part 2): prove the moved
  code equals the shipped code across the full grid BEFORE any behaviour change —
  the step-1 discipline that `test_position_consolidation_bridge` and
  `test_position_resolver_identity` already embody. Never fold a behaviour change
  into an extraction commit.

## PART 1 — the reverse completions (cash in the walkFold / slot law)
_Note (2026-11-21): the abs1 sounding-extent fix landed in 70223183 (the sounding read stays inside the window; rotation wraps inside it). 1a's commit-edge work builds on that — re-read the abs1 arm of `voiceFrameEdgeLive_` against it._

### 1a — abs1 / absolute-mode commit edge
- **Site:** `voiceFrameEdgeLive_`'s abs1 arm (`voiceFrameReadsFullPhrase`,
  ~`:834`) still returns `wrapMod(phaseClock + advanceOffset_, walkPeriod(d,
  lenG)) == 0` — the ROTATION/phase-origin key, the old behaviour the cycle arm
  (`:856`, `wnow == walkEdge`) was fixed away from in 638879aa.
- **Property (same as cycle):** the probability lanes commit at the voice-range
  STEP EDGE — LEFT in forward, RIGHT in reverse — INDEPENDENT of rotation. But
  abs1's read model is the FULL PHRASE (ring = lenG, the abs1 `rho = R − O`
  ruling), so DERIVE the abs1-geometry edge; do NOT blind-copy cycle's
  `wnow == walkEdge`.
- **Reproduce-first in ABSOLUTE mode:** R-sweep (non-degenerate), both dirs,
  assert the commit read-cell is the window edge for every R. FAIL on the current
  abs1 arm, PASS after. Mirror `test_cycle_commit_edge`'s structure.

### 1b — per-lane playhead rows
> NOTE (2026-11-19): this surface is now owned by
> `CC_PROMPT_REVERSE_PROBLANE_ROTATION_EDGE.md` (the probability-lane playhead
> reverse rotation-edge fix, Rack-gated). Do 1b there; it supersedes the sketch
> below.

- **Site:** the per-lane rows set `lanePlayStep[l]` directly
  (`SandsVisualEditorV4`, `lanePlayStep`/`lanePlayDir`, ~`:452`). e2a450ab fixed
  only the voice/macro RANGE STRIPS (`StraitsEast`/`MacroVisual` via
  `playheadSlotBar`); the per-lane rows still draw the playhead at the read cell
  (rotation-tracking), per lane.
- **Goal:** feed each lane row's playhead through `playheadSlotBar` using THAT
  lane's OWN dir + rotation (a lane can be Forward under a Reverse voice), so each
  row enters at its own direction-entry edge reading its own rotation content.
- **Reproduce-first:** per-lane R-sweep with MIXED lane dirs under one voice;
  assert each lane row's slot == its own entry edge for every R. The rotation-tick
  chevron stays untouched (it marks the CONTENT position — its job).

## PART 2 — CA5: the phase-holder maintenance migration
Only after Part 1 is green and merged.

- **Sites (the ~8 producers):** `foldPerVoiceAdvance_` (`advanceOffset_[i] +=
  dStep − dMainStep`, ~`:612`/`:631`), `anchorTransport` (~`:922`),
  `voiceFramePhase_` seeding + `frameKey_` (~`:978`/`:984`/`:1115`). The audit's
  "incomplete shared key": `walkKeyOf(dir, L)` omits the PARENT length, so a
  parent-length change drifts the child — the step-6/7 producer (behaviourally
  masked by the window-edge commit, but the blind spot remains).
- **Goal:** migrate the phase-holder maintenance into `PositionResolver` as the
  ONE authority, so the ×8 producers become VIEWS of one state.
- **Discipline — extraction is byte-identical FIRST:** `PositionResolver`
  advances the holders ALONGSIDE the live engine, and a bridge test (modelled on
  `test_position_consolidation_bridge` / `test_position_resolver_identity`) proves
  bit-identical across the full sweep BEFORE any consumer switches to read it.
  This commit changes NOTHING observable.
- **Lane-addressing idioms (C++20 — Rodney, 2026-11-21; toolchain already settled:
  the Makefile sets `-std=c++20`, CI-green lin/mac/win): `std::span` + a `RingView`.**
  Done as the FIRST sub-step of the migration, byte-identical, one lane family at a
  time (goldens are the gate), because the holder accessors are being rewritten anyway.
  - **Spans for storage:** lane/holder accessors take and return `std::span<float>` /
    `std::span<const float>` over the existing `std::array` storage instead of
    pointer+length pairs (pairs with the array-guard lint). A NON-wrapping window is
    `lane.subspan(O, L)`. Prefer range-for and `std::ranges::{fill,copy,rotate}` over
    hand-indexed loops. No allocating adaptors on the audio thread.
  - **`RingView` for wrapping windows:** a window that wraps the ring (O=30, L=4 on the
    32-ring) cannot be one span and C++20 has no cyclic view (`views::concat` is C++26).
    Add a small `RingView{span, O, L}` with `operator[](i) = data[(O+i) & (ring-1)]` —
    the same arithmetic `frameReadCell` and the scattered `& (kMaskRing-1)` masks do
    today — and route those sites through it. Bounds: keep `-D_GLIBCXX_ASSERTIONS` in
    the test builds (`span::operator[]` is unchecked in C++20).
  - **NOT in scope:** `walkFold`, `laneTickFor`, `walkKeyOf` and the `rho`/`wnow` fold
    stay integer position math — spans are for storage access, not phase arithmetic
    (the abs1 extent bug lived in the position math, which spans would not have caught).
  - **Gate:** nothing observable moves; full suite + every golden byte-identical, and a
    small `RingView` unit test (wrap, non-wrap, O+L==ring, L==1) lands with it.
- **The key COMPLETION (the actual blind-spot fix) is a SEPARATE, later-gated
  step** with its OWN reproduce-first test: construct a parent-length-change
  scenario that drifts the child under the incomplete key, failing before and
  passing after the key includes the parent length. **If you cannot construct a
  failing repro** (the window-edge commit may already mask every reachable case),
  STOP at the byte-identical extraction and REPORT that the completion has no
  current repro — do NOT invent a behaviour change without a failing test.

## ORDER & GATES
1. Rebase on the consolidation. 2. Part 1a (abs1) — reproduce-first → suite +
goldens. 3. Part 1b (per-lane) — reproduce-first → suite + goldens. 4. Part 2
(span/`RingView` lane addressing first, then the) extraction — byte-identical bridge → suite + goldens (nothing observable moves).
5. (only if a failing repro exists) the shared-key completion — its own
reproduce-first. Each part its own commit. Hand the Rack feel-tests back to
Rodney: absolute-mode reverse with mid-range rotation (1a), a Forward lane under
a Reverse voice (1b).
