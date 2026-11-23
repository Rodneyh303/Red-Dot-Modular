# CC prompt — Scrub range 6 -> 32, bidirectional, snap-on-release; slew phrase-fixed

Two linked changes to the same recompute functions (do them in this order, separate commits):
PART A: slew is consumed ONLY at the phrase boundary (fix the mid-phrase leak).
PART B: the scrub becomes a bidirectional +/-32 draw offset (crab-canon alignment), with
snap-to-integer on release and fine/coarse drag modifiers.
Rack behaviour (feel, audio) is Rodney's feel-test; everything below is Rack-free tested.
Rodney: existing patches need not load identically.

## Background (verified by reading + probes, 2026-11-22)
- Every draw is a pure function of (seed, signed draw index); Philox is addressable at any index.
  Pattern at position p = moving-average copula over the K=64 draws ending at p.
- Scrub today (`PatternEngine.cpp` 487/605/692 and the ForVoice variants 772/827/872):
  `s = clamp(mixLatched,0,1)*6; f=(int)s; frac=s-f`, windows N-f and N-f-1 blended in normal space
  (OU bridge), z-cache keyed (N, f, r). Looks BACK only. `SCRUB_K = 6`; the draw-cache prune half-span
  is `K + SCRUB_K + 16` (PatternEngine.hpp ~1213), symmetric by design ("BIDIRECTIONAL-ready").
- Slew contract (PatternEngine.hpp ~1362, MonsoonSandsManager.cpp ~1181): latched every control
  cycle, consumed ONLY at the roll/boundary, "slew must be constant across a phrase".

## PART A — slew consumed at the DRAW boundary only
AMENDMENT (2026-11-22): the boundary is the wrap of the EFFECTIVE DRAW WINDOW (the span of random cells one
slewed buffer represents), NOT necessarily the pattern-length wrap. Today `result.wrapped` is
`pos_.phrasePos()==0` at the global pattern length (SequencerEngine.cpp:506). Under a 16-step pattern over a
32-step voice window that fires every 16 steps, i.e. half-way through the draw. With CC_PROMPT_UAS_CYCLE_PHRASE_MODE
(Cycle: Phrase default) the draw window equals the global phrase so the existing boundary is right; in Cycle: Custom
with L != global length, consume slew only when the effective draw window completes a lap. Add a test: 16 pattern,
Custom L=32: slew unchanged across both 16-laps, applied once at the 32 wrap. Establish first (test, not assumption)
whether the draw COUNTER/roll also ticks at the 16 wrap or the 32 wrap and report.
LEAK (reproduced with a standalone probe): `refreshVisualCache`'s mix branch calls
`recomputeEffective*ForVoice`, which reads the LIVE `*SlewLatched`; a changed r misses the z-cache
and falls through to a FULL all-voice `recomputeEffective*`. So any MIX/scrub movement (knob, CV,
Raffles/Junction) or integer-position crossing applies the current slew immediately. Probe:
slew-only modulation over 8 cycles -> 0 applied changes (correct); slew+mix modulated 8 cycles
-> 7 changes (contract: 0); a single tiny mix nudge flipped the applied slew and rewrote the buffers.
(Not established: whether this explains a per-STEP cadence Rodney reported. If, after the fix, he
still sees step-rate slew with no mix/scrub modulation patched, the cause is elsewhere — report
what you find, do not guess.)

FIX: add a phrase-fixed slew per stream (`rhythmSlewPhrase`, `melodySlewPhrase`, `qmixSlewPhrase`).
- All six recompute functions read `*SlewPhrase` (clamped) instead of `*SlewLatched`:
  `recomputeEffective{Rhythm,Melody,Qmix}` and their `ForVoice` variants.
- `*SlewPhrase = clamp(*SlewLatched)` is assigned ONLY at: the roll (start of `redrawRhythm/Melody/
  Qmix`), the boundary consumer branch (`applyPendingSeedsAndRedraw`, the `else if ... SlewApplied !=
  -1 ... != SlewApplied` branches: set phrase = latched, then recompute), seed / reseed, the mode
  switches, persistence load (`MonsoonPersistenceManager`: after the latched values are restored and
  `SlewApplied = -1`), reset/init, and `forceRecomputeSlewed()` (CA remap, voice-count hint rise:
  use the phrase value, NOT the live latch).
- `latchSlew` stays as is (every control cycle, unconditional). The existing
  `test_slew_latch_regression` MUST stay green (boundary consumption is unchanged).
- Keep the z-cache key (N, f, r): it guarantees both blended windows use the SAME r.

