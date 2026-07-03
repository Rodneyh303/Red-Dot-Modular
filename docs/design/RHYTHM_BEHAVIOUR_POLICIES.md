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
