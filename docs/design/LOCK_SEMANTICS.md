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
  Junction, East var/leg CV). PRECEDENT (meloDICER manual, verbatim pointer note):
  "If you have a MEX3 module connected to your meloDICER, lock-mode also applies for
  incoming MIDI Control Change messages." MIDI CC is meloDICER's remote-modulation path,
  so Vermona freezes remote writes along with the panel controls. NOTE the exact scope of
  the quoted precedent: control elements of the rhythm/melody sections + FIRST/LAST STEP,
  and MIDI CC. The manual does NOT address meloDICER's own CV IN 1/2 under lock, so
  latching external CV is OUR EXTENSION of the precedent (justified by the coherence
  argument below), not a quotation of it. Latching the knob but letting CV
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

## 6. Exhaustive lock-scope checklist  every module + expander (July 2026)

Extends 2's settled scope to the full CURRENT registered set (LOCK_SEMANTICS.md predates
Change Alley V2 and the full expander lineup). Governing rule, restated to correct an earlier
draft: lock mode DISCONNECTS the generation-section shaping controls from the stochastic
engine. Shaping controls normally react to probability LIVE (meloDICER "play the module"); LOCK
inverts that  they LATCH (snapshot resolved knob+CV at lock-on; engine reads the snapshot;
UI still moves; commit on unlock at the next phrase boundary). The reactive DISPLAY (bar graph,
arcs, lantern, playhead) stays live throughout  you watch the frozen pattern play. Modulation
latches WITH its control (snapshot resolved value), never independently.

Legend: LATCH = obeys lock (frozen till unlock) - LIVE = never obeys - OPEN = judgment call.

### Monsoon (core)
- Big-5 sliders NOTE_VALUE/VARIATION/LEGATO/REST/ACCENT ......... LATCH
- PATTERN_LENGTH (master length), OFFSET .......................... LATCH (Vermona FIRST/LAST STEP)
- DNA LOR (18 + globals + interp) ................................. LATCH
- SPREAD + spread attenuverters ................................... LATCH
- SEMI 011 scale toggles, OCT LO/HI range ........................ LATCH
- Big-5 CV mod (mono/poly/global mod attenuverters + incoming CV) . LATCH (with its control)
- TRANSPOSE ....................................................... OPEN (leans LIVE  performance pitch)
- Lane DIRECTION .................................................. OPEN (leans LATCH  structural like rotation)
- BPM/RUN/RESET/MODE/PHASE (clock+drive) ......................... LIVE
- MUTE ............................................................ LIVE
- Themes/lantern/display controls ................................. LIVE
- Dice (all modes) ................................................ queues under lock (3, unchanged)

### Sands editors (all now store-backed  de-param complete for Macro+Mono)
- Macro: globalLor, globalSpread, globalAtten, globalTap, globalDir, macroSend .... LATCH
- Mono:  lorBase[mono], spread[mono], monoAtten, monoLaneDir ...................... LATCH
- Mono/Macro owner (monoOwner / topology ownership) .............................. OPEN (leans LIVE  structural routing, not a pattern value; ownership flip during lock probably applies immediately, see 7)
- Grid probability edits (SandsVisualEditorV4) .................................... LATCH
- East (when de-parammed): LOR/spread/atten/dir equivalents ....................... LATCH (inherits)

### Expanders  probability-SHAPING (LATCH)
- Change Alley (V2) pin matrix + transforms ....... LATCH (reshapes correlation = generation setting; 2 "future Change Alley pins latch" now current)
- Causeway (poly rhythm CV) ....................... LATCH (poly REST/ACCENT modulation = rhythm section)
- Junction (CV routing into big-5) ................ LATCH (remote modulation path, like MEX3 MIDI CC precedent)
- Raffles / Interchange (DNA/LOR-shaping) ......... LATCH (confirm each shapes generation; if pure routing, revisit)
- Shophouse scale mask (Conservation) ............. LATCH the scale EDIT (preparing a scale change is half the point); the Conservation guide/enforce TOGGLE itself is orthogonal (SHOPHOUSE_SPEC) but the mask VALUES latch like SEMI toggles

### Expanders  performance / transport / display (LIVE)
- Changi (airport-themed  confirm role: if transport/vis, LIVE) . LIVE (confirm)
- Temasek (deprecated on change-alley-v2 branch) .................. n/a
- Lantern (note-output visualiser) ............................... LIVE (pure display)
- Any MUTE/performance expander controls ......................... LIVE

