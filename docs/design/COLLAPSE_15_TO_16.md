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

---

## CATALOGUE (done) — NOT a blanket 15->16. Three categories (Rodney asked to catalogue)
Searched the whole repo. The "15 poly voices" concept is PARTLY real bloat (internal data storage) and
PARTLY INTENTIONAL design (param/panel = V1-on-Monsoon). A naive 15->16 would BREAK the intentional half.
Classify every site:

### CATEGORY 1 — FIX: internal DATA / draw-cache arrays -> [16][16], V1 = index 0
The genuine "mono special in storage" bloat. V1 data in a mono [16] twin + V2..V16 in [15][16]. Collapse
each PAIR to ONE [16][16] (V1 at [0]). Bit-identical, INTERNAL ONLY (no param/panel impact).
- src/dsp/engines/PatternEngine.hpp (~28 [15][16]): Source/Random/slewed*/preRemapSlewed*/ZCache/the
  thread_local snapshot buffers pR..pL[15][16] + the 7 mono [16] twins (rhythmSource..qmixSource).
- src/dsp/engines/SequencerEngine.hpp (~10 [15]): per-voice len/off/dir/pingpong state + the <15 loops
  that fill them (e.g. SequencerEngine.cpp:117 polyLenERef(i,l) for i<15 -> i<16).
- src/StraitsEastSandsVisual.hpp: polySpreadEffective[15][POLY_LANES] (+2 others) -> [16][...].
- SpreadManager.hpp, SandsTopology.hpp, Monsoon.hpp: polyVoiceActive[15], laneDir/own already-16
  (check — Phase A note says laneDir has 16 rows), the [15] residuals.
- src/dsp/managers/MonsoonOutputGenerator.cpp (~9 <15), MonsoonExpanderManager.cpp (~4),
  MonsoonPersistenceManager.cpp — data loops/arrays: judge each (mostly data -> 16).

### CATEGORY 2 — LEAVE: the param/panel 15 = INTENTIONAL (V1-on-Monsoon). Do NOT touch.
There are genuinely 15 POLY params; V1's params live on Monsoon (QMIX_LEVEL_PARAM etc). The Straits panel
has 16 KNOBS (v0..v15) where knob0=V1 routes to Monsoon's mono params, knobs1..15 -> POLY_*_PARAM_1..15.
Consistent + deliberate (= the confirmed "V1 controls on Monsoon" design). Bumping these = a phantom 16th
poly param with no home.
- POLY_{REST,ACCENT,QMIX,VARIATION,...}_PARAM_1..15 enums (Monsoon.hpp) — STAY 15.
- MonsoonStraitsExpander.cpp binding: param_*_0 -> Monsoon mono param (V1); param_*_1..15 -> POLY_*_PARAM
  — STAY.
- panel_src/gen_straits.py (16 knobs v0..v15, v0=mono) — already correct; leave.
- SequencerEngine nvIdxForVoice `bank >= 15` = poly-BANK index (15 poly banks) — STAY 15 (unless the
  data-array collapse changes the bank space; judge with Category 1).

### CATEGORY 3 — CASE-BY-CASE: the ~33 live `< 15` loops
Judge EACH: iterating poly DATA array -> `< 16` (rides with Cat 1); iterating poly PARAMS/BANKS -> stay 15.
Per-file live counts (excl deprecated): PatternEngine.cpp 1, SequencerEngine.cpp 5, ExpanderManager 4,
OutputGenerator 9, Monsoon.cpp 4, StraitsBase 2, StraitsEastSandsVisual 8.

### CATEGORY 4 — EXCLUDE: src/deprecated/* (dead code) — ignore.

## Method for the pass
Do Category 1 ONLY (+ its Category-3 data loops). Leave Category 2 (param/panel). It is a STORAGE
unification, not a param change. Per site: collapse mono[16]+poly[15][16] -> [16][16], V1 at [0], V2..V16
at [1..15]; fix the feeding loops to <16; BIT-COMPARE output identical (same values relocated). The
panel/param layer is untouched, so no UI/preset migration. Watch: don't bump a param/bank 15 (Cat 2).
Treat like lane-index: mechanical, bit-identical, chase every DATA site, verify by bit-compare + test gate.
