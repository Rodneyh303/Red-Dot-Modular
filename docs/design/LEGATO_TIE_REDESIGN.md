# Legato / Tie redesign — leading-edge held-gate model (design note)

Status: **design capture — proposed engine behaviour change, not yet implemented.**
Inspired by Lantern making the current legato/tie behaviour visible, and by checking the
real Vermona meloDICER. This is a CORE ENGINE semantic change (affects every pattern), so
it's captured for deliberate decision, not done ad hoc.

---

## Current model (what Monsoon does today)

In `SequencerEngine::executeStep`, legato/tie is decided **at the joining step** — reactively,
each step rolls `r_legato_tie < legatoProb`, and IF it wins:
- same pitch as last + previous note held → **Tie** (`extendHold`)
- different pitch → **Legato** (`slideNote`)
- else → **NewNote** (`triggerNote`)

So the decision is made at the **trailing edge / next step**, and the *first* note of a
slur does not know it will be slurred. The connection is decided when the second note
arrives, based on whether the pitch changed.

## Real meloDICER model (what we believe the hardware does — CONFIRM)

Legato is decided **up front, at the note's LEADING edge**. A note that rolls legato commits
to **holding its gate open at its trailing edge** — it decides, when it starts, that it will
slur into whatever comes next. The held gate is a property of the *first* note, not a
decision made at the join.

> The user is "pretty sure" this is the better model and matches the hardware, but it should
> be verified against the meloDICER manual / careful listening before migrating, since it
> changes how every existing pattern sounds.

---

## Proposed model (leading-edge held gate) + simplification

### Key simplification: tie and legato are ONE concept musically
We do NOT need to distinguish strict tie from legato as separate musical modes. Both are the
same physical thing: **a note holds its gate open into the next note (a slur).** Whether the
next pitch is the same (sounds like a tie) or different (sounds like legato) is just *what
happened to the pitch*, not a different mechanism. The gate behaviour is identical.

So the three-way (NewNote / Tie / Legato) collapses toward:
- **Struck note** — fresh gate (retrigger).
- **Held/slurred note** — gate stays open across the boundary; pitch either stays (tie-like)
  or moves (legato-like), but it's one mechanism.

### But keep an internal binary for POLY CONSISTENCY
Even though the *player* doesn't need tie-vs-legato, the engine still needs a coarse
**retrigger vs hold** binary, because poly voices follow mono's gate TYPE (retrigger or hold)
to stay consistent. That binary is exactly what the leading-edge model produces directly
("did this note hold its gate?"), so poly consistency is preserved — it just keys off
"held?" instead of the finer tie/legato label.

### The two phases
1. **Leading edge (note onset):** roll legato-intent. If it wins AND the note is
   legato-eligible, the note commits to holding its gate open past its natural end.
2. **Trailing edge / next step:** the held gate means the next note does NOT retrigger —
   pitch glides/jumps to whatever is drawn. (Same pitch = tie-like, different = legato-like;
   one mechanism.)

### Rule: legato-eligibility excludes triplet & 1/32
A note can only *initiate* legato if it's long enough to reach its neighbour. A fractional-
length note (triplet, 1/32) **ends before the next step**, leaving a gap the held gate can't
bridge. So: **legato-eligible ⟺ note length ≥ 1 step** (excludes triplet, 1/32 and any
sub-step length). Sub-step notes always strike fresh; they cannot start a slur.

### Rule: a following REST cancels legato
If a note commits to holding its gate but the next step is a **rest**, there is nothing to
slur into. Open question on exact behaviour (see below), but the intent: no slur across a
rest. Either the rest cancels the hold (gate closes at the note's natural end) or the
legato-intent suppresses that step's rest — MUST be decided (leaning: rest wins, gate closes;
a slur into silence is meaningless).

---

## Phrase-boundary behaviour — long/legato notes, FORWARD and REVERSE

Monsoon can be phase-driven (Mode E): the phase ramp *is* the playhead position, and a
backward-moving phase makes the sequencer step its playhead **backward** (`PhaseEngine`
`reverse` flag; edges fire in either direction). So "reverse" is not a mode toggle — it's
whatever direction the phase input is moving, and can change mid-pattern. Long notes and
held (legato) gates crossing the phrase boundary need defined behaviour in BOTH directions.