### Modulation (the disconnected-under-lock half, per Rodney)
- ALL CV modulation of a latched control latches WITH it: snapshot resolved (knob+CV) at
  lock-on. Applies to Causeway, Junction, East var/leg CV, global/mono/poly mod attenuverters,
  and any incoming CV that writes a generation-section value. Latching the knob but passing CV
  would defeat lock  established in 2, restated here as a category rule.
- CV into a LIVE target (clock, transpose-if-live, mute) stays LIVE  it modulates a control
  that itself doesn't latch.

## 7. Lock SCOPE as a future context-menu choice (Rodney)

The checklist above is the DEFAULT (whole generation section latches). A context-menu "lock
scope" could later let the user choose narrower latching, e.g.:
- Whole module (default  everything in the LATCH set).
- Section only (e.g. latch melody prep but keep rhythm live, or vice versa).
- Per-lane (latch selected lanes; matches per-lane owner/direction granularity).
This interacts with the per-lane-vs-global open question in LOCK_MODE_PLAN.md. Default to
whole-module for the first build; scope menu is a later refinement, not v1.

### The read-vs-map principle (emerged from resolving direction/owner/transpose)
A clean test for the OPEN base rulings turned out to be: does the control shape or READ the
probability arrays (LATCH), or MAP the finished output (LIVE)?
- LOR = the window onto the arrays; DIRECTION = the traversal of them; OWNER = selects WHICH
  LOR/mod is used. All three are "how we read the arrays" -> LATCH, same class as LOR.
- TRANSPOSE = post-generation output mapping (shifts finished pitch, changes nothing generated)
  -> LIVE, same side as the clock.
- A/B MIX = blends two already-rolled draws (LockedA + mix*(CandB-A)) PRE-spread, PRE-pins
  (PatternEngine.hpp:102,209). It is upstream generation, MORE fundamental than spread, so it
  LATCHES with spread. The "feels like a live crossfade" instinct was checked against the
  pipeline and did NOT hold -- A/B is not a downstream output blend. Latching it is what keeps
  the DJ-cue promise whole: the review's point is you can move ANYTHING under lock and hear no
  change; a live A/B would be the one control that breaks that.

### Still OPEN (decide before/at build)
- NONE. All base rulings resolved (July 2026). No structural opens, no play-test flags remain.

### The config/event split within Change Alley (manual pin edit vs scatter gate)
Change Alley's pin state has TWO input paths, and they land in different lock categories 
which is correct, and they sequence themselves with no conflict:
- **Manual pin edit** (drag a pin to a SPECIFIC correlation) = CONFIG. You set a deliberate
  value. It correlates the draws POST A/B-mix, PRE-spread (PatternEngine.hpp:102) -- upstream
  array-shaping. LATCH: held silently, commits at UNLOCK. A live manual drag would be audible
  under lock (you'd hear the correlation move), breaking the DJ-cue promise -- same reason A/B
  and spread latch.
- **Scatter/trigger gate** (PERMUTES the pins, Fisher-Yates -- a reshuffle you did NOT specify)
  = EVENT. Like dice: you can't hand-set the result, only arm it. QUEUE: fires at the next
  phrase boundary.
- **Both pending at unlock  NO conflict, natural time ordering.** Latch releases at unlock;
  queue releases at the next phrase boundary; unlock always precedes that boundary. So the
  manual edit commits FIRST (at unlock), then the queued scatter permutes THAT (at the
  boundary). The ordering is not a rule we impose -- it falls out of the two release timings.
  A timing difference, not a contest.

Why the dice analogy has a limit (Rodney): dice has no manual equivalent -- you can't hand-set
the draw dice will roll, so "event queues" is its only behaviour. Change Alley DOES let you
manually set correlations, so it has BOTH a config path (latch) and an event path (queue). That
is not an inconsistency: latch and queue are BOTH "silent under lock," differing only in when
they release, and that difference gives free, musically-sensible sequencing (your deliberate
correlation first, the reshuffle on top of it).

RESOLVED (July 2026): Changi out of lock scope (pure output). Interchange = core CV (note/oct),
latches with Big-5. Raffles: dice/queued gates QUEUE, slew folds into QUEUE (sampled at phrase
boundary), A/B mix LATCH (pre-spread generation blend -- corrected from an earlier LIVE call
once the pipeline showed it is upstream of spread). Change Alley: transform knobs + poly CV
latch (play-test), trigger/scatter gates QUEUE. Transpose LIVE, direction/owner LATCH.

## 8. Two tiers: Vermona-faithful core vs dot.modular extended surface (Rodney, July 2026)

meloDICER's lock scope = its whole generation surface (rhythm/melody + FIRST/LAST STEP +
MEX3 CC). For meloDICER "lock the generation section" and "lock everything" coincide because
that IS the whole module. dot.modular has surfaces meloDICER has no analogue for  the Sands
expanders, Change Alley, Shophouse  so those two statements have COME APART. Including the
extended surface in lock scope goes significantly beyond meloDICER: you could prepare a
correlation remap + a scale change + a per-voice LOR shift silently and commit them together.
That is a different instrument gesture, not just "Vermona plus extras."