PART A TESTS (failing first, Rack-free, register in `run_all.sh`; extend the slew-latch test or add
`test_slew_phrase_fixed.cpp`):
1. Mid-phrase, slew AND mix both modulated for 8+ cycles: `*SlewApplied` and the slewed buffers do
   not change (all three streams). Fails today (7 changes).
2. The same, then `onPhraseBoundary`-equivalent consumption: applied slew changes exactly ONCE, to
   the latest latched value; buffers equal a reference engine rolled with that slew.
3. Mix-only change mid-phrase with r != 0: only the displayed-voice path runs (no full recompute):
   count recomputes via a test hook or compare other voices' buffers bitwise (unchanged).
4. Uniformity guard: for slew in {-1,-0.6,0,0.3,0.7,1}, per-step KS statistic of 1000 INDEPENDENT
   samples (draw indices spaced >= 70 apart, i.e. beyond the K=64 window — consecutive draws are
   autocorrelated and inflate KS) stays under the 5% critical value (~0.043). A phrase read at two
   different r values stays marginally uniform per step (documents that the harm of mid-phrase
   slew is the joint law, not the marginal).
5. Both windows of a blend use the same r (assert via the z-cache key or a reference computation).
Mutation-check: read `*SlewLatched` again in one ForVoice -> test 1 fails; forget to set phrase at
the boundary consumer -> test 2 fails; forget it in `forceRecomputeSlewed` -> a CA-remap case fails.
Red first, then green, then the full suite. Commit PART A on its own.

## WHY THE JOG MATTERS BEYOND CANON — NON-DESTRUCTIVE EXPLORATION (Rodney 2026-11-23)
"With at least 64 discrete draws to explore the user may not need to redraw often." This reframes the
feature's PURPOSE, and is worth keeping as rationale:
- The jog is a BROWSE mechanism over a large space of related patterns, not only a canon-alignment tool
  or a morph. 64+ (up to 190 at K=16) distinct draw positions, each a full pattern.
- Crucially the neighbours are CORRELATED (by r), so moving the jog gives GRADUAL variation — a
  continuum of related patterns, not 64 unrelated rolls. That is far more musical than repeated redraws.
- It is NON-DESTRUCTIVE AND REVERSIBLE: the jog is a pure function of (seed, counter, offset), so you
  can always return to a position you liked. REDRAW is destructive — lose something good and it is gone
  unless the seed is recoverable. So the jog turns variation from "destructive random resampling" into
  "navigation of a correlated space you can always walk back through."
- CONSEQUENCE for the instrument's shape: REDRAW's role changes to "I have exhausted this
  neighbourhood, give me a new one", and the SEED becomes a neighbourhood selector rather than the
  primary variation control. Worth bearing in mind when the panel/UX is tidied.

## MA DEPTH (K) — PANEL HOME, DETENTS, AND DRAW-CADENCE (Rodney 2026-11-23)
- PANEL HOME: the 3 OFFSET READOUTS and the 3 MA DEPTH knobs (one each for R/M/Q) belong on MONSOON
  itself — all of this is fundamental, not expander material. Consistent with the fold criterion:
  optional things become bands, fundamentals stay on the panel. MONSOON MAY NEED MORE HP. NOTE: this
  COMPOUNDS with the reverse work's GATE 4 input (output gates shift one column right) — plan the panel
  rethink for BOTH together, not piecemeal.
  PLACEMENT IS PROVISIONAL (Rodney 2026-11-23): put the readouts/knobs in ANY FREE AREA for now and
  tidy the layout later. BUT that applies to GEOMETRY ONLY — param/port IDs are written into saved
  patches, so moving a control later is free while CHANGING ITS ID BREAKS PATCHES. Decide the depth-knob
  and readout IDs DELIBERATELY in the frozen enum now, even though their positions are temporary.
- DETENTS: the depth knob gets a few detent positions, like Monsoon's main note-length knob. K is an
  integer tap count anyway, and detents HELP CANON — matching K by hand across two Monsoons is far
  easier with a few discrete stops than a continuous sweep (same logic as snapping the offset to
  integers: what a canon must match should be discretely settable). Natural stops fall out of the
  budget: K = 64 (+/-32), 32 (+/-64), 16 (+/-80), optionally 1 (no averaging, the raw draw).
