# Philox nonce addressing — deferred option (come back to)

## The primitive
Philox is a keyed bijection `f(counter, key)` — independent RNG streams are just
non-overlapping *slices* of one function's address space, at ~zero marginal cost per
stream ("N RNGs for the price of one instance"). Three orthogonal address knobs:

- **position** within a stream → low counter half `ctr[0..1]` (what `at(pos)` walks)
- **stream / family** (rhythm / melody / q-mix / CA) → the **key** (`deriveKey(seed, +offset)`)
- **extra dimension** (e.g. per-voice) → the **nonce**, the high counter half `ctr[2..3]`

## Current state (verified in code)
`PhiloxRng::block()` hardcodes `ctr[2..3] = {0,0}` — the **nonce is unused by everything**:
- rhythm & melody get per-voice draws by **cursor-packing** — consecutive draws in the low
  half within a `pos` block (`for v: polyMelody[v][i] = philoxMelodyAt(pos, c++)`), voice folded
  into the cursor. `DRAW_CHUNK = 1024` is sized for this.
- CA separates its 8 scatter streams via **per-index keys** (`deriveKey(seed, STREAM_CA + i)`),
  position via the counter. (The "correlation nonce" in ChangeAlleyTransforms is a **key**-level
  discriminator, not `ctr[2..3]`.)

So the reserved `ctr[2..3]` is a genuinely free fourth axis nobody has touched.

## DECISION (2026-09-03)
**q-mix per-voice uses cursor-packing NOW — consistent with rhythm/melody.** When the per-voice
source-select *consumer* lands, its draw mirrors polyMelody (`philoxSourceSelectAt(pos, c++)` per
voice), not the nonce. q-mix does **not** diverge to nonce addressing on its own.

## Deferred: migrate ALL rng to nonce addressing together (if we do it at all)
Nonce addressing (`ctr[2..3] = voice`) is the *cleaner* per-voice model: each voice becomes an
independent 2-D address `(pos, voice)` off the same key — individually reversible in `pos`, and
voice-count-agnostic (no cursor-layout shift when voice count changes). That's a nicer property
for the reverse/scrub story.

BUT if we adopt it, adopt it for **all** streams (rhythm, melody, CA, q-mix) in **one** pass — a
mixed convention (q-mix on the nonce, the rest cursor-packed) would be worse than either uniform
choice. What it needs: an `at(pos, nonce)` / `block(blockPos, nonce)` addressing variant, then
route each stream's per-voice draw through it. Unit-test the same way: assert per-voice streams are
mutually independent and each independently reversible.