So the design is TWO TIERS, not one flat latch-list:

**Tier 1  Vermona-faithful core (proven, ships first):**
Big-5, PATTERN_LENGTH/OFFSET, DNA LOR, SPREAD (+attens), scale/range, their CV mod. This is
what lock has always meant; it's the well-understood, must-work baseline. 6's core rows.

**Tier 2  extended surface (dot.modular-only, OPT-IN per expander):**
Sands editors, Change Alley pins, Shophouse scale mask, Causeway/Junction/etc. Structurally
these shape generation, so latching is the DEFENSIBLE DEFAULT  but whether each SHOULD latch
is a claim about how the instrument PLAYS, not just what it writes. A Change Alley pin remap
might feel better LIVE (a punch-in performance gesture) than latched. That is a play-tested
decision, discovered by using it, NOT settled from the code. (Same structural-correct vs
musically-right distinction as the VAR/LEG exclusion  structural defensibility is the
starting hypothesis, not the verdict.)

**Consequence for build + UI:**
- Ship Tier 1 as the core lock. Do NOT build Tier-2 latching speculatively.
- Each extended surface opts into lock scope as it proves it WANTS to be lockable in play.
- The context-menu "lock scope" (7) is the natural home for the extras: core always latches;
  the extended surfaces are toggleable inclusions the user (and we, during design) can flip.
- Let it evolve. The value of documenting the tier now is that there's a SLOT for each extended
  surface to join lock scope when play justifies it, rather than a retrofit  not a commitment
  to latch them all on day one.

## 9. Consolidated control-type  module  lock table (the single reference)

Supersedes the tier boundary in 8 where it wrongly placed LOR/spread/scale in the Vermona
core. CORRECTED: meloDICER's lock precedent (1, manual quote) covers ONLY the rhythm/melody
section controls (Big-5) + FIRST/LAST STEP. LOR, spread, and scale are dot.modular's OWN
DNA/generative layer  meloDICER has no analogue  so they are EXTENDED surface, not core.

