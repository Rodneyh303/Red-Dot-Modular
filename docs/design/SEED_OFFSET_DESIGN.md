# Seed offset feature -- design (given the counter structure)

## The counter structure (from PatternEngine.hpp) -- makes this EASY
- rhythmDrawCtr / melodyDrawCtr are int64_t SIGNED. "Philox is a keyed bijection over the full signed
  counter space, so ANY index is a valid reproducible draw." (PatternEngine comment.)
- Draw N = the addressable block [N*CHUNK, N*CHUNK+CHUNK), CHUNK=1024 -- a PURE FUNCTION of
  (counter, key). NO stored history: draw 10 is directly computable without drawing 0..9 first.
- Existing primitives: zeroRhythmIndex() (=0), advanceRhythmDraw(dir) (+-1). NO setIndex(n) yet --
  but adding one is trivial (the counter is just an addressable int). The comment literally calls this
  the "Mode E reverse/JUMP foundation" -- the jump was anticipated.

=> The earlier "have to draw 10 times to reach counter 10" framing was about the missing CONTROL, not
an engine limit. The engine can already compute any draw index directly. The feature is purely: expose
the counter as a settable/offsettable value.

## KEY CONSEQUENCE: no edge cases (given the whole range)
Because the counter is SIGNED + UNBOUNDED (int64) + Philox is a bijection over the whole space:
- No wrapping, no boundary, no modulo, no "what happens at the end". offset +6 / +6000 / -6000 are all
  valid reproducible draws. The offset feature has NO edge cases. (Contrast a bounded ring counter,
  which would need modulo + boundary handling.) The signed-bijection design pays off exactly here.
- "The whole range" therefore does NOT mean "map a knob across 0..2^63". The offset is RELATIVE, not
  absolute -- a knob covers useful relative DISTANCES (a phrase or two), not the whole int64 space.

## DESIGN: one "SEED OFFSET" input, param + CV (relative signed offset)
Core: effectiveCtr = drawCtr + offset. A SIGNED value added to the counter before the draw.
- PARAM (knob): signed, modest range (e.g. -32..+32 draws) -- enough for canon alignment (a phrase or
  two apart). Fine detent at 0.
- CV input: voltage -> counter offset (scaled, e.g. 1V = some draws). Makes the position CV-ADDRESSABLE
  -- an LFO/ramp/sequencer can DRIVE the offset => navigable-probability becomes performable (slow ramp
  sweeps the timeline; LFO oscillates around a region; sequencer jumps between positions).
This single input covers all three use cases:
  1. CRAB / canon alignment: set a FIXED offset between two shared-seed Monsoons (one at +0, one at +6).
  2. SCRUBBABLE TIMELINE: drive the CV with an LFO/ramp -> sweep through probability space live.
  3. JUMP: snap the knob -> jump to a relative position.

Absolute "set counter = N" (jump-to-origin) is a nice context-menu EXTRA but NOT essential -- the
relative offset subsumes most of its use (origin = the other Monsoon's position, or the seed's 0).

## THE REAL DESIGN DECISION: unified vs independent (rhythm/melody)
The two streams have independent keys (rhythmKey=S, melodyKey=S+1). The offset can be:
- UNIFIED (RECOMMENDED default): one offset shifts BOTH rhythm and melody counters in lockstep -- the
  whole VOICE moves through its streams together. Correct for crab/canon (offset the whole voice, its
  rhythm AND pitch). Simplest.
- INDEPENDENT (power-user, later): separate rhythm-offset vs melody-offset -> a voice whose rhythmic and
  melodic material are drawn from DIFFERENT points in their streams. Exotic: rhythm/melody
  decorrelation. Context-menu split, not the default.
Ship UNIFIED as the primary control; independent is a possible later context-menu option.

## Selectable scrub distance (companion, see CRAB_CANON_RECIPE)
Add selectable scrub span (6/8/10/12 draws) so the dice-scrub crossfade / crab has a CHOSEN period
independent of accumulated history. offset = WHERE, scrub distance = HOW FAR. Param or context menu.

## Implementation sketch (Claude Code)
- PatternEngine: add setRhythmIndex(int64_t)/setMelodyIndex (trivial, mirror zeroRhythmIndex), and/or
  read an offset added at the draw-address site (effectiveCtr = drawCtr + offset). Prefer applying the
  offset at the ADDRESS site (draw N -> draw N+offset) so it composes with the existing counter without
  disturbing the reversible-mode +-1 stepping.
- Monsoon: SEED_OFFSET param + SEED_OFFSET_INPUT jack. Sum param+CV -> integer offset. Apply to both
  streams (unified). Panel: one knob + one jack (near the existing SEED controls).
- No edge cases to handle (bijection over signed space). Verify: two Monsoons same seed, offset one by
  N, confirm voice B = voice A shifted N draws (identical material, N-draw phase). Reversible mode still
  steps correctly with a non-zero offset.
- MUST-HAVE / 1.0, fast-follow after library (MASTER_PLAN item 16).

## TIMING + BASE/MOD LAYERING (refined)

### Range: +-32 (or +-64 headroom) is enough
Counter increments once per PHRASE (draw at phrase boundary). A crab offset = the phrase length of the
canon subject (typ. 4/8/16 phrases). +-32 covers canons up to 32 phrases apart -- generous; beyond that
the ear can't connect the voices AS a canon anyway. +-64 for headroom. Wider adds nothing usable; the
CV input (unbounded) covers any exotic case.

