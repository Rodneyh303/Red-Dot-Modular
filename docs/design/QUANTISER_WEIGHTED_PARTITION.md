# Quantiser as a weighted partition — the per-degree fader is a *range*, not a probability

Status: DESIGN AGREED (Rodney, 2026-11-23). Not yet built. This doc is the spec for
the build (core + `quantize()` swap + the octave patched-detection fix).

## The symptom (Rodney, 2026-11-23)

Using qmix with a full melody patched to the quantiser CV, a scale degree whose
fader is set to ≤1% still sounds **~half the time**, while other degrees sit at
50–100%. "The lower note is on at least half the time. Just seems wrong."

## What the code actually does today (verified)

`pickSemitone` (the *generated* degree path, `genPitchLive`) is a correct weighted
CDF — a 1% fader there picks ≈1%. So the symptom is **not** the generated path.

"Using qmix" means a quantiser mode is active (`quantiserPitchSource`): qmix blends
a GENERATED pitch with a QUANTISED external CV, per step, by a draw-vs-threshold
split (`r_qmix >= qmixLevel → generated, else quantised`). On the **quantised**
steps the degree comes from `SequencerEngine::quantize(externalCV)`:

- Each active degree gets a fixed capture **radius** `= w·(1/12)` (fader → width:
  full fader ≈ one semitone, 1% ≈ one cent). A candidate qualifies only if the CV
  is within that radius; among qualifiers, `score = w/(dist + 1/12)` wins.
- **The break:** when the CV lands where *no* degree's radius reaches it — most of
  the voltage axis, since radii are narrow — the code drops to a **nearest active
  degree by pure distance, weight ignored** fallback (`quantize()` lines ~3206–3217).

So a 1% degree, whose own radius is ~1 cent, still wins every CV value for which it
is merely the nearest active degree. With a full melody and a low degree that has no
close active neighbour below it, that "nearest" region is a big slice of the input →
~half the time. The radii say "1% = 1 cent wide," the fallback says "but take it
anyway over an arbitrarily wide region." The fader's width only bites near strong
neighbours; everywhere else it is ignored.

## What it SHOULD do — the Vermona meloDICER semantics

From the meloDICER manual, Quantizer mode (Mode C):

> "The faders of the melody section define your scale. The higher a fader is raised,
> the wider its quantization range."

So in quantiser mode the fader is a **quantization range (capture width)**, not a
playback probability. (In the generated modes the same fader IS a probability — two
meanings of "more of this degree," both monotonic, which is intuitive.) The current
code is *trying* to do this (`radius = w/12`) but the fixed-radius + weight-ignoring
fallback is not a clean partition, so it fails for non-uniform faders.

A range model only makes sense as a **partition of the input voltage axis with no
gaps** — every input snaps to something, each degree's claimed slice proportional to
its fader, **relative within the active set** (Rodney: agreed — two faint degrees
still split the whole octave between them in their weight ratio; "faint" is relative,
per "the faders define your scale").

## The model — weighted-boundary partition

Active degrees are those with fader `w > 0` (a `w = 0` degree is OUT of the scale —
zero sliver; a tiny `w > 0` claims a proportional sliver). Sort them by within-octave
position. The boundary between two adjacent active degrees sits proportional to their
weights:

```
boundary(a → b) = pos_a + (pos_b − pos_a) · w_a / (w_a + w_b)
```

The stronger neighbour pushes the boundary toward the weaker, claiming more range.
Adjacent instances share endpoints exactly → the axis **tiles**: no gaps, no
overlaps, no fallback. A 1% degree beside a 100% degree claims ~1% of the gap; with a
swept melody it sounds ~1%, which is the fix.

**Pairwise-local by design:** a degree's range depends only on its two immediate
neighbours' faders, not the whole scale. This is musical (local contrast) and means a
fader change moves only two boundaries.

### Uniform-weight reduction (the byte-identity pin)

With uniform faders every boundary is the **midpoint** → plain nearest-degree
snapping, the standard quantiser and (almost certainly) what the current `quantize()`
goldens encode. So the partition is **behaviourally identical for uniform weights**;
only non-uniform cases change. The one caveat: a CV sitting *exactly* on a midpoint
could tie-break differently between the old min-distance search and a new `<`/`<=`
interval test — match the old convention so the uniform goldens don't move. Only
non-uniform-weight cases re-pin.

## Octave wrap — one seam boundary, a ±1 delta

