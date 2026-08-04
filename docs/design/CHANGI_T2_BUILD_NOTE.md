# Changi T2 build note -- process() double-advance bug + data source decision

## The bug Claude Code found (latent, pre-existing, T2 would compound)
MonsoonOutputGenerator.cpp calls gs.process() and gsStep.process() on each poly voice TWICE per
sample when both Straits and Changi T1 are present:
- Straits block (~line 73-74): voices[i].gs.process() + gsStep.process()
- Changi T1 block (~line 103-104): same two calls again

GateState::process() advances a PulseGenerator (1ms retrigger) -- NOT idempotent within a sample;
each call consumes sampleTime. So today the poly retrigger pulse advances 2x per sample when
Straits + Changi T1 are both present. Inaudible (one sample-period of error) but architecturally
wrong. Mono path avoids this correctly -- precomputes once in generateOutputs() and both blocks
reuse cached gateV/stepGateV/stepLegatoV. A naive T2 block calling process() a third time = 3x.
Fix the foundations before adding T2.

## Option A (CHOSEN): hoist per-voice process() calls, fan out to all consumers
Precompute each poly voice gs.process() + gsStep.process() ONCE at the top of the poly block (like
mono already does), cache the derived voltages in arrays, then Straits / T1 / T2 each read the cache
without calling process() again.

WHY CORRECT:
- Fixes the existing double-advance bug (one advance per sample, as intended). The "alteration" is
  an improvement; inaudible and correct.
- T2 gets raw computed values from engine state -- the most faithful source.
- Consistent with how mono is already handled. The hoist is minimal: only the process() calls and
  derived voltage values; not the whole output-generation logic.
- Maintain the existing Straits guard (if cachedPolyVoiceExpander). If Straits absent, poly
  computation skips, T2 outputs nothing -- correct (Straits is a poly prerequisite).
- Read MonsoonOutputGenerator.cpp IN FULL before touching. Make the change minimal.

## Option B (REJECTED): T2 reads Straits' output jacks (getVoltage)
Superficially consistent with the "one level up" pattern (T3 reads IT jacks; T2 reads Straits
jacks). BUT: the T2/T3 symmetry argument doesn't hold. T3 reads IT's jacks because Intertropical
has done REAL TRANSFORMATION (slot->output routing, effectiveTranspose) -- T3 must read the
post-routing values. T2's data hasn't been transformed by any intermediate; the step-gate state is
computed directly by the engine. Option B also leaves the existing double-advance bug in place and
builds T2 on broken foundations. Rejected.

## Straits as prerequisite (context, doesn't change the decision)
Straits is required for poly operation and will be the ONLY way to set poly voice count once the
context-menu poly setting is removed (deferred -- see MASTER_PLAN). So "T2 outputs nothing without
Straits" is the normal expander dependency contract, not a hidden gotcha. This narrows the practical
gap between A and B (the "silent failure" concern diminishes) but doesn't change the core argument:
Option A fixes a real bug; Option B leaves it. Option A is correct regardless.

## Files to touch (Claude Code)
- src/dsp/managers/MonsoonOutputGenerator.cpp: hoist per-voice gs.process()+gsStep.process() to
  top of poly block; cache derived voltages; Straits+T1 blocks read cache (no more process() calls).
- src/MonsoonChangiT2Expander.hpp/.cpp: new module, reads cached values from OutputGenerator.
  Passive jack-holder like T1 (host writes into it from the cached voltage arrays).
- plugin.json: add ChangiT2 slug.
- res/panels/: new panel gen_changi_t2.py, airport-terminal theme, ~16-20HP.
