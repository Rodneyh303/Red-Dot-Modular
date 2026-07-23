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

## Round 2 (Rodney's sample-rate audit) — FIXED

- **East prob CV outs** were the heaviest per-sample work in the plugin: laneStep() +
  laneProbability() for every lane x voice, EVERY SAMPLE (up to 4 x 16 = 64 resolver calls).
  Now recomputed on a /32 divider = ~1.5 kHz at 48k, matching the plugin's own controlDivider
  target. Rodney confirmed these are explicitly NOT for audio-rate modulation. The OUTPUT is
  not gated -- ports hold their last voltage, so CV stays continuous; only the COMPUTATION is
  throttled, and channel counts are still set per sample so polyphony changes apply at once.
- **findMonsoonEitherSide() cached in all three expanders** (East, Mono, Macro). It walks the
  expander chain and ran every sample; topology only changes at control rate, so the pointer
  is refreshed on a /8 divider.
  GOTCHA: East shares one divider between the pointer refresh and the gate scan via a flag --
  calling .process() twice would consume two ticks and silently halve the effective rate.

Confirmed already fine (no change): gate-mod edge scan at /8 (gates are milliseconds);
Lantern step-edge sampling (early-returns unless the step changed); processDNA/sync (gated on
sixteenthEdge); Monsoon control-rate section (gated on sixteenthEdge).

## Where to look next if more is needed
1. Widget `step()` methods — these run at frame rate, so they are only a problem if they do
   something very heavy (full-grid rebuilds, SVG reloads, allocations per frame).
   Macro's step() now carries the MVC dual-write mirror (44 param reads/frame — trivial).
2. `MonsoonExpanderManager::sync` — control rate, but it walks the whole expander chain.
3. The pattern to grep for generally: assignments to display/visualisation state that are
   NOT inside a divider.