The partition is periodic with period **1 octave = 1 V** (microtonal included —
the scale is still one octave; only the degree positions move, via
`degreeVolts()`, the `+1 V` seam offset is constant). Within one octave there are
`m−1` interior boundaries plus **one** wrap boundary between the highest active
degree `d_{m-1}` (pos `p_hi`, weight `w_hi`) and the lowest `d₀` one octave up (pos
`p_lo + 1`, weight `w_lo`):

```
b_wrap = p_hi + (1 + p_lo − p_hi) · w_hi / (w_hi + w_lo)
```

Only the seam slice ever carries an octave delta; every other slice is delta 0
(`baseOct = floor(vIn)`). Whether `b_wrap` lands below or above 1.0 is the
"add or subtract 1," and exactly one happens per scale:

- **`b_wrap < 1.0` → the lowest degree spills up.** Region `[b_wrap, 1.0)` → `d₀`
  with **+1**. Example: C(1.0)@0.000 / G(0.5)@0.583 → `b_wrap = 0.722`, so
  `u ∈ [0.722, 1.0) → C, +1` — 0.95 V snaps up to the next C, as a plain quantiser
  would. (C claims 0.667 of the octave, G 0.333 — the 2:1 weight ratio.)
- **`b_wrap ≥ 1.0` → the highest degree spills down.** Region `[0, b_wrap − 1.0)` →
  `d_{m-1}` with **−1**. Example: D(0.01)@0.167 lowest / B(1.0)@0.917 highest →
  `b_wrap = 1.165`, so `u ∈ [0, 0.165) → B, −1`: 1.05 V snaps down to B of the lower
  octave because strong B out-pulls faint D.

The bug case at the seam: C(0.01)@0.000 / B(1.0)@0.917 → in-octave boundary 0.0091,
`b_wrap = 0.999`; C's slice `[0.999−1.0, 0.0091]` ≈ **0.0099 wide** (~1% of the
octave). A swept melody lands on C ~1% of the time.

Resolution algorithm: fold `u = vIn − floor(vIn)`, find the containing slice →
`(degree, delta ∈ {−1,0,+1})`, output `clamp((baseOct + delta) + degreeVolts(degree),
0, 5)`. One octave of boundaries, one tagged seam slice.

### Single active degree

No boundaries — the one degree owns the whole axis; every CV snaps to its nearest
instance. Special-cased (`activeSemiCount == 1`).

### Clamp edges (0 V and 5 V)

Output clamps to [0,5]. A slice whose centre is outside [0,5] (an octave −1 or +5
instance) is suppressed, and the lowest/highest in-range instance absorbs the tail.
So the axis still tiles within [0,5].

### 0 V (patched)

No "no-note" fallback — the axis tiles, so a genuinely-patched 0 V maps to the
**lowest active degree at octave 0** (with C active, 0 V → C; with C muted, 0 V →
the lowest active degree, e.g. E → 0.333 V). Standard quantiser edge behaviour, and
rare for a real swept melody, so NOT the old "half the time" problem.

## Unpatched = pure generated (irrespective of QMIX)

Rodney: there must always be an easy way to get pure generated behaviour —
**unplug the CV**. An unconnected source has no external degree and no external
octave to borrow, so there is nothing to blend: fall straight through to
`genPitchLive` (generated degree AND octave), whatever QMIX is set to.

**Resolution is per CHANNEL, from the PORT not the value** (0 V is a valid pitch):

- Port unconnected (`!isConnected()`) → every voice generates.
- Poly cable connected → a voice whose channel index (`voiceIdx + 1`, ch0 = mono/V1)
  is `>= getChannels()` is NOT fed → that voice generates (QMIX ignored for it).
- A channel the cable DOES provide but carrying 0 V = a real note (patched 0 V →
  lowest degree). We cannot and need not go finer than `getChannels()`.
- The mono CV2 "Quantiser in" source is the exception to per-channel: mono broadcasts
  to all voices, so patched there is all-or-nothing at the port.

This already governs the **degree** axis today (`getBasePolyQmix`: `(ch < n) ? base :
0.f`, `n = getChannels()`; `getEffectiveMonoQmix`: `if (!pitchPatched) return 0.f`).
Forcing the degree threshold to 0 makes `r_qmix >= 0` always true → generated degree.

### The octave patched-detection gap (the fix this doc adds)

`quantiserPitchSource` is set true per-step regardless of patching; the fall-through
to pure `genPitchLive` only happens when **both** the degree and octave thresholds
resolve to "generate." The degree threshold is patched-detected. The octave threshold
`qmixOctaveThreshold()` resolves straight from the authored octave bank via
`QmixOctave::resolve` with **no patched-detection of its own**. In the DEFAULT case
(`followDegree=true`, `ownActive=false`) it returns the degree value and inherits the
gating — fine. But with the octave subpanel engaged (an *own* claim `ownActive=true`,
or *global* `followDegree=false`), it returns raw `ownTh`/`globalTh`. So:

