# The Fallback Ladder — no-preference behaviour across the quantiser/generative family

**Ruling: Rodney, 2026-11-28** (feel-test thread on #359, following the weighted-partition fix).
Companion to `QUANTISER_WEIGHTED_PARTITION.md` (the pairwise gap-splitting model this ladder
degenerates into). Implementation: `src/dsp/NoPreferenceFallbackCore.hpp` (the pure rule),
`SequencerEngine::rebuildScaleCache` + `quantizeResolve` (quantiser path),
`ModeController::updatePatternInput` (generative path), `QuantResult::wrapDelta` +
`voicePitch` mixed rows (the octave-wrap memo). Tests: `test_no_preference_fallback.cpp`.

## The one rule

> **All faders down = no preference = equal faders over the governing scale set.**

When every weight in the governing set is zero, the engine behaves **byte-identically to the
user manually setting every member's fader to the same height** (the height is irrelevant —
both the partition boundaries and the pick CDF are scale-invariant; we use 1.0). The
substitution happens at the single point where weights enter each path, and everything
downstream operates unchanged:

- **Quantiser** (external CV): uniform weights through the same pairwise partition. Equal
  weights ⇒ midpoint boundaries ⇒ **exact nearest-note snapping to the selected scale** — no
  second algorithm exists.
- **Generated notes**: uniform weights substituted **upstream of the pick**, so warp /
  distribution distortion of the draw `r` still shapes the uniform pick exactly as it would
  shape real equal faders. The old degenerate behaviour (pickSemitone sum≤ε → fixed monotone
  C at 0 V) is retired. (An earlier idea — generated rest on all-down — was considered and
  **rejected** in favour of this single unified rule.)

Weight is **loudness**; membership is **scale** (ENABLED_MASK_BUILD_BRIEF). A member with
zero weight is a legitimate fallback target. The fader params are never written — faders are
the STORE; the substitution lands only in engine-side snapshots.

## The governing set (where membership comes from)

| Context | Membership channel | Notes |
|---|---|---|
| Monsoon, 12-TET host path | `ScaleManager::activeScaleMask` | The context-menu scale. Arbiter: override (Shophouse) > authored (enable band) > factory (scale,root) > all-12. **Chromatic is the default**: no-selection fall-through = 0xFFF = selecting Chromatic (menu now shows it checked). Lock does NOT gate the fallback — the menu scale is the declared preference whether or not it is enforced on the faders. A Shophouse/Micro attachment may override the menu scale; the arbiter already encodes that precedence. |
| Micro path (Colonnades Micro-12 / Colonnades Duo Micro-24) | `TuningTable::enabled[]` | The dmtune/authored scale **within the tuning**; all-true default = the whole tuning. Rodney's Duo ladder — (1) dmtune scale, (2) tuning without scale, (3) default 12-TET / 24-TET — is the **data degenerating**, not code: the engine just reads `(cents[], enabled[])`. |
| Sikit | — (positions only) | Sikit retunes `degreeVolts`; the set stays Monsoon's. Which note is "nearest" shifts with the cents automatically. |

Defensive rung (FB6): an empty membership set (the module-side never-all-off guard failed, a
stale `enabled[]`) degrades to **all N degrees**. The quantiser always has a target; silence
is never the quantiser's call. The core's `m==0` raw-clamp in `weightedQuantize` stays as an
unreachable dumb safety net — musical policy lives in the set construction, never the core.

### Plumbing notes

- `rebuildScaleCache(weights, n, members)` builds the active list via
  `redDot::buildActiveDegreeSet` (weighted regime byte-identical to the legacy `w>0` filter).
- The rebuild trigger folds in a **membership signature** (host: the 12-bit mask; Micro:
  N + enabled bits) — the fallback set can change with **no fader movement** (menu scale
  change with lock off, Shophouse push, Micro enabled[] edit while faders are down).
- Degree order never implies pitch order (a Micro tuning may be non-monotone in degree
  index): the partition sorts candidate instances **by voltage**, the pick CDF is order-free
  in distribution. Pinned by the S7 permutation-invariance test.
- Octave-period assumption: the whole stack is octave-period by construction (TuningTable
  stores cents/1200 within-octave; the Scala importer leaves the period to the caller, ours
  assume 1200¢). Non-octave periods (Bohlen-Pierce tritave .scl) would need a period
  parameter on the partition — a separate, future decision.

## The octave-wrap memo (`QuantResult::wrapDelta`)

The margin enumeration lets a wrap-neighbour instance win near the range edges; the in-range
remap folds it back into [vLo,vHi] and **records the fold**: `wrapDelta` = +1 (below-range
winner folded up), −1 (above-range folded down), 0 for every mid-range win. At equal weights
the wrap boundary sits exactly ON the range edge, so a margin instance never wins and
wrapDelta is always 0 — the goldens' regime is untouched.

Who consumes it (ruling: "modify the octave up or down 1 if the wrap case warrants it, in
both mixed rows"):

| Row | Octave source | Behaviour |
|---|---|---|
| deg EXT + oct EXT | the snap itself | FOLDED absolute answer (output must live in 0–5 V). |
| deg EXT + oct GEN | generated register | `register + (frac − wrapDelta)`: "just below the bottom C" sounds as the **B just below** the generated register (−0.083), not the B at its top. |
| deg GEN + oct EXT | external CV | `(q.octave − wrapDelta) + degTerm`: the generated degree seats in the octave the CV **actually pointed at** (final clamp catches range). |
| deg GEN + oct GEN | — | No quantiser involvement. |

Mid-range the memo is 0 everywhere ⇒ **byte-identical to the pre-memo engine**. Deliberately
NOT adopted: a `snapOctave − floor(vIn)` memo mid-range — it moves the jump to the integer
volt line where it fires by a full octave with no degree change. Mid-range, the mixed row's
pitch-class semantic stands: compressing octaves of CV into one register must jump somewhere,
and the scale's wrap point (the one-register "kalimba" rollover) is the degree-aligned,
least-wrong place.

## What a feel-test should hear

1. All note faders down, chromatic (default): external CV quantises to the nearest semitone;
   generated notes walk all 12 uniformly (warp still shapes them). No monotone-C drone.
2. All faders down, Pentatonic Major selected (lock on or off): snaps/picks only pentatonic
   members, nearest-note.
3. One fader creeps up from 0: boundaries deform continuously from the midpoints — no
   algorithm seam.
4. Micro attached, dmtune scale loaded, Micro faders all down: snaps to the dmtune scale
   within the tuning; Micro detached mid-performance falls back to Monsoon's own ladder.
5. Mixed row (external degree + generated octave), CV parked just below 0 V with C muted:
   the sounding note sits just below the register's C, not near its top.
