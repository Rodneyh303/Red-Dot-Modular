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

## DECISION (2026-09-03): cursor-packing — nonce SET ASIDE (not just deferred)
**q-mix per-voice uses cursor-packing, consistent with rhythm/melody.** When the per-voice
source-select *consumer* lands, its draw mirrors polyMelody (`philoxSourceSelectAt(pos, c++)` per
voice). This is the CORRECT architecture here, not a stopgap — see why the nonce isn't motivated.

## Why the nonce is not motivated (corrected rationale)
The nonce's only advantages over cursor-packing are per-voice-**independent reversibility** and a
voice-count-agnostic layout. Both matter ONLY if voices are reversed / re-indexed independently.
**They are not.** There is one dice PER STREAM (rhythm, melody, CA, q-mix each) — but each dice
reverses/reseeds a whole STREAM, never individual voices. So reverse/reseed granularity is
PER-STREAM; the nonce's axis is PER-VOICE — a dimension nothing in the design ever addresses. (Per-voice reverse/scrub was never a design
goal; an earlier draft of this note wrongly leaned on it.) Cursor-packing already gives decorrelated
per-voice streams, which is all the design needs. Migrating a working, tested generation core to the
nonce would buy an unused property at pure risk — so it's set aside, not scheduled.

## The ONLY thing that would revive it
A feature that reverses or re-indexes individual voices independently of the others (not on the
roadmap, not envisaged). If that ever appears: adopt the nonce for **all** per-voice streams
(rhythm/melody/q-mix) in **one** pass — a mixed convention would be worse than either uniform choice
— via an `at(pos, nonce)` / `block(blockPos, nonce)` variant. (CA separates by *key*, a stream axis,
so it is orthogonal to this and likely stays as-is even then.) Until such a feature exists, this
stays theory.