> **unpatched input + octave subpanel own/global claim → octave threshold non-zero →
> the octave axis uses the external octave of a 0 V/absent note → the generated
> degree is pinned to octave 0.** The one case where "unplug = pure generated" fails.

**Fix:** the octave threshold must honour the SAME per-channel port-presence signal
as the degree. Unpatched (for that voice/channel) ⇒ force `qmixOctLevel` to generate,
so **unpatched ⇒ both thresholds 0 ⇒ `genPitchLive`**, whatever the octave bank says.
This is a ModeController/Monsoon (Rack-layer) change; the resolution logic is
unit-testable, the behaviour needs a Rack feel-test.

## How the qmix-octave blend consumes the quantiser

Octave-crossing is NOT new — today's `quantize()` already searches `o ∈ [octave−1,
octave+1]` and returns cross-octave results. The weighted partition only moves the
boundaries; the output is still `integer_octave + degreeVolts(degree)`, so the three
rows in `voicePitch` keep decomposing it correctly:

- **both external** → the quantiser's `volts` (degree + octave both the external
  note's, wrap included).
- **external degree, generated octave** → generated octave (`octTerm`) + the external
  note's within-octave degree. The degree is **wrap-invariant** (`frac` is always
  `degreeVolts(chosenDegree)` regardless of which octave the wrap assigned), and the
  octave is overridden by the generator, so the wrap is correctly discarded.
- **generated degree, external octave** → the external note's octave (`floor(vExt)` =
  `baseOct + delta`) + the normal generated (non-qmix) degree. Rodney's ruling: the
  octave is "the octave of the external note playing" — i.e. wherever the external CV
  quantised to, wrap and faders included (that IS where the note landed; the octave
  legitimately depends on the scale). = today's `floor(vExt) + split.degTerm`, which
  stays correct because `vExt` carries the wrap in its integer part.

**Structured return:** the core should return `(degree, octave, volts)`, not a bare
voltage, so the both-external and external-degree rows take `degree` directly instead
of round-tripping through `round(frac·12)` / `nearestDegree(frac)` (removes a latent
micro-tuning rounding mismatch).

## Build plan

1. **`src/dsp/QuantiserPartitionCore.hpp`** (Rack-free, pure):
   `weightedQuantize(positions[], weights[], n, vIn) → {degree, octaveDelta, volts}`.
   Precompute boundaries (or compute per call — N ≤ 24, cheap). One-octave partition
   + the single seam boundary with its ±1. Single-degree special case. [0,5] clamp
   with edge absorption.
2. **`test/test_quantiser_partition.cpp`** (red-first against the symptom):
   - A full CV sweep across [0,5], one degree at 1% among strong neighbours → asserts
     it wins ≈1%, NOT ≈50% (fails on today's `quantize()` behaviour, passes on the
     core). THE reproduction.
   - Seam scenarios: spill-up `+1`, spill-down `−1`, both worked examples above.
   - Single active degree → whole axis.
   - Clamp edges (0 V → lowest degree octave 0; near 5 V).
   - Uniform-weight byte-identity vs the current `quantize()` (incl. the midpoint
     tie-break convention).
   - Relative-weight property: two faint degrees split the octave in their ratio.
3. **`SequencerEngine::quantize()` → the core**, structured return threaded into the
   three `voicePitch` rows. Uniform-weight goldens must not move.
4. **Octave patched-detection fix** (Rack layer): unpatched (per-channel) ⇒ octave
   axis generates too. Unit-test the resolution; feel-test the behaviour.

## Settled decisions (Rodney, 2026-11-23)

- Fader in quantiser mode = quantization RANGE, relative within the active set.
- Tiny fader → sliver; `w = 0` → out of scale (zero sliver).
- Octave wrap handled in one octave + a ±1 delta on the single seam slice; micro
  scale is still one octave (1 V seam).
- Patched 0 V → lowest degree, octave 0. Unpatched → pure generated.
- Unpatched is per CHANNEL (`getChannels()`), from the port not the value; 100%
  generated for that channel irrespective of QMIX. CV2 mono source is port-level.
- "generated degree, external octave" rides the external note's octave (wrap/faders
  included) = today's behaviour.
- Needs the octave axis to honour the same per-channel patched-detection as the
  degree (the gap fix), so unplug = pure generated holds with the octave subpanel on.
