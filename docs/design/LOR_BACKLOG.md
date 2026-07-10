# LOR backlog — per-lane DIRECTION (reflection)

**Status: LOGGED, NOT RECOMMENDED. Do not build without re-reading §4.**

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

**4b. "Reverse" already has a precise, different meaning here.** `src/dsp/SquaresRng.hpp` is a stateless,
addressable counter-based PRNG: `value(N) = squares64(N, key)`. Phase mode (E) reverses by walking the
**draw index** backwards — `advanceRhythmDraw(dir)`, *"negative indices are valid reproducible draws"* — so
backward phase **un-rolls the dice**, replaying the exact same draws. That is what direction means in this
engine, and it is a transport operation over the whole sequence. Introducing a second, unrelated "reverse"
at the lane level would make the word ambiguous precisely where precision has been load-bearing.

**4c. Cost is not small.** `lorStore_[16][6][3] → [16][6][4]`, a branch in `getStrandIdx`, persistence, and —
the real cost — another round of per-voice param banks (cf. the 90 ids added for VAR/LEG in
EAST_EXTRA_LANES stage 1b).

**4d. Payoff is bounded.** It doubles distinct read-orders per length from 16 to 32. Useful; not
transformative.

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
