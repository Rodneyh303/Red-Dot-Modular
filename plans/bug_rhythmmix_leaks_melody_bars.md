# BUG — modulating RHYTHM mix (via Raffles) changes VOICE 2 MELODY bars (sudden/jumpy, not smooth)

## Symptom
Modulate rhythm mix (Raffles CV). Voice 2's MELODY probability bars change at the same time — and the change
is SUDDEN/JUMPY (not the smooth scrub morph). Cross-lane leak (rhythm -> melody), discrete movement.

## RULED OUT (don't re-check these — verified)
- Raffles CV routing: rafflesToCv3[] is CORRECT — MIX_R(Raffles 3) -> cv3 2 (Rhythm mix); MIX_M -> cv3 3
  (Melody mix). getRhythmMix uses cv3Offsets[2], getMelodyMix uses cv3Offsets[3]. Clean. (This is a SIBLING
  of the fixed 713ff703 SLEW_Q->RhythmMix cross-wire, but THIS mapping is right.)
- Recompute triggers (refreshVisualCache ~692): per-stream gated — rhythm-mix change calls ONLY
  recomputeEffectiveRhythmForVoice; melody is independently gated on melodyMixLatched/melodyDrawCtr.
- recomputeEffectiveRhythmForVoice does NOT touch melody arrays.
- genPitchLive does NOT depend on the rhythm pattern (reads in.semiWeights/octaveLo/Hi/transpose only) —
  so it's NOT a melody-gated-by-rhythm musical coupling.
- xi bridge keys are separate per stream (S+4/5/6).

## PRIME SUSPECT — the unconditional melodyPitchV loop + a MONO/POLY array mismatch
refreshVisualCache ENDS with a loop that ALWAYS runs (every call, regardless of which stream changed),
recomputing melodyPitchV from melodyRandom (PatternEngine.cpp ~710):
    for (int i=0;i<16;++i) {
        rhythmPattern[i] = (rhythmRandom[i] >= in.restProb);
        melodyPitchV[i]  = genPitchLive(sem, in, melodyRandom[i], octaveRandom[i]);   // <-- always
        melodySemitone[i]= sem;
    }
So a refreshVisualCache TRIGGERED BY RHYTHM-MIX still re-runs this melody-pitch loop. If melodyRandom /
octaveRandom hold a stale or WRONG-ARRAY value for the displayed voice, melodyPitchV jumps.

Likely root = MONO/POLY array mismatch for the DISPLAYED voice (ties to the [15][16]+mono-twin split,
COLLAPSE_15_TO_16.md):
- melodyRandom/slewedMelody are the MONO (V1) arrays; per-voice is slewedPolyMelody[v][i] /
  (poly melodyRandom twin).
- recomputeEffectiveMelodyForVoice(displayedVoice_) writes the POLY array for that voice.
- But the melodyPitchV display loop reads the MONO melodyRandom.
- So for a POLY displayed voice (voice 2), the displayed melody bars are derived from the MONO array, not
  voice-2's poly array — and whatever last wrote melodyRandom (a prior redraw / the mono path) leaks in,
  JUMPING when rhythm-mix forces the loop to re-run.

## What to confirm / fix
1. CONFIRM: with displayedVoice_ = 2 (a poly voice), what array does melodyPitchV read — the MONO
   melodyRandom, or slewedPolyMelody[2]? If it reads MONO while the displayed voice is poly -> that's the
   bug (display shows the wrong voice's/stale melody, jumps when the loop re-runs).
2. FIX: the display pitch loop must read the DISPLAYED VOICE's array (poly slewedPolyMelody[displayedVoice_]
   for poly voices, mono for voice 0), consistently with what recomputeEffective*ForVoice writes. I.e. the
   melodyPitchV (and rhythmPattern) display derivation must be voice-correct for displayedVoice_, not
   hardwired to the mono arrays.
3. Also: does this loop even need to re-run when only RHYTHM changed? rhythmPattern[i] depends on rhythm;
   melodyPitchV depends on melody. Split so a rhythm-only change doesn't recompute melodyPitchV (and vice
   versa) — eliminates the spurious re-derivation entirely.

## Verify
Modulate rhythm mix @ displayed voice 2: melody bars DON'T move. Modulate melody mix: melody bars scrub
SMOOTHLY (voice 2's poly array). Rhythm bars scrub smoothly. No cross-lane leak. (This likely also wants
the [15][16]->[16][16] collapse to remove the mono/poly split that enabled the mismatch — but a targeted
voice-correct read fixes the bug now.)
