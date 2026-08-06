# Microtonal / tuning -- MASTER DOC (entry point)

The microtonal/tuning/scales work is spread across several docs. This is the map. All of it is
POST-LIBRARY (not a 1.0 blocker). Read in this order.

## The core idea (one paragraph)
12-TET conflates two things a general system must separate: the TUNING (the set of available pitches per
octave + their cents -- the superset) and the SCALE (a selected subset of the tuning -- the mode/raga/
key). Mechanism: the current 12-bit scale mask generalises to an N-BIT MASK (N = the tuning's degree
count). Per-degree enable/disable (VCV Scalar's model) is the always-available base; optional curated
named-scale presets per tuning are the convenience layer. 12-TET is just the best-curated instance.

## Phased roadmap (Rodney's insight -- ship value incrementally)
Refactored: the microtonal work splits cleanly into three phases at the module-boundary layer.
- PHASE 1 -- TUNING EXPANDER (TUNING_EXPANDER_SPEC.md): retune 12-TET with a 12-cents-knob expander;
  scale stays with Monsoon. Ships "retune keep-your-scale" alone, no engine widening. Small module.
- PHASE 2 -- MICRO-12: full 12-tone microtonal (custom scale + tuning), same TuningTable, still N=12.
- PHASE 3 -- MICRO-24 + engine widening: arbitrary Scala N=24, the pervasive %12/*12 audit.
Compositional principle at module-boundary: ONE JOB PER MODULE. Tuning = one job (Phase 1); tuning-AND-
scale-at-N-degrees = a different job (Phases 2-3). Different modules, different scopes, cleanly ordered.

## The docs, in reading order
1. TWELVE_TET_AUDIT.md -- where 12 is hardcoded in the engine; the N-bit-mask generalisation target.
   Framed for Monsoon Micro (12-slider + 24-slider fixed panels). THE ENGINE AUDIT.
2. SCALES_AND_QUANTIZER_TODO.md -- THE MAIN DOC. Scales-to-add list (Slendro priority, Chinese
   pentatonic, Carnatic); scales-within-tunings design (scale subset-of tuning subset-of octave; N-bit
   mask; hybrid manual + curated named scales); .scl role-agnostic (tuning OR scale by use); .kbm =
   keyboard mapping, irrelevant for slot-less CV quantizing BUT the right tool for Monsoon Micro's fixed
   12/24 faders (fader bank = keyboard). Curation = real ethnomusicological work.
3. TUNING_EXPANDER_SPEC.md -- PHASE 1 module: 12 cents knobs, retune 12-TET without touching the scale
   mask. Small, ships first. Common real-world microtonal case (well-tempered, meantone, stretch, expressive).
4. MONSOON_MICRO_SPEC.md -- PHASES 2 & 3 modules: the 12/24 fixed-fader TUNING/SCALE AUTHORING expanders (Scalar-modelled:
   per-degree cents + enable/disable + .scl read/write; one cents dial + edit-mode selection; delegation
   rule = one Micro attached -> Monsoon faders blank, authority delegates). AUTHORING home.
5. SHOPHOUSE_SPEC.md -- Shophouse = the scale/tuning selector expander (CONSUMING: import/display/modulate). In the generalised world it
   loads a .scl (tuning) and its shutters become N tuning-width degree toggles (shutter = pitch).
6. SHOPHOUSE_FACADE_NOTES.md -- Shophouse panel/facade (Peranakan). Functional layer DONE on hardware;
   facade redesign decided-not-built.

## Key decisions already made (see the docs for detail)
- Scale = N-bit mask over the loaded tuning's degrees (generalises the 12-bit mask). [SCALES_AND_QUANTIZER]
- Hybrid: manual per-degree toggle (Scalar-style) always; curated named-scale presets per tuning where
  the ethnomusicology is done right (Pelog pathets etc.). Don't half-do cultural scales. [SCALES_AND_QUANTIZER]
- Root/transpose in UNEQUAL tunings is ABSOLUTE, not a rotatable mask (intervals differ per degree).
  [SCALES_AND_QUANTIZER]
- .scl = pitch list (tuning or scale by role). Consume for both tuning + named-mode presets. [SCALES_AND_QUANTIZER]
- .kbm = degree->slot map. SKIP for CV quantizing; USE for Monsoon Micro's fixed 12/24 faders. [SCALES_AND_QUANTIZER]
- Shophouse is the home: tuning loader + N-width degree toggles. [SHOPHOUSE_SPEC]

## Cross-cutting: the tuning table is SHARED across ALL modes
Monsoon Micro defines the tuning table for GENERATIVE output (modes A/B/E). But the QUANTIZER modes C & D
(MODES_C_D_QUANTIZER_PRERELEASE.md) must quantize to the SAME table -- a Micro that retunes A/B/E but
leaves C/D on 12-TET would be incoherent. Design the tuning table as one shared structure all modes read.
(C/D also need a NEGLECT pass pre-release, independent of microtonal -- see that doc.)

## Open / needs work (all post-library)
- Named-scale CURATION per tuning (the real ethnomusicological effort).
- Absolute root/transpose handling for unequal tunings (engine change).
- Monsoon Micro 12/24 fixed-fader variants + the .kbm-style degree->fader mapping. [TWELVE_TET_AUDIT]
- Scale additions: Slendro (priority), Chinese pentatonic, Carnatic. [SCALES_AND_QUANTIZER, small]

## Related but not core (incidental tuning mentions)
PITCH_PATCHABILITY_AND_DISTINCTION.md (East/West axis, Pelog as a pole), SEED_OFFSET_DESIGN.md
(unrelated, mentions in passing). Not part of the microtonal build.
