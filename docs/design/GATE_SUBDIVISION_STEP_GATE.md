# Gate subdivision via STEP_GATE — the "make a deterministic rhythm stochastic" completion (Rodney)

STATUS: mostly ALREADY BUILT (step-legato exists); this records the framing and the one open question.

## The gap this closes
Gate mode can SKIP gates (rest) and JOIN them (legato), but not REPLACE a long gate with several short
ones — rest/legato are subtractive/combinative, never subdividing, because they only act on the events
you sent. Adding density needs a finer grid to articulate against.

## The realisation (Rodney)
Subdivision is NOT a new rhythm stream (rhythm q-mix / a 4th Philox stream — considered and rejected as
over-scoped). It is the rhythm-shaping lanes evaluated at a finer, EXTERNALLY-CLOCKED grid.

**STEP_GATE is an INPUT, not the existing generated step-legato — do not conflate them (Rodney).**
- *step-legato* is GENERATED: the engine decides legato at step boundaries; it is an OUTPUT behaviour.
- *STEP_GATE* is an external clock sent IN that sets the RESOLUTION at which the rhythm lanes are
  evaluated. Opposite direction of signal flow. The decision LOGIC exists (step-legato); the missing
  piece is the INPUT that drives it — a jack, param/enum entry, the plumbing to clock lane evaluation
  off it, and a kit anchor. Bounded, but a real build, not just config.
- **Name the input to avoid the near-homonym** (STEP_GATE vs step-legato read as the same feature on the
  panel). Prefer something like SUBDIV_IN / GRID_IN — "a resolution clock", not "the legato thing again".

**It drives REST and LEGATO both (Rodney), not legato alone** — they are the two lanes that shape the
rhythm of incoming events, so a resolution clock must govern both or it is only half a control:
- LEGATO at the fine grid: tie adjacent sub-slots or retrigger — density UP.
- REST at the fine grid: sound or drop a sub-slot — density DOWN (e.g. subdivide an 1/8 into two 1/16s,
  then rest silences one -> a 16th + 16th-rest figure the input never contained).
Possibly ACCENT too (so ratcheted sub-hits carry their own accent pattern). The main gate marks note
EVENTS; the input sets the RESOLUTION at which rest/legato(/accent) may subdivide and re-articulate.
Unpatched = step resolution, as today.

## Worked example
External sequencer plays a mix of 1/8 and 1/16 notes; send its 1/16 clock to STEP_GATE. The engine now
evaluates legato at 1/16, so an 1/8 note is "two 1/16 slots, tied", and the legato lane decides PER
VOICE whether they stay tied (sounds as the 1/8) or retrigger (becomes two 1/16s). A fixed 1/8 becomes a
probabilistic 1/8-or-two-16ths.

## Why it is the right form
- **Join OR split, both directions**, at the finer grain: tie 1/16s up into longer notes, or break a
  held note into 1/16 retriggers. The "can't turn one long gate into several short ones" limit dissolves
  once there is a finer grid — HONESTLY (via a real clock) rather than by inventing subdivisions.
- **No internal grid generation needed** in the patched case — the external stream IS the grid, so the
  sub-step-timing worry (could the engine schedule between step edges?) is already solved by step-legato.
- **Per-voice + correlated + reversible** is the part patching cannot touch: an external ratcheter
  subdivides every voice identically; here WHICH voices split moves with the correlation structure and
  reverses, because the decision is a seeded per-voice lane value. This is why it is an internal feature
  and not just "patch a burst generator" (which remains the answer for uncorrelated subdivision).

## Design fork: pitch per sub-hit (ratchet vs roll)
Rest/legato/accent clearly follow the fine grid. PITCH is the fork: when a gate subdivides, do the
sub-hits SHARE the parent note's pitch (a RATCHET — same note repeated) or draw FRESH pitch each sub-hit
(a ROLL/TRILL — different notes)? Default should be ratchet (subdividing should not silently re-roll
pitch every sub-hit); fresh-pitch is the opt-in. Likely a per-voice or per-lane choice, not global.
Pitch/octave/q-mix otherwise stay at the note-event grain.

