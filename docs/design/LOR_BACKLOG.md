# LOR backlog — per-lane DIRECTION (reflection)

**Status: LOGGED, NOT RECOMMENDED — see §4e, which is the decisive one. Do not build without re-reading §4.**

Origin: comparing dot.modular's lanes to Reaktor's Kodiak **Shift Sequencer**, which has independent
sequence-length *and direction* per lane (pitch, octave, velocity, hold, gate). The lane sets correspond
almost exactly — Shift's `hold` is our LEGATO, its `gate` our REST — so the missing axis is conspicuous.

---

## 1. What we have

`getStrandIdx(tick, len, off, mut)` → `t = ((tick + mut) % L + L) % L; return (t + off) % 16;`

Three items per lane (`lorStore_[16][6][3]`): **LENGTH, OFFSET, ROTATION**. All are *shifts*. Together they
generate the **cyclic group** on a lane's read order.

## 2. What direction would add

`t = L-1-t` (reverse) or a fold (pendulum) → **reflections**, extending cyclic → **dihedral**. A reflection
is not reachable by any offset or rotation, so it is a genuinely new transform, not a re-parameterisation.

Taxonomy of what each knob does to a *probability* lane crossing a global threshold:

| knob | changes | density (slur/short rate) |
|---|---|---|
| LENGTH | which cells are sampled; the period | **changes** — a 6-cell window's mean ≠ the 16-cell mean |
| OFFSET | which cells, and where you start | changes (if len < 16) |
| ROTATION | phase within the window | unchanged |
| **DIRECTION** | contour / read order | **unchanged** |

So reflection is the **only** transform that alters a voice's articulation contour while leaving its rate
exactly untouched. That is the whole case for it.

## 3. It does NOT lengthen the cycle

Claimed in conversation, then tested and **false**. Reflection lives inside the modulus:

| lanes | both forward | one reversed |
|---|---|---|
| 6 vs 6 | period 6 | period **6** |
| 6 vs 4 | period 12 | period **12** |
| 6 vs 12 | period 12 | period **12** |

Multi-bar structure comes from `lcm(lengths)` alone. Direction adds none of it.

## 4. Why it is probably NOT worth building

**4a. Retrograde of noise is noise.** Shift's lanes hold *composed* values (a melody, a velocity contour),
so reading them backwards is a classical musical operation. Our lanes hold a **diced terrain**. Reversed, it
has identical statistics and no audible relation to the forward reading. Two voices that are mirror images
of a random field do not *sound* related.

**4b. "Reverse" already has a precise, different meaning here.** `src/dsp/PhiloxRng.hpp` is Philox4x32-10
(Salmon et al., Random123) — counter-based, therefore **stateless and addressable**: `philox4x32(counter,
key)` is a pure function, so the draw at index N does not depend on having generated 0..N-1. Phase mode (E)
reverses by walking the **draw index** backwards — `advanceRhythmDraw(dir)`, *"negative indices are valid
reproducible draws"* — so backward phase **un-rolls the dice**, replaying the exact same draws. That is what
direction means in this engine: a transport operation over the whole sequence, not a lane transform.
Introducing a second, unrelated "reverse" at the lane level would make the word ambiguous precisely where
precision has been load-bearing.

> Philox, not Squares: chosen for standard-library recognition — it is the same family C++26 adds as
> `std::philox_engine`, and this is a standalone C++17 implementation of the identical algorithm, verified
> against the canonical Random123 known-answer test vectors. Better-pedigreed than Squares (NumPy/TensorFlow
> default) and yields 4 x 32-bit outputs per block, which suits block-drawing the pattern arrays.
> `src/dsp/SquaresRng.hpp` remains as a drop-in alternative of the same shape (currently zero includes) and
> is slated for deprecation; the addressability argument above is identical for either.

**4c. Cost is not small.** `lorStore_[16][6][3] → [16][6][4]`, a branch in `getStrandIdx`, persistence, and —
the real cost — another round of per-voice param banks (cf. the 90 ids added for VAR/LEG in
EAST_EXTRA_LANES stage 1b).

**4d. Payoff is bounded.** It doubles distinct read-orders per length from 16 to 32. Useful; not
transformative.

**4e. The CONTROL-SURFACE cost is the real blocker (Rodney).** A fourth LOR item is not just storage — it
is knobs and modulation on every surface that exposes LOR:

- **East**: a per-voice DIR knob per lane. 15 poly voices x 4 poly lanes = **60 new params**, before
  VARIATION/LEGATO (which would add 30 more). Compare: the whole VAR/LEG feature cost 90.
- **Macro sends**: `sendId(v, lane, item) = MACRO_SEND_START + (v*4 + lane)*4 + item` — the item stride is
  hard-coded **4** (LEN/OFF/ROT/SPR). A fifth item changes the stride to 5, shifting **every id in the send
  bank**, and the same for `sendDispId(lane, item) = ... + lane*4 + item`.
- **Macro panel**: a fifth send row per lane group. Each group is `ED_W/4 ≈ 27.75mm` wide, laid out
  2 items per row at `SEND_DY = 9.5mm`. The send area was *just* shrunk (BLEND_TOP 72→76, SEND_DY 11→9.5)
  to free space below the lanes. Macro is the most space-constrained panel in the set. **The sends would
  have to grow, on the one panel with no room.**
- **Modulation**: a CV/mod path per new item, on East and via Macro sends.

So the ledger is roughly: +90..120 params, a 25% growth of the Macro send bank with an id-stride break,
a fifth row on a panel already at its limit, and new mod plumbing — to double read-orders and change
contour without changing density, over a field of dice.

Note the asymmetry that makes this sting: VARIATION/LEGATO are **LOR-only** (no spread, never
Macro-owned), so direction would cost them *no sends at all*. It is the four ordinary poly lanes — the ones
Macro does own — that carry the whole control-surface burden.

## 5. Where global direction correctly lives

`advancePlayhead(dir)` walks a single counter `totalStepsElapsed = (totalStepsElapsed ∓ 1) % DNA_LCM`, and
every lane reads it through `getStrandIdx`. Global reverse therefore rewinds the whole field at once, with
**relative phase between lanes exactly preserved** — plus the draw-index rewind of §4b. Given one counter
and an addressable RNG, that is the natural and correct home for direction. It is a transport transform,
not a lane transform.

(Heritage note: MeloDicer — the module this family descends from — does not support backwards at all.)

## 6. If it is ever built anyway

- Name it something other than "reverse" (e.g. **MIRROR**) so it cannot be confused with phase reverse.
- Confine it to lanes whose content is *composed*, not diced, if such lanes ever exist.
- Keep it out of VARIATION/LEGATO: those are LOR-only precisely because their probability field is shared
  and mono-owned. A mirrored read there changes contour but not rate — the least audible case of all.
