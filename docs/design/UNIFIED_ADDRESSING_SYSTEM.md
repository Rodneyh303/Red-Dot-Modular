# Unified pattern-addressing system (global / voice / lane) + voice LOR

STATUS: design direction, POST-RELEASE. Major but conceptually clean. This is the next natural
consolidation (like copula unified seq/quant, correlation unified per-voice value): pattern ADDRESSING
is the subsystem not yet unified. Capturing the full worked-through design so it isn't re-derived.

## LOR component definitions — AUTHORITATIVE (Rodney). Read the rest against these.
For the EXISTING lane LOR (and the same meanings nest at voice/global):
- **L = LENGTH** — the pattern length for this lane: the NUMBER OF 1/16 STEPS it spans (how many
  steps before it repeats).
- **O = OFFSET** — WHICH of the 16 steps it STARTS FROM (where the lane's window begins in the
  16-step space).
- **R = ROTATION** — WHERE WITHIN the steps it starts when the GLOBAL PHRASE STARTS (the phase into
  the lane's content/window at phrase start).
So O+L define the WINDOW (O = where it starts; L = how many steps); R defines the PHASE INTO that
window at phrase-start. These are three independent controls.
NOTE (correction): earlier in this doc an "offset" example (window 5..12, "start at 7") actually mixed
O and R — correctly it is L=8, O=5 (window steps 5..12), R=2 (at phrase start begin at step 7=5+2,
wrap within 5..12). Read all composition rules below with THESE meanings: "offset" = O (window
placement, which step it starts from); the phase-into-window-at-phrase-start is R (rotation), NOT O.

## NESTING REFERENCE — lane R is referenced to VOICE step 1, voice R to GLOBAL step 1 (Rodney, resolved)
This resolves how voice LOR actually propagates to the lanes (and fixes "voice rotation does nothing"):
- **Voice R** = where the VOICE reads when **GLOBAL is at step 1**. (e.g. voice R=7 => global 1 => voice
  reads window-step 7.)
- **Lane R** = the lane's position when the **VOICE is at step 1** (voice-window start) — NOT when global
  is at 1. (This is the CHANGE from the old behaviour, where lane R referenced global step 1.)
So the anchor chain nests: global-1 anchors the voice (voice R); voice-step-1 anchors the lanes (lane R).
Lanes advance driven by the VOICE's step. Because voice R determines WHEN voice-step-1 occurs in global
time, voice R now PROPAGATES to the lanes (shifting voice R shifts the voice-step timeline, moving when
the lanes hit their rotations and everything downstream). Voice R is no longer inert.

### Worked two-lane example
Voice: L=8, O=5, R=7 (window 5..12). Lane A: L=4, O=1, R=3 (win 1..4). Lane B: L=6, O=3, R=6 (win 3..8).
Voice reads its window from 7 at global 1, looping within 5..12. "Voice step" = window position 1..8
(pattern 5=pos1 ... 12=pos8); voice-step-1 (window start, pattern 5) occurs at global 7 and 15.
Lanes hit their rotation at voice-step-1 (global 7/15), advancing with voice-step:

| global | voice reads | voice step (1..8) | lane A (win1-4,R=3) | lane B (win3-8,R=6) |
|   1 |  7 | 3 | 1 | 8 |
|   2 |  8 | 4 | 2 | 3 |
|   3 |  9 | 5 | 3 | 4 |
|   4 | 10 | 6 | 4 | 5 |
|   5 | 11 | 7 | 1 | 6 |
|   6 | 12 | 8 | 2 | 7 |
|   7 |  5 | 1 | 3 (=lane R) | 6 (=lane R) |
|   8 |  6 | 2 | 4 | 7 |
|   9 |  7 | 3 | 1 | 8 |
|  10 |  8 | 4 | 2 | 3 |
|  11 |  9 | 5 | 3 | 4 |
|  12 | 10 | 6 | 4 | 5 |
|  13 | 11 | 7 | 1 | 6 |
|  14 | 12 | 8 | 2 | 7 |
|  15 |  5 | 1 | 3 | 6 |
|  16 |  6 | 2 | 4 | 7 |

Checks: global 1 -> voice reads 7 (voice R); lanes NOT at their R (they reference voice-step-1, which is
at global 7). global 7 (voice step 1) -> lane A=3, lane B=6 (their Rs). Change voice R -> voice-step
timeline shifts -> lanes shift too (voice R now matters). (Assumes voice-step-1 = voice WINDOW START; if
instead it should be the voice's rotation position, shift the anchor accordingly — mechanism unchanged.)

## The gap (Rodney)
Three levels of pattern addressing exist but are NOT one system:
1. GLOBAL pattern length + offset (Monsoon) — top frame.
2. PER-LANE LOR (length/offset/rotation/direction per lane, Sands East) — lane windows.
3. PER-VOICE — does not exist. (The thing this doc adds.)
Global and lane were added separately; they interact but aren't instances of one model. Adding
per-voice onto an un-unified base is what made it feel "fiendish" (length-vs-length conflicts, "what
wins"). Unify first; per-voice falls out.

## The unification — NESTED MAPS: lane within voice within global
Hierarchy (Rodney): **global step → voice step → lane step.** "Lane is the probability half of voice."
- Each level is the SAME transform: (length, offset, rotation, direction) = LOR.
- **Lane LOR, re-based:** currently maps GLOBAL step 1 → lane step. CHANGE it to map VOICE step 1 →
  lane step. (Only change to existing code.)
- **Voice LOR, NEW:** maps GLOBAL step 1 → VOICE step 1. This IS voice LOR.
- Compose: `read_position(voice, lane) = laneLOR( voiceLOR( counter ) )`.
- **Backward-compatible by construction:** voice LOR at identity ⇒ global step = voice step ⇒ re-based
  lane LOR reads EXACTLY as today. Bit-compare to prove the refactor reproduces current behaviour
  before voice LOR carries any new setting.
- No length conflict: lane is NESTED in voice (the probability read WITHIN the voice's position), not
  competing on the same axis — that's why "lane is the probability half of voice" resolves it.

## HARD CONSTRAINT (Rodney) — NO LOGIC CHANGE OUTSIDE THE PROBABILITY READ
Voice LOR only changes WHERE each voice reads its probability — an addressing transform on the read.
It does NOT change generation, gate/subgate logic, legato/tie timing, correlation, or anything
downstream. This is what keeps it cheap (UI + compute) AND avoids the fiendish per-voice gate-timing
interaction.
**Scope this honestly:** because only the READ position moves (not step TIMING), voices still step on
the same clock/grid — they READ DIFFERENT PROBABILITY WINDOWS at the same step times. So this buys
per-voice probability-WINDOW offset (content staggering), NOT per-voice time-DISPLACEMENT (true
timing-canon). That is the correct, tractable scope: true timing-canon is the fiendish gate-logic
version we are deliberately NOT doing. The constraint self-selects the tame interpretation.

## What it buys musically — the TIME axis, orthogonal to correlation's VALUE axis
Correlation (copula) relates voices in VALUE (±correlation, groups) — simultaneously, same position.
Voice LOR relates voices in TIME/PHASE — different read windows. These are orthogonal; together they
span the full "how voices relate" space. Voice LOR unlocks forms correlation STRUCTURALLY cannot:
- **Offset** → canon-like staggering (content read-shifted).
- **Length** → per-voice polymeter (different cycles, drift/realign over a meta-cycle).
- **Rotation** → phase within the window.
- **Direction** → retrograde / mirror / convergence (read backward) — note: time-reversal is a
  DIFFERENT "opposite" from negative correlation (value-inversion). Having both makes "the opposite of
  a voice" a 2D choice (invert what it plays AND/OR reverse when it reads).
(Within the no-timing-change scope these read as content/window effects rather than literal temporal
canon — still a large, distinct musical class vs value-correlation alone.)

## Direction composition — voice direction is EXPRESSED THROUGH lane direction (Rodney)
Direction is the one LOR component where composition isn't offset-addition — pin it explicitly:
- **Effective direction = voice direction x lane direction (sign multiply).** Lane direction is
  RELATIVE TO THE VOICE, not absolute real-time, because lane maps FROM voice step ("lane is the
  probability half of voice"). So:
  - voice forward x lane forward = forward
  - voice REVERSE x lane forward = reverse (voice reversal carries through)
  - voice REVERSE x lane reverse = forward (double negative — lane un-reverses within a reversed voice)
- **Do NOT flip lane direction when the voice reverses — it INHERITS by composition.** The lane's own
  setting stays; reversing the voice reverses the voice step, and the lane (forward-relative-to-voice)
  consequently reads backward in real time.
- This is FORCED by the nesting, not a free choice: "lane reads from voice step" is incompatible with
  "lane direction is absolute in real time" (they conflict the moment the voice reverses). So
  **voice direction can only be EXPRESSED VIA lane direction** — there is no separate real-time voice
  traversal to show; the voice's reversal manifests as the lanes (its probability reads) traversing
  backward. That is the only place voice direction becomes audible/visible.
- Reversible: sign product composes and inverts cleanly, same as the rest.

### THREE-factor direction: add the PLAYHEAD (phase can drive it backward)
Two axes combine at the final read: PLAYHEAD direction (timing — the light ring; phase mode can drive
it BACKWARD) and the probability-READ direction (voice x lane). They multiply into one effective
real-time read direction:
  **effective = playhead_dir x voice_dir x lane_dir   (sign product)**
  Even total reversals => FORWARD; odd => BACKWARD.
Cases (Rodney):
- everything fwd, playhead reverses: (-)(+)(+) = BACKWARD (whole ensemble dragged backward).
- a lane previously reading BACKWARD (lane-rev, voice-fwd, playhead-fwd = backward), THEN playhead
  reverses: (-)(+)(-) = FORWARD — the backward lane now reads FORWARD. Two reversals cancel.
So reversing the playhead FLIPS every lane's effective direction (backward lanes become forward and
vice versa) — NOT "everything becomes backward". That sign-cancellation is the proof it's
multiplication, not "any reverse = backward".
Note the axes: playhead dir is the TIMING axis (the ring, phase-backward); voice/lane dir are the
READ axis. They're distinct axes but multiply into ONE effective read direction at the final step.
A distinctive case: ring spinning BACKWARD (phase) x a REVERSED voice = FORWARD reads on a backward
playhead. Associative + commutative + reversible (flip any/all signs).

### PING-PONG / PENDULUM — instantaneous sign multiplication
Ping-pong/pendulum is a direction that FLIPS at the window boundary, so direction per level is an
INSTANTANEOUS sign (forward/reverse = constant; ping-pong = +/- depending on which leg it's on). The
composition still holds as an INSTANTANEOUS product:
  **effective_dir(t) = playhead_dir(t) x voice_dir(t) x lane_dir(t)**  (each factor = that level's
  current sign). Each ping-pong level flips at ITS OWN window boundary (voice at voice edges, lane at
  lane edges, playhead at global edges). Multiple pendulums flipping at their own boundaries produce an
  intricate interleaved forward/backward pattern (the product of their phases) — rich, and reversible.

**Endpoint-direction rule (Rodney) — makes the sign DEFINITE at turnarounds:** at a turnaround the
flip involves two endpoints; assign the FIRST endpoint the PRE-endpoint direction (the way you arrived)
and the SECOND endpoint the POST-endpoint direction (the way you leave after the flip) — EACH IN ITS
OWN FRAME OF REFERENCE (each level applies this to its own window/direction independently). This makes
the instantaneous sign unambiguous at every instant INCLUDING endpoints, so the product is well-defined
even when multiple levels turn around simultaneously (each has a definite own-frame sign). It also fixes
the repeat-endpoint ambiguity: the turnaround is two endpoint-reads with distinct (pre/post) directions,
not one doubled read. Reversible: the assignment is deterministic and symmetric under scrub, so the
round-trip retraces endpoint directions exactly.

**Two turnaround STYLES, both covered by the pre/post rule:**
- **PING-PONG (double-tap):** endpoint read TWICE — ...10,11,[12 pre][12 post],11,10... The two
  endpoint reads are the first (pre-direction) and second (post-direction) of the rule. +1 step per
  turnaround.
- **PENDULUM (no double-tap):** endpoint read ONCE — ...10,11,[12 pre],11,10... = ping-pong with the
  SECOND (post-direction) endpoint read OMITTED. The single endpoint carries the PRE-direction (the
  first-endpoint rule); the post-direction resumes at the NEXT inward step. No extra step.
So pendulum = "ping-pong minus the second endpoint read." The pre/post framing defines WHICH read is
which; pendulum just drops the post read. Step counts differ (ping-pong +1/turnaround) → feeds the
polymeter when levels use different styles; the instantaneous sign-product composition holds for BOTH
(the single pendulum endpoint's sign = pre-direction). Reversible for both IF the drop is applied
consistently forward and back (same frame rule), so scrub retraces single-vs-double endpoints exactly.
Turnaround style is a per-level setting.

### Offset (the O in LOR) — nested, relative to parent read position
Offset is the usual LOR O (NOT the range start). Range = {where the window is, length}; OFFSET = where
within/along that window the read starts, relative to the PARENT's read position:
- VOICE offset = where the voice is reading when GLOBAL is at its range start.
- LANE offset = where the lane is reading relative to the VOICE's offset position.
Example (Rodney): playhead 1..16; voice range FIXED 5..12; voice offset 2 => when playhead at 1 the
voice reads 7 (5+2); as playhead advances the voice advances within 5..12; reaching 12 it WRAPS to 5.
Offset 0 => voice starts at 5. So offset slides the read phase WITHIN the fixed window (wraps at the
window boundary), independent of the range (which sets the window + wrap points).

### UI legibility for composed direction
Composed direction can surprise (set lane forward, reverse voice, lane plays backward — correct but
non-obvious). The ANIMATED voice-range lane handles this: the playhead visibly moves backward across
the window when the voice is reversed, and the probability-lane playheads move backward too — so the
EFFECTIVE (composed) direction is shown directly by the animation, not just inferred from a setting.
Show the voice's direction as the arrow on the voice-range lane; the lanes' real-time direction is
then self-evident from their playhead motion.

## Delegation + placement — REUSE the spread two-module model
Same architecture as spread (Macro broad/global, East fine/per-voice, per-voice delegation):
- **Macro = GLOBAL voice range** (all voices share one range).
- **East = PER-VOICE voice range** (each voice its own L/O/R+direction).
- **Delegation:** per voice, follow Macro global range OR have independent East per-voice range — so
  some voices share a global frame (locked ensemble) while others break free (soloists/canon voices).
Implementationally cheap: reuses the existing spread delegation machinery, one axis over.
Available **out of Straits** (generation-level voice time-structure) and optionally **out of
Intertropical** (arrangement-level) — consistent with the generation/arrangement hierarchy.

## UI — a thin EXTRA LED LANE on Sands, constant block heights (Rodney)
Voice LOR has NO probability bar to show — it shows a RANGE. So:
- An extra THIN lane in the Sands LED grid, **constant block heights** (flat lit = in-range step;
  unlit/dim = out of range). Flatness distinguishes it from the probability-magnitude lanes.
- **Animates with the playhead** — the current voice read-position moves across its range like the
  lanes animate, so you SEE the voice reading its window in time.
- **Shows LOR modulation live** — as voice LOR (L/O/R+direction) is knob/CV-modulated, the lit range
  moves/resizes/reverses in real time.
- Reuses the existing lane rendering + animation (constant height instead of prob-mapped). Minimal
  height; NO shrinking the probability lanes, NO dropping Sands/helix graphics.
- Direction shown as an arrow / orientation on the range. Place at TOP of the grid to read as the
  frame the probability lanes nest within (optional: dim lane columns outside the voice window to make
  the nesting visceral).

## MUSICAL WINS — clear-eyed (what's genuinely new vs already reachable)
Already reachable (NOT new): per-voice per-lane LOR+direction in any combination; lane-vs-lane
polymeter (lanes at different lengths beat at their LCM); global offset/rotation/length (playhead,
the ring). So "polymeter" per se is NOT the new thing.

**The one irreplaceable new capability = the VOICE-LENGTH MODULUS: a common per-voice cycle ABOVE the
lanes that re-phrases ALL of a voice's lanes TOGETHER.** Per-lane LOR can't do this (no shared cycle
to re-base the lanes as a group). Static, it's a second-order refinement (grouped re-phasing). What
makes it FIRST-ORDER:

1. **MODULATED voice length = METRIC MODULATION.** e.g. start voice length 8, add 2 every bar or two
   (8->10->12...). The re-phasing point WALKS against the bar and the lanes progressively, and because
   the lanes beat WITHIN the voice cycle, the texture TRANSFORMS as the grouping grows — a developing
   arc, not static drift. Generative metric modulation (Carter/Reich-style regrouping). Only reachable
   via the voice modulus: modulating the voice length re-groups the lanes COHERENTLY (they stay
   related, the whole group re-phases); modulating lane lengths individually would de-sync them.
2. **Different voices re-phrasing SIMILAR material differently = HETEROPHONY.** Correlation (copula)
   makes voices play RELATED content; different per-voice voice-length (and different modulation of it)
   groups/phrases that shared material differently per voice. Same idea, stated at different metric
   groupings simultaneously = true heterophony / gamelan-style stratification (a core design goal).
   e.g. one voice phrases in 8s, another in 10s, another growing 8->12 — all the same correlated idea,
   re-phrased, evolving.

So the win is **modulatable per-voice GROUPING of CORRELATED material -> metric modulation + evolving
heterophony.** Fits the instrument's identity exactly: copula = related material (value axis);
voice-length modulus = that material re-phrased/grouped differently and evolvingly per voice.

**It is ANOTHER WAY OF TAMING THE CHAOS (Rodney).** Alongside correlation (tames value-chaos: voices
relate in what they play) and the grid (bounds timing-chaos), the voice modulus bounds/organises the
METRIC/PHRASING relationship: voices don't just scatter in grouping — they re-phrase a shared idea on
controlled, relatable, modulatable cycles. Order<->chaos on the grouping/phrasing axis. The
irreplaceable bit is the modulus; offset (phased-canon) and direction (retrograde/mirror) are the
read-relationship enrichments on top.

(Honesty caveat per the hard constraint: because voice LOR moves only the probability READ, these are
content/window-phase versions — heterophonic re-phrasing of READS on a shared grid, not onset-timing
displacement. Still the heterophony/metric-modulation class; scoped to the read axis.)

### THE PRECISE GAIN (Rodney) — two fixed constants of the lane beating become TUNABLE
The sharpest statement of what voice LOR buys, grounded in the behaviour:
- **BEFORE (lanes referenced to global):** lanes beat against EACH OTHER (different lane lengths ->
  LCM beating) phase-linked at GLOBAL STEP 1, and against the FIXED 16-step global frame. Two beating
  references, both fixed: lanes-vs-lanes (sync at global 1) and lanes-vs-16.
- **NOW (lanes referenced to the voice):** lanes beat against each other AND against the VOICE length,
  which is ANY value 1..16 (not just 16), phase-linked at VOICE-STEP-1 — whose position in the phrase
  is SELECTABLE via voice R.

So two previously-FIXED parameters of the lane polymeter become CONTROLS:
1. **Beating CONTAINER cycle: 16 (fixed) -> voice length 1..16 (tunable).** You choose the modulus the
   lanes nest within.
2. **Phase ANCHOR: global step 1 (fixed) -> voice-step-1 position (selectable via voice R).** You choose
   where the lanes' common phase reference falls, not just the downbeat.

Why it matters (the felt result): tight, FEELABLE ratios like 6-in-8 were UNREACHABLE before — a 6-lane
could only beat against 16 (LCM 48, a long loose cycle). 6-against-8 (LCM 24, tight, clearly felt) needs
the tunable voice length. So voice LOR doesn't just add beatings — it adds the USEFUL, tight,
feelable ones (6/8, 3/4, 5/8...) that beating-against-16 can't give. Example reads for a 6-lane in an
8-voice: 5,6,7,6,7,8,9,10 — you can FEEL the 6/8 lilt in how the probability reads recur (the
overlapping 5,6,7 -> 6,7,8). That felt groove is the payoff that the abstract 'metric modulation /
heterophony' framing names but the EAR confirms.
One-line: voice LOR makes the lane-beating CONTAINER (was fixed 16) and the phase ANCHOR (was fixed
global-1) into tunable controls -> lanes beat against any cycle length, synced at any point -> unlocks
the tight feelable polymetric ratios.

### Stated for the CORRELATION-aware reader (Rodney) — resync at different steps x +/- correlation
The sharpest musical framing: voices playing CORRELATED (positive OR negative) lane data can RESYNC
(re-anchor) their probability reads at DIFFERENT steps (voice range 1..16). Correlation relates WHAT the
voices read (agree / complement); the per-voice reset point relates WHEN they re-anchor. Combined:
- **Positive correlation + different reset steps** -> the SAME idea, re-anchored at staggered points ->
  CANON / round-like (agreeing material, phase-shifted by reset point).
- **Negative correlation + different reset steps** -> COMPLEMENTARY material, re-anchored at staggered
  points -> INTERLOCKING / HOCKET-like (complement, phase-shifted).
So the voice range adds a per-voice RESET-PHASE dimension ON TOP of the value-correlation axis, and
because correlation is +/- you get both canon (positive+offset) and hocket (negative+offset) from the
one mechanism — correlation SIGN x reset OFFSET. That is what "resync probability at different steps for
correlated voices" buys: the value axis (correlation) composed with a reset-phase axis (voice range).

## Reversibility
Each level (global/voice/lane) is a deterministic LOR transform of the counter-addressed spine. Forward
= compose the maps; reverse = compose the inverses. Same reversibility the existing lane LOR already
has, just two (or three) composed. Validate round-trip bit-exact as standard.

## Build path (de-risked, post-release)
1. **Refactor existing global + lane into the unified nested-map model** (lane LOR re-based to
   voice-step origin; voice map at identity). Bit-compare: identical reads to today. NO new behaviour.
2. **Add voice LOR** as the global→voice map (identity by default). Per-voice L/O/R+direction, Macro
   global / East per-voice + delegation (reuse spread machinery).
3. **UI:** the thin constant-block animated voice-range lane on Sands.
4. Hold the HARD CONSTRAINT throughout: addressing/read only — no gate/generation/correlation changes.
5. Reversibility round-trip + bit-compare at each step.

## Why this is the right next consolidation
Correlation unified the VALUE relationship; this unifies the ADDRESSING (global/voice/lane as one
nested-map system) and adds the TIME relationship (voice LOR) as the orthogonal complement. Two axes
— value (correlation) × time (voice LOR L/O/R+direction) — each with Macro-global/East-per-voice
delegation, each reversible, spanning the full field of how an ensemble's voices relate. The forms it
reaches (staggering, polymeter, phase, retrograde/mirror) are compositional techniques, not
elaborations. Conceptually cheap (UI thin lane; compute read-only; delegation reused); the care is the
nested-map composition semantics and holding the no-logic-outside-read constraint.

---

## ABSOLUTE MODE — final (Rodney): same mappings as CYCLE, just gate the output
A per-voice MODE flag (HP, near delegation/direction): CYCLE (default) vs ABSOLUTE. ABSOLUTE is
DEAD SIMPLE and supersedes all earlier elaborations (R-as-sync-point, read-roams-free, etc. — DROPPED):

**Keep the EXACT cycle-mode read mappings (global->voice->lane, voice R, lane R referenced to
voice-step-1 — all unchanged, see the nesting table above). Absolute adds ONLY an output gate:**
1. **Zero the voice's GATE and CV OUT when the GLOBAL step is OUTSIDE the voice range** (outside
   [O, O+L) of the voice window — global-position space). CV OUT = the voice's OUTPUT (pitch/CV to the
   patch), zeroed so the voice emits NOTHING outside its region. **NOT the modulation CV INPUTS**
   (spread/correlation/etc. CV in) — those are UNTOUCHED (absolute mode never touches generation or
   correlation; it is purely an output gate+zero on the voice's outputs).
2. **Stop any gate still HIGH at the range END — but respect PHRASE WRAP (Rodney).** The cut is at the
   range's ACTUAL end boundary, NOT mechanically at step 16:
   - Range does NOT wrap the phrase (e.g. 5..12, inside 1..16): cut the gate at the range end (12).
   - Range WRAPS the phrase boundary (e.g. 13..4 = 13,14,15,16,1,2,3,4): step 16->1 is INSIDE the range,
     so do NOT cut at step 16 — the gate carries across the 16->1 wrap normally. Only cut at the range's
     real end (here step 4).
   - **Only cut at step 16 if step 1 is OUTSIDE the range** (range ends at/before 16, doesn't include 1).
   - At a wrap that is inside the range, follow NORMAL phrase-wrap behaviour including the applicable
     context-menu options (tie-across-rest etc.) — same as cycle mode across 16->1. The range end is a
     hard edge; the phrase boundary is only an edge when the range doesn't span it.

That's it. Reads are computed everywhere (cycle mappings run continuously, nothing internal changes);
output is gated to global ∈ [O, O+L) with a hard gate-stop at range end.
- OUTPUT-gate only => marginals/correlation UNTOUCHED (NOT probability-modifying). Safe output-stage mute.
- No roaming reads, no R re-interpretation, no sync-point logic — the earlier convolution came entirely
  from trying to make the READS special in absolute mode; they are NOT special, only the output is gated.
Payoff: per-voice SOUND REGIONS -> entrances/exits, builds, drops, call-and-response, voices occupying
different regions of the phrase = arrangement-level structure (not otherwise reachable; rests give
probabilistic silence, not a clean "active only here" region).



## ABSOLUTE 2 & 3 — per-step play/mute MASK (added output layer; static vs follows-rotation; Rodney)
An ADDED output layer that REPLACES NOTHING — stacks on top of the single range + the reads. Output
emits only if it passes ALL gates: within the voice range (if absolute 1 is active) AND the mask cell is
"play". Purely additive; the probability reading / addressing / generation / correlation are untouched
(same safe output-stage mechanism as absolute 1, just a per-step mask instead of a single range test).
- **Per-step play/mute mask over the 16 steps** — painted via a click-toggle UI action on the RANGE
  LANE's 16 cells (puts the bars to real interactive use; lit=play, dim=mute). Any subset, not just a
  contiguous run.
- **Delegation (existing model):** GLOBAL mask = Sands MACRO (all voices); PER-VOICE mask = Sands EAST
  (displayed voice). Same Macro/East split as spread/LOR/range.
- **Musical use:** hand-painted RHYTHMIC gating of generative content per voice — deterministic WHEN,
  generative WHAT. Per-voice masks across the ensemble = interlocking rhythms (hocket-by-mask).

Same phrase-wrap / gate-cut-at-boundary rules as absolute 1 apply at mute-cell edges.

**ABSOLUTE 2 vs ABSOLUTE 3 — the mask is STATIC vs FOLLOWS ROTATION (two distinct MODES, Rodney):**
- **ABSOLUTE 2 — STATIC mask:** fixed to absolute phrase-step positions (steps 1,4,7 always play
  regardless of R). A fixed rhythmic gate; modulating R shifts the CONTENT through the fixed mask,
  rhythm stays put.
- **ABSOLUTE 3 — mask FOLLOWS range ROTATION (R):** the mask is defined relative to the voice's
  reset/phase, so it SHIFTS WITH the phase reset points — modulating R slides rhythm AND content
  together. The rhythmic figure becomes part of the phased voice (travels with the reset) — richer for
  the correlation x reset-phase textures (canon/hocket where the RHYTHM also phases, not just pitch).
Split into two modes (not a sub-toggle) so each has one unambiguous behaviour; the mode switch just
steps through cycle / absolute 1 / absolute 2 / absolute 3.

## THE OUTPUT MODES ARE A CONTAINMENT HIERARCHY — implement as ONE mechanism (Rodney)
The modes are NOT four parallel things — each is a SUBSET of the next (even cycle, the original, is a
subset):
  **Cycle  subset of  Absolute 1  subset of  Absolute 2  subset of  Absolute 3**
- **Cycle** = mask ALL-ON (everything sounds, no gating).
- **Absolute 1** = mask is a single CONTIGUOUS RUN (editable as O/L).
- **Absolute 2** = ARBITRARY static mask.
- **Absolute 3** = arbitrary mask + FOLLOWS-ROTATION.

So implement ONE output layer: **a per-step play/mute MASK + a FOLLOW-ROTATION flag.** The "modes" are
just configurations of it, not separate code paths:
- mask all-on  -> cycle
- mask contiguous run -> absolute 1
- mask arbitrary, follow-R off -> absolute 2
- mask arbitrary, follow-R on  -> absolute 3
REFINEMENT (Rodney): the modes differ in WHAT THE MASK IS and WHAT IT FOLLOWS:
- **Cycle** = all-on mask.
- **Absolute 1** = the mask IS the O/L range (contiguous) — a UI action sets the mask to play inside
  [O,O+L), mute outside. It FOLLOWS L/O: modulating O or L moves/resizes the play-region with the range.
  It does NOT follow R. (The mask is DERIVED from O/L, so it tracks them by definition.)
- **Absolute 2** = arbitrary PAINTED mask, STATIC (follows nothing — fixed to absolute phrase steps).
- **Absolute 3** = arbitrary PAINTED mask, FOLLOWS R (rotation / phase reset).

CLEANEST FRAMING (Rodney): the real distinction is WHAT VARIES — mask SIZE vs mask ROTATION:
- **Absolute 1 = a DYNAMICALLY-SIZED mask.** Its size/position is a function of O/L, so modulating O/L
  RESIZES/MOVES it. Dynamic geometry (recomputed as O/L change). Contiguous run = the range-as-mask.
- **Absolute 2 & 3 = FIXED masks, but ROTATABLE.** The mask SHAPE is fixed (the painted play/mute
  pattern doesn't change size); only its ROTATION can vary. Abs2 = rotation static (fixed); abs3 =
  rotation follows R.

So TWO mask KINDS:
1. **Dynamically-sized** (abs1): computed from O/L each change — extent tracks the range.
2. **Fixed rotatable** (abs2/abs3): stored painted pattern + a rotation offset; rotation SOURCE = static
   (abs2) or follows-R (abs3).
Abs2 & abs3 thus collapse to ONE kind ("fixed rotatable mask") with a rotation-source flag (static /
follows-R). Cycle = the all-on degenerate full mask. Net structure: two mask kinds (dynamically-sized
vs fixed-rotatable), the fixed-rotatable one having a rotation-source sub-choice.

**UI implication:** possibly NO explicit 4-way mode switch — paint the mask + one follow-R toggle, and
the behaviour follows from what's painted (all-on = cycle, contiguous = abs1, arbitrary = abs2/3).
Optionally offer named PRESETS (set-all-on, set-single-range) as conveniences, but the underlying code
is ONE path (one mask gate + one flag), not four. Big simplification: four modes collapse to one
mechanism with two degrees of freedom (mask pattern, follow-R).

## MASK <-> ROTATION ANCHORING (Rodney) — base re-anchors, modulation moves relative to it
How the fixed rotatable mask (abs2/abs3) connects to rotation. PER STEP:
1. **Check whether the UNMODULATED (base knob) rotation has changed** — i.e. has the anchor link
   between MASK-STEP-1 and the base rotation value changed?
2. **If the base changed -> UPDATE THE ANCHOR** (re-pin mask-step-1 <-> new base rotation; move the mask
   by that change).
3. **Then FOLLOW THE MODULATION** — apply the current rotation MODULATION (CV) relative to the
   (possibly-updated) anchor.
4. **Always reflect the LATEST anchor + latest rotation mod:**
   mask position = latest_base_anchor + current_modulation_offset.

Two layers acting on the mask:
- **Anchor** = mask-step-1 pinned to the BASE (unmodulated) rotation; re-pins ONLY when the base knob
  changes (per-step change detection). Stable between knob changes (no jitter).
- **Modulation** = the CV rotation offset, applied relative to the current anchor, continuously.

Behaviour: base static + mod static -> mask at anchor, stable. Turn the knob -> anchor re-pins to new
base, mask moves there (mod offsets from there). CV-modulate rotation -> mask moves by the mod relative
to the current anchor. Both -> anchor tracks base (re-pin on change), mod offsets from the latest anchor.

This resolves the earlier "establish the link first, then move together" question: the BASE establishes
(and re-establishes on change) the anchor; the MODULATION moves the mask relative to it. No separate
reset/phrase-boundary decision needed — base-change detection IS the re-anchor trigger.

## MUSICAL PAYOFF OF THE MASK MODES — "rhythmic in and out" = the PRESENCE / STRUCTURE axis (Rodney)
The extra absolute modes open a distinct musical register: **rhythmic in and out** — voices rhythmically
ENTERING and LEAVING on a hand-shaped pattern (not probabilistic dropout). Plain working term: "rhythmic
in and out". (Snappier names if wanted later for manual/marketing: "rhythmic framing", "presence
masking"; technical: "rhythmic gating / presence sequencing".)

What it spans (via the three mask kinds):
- **Static mask (abs2):** a fixed rhythmic in/out figure (voice plays on these steps, absent otherwise).
- **Rotatable mask (abs3):** the in/out figure MOVES/EVOLVES as rotation modulates.
- **Dynamically-sized mask (abs1):** swell a voice IN / fade OUT by widening/narrowing its range (density).
- **Per-voice masks across the ensemble:** interlocking entrances/exits -> HOCKET, call-and-response,
  builds, breakdowns, drops.

Why it's a NEW register, not just "mute steps": rests give PROBABILISTIC sparseness (generative texture);
the mask gives DETERMINISTIC, COMPOSED rhythmic presence/absence (arrangement/structure). Different jobs.
Combined: **deterministic WHEN (the mask) x generative WHAT (the engine)** — you COMPOSE the rhythmic
architecture (who's in when), the engine fills it with correlated generative content. This directly
addresses the common criticism that generative music meanders without structure: the user gets composed
STRUCTURAL control over presence while content stays generative.

**Best framing — the PRESENCE / STRUCTURE axis of the instrument's order<->chaos family:** the mask is
the FOURTH composed-control dimension alongside the existing three:
- Correlation (copula) -> VALUE relationships.
- Grid -> TIMING floor.
- Voice-length nesting -> METRIC / phrasing.
- **Mask -> PRESENCE / STRUCTURE** (rhythmic in and out — who is present when).
So "rhythmic in and out" isn't a bolt-on feature; it's the instrument's composed-control-over-generative
thesis extended to the PRESENCE axis — compose who's present when, the engine generates what.
---

## KNOWN ISSUE to fix WITH this system — global length/offset doesn't wrap the Sands read (Rodney)
Observed: changing the GLOBAL playhead length/offset (PATTERN_LENGTH_PARAM / global offset) wraps the
PLAYHEAD (it cycles fewer steps), but Sands keeps READING/showing the FULL 16-step probability range
instead of wrapping to the global window. So playhead wraps, Sands read/display does NOT — inconsistent.
Root: the GLOBAL-level LOR (top of the nested global->voice->lane chain) isn't propagating to the Sands
read/display; the global window should BOUND the reads beneath it.
Fix this as PART OF the unified addressing implementation (the nested maps), NOT as an ad-hoc patch now:
it's the same mechanism (global LOR wrapping the read), the addressing work rebuilds this read-position
logic anyway, and the correct wrap falls out of implementing the nested maps properly.
Sub-question to settle then: should Sands DISPLAY the full 16 with the active window highlighted, or only
the active window? (Ties to the adapt-to-length vs fixed display decision — see STEP_COUNT_DECISION.md /
LIGHT_RING_32_STEPS.md.)