## Open question (the other real build item)
Two parts: (1) does patching the input AUTOMATICALLY raise the evaluation resolution, or only within an
explicit legato span? Auto is more discoverable (patch a faster clock -> subdivision just happens). (2)
WHICH lanes evaluate at the fine grid — rest + legato at least, accent probably, pitch NOT (see fork
above). Confirm what the engine reads today: step-legato currently supplies inner boundaries WITHIN a
legato run; the extension is to let an external clock govern rest/legato(/accent) re-articulation of ANY
gate, not only inside a committed slur. Verify before building.

## Deferred / rejected alternatives (recorded so they are not re-proposed)
- **Rhythm q-mix / 4th stream** — over-scoped; doubles the Sands/panel/CA surface for a capability this
  covers, and muddies the clean "timing origin (mode) x pitch origin (q-mix)" two-axis story.
- **Internally-generated subdivision grid** (invent N even divisions within a gate with nothing patched)
  — the only genuinely new build; leave until a musical case needs subdivision with NOTHING clocked in.
- **Patched burst/ratchet generator in front of the gate input** — works today for UNcorrelated,
  non-reversible subdivision; on-brand (Monsoon perturbs what arrives). The internal route exists only
  for the correlated/reversible version.

## Phase mode: subdivision is PPQN, NOT STEP_GATE (Rodney)
Phase mode receives a POSITION (a phasor ramp), not events, so there is nothing to subdivide the
gate-mode way. Its subdivision resolution ALREADY EXISTS as PPQN: the phasor is quantised to the PPQN
grid, so phase mode already reads the pattern as discrete steps at a set resolution. So:
- **Do NOT wire STEP_GATE into phase mode.** It would be redundant with PPQN and would re-muddle the
  one-external-signal cleanliness that makes phase mode coherent (position in, nothing else).
- Phase mode's purpose is rhythm VARIATION BY MODULATING THE PHASOR — sweep/scrub position and the
  pattern reads out differently — not perturbation of an incoming rhythm.

Resolution mechanism per mode, each matching the kind of timing it receives:
- **Clock** — the clock pulse is the grid; PPQN divides it.
- **Gate** — STEP_GATE (external clock) sets the grid at which rest/legato perturb your events.
- **Phase** — PPQN (internal) sets the grid at which the phasor resolves to steps.

## If you want phasor-derived rhythm AS GATES: convert, then use gate mode
A phasor->gate converter (e.g. HetrickCV **Phasor to Gates** — compares the 0-10V ramp against
pulse-width thresholds, "smart" mode handling reverse/ping-pong phasors so gates stay even) turns
position into an EVENT stream. Chain phasor -> PhaseToGates -> Monsoon GATE mode to get
position-derived rhythm that you can THEN perturb with rest/legato/STEP_GATE. So the two approaches are
the two ends of the mode taxonomy meeting:
- **phasor -> phase mode**: position stays internal, pattern read at PPQN. Rhythm variation, no
  perturbation.
- **phasor -> PhaseToGates -> gate mode**: position externalised to gates, so the full gate-domain
  perturbation machinery applies.
This is also WHY phase mode should not gain STEP_GATE: if you want gate-domain subdivision of a
phasor-derived rhythm, you convert to gates first and use gate mode, where STEP_GATE already lives.
Phase staying position-only keeps that boundary clean. (On-brand: Monsoon does interesting things to
what other modules produce, rather than absorbing every function.)

## Two edge streams, two tie SCOPES — one model, two grids (Rodney)
The unifying insight: **STEP_GATE is to the main gate what step-legato already is to a legato span** —
a coarse container subdivided by a fine grid, with tie-or-re-articulate decided at each internal
boundary. Keep the behaviours as CONSISTENT as possible so a user learns one model.

### Playhead / edge tracking
- Plain gate mode: the playhead advances ONE STEP PER MAIN-GATE EDGE — the incoming gate is the step
  clock.
- With STEP_GATE patched: the playhead advances at the STEP_GATE RATE; the main gate becomes a SECOND
  edge stream layered on top. The engine tracks BOTH edge streams and reconciles them per cell:
    - STEP_GATE edges: advance the playhead, define the fine grid, and are WHERE rest/legato are
      evaluated.
    - Main-gate edges: mark where note events BEGIN and END in the incoming material.
  A main gate spanning N STEP_GATE cells is a note of length N cells; rest can drop a cell; legato at
  each internal boundary decides tie vs re-articulate.

