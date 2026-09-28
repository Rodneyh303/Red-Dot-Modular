# Collapse 6 modes to 3 — decision note (Rodney)

STATUS: DECIDED IN PRINCIPLE, sleeping on it before committing. Largely ORTHOGONAL to the remaining
feature work, so it can be scheduled independently. Do the mode collapse FIRST of its cluster — the
other three changes below depend on it.

## The realisation
The six modes (A/B sequencer, C/D quantiser, E/F phase) were inherited from meloDICER's framing, with
phase added on top. meloDICER has discrete modes because it IS a discrete-mode module. Our engine is
not: `executeModeC/D` are already DEAD CODE and Mode C already runs Mode A's generated rhythm with
quantised pitch. So the six were a surface fiction the engine stopped honouring long ago.

What the engine actually has is TWO ORTHOGONAL AXES:
- **Timing origin** (3 states): CLOCK (you supply a pulse, it picks which pulses become notes),
  GATE (you supply note events), PHASE (you supply position, it derives the rest). A progression by
  how much timing authority you hand over: pulse -> event -> position. "Internal" is just clock with
  nothing patched (the clock jack normals to internal BPM).
- **Pitch origin** (continuous, PER VOICE): q-mix. 0 = generated, 1 = quantised to the incoming pitch.

Six named combinations pin these into a grid and throw away every hybrid. meloDICER never needed the
hybrids because it had no continuous per-voice pitch axis to combine with; we do.

## The change is ONE decision with FOUR reinforcing consequences
1. **3 modes** (clock / gate / phase) — a mode is a timing-origin choice, nothing more.
2. **Flip q-mix polarity** to 0 = generated, 1 = quantised. Required, because q-mix defaults to 0 and
   with three modes there is no "sequencer mode" guarding the default — 0 MUST mean generated or a
   fresh patch is silent. (Today high q-mix = generated, and it is only read in quantiser mode.)
3. **The flip makes QMIX honest** — "quantiser mix", so more q-mix = more quantiser. Today the label
   means the opposite.
4. **The two-axis story becomes true**, which is the claim no other module can make, and which makes
   "generative sequencer-quantiser" an accurate name rather than a hopeful one.
These are not four things to weigh independently; they point the same way, which is the sign the model
was right and the taxonomy was borrowed.

## Why it matters MORE in multi-Monsoon patches
Modes are per host and discrete, so a rig is a set of ROLE ASSIGNMENTS (this one sequences, that one
quantises) and the hybrids are unreachable. A continuum makes the rig a FIELD of partial relationships,
per voice. It also removes an arbitration problem: a shared CA spanning hosts in different modes had no
coherent meaning for its q-mix row; with a continuum there is nothing to arbitrate.

## The one real cost, and its answer
DISCOVERABILITY: "quantiser" leaves the mode column. Answered better than a mode entry did — a PANEL
SUBTITLE ("generative sequencer-quantiser") is read every glance, a mode position only when you are in
the column; and QMIX expands to "quantiser mix", which invites the question. Collapsing to 3 also frees
~27mm at the top-right where the modes were, which is where the subtitle can sit.

## Patched-detection rule (needed by the flip, independent of the collapse)
With 0 = generated safe-default, q-mix must be forced to 0 where there is nothing to quantise. Presence
comes from the PORT, never the value (0V is a valid pitch, not an absence):
```
if (!pitchIn.isConnected())          eff[v] = 0;      // nothing patched -> generate
else if (pitchIn.getChannels() == 1) eff[v] = knob;   // mono -> broadcast (VCV poly convention)
else                                 eff[v] = (v < pitchIn.getChannels()) ? knob : 0;
```
Poly voices beyond the source channel count GENERATE (do not fold/wrap) — a deliberate poly mapping
should not be silently wrapped. Confirm the pitch input is read per-channel (getPolyVoltage/getVoltage),
not a single getVoltage.

## Sequencing when built
Mode collapse FIRST (it determines the polarity flip, the panel subtitle, and how much the QMIX name
must carry). Then the panel changes fall out: 3-position mode column, subtitle, QMIX label/polarity.
Migration is free (pre-release): old C/D/F map to their clock source + q-mix at full.

## Relation to the rest of the roadmap
Orthogonal to: connection rework, seed-offset crab canon, panel-kit consistency audit, Keppel/MPE,
microtonal finishing. None of those depend on it and it depends on none of them. It is a self-contained
cluster, best done in one focused pass.
