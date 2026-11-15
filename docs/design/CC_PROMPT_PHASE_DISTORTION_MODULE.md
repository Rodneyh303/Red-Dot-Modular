# CC PROMPT  Phase distortion, STANDALONE module model (§13, comparison build)

Purpose: build the standalone version so it can be compared with the expander version (§14, CC_PROMPT_PHASE_DISTORTION_EXPANDER.md).
Authority: PHASE_DISTRIBUTION_SPEC.md §13 and PHASE_DISTORTION_COMPARISON.md. Build on feat/phase-duals-gaps.
Do NOT modify the expander version. Build B alongside A; do not delete A.

## THE MODEL
- A standalone PHASE DISTORTION MODULE with EIGHT phase inputs and EIGHT phase outputs. Channel k in -> channel k out.
- Each channel has its own parameter set (warp, tilt, gap, atom, delegation) and its own plot in the module.
- IDENTITY BY DEFAULT: an unpatched or zeroed channel passes phase through unchanged (plus the module's fixed hop).
- The transform is the SAME shared primitive as the expander version (Task 2 of the expander prompt). Compile it once and
  use it from the module. Do not copy the chain into the module.
- EVERY Monsoon's phase and subphase come from the module's OUTPUTS. The channel is the jack: a Monsoon patched to output
  k uses parameter set k. Several Monsoons may share an output.
- Monsoon gets NO transform code and NO index parameters in this build. Its inputs and behaviour are otherwise unchanged.

## TASK 1  The shared primitive (only if not already present)
If the expander prompt's Task 2 primitive exists, use it. Otherwise factor the chain (tilt -> gap -> atom, warp mix,
quantise-to-pulses) into one callable primitive: phase in [0,1), parameter set, reading by warp sign, returns distorted
phase. Identity parameters return the input unchanged. Keep the landed gap convention, clamp eps = 1e-6, atom composition
and dual sign rule exactly.

## TASK 2  The module
- Eight phase inputs, eight phase outputs, eight parameter sets.
- Per channel: warp, tilt, gap (location, size), atom, delegation, and the plot (reuse the existing distribution plot
  widget, one per channel or for the selected channel).
- Panel: follow the existing distribution-module conventions. Gap knobs and jacks beside the atom, as landed.
- Delay: the module adds one hop. Document it in the module's manual text. Do not compensate in software.

## TASK 3  Monsoon compatibility (no transform, no new indices)
- Monsoon's phase and subphase inputs are unchanged. The patch rule is a PATCHING CONVENTION: phase comes from the module.
- Add NO transform code, NO index parameters, NO store reads to Monsoon in this build.

## GATES
1. IDENTITY (the adjusted gate): with all eight channels at identity, the module's output equals the source phase delayed
   by EXACTLY ONE sample. Test by comparing the module output with the source shifted one sample. Not "bit-identical to
   today" without the shift.
2. CHANNEL SEPARATION: changing channel k's parameters changes only the output of channel k.
3. SAME OUTPUT, SAME RESULT: two Monsoons patched to output k produce identical outputs, sample for sample.
4. SHARED PRIMITIVE: the transform exists once; grep confirms no second copy in the module.
5. GAP/ATOM/DUAL semantics unchanged: the existing phase distribution tests stay green.
6. A/B READY: with matching parameters, the module's output equals the expander build's output for the same input,
   delayed by one sample. This is the comparison test between the two models.
7. run_all green on both SDK legs.

## DO NOT
- Delete or alter the expander build (A).
- Change the gap convention, clamp, atom composition, dual sign rule, warp mix, or the uniform-centred morph.
- Put transform code or index parameters in Monsoon.
- Compensate the one-sample hop in software.
