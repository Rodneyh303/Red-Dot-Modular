# LOR backlog — per-lane DIRECTION (reflection)

**Status: RECONSIDERED — see §7. The original "not recommended" verdict rested on two mistakes, both
Rodney's corrections. §4 is kept as written, with its errors marked, because the errors are the useful part.**

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

**4e. ~~The CONTROL-SURFACE cost is the real blocker.~~ — WRONG, see §7a.** *(kept for the record)* A fourth LOR item is not just storage — it
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


---

# 7. RECONSIDERED (Rodney's corrections)

## 7a. §4e is wrong: direction is BINARY

Length/offset/rotation are ints; spread is a float. **Direction is a bool.** There is nothing to attenuvert,
nothing to spread, and no meaningful *delta* for a Macro send to blend into a bool. So:

- no fifth send row, no `sendId` stride break, no growth of the send bank — **§4e evaporates**;
- no attenuverters;
- modulation is a **gate that flips direction**, not a CV that scales it. With MVC separation the gate can
  drive engine state directly.

Implementation, honestly measured:

- `getStrandIdx`: **one line** — `if (rev) timelineIdx = safeLen - 1 - timelineIdx;`
- storage, **per-lane** (the Shift-like design): `bool laneReverse_[NUM_STRANDS]` — *six bools*.
  Per-voice-per-lane would instead cost ~90 toggle params; almost certainly not worth it, and per-lane
  preserves the voices' relative phase, which is the thing that makes them sound related.
- UI: a lane toggle, plus 1..6 gate inputs. No knobs.

## 7b. §4a is wrong for a LIVE flip: it produces an audible palindrome

"Retrograde of a diced field is statistically identical noise" is true of a **fixed** reversed read. It is
not true of a **live** flip. Reversing mid-cycle makes the read-head walk back over cells it has just
played, and *that* is audible regardless of whether the field is composed or diced — it is the same reason
phase-mode scrubbing sounds like something.

Verified it really retraces rather than jumping to a mirror image. With `len = 6`:

```
forward   idx(t) = t % 6            → 0 1 2 3 4 5 ...
reverse   idx(t) = 5 - (t % 6)      → 5 4 3 2 1 0 ...
flip at t=3                         → 0 1 2 | 2 1 0 5 4 ...   (a genuine backwards walk)
```

Still a pure function of `(tick, dir)`: **no path dependence, no state.** One caveat — the stateless mirror
has a one-cell hitch at the flip (t=3 reads 3 forward, 2 reversed). A seamless bounce, `idx = (2*t0 - t) mod
len`, needs the flip time `t0`, i.e. state. Take the hitch; keep it stateless.

## 7c. What actually survives

- **§4b stands**: "reverse" already means *un-rolling the Philox draw stream* in phase mode. A lane-level
  flip is a different operation. **Name it MIRROR**, and the ambiguity is gone.
- **§3 stands**: it does not lengthen the cycle. Multi-bar structure is `lcm(lengths)` alone.
- **§4d is revised**: static reflection doubles read-orders 16 → 32 (marginal). A *gate-flippable* MIRROR is
  a live performance control, which is a different and better proposition.

## 7d. Revised verdict

The cheap, defensible feature is: **per-lane MIRROR — six bools, one line in `getStrandIdx`, a gate input
per lane, no knobs, no sends, no attenuverters, no Macro involvement.** Musically it is a live retrace, not
a static mirror. Cost is trivial; the earlier estimate of 90–120 params and a send-bank rewrite described a
design nobody proposed.

Two things to settle before building: (1) per-lane or per-voice-per-lane — per-lane is 6 bools and keeps
voices phase-related, per-voice is ~90 params; (2) whether the one-cell hitch at the flip is acceptable, or
whether seamless bounce is wanted badly enough to hold `t0`.

## 7e. Lesson

Two of the three arguments in §4 were wrong, and both because I reasoned about a *continuous* LOR item and
about a *static* reflection. Neither was the feature. The pattern is by now familiar in this codebase:
**the plausible-looking wrong thing compiles, and the plausible-looking wrong argument persuades.**
