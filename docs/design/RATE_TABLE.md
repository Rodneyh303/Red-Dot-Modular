# Rate table -- modulation inputs x (trigger, quantisation) -- FOR AUDIT

Companion to RATE_DISCIPLINE_UNIFICATION. Enumerates the modulation/shaping inputs and classifies each
by TWO axes (the key refinement): TRIGGER (what causes an update) and QUANTISATION (what boundary the
update snaps to / is consumed at). Built from the current code -- AUDIT the "correct?" column against
the running build. This is the plan the re-timing pass executes against (after the WriteLedger is wired).

## Rungs (confirmed against code)
- SAMPLE (~48k): audio loop.
- PPQN PULSE (pulsesPer16th = ppqn/4 = 6/12/24 per 16th): sub-16th grid. **Consumed ONLY by gs.tick()
  for gate-DURATION timing. Everything else is 16th-gated.** => PPQN>24 buys only finer gate-length
  resolution at 2-4x the per-pulse edge/tick cost. OPTIMISATION CANDIDATE: cap at 24, or make >24 opt-in.
- 1/16 STEP (sixteenthEdge): pattern advance + ALL pitch/rest/accent/pattern decisions (executeModeA
  returns early unless sixteenthEdge). THE consumption rate for shaping inputs.
- BEAT (quarterEdge, every 4 sixteenths): EXISTS as an edge but currently no shaping mod samples per-beat.
  LATENT rung -- empty now, valid target if a beat-locked mod ever appears.
- PHRASE / BAR (wrapped): loop boundary; queued events applied here.
- CONTROL (block, once per process()): block-level mod math, getEffective* reads, expander sync.
- SCREEN (~60Hz, draw()): display only, pulled from engine, never cached across.
- PHASE DRIVE: NOT a separate continuous rung -- PhaseEngine converts phase->pulse/16th edges (same
  contract as internal clock). So phase-driven playback is QUANTISED TO THE SAME PPQN/16th grid. (Your
  read confirmed: sampled at ppqn rate.) Only distinction: edges can fire in REVERSE.

## Trigger types (the second axis -- not rates)
- PERIODIC (clock/phase edges): the rungs above.
- APERIODIC EVENT: dice, scatter, scene-advance, reset, run/stop -- fire on a trigger, not a clock;
  APPLICATION is phrase-quantised. (trigger rate != application rate.)
- USER-GATE / UNLOCK: latched config released on user action. A gate, not a rate.

## The table (AUDIT the last two columns)
Legend: Trigger = what updates it | Consumed-at = boundary it actually takes effect | Writers-now =
approx write sites | Correct? = does the current sampling match what it feeds (Y / ? / FIX).

| Input | Feeds | Trigger | Consumed-at (now) | Writers | Correct? |
|---|---|---|---|---|---|
| mono restProb | pattern READ (rest roll) | control (getEffectiveMonoRest each block) | 16th (executeModeA) | 1 (currentPatternInput) | Y -- written every block, consumed at 16th; safe (idempotent) |
| poly restProb[i] | per-voice rest | control (getEffectivePolyRest each block) | 16th | 1 per voice (ModeController:19) | ? audit: also a commented ExpanderManager write -- confirm single writer |
| mono accentProb | accent decision | control | 16th | 2+ (ModeController :112 & :135) | FIX? two write sites -- WriteLedger will show if same-block conflict |
| poly accentProb[i] | per-voice accent | control | 16th | 1 (ModeController:20) | ? audit vs any expander write |
| legatoProb | legato roll | control | 16th | 1 | Y |
| noteVal | note length | control | 16th (consumed) + gs.tick (pulse, duration) | 1 | Y (value at 16th; duration ticks at pulse -- correct split) |
| LOR (len/off/rot) per strand | pattern READ | control (Sands read) | 16th | 1 via setStrand (ledgered) | Y -- StrandLedger already guards |
| A/B mix prob | pattern gen blend | control CONTINUOUS (no wrap latch) | continuous (blends live) | 1 | Y -- intended continuous, not step-quantised |
| spread prob | poly spread | control CONTINUOUS | continuous | 1 | Y (audit: double-clamp history -- MODULATION_CLAMP_INVARIANT) |
| direction | playhead dir | control | 16th (advancePlayhead reads dir) | 1 | Y |
| variation | pattern variation | control | 16th | 1 | ? audit which rung it truly needs |
| slew (R/M) | smoothing | control | control (per-block smoothing) | 1 | Y -- inherently control-rate |
| scale mask / lastSelectedScale | quantise READ | control / event (scale change) | 16th (read at pitch pick) | 2 (Monsoon:563 + persistence) | ? D2 compute-on-read candidate (was a stale-derived bug) |
| scatter (CA) | pin remap | APERIODIC event | phrase-applied | 1 (remap-on-change, flicker fix) | Y -- event trigger, phrase apply |
| dice / scene advance | pattern/scene | APERIODIC event | phrase boundary | 1 | Y |
| per-output transpose (IT) | OUTPUT pitch | control CONTINUOUS but TIE-LATCHED | continuous, held across true tie | 1 (effectiveTranspose) | Y -- special case, see INTERTROPICAL_SPEC |
| lock-latched config | frozen params | USER-GATE (unlock) | until user unlock | 1 | Y |

## Optimisation candidates surfaced (distinct from correctness)
1. PPQN>24 = wasted work (only gs.tick uses sub-16th; everything else 16th-gated). Cap or opt-in.
2. getEffective* reads run EVERY block for values consumed only at the 16th. Cheap individually, but if
   the per-block cost shows in profiling, gate the reads behind sixteenthEdge (compute-on-consume). AUDIT
   with a profiler in Rack -- do NOT assume; some must stay per-block (continuous ones: mix/spread/
   transpose). The table's "Consumed-at" column says which are safe to defer to the 16th.
3. Continuous vs 16th split is already mostly right (mix/spread/transpose continuous; rest/accent/LOR at
   16th). The audit is to CONFIRM each row, catch the mis-rung ones, and gate the safe-to-defer reads.

## How to use
1. Wire the WriteLedger to the multi-writer rows (accentProb, restProb, scale) -- watch which conflict.
2. Audit each "Correct?" cell against the running build (esp. the ? and FIX rows).
3. For confirmed-16th-only reads that show in the profiler, gate behind sixteenthEdge.
4. Decide PPQN cap.
