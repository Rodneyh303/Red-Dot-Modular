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

## 3+4. SEAM — self-contained per-panel DOUBLE-RAIL (Rodney, redesign; supersedes "align perfectly")
Attempt 1: the edge-tiling BOTTOM WAVE WORKED (keep it). But cross-panel seamlessness (one continuous
vertical line + perfectly-seamless bg) FIGHTS Rack's imperfect panel alignment — abutted SVGs get
sub-pixel seam gaps (HP grid + zoom + float rounding), so a single line that must span the seam, or a
bg that must meet exactly, will show a glaring break. Don't chase it.

**Design around it — DOUBLE-RAIL:**
- **Each panel (base + EVERY lane) draws a vertical rail on BOTH its LEFT and RIGHT edges** (with the
  horizontal notches), entirely WITHIN its own SVG — perfectly positioned per-panel, zero cross-panel
  coordination.
- At each abutment: panel-A-right-rail + panel-B-left-rail = **two close parallel rails = an
  intentional DOUBLE-RAIL seam.**
- **Robust to misalignment:** a sub-pixel panel gap just changes the spacing between the two rails
  (unnoticeable) — where a single continuous line would show a break.
- **Covers the bg seam:** any dark gap between panel backgrounds HIDES BEHIND the rails, so the bg no
  longer needs to be perfectly seamless either. (Solves both #3 vertical-lines AND #4 seam at once.)
- **Outer edges** (assembly far-left / far-right): single rail (frames the module) — fine; optionally
  style the outer frame distinctly from inner double-rail seams.

Principle: don't make art span the seam; make each panel's edges SELF-COMPLETE (own L+R rails + notches
+ full-height bg), and let the abutment of two self-complete edges BE the design. Turns "seam must be
invisible" (unwinnable in Rack) into "seam is an intentional double-rail" (always works).

## 5. QMIX mod-arc GLITCH — shows at construction with NO modulation
The arc is active when getEffectivePolyQmix(v) != getBasePolyQmix(v). The earlier fix made
getBasePolyQmix apply the same patched-detection as effective (Monsoon.cpp:285-295) so they agree at
rest — BUT it checks `expanderManager.cachedPolyVoiceExpander` + `StraitsIds::QUANT_CV_INPUT` = the OLD
Straits, NOT the new StraitsR base. With StraitsR attached, cachedPolyVoiceExpander isn't the StraitsR
QMIX source, so patched-detection doesn't fire -> base returns the raw knob, effective returns 0 ->
they diverge -> arc shows even with no modulation.
**Fix:** make the QMIX patched-detection in getBasePolyQmix AND getEffectivePolyQmix (Monsoon.cpp:285,
301) check the STRAITSR base (the refactor's QMIX CV input), or whichever expander is actually attached
(old Straits OR StraitsR), so base == effective at rest regardless. Arc off at construction; on only
when real Causeway/quantiser CV is patched.
**Also grep for siblings:** other `cachedPolyVoiceExpander` / `QUANT_CV_INPUT` reads that assume OLD
Straits (REST/ACCENT patched-detection, etc.) — same root cause as the de-paramming crash (code
reaching for old Straits when StraitsR is attached). Fix any that'd misbehave with StraitsR.
Verify: fresh StraitsR + QMIX lane, non-zero knob, NO CV patched -> NO mod-arc. Patch quantiser/Causeway
CV -> arc appears.

## Verify
Attach base + various lane combinations: continuous wavy bg behind lanes; continuous bottom wave across
ALL panels in ANY combination; vertical separators between sub-panels; truly seamless seam (no visible
gap at the top-rule or panel edges).

## Same model later applies to CAUSEWAY
Causeway's lane expanders (CV-mod-input lanes) use the same panel-extension model, so the edge-tiling
bottom wave + wave-field + vertical lines + seam precision carry over — build them reusably (shared
helpers) so Causeway inherits them.
