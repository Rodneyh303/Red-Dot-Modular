# Rhythm behaviour policy toggles (context-menu group)

A small, growing family of engine rhythm-behaviour choices that are each ~one-line forks in the
sequencer but real differences in musical feel, with no single "correct" answer. Collected here
because they share a shape and should probably live together in the UI (a "Rhythm behaviour"
context submenu) rather than as scattered checkboxes.

## The heuristic for exposing one as a toggle
Expose a choice as a menu toggle when ALL of:
  (a) the code difference is genuinely small (roughly one branch / one line at a decision site),
  (b) the alternatives are each musically wanted by some users (real taste difference), AND
  (c) it doesn't interact badly with the other toggles.
Caveat on (c): each independent toggle MULTIPLIES the behaviour space to test/reason about
(2 toggles = 4 combinations, 3 = 8). This engine is subtle (fractional tails, poly slaving,
reverse mode all interact), so watch the combinatorial growth — a toggle's true cost is the
testing surface, not the one line. Add deliberately.

## The policies

### 1. Slur vs rest (leading-edge legato) — see LEGATO_TIE_REDESIGN.md
When a leading-edge slur note N meets a REST roll on N+1: legato-beats-rest (slur suppresses the
rest) vs rest-beats-legato (rest cancels the slur; N ends at nominal length). Default leaning:
rest-beats-slur ("can't slur into silence"). Candidate toggle.
FIXED (not a toggle): a fractional NOTE TAIL always outranks a rest (physical — the note isn't
done sounding). Only the OPTIONAL slur reach is up for the toggle.

### 2. Boundary wrap — see LEGATO_TIE_REDESIGN.md
Held/long/legato notes CONTINUE across the phrase boundary (current) vs INTERRUPT (cut at the
boundary). Default: continue. Candidate toggle. Interacts with reverse mode + Lantern carets —
both must handle whichever is selected (the (c) caution in action).

### 3. Dice cuts held note? (latent — currently NO) — see LEGATO_TIE_REDESIGN.md exceptions
Currently dicing re-rolls pattern values but does NOT touch an in-flight gate hold (a held note
finishes its length before new diced values take over). Reasonable default; could become a
toggle if anyone wants dice to cut held notes. Not planned — noted for completeness.

## Implementation note
Keep each policy as a single boolean read at its decision site (engine field, JSON-persisted,
context-menu-bound), so the fork stays one branch and the combinations stay auditable. Group the
menu items so their relatedness is visible. These are engine POLICY (how it behaves), distinct
from user VALUES (knob/fader settings) — same conceptual layer as lock-mode/Conservation.

---

## The clearest illustration of boundary-wrap (lap 1 vs lap 2), from the build

A wraparound legato at step 0 makes the continue-vs-interrupt policy concrete and visible:
- LAP 1: step-0 note fires BLUE (NewNote) — no previous lap, so nothing to slur from; fresh note.
- LAP 2: step-0 note is TEAL/Legato with a gold held-in caret — the PREVIOUS lap's final note is
  a real predecessor, held across the boundary, so step 0 slurs from it.
Same pattern, same settings, NO modulation change — yet lap 1 ≠ lap 2. This is the essence of the
CONTINUE (current) policy: the sequence carries state across the loop point (a kind of cross-lap
MEMORY), so successive laps can differ. The gate held over the boundary was confirmed on Rack's
scope; the caret fix (Lantern heldIn now includes Legato) is what makes it visible.

The alternative INTERRUPT policy (force a new gate / reset at step 0) would make lap 1 and lap 2
IDENTICAL — every lap starts cold, so step 0 is always a fresh blue NewNote. Deterministic and
repeatable, no cross-loop memory.

So the boundary-wrap toggle is precisely: EVOLVING-across-laps (continue, has memory) vs
REPEATABLE-every-lap (interrupt, no memory). Neither is 'correct' — different musical behaviours,
which is why it's a toggle. These two build screenshots are the reference illustration. Not a bug:
the leading-edge model is working; the lap-1≠lap-2 difference is the continue policy by design.

---

## Maintenance note: the ACCENT lane lags RHYTHM/REST — always check the twin

