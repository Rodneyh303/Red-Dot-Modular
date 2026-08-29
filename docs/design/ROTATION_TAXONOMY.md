# Rotation taxonomy -- the family of rotations across pitch and time

dot.modular has MULTIPLE distinct rotations. They share ONE abstract operation (cyclic shift of an indexed
array = rotate(arr, k, modulus)) but act on DIFFERENT AXES with DIFFERENT MODULI at different pipeline
stages. So: share the rotate() UTILITY (generalise rotateMask12 -> rotateMaskN); keep INDEPENDENT controls;
offer an optional "rotate everything" COMPOUND on top. (Rodney, assembled across the microtonal thread +
the Dubai/SG legs.)

Code status: SandsVisualEditorV4.hpp already has a live per-lane `rotation` (EDIT value + dispRotation +
a rotation marker drawn WITHIN the start-end window, (dispOffset+dispRotation)%16). MonsoonWidget.cpp:1130-
1148 has a COMMENTED-OUT "DNA Rotation" menu naming: Rotate Rhythm Pattern, Rotate Melody Pattern, Rotate
EVERYTHING (+1). So this taxonomy is partly built + was already envisioned; this doc consolidates it.

## PITCH DOMAIN (the pitch-resolution pipeline)
Pipeline: step ->[PROBABILITY]-> degree ->[MASK]-> pitch-class ->[TUNING]-> cents; plus register:
- PROBABILITY rotation: which DEGREE (within the mask) fires per step. Emphasis / selection pattern.
  ~ maqam SAYR (which degree gets weight). Modulus = mask degree count / step count.
- MASK rotation: which degrees are MEMBERS of the scale (enabled pattern) -> which pitches map to which
  degree. Membership. rotateMask12/N. Modulus = N (degrees per octave).
- TUNING rotation: the CENTS of the pitches. Intonation. .dmtune/Sikit. Modulus = N.
- NOTE rotation vs OCTAVE rotation (Rodney's new split):
  - NOTE rotation = rotate WITHIN the octave (pitch-class), wraps at the octave. Modulus = N (degrees/oct).
  - OCTAVE rotation = rotate ACROSS octaves (register), wraps at the range boundary. Modulus = # octaves
    in range.
  Separate so you can shift pitch-class without changing register, or shift register without changing
  pitch-class. Different moduli, different gestures.

## TIME DOMAIN
- RHYTHM rotation: rotate the rhythm pattern (which steps are active) = "rotation of all rhythm elements".
  Mechanism = the Sands per-lane rotation (SandsVisualEditorV4 rotation marker within the window).
  Modulus = lane length (<=16).
- (MELODY rotation in the old menu = likely the pitch-side selection/note rotation applied per pattern.)

## COMPOUND
- ROTATE EVERYTHING = rotate pitch + rhythm together (the commented-out Rotate EVERYTHING (+1)). Built ON
  TOP of the independent controls, not instead of them.

## The unifying principle
All are cyclic array rotations -> share rotate(). All act on different axes/moduli/stages -> independent
controls. One CONCEPT (rotation) at many layers = elegant + learnable (user reasons about a stack of
rotations). Compose freely = a multi-dimensional rotation space (emphasis x membership x intonation x
pitch-class x register x rhythm), each axis a musically distinct gesture.

## OPEN placement question (Rodney to confirm)
"NOTE rotation" vs "PROBABILITY rotation" vs "MASK rotation" all touch "which note comes out" -- confirm
they're distinct:
- MASK = which degrees are IN the scale (membership).
- PROBABILITY = which in-mask degree is SELECTED per step (emphasis).
- NOTE = rotate the SELECTED/played pitch-class within the octave (output transpose within octave)?
If NOTE rotation is a transpose-the-output within the octave, it sits AFTER selection, near the pitch-class
layer alongside octave rotation. If it's really the same as probability (which degree fires), it's not a
separate axis. CONFIRM which, so the taxonomy doesn't double-count.

Cross-ref: PROBABILITY_MODIFIER_MODEL (the pitch pipeline: probability/mask/tuning layers), TONIC_
TRANSPOSE_BUILD_BRIEF (mask + tuning rotation, rotateMaskN, un-rooted-.scl framing), SandsVisualEditorV4
(per-lane rhythm rotation, live), MonsoonWidget.cpp:1130-1148 (the old DNA-rotation menu = the scaffold),
presets/maqam/README (rotation-vs-mode + sayr).
