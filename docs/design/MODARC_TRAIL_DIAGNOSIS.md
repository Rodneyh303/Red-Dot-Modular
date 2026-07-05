# Mod-arc trail — root-cause diagnosis (and why padding didn't fix it)

## Two DISTINCT problems, now separated

### A) NOTE knob permanent line — FIXED (commit 8437a9d)
Pure normalization mismatch: setNorm = getScaledValue() = idx/7 (param range 0..7),
but modNorm = getNoteValueNorm() = getNoteValue()/8. Never agreed → permanent arc,
no modulator needed. Fixed: norm ÷7, and getNoteValue() range 0..8→0..7 (matches
configSwitch + the getNoteLenIdx [0,7] consumer). This one is done.

### B) Trail/smear on turning ALL modulatable knobs — diagnosed, NOT yet fixed
NOT a box/containment problem (the arc draws INSIDE its padded box — proven by the
radius math; the earlier padding work was aimed at the wrong cause). It's a
SET-vs-MOD SOURCE DESYNC:
  - setNorm = paramQuantities->getScaledValue()  — LIVE, every UI frame.
  - modNorm = modViz.<field>                     — a snapshot published only on the
    throttled lightDivider cadence (Monsoon.cpp ~517, "Throttle UI and Light").
During a manual turn, the live set value races ahead of the throttled mod snapshot.
For ~one throttle interval they differ. One note-index step = 1/7 ≈ 0.143, while
minDelta = 0.01 — so the lag-induced delta is ~14× the draw threshold → a clearly
visible transient arc each frame of the turn. Because the overlay's prior-frame
pixels aren't cleared by the parent every frame, those transient arcs read as a
"trail/smear." With NO modulator attached, set and mod are the SAME underlying
value sampled at different times — so the turn alone manufactures the delta.

isActive() (anyBig5Modulated) is ALSO in the throttled snapshot, so it can't
reliably suppress the transient (it lags too, and the value delta is what draws).

## Recommended fix (pick one)

OPTION 1 — make modNorm = setNorm + offset, from ONE source (BEST, kills it at root)
Instead of modViz carrying the absolute modulated value (snapshot), carry just the
OFFSET (modulated - base), and have the arc compute modNorm = getSetNorm() +
offsetNorm. Then with no modulation offset == 0 → modNorm ≡ setNorm EXACTLY, every
frame, no lag possible. A turn moves both together. Arc only ever shows real
modulation depth. Requires: modViz fields store offset (or add offset fields), and
the getModNorm lambdas do setNorm + offset. Moderate, but correct by construction.

OPTION 2 — live-sample both sides (simple, mostly effective)
Publish the modViz big-5 snapshot EVERY sample (move those ~10 lines out of the
lightDivider block), so mod tracks set with ≤1 sample lag instead of ≤1 throttle
interval. Cheap (10 float calls/sample), removes the visible lag. Doesn't fix the
"compositing doesn't clear" root, but with <1-sample lag the delta never exceeds
minDelta from timing alone, so nothing draws during an unmodulated turn. Lowest-risk
one-liner-ish change.

OPTION 3 — gate on offset magnitude, not value delta
In draw(), require an explicit "offset != 0" flag (computed live, not throttled)
before drawing. Equivalent effect to 1 but needs a live offset signal anyway.

RECOMMENDATION: Option 2 first (move modViz publish to per-sample) — smallest change,
directly removes the lag that manufactures the delta, easy to test/revert. If a
faint trail persists from genuine compositing (prior-frame pixels), THEN add the
overlay-redraw/dirty handling. But I believe the desync is the dominant cause and
Option 2 alone will fix what you're seeing. Option 1 is the "proper" version if you
want it bulletproof and don't mind reworking modViz to carry offsets.

## On the poly Straits East/West rest knobs (no trail seen)
You noted these don't trail — consistent with them "not being wired for mod arcs
properly," i.e. their getModNorm/getSetNorm aren't both live+snapshot in the same
way, so no desync. If/when they're wired, apply the same Option-2 discipline.
