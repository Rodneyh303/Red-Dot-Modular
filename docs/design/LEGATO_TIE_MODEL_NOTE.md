# Legato / Tie model — leading-edge decision + collapse (design note, FOR LATER)

Status: **design capture, not implemented.** Inspired by building Lantern (which made the
current timing visible) and checking the real Vermona meloDICER. This is a CORE ENGINE
behaviour change — it will alter how existing patterns sound — so it's captured for a
deliberate decision, not changed yet.

---

## Current model (Monsoon today, SequencerEngine::executeStep ~line 273)

Legato/tie is decided at the **step that continues** (the join), re-rolled every step:

    else if (r_legato_tie < legatoProb) {
        if (sem == lastSemitone && (wasHeld || hadTail)) { extendHold(); decision = Tie; }
        else                                             { slideNote();  decision = Legato; }
    }

So the FIRST note doesn't know it will be slurred; the NEXT step rolls the dice and, if it
wins, retroactively makes the join a tie (same pitch) or legato (different pitch). This is a
TRAILING-edge / reactive model.

Consumers of the Tie vs Legato distinction (audited):
- MonsoonOutputGenerator.cpp:94-96 — SEPARATE tie-gate and legato-gate OUTPUT JACKS. This is
  the only functional consumer.
- Poly voices (SequencerEngine.cpp:441) always slideNote() regardless — poly does NOT use
  the tie/legato label. Poly consistency needs only "gate held vs retriggered".
- Everything else (extendHold vs slideNote) is just mechanism (hold-same-pitch vs
  slide-to-new-pitch), determined by PITCH, not a separate decision.

---

## Proposed model (leading-edge + collapse)

### A. Leading-edge legato (matches the meloDICER hardware)
Legato is decided **up front at the note's onset** — a note commits to holding its gate open
past its natural end (the trailing edge slurs into whatever follows). The first note KNOWS it
is trying to legato. Not re-rolled at the join.

- **Legato eligibility:** only notes of length >= 1 step can INITIATE legato. Triplets and
  1/32 (fractional-length) notes end before the next step, leaving a gap the held gate can't
  bridge, so they are ineligible to start a legato.
- The note sets a "will hold gate" intent at onset; the gate stays high at its trailing edge.

### B. Collapse tie vs legato into ONE internal "held/slurred" state
Since the decision is just "hold my gate open", tie-vs-legato is not a separate decision —
it's DESCRIPTIVE, resolved by pitch at the join:
- next note SAME pitch  → reads as a **tie**
- next note DIFFERENT pitch → reads as a **legato slide**
- We only KNOW which when the next note's pitch is drawn ("strict legato vs tie known if/when
  the note changes").

Internally the engine makes one decision (hold gate). The tie/legato LABEL is DERIVED
post-hoc (did the pitch change?) purely for:
  - the two output jacks (keep them: tie-gate fires when held+same-pitch, legato-gate when
    held+different-pitch — a cheap derived read, not a decision), and
  - Lantern's colour (one "held" colour; tie-vs-legato emerges from whether the pitch bar
    changes height at the join — truthful, no engine label needed).
Poly voices just mirror "held" and draw their own pitch (already do — no change).

### C. Rest after the first note cancels legato
If a note commits to holding its gate but the NEXT step is a rest, there is nothing to slur
into. OPEN QUESTION: does the gate close at the rest (legato intent cancelled), or does the
legato-intent suppress the rest? Current guess: a following rest cancels the legato (gate
closes). Needs deciding.

---

## Why this is better
- Matches the hardware's actual gate behaviour (gate physically stays high across the
  boundary — a decision the sounding note makes about its own release, before it knows the
  next note).
- Simpler engine: one "hold" decision instead of a tie/legato branch.
- Simpler poly: mirror "held", no label to propagate (already the case).
- Simpler + more truthful Lantern: one held/slurred colour; pitch tells tie vs legato.

## Migration implications (why it's captured, not done)
- CORE ENGINE change: existing patterns will sound different (legato placed at onset, not
  reactively at joins; triplets/1/32 can no longer start legato).
- Keep the tie-gate / legato-gate output jacks working via the derived label so patches
  using them don't break silently.
- Accent already decided at onset + carried through (result.accented inheritance) — consistent
  with a leading-edge model, so accent needs no change.