Accent was added as a poly lane AFTER rhythm/rest, so it keeps turning up inconsistent with them
— not for any principled reason, just missed PARALLEL steps, one per site. Four instances found:
  1. getPolyAccent read the HOST param space, not the East expander (getPolyRest read the
     expander) → poly accentProb ~0 → no poly accents.
  2. polyAccentRandom had no init seed (polyRhythmRandom = 1.0 did) → stuck at 0 → all accent.
  3. SpreadManager stubbed accent in 5 places (spread[8][3]→[8][4], avgCache_, guards, etc.).
  4. recomputeEffectiveRhythm's !sandsActive block promoted polyRhythmRandom = slewedPolyRhythm
     but had NO polyAccentRandom = slewedPolyAccent line → accent never updated during play.

These are ONE incompleteness (accent bolted on later) surfacing wherever the two lanes should
mirror — not four unrelated bugs. So the reliable rule when touching accent OR debugging it:
  GREP for the rhythm/rest twin on the same code path and confirm the accent line exists.
  Anywhere you see polyRhythmRandom / rhythmRandom / slewedPolyRhythm / getPolyRest / a rhythm
  spread case, there MUST be a matching polyAccentRandom / accentRandom / slewedPolyAccent /
  getPolyAccent / accent spread case beside it. A missing twin is the bug.
If a fifth accent bug appears, this grep finds it before it ships.

(Lantern was the diagnostic each time — the mono-works/poly-doesn't and all-on/all-off contrasts
are immediately visible on the grid, far more legible than a scope trace for this class of bug.)

---

## OPEN BUG: reverse-mode (Mode E) isolated teal — investigation state (for next session)

SYMPTOM: In reverse/phase mode, Lantern shows teal (Legato) with no predecessor IN THE PLAY
DIRECTION (read right-to-left). Confirmed by user picture AND VCV scope: those teals RETRIGGER
(gate drops, distinct attacks) — the teal is drawn over a retrigger, so the LABEL is wrong.
Forward is clean; reverse-only. User confirms forward/reverse look identical in the log at the
mono-decision level.

KEY CONSTRAINT (the paradox to respect): the mono LEGPROBE state was IDENTICAL forward vs reverse
(wasHeld=1, holdRemain=0.5, prevPulse=-1 on every legato, both directions). So if the mono
decision state is the same both ways, the mono DECISION cannot be the sole differentiator — yet
the OUTCOME differs. Whatever differs must be per-direction and NOT captured by the mono probe.

THEORIES FALSIFIED (do not re-propose without new evidence):
1. "Reverse connects rightward, just reads backward" — FALSE (scope shows retrigger; picture shows
   no predecessor in play direction either).
2. "Gate-carry is forward-baked, reverse reads against the grain / perpetual slur chain" — FALSE
   (scope shows gate DROPS, not a continuous slur).
3. "wasHeld should use the true gate (gatePulseRemain>=0) not holdRemain" — FALSE and HARMFUL:
   a short note's gate legitimately closes mid-step (0.5 step = 3 of 6 pulses at ppqn24) in BOTH
   directions, so gatePulseRemain=-1 at capture is normal; testing it killed ALL legato (forward
   too). Reverted (c3e8526).
4. "A rest is wrongly leading a slur in reverse" — FALSE: leStarting excludes Rest (a rest can't
   set slurForward), and slur-into-a-rest is cancelled at the RECEIVER's rest branch in both
   directions. Rest/legato-lead coherence holds both ways.

STILL UNEXPLAINED / NEXT LEADS:
- Strands are read via getStrandIdx(totalStepsElapsed, len, off, rot) PER STRAND (separate
  len/off/rot). totalStepsElapsed advances WITH direction (−1 reverse). So content runs backward
  correctly. But note LENGTH (getNoteLenIdx → nvIdx) and the legato draw are read at the played
  step; slurForward is set by the lead and consumed by the NEXT PLAYED step as prevSlur. Need to
  check: in reverse, does the lead's committed length/gate actually correspond to bridging to the
  reverse-next note, or does the backward strand read pair a lead's commitment with a
  non-adjacent note's gate? I.e. the DECISION chains temporally (fine) but the GATE SPAN that
  makes it audibly a slur may not, because gate arming uses THIS note's length while the reversed
  strand places a different-length note next.