### Forward (phase increasing)
- **Long note spanning the boundary:** a note whose length extends past the last step of the
  phrase. Current behaviour holds the gate via `holdRemain`/`gatePulseRemain` across the wrap
  (Lantern's heldOut caret marks this). Proposed: unchanged — the note's gate countdown is in
  pulses and simply continues across the phrase wrap; the next phrase's step 0 shows a
  heldIn continuation.
- **Legato at the boundary:** if the last note of the phrase rolled legato-intent, it holds
  its gate into the FIRST note of the (next) phrase — the slur crosses the loop point. This
  is musical (phrases connect). Eligibility still applies (last note must be ≥ 1 step).

### Reverse (phase decreasing)
This is the subtle case and MUST be defined, because the leading-edge model assumes "the note
knows up front it will slur into *the next note*" — but in reverse, "next" is the *previous*
step in pattern order.
- **Long note in reverse:** the note-length countdown is direction-agnostic (pulses elapsed),
  but the note *starts* at its leading edge in playback-time. In reverse, a note that visually
  occupies steps N..N+k is entered at step N+k moving toward N. Decision: does the note's
  gate/length re-evaluate from the reverse leading edge, or is playback a strict time-reversal
  of the forward render? LEANING: playback is phase-position-driven, so the note that *would*
  sound at the current phase position sounds — length is whatever was decided when that note's
  leading edge was last crossed. Reverse should REPLAY the committed gate states, not re-roll.
- **Legato in reverse:** here's the crux. Forward, note A commits to slur into note B (the
  next). Played in reverse (B then A), does the slur still connect A→B, or does it now read as
  B holding into A? Two options:
  - (a) **Slurs are directional (belong to the note that rolled them):** A→B slur replays as
    a held gate between A and B regardless of direction — i.e. the *connection* is preserved,
    just traversed backward. Consistent, replay-faithful.
  - (b) **Slurs re-decide by playback direction:** in reverse, the leading edge is B, so B
    rolls its own legato-intent to slur into A. Fresh randomness in reverse.
  RECOMMENDATION: **(a) — preserve committed connections, replay faithfully.** Re-rolling in
  reverse (b) makes reverse playback non-deterministic vs the forward pass and breaks the
  "note knows up front" contract. Reverse should be a faithful traversal of what was
  committed, not a new generative pass. (This mirrors how we handled the visual editors:
  the committed pattern state is authoritative; direction just traverses it.)
- **Boundary in reverse:** crossing the phrase start going backward wraps to the phrase end.
  A held gate at step 0 moving backward continues into the previous phrase's last step —
  symmetric with the forward boundary case.

### Determinism requirement (both directions)
Whatever we choose, forward-then-reverse over the same phrase should be **consistent** — a
note's committed length and slur-connections are decided once (at the leading edge, forward)
and REPLAYED under reverse, not re-rolled. Re-rolling on reverse would make the playhead's
direction change the notes, which is surprising and breaks phase-scrub/DAW-automation use
(scrubbing the phase back and forth would mutate the pattern). This is the single most
important constraint for phase-mode support.

---

## Impact on Lantern (the display)

If tie/legato collapse to one "held/slurred" concept, Lantern's colours simplify:
- Struck note (fresh gate) = one colour.
- Held/slurred-into note (gate stayed open) = a shade/variant.
- Pitch change is already visible (bar at a different pitch/row-label), so we don't need
  separate violet/teal for tie-vs-legato — "held" is the distinction that matters.
This is arguably CLEARER to read than the current three-colour scheme. Revisit
LANTERN_SPEC.md colour section if the engine model changes.

Lantern also needs phase-mode support (forward/reverse playhead) — same approach as the
visual editors (traverse committed state; the phase line + heldIn/heldOut carets already
model boundary holds; reverse just moves the playhead the other way over the same cells).

---

## Open questions to settle before implementing

1. **Confirm the hardware** actually decides legato at the leading edge (user "pretty sure").
2. **Rest-follows-legato:** rest wins (gate closes) vs legato suppresses the rest. (Lean: rest wins.)
3. **Legato-eligibility threshold:** confirm "≥ 1 step" is the right cutoff (excludes triplet
   1.333? — 1.333 ≥ 1, so a 1/8T IS ≥1 step... need to define: is it "≥1 step" or "not a
   fractional/sub-16th VALUE"? The user said exclude triplet AND 1/32 — 1/32 is 0.5 steps
   (<1) but 1/8T is 1.333 (>1). So the rule is NOT purely length-based. Likely: exclude note
   VALUES whose grid doesn't align (triplets) + sub-step (1/32). RESOLVE THIS — it's the one
   spot where "length ≥ 1 step" and "exclude triplets" disagree.)
