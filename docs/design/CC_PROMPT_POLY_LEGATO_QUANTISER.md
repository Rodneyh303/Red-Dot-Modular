# CC prompt — poly legato / re-articulate / mid-hold must honour the quantiser (degree AND octave)

RULING (Rodney, 2026-11-22): option A. All three poly sites that currently draw a
generated pitch must follow the quantiser decision, for BOTH the degree and the octave axis.
Rack behaviour needs his feel-test; everything below is code-reading + unit tests.

## The bug (verified by reading `src/dsp/engines/SequencerEngine.cpp`)
`quantiserPitchSource` modes route pitch through `voicePitch(...)` (hpp ~1762), which takes
`forceGenerated` (degree axis) and `octForceGenerated` (octave axis) and picks quantised CV,
generated, or a MIXED row. The poly ONSET path (~2853) does this correctly. Three other poly
sites call `pe.genPitchLive(...)` directly, so they always draw a generated degree AND octave:

| site | line (approx) | what it is |
|---|---|---|
| S1 | ~3001 | committed legato slide + re-articulate ("draws its OWN pitch") |
| S2 | ~3047 | mid-hold Legato follow (`holdFollow && wasHeldPoly`, mono slid) |
| S3 | the re-articulate arm of S1 (`else` after `if (connect)`) | fresh onset on a re-struck note |

Consequences in a quantiser mode: after the first note of a phrase a poly voice's slides and
re-strikes ignore the quantiser input, and the octave is re-drawn from the octave lane every
time. Mono is fine (`voicePitch` at ~1964, and `slideNote(..., isQuant)` at ~2056), so mono
and poly disagree. The slides at ~3003 and ~3048 also omit the `isQuant` argument that
`slideNote` takes (colour/flash path); pass it.

## Required behaviour
At S1, S2, S3 compute, per draw, exactly what the onset path computes:
- the q-mix draw for this voice at this step: `r_qmix_voice = pe.warpRow(voiceIdx, PL_QMIX)[qmixIdx]`
  after `effectiveLaneLor` / `getStrandIdx(polyLaneTick(...))` / `refreshPolyRandomCell(PL_QMIX)`;
- `qmixUseGenerated = quantiserPitchSource && (r_qmix_voice >= v.qmixLevel)` (degree axis);
- `qmixOctUseGenerated = quantiserPitchSource && (r_qmix_voice >= v.qmixOctResolved())` (octave axis);
- `isQuant = quantiserPitchSource && !qmixUseGenerated`;
- `pitchV = voicePitch(voiceIdx, sem, input, r_semi, r_oct, qmixUseGenerated, qmixOctUseGenerated)`;
- pass `isQuant` to `slideNote` / `triggerNote`.

So the degree and the octave each keep their own quantiser decision on a slide or re-strike,
including the MIXED rows (degree external + octave generated is "the musical case": the
external theme's degree rides the voice's own octave).

Prefer ONE private helper (e.g. `polyPitchDraw(voiceIdx, ..., outSem, outIsQuant)`) used by
the onset path AND S1/S2/S3, so the four sites cannot drift again. The onset path must stay
byte-identical.

## Byte-identity guard (important)
The onset path calls `refreshPolyRandomCell(PL_QMIX)` unconditionally. At S1-S3 you must NOT
add a QMIX refresh/draw when `quantiserPitchSource` is false — it can change RNG/lane state and
break byte-identity outside quantiser modes. Guard the new QMIX work with
`if (quantiserPitchSource)`; outside quantiser modes S1-S3 must reduce to exactly today's
`genPitchLive` call with the same draw order. Prove it with a before/after trace test.

## Tests (failing first, Rack-free; register in `test/run_all.sh`)
Add `test/test_poly_legato_quantiser.cpp`. Build the engine the way existing quantiser tests do
(see the qmix / `qmixOctLive` tests; copy their harness). Independent oracle: expected pitch
computed from the quantised CV directly (`quantize(quantiserCV[v])` degree, floor octave), not
by calling `voicePitch`.
1. Quantiser mode, qmixLevel such that every draw picks quantised, qmixOctLive both off/on:
   force a poly legato SLIDE (S1 connect), a RE-ARTICULATE (S1/S3), and a MID-HOLD slide (S2).
   Assert each voice's pitch == quantised CV (degree AND octave). Fails today (generated).
2. MIXED: degree quantised, octave generated (qmixOctLive on, octave threshold different):
   assert degree == quantised degree, octave == the voice's generated octave. And the reverse.
3. Per-voice: 16ch CV with different pitches per voice; each voice follows ITS OWN CV on slide.
4. Changing the CV between onset and the slide: the slide follows the CV at the slide step.
5. Byte-identity: quantiser mode OFF — full poly run trace (pitch, gate, slurMember, verdict)
   identical before/after the change across several seeds (capture the golden trace from the
   pre-change binary first and check it in as a test fixture or compute both in one run via the
   old call kept in the test as the oracle).
6. `isQuant` reaches `slideNote`/`triggerNote` at all four sites (assert the flag the green-flash
   path reads).
Mutation-check: (a) revert S1 to genPitchLive -> test 1 fails; (b) revert only S2 -> mid-hold
case fails; (c) pass octForceGenerated = forceGenerated -> test 2 fails; (d) drop the
`quantiserPitchSource` guard around the QMIX draw -> byte-identity (test 5) fails.
Show red first, then green, then the full suite: `CPLUS_INCLUDE_PATH=/home/claude/simde bash test/run_all.sh`.

## Also
- Update the comments at S1/S2 ("draws its OWN pitch (melody/octave LOR), never mono's") — in a
  quantiser mode it draws per the q-mix decision.
- Add a line to `docs/design/QUANTISER_MODES_UNIFICATION.md` stating: slide / re-articulate /
  mid-hold honour the degree and octave quantiser decisions.
- Open question to answer in your report, do not guess: does a legato SLIDE in quantiser mode
  with quantised octave re-read the octave from the CV (a glide across octaves), or must it keep
  the held note's octave? Report what `voicePitch` yields and flag it for Rodney; do not
  change it unprompted.
- Say "needs Rack feel-test" for audible behaviour: slide/re-strike in quantiser modes, MIXED
  degree-ext/octave-gen, 16ch CV.
