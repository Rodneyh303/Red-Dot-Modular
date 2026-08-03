# Verify: external-gate drive applies full articulation (before the pitch claim goes public)

The PITCH doc (point 3) claims external GATE isn't just a clock -- an incoming external gate passes
through the engine's REST / LEGATO / ACCENT (mode B) articulation, so an arbitrary/random external
gate stream (e.g. Venom Rhythm Explorer) becomes raw material the generative layer shapes, rather than
a bare metronome. This must be TRUE AT THE CODE/RACK LEVEL before publishing it.

## What to confirm
1. External-gate-driven steps route through the SAME rest/legato/accent path as internal-clock steps
   -- not a reduced/bypassed path. Find the external-gate entry point in process() and confirm it
   feeds the same step-advance -> GateState articulation pipeline (rest roll, legato/slur commitment,
   accent mode B) that the internal clock does.
2. ACCENT MODE B specifically under external gate: mode B accent behaves correctly when the drive is
   external (not just internal). Mode B is the one to check -- easy for an alt-accent mode to be wired
   only on the internal path.
3. SANDS EXPANDERS under external gate: confirm the Sands shaping (probability, LOR, spread, direction)
   still applies when the sequence is externally gate-driven. i.e. external gate + Sands mods compose
   the same as internal clock + Sands mods. Check each Sands lane (REST/MEL/OCT/ACCENT) resolves through
   the resolver under external drive.
4. THE BIG-5 MODULATION under external gate: verify the five headline modulation targets (probability,
   length, offset, rotation (LOR), spread -- plus direction) all still MODULATE correctly while the
   sequence is running off an external gate rather than the internal clock. The risk is a mod that's
   sampled/applied on an internal-clock edge and silently no-ops or mis-times under external drive
   (a rate-boundary issue -- see RATE_DISCIPLINE_UNIFICATION: which edge samples the mod?).

## Why it matters
- Pitch honesty: don't claim "external structure composes with internal generation" if mode B or the
  Sands mods quietly drop out under external gate.
- It's also a likely-real bug surface: external gate is a less-travelled path than internal clock, so
  articulation/mod application there is exactly where an untested code path hides (the "compiles clean,
  returns a plausible wrong value" failure mode).

## Method
Enumerate the internal-clock step pipeline first (rest roll -> legato -> accent -> Sands resolve ->
big-5 mod apply), then walk the external-gate entry and confirm it joins that pipeline at the right
point, not a shortcut. Rack-test: drive Monsoon from an external irregular gate (Venom RE), toggle
each Sands mod + accent mode B, confirm audible/visible effect (Lantern makes this checkable). Only
then mark the pitch point 3 verified.
