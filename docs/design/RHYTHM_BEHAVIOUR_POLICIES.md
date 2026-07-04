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

---

## Candidate mechanism: how a legato-receiver follows a REST in reverse (Mode E)

Q (user): why can a legato-receiver follow a rest in reverse? How would it happen?

Ruled out first: a REST step itself DOES clear the slur. The Rest branch falls through (no early
return) to `gs.slurForward = leWouldSlur`, and leStarting excludes Rest → leWouldSlur=false →
slurForward cleared. So "rest failed to clear slurForward" is NOT it.

THE CANDIDATE (code-grounded, NOT yet confirmed — needs the test below):
- armGate (GateState.cpp:17) sets gatePulseRemain as a pure DURATION (durSteps×pulses),
  DIRECTION-BLIND: "gate open for N pulses of wall-clock time," no notion of fwd/reverse.
- MidNote guard (SequencerEngine.cpp:241): `if (holdRemain>=1 || gatePulseRemain>0) → MidNote,
  EARLY RETURN` — fires whenever the gate is still counting down, REGARDLESS of direction, and
  returns BEFORE the rest branch AND before the slurForward update (line 363).
- FORWARD: a note's forward-in-time hold covers the NEXT FORWARD step = the next PLAYED step. So
  MidNote masking a rest slot there is the intended "long note overlaps a rest slot" behaviour,
  and slurForward persisting through the note's own held steps is correct.
- REVERSE: the gate still holds FORWARD IN TIME, but the next PLAYED step is the LOWER step. The
  strand content is read BACKWARD (totalStepsElapsed decrements) while the gate holds forward. So
  the step the hold MASKS and the note the slur HANDS TO are paired DIFFERENTLY than forward — a
  reverse-played step can be MidNote-masked (its own rest roll never runs) by a hold pointing the
  "other way" in strand terms, leaving slurForward stale → the next played step reads prevSlur=1
  and connects → legato-receiver following what was effectively a rest/ masked step.

Fits ALL evidence: identical probe state fwd/reverse (gate values genuinely equal), scope shows
RETRIGGER (mask + re-arm = drop/reopen), teal drawn (decision follows stale prevSlur), reverse-only
(the fwd/reverse pairing scramble).