- The mono probe showed identical state because it only logs at emission; it does NOT log the
  RELATIONSHIP between a lead's slurForward and the receiver's actual gate bridging. NEXT PROBE
  (if needed): log, at a RECEIVER step, prevSlur AND whether the predecessor's gate pulse spanned
  to this onset — i.e. instrument the lead→receiver handoff, not each step in isolation.
- Consider: is the bug actually in DISPLAY (Lantern colour = decision) while the AUDIO is
  arguably-acceptable, i.e. should reverse teal simply not be drawn when the gate retriggers? That
  reframes it as "decision/label vs gate reality" — make the teal follow the actual gate, not the
  intent. This is the most consistent with ALL evidence (identical decisions, gate drops, teal
  wrong) and was the user's spec: "teal only if the lead's gate genuinely holds into this step."
  The failed fix #3 had the RIGHT INTENT but the WRONG SIGNAL (gatePulseRemain at capture). The
  right signal is whether the gate spanned CONTINUOUSLY from the lead's onset to THIS onset with
  no pulse gap — which needs tracking across the handoff, not a point read.

RECOMMENDATION for next session: instrument the lead→receiver handoff (not per-step), or
reconsider as a display/label fix (teal follows real gate continuity). Do NOT touch wasHeld's
holdRemain basis again — it's load-bearing for forward.

---

## Lantern architecture finding (narrows the reverse-teal fix location)

Q (user): is Lantern incorrect in reverse? Is it MVC?
A: Lantern IS MVC-ish and IS a FAITHFUL VIEW — it is NOT the source of the bug in the
view-mis-renders-model sense.
  - MODEL: Lantern's own cells[16][16] store (Cell{type,pitch,carets,...}).
  - VIEW: draw() renders cells[][].
  - UPDATE: recordCell() translates the ENGINE decision into a Cell, once per step.
  - Cell TYPE is set DIRECTLY from the engine decision: Lantern.cpp ~182
    `c.type = Legato` iff `dec == MonoDecision::Legato`. So the teal colour is a faithful
    mirror of the engine's decision.

THEREFORE the incoherence is at the MODEL level: the engine produces decision=Legato while the
GATE retriggers (scope). Lantern correctly displays an incorrect model value. The scope faithfully
shows the gate; Lantern faithfully shows the decision; they disagree because the ENGINE emitted an
incoherent (decision, gate) pair. This is THE key finding for where the fix goes.

TWO FIX LOCATIONS (choose next session):
  (A) ENGINE (model): make decision follow the gate — if the gate retriggers, don't emit Legato.
      "Correct" (model should be internally coherent) and matches the user's spec. Risk: this is
      exactly where failed-fix #3 lived; the hard part is the SIGNAL (see caveat).
  (B) recordCell (view mapping): derive c.type from the ACTUAL gate, not dec. recordCell ALREADY
      receives `const GateState& gs` AND already reads gs.gatePulseRemain (Lantern.cpp ~15 for
      lengthSteps). Attractive: display-only, off the audio path, both signals already in hand.

CAVEAT (do not repeat fix #3's mistake): recordCell runs at emission, when gs.gatePulseRemain is
the FRESHLY-ARMED value (e.g. 3), not the gap history. A single-instant gate read CANNOT tell
"the gate dropped in the gap before this onset" — that is precisely what sank fix #3
(gatePulseRemain>=0 at capture is normal for short notes both directions). The correct signal is
GATE CONTINUITY ACROSS THE LEAD→RECEIVER HANDOFF (did the gate stay high from the lead's onset to
this onset with no pulse gap?), which must be TRACKED across steps, not point-read. Whether the fix
is (A) or (B), it needs this continuity signal, not an instantaneous gate sample.

So: the cleanest framing is "teal = the gate was genuinely continuous from the previous onset to
this one" — a per-handoff continuity bit the engine can set (e.g. gateWasContinuous = gate never
hit the -1 sentinel between the previous onset and this one), consumed by the decision (A) or by
recordCell (B). That bit is the missing signal; design it first, then pick A or B.
