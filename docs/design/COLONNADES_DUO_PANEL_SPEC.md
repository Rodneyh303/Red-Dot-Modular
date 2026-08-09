# Colonnades Duo panel -- ONE instruction: it is gen_colonnades.py with N=24

Do NOT hand-build or iterate a new Duo panel. Colonnades Duo IS structurally two Colonnades. The
panel generator gen_colonnades.py is ALREADY fully parameterised on N (N=12, PITCH=9.0, FIRST_X=7.5,
fader_cx(i)=FIRST_X+i*PITCH, W=FIRST_X+(N-1)*PITCH+FIRST_X). Every element -- faders, level ticks,
number strip, staggered cents knobs, display cells -- is a function of N. So the Duo panel is the
SAME generator with N=24. Nothing about the layout logic changes.

## The instruction
Produce the Duo panel by running the Colonnades generator with N=24, either by:
- **Option A (preferred):** parameterise gen_colonnades.py to take N as an argument (or a second entry
  point gen_colonnades_duo.py that imports the same functions with N=24). ONE codebase, two outputs.
  This guarantees the Duo is literally two-Colonnades-wide with identical styling, and any future
  panel tweak applies to both automatically.
- **Option B:** copy gen_colonnades.py -> gen_colonnades_duo.py, change only `N = 12` -> `N = 24`.
  Acceptable but duplicates; Option A is cleaner.

With N=24 the existing formulas already give:
- 24 faders at 7.5 + i*9.0 mm (same 9.0mm pitch -- so the Duo reads as two Colonnades fader blocks
  end to end).
- Width W = 7.5 + 23*9.0 + 7.5 = 222mm (~43.7 HP) -- exactly twice-minus-one-margin of Colonnades'
  114mm, because the faders tile at the same pitch. This IS "two copies joined."
- 24 numbered labels 1..24 below the faders (the widget loop already runs to N).
- 24 staggered cents knobs on the two zigzag rows (the even/odd stagger logic is N-agnostic).
- The cents display cells extend to 24 (same per-degree cell, more of them).

## What must NOT change from Colonnades (keep identical, so it reads as two-of-the-same)
- PITCH = 9.0, FIRST_X = 7.5 (fader geometry identical -> faders align if stacked, and the two halves
  of the Duo are visually continuous).
- FADER_TOP=45, FADER_BOT=74.5, FADER_CY=59.75 (travel identical).
- TICK_LEVELS / tick style (level markers identical).
- NUM_Y=80, CENTS_ROW_A=92, CENTS_ROW_B=101, KNOB_R=3.0 (number strip + staggered knob rows identical).
- ColonnadesLightSlider<GreenRedLight>, theme dicts, DSEG cents display, staggered Scalar-style grid
  (round-7) -- all identical, just 24 of them.
- Root (degree 0) locked-plate, no cents knob -- same as Colonnades (only degree 0 is the root; degrees
  1..23 all have cents knobs).

## The ONLY Duo-specific differences
1. N = 24 (the one value that drives everything).
2. Wordmark "Colonnades Duo" instead of "Colonnades".
3. NOTES readout ranges 1..24 instead of 1..12 (the knob/readout is N-aware; it already derives from
   the mask, so it just spans more degrees).
4. Nothing else. The maqam/24-tone SEMANTICS are engine-side (M4, MICROS_ENGINE_CLAUDE_CODE_GUIDE),
   NOT panel. The panel is purely 24 of the Colonnades block.

## Why this ends the panel dance
The previous rounds churned because "make it look like X" invited reinterpretation. This instruction is
mechanical and unambiguous: SAME generator, N=24, same constants, wordmark + NOTES range are the only
edits. If the output does not look like two Colonnades joined, the cause is a constant that failed to
parameterise on N -- find it and make it N-derived, do not hand-adjust positions. Single-source
geometry (the collection's established principle): the panel art is a function of N, not a hand-placed
layout.

## Verify
- Render at N=24, place directly beside a Colonnades (N=12): the fader pitch, tick style, number strip,
  knob stagger, and display cells must be visually identical, just 24 wide vs 12.
- Grep the generator for any hardcoded 12 / 114 / 11 that should be N / W / (N-1) -- those are the
  bugs that would break the "two copies" look.

## Cross-refs
- panel_src/gen_colonnades.py -- the N-parameterised generator (N=24 for the Duo).
- COLONNADES_PANEL_LIFT_SPEC.md -- the Colonnades panel design (round 7 grid, the block being doubled).
- MICROS_ENGINE_CLAUDE_CODE_GUIDE.md -- the N=24 ENGINE work (separate from this panel work).
