# CC PROMPT — Phase distortion, expander model (adopted, spec §14)

Authority: docs/design/PHASE_DISTRIBUTION_SPEC.md §14 (adopted). §12 (expander routing matrix) and §13 (module-only)
are NOT built. Build on feat/phase-duals-gaps (gap stage, dual reading, warp and plot already landed).

## THE MODEL (do not change)
- Each Monsoon takes PHASE and SUBPHASE DIRECTLY from the phase source (one cable hop). No Monsoon takes its phase
  through another module. This keeps every path the same hop count, so all Monsoons stay in sync.
- The TRANSFORM (tilt, gap, atom, warp, and the quantise-to-pulses step) runs INSIDE each Monsoon as ONE SHARED PRIMITIVE.
  Compile it into Monsoon. Do not duplicate it per Monsoon or per module.
- EIGHT PARAMETER SETS (warp, tilt, gap, atom, delegation) live in the EXPANDER's store.
- Each Monsoon holds TWO INDICES: one selects the parameter set for phase, one for subphase. These are the only
  routing choices. No matrix.
- A Monsoon reads its parameter set from the store with ZERO DELAY.

## TASK 1  Store consistency mechanism (do this FIRST, it gates everything)
Inspect src/ui/StoreBound.hpp and the store it writes. Determine whether a Monsoon reading a parameter set sees the same
values as every other reader within one audio block.
- If it does NOT, implement ONE of:
  (a) PER-BLOCK SNAPSHOT: each channel set is copied once at block start; all readers in that block use the snapshot.
  (b) DOUBLE-BUFFERED STORE: writes go to a back buffer and become visible only at a block boundary (swap at block start).
Pick the one with the smaller change and no per-sample locking. Document the choice in the PR.
Gate: test with TWO reader instances and a parameter write between blocks; both readers see the new value at the SAME
block boundary, never one before the other.
Do NOT rely on relaxed atomics alone.

## TASK 2  Shared transform primitive
Factor the distortion chain (tilt -> gap -> atom, with the warp mix and the quantise-to-pulses step) into one primitive
callable from Monsoon. It takes: phase in [0,1), a parameter set, and the reading (quantile/dual by warp sign). It returns
the distorted phase. Identity parameters return the input unchanged. Keep the landed gap convention, clamp (eps = 1e-6),
atom composition, and dual sign rule exactly as they are.

## TASK 3  Monsoon wiring
- Each Monsoon: a PHASE input and a SUBPHASE input from the source cable (existing phase input behaviour unchanged).
- Two index parameters per Monsoon: phaseSet (1..8) and subphaseSet (1..8). DEFAULT 2N-1 and 2N for Monsoon N.
- Apply the shared primitive to the phase and to the subphase with the selected sets. Quantise each to the Monsoon's
  own 24-PPQN pulse grid, as the existing snap does.
- Subphase UNPATCHED: identity, the Monsoon behaves exactly as today.

## GATES (all must pass)
1. IDENTITY: all eight sets at identity, each Monsoon's output bit-identical to today (Stage 1 gate). No re-pins of
   <=16 goldens.
2. SAME INDEX: two Monsoons with the same phaseSet and subphaseSet produce identical outputs, sample for sample.
3. DIFFERENT INDEX: two Monsoons with different sets differ only by the parameter sets they read.
4. BLOCK CONSISTENCY: the Task 1 test (both readers switch on the same block boundary).
5. SYNC: a Monsoon's phase path has the same hop count as the direct source path (no module between source and Monsoon).
6. SINGLE IMPLEMENTATION: the transform exists once; grep confirms no second copy in Monsoon or the expander.
7. Gap/atom/dual semantics unchanged: the existing phase distribution tests stay green.
8. run_all green on both SDK legs.

## DO NOT
- Build the matrix (§12) or the module-only model (§13).
- Change the gap convention, clamp, atom composition, dual sign rule, warp mix, or the uniform-centred morph.
- Add Monsoon phase inputs beyond phase and subphase.
- Use bisection or integration for any plot curve.

## ORDER
Task 1 (store consistency) -> Task 2 (primitive) -> Task 3 (Monsoon wiring). Gate 4 (block consistency) must pass before
Task 3 is merged.