4. **Poly consistency:** confirm poly voices key off the "held?" binary correctly under the
   new model (they follow mono's retrigger-vs-hold).
5. **Migration:** existing patches will sound different. Version/opt-in, or clean change?

> NOTE (item 3 is important): the user's two eligibility criteria — "≥1 step" implied by
> "reach the neighbour", and "exclude triplet and 1/32" — are not the same set. A 1/8 triplet
> is 1.333 steps (≥1) yet the user wants it excluded. So eligibility is about note-VALUE
> alignment to the step grid, not raw length. Needs an explicit definition before coding.

---

## Phrase-boundary RESET (meloDICER behaviour — simplifies everything above)

The real meloDICER **hard-resets at the phrase boundary**: all gates stop, and any
in-progress legato or long note is CUT at the boundary. The next phrase starts fresh.

This REMOVES the hardest parts of the phrase-boundary section above rather than adding to
them:

- **No cross-boundary holds** — a long/legato note cannot continue into the next phrase; its
  gate stops at the boundary. So the "forward long note spanning the boundary" case is just a
  truncation, not a continuation.
- **Reverse mode simplifies** — the thorny "does a held gate/slur reconnect the other way in
  reverse?" question evaporates: there is no slur crossing the boundary to reconnect. The
  boundary is a hard barrier in BOTH directions.
- **Determinism-under-direction** still matters WITHIN a phrase (replay committed state, don't
  re-roll on reverse), but the boundary itself is a clean cut either way — implementation is
  just "force all gates off / reset gate state at the boundary crossing."

**Implication for legato eligibility:** a note near the end of a phrase that would extend past
the boundary is simply cut at the boundary (gate stops). Whether it should even be *allowed* to
roll legato when it can't complete before the boundary is an open question — leaning: allow it,
it just gets truncated (matches "gates stop"), consistent with the hardware.

**Implication for Lantern:** the heldOut (continues-past-step-16) and heldIn (continues-from-
previous-phrase) carets become mostly moot — notes are TRUNCATED at the phrase edge, they don't
continue across it. Lantern's boundary rendering simplifies to "draw notes clipped at the phrase
boundary," no cross-boundary caret logic. (A long note whose gate is cut early by the boundary
should render as ending AT the boundary, which is truthful.) Update LANTERN_SPEC.md accordingly
when the reset lands.

---

## Phrase-boundary behaviour — CORRECTED after checking code + searching for evidence

Earlier this doc sketched a "hard-reset at the phrase boundary" (gates stop, in-progress
legato/long notes CUT). Two findings correct that:

1. **The current implementation does NOT reset at the boundary.** In SequencerEngine.cpp the
   wrap is detected (`wrapped = advancePlayhead(dir)`) but `wrapped` is NEVER used to reset
   holdRemain/gateHeld — the hold state ticks down purely by the clock (`gs.tick(...)`),
   independent of the wrap. So a long/legato note in progress just before the boundary
   CONTINUES PAST the boundary. The wrap flag is display/consumer-only, not gate logic.

2. **No evidence found for the hardware doing a hard reset.** Searched the Vermona meloDICER
   manual (v1.1, v1.4) + multiple reviews (SoundOnSound, Waveform, Juno, ModularGrid). They
   confirm legato = GATE HELD HIGH across the note transition (no new gate edge) — supporting
   the leading-edge-legato model — but NONE address what happens to a held gate at the pattern
   LOOP point. It appears undocumented; only an empirical scope test on real hardware (or a
   Vermona-owner forum answer) could confirm it.

