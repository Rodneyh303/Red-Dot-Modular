# BUG (TWO symptoms, ONE root) — voice 2 (first poly) behaves like V1. Mono/poly index off-by-one.

Both bugs are ONLY voice 2 (first poly voice); V3/V4 fine. That = a +1/-1 slip at the V1->V2 boundary.

## CONFIRMED convention (the source of truth)
- random_[voiceSlot][lane]: voiceSlot is DIRECT, 0-based with **0 = V1, 1 = V2, ..., 15 = V16** (16-row
  unified array). polyRandom(voiceSlot, lane) = random_[voiceSlot][...] — NO internal -1.
  (PatternEngine.hpp:175.)
- SequencerEngine.hpp:537 calls pe.polyRandom(voiceIdx + 1, ...) — its voiceIdx is 0-based POLY (0=V2), +1
  -> voiceSlot (1=V2). CORRECT.
- Published arrays: pubSlewedPolyMelody[0]=V1(mono), [v]=slewedPolyMelody[v-1] for v=1..15 -> pub index v =
  V(v+1). (PatternEngine.hpp:550.) Another +1/-1 boundary.
- Per-voice direction: laneDirV_[15] is 0-based POLY (0=V2). (SequencerEngine.hpp:291.)

## BUG 1 — rhythm-mix -> voice-2 MELODY bars leak/jump
Display read (PatternEngine.cpp:714/720): polyRandom(displayedVoice_, PL_MELODY). displayedVoice_ =
selectedVoice (StraitsEastSandsVisual.cpp:1023), direct from the UI tab.
THE SLIP: is selectedVoice in voiceSlot space (0=V1,1=V2,2=V3) or UI-label space ("voice 2" = first poly)?
- polyRandom expects voiceSlot (0=V1,1=V2). If the UI's "voice 2" sets displayedVoice_=2, then
  polyRandom(2)=random_[2]=V3 -> voice 2 shows V3; and the V1->V2 boundary + the pubSlewedPolyMelody[0]=mono
  alias make the first poly read mono/adjacent data -> JUMPS when rhythm-mix forces a refresh.
CONFIRM: what integer does selectedVoice hold for the first poly voice? (0=V1 so first poly=1, or labelled
2?) Make displayedVoice_ consistently voiceSlot (0=V1, 1=V2, ...) at the SET site (1023) AND the read
(714/720). If selectedVoice is UI-label (V1=1, V2=2...), convert: displayedVoice_ = selectedVoice - 1.

## BUG 2 — V2 ACCENT lane set to Forward but behaves like V1's Pendulum; V3/V4 fine
Same boundary. V2's accent DIRECTION is reading V1's direction (pendulum) instead of its own (forward).
laneDirV_[15] is 0-based poly (0=V2). The read that drives V2's accent direction is using the wrong voice
index at the boundary (pulling the mono/V1 direction, or index-shifted by one). CONFIRM: trace where V2's
accent lane direction is read for stepping (recomp(laneDirV_[v][l]...) SequencerEngine.cpp:250/344) and
where the UI SETS V2's accent direction (laneDirVPending_) — do the SET index and the READ index agree for
V2? The V2-only + V1-pendulum-copied symptom = V2 reading laneDirV_ of V1 (or an index off by one so V2
inherits V1's auto-flip state).

## ROOT (both) — the mono/poly DUAL INDEXING (voiceSlot 0=V1 vs poly-array 0=V2, with scattered +1/-1)
This is exactly the accretion the [15][16]+mono-twin split creates (COLLAPSE_15_TO_16.md). Every per-voice
access that crosses the V1->V2 boundary via a different convention slips HERE. The [16][16] collapse (V1=0
uniformly, no poly-array[-1] shift, no mono alias) would REMOVE this whole bug class. For NOW: make the
voice index CONSISTENT (pick voiceSlot 0=V1 everywhere; audit every +1/-1 and the pubPoly[0]=mono alias).

## Fix + verify
- Pick ONE convention (voiceSlot: 0=V1, 1=V2, ...). Audit: displayedVoice_ set-vs-read; the pub...[0]=mono
  alias + pub[v]=poly[v-1]; polyRandom callers (+1 at 537); laneDirV_ set-vs-read for V2.
- Verify: rhythm-mix does NOT move voice-2 melody bars; voice-2 shows ITS OWN melody (scrub melody-mix ->
  voice2 bars move, V1/V3 don't). V2 accent = Forward actually steps forward (not pendulum). V3/V4 unchanged.
- Strongly consider doing the [16][16] collapse to kill the class (it's the real root).