### TWO tie scopes, BOTH preserved (the subtle requirement)
Legato is asked "tie or re-articulate?" at whichever boundaries exist, and there are two kinds:
- **Intra-gate legato** — tie across STEP_GATE cells WITHIN one main gate (the subdivision case; a long
  gate stays one note instead of re-articulating every fine cell). This is the step-legato analogue.
- **Inter-gate legato** — tie across a MAIN-GATE boundary, fusing two separate incoming gates into one
  sustained note. This is the slur-across-notes the module already does in plain gate mode, and it MUST
  STILL WORK with STEP_GATE patched.
Rule: at a STEP_GATE edge inside a gate, apply the intra-gate decision; at a main-gate edge, apply the
inter-gate decision. A note ENDS only when NEITHER says hold.
**TRAP to avoid:** STEP_GATE must NOT override inter-gate legato — i.e. do not force a re-articulation
at every main-gate edge just because it is also a step boundary. That would silently break slurs across
gates (a capability the module has today) — a regression hidden inside a new feature. Main-gate-boundary
legato is evaluated on its own terms whether or not a STEP_GATE edge coincides.

### Alignment / robustness (decide before building)
- **Off-grid main-gate edges.** The two streams are not phase-locked; a main-gate edge can land
  mid-cell (a swung or slightly-off 1/16). Rule needed: quantise note start/end to the nearest
  STEP_GATE edge (almost certainly wanted — it is why STEP_GATE was patched) vs honour the fractional
  cell. Pick quantise; make it explicit.
- **STEP_GATE stalls while main gates keep arriving.** Playhead freezes (STEP_GATE is the declared
  clock) vs falls back to main-gate edges. Freeze is cleaner; define the unpatch/repatch handover.
- **Display.** The Sands playhead shows the ACTIVE step, so under STEP_GATE it moves at the fine rate —
  it must read the SAME resolved step the engine uses (published state), not a separately computed
  position, or it is the display/engine race that bit the spread work.

### Test (header-level, no Rack)
Synthetic edge streams: (a) a main gate spanning N cells -> note length N, legato asked at N-1 internal
boundaries; (b) a legato tie expected to HOLD across a main-gate boundary AND one that re-articulates at
a STEP_GATE boundary inside a gate, IN THE SAME PATTERN — this is the test that catches STEP_GATE
stomping inter-gate legato; (c) an off-grid main-gate edge quantising to the nearest cell.

## Accent under subdivision: no step-accent output (Rodney)
Accent parallels rest/legato only PARTLY. Rest/legato act on note boundaries (whether/how long a note
sounds); accent is emphasis on a note already sounding, so at the fine grid its question is per-ONSET,
not per-cell.

**Rule (internal, cheap, keep it): ACCENT FOLLOWS THE ARTICULATION.** Accent is asked "emphasise?" only
at ONSETS — a fresh onset at a main-gate edge, or a fresh onset at a STEP_GATE re-articulation. A TIED
continuation is not an onset, so it gets no new accent decision (do not re-accent mid-slur). Wherever
legato decides re-articulate -> accent gets a decision; where legato decides tie -> it does not. So
accent rides on the onsets legato already computes; no separate accent clock or edge reconciliation.
This is just correct behaviour for the existing gate-masked ACCENT_OUTPUT when STEP_GATE subdivides.

**DECIDED: no STEP_ACCENT output.** Considered and declined.
- step-legato earns its jack because "these sub-hits are slurred, don't retrigger" is otherwise
  invisible to a downstream envelope/VCA. Step-accent does not clear that bar: its only new information
  is per-sub-hit emphasis on a ratchet, which is niche AND reachable downstream (envelope triggered by
  ACCENT_OUTPUT, or accent -> VCA) without Monsoon emitting it.
- Keeps the output count down on panels we are simplifying, and avoids the step-legato/step-accent mask
  interaction (a sub-hit can be both slurred and accented) for a marginal feature.
- If ever needed, per-sub-hit accent is DERIVABLE by logic: `STEP_GATE AND re-articulated AND accented`
  — the same masking move step-legato uses (STEP_LEGATO_GATE_OUTPUT = STEP_GATE masked to slurred
  notes). So it is a logic patch or a trivial masked output later, not new engine work now.