**DECISION: keep current behaviour — long/legato notes CONTINUE across the phrase boundary.**
Not changing the boundary/reset behaviour on an unconfirmed hypothesis. This is a defensible
(arguably more natural) model: a held note stays held; the loop point is just a position marker.
Consequences of NOT doing the reset:
- Reverse mode still needs its cross-boundary handling (the reset would have simplified it, but
  we're not taking that simplification on faith).
- Lantern's heldIn/heldOut carets remain MEANINGFUL (notes genuinely cross the boundary), so
  keep them — do NOT remove them as the earlier note suggested.

The LEGATO change itself (leading-edge legato holds gate high) is evidence-supported and
proceeds independently of the boundary question.

---

## FUTURE (much later) — context-menu boundary-wrap behaviour toggle

Possible context-menu option: rhythm boundary wrap = CONTINUE (current: held/long/legato notes
carry across the phrase boundary) vs INTERRUPT (cut held notes at the boundary — the "reset"
behaviour we investigated but did NOT adopt by default). Would make the continue-vs-reset
question a user choice rather than a hardcoded decision. The interrupt path is the reset logic
sketched earlier (use the `wrapped` flag to clear holdRemain/gateHeld at the boundary). Defer —
noted so the option isn't forgotten. If added, reverse mode + Lantern carets must handle both.

---

## SCOPE — leading-edge legato implementation plan

### Current model (reactive / decided at the JOINING step)
In SequencerEngine::executeStep (mono; poly slaves to it via lastStepResult):
- When a note's hold expires (holdRemain < 1 && gatePulseRemain == 0), the cascade runs FOR
  THE NEXT note and reads r_legato_tie = legatoRandom[getLegatoStep()] AT THE JOIN:
    legatoProb>=0.999 → LegatoMax (always slide)
    else rest roll → Rest (or MidNote if a tail blocks it — "tail takes priority over rest")
    else r_legato_tie < legatoProb → same pitch = Tie (extendHold); diff pitch = Legato (slideNote)
    else → NewNote (triggerNote, fresh gate)
- So the JOIN's roll decides, reactively, whether note N+1 connects back to N. The gate-hold
  choice is made AFTER N has already played. Gate commitment lives in GateState:
  triggerNote/slideNote/slideMax/extendHold (all set holdRemain / gate).

### Target model (leading-edge / decided at the note's OWN onset)
"A note designated legato holds its gate high into the next note." The legato character is
committed when the note STARTS, not at the join:
- At note N's onset, roll legato → if legato, N commits to keeping its gate OPEN through to N+1
  (hold-forward), rather than N+1 deciding at the join to connect backward.
- Musically identical in the simple case, but: (a) the governing random draw is N's onset draw,
  not the join draw; (b) the commitment is known at N's start (matters for look-ahead-free
  display like Lantern — the onset already knows its legato-ness); (c) it generalises tie vs
  legato as ONE "hold/slur" concept (gate stays open; same pitch reads tie-like, diff pitch
  legato-like) while keeping the internal retrigger-vs-hold binary for poly consistency.

