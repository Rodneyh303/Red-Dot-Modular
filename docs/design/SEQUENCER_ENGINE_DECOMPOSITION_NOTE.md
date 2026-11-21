# Note — Is SequencerEngine handling too much? (decomposition assessment)

**Date:** 2026-11-17 · **At:** `lane-expander-refactor`
**Trigger:** SequencerEngine is ~2k loc `.hpp` + ~3k loc `.cpp`. Size alone isn't the problem; the
question is whether it has accreted separable concerns. Assessment below (surveyed the
function list, not just the line count).

## Verdict
Not a god-object yet, but trending that way: it has accreted ~3–4 concerns beyond its core.
Size is the symptom; the mixing is the thing. Keep the core whole; peel read-only views and
self-contained units off, **driven by friction, not line count**.

## The core — keep it as ONE tight unit (do NOT split)
The per-step orchestration state machine and the holders it drives:
`advancePlayhead` / `tickHolders` / `deriveHolders` / `executeStep` /
`executeModeA|B|BSubdivided` / `executePolyVoices` / `handlePhraseBoundary`, plus the
position holders. Tightly coupled by design; splitting it just scatters one state machine.

## Bolted-on concerns with clean seams — extraction order
1. **Display/lights adapter (lowest risk, do first).** `getBarLightBrightness`,
   `getStepLightBrightness`, `syncVisuals`, `macroLaneDisplay`, `stepAdvanceDrawAt` — UI-facing
   READ-ONLY views of engine state. No state, no ownership → safe to move into an
   `EngineView`/adapter. Shrinks both files immediately.
2. **Scale / quantize.** `quantize`, `rebuildScaleCache`, `degreeOf`, `computeNoteLengthIdx`,
   `snapToPhaseStep` — a self-contained music-theory unit with its own cache; the engine only
   needs it as a collaborator. Testable in isolation.
3. **The latched-at-boundary commit lifecycle (= CA1 in the concept audit).** The LOR/dir/sign
   commit plumbing (`refreshVoiceLorLatches`, `refreshLaneLatchesForVoice`, `pushVoiceLorCv`,
   `setLorCvSuspend`, pending stores, `dirLive_`/`lorLive_` gates). NOT a file move — a
   refactor-in-place into a `LatchedAtBoundary<Policy>` skeleton (same shape as
   `SandsModifierResolver<Policy>`). Pays back in fewer *bugs*: last night's double-commit was a
   drift inside the LOR copy of this lifecycle.
4. **Position — finish the journey already started.** `PositionResolver` (`pos_`) exists;
   migrate the remaining walk (`laneTickFor`, `getStrandIdx`, the `voiceFramePhase_`/holders)
   behind it. This is the natural endpoint of the globalWalk_ / holder-model work.

## Caveats
- **Split on pain, not loc.** Trigger = "edits to X keep forcing touches to unrelated Y" or
  "bugs cluster at this seam." If the display helpers never interfere with the DSP core in
  practice, the split buys little.
- **Part of the `.hpp` bloat is duplication, not responsibility.** The inlined `effective*Lor`
  accessor family is written ~8× (prior audit F7 / concept audit CA4). Collapsing it to one
  `composeLor_` shrinks the header without moving any responsibility — do that before judging
  size.

## Cross-refs
- Concept ODR audit (CA1 = the latched-control lifecycle; CA4 = the effective-LOR shape).
- Line-level audit (`LINE_LEVEL_ODR_AUDIT.md`) F7 (`composeLor_`), F3 (direction authority — rides with the position/dir work).
- Natural pairing: the CA1 extraction + the C3 direction track, together.