FALSIFIABLE TEST (do this next session BEFORE any fix): probe at each MidNote EARLY-RETURN — log
whether the strand's REST roll WOULD have fired at that masked step (compute r_rest<restProb there
without acting on it), plus dir/step. PREDICTION: in reverse, isolated-teal receivers are preceded
by a MidNote-masked step whose suppressed rest roll would have been a rest. If the log confirms
masked-rest-before-teal in reverse but not forward → mechanism confirmed. THEN fix: the MidNote
guard / hold must be evaluated against the PLAY-DIRECTION-next step, or the slurForward handoff must
not survive a masked step in reverse. Design the continuity/direction-aware guard; do not point-read
the gate (that was failed fix #3).

---

## Reverse isolated-teal — session 2 close: what's PROVEN, what's ELIMINATED, the narrowed question

Probed extensively this session (MASKPROBE, RECVPROBE, LANTERNPROBE — all stripped). Established
BY DATA (a full reverse lap logged), not theory:

PROVEN:
- Reverse MONO engine decisions are correct & sparse: connects (LEG/TIE) ONLY at a few steps
  (logged run: steps 10, 5, 15), each with prevSlur=1 AND wasHeld=1 (a real held predecessor).
  Writes NEW (blue) at every other step.
- LANTERNPROBE confirmed Lantern WRITES those exact decisions to the matching cells: writeStep=10
  dec=LEG, writeStep=5 dec=LEG, writeStep=15 dec=TIE; writeStep=6 and =11 dec=NEW (blue). So the
  cell STORE matches the engine — no stale cells, no write skew, no stepIndex/decision mismatch.

ELIMINATED (do not re-propose):
- MidNote-masks-a-rest mechanism: ZERO MASKPROBE fired all lap. Dead.
- Stale forward teal at 6/11: LANTERNPROBE shows 6/11 written NEW in reverse. Dead.
- wasHeld true-gate (gatePulseRemain) basis: killed all legato forward too. Dead (reverted).
- Rest-leads-a-slur: leStarting excludes Rest; slur-into-rest cancelled at receiver both dirs. Dead.

THE REMAINING CONTRADICTION (the whole unsolved crux):
- The LOG says reverse mono is a correct, sparse set of real connections (10/5/15).
- The USER SEES isolated teal (mono AND poly) that does NOT have an adjacent blue predecessor to
  its right (checked against a picture — the isolated teal has SPACE around it, not a touching
  neighbour). So "predecessor is one column to the right, just no caret" does NOT cleanly hold.
- Earlier scope showed these teals RETRIGGER (gate drops), yet decision=Legato.

So the narrowed question for next session (fresh eyes): for ONE specific isolated teal, what is in
the cell IMMEDIATELY BEFORE IT IN PLAY ORDER (to its RIGHT in reverse)? — a touching blue, a blue
with a gap, or empty? And does THAT teal retrigger or hold? The answer splits it:
  - touching blue + holds  → pure display: no rightward connection caret in reverse (fix: dir-aware
    carets; connection visuals at Lantern.cpp heldIn/heldOut/onset are ALL left-biased, no
    lastPlayDir awareness).
  - gap/empty before it, or retriggers → the decision (LEG) is genuinely wrong for THAT cell: a
    Legato emitted where the gate did NOT bridge — engine model incoherence at that step, despite
    the sparse-and-valid-looking RECVPROBE. Would mean RECVPROBE's prevSlur/wasHeld can both be 1
    while the actual gate still dropped in the gap (the holdRemain-vs-gatePulseRemain disagreement,
    but WITHOUT the failed point-read fix — needs a continuity bit tracked across the handoff).

Lantern connection visuals confirmed LEFT-BIASED / no direction awareness: heldIn caret only at
s==0 (left edge), heldOut only at s==N-1 (right edge), onset=!heldIn. None consult lastPlayDir. If
the answer is "display," this is where the dir-aware fix goes.

STATE: clean build (probes stripped). Bug NOT fixed. 5+ theories eliminated. One clean
discriminating observation needed to finish.

---

## SOLVED (diagnosis): reverse isolated-teal = mid-pattern connection has no explicit visual link

The sequential play-order TRACE (one line/step, in play order, full final state) finally showed the
receiver→predecessor relationship DIRECTLY. Reverse lap trace (dir=-1), the connections:
  phys=10 LEG sem=9  ← predecessor phys=11 NEW sem=7 hold=1.00 pulse=6  (held, new pitch → valid slur)
  phys= 5 LEG sem=4  ← predecessor phys= 6 NEW sem=5 hold=1.00 pulse=6  (held, new pitch → valid slur)
  phys=15 TIE sem=7  ← predecessor phys= 0 NEW sem=7 hold=1.00 pulse=6  (held, same pitch → valid tie)

CONCLUSION (from data, not inference): every reverse teal/tie is a GENUINE, CORRECT connection —
held predecessor (pulse=6, gate fully open), sliding to a new pitch (or same for tie). The ENGINE
IS CORRECT in reverse. No phantom hold, no retrigger-mislabel in this trace. Prior "gate drops /
retrigger" reading was a different state/misread; the trace shows pulse=6 held predecessors.

WHY IT LOOKS ISOLATED (the actual bug — PURE DISPLAY): each reverse receiver's predecessor is the
physically-adjacent HIGHER step — one column to its RIGHT (10←11, 5←6). Lantern draws NO explicit
connection indicator for a mid-pattern Legato/Tie — the only carets are the s==0 held-in (LEFT
edge) and s==N-1 held-out (RIGHT edge), both for the PHRASE WRAP only. A mid-pattern teal relies
entirely on being VISUALLY ADJACENT TO ITS PREDECESSOR to read as connected. Forward: predecessor
is the cell to the LEFT (adjacent) → reads connected. Reverse: predecessor is the cell to the RIGHT
→ the left-to-right eye finds nothing on the left → reads ISOLATED, even though the blue predecessor
is right there to its right. Both mono & poly (same draw code), reverse-only.

FIX (design choice for the user — NOT yet built):
The connection visual must indicate WHICH neighbour a teal/tie connects to, direction-aware. Lantern
has lastPlayDir. Options:
  (A) Directional connection caret on the receiver cell pointing at the predecessor: forward → a
      small mark on the LEFT edge of the teal; reverse → on the RIGHT edge. (Mirrors the existing
      wrap carets but per-cell and dir-aware.)
  (B) Draw an explicit JOIN/tie-bar between the receiver and its predecessor cell (a line/bracket
      spanning the two columns), so the connection is shown regardless of read direction.
  (C) Leave the cell colours; just fix the READING by making the wrap carets/onset logic dir-aware
      AND accept that mid-pattern relies on adjacency (adjacency is correct in BOTH dirs — the
      predecessor IS adjacent, just on the other side; so maybe only the s==0/s==15 edge carets
      need dir-swapping for the wrap case, and mid-pattern is actually fine once the user knows to
      read right-to-left in reverse).

RECOMMENDATION: confirm with user whether mid-pattern reverse teal is truly a PROBLEM (the
predecessor IS visually adjacent, just to the right) or only the WRAP-boundary carets need
dir-awareness. Cheapest correct fix is likely (A) a per-receiver directional tick. Engine untouched.

STATE: clean build, probes stripped, ENGINE CONFIRMED CORRECT in reverse, bug is display-only and
localized to Lantern's lack of a direction-aware mid-pattern connection indicator.

---

## POSTMORTEM: reverse "isolated teal" was a DISPLAY bug (left-anchored bars). Engine was always correct.

ROOT CAUSE: Lantern drew every note bar anchored to the cell's LEFT edge, extending right
(bx = gridX + s*stepW), with no play-direction awareness. A short note (1/16 = 0.5 step) fills only
the left ~half of its cell. Forward, the empty right half reads as the natural gap before the next
note. In REVERSE (right-to-left), a note's predecessor sits to its RIGHT, but the fill still went
left — so the empty half landed on the play-direction side, making a valid legato receiver look
isolated with a "rest" to its right. The ENGINE produced identical, correct decisions in both
directions (proven by a forward-vs-reverse [G] trace side-by-side: reverse teals were valid legatos
with fully-sounding predecessors, pulse=6, same as forward).

FIX: Cell.playDir (from eng.lastPlayDir); in draw() anchor short-note bars toward the play direction
— reverse anchors to the cell RIGHT edge and extends left (bx = cellRight - bw); forward unchanged.
(Lantern.cpp draw loop.)

WHY IT TOOK SO LONG (the lesson): the left-anchored bar was VALUABLE and visibly correct in forward
mode, so it was trusted and never suspected for reverse — the mirror case failed for the very same
line that worked forward. Many engine theories were chased and falsified (wasHeld/gate direction,
LegatoMax, prevPlayedSounded, rest-leads-slur, MidNote-mask, strand offset, forStep column skew,
Mode E edge timing) — all dead ends because the engine was never wrong. The single diagnostic that
cracked it was the one NOT done for far too long: a forward vs reverse trace of the SAME fields,
side by side, showing the engine identical both ways ⇒ the difference had to be in the draw.

RULE FOR NEXT TIME: when a mirror/inverse case (reverse) breaks but the common case (forward) is
fine, SUSPECT THE SHARED CODE THAT WORKS IN THE COMMON CASE FIRST — especially any left/right/+1
draw or index expression with no direction term. Do the forward-vs-mirror side-by-side comparison
EARLY. "It clearly works [forward]" is the reason to check it, not skip it.

COLLATERAL FIXED: a decFired sounding shortcut (tried mid-investigation) had rendered every poly
voice whenever mono fired, erasing poly rests; reverted to per-voice gate test.
