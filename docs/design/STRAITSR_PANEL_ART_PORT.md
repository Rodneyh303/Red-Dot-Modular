# StraitsR panel artwork — port from original + edge-tiling for seamless expanders

The refactor (StraitsR: gen_straits_base.py + gen_straits_lane.py) is MISSING art the original
(gen_straits.py) has. Four fixes on branch lane-expander-refactor.

## 1. Wave-field background behind each lane — DEFINED but not DRAWN
gen_straits_lane.py defines wave colours per tint (rest wave=#2a5a56, accent #8a5410, qmix #4e3a78)
but never CALLS a wave draw — that's why the wavy background is absent. **Port `wave_field(A, t, x0,
y0, w, h, colour, n)` from gen_straits.py:77** (flowing contour "water" lines) and call it behind each
lane's knob grid using that lane's `wave` tint.

## 2. Bottom wave footer — needs an EDGE-TILING V2 (the non-obvious part)
The original's Marina Bay bottom wave must read as ONE CONTINUOUS wave across base + any combination of
attached lanes. The panels can't know their rack-X at SVG-generation time, so use the EDGE-MATCHING
approach: **make the bottom wave PERIODIC with period = panel width (or a divisor of it), phase-anchored
so EVERY panel's LEFT edge and RIGHT edge sit at the SAME wave phase/Y.** Then the wave at any panel's
right edge == the wave at the next panel's left edge, for ANY base+lane pairing -> seamless across
arbitrary combinations. (A per-panel random/free wave would NOT tile — it must match at edges.)

## 3. Inter-panel vertical separator lines
Original draws vertical lines between banks (gen_straits.py:161). **Port them** — a vertical separator
spine at each lane/base boundary, so abutted sub-panels show the separator as the original did.

## 4. Seam not exactly seamless (close-up shows a gap at the top-rule / edges)
Base<->lane abutment isn't bit-aligned. **Verify:** base and lane panels have IDENTICAL top-rule Y +
height + colour (the red rule), identical bg fill, and the lane's left edge sits EXACTLY at the base's
right edge (no sub-pixel gap). The seamless look requires shared bg/rails bit-aligned across the seam.

## Verify
Attach base + various lane combinations: continuous wavy bg behind lanes; continuous bottom wave across
ALL panels in ANY combination; vertical separators between sub-panels; truly seamless seam (no visible
gap at the top-rule or panel edges).

## Same model later applies to CAUSEWAY
Causeway's lane expanders (CV-mod-input lanes) use the same panel-extension model, so the edge-tiling
bottom wave + wave-field + vertical lines + seam precision carry over — build them reusably (shared
helpers) so Causeway inherits them.
