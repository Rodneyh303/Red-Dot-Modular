# 64-step idea — runtime 16/32/64 with a pop-out WIDER Sands

Status: IDEA, much later — after PANEL WORK and after CA5 (PositionResolver).
Not scheduled. Gated on one condition (below).

## The vision (Rodney, 2026-11-19)

- A runtime-selectable max-length MODE — 16 / 32 / 64 steps (a global module
  setting), not a compile-time cap flip.
- Inspired by Tessera's runtime variable-width subpanels. **GATE: only worth
  doing if the Sands editor can POP OUT WIDER for 64** — so cell width TRACKS
  step count (64 steps → ~2x wider Sands → the same cell width / feel as 32
  today), never 64 cells crammed into the current 32-width panel. Without the
  wider pop-out, don't do it.
- The Monsoon light circle then goes 4 laps instead of 2 (if the lap mapping is
  `step % lights`, this falls out for free).

## Why it's tractable (grounded in the tree)

- **Storage vs ring already separate.** Arrays size to `kStepStorage` (set =64,
  the compile-time max); the RING (`kMaskRing`/`kPatternLengthCap`) is the usable
  ceiling on top. The invariant `kPatternLengthCap <= kStepStorage` IS that
  separation.
- **The ring is never constexpr-locked.** Grep: zero uses of the ring as an array
  size or template param — only runtime comparisons. So demoting it from a
  `constexpr` to a runtime field is clean: no array resize, no re-instantiation.
- **Variable LENGTH is already fully runtime.** `effectiveVoiceLor` returns L/O/R
  as values; `walkFold`/`laneTickFor`/`wrapMod` and the `min(global,voice,lane)`
  extent all take length as a parameter. You are only making the CEILING vary,
  not lengths — the smaller ask.
- **The one overflow trap is already guarded.** `kMaskLenMask = (kMaskRing >= 64)
  ? ~0ULL : ((1ULL << kMaskRing) - 1ULL)`; masks are `uint64` and reserve 64 bits
  by design (`static_assert kStepStorage <= 64`).
- **Precedent:** Stage 2b already flipped the ring 16→32 "in ONE move, pre-gates
  green." Going to 64 (or runtime) is a repeat of an executed move.

## Two shapes for "wider Sands"

1. A wide Sands SUBPANEL variant that joins the Monsoon network like the existing
   subpanel family (Macro-send / Distribution / Phase-Dist — role-walk /
   left-link / drag-resnap). Fits the current architecture; the 64 mode = "dock
   the wide Sands."
2. A runtime-resized Sands surface (literal Tessera — `box.size.x` grows on mode
   change). More flexible; carries the VCV resize gotchas: a panel background that
   scales to width, the width saved in the patch, neighbour push, port/cable
   anchoring.

Either way cells stay legible: the grid already derives
`stepW = (ED_W - pad) / STEP_COUNT`, so wider host + `STEP_COUNT`=64 = same cell.

## Two genuinely-new design decisions (both have precedent to reuse)

1. **Dormant-on-shrink:** 64→16, do steps 16–63 vanish or persist DORMANT
   (restorable on switch-back)? Reuse the painted-overlay dormant-high-bits
   discipline (storage 64, evaluation ring = the active phrase).
2. **Switch-time transport:** changing the ring mid-play — re-anchor / clamp /
   keep counting? Same family as the live global-length-change work (C1 step-rate
   / C2 keep-counting).

## Cost, and the sequencing

- Engine half (runtime ring): mechanical — replace the `constexpr`
  `kMaskRing`/`kMaskLenMask` with a runtime read; compute lenMask + the all-ones
  sentinel at runtime; the `& (ring-1)` folds stay valid (16/32/64 all powers of
  2); drop `static_assert(STEP_COUNT == 32)` (it IS the compile-time-fixed
  guardrail). Persistence stores/restores the mode.
- UI half (the real engineering): `STEP_COUNT` becomes runtime, the grid relays
  out per mode, and the POP-OUT-WIDER is PANEL WORK — it folds into the panel work
  already planned, not a separate epic.
- Tests: three rings x the switch transitions.

**Order:** panel work (builds the pop-out-wider subpanel capability) → CA5
PositionResolver (prove the position law before tripling the ring — 64 gives an
off-by-one three times the places to hide) → then runtime 16/32/64 rides both.
**Skip the standalone compile-time 32→64 flip** — runtime includes 64 as a mode,
it is the superset; the flip would be partly-redone goldens work.