## Pitch under subdivision: ALL Sands lanes draw at STEP_GATE onsets (Rodney)
CORRECTION of an earlier muddle in discussion: a TIE is emergent from the PITCH DRAW, not decided by
the legato lane. Per the engine enum (SequencerEngine.hpp MonoDecision):
- `Tie`   = mono extended hold, **same pitch**
- `Legato`= mono slid to a **new pitch**, no retrigger
- `NewNote` = retriggered
The legato lane decides RETRIGGER-vs-not (NewNote vs the no-retrigger group). Within the no-retrigger
group, Tie-vs-Legato is simply whether the freshly drawn pitch EQUALS the held pitch. (What the engine
REPLACED was the old reactive legato ROLL at the joining onset — that is the retrigger axis, NOT the
Tie/Legato split, which is still pitch-equality.)

So clock mode already "makes the pitch decision at each step, including steps within legato; if it draws
the same pitch it is a tie." Apply the SAME at STEP_GATE onsets within a main gate:
- **ALL Sands lanes — melody, octave, and q-mix — draw at STEP_GATE onsets** (the fine grid), on exactly
  the clock-mode step rules. Same pitch as held -> Tie; new pitch -> Legato; legato lane may call
  NewNote (retrigger). Pitch, like accent, FOLLOWS THE ARTICULATION: a draw happens at each onset; a
  held/tied continuation is not a fresh onset.
- **Quantiser mode reads the incoming pitch CV at the SAME resolution** — sampled at each STEP_GATE onset
  — so a repeated incoming pitch naturally yields a Tie, and a changed one a Legato/NewNote.
- **q-mix per voice at those onsets**: a voice can be sequenced on one sub-hit and quantised on the next;
  the user selecting quantised melody via the q-mix knobs can therefore get a Tie when the quantiser
  reads the same CV at consecutive STEP_GATE onsets. This is the fine-grained form of the pitch-origin
  axis.

Consistency summary — at each STEP_GATE onset the engine does exactly what it does at a clock step:
rest/legato/accent evaluated, melody/octave/q-mix drawn, Tie/Legato emergent from pitch equality,
quantiser CV sampled. STEP_GATE just changes the RESOLUTION at which "a step" happens inside a main gate.

## GHOST PROTOCOL — notes OUTSIDE the gate envelope via the VARIATION lane (Rodney)
*Feature name: **Ghost Protocol**. Menu item stays literal ("Ghost notes"); code identifier plain
(ghostFill/ghostProb). "Ghost Protocol" is the manual/marketing name.*

STATUS: design agreed; not built. This is the ONE feature that deliberately opens the gate-mode ceiling
(output onsets outside the incoming gate union). Bounded and opt-in.

### Why it can exist cleanly
- **Variation is the one lane UNUSED in gate mode.** Repurpose it here rather than adding a lane —
  and semantically consistent: filling gaps IS "departing from the played pattern", which is what
  variation means. Name it **VARIATION/GHOST** so it reads as "the variation lane, whose gate-mode job
  is outside-gate notes".
- **Patching alone cannot do this** (retracted an earlier claim): a second clock-mode instrument
  overlays a whole part, not gap-fill; masking it by the inverted gate gives foreign scale, no
  correlation, separate voice frame, not reversible. Ghost-fill is musical only if the ghosts share the
  SAME generator's scale, correlation, voice frame and reversibility — so it must be internal.

### Mechanism (all on the SUBGATE grid — depends on STEP_GATE patched)
- **Placement**: outside-gate subgate cells (main gate LOW, playhead advancing — today's `forced Rest`
  branch in executeModeBSubdivided). At each such cell, roll the VARIATION/GHOST probability; if it
  fires, a ghost note is born.
- **Length**: ONE subgate cell. Note length is kept OUT — ghosts are sized by the subgate, not by
  NOTE_VALUE. Longer ghosts arise by LEGATO tying consecutive ghost cells — the same way in-gate note
  length emerges from ties. One sizing rule, not two.
- **Pitch**: generated from the melody/octave lanes as clock mode does (no incoming pitch in a gap).
  This is the one line-crossing (gate mode generating pitch), confined to gap cells and gated by
  variation.