- Confirm against the meloDICER manual / careful listening before committing (user "pretty
  sure" but not certain).

## Open questions
1. Rest-follows-legato: cancel (close gate) vs suppress the rest? (C above)
2. Keep two separate output jacks (derived label) or collapse to one "held" gate output too?
3. LegatoMax (legatoProb >= 0.999) — how does it fit the leading-edge model? (currently a
   separate always-slide path)
4. Does legato eligibility (>=1 step) interact with the note-value probability draw, or is it
   a hard gate applied after the length is known?

---

## Resolved: one gate out, no separate legato/tie jacks (closes Open Q2)

**Decision.** Straits/Monsoon expose ONE gate output per voice: it stays high across a
held boundary and only falls + re-rises on a genuine RE-ARTICULATION. No `LEG`, no `TIE`,
no `T|L` jack. (Monsoon's three legato-family gate outs collapse to this one; the accent
short-column blank in Straits layout E is NOT filled with a poly-legato out.)

**Why it's forced, not chosen.** Under the leading-edge model, at a note's ONSET the engine
cannot yet know whether the step will tie, legato, re-legato, or rest+end-edge -- that
resolves later. So "legato gate" and "tie gate" are not distinct *measurable* signals at
emission time; they are two labels for one physical fact: **the gate stayed high and was
not retriggered.** A separate legato jack would have to assert a classification the model
doesn't possess when the jack would need to go high. It is a phantom output.

An envelope patched to the single gate does the correct thing regardless of eventual
resolution: hold while high, no re-attack while it stays high, re-attack on a real
retrigger. Tie / legato / re-legato all produce an identical downstream response *because
downstream they ARE identical* -- a continuous gate. Only rest+end-edge differs, and that is
just the gate falling, already expressed.

**Industry check (2026-07).** No battleship or utility sequencer surveyed advertises a
legato gate distinct from its main gate (Frap Tools Usta, RYK M185, Intellijel, Noise
Engineering Steppy, doudoroff's comparison, etc.). The distinction lives on the RECEIVING
side, exactly as our model implies:
- Moog Mother-32 `Sustain` switch: when ON, overlapping/held notes do NOT retrigger the EG
  -- legato is a property of how the gate is *interpreted*, not a second signal.
- Mutable Shruthi/MIDI legato mode: hold a note, play another, envelopes are NOT retriggered,
  pitch moves -- same single-gate-no-retrigger behaviour.
- Intellijel uMidi/1U expose GATE **and** a separate TRIGGER (not a legato gate): the trigger
  re-fires per note so a patch can CHOOSE to retrigger under legato. The extra jack is the
  RE-ARTICULATION signal, i.e. the *complement* of legato, and it is only definable because a
  trigger is knowable at onset. Our single sustained gate carries the same information: its
  edges ARE the re-articulations.

So the convention is universal and the reason is the one above: legato is carried by the
gate's continuity, and re-articulation by its edges. One output says both.

**If a second jack is ever wanted**, the defensible one is a per-note TRIGGER/RE-ARTIC gate
(fires on genuine onsets only), never a "legato gate" -- because a trigger is knowable at
onset and legato is not. That is the Intellijel pattern, and it would be the honest addition
to the accent blank if poly re-articulation proves patch-worthy. Do not reintroduce LEG/TIE:
they encode a distinction the engine cannot make when the signal would need to assert it.

---

## Adopted second output: STEP GATE (the un-fused gate)

Supersedes the "sub-step hold-clock" sketch below-the-line earlier drafts explored. That
framing (a pulse train alongside a flat gate) was close but mis-scoped: it made the second
signal a derivative of the held note. The correct model is simpler and symmetric.

**Two gate outputs, one pattern, two renderings of it:**

- **GATE** — the pattern with legato/tie APPLIED. Tied/legato steps FUSE into one
  continuous high. A 1/4 legato-lead into 1/8 into 1/16 is a single sustained gate across
  the whole span (one phrase).
- **STEP GATE** — the same pattern with legato/tie STRIPPED. Every step is articulated as
  if legato were off: the 1/4, 1/8, 1/16 come out as three separate gates, each carrying
  its OWN note length (a 1/8 continuation = a 1/8-long gate, a 1/16 = a 1/16-long gate).
  Not a fixed-width trigger — a GATE, whose length is the sub-note's length.

So: **GATE = legato applied. STEP GATE = legato removed.** Same notes, same pitch CV, two
gate streams — one fuses tied notes into a phrase, one keeps them separate.

**Why it's trivially honest (knowable at emission).** STEP GATE is the gate the engine
ALREADY computes one stage BEFORE the legato-fusion step — the pre-fusion gate that is
currently consumed and discarded. The second output is a TAP one stage upstream, not new
logic and not a classification the model lacks. (Implementation note: confirm the pre-fusion
gate exists as an addressable value at the emission point rather than being folded in place;
if folded, split the fusion into "compute step gate -> fuse -> gate" so both are tappable.)

**Degenerates cleanly.** For a pattern with NO legato/tie, there is nothing to fuse, so GATE
and STEP GATE are identical. The second jack costs nothing and confuses no one on ordinary
patterns; it only diverges where legato/tie is actually present. That is the mark of an
honest output.

### The piano analogy (why this is a real musical decomposition, not an engineering novelty)
This is the sustain-pedal-vs-hammer split that acoustic pianos have carried for centuries:
- **GATE = the sustain pedal.** Held down across the phrase; the sound rings on, one
  continuous gesture.
- **STEP GATE = the hammers.** Each key in a pedalled run still strikes its own string —
  the notes re-articulate individually even though the pedal never lifts.
Un-pedalled staccato = pedal and hammers coincide = GATE and STEP GATE identical (the
non-legato degenerate case). A pedalled trill or run = one held pedal, many hammer strikes =
GATE high while STEP GATE fires per sub-note. The two jacks expose the two mechanisms the
piano keeps mechanically separate.

### Patch cases (confirming it's useful, not just tidy)
The design pays off ONLY when the two jacks drive DIFFERENT destinations (patched to the
same envelope, STEP GATE's articulations merely fight GATE's sustain). Manual should say so.
- **VCA off GATE, VCF off STEP GATE** — amplitude sustains the phrase; filter env re-strikes
  each sub-note. One swell, three sweeps inside it.
- **VCA off GATE, second env off STEP GATE -> VCA gain offset** — held phrase with internal
  dynamic bumps on each sub-note (articulation without closing the VCA). Arguably the most
  musical: a legato line that still has internal emphasis.
- **STEP GATE -> resonant body / physical model strike (Rings, Plaits), GATE -> its
  sustain** — bowed-then-re-excited string: rings continuously, re-struck per sub-step.
- **STEP GATE -> S&H clock on noise -> micro-timbre (detune, fold)** — phrase holds, timbre
  shifts locked to the interior rhythm the fused gate hides.
- **STEP GATE as a clock** — a held legato phrase advancing an arp/LFO one step per sub-note;
  the "held" note secretly drives downstream rhythm.

The patches above use one jack each. The next set exploit the PAIR -- combined with one
logic/utility module, GATE and STEP GATE act as a BASIS that spans a space of legato-aware
behaviours (their sum, difference, and XOR are all musically meaningful):
- **Legato-only vibrato (GATE XOR STEP GATE).** The XOR is high ONLY during a slur (see
  next section). Route it to a VCA on a mod source (vibrato, filter wobble): modulation
  appears only while notes are connected, dead on staccato -- exactly how a string player
  adds vibrato to slurred/held notes but not detached ones. Two jacks + one XOR = an
  idiomatic articulation-dependent expression.
- **Density-following percussion.** STEP GATE -> a hat/click voice: held phrases still emit
  grid pulses, so busier legato passages auto-generate busier percussion. The rhythm section
  tracks melodic density with no extra sequencing.
- **Envelope morph (pedal<->hammer as a knob).** GATE -> slow pad env, STEP GATE -> fast
  percussive env, crossfade the two VCAs with a manual control: sweep a phrase continuously
  from "sustained pad" to "articulated pluck" -- the sustain-pedal-vs-hammer blend made
  performable.
- **Continuation accents vs phrase-initial accents.** Two edge-derived pulses select which
  part of a phrase gets stressed (-> velocity/accent):
  - phrase-initial only = **rising-edge(GATE)** -- GATE only rises at a phrase start (held
    high through the span), so its own rising edge IS the phrase-initial pulse. One edge
    detector on GATE; STEP GATE not even needed.
  - continuation only = **rising-edge(STEP GATE) AND GATE-already-high** -- a sub-note onset
    interior to a held span (equivalently rising-edge(STEP GATE) AND NOT rising-edge(GATE)).
- **Answer voice, harmonic.** STEP GATE -> a poly voice's gate, its pitch from the quantizer
  a third/fifth above the lead: the slur's internal rhythm becomes a harmonised counter-line
  (the rhythmic version -- lead pitch, no transpose -- is the plain answer-voice patch).
- **Accent per sub-note (ACCENT AND STEP GATE).** ACCENT is held across an accented slur
  (phrase-shaped, like GATE); AND it with STEP GATE to get an accent pulse on EACH sub-note
  of the slur, silent on unaccented phrases. Route to a velocity/VCA-offset so every hit of
  an accented run is emphasised while unaccented runs stay flat. (There is deliberately no
  STEP ACCENT jack -- this patch is why one isn't needed.)

### Not needed: STEP ACCENT (accent is one value per phrase; the useful signal is derivable)
Symmetric question to STEP GATE: should there be an un-fused accent output? No -- and for a
structurally different reason, which is why it's worth recording rather than assuming.

STEP GATE is legitimate because a legato span has TWO real renderings of the gate (fused /
un-fused) and both are wanted. Accent has only ONE value across a span: it is decided at the
phrase's leading edge and INHERITED by the continuations (`result.accented` / per-voice
`pv.accented`, the same value the Lantern reads to brighten a whole slur). The sub-notes of a
slur have no independent accent states, so there is nothing to un-fuse. A "STEP ACCENT" jack
would have to emit per-sub-step accent values that never existed -- the LEG/TIE failure mode
(assert a distinction the engine doesn't hold), failing the emission test not on timing but
on absent values.

The genuinely useful nearby signal -- accent re-articulated per sub-note of an accented slur
-- is DERIVABLE, exactly like the legato-region gate: the accent flag is known (one value,
held across the span) and STEP GATE is the per-sub-step rhythm, so

    ACCENT_GATE  AND  STEP_GATE  =  accent, pulsed at each sub-step, only while accented.

One logic module. So no fifth jack. **ACCENT stays as-is: FUSED / phrase-shaped, mirroring
GATE** -- accent is a property of the phrase (one decision), so its gate carries the phrase's
shape; AND it with STEP GATE at patch time if you want it articulated. This makes the pair
symmetric: GATE and ACCENT are both phrase-shaped, STEP GATE is the articulator for either.

**The output set is therefore complete: GATE, STEP GATE, CV, ACCENT** -- four jacks. Every
legato-aware variant is a set operation on two EDGE-SETS plus utilities the patcher already
owns (see "Edge-set algebra" below): the levels GATE/STEP GATE span the LEVEL questions
(in-legato, isolated), their EDGES span the ONSET questions (phrase-initial, every-onset,
continuation-only), and ACCENT gates any of them.

### Edge-set algebra: phrase structure lives in GATE-edges vs STEP-GATE-edges
The two gates give two edge-sets, and ALL "which part of the phrase" signals are set
operations on them (edge detector / gate-to-trigger utility on the level, e.g. Doepfer
A-162, Maths rise, any rising-edge module):

- **GATE edges = phrase boundaries.** GATE rises ONCE at a phrase start and holds; it does
  not re-rise mid-phrase. So its rising edge already IS the phrase-initial pulse.
- **STEP GATE edges = every articulation.** Rises at the phrase start AND at each
  continuation.

From which:
- **Phrase-initial only** = `rising-edge(GATE)`. ONE primitive -- GATE alone, no STEP GATE
  needed. (Fires once per phrase, at the start.)
- **Every onset (incl. continuations)** = `rising-edge(STEP GATE)`. STEP GATE alone.
- **Continuation only** = every-onset MINUS phrase-initial =
  `rising-edge(STEP GATE) AND GATE-already-high` (equivalently `AND NOT rising-edge(GATE)`).
  This is the one that genuinely needs BOTH: a continuation is a STEP GATE onset interior to
  a GATE-high region.

(Earlier drafts listed "phrase-initial" as a two-primitive Boolean -- overstated. It is the
cheapest of the family: GATE's own rising edge. Corrected here.)

Combined with the LEVEL operations (next section: in-legato = GATE XOR STEP GATE; isolated =
its inverse) and ACCENT gating any of the above, the four jacks span the space: two levels
for the "are we in a slur" questions, their two edge-sets for the "which onset" questions.

### Not needed as jacks: the derived legato-region signals (and the ONE that needs memory)
Signals like "fires only outside legato", "only during it", or "a continuous gate high for
the whole slur" are recurrently tempting. None is a new primitive; each is derived from the
gates we emit. But they split into two classes -- pure Boolean (combinational) vs
latch-requiring (needs one bit of memory) -- and it's worth being exact about which is which,
because an earlier draft here overstated the XOR case.

**Combinational (pure AND/OR/NOT, no memory) -- one logic module:**
- **within-legato interior** = `GATE AND NOT STEP GATE`. High in the GAPS between a slur's
  re-articulations (where GATE holds but STEP has dropped). This is NOT a continuous
  "in a slur" level -- it is low DURING each STEP pulse, so it looks like the inverse of the
  slur's rhythm, not a phrase-long gate. (The earlier claim that `GATE XOR STEP` "is high
  exactly during slurs" was wrong in exactly this way: XOR gives the interior gaps, not a
  sustained level.)
- **continuation-only** (STEP gates GATE fused away) = `edge(STEP) AND GATE AND NOT edge(GATE)`.
- **phrase-initial only** = `rising-edge(GATE)` (GATE alone -- it rises once per phrase).
- **every onset** = `rising-edge(STEP GATE)`.

**Latch-requiring (needs one bit of memory) -- ~2 modules:**
- **IN-LEGATO** = a CONTINUOUS gate, high for the whole duration of a slurred phrase, low on
  isolated notes ("the legato-envelope gate" other sequencers are imagined to have). This
  CANNOT be a pure Boolean: it is a phrase-level STATE, but every gate we emit is note-level
  PULSES, and turning "a slur pulse occurred" into "we are inside the slur" needs memory.
  Derivation: `SR-latch(set = STEP LEGATO, reset = falling-edge GATE) AND GATE`. The latch
  goes high on the first STEP-LEGATO pulse and holds until GATE falls; ANDing GATE bounds it
  to the phrase. On an isolated note STEP LEGATO never fires, so the latch stays low -> not
  in legato. (Equivalent set/reset using STEP: latch `GATE XOR STEP`, reset on falling GATE.)
  ~2 utility modules (any flip-flop + AND).

**Why IN-LEGATO is correctly NOT a jack** -- two reasons, both decisive:
1. It's a cheap latch-derivation from gates we already emit (STEP LEGATO makes it trivial).
2. More fundamentally, on a CONVENTIONAL sequencer "gate high during legato" IS just the main
   gate -- a normal gate goes high and stays high through a slur (that's what legato means to
   it). IN-LEGATO and GATE are the same wire there. It only looks like a distinct signal on
   OUR design because we deliberately split articulations out of the main GATE into
   STEP/STEP LEGATO. Nobody ships it as an output because it's either the plain gate
   (conventional) or a two-module latch (ours). Exposing it would be the LEG/TIE mistake
   inverted: shipping what the patcher gets for free.

So the emitted set stays GATE + STEP + STEP LEGATO (+ CV + ACCENT). Every legato-region
variant -- IN-LEGATO, ISOLATED, within-legato, continuation-only, phrase-initial -- is a
Boolean or a one-latch derivation of these, per the edge-set algebra below.

**Why the whole family is derivable -- the two edge-sets.** Phrase structure is entirely
encoded in the relationship between GATE's edges and STEP GATE's edges:
- **GATE edges = phrase boundaries.** GATE rises once at a phrase start and does not re-rise
  until the phrase ends, so its rising edges are exactly the phrase-initial onsets.
- **STEP GATE edges = every articulation.** It rises at every sub-note (initial + each
  continuation).
Every "which part of the phrase" question is then a set operation on those two edge-sets:
- phrase-initial only      = rising-edge(GATE)                              [GATE alone]
- every onset (incl. cont) = rising-edge(STEP GATE)                         [STEP GATE alone]
- continuation only        = rising-edge(STEP GATE) AND NOT rising-edge(GATE)
- in-legato (region)       = GATE XOR STEP GATE   (level, high during slurs)
- isolated (region)        = NOT (GATE XOR STEP GATE), gated by note activity
- accent per sub-note      = ACCENT AND STEP GATE
That is the sense in which GATE + STEP GATE (+ ACCENT, + CV) is a COMPLETE basis: the level
signals come from Boolean combination, the event signals from edge detection on one or the
other, and the phrase-vs-continuation distinction is just "which edge-set". No further jack
adds anything not on this list.

### Naming
**STEP GATE** (leaning). It is the per-step gate — the pattern's articulation before legato
fuses it. NOT "retrig"/"legato gate": it is a full-length gate per sub-note, and it says
nothing about note semantics — it is simply legato removed. Alternatives considered: ARTIC,
DRY GATE, GATE-S.

### Prior art: why Vermona's meloDICER never exposed this (and it was right not to)
meloDICER emits ONLY the fused gate. Its manual: legato means "the pitch (1V/OCT) between
two notes changes without generating a new gate signal at GATE OUT", and at maximum "GATE
OUT is always on (+10 volts)". So STEP GATE is a signal meloDICER computes internally (it
must, to decide whether to fuse) and never outputs. It did not consider-and-reject the
un-fused gate; it simply lives in the one quadrant where the un-fused gate isn't needed.

Vermona occupies exactly two contexts, and NEITHER has the problem STEP GATE solves:
- **Mono CV/gate (base module).** One voice, one 1V/OCT out, one gate. STEP GATE would buy
  only the VCA/VCF split -- there is no second voice to "answer" with. On a deliberately
  minimalist mono instrument ("cut all the fat") a jack that only diverges during legato is
  exactly the fat they prune. Correct omission for THAT module.
- **Poly MIDI (MEX3 expander).** MEX3 adds three-voice polyphony but adds ZERO CV/gate jacks
  -- the polyphony is MIDI-only (SOS: "with CV/gate it stays mono, primarily because of the
  missing outputs"). And over MIDI the question DISSOLVES: MIDI note-on/note-off carries
  articulation per note intrinsically -- a tie is overlapping note-ons with no note-off
  between; a re-articulation is note-off-then-on. There is no single gate LINE forced to
  choose fuse-vs-articulate, so there is nothing for a STEP GATE to disambiguate. The
  receiving synth's own legato/retrigger mode decides -- legato lives on the receiving side
  (third confirmation of that principle in this doc).

So STEP GATE is a solution to a problem that exists ONLY in the poly + CV/gate quadrant:
poly gives answer voices to route to; CV/gate makes articulation a per-voice gate-LINE
choice (one bit per wire) rather than a free per-note MIDI event. Vermona sat in mono-CV and
poly-MIDI; dot.modular sits in poly-CV, the one corner where the un-fused gate is both
necessary (CV can't imply it) and useful (poly can consume it). The claim is not "cleverer
than Vermona" -- it is "a different quadrant, where this signal is real."

### The engine already surfaces this per voice (Lantern) -- panel vs patchbay parity
The per-voice fusion state already EXISTS and is already read out: the Lantern reads
`voices[i].gs.lastNoteType` (Single / Tie / Legato) per voice to colour each lane. So:
- STEP GATE needs no new engine state -- `NoteType` per voice is the exact fork it taps.
  The Lantern is proof the pre-fusion distinction is a first-class per-voice value, not
  something folded away (this de-risks the "is it addressable at emission?" open worry).
- It is a CONSISTENCY fix: today the panel DISPLAYS Single/Tie/Legato per voice but the
  outputs cannot EXPRESS that difference -- you can see more than you can patch. STEP GATE
  routes the same distinction the Lantern shows to a jack, so panel and patchbay finally
  agree.

### Monsoon / Straits consequences
- Monsoon's three legato-family gate outs collapse to **GATE + STEP GATE = 2** distinct
  signals (fused / un-fused), not three labels for one.
- Straits layout B (bottom output strip) widened to 4: polygate, polystepgate, polycv,
  polyaccent — four poly outs, each a distinct signal. This is the reason B is preferred
  over E: the output row is the right home for a growing set of poly outs, and STEP GATE is
  the fourth that makes the 4-wide strip real rather than speculative.
- Poly is where STEP GATE is irreplaceable: per-voice "this voice's tied phrase, un-fused"
  cannot be derived from any single fused gate externally — only the engine holds each
  voice's pre-fusion gate.

---

## (superseded) earlier sketch: sub-step hold-clock
Kept for provenance; STEP GATE above is the adopted form. The hold-clock framed the second
signal as pulses INSIDE a held note; STEP GATE reframes it as the whole pattern with legato
removed, which is simpler, symmetric (applied vs removed), carries real per-note lengths,
and is a direct upstream tap rather than a derived pulse train.

