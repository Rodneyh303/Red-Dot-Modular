# Collapse all [15] poly matrices + mono [16] twins -> ONE [16][16] (V1 = index 0)

SEPARATE PASS, do SOON. Mechanical, bit-identical. (Rodney flagged while in the mix work.)

## The problem — mono is STILL special in STORAGE (not logic)
V1->voice-0 unified the LOGIC (uniform handling -> killed the spread/alignment bug class). But it did NOT
collapse the STORAGE: PatternEngine still has, for EVERY field, a MONO scalar [16] PLUS a POLY [15][16]:
  float rhythmSource[16]; ... float polyRhythmSource[15][16];   // x7 strands x several stages
(Source, Random, slewed, preRemapSlewed*, the ZCache, the snapshot buffers pR/pM/... [15][16], etc.)
Scope (PatternEngine.hpp alone): ~26 [15]/[15][16] decls + ~7 mono [16] scalar twins, and loops
`for (v=0; v<15; ++v)` scattered through .cpp (snapshot, remap, blend, source copy).

This is NOT bug-generating (the mono + poly code paths are IDENTICAL — parallel storage, uniform logic,
confirmed). It's ACCRETION-shaped maintenance debt (each field added as a mono+poly pair — see the
"SANDS CONSOLIDATION", "q-mix twin" comments). Costs: inefficiency, CODE BLOAT (everything written
twice: a mono line + a poly loop), and it keeps "mono is special" alive in the layout.

## The fix
Collapse every mono[16] + poly[15][16] PAIR into ONE [16][16] with V1 = index 0 (finishing the voice-0
unification DOWN INTO the draw-cache/source layer). Then:
- One array per field (e.g. rhythmSource[16][16]), not mono-scalar + poly-matrix.
- One loop `for (v=0; v<16; ++v)` everywhere, not "mono line + for(v<15)". Halves the blend/source/snapshot/
  remap code.
- [15]->[16] everywhere (NUM_POLY etc.); V1 lives at [0], old V2..V16 shift to [1..15].
- The ZCache, preRemapSlewed*, the thread_local snapshot buffers (pR/pM/...[15][16]) all -> [16][16].

## Why now-ish / opportunistic
The mix decomposition + z0/z1 cache work is touching ALL these arrays RIGHT NOW, writing everything twice
(mono + poly). Collapsing to [16][16] would HALVE that code. So EITHER fold the collapse into the mix pass
(cleaner mix code) OR do it as the very next separate pass before more features pile on more [15] twins.

## Risk / verify
LOW — mechanical relabelling (storage shape, uniform logic unchanged). V1 output comes from [0] instead of
the mono array; V2..V16 from [1..15]. BIT-COMPARE: output IDENTICAL before/after (same values, relocated).
The automated test gate + bit-compare catch any mismap. Watch for the scattered hardcoded `15` / `< 15`
loops — grep them all (`[15]`, `< 15`, `= 15`, NUM_POLY) and bump to 16 consistently; one missed loop =
a voice dropped. Same discipline as the lane-index relabelling (bit-identical, chase every site).
