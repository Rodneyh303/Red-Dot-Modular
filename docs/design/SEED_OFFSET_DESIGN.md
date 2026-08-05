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

## EXPANSION (open): three-stream offsets + where this lives
Rodney: prefer SEPARATE offsets for rhythm and melody; Change Alley RNG will also use Philox and MAY
want its own offset. So potentially THREE (or more) offsets: rhythm, melody, CA/correlation. Home
undecided (Monsoon vs revamped Raffles vs CA) -- documented as a direction, not a settled decision.

### Three-stream offsets (the principled design)
The seed derivation already makes three INDEPENDENT keyed streams: rhythmKey=S, melodyKey=S+1,
caKey=S+2. Independent streams => independent offsets is the CORRECT match (unified was just the simple
choice). Same primitive (base offset at reset) applied per stream:
- rhythmOffset : rhythm counter <- rhythmOffset at reset
- melodyOffset : melody counter <- melodyOffset at reset
- caOffset     : CA correlation counter <- caOffset at reset (OPTIONAL, default 0)
Not 3x the complexity -- the same one-line-at-reset primitive on each of three streams that already
exist independently.

### What separate offsets unlock (musically real, not just tidy)
- rhythmOffset != melodyOffset = a voice whose RHYTHM is drawn from one stream point and MELODY from
  another. Natural rhythm/melody DECORRELATION -- crab-canon the rhythm while the melody stays aligned,
  or vice versa. Independent contrapuntal treatment of the two dimensions.
- caOffset = offset the CORRELATION structure independently of content. Hold content fixed, scrub the
  correlation; or canon the correlation pattern itself. A genuinely new axis.
- CA "may or may not want offset": for a crab you usually want correlation SHARED + aligned (caOffset=0
  even when rhythm/melody are offset). But offsetting it is a valid texture. So caOffset EXISTS but
  defaults 0 / optional -- least-used of the three but real. (Rodney's instinct to make it separately
  controllable, not forced, is right.)

### WHERE IT LIVES -- open question (three candidates)
The offsets address Philox counters that live in DIFFERENT modules: rhythm/melody in PatternEngine
(inside Monsoon); CA correlation in Change Alley. So no single owner is obvious.
- MONSOON: natural for rhythm+melody (counters are here) but CA offset would have to reach into the CA
  expander; Monsoon panel already dense. Fits 2 of 3.
- CHANGE ALLEY: natural for caOffset (its counter) + thematically on-brand (CA is about correlation/
  probability relationships) but rhythm/melody counters aren't here. Fits 1 of 3.
- REVAMPED RAFFLES (strongest candidate to evaluate): Raffles is being cleaned up anyway (trial
  removal). It could become the "probability-navigation / seed & offset" CONTROLLER -- owning seed +
  the three offsets + selectable scrub distance, pushing to Monsoon (rhythm/melody) and CA
  (correlation). Why compelling:
    * THEMATIC FIT: Raffles = the raffle / the draw; seed+offset IS draw addressing. The name already
      means this.
    * Being revamped anyway -> room for a clear new PURPOSE instead of a diminished old one.
    * CENTRALISES the offset system in one module instead of scattering knobs across Monsoon + CA.
    * Gives the navigable-probability-space HEADLINE a physical home -- currently diffuse (a property
      of the counter); a dedicated module makes it a thing you can see/touch. Better for pitch + user
      understanding.
  Caution: expands Raffles from "cleanup" to "redesign as probability-navigation controller" -- more
  work, and needs seed+offset+scrub-distance concepts stable first.

### OPEN QUESTIONS to resolve before committing a home
1. Does Raffles' current Monsoon connection support pushing offset values to BOTH Monsoon AND Change
   Alley? Or does it need the rack-wide pairing scan (PAIRING_CROSS_ROW_NOTE) to reach CA?
2. Is turning Raffles into the probability-navigation module consistent with what Raffles should BE, or
   does it have a different identity worth keeping?
3. Does caOffset interact with the shared-CA (Option A) owner/reader model? If a CA is shared by two
   Monsoons, whose caOffset wins? (Likely the owner's -- consistent with applyPendingTransforms owner
   rule -- but confirm.)

STATUS: direction documented, home OPEN. "Still open but we will find the right home." Decide when the
seed/offset concepts are stable and Raffles' revamp scope is being set.

## OWNERSHIP: shared CA creates a "which Raffles" conflict -> offset-lives-with-its-key
Rodney: with a SHARED Change Alley, a CA offset on Raffles hits a "which Raffles?" problem -- two
Monsoons => two Raffles, each wanting to set the ONE shared CA's offset. The shared CA can only be at
one correlation-stream position, so two Raffles pushing different caOffsets is a genuine contradiction.

### The offsets have DIFFERENT ownership topologies (the key realisation)
- RHYTHM offset + MELODY offset = PER-MONSOON. Each Monsoon owns its rhythm/melody counters, so each
  Monsoon's Raffles setting its own rhythm/melody offset is unambiguous. NO conflict -- and different
  values are exactly what the crab WANTS (the canon interval).
- CA offset = PER-CA, and the CA may be SHARED. Its offset is a property of the shared RESOURCE, not of
  either Monsoon. A per-Monsoon Raffles owning it => the "which Raffles" conflict.

### Resolution: CA offset lives on the CHANGE ALLEY, not on Raffles
Put the CA offset on the CA itself (where its caKey counter lives). Then: one CA, one caOffset, no
ambiguity. Two Monsoons sharing the CA both read the same correlation at the same offset -- which is
what "shared" MEANS. Raffles isn't in the loop for the CA offset at all, so "which Raffles" dissolves.
Consistent with the existing shared-CA rule (applyPendingTransforms owner-only; readers read): caOffset
is the CA's own state, set on the CA, read by whoever's paired.

### Governing principle: OFFSET-LIVES-WITH-ITS-KEY
Seed derivation already puts the keys in different modules: rhythmKey/melodyKey are Monsoon's; caKey is
the CA's. So the OFFSETS living in the same places is consistent -- offset lives with its key. Trying to
put all three offsets on Raffles would separate the CA offset from the CA key, which is EXACTLY what
creates "which Raffles". Keeping offset+key together makes the conflict evaporate.

### Refined home (partially answers the earlier open question)
- Raffles (or Monsoon) = the PER-MONSOON navigation controller: rhythm + melody offsets for ITS Monsoon.
  No sharing conflict.
- Change Alley = owns the CORRELATION offset (caOffset). One per CA; shared CAs inherit it.
So Raffles is the per-VOICE navigator, the CA owns correlation addressing. Arguably MORE coherent than
one module owning all three -- each offset sits where its state actually lives.

### Remaining subtlety (note, don't solve now)
Per-Monsoon offset ownership means YOU maintain any cross-Monsoon offset RELATIONSHIP by hand (set two
Raffles to related values); the system doesn't enforce it. For the crab this is CORRECT (you want
explicit control of the interval). Just be clear: per-Monsoon = manual cross-instance relationship,
which is the same "patch the relationship you want" freedom as everywhere else.
