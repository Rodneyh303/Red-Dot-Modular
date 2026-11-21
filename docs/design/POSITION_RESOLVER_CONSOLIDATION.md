# PositionResolver — what to consolidate, and the sibling it implies

Status: a NOTE, not a task. Context: `PositionResolver` is still minimal after
weeks of work on position. That is partly the good outcome — the class holds a
hard-won invariant and the complexity went into *finding the law*, not into code
— but the law is currently **re-applied by hand at each read site**, and that is
the direct cause of the repeated reverse / rotation off-by-one bugs: the fix has
to be threaded to ~10 sites (the `anchorBias` fan-out), and a new mode (abs1) or
a new surface (the UI playhead) re-derives it and gets it wrong again.

## The symptom that justifies consolidation

Every recent bug in this family is the SAME law applied inconsistently in a
different place:

- the voice frame read, the macro frame read, `laneRecomp`, the UI display twin,
  the two `voiceFrameTickAt` seeds — the `anchorBias(+1)` reverse fix had to land
  at all of them (7aae5ee9), and the sign-law twin requirement existed only
  because the law was spread across paired sites.
- the commit edge keyed on `vCoord==0` (rotation) instead of the window edge — a
  position concept, re-expressed in `voiceFrameEdgeLive_` (638879aa).
- the reverse playhead reads rotation content at the wrong edge SLOT — the same
  content↔slot↔box-offset mapping, re-expressed again in the UI
  (CC_PROMPT_REVERSE_VOICE_ROTATION_EDGE).

One authority for the position law would turn each of these into a one-site fix
with one R-sweep test, instead of a fan-out plus a per-surface rediscovery.

## What deserves to move INTO PositionResolver (the position law)

The law is: one signed phrase coordinate, direction-agnostic, the anchor a
consequence not a stored clamp. Concretely, the authority should own:

1. **The signed phrase coordinate** — `sPosV` / `vCoord` (phrasePos with the
   transport-direction sign, plus the per-voice `advanceOffset_`, mod the current
   global length). Currently inlined in `voiceFrameEdgeLive_` and the read sites.
2. **The walk + rotation + reverse-anchor fold** — `laneTickFor(d, t, L) + rho +
   anchorBias(d)`, i.e. the rotation anchor and the reverse `anchorBias(+1)`. This
   is the ~10-site fan-out. `laneTickFor` stays the primitive (goldens depend on
   it); the resolver owns its APPLICATION so the bias is defined once.
3. **The window-edge / box-offset mapping** — box-offset `wnow`, the edge
   (0 left / L-1 right), and "which edge is the direction-entry edge" (left fwd /
   right rev). The commit edge and the playhead-at-edge property both read this.
4. **The content↔slot↔box-offset mapping** — where rotation content `R` sits in a
   slot, and the reverse mirroring (content R at the RIGHT slot in reverse). This
   is the one that bit the UI playhead; it must be the SAME mapping the engine
   read uses, served from one place, not re-folded in `SandsVisualEditorV4`.

Keep the resolver a thin pure authority: inputs (phrasePos, direction,
advanceOffset, L/O/R, global length) → outputs (signed coord, read cell, box
offset, is-at-entry-edge). No engine state mutation; the engine and the UI both
CALL it rather than each re-deriving.

## The sibling it implies — an ADDRESS / WINDOW resolver (closely related, separate)

Rodney's question — "anything similar for resolving anything closely related" —
yes: there is a second, distinct law tangled into the same read sites, the
**addressing** law (UAS), which answers *which cells are live and gated*, as
opposed to *where/when the playhead is*:

- the extent `min(global, voice, lane)`;
- cycle vs abs1 (MASK_CYCLE / MASK_DYNAMIC) and the painted overlay
  (abs2 static / abs3 follow-R), the evaluation ring = current global phrase;
- the mask rotation / `rotMaskRing` / `effectiveOverlayPattern`.

Position and addressing INTERACT (the playhead moves within a window the
addressing defines) but they are separable concerns and are currently both
inlined in the engine reads. A sibling `AddressResolver` (or `WindowResolver`)
owning the addressing law would do for abs1/mask what PositionResolver would do
for the reverse/rotation family: one place, one test surface. The abs1 commit-edge
item (still open, `voiceFrameReadsFullPhrase` keys on `vCoord==0`) is precisely a
point where the two laws meet and both are currently re-derived.

## Payoff / boundary

- PositionResolver: *where and when* the playhead is (coord, read cell, box
  offset, entry edge, rotation fold incl. reverse anchor, post-mod rotation jump).
- AddressResolver: *which cells are live/gated* (extent, cycle/abs, mask overlay).
- The recurring reverse off-by-ones are all PositionResolver-law issues; the abs1
  / mask drift is AddressResolver-law. Centralising each turns a fan-out fix into
  a one-site fix with an R-sweep (position) or an extent/ring sweep (address) test.

Elective, not urgent. But every further read site added without this makes the
next reverse/rotation off-by-one more likely, not less.

## C++ concepts — where the SandsModifierPolicy pattern transfers (and where it doesn't)

`SandsModifierResolver` earned its C++20 concept by unifying FOUR drifted
hand-rolled delegation shapes (spread/warp/LOR/direction): the `SandsModifierPolicy`
concept abstracts the per-modifier value math over one fixed, once-tested
delegation skeleton and BITES — an incomplete policy is a LOCATED compile error at
its declaration, not a deep instantiation failure. The justifying shape is N
structurally-similar strategies with a clean per-type boundary.

PositionResolver's CORE does NOT have that shape: it is ONE traversal law over ONE
state (the played column), parameterised by a small closed direction enum and the
window. A concept over the core is ceremony — the right proof there is the
bit-identical pure-mirror harness (test_position_resolver_identity, slice 1), not a
type constraint. Do not add a concept just to mirror the modifier resolver.

WHERE the pattern DOES transfer — the strategy axes the consolidation implies:

- a `WalkPolicy` concept over DIRECTION (Forward/Reverse/Pendulum/PingPong), each
  declaring `{ tick (via laneTickFor, UNCHANGED), anchorBias, entryEdge }`, with the
  shared skeleton folding rotation / offset / box-offset / content↔slot ONCE. This
  is the true analogue of SandsModifierPolicy, and it attacks the reverse
  off-by-one family at the root: "reverse forgot its right-edge rule" becomes a
  LOCATED COMPILE ERROR instead of a feel-test discovery, and `anchorBias` stops
  being a ~10-site fan-out and becomes one declared member of the reverse policy.
- an `AddressPolicy` concept over the sibling AddressResolver's modes
  (cycle / abs1 / abs2 / abs3 painted), each declaring `{ ring, extent, gate,
  content-map }`. Same payoff for the abs1 / mask drift.

Carry over UNCONDITIONALLY (concept or not): the step-1 discipline — a
BYTE-IDENTICAL bridge test proving the extraction changes nothing vs what ships,
before any behaviour change (how SandsModifierResolver landed spread/warp/LOR/dir,
and how PositionResolver slice 1 is already proven). That discipline, not the
`concept` keyword, was the modifier resolver's real win.
