# V16 poly-bar flicker — ROOT SMELL FOUND: VoiceResolver::polyBankIndex LIES about its return
After 3 static-fix attempts failed (two of mine were effectively NO-OPS — see below), the audit found WHY
nobody can reason about this correctly: the convention source-of-truth contradicts ITSELF.

## The lying abstraction (VoiceResolver.hpp)
```
line 41:  polyBankIndex(int v) { return isPoly(v) ? (v - 1) : -1; }   // CODE: V16(16) -> 15
line  5:  // "Poly voiceSlot (1..15) for V2..V16 — same as voiceSlot for poly voices"   <-- says ==voiceSlot
line 18:  // "polyBankIndex(v) — 15-WIDE, poly-only. bank 0 = V2"                        <-- says 0=V2 (v-2!)
line 56:  // "voiceSlot(v) == polyBankIndex(v) + 1"                                       <-- says +1 apart
line 60:  voiceSlot(int v) { return v - kMonoVoice; }  kMonoVoice=1  // V16(16) -> 15
```
CONTRADICTION:
- line 5 comment: polyBankIndex == voiceSlot for poly.  (TRUE per the code: both = v-1 = 15 for V16)
- line 18 + line 56 comments: polyBankIndex is "0=V2" / "voiceSlot = polyBankIndex+1".  (FALSE per the code)
- The NAME "polyBankIndex" and line 18/56 imply 0-based poly (V2=0, V16=14 = v-2).
- The CODE returns v-1 (V2=1, V16=15), which is voiceSlot-for-poly, NOT 0-based poly bank.
So polyBankIndex() does NOT return a 0-based poly bank. It returns (v-1) = voiceSlot value, but is NAMED and
PARTLY DOCUMENTED as if it returns a 0-based bank. **The function lies.** Every reader who trusts the name/
line-18 comment reasons wrong; every reader who happens to match the actual (v-1) return works by luck.

## Why my fixes failed (and were no-ops)
- polyVoice() = polyBankIndex(currentVoice()).  currentSlot() = voiceSlot(currentVoice()).
- For POLY voices both = v-1 -> **polyVoice() == currentSlot() IDENTICALLY.**
- So changing `polyVoice()` -> `currentSlot()` (26fa2a3c spread-arc; aeb7a8b8 bar-draw) changed NOTHING.
  Harmless but pointless. The real downstream arrays (pubSlewedPoly*, laneTickV_) are [16] voiceSlot-indexed
  and were ALREADY getting the correct index (15 for V16) from polyVoice(). So the flicker is NOT a V16
  index-15-vs-14 error at those sites. We were chasing the wrong class entirely.

## So what IS the flicker? (NOT yet found — needs Rack, not static)
Since V16 DOES read slot 15 correctly, the flicker is one of:
  (a) a DIFFERENT accessor that genuinely takes 0-based-poly (v-2) somewhere, mismatching, OR
  (b) a TWO-WRITER / timing issue on slot 15 (the value at [15] alternates), OR
  (c) a render/highlight issue (playhead or color), not the data value.
3 static attempts + 5 measured rounds haven't isolated it -> it resists both. Likely (b) or (c): a timing/
render thing that static reading can't see and that perturbs under measurement.

## THE ACTUAL FIX PATH (rested + Rack, not more guessing)
1. FIRST fix the LIE: make VoiceResolver self-consistent. Decide what polyBankIndex SHOULD mean, then make
   name + all 3 comments + the code agree. (It currently returns v-1 = voiceSlot-for-poly. Either rename it
   to `polyVoiceSlot` and delete the "0=V2/+1" comments, OR change it to v-2 and fix every caller.) Until the
   abstraction stops lying, NOBODY can reason about callers correctly — this is the root smell that made the
   whole session's voice bugs so hard.
2. THEN, with a truthful convention, audit callers (grep polyBankIndex/polyVoice/voiceSlot/currentSlot) —
   each now checkable against the ONE true meaning.
3. For the flicker specifically: instrument slot 15 at the draw AND confirm (a)/(b)/(c) by reading the value
   in Rack (does pubSlewedPolyMelody[15] itself change frame-to-frame? -> two-writer; or is it steady but
   the bar still flickers? -> render/playhead).

## Status
- Flicker: COSMETIC (V16 audio is correct). NOT blocking anything. Safe to park indefinitely.
- My 2 fix commits (26fa2a3c, aeb7a8b8): no-ops (polyVoice==currentSlot for poly), harmless, can stay or revert.
- The real work is resolving the lying VoiceResolver abstraction — a rested, Rack-in-hand task, NOT another
  tired static guess. 3 near-misses today prove static reasoning fails while the source contradicts itself.