Tier: **V** = Vermona-faithful core (the manual's actual scope). **X** = dot.modular extended
Tier: **V** = Vermona-faithful core, WHERE "core" is by FUNCTION not by whether meloDICER
shipped it -- a poly rhythm/melody probability is the same KIND of control as the mono Big-5,
so Straits poly REST/ACCENT knobs are core. **X** = dot.modular extended (a control TYPE
meloDICER has no analogue for: LOR, spread, scale, correlation, ownership).

CV is its own column, not a "+CV" annotation: the CV path is a distinct control surface with
its own lock cell, even though its lock behaviour is BOUND to its target (a control's CV latches
iff the control latches; snapshot the resolved knob+CV at lock-on). "" in CV = control has no
CV path. Lock cells: LATCH / LIVE / QUEUE / OPEN / .

QUEUE is a THIRD lock category (not latch, not live): an event TRIGGER can't hold a value, so
under lock it defers -- arms now, fires at the first unlocked phrase boundary. Established by
dice (3); extends to Raffles queued gates and Alley scatter/trigger gates. A param that is only
SAMPLED at that boundary (Raffles slew) folds into queue behaviour automatically -- it's read
when the queued redraw fires, so it needs no independent ruling.

| Module | Control | Type | Tier | Base lock | CV lock |
|---|---|---|---|---|---|
| Monsoon | Big-5 sliders (NOTE_VALUE/VAR/LEG/REST/ACCENT) | probability | V | LATCH | LATCH |
| Monsoon | POLY_REST 115, POLY_ACCENT 115 | probability (poly) | V | LATCH | LATCH |
| Straits | per-voice REST + ACCENT knobs (poly Big-5 analog) | probability (poly) | V | LATCH | LATCH (Causeway) |
| Interchange | modulates NOTE_VALUE + octave sliders | core CV (note/oct) | V | (is CV) | LATCH (with Big-5) |
| Change Alley | pin matrix + transform knobs (grain/leader/step) | correlation config (shapes draws PRE-spread) | X | LATCH | LATCH (poly CV in, with base) |
| Change Alley | trigger gates (domain/codomain, 4 scatter-back) | correlation EVENT | X | QUEUE (dice precedent, 3) |  |
| Raffles | dice-roll / queued gates | regeneration event | X | QUEUE (dice precedent, 3) |  |
| Raffles | slew | sampled at phrase boundary | X | folds into QUEUE (read at the queued redraw, not an independent axis) |  |
| Monsoon | PATTERN_LENGTH, OFFSET | first/last step | V | LATCH |  |
| Monsoon | DNA LOR (len/off/rot, 18 + globals + interp) | generation structure | X | LATCH | LATCH |
| Monsoon | SPREAD + spread attenuverters | generation setting | X | LATCH | LATCH |
| Monsoon/Raffles | A/B MIX + its mod | generation blend (pre-spread) | X | LATCH | LATCH |
| Monsoon | SEMI 011 scale toggles, OCT LO/HI range | scale/range | X | LATCH |  |
| Monsoon | TRANSPOSE | output mapping (post-generation) | X | LIVE | LIVE |
| Monsoon | Lane DIRECTION | array READ (how the probability arrays are read, like LOR) | X | LATCH | LATCH |
| Monsoon | BPM/RUN/RESET/MODE/PHASE | clock/drive | (both) | LIVE | LIVE |
| Monsoon | MUTE | performance | (both) | LIVE |  |
| Monsoon | Dice (all modes) | regeneration | (both) | queues under lock | queues |
| Monsoon | Themes, lantern, display toggles | display | (both) | LIVE |  |
| Sands Macro | globalLor / globalSpread / globalAtten / globalTap | generation | X | LATCH | LATCH |
| Sands Macro | globalDir | array READ (like LOR) | X | LATCH | LATCH |
| Sands Macro | macroSend (per-voice mix-in) | generation routing | X | LATCH |  |
| Sands Mono | lorBase[mono] / spread[mono] / monoAtten | generation | X | LATCH | LATCH |
| Sands Mono | monoLaneDir | array READ (like LOR) | X | LATCH | LATCH |
| Sands Mono/Macro | owner (monoOwner / topology) | selects WHICH LOR+mod (LOR-class) | X | LATCH | LATCH |
| Sands East | LOR/spread/atten/dir (when de-parammed) | generation | X | LATCH (inherits) | LATCH |
| Sands (all) | grid probability edits | probability | X | LATCH |  |
| Causeway | poly rhythm CV | rhythm modulation | V | (is CV) | LATCH |
| Junction | CV routing into Big-5 | remote modulation | V | (is CV) | LATCH |
| Shophouse | scale mask VALUES | scale | X | LATCH (like SEMI) |  |
| Shophouse | Conservation guide/enforce TOGGLE | orthogonal mode | X | separate (SHOPHOUSE_SPEC) |  |
| Changi | pure OUTPUT expander | output | n/a | out of lock scope (no shaping/regeneration control) |  |
| Lantern | note-output visualiser | display | X | LIVE |  |
| Temasek | deprecated (change-alley-v2) | n/a | n/a | n/a | n/a |

Notes:
- The V core is defined by FUNCTION, not by meloDICER's parts list: mono Big-5 + poly Big-5
  (Straits per-voice REST/ACCENT, Monsoon POLY_REST/ACCENT) + length/offset + their CV. A poly
  rhythm probability is the same KIND of control as a mono one, so it's core even though
  meloDICER had no poly. Causeway/Junction are core-tier because they ARE the CV of core
  controls. Everything that is a NEW control TYPE (LOR, spread, scale, correlation, ownership)
  is X.
- CV is its own column: a control's CV latches IFF the control latches (snapshot resolved
  knob+CV). "OPEN (with base)" means the CV inherits whatever the base ruling turns out to be.
  "(is CV)" marks modules that ARE a CV surface (Causeway/Junction) rather than having one.
- OPEN rows are the unresolved rulings (transpose, direction, owner) gathered in one place.
- "confirm" rows (Raffles/Interchange/Changi) need their actual role checked against the split.
