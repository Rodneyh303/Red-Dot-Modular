# Per-voice deterministic octave offset — orchestrate voices to registers (bass/lead/pad)

## The Melodicer criticism, and why it only PARTLY applies to us
A known criticism of Melodicer: all notes forced into the SAME octave range — can't orchestrate voices
across registers. For us it only partly applies, because Sands lanes are per-voice:
- **MELODY lane — per-voice:** each voice draws a different PITCH within the octave.
- **OCTAVE lane — per-voice (PL_OCTAVE, slewedPolyOctave[v], polyOctaveSource[v]):** each voice draws a
  different random OCTAVE within the slider range. So voices ALREADY get STOCHASTIC octave variation.

## The real gap: DETERMINISTIC register assignment (not stochastic)
What's missing is not more randomness — it's the ability to PREDETERMINE voices to roles: voice 1 =
bass, voice 2 = lead, voice 3 = pad, etc. That is an ORCHESTRATION / VOICING choice, made once — it
should NOT be stochastic (you don't want the bass randomly jumping to lead register; that's chaos, not
orchestration). The stochastic octave lane was the wrong tool for it.

## The fix: a per-voice DETERMINISTIC octave OFFSET on Sands
- A fixed per-voice value (−N..+N octaves) added to the voice's pitch at the realization/output stage.
  Deterministic, SET per voice (not a draw).
- **Composes with the existing stochastic OCTAVE lane:** the offset sets each voice's BASE REGISTER
  (bass/mid/lead/pad); the octave lane then adds generative octave MOVEMENT WITHIN that register. So:
  deterministic register role (offset) + stochastic movement within it (lane).
- Net: orchestrate voices across registers deterministically AND keep generative octave life within each
  register — exactly how you voice an ensemble (assign roles by register, let each role breathe).

## Why deterministic is RIGHT (matches the two intents)
- Register ASSIGNMENT = deterministic (compositional choice, set once).
- Register MOVEMENT = stochastic (generative wander within the assigned register — the existing lane).
Different intents -> different tools. The offset does the job the stochastic lane shouldn't.

## UI (compact — fits the HP constraints)
Deterministic set-once value per voice -> does NOT need a full lane of bars. A compact per-voice
octave-offset control, or set via the voice tabs / context menu (it's set, not performed) -> low
panel-space cost.

## Placement, Causeway, and Intertropical consolidation (Rodney)
- **Home: Straits, NOT crowded Sands.** It's deterministic, per-voice, SET-ONCE (a value, not a
  probability bar) — belongs with the per-voice controls on Straits, not the Sands lane grid (already
  crowded, see the range-lane fit). Likely a Straits LANE-EXPANDER: a deterministic per-voice
  octave-offset lane in the expander chain alongside the probability lanes — same mechanism, different
  payload (set offsets, not draws). Reuses the lane-expander infra being completed in the Straits
  refactor; natural to add as part of that lane set.
- **May earn a DEFAULT / BASE panel spot:** if orchestrating voices to registers (bass/lead/pad) proves
  commonly needed, it may be core enough for the base/default panel (always present) rather than an
  opt-in expander. Open (expander vs base-panel), pending how central it proves.
- **CAUSEWAY = CV-modulatable version (Rodney):** a Causeway octave-offset lane = CV-MODULATABLE per-voice
  register assignment -> voices MIGRATE between registers over time (a voice rising bass->lead over a
  phrase). Static assignment (Straits, set-once) + dynamic migration (Causeway, CV) = the Straits/Causeway
  knob-vs-CV duality applied to register. Orchestration automation.
- **Then REMOVE output semitone/octave offset from INTERTROPICAL (consolidation):** Intertropical
  currently carries deterministic output semitone/octave offsetting — it was a WORKAROUND for the missing
  per-voice octave. Once the proper per-voice octave offset exists (Straits), Intertropical no longer
  needs it: move the offsetting responsibility to its proper home and REMOVE it from Intertropical.
  Each module does its own job (Intertropical = arrangement/routing; octave offset = Straits per-voice
  register). Pays down the band-aid; clarifies responsibilities. (Do after the octave offset lands.)

## Result vs Melodicer
Melodicer: global octave, all voices same register (can't orchestrate). Monsoon: per-voice stochastic
octave lane (voices wander) + per-voice deterministic octave OFFSET (assign bass/lead/pad) -> BOTH
orchestrated register roles AND generative movement within them. Strictly better; directly answers the
criticism with a small, well-motivated addition.
