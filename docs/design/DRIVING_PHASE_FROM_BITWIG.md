# Driving Monsoon's Mode E phase from Bitwig

Practical recipe, verified working in Bitwig (110 BPM, 4/4). Companion to
PHASE_ENGINE_AUDIT.md (the engine-side motion/reverse analysis) and
DAW_PARAM_AUTOMATION_ISSUE.md (why the param wasn't reaching the host until the
NUM_PARAMS migration dropped every module under the 1024-slot cap).

## What this drives

Monsoon Mode E takes its phase from CV1 when patched, and from the on-screen
PHASE knob (PHASE_PARAM) when CV1 is UNPATCHED. One full sweep of the knob
(0 -> 1) advances exactly one bar (scaled by phase.phaseInMax). Because
PhaseEngine reads phase MOTION rather than rising edges, whatever you map onto
PHASE_PARAM can run the sequence forward, backward, and at varying speed.

## The canonical route (transport-locked, scrub-capable)

1. Put **VCV Rack 2 Pro** in the Note Grid track's **Post FX** slot. This runs the
   whole Rack as an insert on the Grid device chain, so the Grid's own phase source
   sits in the same chain as the plugin it drives -- no separate modulator track.
2. In the Note Grid, build: **Phasor -> Ø Skew -> lfo out** (a modulation output).
   - The **Phasor** is transport-locked (ratio e.g. 1:8, 1:64 etc.), so timeline
     scrubbing keeps the sequence aligned -- this is what a free LFO can't do.
   - **Ø Skew** warps phase velocity across the bar: linear = constant rate,
     skewed = accelerate/decelerate. This is the variable-speed scrub control.
   - **Ø Reverse** (optional, before the out) runs the ramp backward; the engine
     follows it because it reads motion, so the sequence genuinely plays in reverse.
3. Map the modulator output onto Monsoon's **PHASE** knob via Bitwig's normal
   modulation routing (grab the modulator, drag to the PHASE param, set depth).
4. Leave Monsoon's **CV1 unpatched** inside Rack, so PHASE_PARAM is the active
   phase source.

## The quick route (works, not transport-locked)

A plain **sawtooth LFO** mapped to PHASE also drives the sequence -- a saw ramp IS
a phase ramp, mechanically identical to the phasor. The only thing you lose is
transport sync: a free-running LFO drifts against the bar, so scrubbing the
timeline won't stay aligned. Fine for playing and modulating; use the Phasor route
when you want scrub-follows-timeline.

## Plain automation lane

Automating PHASE_PARAM directly from a clip/automation lane also works (it's just a
host parameter now). Draw a rising ramp per bar for forward playback; draw it
however you like for tape-style scrub effects.

## Known seam caveat

Shortest-path phase delta reads 0.98 -> 0.02 as *forward* across the bar-line
(correct for a looping ramp, which is the intended use). So a deliberate BACKWARD
scrub that crosses the exact 0/1 seam aliases to forward for that one step. This is
inherent to any wrapped phase and is imperceptible in musical use. If unambiguous
full-range backward scrub is ever needed, the unbounded non-wrapping phase mode
sketched in PHASE_ENGINE_AUDIT.md (prevContPhase already tracked) is the fix.

## Why it took a param migration to get here

configParam auto-exports every param to the host, and there's no host-hide flag in
the Rack SDK. Monsoon-family modules sized their param array to
MonsoonIds::NUM_PARAMS (~1128), overflowing Bitwig's 1024-slot per-module pool, so
the host dropped ALL of that module's params -- including PHASE. Migrating the
internal-state ranges (LANE_DIR 96 + VARLEG 128 + MACRO 584 = 808 params) out of
params[] into Monsoon::editor fields brought NUM_PARAMS to 476, under the cap, and
the params register.