### The precise mechanism change
- Move the legato roll from "read at the join to decide backward-connection" to "read at N's
  onset to set a HOLD-FORWARD flag on N". Concretely: when triggerNote fires for N, also decide
  (from N's legato draw) whether N is a "slur note" that holds its gate past its nominal length
  into the next onset. Store that on GateState (e.g. a `holdForward`/`slur` flag or by extending
  holdRemain to bridge to the next step at N's onset).
- At the next step, if the previous note was flagged hold-forward: the new note SLIDES (pitch
  changes, gate stays high) instead of retriggering — same visible result as today's Legato,
  but the DECISION already happened at N's onset.
- Tie vs Legato remains a same-pitch-vs-different-pitch distinction at the actual join, but
  gated by N's onset slur-flag rather than a fresh join roll.

### Interrupts this must respect (fixed constraints — see the exceptions section)
A hold-forward note still gets cut by: RESET (engine.gs.reset, at boundary), MUTE, a rest roll
ON A NON-HELD step (but "tail takes priority over rest" precedence must be preserved — a
slur/tail outranks a structural rest), poly slaving to mono gate. Phrase boundary does NOT
interrupt (continue-across, current behaviour). Dice/lock do NOT interrupt (side-effect; keep).

### Tricky bits / risks
- LegatoMax (legatoProb>=0.999) currently ALWAYS slides at the join — in the leading-edge model
  it means "every note is a slur note" (always hold-forward). Verify equivalence.
- Fractional tails (triplets/1/32): holdRemain<1 with gatePulseRemain>0 is the MidNote guard.
  The hold-forward flag must compose with sub-step tails without double-holding or cutting them
  (the exact bug class the current gateSecRemain/gatePulseRemain checks fix).
- Poly consistency: executePolyVoice reads lastStepResult.decision (Tie/Legato/MidNote) to slave
  poly gates. The leading-edge flag must still surface the same decision enum so poly slaving is
  unchanged — keep the internal retrigger-vs-hold binary.
- accent inheritance: monoStarting logic (accent decided on NewNote or a legato shift from dead)
  must still fire once per real onset, not per held step.

### Stepped plan (own branch, cross-checked; engine-core change, needs build + ear)
1. Add a hold-forward/slur flag to GateState, set at onset from N's legato draw (no behaviour
   change yet — flag computed but the join still decides). Cross-check the flag matches the
   join decision in the simple case.
2. Switch the join to CONSUME the onset flag instead of rolling fresh; keep the same
   MonoDecision enum outputs so poly slaving + Lantern are unaffected.
3. Verify interrupts (rest-precedence, RESET, mute, poly slave) and fractional tails unchanged.
4. Confirm LegatoMax and legatoProb sweep behave; build + ear test; then remove the old join roll.
Test suite: extend the mono decision tests (the 347-test suite / PatternEngine tests) to assert
onset-committed legato, same-pitch tie vs diff-pitch legato, tail precedence, and poly slaving.

---

## OPEN DECISION — slur vs rest conflict (leading-edge only)

Leading-edge commitment creates a conflict the reactive model never had: note N commits at its
OWN onset to hold its gate forward (slur), but N+1's REST roll happens later. When N+1 rolls a
rest, which wins — N's prior slur commitment, or N+1's rest?

Existing precedent in the code (executeStep): a fractional NOTE TAIL already outranks a
structural rest — "note tail takes priority over pattern structural rest" (gateHeld=true,
MidNote). So "hold beats rest" exists — but that was written for PHYSICAL tails (a triplet
finishing its real sub-step length), which is a different case from an OPTIONAL slur reach.

Two options for the slur case:
- **A: hold wins, rest suppressed.** N slurs into N+1 regardless of N+1's rest. Legato is a
  strong commitment. Risk: high rest + high legato → legato stampedes over rests.
- **B: rest wins, slur cancelled.** N+1's rest cancels the slur; N ends at nominal length, N+1
  is silent. "You can't slur into silence." Matches the doc's earlier "rest-follows cancels
  slur" instinct.

LEANING B, with a key distinction that keeps the existing tail behaviour intact:
  **A note must finish its OWN length (TAIL > rest — physical, unchanged), but its OPTIONAL
  forward reach yields to a rest (REST > slur — structural).**
So: fractional tails still outrank rests (no change to current behaviour); a leading-edge slur
does NOT outrank a rest (rest cancels the slur, note ends at nominal length). This answers the
conflict without touching the tail-precedence code and matches "can't slur into silence."

Settle A vs B before implementing plan step 2 (it's ~a one-line difference in the join
consumption logic but a real difference in feel — test both by ear if unsure).

---

## STEP 1 FINDING (important) — the legato roll is ALREADY at the onset

Implementing step 1 surfaced a subtlety that corrects the plan. The current model's legato roll
does NOT happen "at the join, separately from the onset" — the join IS the next note's onset:
- executeStep's MidNote guard (holdRemain>=1 || gatePulseRemain>0) returns early WHILE a note
  holds, so the legato cascade only runs on steps where a note is STARTING.
- At that step, r_legato = legatoRandom[getLegatoStep()] is read — this step is simultaneously
  "the join between N and N+1" AND "N+1's onset."

So the real difference between current and leading-edge is NOT "which random draw / which step"
— it's the COMMITMENT DIRECTION and WHICH NOTE OWNS the decision:
- CURRENT: at N+1's onset, decide "does N+1 connect BACKWARD to N?" (N+1 slides from N's gate).
- LEADING-EDGE: at N's onset, decide "will N reach FORWARD and hold into N+1?" — using N's OWN
  legato draw (N's onset index), which is a DIFFERENT draw than N+1's onset draw.

Consequences:
- The two models use DIFFERENT random draws (N's onset vs N+1's onset), so a leading-edge flag
  will NOT always match the current decision — "assert they match" (the naive step 1) is wrong.
- The slur-vs-rest conflict is INHERENT to leading-edge and ABSENT from current: current decides
  legato at N+1's onset, the SAME step as N+1's rest roll (no ordering conflict). Leading-edge
  commits N's reach BEFORE N+1's rest exists → the conflict. This confirms why that open
  decision matters and is new.

### Revised STEP 1 (safe, zero behaviour change) — INSTRUMENT, don't switch
Add a hold-forward/slur flag to GateState, computed at N's onset from N's OWN legato draw, wired
but UNUSED by the gate logic (the join still decides exactly as today). Also record, per starting
step, what the leading-edge flag WOULD dictate vs what the current model DID — to characterize
divergence before committing. No output change. This gives real data on how different the models
are, and lets step 2 flip the decision source deliberately with the slur/rest policy in hand.

---

## STEP 1 CONSTRAINT — only grid-aligned notes can LEAD a legato (leading-edge model)

User caught this: in the leading-edge model a note commits AT ITS ONSET to hold its gate forward
into the next note. But a note that ends BETWEEN 1/16 grid edges cannot cleanly do so — the next
note's onset is on a grid edge, and an off-grid-ending note doesn't end there.

Note lengths in 1/16-step units (noteValueSteps = fraction×16, from NoteValues.hpp):
  1/1=16, 1/2=8, 1/4=4, 1/8=2, 1/16=1  → INTEGER (land on grid edge) — CAN lead a legato
  1/4T=2.667, 1/8T=1.333, 1/32=0.5      → FRACTIONAL (end off-grid) — CANNOT lead a legato

Why, precisely (differs by note):
- 1/32 (0.5 steps) closes BEFORE the next edge (at 1.0) → gate already down at the next onset →
  nothing to hold forward. (In current model: wasHeld=false at that edge.)
- Triplets (1.333, 2.667) end MID-WAY between edges → still holding at the intervening edge
  (sustain/MidNote through it), but their actual END is off-grid, so the next onset can't cleanly
  connect a forward hold.

CONSTRAINT for the onset slur-forward flag: it may be set at N's onset ONLY IF N's note length is
integer-step (nvIdx ∈ {1/1,1/2,1/4,1/8,1/16}). Fractional-length notes (1/4T, 1/8T, 1/32) can
NEVER be a legato LEAD — the flag stays false for them regardless of the legato draw.
(They can still be tie/legato TARGETS / sustain via the existing wasHeld||hadTail path — this
constraint is specifically about LEADING a forward-hold in the leading-edge commitment.)

Implementation: gate the flag on an "integer step length" predicate, e.g.
  float ns = noteValueSteps(nvIdx); bool canLead = (ns == std::floor(ns));
Also: in leading-edge we must draw BOTH a legato roll AND an appropriate (grid-aligned) note
length to START a legato — the note-value draw and legato draw jointly gate the lead.

---

## Fractional-as-destination = the DOTTED-NOTE generator (why the rule is load-bearing)

The rule "a fractional note can't LEAD but CAN be a legato/tie destination" is not just a
constraint — it's the mechanism that produces DOTTED (and other compound) note lengths from the
base note-value set, via extendHold summing the lengths:
  1/16 (1.0) + 1/32 (0.5), same pitch, tie → holdRemain 1.5 = DOTTED 1/16   (user's example)
  1/8  (2.0) + 1/16 (1.0)  tie            → 3.0 = DOTTED 1/8
  1/4  (4.0) + 1/8  (2.0)  tie            → 6.0 = DOTTED 1/4
extendHold does `holdRemain += add; armGate(holdRemain)` — one continuous gate to the exact
(sub-step) end. Engine comment "rule 2" already documents fractional destinations as exact
(e.g. 1/8→1/32 = 2.5 steps).

Leading-edge preserves this: note1 (grid, slurForward=true) → note2 (fractional, same pitch)
arrives as a Tie via prevSlur → extendHold sums → dotted note; note2's own slurForward=false
(can't lead) so the chain ends cleanly after the dotted note. So the fractional-destination
behaviour must be protected — dotted rhythms depend on it; it's load-bearing, not incidental.

### Compound lengths, full picture — clean dotted AND unusual hybrids

The tie/extendHold sum yields TWO classes of compound note, both by design (engine already lists
some as exact — 1/8→1/4T=4.667, 1/4→1/8T=5.333):

1. STRAIGHT + STRAIGHT → clean dotted/compound:
   1/16+1/32=1.5 (dotted 1/16), 1/8+1/16=3.0 (dotted 1/8), 1/4+1/8=6.0 (dotted 1/4).

2. STRAIGHT + TRIPLET (or TRIPLET + TRIPLET) → "unusual" binary+ternary HYBRIDS with no standard
   notation name — the organic, off-grid durations that give the stochastic engine its feel:
   1/8+1/8T=3.333, 1/4+1/8T=5.333, 1/8+1/4T=4.667, 1/16+1/8T=2.333, 1/8T+1/8T=2.667.
   (Some triplet+triplet sums resolve BACK to clean: 1/4T+1/8T=4.0 = a clean 1/4.)

All form identically: a grid-aligned lead slurs forward; the destination (which CAN be a triplet
or 1/32) arrives as Tie/Legato and extendHold sums the exact length. Leading-edge preserves it
(destination reached via prevSlur; fractional destination's own slurForward=false ends the chain).
So the note-value dial × the tie mechanism spans dotted, compound, AND hybrid lengths from just 8
base values — a real expressive range, load-bearing on the fractional-destination rule.
