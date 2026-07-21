# Lock Semantics — settled design

Status: DESIGN, awaiting implementation of Vermona-faithful LOCK. Documents settled
decisions from the design discussion (July 2026).

## 1. Background — two different locks

The Vermona meloDICER manual states:

> "All adjustments of the rhythm- and melody sections are instantly applied. The
> stochastic engine reacts as soon as you turn a knob or move a fader. Thus, meloDICER
> can be treated as an instrument. You can and should play the module's control elements.
> On the other hand you might want meloDICER to play a pattern while you already prepare
> new settings for rhythm and/or melody changes. Luckily we implemented lock-mode for
> that purpose. Press LOCK to activate lock-mode; the red LED goes on. As long as
> lock-mode is active all control elements of the rhythm and melody section as well as
> FIRST STEP and LAST STEP are now decoupled from the stochastic engine."

dot.modular's current LOCK does the OPPOSITE:
- **Current (wrong)**: material freezes (tables held, RNG not advanced, 23 tests pin
  this). Params stay LIVE over frozen material — you can ride probability knobs and LOR
  against a held pattern. Freeze(material) × live(params).
- **Vermona (intended)**: params LATCH (adjustments have no immediate impact until
  unlock drops them). Material keeps generating with pre-lock settings. Freeze(params)
  × live(material).

Same word, inverted axes. Verdict: implement Vermona semantics. Current behaviour is
valuable — "ride knobs over a frozen pattern" — but belongs under a different gesture
(e.g. the HOLD concept, not LOCK by name, not designed yet).

## 2. What latches under LOCK — the settled scope

**Lock-latches (no audible effect until unlock):**
- Big-5: NOTE_VALUE, VARIATION, LEGATO, REST, ACCENT
- Per-voice POLY_REST 1–15, POLY_ACCENT 1–15 (the poly extension of the rhythm section)
- Their CV modulation: mono/poly/global mod attenuverters AND incoming CV (Causeway,
  Junction, East var/leg CV). The Vermona manual explicitly decouples incoming MIDI CC
  (MEX3); the dot.modular analogue is external CV. Latching the knob but letting CV
  through would defeat the purpose. Mechanism: snapshot the RESOLVED value (knob+CV)
  at lock-on; engine reads the snapshot.
- Scale + range: SEMI 0–11 toggles, OCT LO/HI (melody section — preparing a scale
  change is half the point of lock)
- PATTERN_LENGTH + OFFSET (the Vermona FIRST/LAST STEP analogue, named in the manual)
- DNA LOR (all 18 + globals + interp) — rhythm/melody structure; audibly live under
  lock breaks "prepare silently"
- SPREAD + spread attenuverters. NOTE: spread currently freezes due to an implementation
  accident (it rewrites the final arrays; the lock gate protects the pool). Under
  Vermona lock, spread latches for the RIGHT reason: it is a generation setting,
  consistent with everything else in the section. The anomaly is dissolved.
- Future Change Alley pins: they shape generated output, so they latch.

**Stays live (transport/performance):**
- BPM, RUN, RESET, MODE, PHASE — clock and drive; Vermona locks neither
- MUTE — performance gesture, always immediate
- Lantern/display controls, themes

**Judgment calls:**
- TRANSPOSE: leans LIVE (pitch transposition is performance like transposing any
  sequence, not a generation setting). OPEN.
- Lane DIRECTION: leans LATCH (it is structure like rotation). OPEN.

## 3. Dice under LOCK — no change to existing behavior

Dice (all modes: live, trial, last-dice, last-trial) already queue under lock:
PatternEngine's three regeneration paths all begin `if (in.locked) return` — seeds stay
pending and fire at the first unlocked phrase boundary. test_lock_behaviour's 23
assertions pin this. The Vermona model confirms the behavior is correct:
- You can press dice while locked to ARM a re-draw
- The engine hears none of it until unlock
- At unlock the queued action fires on the first phrase boundary

No change needed. The only thing that changes on the dice side under the new semantics is
that param moves ALSO latch — previously they were live over frozen tables. Dice-queue-
under-lock was always right.

## 4. DNA Scramble — unimplemented, and heading for deprecation

**Finding (July 2026):** DNA scramble is UNREACHABLE from UI or CV. The trigger
infrastructure (scrambleParamTrig_*/scrambleInputTrig_* in MonsoonSandsManager.hpp,
the configButton and configInput registrations in MonsoonConfigurator) exists, and the
scramble METHODS (scrambleAll etc., which call rotateRhythm/rotateMelody with a random
offset) work correctly. But the processing loop that polls the triggers and calls the
methods was never wired. The buttons and gate inputs are dead.

**Disposition:** the DNA rotation context menu block in Monsoon's context menu (starting
at `menu->addChild(createSubmenuItem("DNA Rotation"...)`) is commented-out legacy code
from before Sands existed. The whole DNA scramble concept — random rotation to change
the pattern — is subsumed by the Sands LOR surface, which does the same thing with
per-lane control and visual feedback. SCHEDULED FOR REMOVAL:
- Remove the commented-out DNA rotation context menu block from Monsoon.cpp
- Remove or archive DNA_SCRAMBLE_* params and inputs (Monsoon.hpp, Configurator)
- Remove MonsoonSandsManager's scramble methods and trigger members (they are
  unreachable and the functionality is better served by Sands LOR)
- Remove SequencerEngine's scramble* methods (same reason)
Before removing: confirm no test or save-file migration path references these ids.
Param id removal is acceptable (back-compat not a priority, per established policy).

## 5. Implementation notes

The implementation flip is in the engine: currently `if (engine.locked) return` gates
MATERIAL regeneration. Under Vermona lock:
- At lock-on: snapshot the resolved param+CV values for the latched set into a parallel
  store. Engine reads the SNAPSHOT instead of the live params.
- During lock: snapshot is static; material keeps generating with snapshot values.
- At unlock: copy live values into the snapshot (or simply switch back to reading live);
  the next phrase boundary starts hearing the new settings.

The snapshot mechanism is analogous to how dice queuing works: latch-on takes a
consistent snapshot of the whole generation section in one atomic moment, preventing
partial-state lock (e.g. new scale but old probabilities) which would be worse than
either state alone. This also means a param change while locked updates the UI
(knobs visually move) but the snapshot stays frozen — same as how the Sands visual
shows pending state while locked in the Vermona model.