- **Per-voice ghosting comes from variation/ghost's OWN per-voice probability (settled model).**
  Clarification of the engine reality: variation/ghost ALREADY has a per-voice probability that is
  poly and correlatable across voices — it simply is not read by anything today. Ghost-fill reads it.
  So ghost PLACEMENT is per-voice, and its CORRELATION setting is the whole spectrum:
    - correlation HIGH -> voices share (nearly) the same ghost probability -> ghost the same gaps ->
      "shared ghost rhythm" (what earlier drafts called Model A);
    - correlation LOW  -> independent per-voice ghost probability -> each voice ghosts different gaps
      (Model B);
    - between -> partially-shared. These are ENDPOINTS OF ONE CONTROL, not separate models — so there
      is NO A/B/C toggle and NO mono-placement model. Menu is simply **None / Ghost**.
  The AMOUNT is a GLOBAL control on Monsoon (per-voice-Straits-style control is NOT provided) — exactly
  the poly-LEGATO precedent (global control, per-voice probability), so it is consistent, not a
  compromise; a user who knows legato's behaviour knows ghost's. Per-voice Straits control, if ever
  wanted, is the same promotion legato would need, deferred on the same reasoning.
  Rest/legato/accent still additionally shape each ghost onset per voice with their existing algorithms.
  **Poly REST acts on ghost cells (this is what makes the hybrid CONTAIN Model A).** At high placement
  correlation the voices share the same ghost gaps; poly rest then thins them PER VOICE — i.e.
  shared-placement + poly-thinning, which is exactly Model A. So A is not a separate model, it is the
  high-correlation end WITH rest still acting. Placement correlation controls how ALIGNED the voices'
  ghost rhythms are; rest independently controls DENSITY on top. They are distinct (alignment vs
  density) and both act at every correlation setting — kept independent to avoid coupling "thin the
  ghosts" to "align the ghosts".
  **Ghost cells inherit the existing `restBeatsLegato` toggle** (RHYTHM_BEHAVIOUR_POLICIES.md; Monsoon
  context menu "Rest beats legato") for the rest-vs-slur conflict — a ghost that is both tied and rolls
  a rest is the SAME conflict as in-gate, resolved the SAME way. Do NOT add a parallel ghost arbitration
  control.
  Reversible is free (variation rides the Philox spine). Pitch is generated (melody/octave) as clock
  mode does. (Earlier drafts wrongly said variation is mono-placement + A/B/C models — corrected here.)
- **Legato is CONTINUOUS across the gate->gap boundary**: the legato decision applies at the next
  outside-gate subgate onset exactly as inside a gate — tie or re-articulate ghost cells, and govern
  whether a note carries past the gate edge into the first ghost cell. One rule, one grid, three regions
  (in-gate / boundary / gap) treated identically. The "one model" property holds even here.

### TWO subgate inputs — ratchet and ghost are independent capabilities (Rodney)
Confirmed in code: there is ONE Gate 3 today doing both jobs, and in-gate subdivision does NOT
self-disable at zero probability — SequencerEngine.cpp:551 `gsStep.triggerNote(...) // STEP:
re-strike, same pitch`, so a subdivided gate RE-STRIKES (ratchets) regardless of rest/legato rolls.
So ratcheting is inherent to subdividing, not gated by a lane value, and VG=0 only silences ghosts.
The four combinations are therefore only all reachable with TWO clocks:
- **SUBGATE_RATCHET** — clocks IN-GATE subdivision (ratchet/tie/rest/legato within gates). Unpatched =
  gates play whole, no ratcheting.
- **SUBGATE_GHOST** — clocks GHOST-fill in the gaps (additionally gated by VG). Unpatched = no ghosts.
- ratchets only = patch RATCHET; ghosts only = patch GHOST (+VG>0); both = patch both; neither = patch
  neither.
- **TWO inputs, NO normalling (Rodney, settled).** Each input does one job, explicitly: want ratchets,
  patch SUBGATE_RATCHET; want ghosts, patch SUBGATE_GHOST; want both, patch both; want neither, patch
  nothing (plain gate mode). No inference, no normalling — dropping the earlier mutual-normal scheme,
  which also removes its direction ambiguity. Consistent with the "render what's patched, don't be
  clever" stance. Reuses Monsoon's existing assignable gate jacks.
  Rationale for keeping TWO (the original "they're different behaviours" reason was ABSORBED by the
  envelope rule — one clock classified by in-gate-vs-gap already does both): what two still buys is
  (a) different RATES for ratchets vs ghosts, and (b) different SIGNAL TYPES simultaneously — ghosts
  want GATES (onset + duration), ratchets accept TRIGGERS (onset only). One input would force both
  roles onto one signal; two lets you e.g. gate-driven sustained ghosts WITH trigger-driven ratchets.