- CADENCE: PER DRAW, LIKE SLEW. K changes are latched every control cycle and CONSUMED AT THE DRAW
  BOUNDARY, never mid-draw. ARCHITECTURAL CONSEQUENCE: r (slew) and K (depth) are BOTH copula-window
  parameters — both define the window a draw's value is computed from, so both must be constant across
  a draw for the SAME reason. PART A SHOULD THEREFORE BE GENERALISED: not a slew-specific fix but a
  "copula-window parameters are draw-fixed" mechanism carrying `*SlewPhrase` AND `*DepthPhrase` through
  ONE boundary-consumption path, rather than two mechanisms that must be kept in sync.
- RE-CLAMP ON K CHANGE (and an open feel call): the jog range depends on K, so consuming a K change at
  the boundary must ALSO re-clamp the live offset (`clampOffsetForK`, already built). INCREASING K
  SHRINKS the range: at offset +60 with K=32 (range +/-64), moving to K=64 (range +/-32) clamps the
  offset to +32 — a 28-draw JUMP, audible. RESOLVED (Rodney 2026-11-23): ACCEPT THE JUMP (option a) —
  the user did ask for a shorter reach. Make the readout show the clamped value so the move is visible.
  Do NOT constrain the depth knob to the current offset.

## PART B — scrub +/-32, bidirectional, snap on release

### Engine
- Position f in [-32, +32] integer part + `frac` in [0,1). f > 0 = behind the counter (today's
  behaviour, window N-f), f < 0 = AHEAD (N+|f|; Philox makes future draws computable).
  Named constants: `SCRUB_K = 32` (replace every literal `6` / `*6.f`), one `scrubPosition(mix)`
  helper used by all six sites so they cannot drift.
- IMPORTANT: split `s` with `std::floor`, NOT `(int)s` (truncation toward zero breaks negative `s`:
  frac would go negative). `f = (int)std::floor(s)`, `frac = s - f`. Keep the pair (N-f, N-f-1) and
  the bridge key `N - f - 1` consistent for negative f; add a test.
- Parameter representation: choose a mapping where DEFAULT = live (f = 0) and each integer position
  is EXACTLY representable (use 1/32 steps: k/32 is exact in float; a stored normalised value in
  [-1,1] with f = value*32 is the natural choice). Mix CV (Raffles/Junction) scales so full CV =
  full travel; document the mapping. Update `ParamQuantity` ranges/defaults/labels (`MonsoonWidget`,
  `Monsoon.hpp` params), persistence, and everything that read the old 0..1 mix (grep `MixLatched`,
  `MixApplied`, `latchMix`, the ModeController CV offsets, the Lantern/UI that shows the scrub).
- Draw cache: prune window must cover f in [-32,32]: draws touched are [N-32-63-1, N+32+1].
  Either keep the symmetric `K + SCRUB_K + 16` (= 112 each side, worst case ~9.5 MB per Monsoon:
  one cached position across rhythm+melody+qmix is ~42 KB) or prune asymmetrically to ~131 entries
  (~5.5 MB). Your call; report the measured working set. A reverse-running Monsoon mirrors the
  window — the rule must cover both directions.
