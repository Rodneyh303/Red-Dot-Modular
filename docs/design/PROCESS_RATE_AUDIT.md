# process() rate audit — what runs per-sample vs throttled

Prompted by Rodney noticing CPU creep. Rule: audio-rate work belongs in `process()`;
control-rate work goes under `controlDivider` (~1500 Hz); DISPLAY/visualisation state goes
under `lightDivider` (frame rate). Anything computed only for a widget to read is display.

## Fixed

**`Monsoon::process` — `modViz` block (was FULL SAMPLE RATE).** Gated only on
`if (paramManager)`, which is always true. Each pass resolved ~35 modulated values
(`big5Lane`×5, `cv3Lane`×4, `semitone`×12, `pitchLane`×14, plus slew/mix/octave/active
flags) via MonsoonParameterManager. Every consumer is in `MonsoonWidget.cpp` — display only.
At 48 kHz that was ~1.7M modulation resolutions/second that no audio-rate code read. Moved
under the `lightDivider` block immediately below it. Predates the de-param work (present on
master).

## Audited and found CLEAN — do not re-investigate without new evidence

- **Expander `process()` bodies** — all small (2–31 lines, no heavy loops).
  Change Alley's single loop is the 8-row restructure-queue latch (8 iterations, trivial).
  Sands Visual / Macro / Interchange / Changi do essentially nothing per sample.
- **`Monsoon::process` head region (pre-divider)** — the two loops there are properly
  conditional, not per-sample: the poly `tickPulse` loop sits inside `if (gridPulse)` (fires
  on clock pulses only), and the nested step/pulse loop is inside the phase-JUMP handler.
  The remaining straight-line code is genuine audio work (input voltage reads behind cached
  connection flags, clock/gate/mode dispatch, outputs).
- **`processDNA`** — inside `if (controlDivider.process())`, correct. The Change Alley
  pre-spread remap (`forceRecomputeSlewed` + `remapSlewedByPins`) is additionally guarded to
  run ONLY when pins are off the identity diagonal, so it costs a 16-iteration check in the
  common case.

## Where to look next if more is needed
1. Widget `step()` methods — these run at frame rate, so they are only a problem if they do
   something very heavy (full-grid rebuilds, SVG reloads, allocations per frame).
   Macro's step() now carries the MVC dual-write mirror (44 param reads/frame — trivial).
2. `MonsoonExpanderManager::sync` — control rate, but it walks the whole expander chain.
3. The pattern to grep for generally: assignments to display/visualisation state that are
   NOT inside a divider.