### Subgate signal types (settled)
- **SUBGATE_GHOST needs a GATE** — a ghost has a DURATION, and the only source of it is the subgate's
  own high time (rise = ghost onset, width = ghost length). A TRIGGER into ghost gives no length:
  render it as a SHORT FIXED BLIP (the trigger's own/default short length), do NOT invent duration by
  stretching to the next event and do NOT advance the playhead. So: gate -> sustained ghosts, trigger
  -> staccato ghosts. The signal type IS the articulation choice.
- **SUBGATE_RATCHET takes a TRIGGER or GATE** — onset-only suffices (ratchet cell length is bounded by
  the MAIN gate per the clip rule and by the next ratchet onset, NOT by the subgate's own width), so a
  trigger's rising edge is all it needs. (This "retrigger while the main gate is held" is also exactly
  the dual-wire abutment case strict legato ties on — same onset event, different feature lens.)

### Dependency and default
- **Requires the relevant subgate input patched** — no subgate grid, no placement clock or length quantum, so
  VARIATION/GHOST is inert (or falls back to plain variation) when STEP_GATE is unpatched. Clean, and
  discoverable: patch a subgate clock and turn up variation -> gaps start filling.
- Plain gate mode (no subgate, or variation at 0) keeps the ceiling intact — output onsets a subset of
  the incoming gates. Ghost-fill is strictly opt-in.

### Correlated by CA GROUP, not just per voice (Rodney)
Ghost placement, rest, legato and accent all run through the correlation structure, and CA defines
voice SUBGROUPS — so ratchets and ghosts organise BY SECTION: one choir ratchets together while
another stays sparse; the melody group fills gaps the bass group leaves; negative correlation makes
one group fill exactly the gaps another leaves (hocket/kotekan at the subgate grid, DERIVED from a
played pattern, not authored). The order/chaos field gains the sub-gate and gap dimensions at GROUP
level, which is where it is musical rather than busy.

### What ghost + ratchet buys musically
- **Correlated ornamentation of a PLAYED line**: ratchets add rolls/re-articulations ON the notes,
  ghosts add grace notes/fills BETWEEN them, and CA groups decide which voices ornament together —
  heterophonic ornamentation of an external line, the non-Western ensemble texture the correlation
  model was built for, now on the RHYTHMIC surface.
- **Two independent density axes** over the same input: ratchet density (within notes) and ghost
  density (between notes), separately clocked and separately correlated — busy-within/sparse-between or
  the reverse, each slidable. A 2-D rhythmic space no "amount of randomness" knob gives.
- **Interlocking** at the subgate grid via negative-correlation ghosts (one group fills another's gaps).
- **Gradual order<->chaos on your own material, reversibly** — the played onsets stay the skeleton;
  ratchets/ghosts elaborate around them and reverse back. The line is never lost.

Manual one-liner: *feed in any line and an ensemble ornaments it — ratchets within notes, ghosts
between them, organised into sections by Change Alley, sliding from faithful to florid and back.*

### Scope line for the manual
Gate mode reworks the ARTICULATION of your gates — chop and tie within and across gates, output onsets a
subset of the gate union — UNLESS variation/ghost + subgate is engaged, which is the one deliberate way
to add correlated, reversible notes in the gaps, on the subgate grid.


## OPEN (Rodney) — subgate applicability: within vs outside the main-gate envelope
The ratchet/ghost split is defined by MAIN-GATE-ENVELOPE MEMBERSHIP, and this must be settled BEFORE the
ratchet/ghost build, not after:
- subgate cell INSIDE a main gate (gate1High) -> RATCHET / subdivision (SUBGATE_RATCHET grid): chop /
  re-articulate the sounding note.
- subgate cell OUTSIDE (main gate low, a gap) -> GHOST (SUBGATE_GHOST grid): place ghost notes.
So "within vs outside the envelope" is the switch selecting which behaviour a subgate edge triggers, and
it maps onto the two subgate inputs.

**The hard part is cells that CROSS the envelope boundary** (must be handled — subgates are NOT assumed
aligned to main gates; wonky or steady both allowed):
- a subgate cell that STRADDLES a main-gate edge (starts inside, ends in the gap, or vice versa) — which
  grid owns it?
- main-gate rise/fall edges rarely align with subgate cell boundaries, so there is routinely a PARTIAL
  cell at each end of every gate. Define its treatment.
**Provisional rule (Rodney's first cut — NOT settled):** a subgate cell is GHOST if it STARTS between a
main-gate FALL and the next RISE, and RATCHET otherwise. I.e. classify by where the cell STARTS relative
to the main-gate envelope — decidable at the cell's onset, no lookahead. This follows gate mode cleanly.
### Straddle resolution (Rodney, initial — to confirm at build): MAIN GATE WINS AT ITS EDGES, asymmetrically
- **Falling edge — main gate wins (clip).** A cell that starts IN-gate but the main gate falls partway
  through it (a ratchet/subdivision) TERMINATES at the boundary — it abuts the gate's end, does not run
  on into the gap. A subdivision belongs to the note it subdivides, so it ends when the note ends
  (subject to the normal gap-legato rules for the note itself).
- **Rising edge — a GHOST may TIE THROUGH it.** A cell that starts in a gap (a ghost) reaching a main
  gate does NOT simply hard-cease: if the ghost is a slur-forward candidate it can LEGATO into the
  incoming note — so the output note is already sounding (from the ghost) and CONTINUES through the rise
  with no re-attack. **The OUTPUT gate is therefore allowed to START BEFORE the main gate's rising edge**
  (a ghost-led pickup/anticipation). This is fine: gate mode re-articulates the input, it does not
  slavishly reproduce main-gate timing — the main-gate rise AUTHORISES a note; legato decides whether it
  is a fresh attack or a continuation. If the ghost does NOT tie, it ceases at the boundary and the main
  note attacks fresh.
- **Decision reuse:** ghost-into-note uses the SAME legato handshake as note-to-note (ghost commits
  slurForward; the arriving main-gate note decides tie-in per the FALSE checkpoint logic). One model — a
  boundary is a boundary; ghost->note is governed like note->note.
- **Degenerate guard:** a cell whose end coincides exactly with a boundary must collapse cleanly, not
  emit a zero-/1-sample sliver.
- **Robustness:** clipping at main-gate edges makes subgate-grid REGULARITY irrelevant at boundaries —
  wonky or steady subgates both honour main-gate timing exactly (except the deliberate ghost-tie-through
  early start). This is the property we wanted; clip delivers it for free.
**Tie-vs-re-articulate at the rise is NOT a default — it is the GHOST'S OWN legato roll (Rodney).** A
ghost is just a note and rolls slurForward like any note:
- ghost COMMITTED slurForward -> ties through the rise -> note continues, no re-attack, output-gate-start
  precedes the main gate (pickup);
- ghost did NOT commit -> ceases at the boundary -> main note attacks FRESH (hard re-articulate).
So it is EMERGENT from the ghost's legato probability, per voice, correlated, reversible — exactly like
note-to-note legato. High legato -> more ghost pickups; low legato -> ghosts stay separate. No special
case: a ghost decides tying-OUT like every note, the main-gate note decides tying-IN like every arriver.
One model to the corner. Nothing left open here.


## PLAYHEAD ADVANCE for ghosts (Rodney, settled)
**A ghost onset ALWAYS advances the playhead, regardless of length (trigger or gate).** The advance IS
the point: the playhead landing on a step is how the ghost gets its REST / LEGATO / ACCENT / PITCH lane
data. A ghost without an advance would be a note with no data source. So there is NO gate-vs-trigger
distinction to make here — a trigger is just the shortest possible gate, both are ghosts, both need a
step's data, both advance and draw. (This also dissolves the "how do we not advance on a trigger"
question — we DO advance on it; a staccato trigger-ghost still draws a full step, it just sounds short.)

**Playhead model (whole engine):** the playhead is EDGE-DRIVEN — it advances ON an onset (main-gate
rise, ratchet rise, ghost rise) and draws that step; it NEVER free-runs between onsets (no motion in
gaps). A ghost edge is simply one more onset type that advances it — fully consistent with the existing
model, not new free-running behaviour.

**The one proposed EXCEPTION (still to-mull):** the FALSE-mode "tie into rest" refinement would advance
the playhead on a FALLING edge into a rest checkpoint — the sole case that advances on something other
than a rising onset. Flagged as the single deliberate violation of "advance on onset only"; weigh it
carefully if built.


## Ghost PLACEMENT is per-voice, CORRELATED to the mono reference (Rodney — corrects earlier notes)
Ghost candidate placement is NOT a binary mono-vs-per-voice choice. It goes through the CORRELATION
matrix like everything else: the per-voice variation/ghost probability is correlated TO THE MONO
variation reference, correlation in [-1, +1]:
- **+100%** -> per-voice locked to mono -> all voices ghost the SAME cells -> shared ghost rhythm.
- **0%** -> independent -> each voice ghosts its own cells.
- **-100%** -> anti-correlated -> voices ghost where mono does NOT -> interlocking / complement.
Uses the SAME graded-correlation machinery (copula / spread / follow-CA) as pitch. The per-voice
variation probability (poly + correlatable, currently unread) is what ghost-fill reads, correlated to
the mono variation strand.

**Playhead stays MONO / reference anchored:** the mono variation strand defines the canonical candidate
timeline (one playhead). Each voice's ACTUAL ghost cells are a CORRELATED perturbation of that reference
— at +1 they coincide with mono (one effective timeline); below that they diverge per voice, but as a
STRUCTURED correlated field, not independent chaos. Mono anchors it; correlation controls departure.
This is spread-follows-CA applied to ghost placement: mono = reference, per-voice = correlated
deviation, correlation (+1/0/-1) = the control. Supersedes the earlier "placement must be mono" and
"placement could be per-voice (binary)" notes — it is per-voice CORRELATED, which is neither.

(Rest/legato/accent still additionally shape each surviving ghost per voice, as before — that is on top
of the correlated placement, not instead of it.)


## INVARIANT (Rodney) — gate behaviour is MODE-AGNOSTIC: generator and quantiser share EXACT gate code
The sequencer-quantiser unification REQUIRES that gates/subgates/ghosts/legato/rest/accent behave
IDENTICALLY whether pitch comes from the GENERATOR or from the QUANTISED INPUT. q-mix (the pitch-origin
axis) is the ONLY thing that differs between the two — it changes WHERE PITCH IS READ FROM, nothing
else. If the gate behaviour diverged between "generator mode" and "quantiser mode" they would be two
instruments again, not one instrument with a pitch axis.
**Rule:** gate/subgate/ghost/legato code MUST be a single mode-agnostic path — NO "if quantiser mode"
special cases in the gate logic. Same code, same result; only the pitch SOURCE is switched by q-mix
(per voice, correlated). This is what makes the unification real rather than cosmetic, and it is the
kind of invariant that silently rots (someone adds a quantiser-mode special case) — so it must be
TESTED: assert gate/rest/legato/accent/subgate/ghost output is bit-identical for the same gate input
regardless of pitch-origin, across the q-mix range.
(Same principle as the Sands consolidation: one parameterised implementation beats parallel ones;
here q-mix is the parameter wrongly tempting a structural split, as the correlation matrix was for Sands.)


## BOUNDARY-COINCIDENCE rule (Rodney) — subgate edge landing EXACTLY on a main-gate edge
Completes the straddle logic (straddle = cell CROSSES a boundary; this = cell edge lands ON the same
sample as a main-gate edge). One principle: **the main gate OWNS its edge samples** (a rise is a
note-START, a fall is a note-END — neither sample is "gap"). Ghost = gap-only, ratchet = in-gate-only,
applied to the coincident sample:

| subgate event | at main-gate RISING edge (same sample) | at main-gate FALLING edge (same sample) |
|---|---|---|
| **Ghost** | **IGNORE** — rise is a note-start, not a gap; ghost would collide with the main onset | **IGNORE** — fall is the note-end transition; not a gap |
| **Ratchet** | **INCLUDE** — the rise is the gate's START, so an aligned ratchet is the valid FIRST in-gate cell | **IGNORE** — no gate left to subdivide (clip rule: in-gate cells terminate at the fall) |

- **Ghost resumes >=1 sample clear of the boundary:** a ghost onset ONE SAMPLE after the main gate
  falls -> INCLUDE (now genuinely in the gap). Same <=1-sample reasoning as the legato work — one
  sample clear is "in the gap" and below the threshold of anything mattering.
- **Degenerate guard** still applies: coincidence resolution must not emit a zero-length cell.

Consistent with: the envelope rule (ghost=gap, ratchet=in-gate) applied at the exact-coincidence sample;
the clip rule (in-gate cells end at the fall); and the playhead rule (a ghost that is ignored does not
advance the playhead, because it is not emitted).