### Reset-to-sync is the KEYSTONE (defines the origin the offset is measured from)
Before a crab piece: RESET everything (the IT/engine reset zeros counters, syncs all instances to a
known origin -- both Monsoons at counter 0). THAT is the shared datum the offset is relative to. Without
it, "offset by 6" is ambiguous (6 relative to what?). Workflow:
  reset (all -> 0)  ->  base offset applied (B now effectively at 6)  ->  play (both advance lockstep,
  staying 6 apart).
The reset establishes the datum; the offset is measured from it. Reset is a PREREQUISITE for a coherent
crab, not just nice-to-have.

### BASE offset vs MOD offset -- two inputs, two policies (mirrors synth base-tune vs mod)
effectiveCtr = drawCtr + baseOffset + modOffset
- BASE OFFSET: a knob/setting = the STRUCTURAL canon interval ("B starts 6 phrases into the material").
  Set once at setup. LATCHED AT RESET/RESEED (not sampled per phrase) -- you don't change the canon
  interval mid-canon. Part of the patch's reproducible identity. Range +-32/+-64.
  (This is where Rodney's "only allow new offset at reseed" instinct correctly applies -- to the BASE.)
- MOD OFFSET: the CV input = the PERFORMANCE layer, added ON TOP. SAMPLED AT PHRASE BOUNDARY (Policy B,
  the existing rate discipline applied to this input), applied to the UPCOMING draw (causal, no
  retroactive mid-phrase recompute). Performable: sweep the crab crossing, scrub the timeline, add drift.
  (This is where "sample offset mod at phrase boundary" correctly applies -- to the MOD.)

This split IS the reconciliation of the earlier Policy-A-vs-B tension -- no mode toggle needed. Base =
stable/reproducible (A-like); Mod = performable/boundary-sampled (B-like). Each input gets the policy
that fits its role. It mirrors a synth's coarse-tune (structural, set once) vs mod-input (live) -- you
don't sweep coarse tune to play vibrato.

### Reproducibility
- Static patch (base latched, no mod CV) = fully reproducible: same seed + same base offset + reset =
  same output every time.
- CV-driven mod = output depends on the mod source (LFO phase etc.) -- by the user's choice, exactly
  like any CV modulation. Reproducible-when-static, performable-when-driven, from one design.

### Implementation refinement (Claude Code)
- Two contributions to the draw address: baseOffset (latched at reset/reseed) + modOffset (sampled at
  phrase boundary from param+CV). effectiveCtr = drawCtr + baseOffset + modOffset applied at the
  ADDRESS site (draw N -> draw N+base+mod), composing with reversible-mode +-1 stepping.
- Base offset: latch on reset trigger + on reseed. Mod offset: sample at phrase-boundary (the draw
  point), hold for the phrase, apply to the upcoming draw.
- UNIFIED across rhythm+melody (both streams, lockstep) as before.
- Panel: BASE OFFSET knob + MOD OFFSET CV jack (near SEED). Optionally one shared knob that acts as base
  with the jack as mod -- but two clearly-labelled controls is cleaner for the canon workflow.

## SIMPLIFICATION (decided): base offset at reset only -- NO CV mod of counter
Rethink prompted by: "we already have gates to move the counter back/forth; does CV mod add anything?
also the dice-scrub probability range depends on the counter." Conclusion: CV mod of the counter is
NOT worth building. The feature collapses to a static base offset applied at reset.

### Why NOT CV-mod the counter
- REDUNDANT with existing gates: forward dice (+1), backward dice (-1, reversible), and dice-scrub
  already give a complete, COHERENT vocabulary for MOVING the counter -- discrete, intentional,
  musically-timed integer steps. CV mod is a second, fuzzier way to do the same thing.
- ARBITRARY mapping: voltage->counter-position has no natural semantics (what position is 2.3V?), and
  CV noise = position jumps. Gates have unambiguous +1/-1 semantics; CV position does not.
- COLLIDES WITH SCRUB RANGE: the dice-scrub's probability range is defined RELATIVE to the current
  counter position. If CV is ALSO moving the counter, the scrub range moves too, and the two interact
  unpredictably -- two owners of the same coordinate with different semantics. This is complexity that
  produces "can't predict what it does", not "navigation".
=> Do NOT build CV mod of the counter. The gates already own the counter-MOVEMENT axis coherently; CV
mod would be a colliding second owner.

### What the feature actually IS: base offset at reset (static origin displacement)
NOT counter movement -- a static displacement of the ORIGIN. "When everything resets to 0, this Monsoon
resets to baseOffset instead." A one-time structural setting, latched at reset, establishing the canon
interval. Does not move during play, no scrub-range interaction, no voltage mapping.
- effectiveOrigin = baseOffset (applied when the reset zeros the counter: counter <- baseOffset, not 0).
- Static knob, range +-32/+-64, reproducible, part of patch identity.
- The crab workflow: reset (sync; each Monsoon's counter <- its baseOffset) -> both advance in lockstep
  via the NORMAL clock, staying baseOffset apart -> perform the crossing with the EXISTING dice-scrub
  crossfade. No new modulation surface.

### Net design (much smaller than the earlier param+CV version)
- BASE OFFSET knob, applied at reset (counter <- baseOffset instead of 0). Latched, reproducible. DONE
  = the whole feature.
- Counter MOVEMENT during play = existing gates (fwd/back dice, scrub). Already built.
- NO mod-offset CV, NO counter CV input. Rejected above.
- Still UNIFIED across rhythm+melody (both counters <- baseOffset at reset).
- Selectable scrub distance (6/8/10/12) remains a separate, independently-useful companion (shapes the
  scrub/crab period) -- that's about the SCRUB gesture, not counter CV, so it stands.
Impl: on reset trigger, set rhythmDrawCtr = melodyDrawCtr = baseOffset (instead of 0). One param, one
line in the reset path. That's it.
