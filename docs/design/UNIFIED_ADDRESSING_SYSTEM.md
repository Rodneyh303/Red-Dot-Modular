# Unified pattern-addressing system (global / voice / lane) + voice LOR

STATUS: design direction, POST-RELEASE. Major but conceptually clean. This is the next natural
consolidation (like copula unified seq/quant, correlation unified per-voice value): pattern ADDRESSING
is the subsystem not yet unified. Capturing the full worked-through design so it isn't re-derived.

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