- MOVING-AVERAGE PERIOD AS A USER CHOICE + THE BUDGET TRADE (Rodney 2026-11-23, NEW). It was always
  intended that the user can choose the moving-average period (the copula K); not added yet. The cache
  half-span is a FIXED allocation spent on TWO things — the window (K draws) and the jog range (SCRUB
  each side): `K + scrub + margin <= kCacheHalfSpan`. So keeping the cache sized for the MAX K (64) and
  letting the user pick a SHORTER period converts the freed entries into EXTRA JOG RANGE at no cache
  cost. With the current 112 allocation and margin 16 the K+scrub budget is 96:
      K = 64 -> scrub +/-32 (today)    K = 32 -> scrub +/-64    K = 16 -> scrub +/-80
  A shorter average = less smoothing/correlation but a longer reach; a longer average = more correlated
  but shorter reach. A real musical trade. LAW IS BUILT + TESTED: `ScrubOffsetCore` now has
  `kCacheHalfSpan`, `maxScrubForK`, `maxKForScrub`, `budgetFits`, `clampOffsetForK` (re-clamp a live
  offset when K changes) — test_scrub_offset §7, mutation-proven (forgetting the margin fails 204).
  RESOLVED (Rodney 2026-11-23): K is PER-STREAM (R/M/Q), like slew/mix/the draw counters. Reasons:
  (1) the CACHES ARE ALREADY PER-STREAM (rhythmDrawCache_/melodyDrawCache_/qmixDrawCache_, ~24/12/6 KB
  an entry) — the budget trade is about cache entries, so it is naturally per-stream; a GLOBAL K would
  impose a coupling the storage does not require (the melody stream's reach limited by the rhythm
  stream's memory depth, for no structural reason). (2) It matches its layer — slew, mix/scrub and the
  draw counters are all per-stream; a global K would be the lone exception. (3) Musically it is a real
  axis: a long rhythm average (slowly-evolving groove) under a short melody average (pitch free to
  move), or inverted. No technical obstacle: streams are independent draws with separate Philox keys,
  the copula correlates across VOICES and across TIME within a stream, never between streams, so each
  K is independent and uniformity holds per stream.
  CONSEQUENCE — streams get DIFFERENT MAX JOG RANGES (rhythm K=64 -> +/-32 while melody K=32 -> +/-64).
  That is FINE and needs no reconciliation: RULED (Rodney 2026-11-23) the scrub knobs are PER-STREAM and
  stay that way — DO NOT create a linked jog. Each stream scrubs within its own budget; there is no
  clamp-to-minimum logic to design and the earlier "link R/M/Q" open question is CLOSED (no link).
  K AND r INTERACT (the other per-stream copula control is the temporal correlation r): the weights are
  geometric (w_j ∝ r^j), so the USEFUL depth depends on r. At r=0.5 the tail is negligible past ~3 taps
  — K=64 is waste, drop to K=16 and gain +/-80 jog with no audible loss. At r=0.97, 0.97^64 ≈ 0.14, so
  the tail still matters at 64 taps and deep K genuinely earns its cache. I.e. the budget trade is nearly
  FREE at low correlation and genuinely COSTLY at high correlation — worth a UI hint eventually.
  CANON: per-stream K adds no NEW class of problem — canon already requires matching slew per stream,
  so K joins that set; the mirror test compares per-stream as it already does.
  Implementation note: `MovingAverageCopula::K` is a compile-time constant sizing the weight array —
  make it a runtime K_active <= 64 per stream, using the first K taps (uniformity is preserved for ANY
  K since the weights are L2-normalised: Sum w^2 == 1, pinned by test_moving_average_copula).
- PARAM REPRESENTATION — USE OFFSET-DIRECT, NOT NORMALISED (decided here; wire it this way).
  WHY IT MATTERS: the param is saved in the patch and drives snap + the canon mirror. Snap-on-release
  must land EXACTLY on an integer draw offset; the canon mirror needs two Monsoons to agree BITWISE; and
  once patches are saved with one encoding, changing it breaks them. Cheap now, expensive later.
  OPTION A (normalised, what ScrubOffsetCore first used): param in [-1,1], offset = param * range. At a
  FIXED range of 32 every integer k maps to k/32 — a dyadic rational, EXACT in binary float. Fine.
  WHY THE VARIABLE RANGE BREAKS IT: with range varying by K, k/32 and k/64 stay exact (powers of two)
  but k/80 does NOT — 80 = 2^4 * 5 and that factor of 5 has no finite binary representation. So at K=16
  snap lands ALMOST on an integer, round-trips can drift, and two Monsoons can differ in the last bits —
  exactly where canon needs them identical.
  OPTION B (CHOSEN) — OFFSET-DIRECT: the param value IS the offset (16.0 = +16 draws; 16.25 = offset 16,
  frac 0.25). Integers are exact in float for ANY range (float32 is exact for integers to 2^24), so
  exactness no longer depends on the range being a power of two. Snap = round-to-nearest-integer,
  trivially exact. The canon comparison becomes an INTEGER comparison. The value is also legible.
  THE WRINKLE + FIX: Rack param ranges are fixed at config() but the usable limit moves with K.
  Configure the param at the WIDEST range (+/-80, the K=16 case) for STORAGE, and have the WIDGET map
  its rotation to the CURRENT maxScrubForK(K) — so full knob travel always covers the current range,
  the param merely carries headroom, and clampOffsetForK (already built) enforces the limit. CV scaling
  references the CURRENT range, not the configured max. (Naively configuring +/-80 and letting the knob
  sweep it would leave 60% dead travel at K=64 — do not do that.)
  NOTE: ScrubOffsetCore's positionToParam/paramToPositionInt still encode the k/32 normalisation; they
  must be revised to offset-direct when the param is wired (the splitScrub/bridge/snap/budget functions
  are unaffected).
- Per-stream mix knobs (R, M, Q) stay per stream. A canon needs all three at the same offset: add
  nothing unrequested, but REPORT a proposal for linking (e.g. a modifier or a master) as an open
  question for Rodney.

> **STATUS (2026-11-23 update): the jog's pure widget laws are ALSO done — `jogAccumulate`
> (endless-encoder: monotone, clamped to the CURRENT maxScrubForK limit, STICKY at a boundary, and
> SCALE-BEFORE-CLAMP so a big gesture with the fine modifier lands correctly — the clamp-first mutant
> fails 3) and `snapKToDetent` (nearest depth detent, ties to the LARGER K so a tie never silently
> grants extra jog range; the default set {1,16,32,64} is a parameter, retune by feel). The widget
> should CALL these; the only widget work left is the Rack plumbing + the feel constants
> (pixelsPerDraw, fineScale) which are Rodney's feel-test.**
>
> **STATUS (2026-11-22): the pure offset MATH is DONE — `src/dsp/ScrubOffsetCore.hpp`
> (splitScrub FLOOR-safe, positionToParam/paramToPositionInt exact k/32, snapToIntegerPosition,
> drawIndexHi/Lo bridge keys, scrubPositionFromMix, cacheHalfSpan), test_scrub_offset registered +
> green, the truncation mutant fails 450 checks. The engine + widget must CALL this core, not re-derive
> the split: wire the six scrub sites and ScrubKnobT through it. Tests 2/4/6 (negative-f split, k/32
> exactness, snap) are covered at the pure level; the behavioural tests (1 mirror/canon, 5 cache, 7
> held-frac uniformity) still need the real engine.**

### Widget (`src/ui/Controls.hpp`, `ScrubKnobT`)
- `SCRUB_STEPS` -> 32 and bidirectional range (65 positions); the template must stay usable for any
  other `Scrub_Small_Cog` bound params (check all bindings: `grep Scrub_Small_Cog`).
- Drag: default = continuous with the existing magnetism toward integer positions; SHIFT = fine
  (slower, Rack's own modifier convention); CTRL/SUPER = discrete click-through, one integer
  position per small pixel step (120 px/step is far too slow for 65 positions; ~8-12 px — expose as
  a named constant, Rodney will tune by feel).
- JOG INTERACTION MODEL — ENDLESS RELATIVE ENCODER (Rodney 2026-11-23, supersedes the absolute-knob
  framing). The jog is an "infinite-like" rotary dial: spin LEFT always DECREASES the position, spin
  RIGHT always INCREASES it, up to the boundaries. It accumulates DELTAS — there is NO absolute
  rotation-to-position mapping. Consequences: (a) the param can be configured wide (kMaxScrubEver = 95)
  and software-clamped to maxScrubForK(K_active) with NO wasted knob travel, because there is no
  absolute travel to waste — this is what makes OFFSET-DIRECT storage free of downside; (b) at a
  boundary the encoder simply stops accumulating.
- SNAP IS MODIFIER-GATED, AND MODULATION NEVER SNAPS (Rodney 2026-11-23 — supersedes the earlier
  "release behaviour is a persistent mode"): HAND control with a MODIFIER held snaps to an integer
  offset; hand control WITHOUT the modifier is continuous/free; CV MODULATION IS NEVER SNAPPED. This
  resolves the old open question "should canon-grade operation quantise the CV offset?" — NO. The two
  uses separate cleanly: a bitwise-exact CANON is set BY HAND with the modifier (integer offsets), and
  CV-driven offsets are for MORPHING, where continuous is the point. (Whether the default feel also has
  gentle magnetism toward integers, with the modifier giving a hard snap, is Rodney's feel-test call.)
- READOUT IS NOW NECESSARY, not optional: an endless encoder gives NO absolute position feedback —
  rotation cannot tell you WHERE you are, only that you moved. So the current offset must be DISPLAYED
  (a number, the Lantern, or an LED), otherwise +/-3 is indistinguishable from +/-30. (The ER-301 the
  idea came from has a screen.) The "Lantern/UI that shows the scrub" item above is therefore a
  requirement, not a nicety.
- DOUBLE-CLICK -> NEUTRAL (Rodney's gesture): reset to f = 0 / frac = 0 = the LIVE read, reflecting the
  MOST RECENT draw (no offset). Neutral == the centre of the bidirectional range == live; one gesture
  gets back to "just the current draw" from anywhere.
- RELEASE BEHAVIOUR IS A MODE (Rodney): a per-knob (or context-menu) choice —
  (a) SNAP-ON-RELEASE: on `onDragEnd` round the param to the nearest integer position BEFORE the undo
      `ParamChange` is pushed (else undo records the pre-snap value). Exact (k/32). This is the canon /
      land-on-a-clean-draw mode.
  (b) FREE-FLOAT: no snap — the fractional offset is held wherever released. This is the expressive /
      morph mode (sit "between" two draws). Default: pick one and say which; make it a one-flag switch.
- CV modulation adds a continuous fraction on top of the base (so the fractional position can be
  MODULATED or HELD at any point within a phrase — the brownian-bridge read is a genuine intermediate
  correlated draw-state, not an output crossfade). The release snap applies to the knob base only.
  (Open question for Rodney: should canon-grade operation quantise the CV-added offset too? — that is
  the same fork as the release mode: quantise for a mirror-exact canon, leave continuous for morphing.)

### Tests (failing first; Rack-free)
Add `test/test_scrub_bidir.cpp` (register):
1. Mirror/alignment (the crab-canon core): two engines, same seed, slew and length. Engine X at
   counter n with scrub f1, engine Y at counter n' with scrub f2, with n - f1 == n' - f2: the slewed
   buffers are BITWISE equal for all three streams, all voices, f in [-32,32] including negative f.
   Reach the counters both by forward rolls and by REVERSE rolls (`setReverseActive(true)`, the draw
   index steps -1 per boundary) — same pattern either way. (This proves the offset aligns two
   Monsoons; the step-order reversal within a phrase is Mode E's job and is NOT tested here.)
2. Negative-f floor split: s = -3.25 -> f = -4, frac = 0.75; blend result between the windows of
   f=-4 and f=-3 positions; bit-identical to a reference computed from the two windows directly.
3. f = 0 / frac = 0 equals the old default behaviour bitwise (slew r=0 raw draw; r != 0 copula).
4. Exact representability: every integer position k in [-32,32] round-trips param -> position
   with no off-by-one (k/32).
5. Cache: after touching f = +32 then f = -32, every touched draw is inside the prune window (no
   eviction of a draw still needed by the current window); a miss recomputes bit-identically.
6. Snap: a knob-level pure helper `snapToIntegerPosition(v)` (keep it Rack-free in a core header so it
   is testable) — nearest integer, ties defined, exact k/32 output, idempotent.
7. HELD-FRAC MARGINAL UNIFORMITY (Rodney: "we preserve uniform throughout until Distributions"): the
   scrub read at a HELD fractional offset is marginally uniform per step, exactly as at integer draws.
   Read the PRE-DISTRIBUTION draw output (the copula/quantile stage — NOT after the warp/Distribution
   shaping, which is where non-uniform shaping legitimately enters). For frac in {0.15, 0.4, 0.6, 0.85}
   (and a few integer f), per-step KS of 1000 INDEPENDENT samples (draw indices spaced > K = 64 apart —
   consecutive draws are autocorrelated and inflate KS) stays under the 5% critical value (~0.043).
   This proves the MORPH / free-float mode is uniform, not just assumed; the brownian-bridge endpoints
   are standard normal and the interpolation stays Gaussian, so the marginal should hold at any frac —
   the test pins it. Mutation: a non-bridge crossfade of the two draws' OUTPUTS (instead of the normal-
   space z interpolation) breaks per-step uniformity at frac!=0 -> this test fails.
Mutation-check: truncate with `(int)` instead of floor -> test 2 fails; keep SCRUB_K at 6 -> test 5
or 1 fails; snap after the undo push (simulated order helper) -> a snap-order test fails.
Red first, then green, then the full suite.

### Performance (measure, do not assume)
Bench (print, assert only a generous bound): cold-cache cost of (a) a +/-1 position step (should be
~1 new draw: the window shifts by one), (b) a jump of 32 and 64 positions, for three streams at 16
voices. Dragging is cheap; the risk is discontinuities (CV steps, patch load, the first fill after
seeding, double-click reset). Determine which thread runs `refreshVisualCache` /
`recomputeEffective*` (audio vs UI). If a 32-position jump exceeds ~1 ms on the audio thread,
propose a mitigation (precompute neighbouring positions across blocks, or limit CV steps) rather
than shipping a glitch.

## Out of scope (separate)
Mode E / reverse step-order work for the full retrograde canon; the optical/endless knob idea; the
Monsoon scrub linking across R/M/Q beyond the proposal above.

## Report back
Part A red/green + the answer to "is the per-step slew cadence explained?"; Part B red/green,
mutation results, the parameter mapping chosen, the cache window rule and measured working set, bench
numbers and thread, and the proposals/open questions (R/M/Q linking, CV quantisation).
