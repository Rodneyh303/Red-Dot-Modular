# CC BRIEF — collapse mono/poly to [16][16] (V1=index 0) — fixes the voice-2 boundary bugs at the ROOT

Two targeted attempts at the voice-2 bugs (melody-bar leak + accent pendulum) MISSED, because they're not
ONE off-by-one — they're the mono/poly DUAL INDEXING creating many boundary/alias hazards. Fix the ROOT.
Full catalogue + categories: docs/design/COLLAPSE_15_TO_16.md (read it). This brief = the actionable pass.

## The root (why the voice-2 bugs keep slipping)
The engine stores per-voice data in TWO shapes with a boundary between them:
- MONO scalar arrays (slewedMelody[16], melodyRandom[16], ...) = V1.
- POLY arrays slewedPoly*[15][16] / random_[... ] = V2..V16, 0-based (poly[0]=V2).
- A MONO ALIAS at publish: pubSlewedPoly*[0] = slewed*(mono)=V1; pubSlewedPoly*[v]=slewedPoly*[v-1] for
  v=1..15 (PatternEngine.hpp ~550). Plus scattered voice+1 / v-1 shifts (SpreadInterp voice+1, SeqEng:537
  polyRandom(voiceIdx+1), laneDirV_[15] 0-based).
Every per-voice READ that crosses V1->V2 must get the convention right; the FIRST poly voice (V2) is where
+1/-1 slips land -> V2 reads V1/mono data. That's BOTH bugs (V2 melody = mono melody; V2 accent = V1 dir).

## The fix — ONE uniform [16][16], V1 = index 0 (Category 1 of COLLAPSE_15_TO_16.md)
Collapse each mono-scalar + poly-[15][16] PAIR into ONE [16][16] (or extend the existing 16-row random_
pattern to ALL per-voice arrays): V1 at [0], V2..V16 at [1..15]. Then:
- DELETE the mono scalar twins (slewedMelody[16] etc.) — V1 lives at index [0] of the unified array.
- DELETE the pubSlewedPoly*[0]=mono ALIAS and the pub[v]=poly[v-1] shift — pub[v] = unified[v] directly
  (V1=0, V2=1, ...). No alias, no shift.
- REMOVE the voice+1 / voiceIdx+1 / v-1 shifts (SpreadInterp, SeqEng:537, publish loops) — every per-voice
  access is unified[voiceSlot] with voiceSlot = 0(V1),1(V2),... CONSISTENTLY.
- laneDirV_[15] -> laneDirV_[16] (V1 at [0]); audit its set-vs-read so V2 = [1] both sides.
- [15] -> [16] on the per-voice data arrays; fix the feeding loops to <16 (the data ones; see Cat 3).
- Leave Category 2 untouched (POLY_*_PARAM_1..15 enums, Straits binding, panel generators — intentional
  V1-on-Monsoon; param/anchor layer unchanged, no preset/UI migration).

## Why this fixes the voice-2 bugs
With one [16][16] (V1=0, V2=1) and NO alias/shift, there is NO boundary to slip at: V2 = index [1]
everywhere, reading V2's own data. The melody-bar leak (V2 was reading pub[0]=mono) and the accent pendulum
(V2 was reading V1's laneDir) both vanish because the aliasing/shift that caused them is gone.

## Verify (CRITICAL — the baseline is CORRECT voices, not the buggy V2)
- BIT-COMPARE V1 and V3..V16: output IDENTICAL before/after (they were already correct; relocating storage
  must not change them). This is the bit-compare gate.
- V2 (the BUGGY voice): output CHANGES — to the CORRECT value. This is EXPECTED (not a regression):
  * rhythm-mix modulation does NOT move V2 melody bars; V2 shows its OWN melody (melody-mix moves V2 only).
  * V2 accent set to Forward STEPS FORWARD (not pendulum); V3/V4 unchanged; V1 pendulum unaffected.
- All 3 platforms green. Behavioural checks in Rack (display/direction bugs aren't caught by the suite).

## Method
Mechanical relabelling (like lane-index): one array/pair at a time, build + bit-compare (V1,V3-16) + Rack
check (V2), commit + PUSH. Chase EVERY [15]/<15/voice+1/v-1/pub[0]-alias site (grep: `[15]`, `< 15`,
`voice + 1`, `v-1`, `pubSlewedPoly*[0]`, `laneDirV_`). The audit_anchor_bind.py + bit-compare guard it.
KEEP ac33f157 (CC's SpreadInterp torn-read fix — correct hygiene; just move its indices to the unified
[16][16]).
