# Sands — Lane Ownership UX options

Status: **options for discussion, not actioned.** Supersedes/expands
SANDS_COMBINATIONS.md question B and the UX note in
SANDS_ARCHITECTURE_CONSOLIDATION.md.

## The problem

Ownership = per lane, per poly voice, does the lane's base come from **East**
(this voice's own LOR edit) or **Macro** (the global base)? Today it's toggled
via a right-click context menu on the editor lane row.

**Why the context menu is fiddly (root cause):** the editor lane row *is the
data-editing surface*. `SandsVisualEditorV4::onButton` hit-tests LOR handles
and the window on that exact row (left-click drag = edit length/offset/
rotation). Right-click has a clean callback path (`onLaneRightClick`), but it
shares the same spatial zone as the draggable data, so:
- it's easy to be a few px off and grab a handle instead,
- in VCV a stray right-click can surface the module menu,
- there's no resting visual indication of current ownership — you have to open
  the menu to find out, then close it without nudging anything.

**Design rule for a good fix:** put the ownership control in a zone that is
**not** a data-editing surface, and make current ownership **visible at rest**.

## Editor geometry (the zones available)

From `SandsVisualEditorV4::Layout`:
- `topPadding = 18px` — header/label row above the lanes (NOT a data surface).
- `padding = 6px` — left inset before the step grid; lane name is drawn
  right-aligned at `padding - 8` (i.e. in a thin left gutter, NOT a data
  surface).
- Lane rows themselves (`getLaneY..+laneHeightF`) — THE data surface; avoid.
- Step grid starts at `padding`; handles/window live here.

So the **left gutter** (where the lane label already sits) and the **header
row** are the collision-free zones. The label gutter is the natural home —
it's already per-lane, already non-interactive, and reads as "the lane's
identity strip."

## Options

### A. Ownership chip in the left label gutter  ⭐ recommended
Widen the left gutter a little and put a small clickable chip per lane, beside
(or behind) the lane name. Click toggles East↔Macro for the current voice.

- **Collision-free:** the gutter is left of `padding`; the editor's onButton
  hit-test is for `x >= padding`. A chip here never overlaps handles/window.
- **Visible at rest:** the chip *is* the indicator — its colour/glyph shows
  ownership without opening anything (see "Visual indication" below).
- **One click**, on the thing it affects, on the correct voice (the editor is
  already showing one voice's lanes).
- Implementation: extend `Layout` with a `gutterW` (e.g. 14px), draw the chip
  in `drawLaneLabel`, add a hit-test in `onButton` for `e.pos.x < padding`
  BEFORE the lane-data hit-test, calling a new `onLaneOwnershipToggle(lane)`
  host callback. Right-click menu can stay as a power-user fallback.

### B. Click the lane LABEL text itself
No new chip — clicking the lane *name* (MELODY/OCTAVE/…) toggles ownership.
The label already lives in the gutter, so same collision-free zone.

- Pro: zero extra panel space; the label does double duty.
- Con: less discoverable (a name doesn't *look* clickable); and the label
  colour is already overloaded (selected-lane highlight). Workable but the
  chip (A) is clearer.

### C. Dedicated owner column of buttons (kit-bound params)
A vertical strip of 4 latch buttons (one per lane) just left of the editor,
bound via SvgPanelKit like every other control. This is essentially today's
"lower blend section latches" relocated to sit beside the lanes and aligned to
lane rows.

- Pro: real params (automatable, save/restore for free, no custom hit-testing).
  Aligned to lane rows so the mapping is obvious.
- Pro: fits the kit model we just adopted — markers in the gen script, bound in
  C++, no editor-internal mouse code.
- Con: costs panel width beside the editor; ownership is per-voice so the button
  must reflect/drive the *display proxy* (ownerDispId) that syncs per tab — same
  proxy pattern already used for attens/sends, so not new machinery.
- This is the most "in-grain" option for the current kit architecture and the
  least custom-code. Strong contender alongside A.

### D. Double-click lane row to toggle
Double-click anywhere on the lane toggles ownership.

- Pro: no new UI at all.
- Con: back on the data surface — exactly the collision we're trying to escape.
  A double-click still starts with a single click that the drag logic sees.
  Rejected for the same reason the context menu is fiddly.

### E. Modifier + click (e.g. Ctrl/Alt-click the lane)
Hold a modifier and click the lane to toggle ownership.

- Pro: no new UI; unambiguous vs plain drag.
- Con: invisible affordance (nothing shows it's possible); still on the data
  surface so a mis-timed release could register a tiny drag. Better than D, but
  worse than the gutter approaches. Possible as a *secondary* accelerator.

## Visual indication of ownership (independent of the toggle method)

The strongest improvement may be the *indicator*, not the toggle. Make
ownership a legible lane state so the user reads it at a glance:

1. **Gutter chip colour/glyph (pairs with A):** e.g. filled dot = Macro
   (global), hollow/outline = East (per-voice). Or a tiny "G"/"V" glyph.
2. **Lane background tint:** Macro-owned lanes get a faint global-source tint
   (a cool/desaturated wash) behind the step grid; East-owned lanes stay the
   normal per-voice lane colour. Subtle, doesn't fight the data, and you see
   the whole pattern of ownership across lanes instantly.
3. **Left edge bar:** a 2px colour bar at the lane's left edge (in the gutter),
   colour-coded by owner. Minimal, crisp, kit-friendly.

Tint (2) + chip (1) together = you both *see* and *set* ownership without ever
touching the data surface. This is the user's "change lane visuals to indicate
ownership" instinct, made concrete.

## Ownership is a property of the EDITING surface, not of "East"

Reframing from SANDS_ARCHITECTURE_CONSOLIDATION.md: ownership is really
"**this voice's own per-voice edit** vs **the global base**," not "East vs
Macro." That changes *where the control must live*:

- **V2–V16** are edited by **East** → the owner control for poly voices belongs
  with East's editor (gutter chip / owner column).
- **V1** is edited by **Mono** (when present) → the owner question for V1 is
  "Mono's edit vs Macro's global base," so **V1's owner control belongs on
  Mono**, not East.

This surfaces a gap the old framing hid. **Combination 6 (Mono + Macro, no
East)** currently says "ownership latches not shown (no East)." But there IS a
real choice there: does V1's lane come from Mono's edit or Macro's global base?
Today V1 mirrors Mono unconditionally — there's no way to say "let Macro's
global base drive V1 for these lanes." If the owner control lives only on East,
this combination can never express it.

**Implication:** the per-lane owner control should exist on **whichever module
owns the editing surface for the displayed voice** — Mono carries it for V1,
East carries it for V2–V16. Same control concept, two homes. (This also means
Mono needs the gutter chip / owner strip too, not just East.)

## Alternative home: Macro's bottom control section

The user's second idea: put **both** the per-lane owner control **and** the
PRE/POST CV-tap choice in Macro's lower send section, rather than on the editor.

Macro's lower band already has 4 per-lane send groups (`BLEND_TOP=72`, each a
labelled 2×2 box with a header line at `+7.5`). That band is the natural home
for Macro-local routing decisions:

- **PRE/POST CV-tap → strongly fits here.** It's a property of Macro's own CV
  path (does the per-voice send tap before/after the global attenuverter), and
  it sits literally on top of the sends it governs. A small per-group (per-lane)
  pre/post switch in each send box's header is the obvious placement. This is
  the best argument for the bottom section.
- **Owner control here → workable but weaker.** Ownership is per-voice and
  per-lane; the send groups are already per-lane and per-(view)voice, so a
  small owner toggle in each group's header is consistent. BUT: ownership is
  conceptually about the *editor's* lane (where the value is shown/edited),
  so putting it on Macro's sends splits "what you edit" (editor) from "who owns
  it" (Macro bottom) across the panel — the user looks at the East/Mono editor
  to see the lane but at Macro to change ownership. That's the same
  look-here-act-there friction the context menu has, just relocated.

**Resolution:** these two controls have different natural homes.
- **PRE/POST CV-tap → Macro bottom section** (per-lane switch in each send
  group header). It's Macro-local and belongs by the sends.
- **Owner control → on the editor surface** (gutter chip / owner strip), on
  **both Mono and East**, because ownership is about the lane you're looking
  at and must work when Macro is absent (combinations 5, 2) and when East is
  absent (combination 6). Putting it on Macro's bottom would break exactly the
  no-Macro combinations.

A nice consequence: when Macro IS present, its bottom section shows the
pre/post + send depth (Macro's contribution), while the editor gutter shows
ownership (whose value wins) — each control sits with the thing it most
naturally belongs to, and neither depends on Macro being attached.

## Recommendation

- **Owner toggle:** put it on the **editor surface** so it works regardless of
  Macro, and on **both Mono (for V1) and East (for V2–V16)** since ownership
  follows the editing surface. Implementation: Option **C** (kit-bound owner
  strip aligned to lane rows) is the lower-risk, in-grain choice; Option **A**
  (gutter chip) is slicker but adds editor-internal hit-testing. Either way the
  same control appears on both Mono and East.
- **PRE/POST CV-tap:** put it in **Macro's lower send section**, a per-lane
  switch in each send group's header — it's Macro-local and belongs by the
  sends.
- **Indicator:** do the **lane background tint + gutter mark** on the editor
  regardless of toggle choice — the visible resting state is most of the felt
  improvement, and it lives where the user is already looking.
- Keep the right-click menu as a harmless power-user fallback.

## Note on the bigger picture

If any East/Macro merge lands later (SANDS_ARCHITECTURE_CONSOLIDATION.md), the
editor owner strip and the Macro-bottom pre/post switch would simply move into
the one merged poly panel together — building them now as clean per-lane
controls is forward-compatible, not throwaway. The "owner lives on the editing
surface" principle also means the merged module's editor carries ownership
naturally, and Mono keeps its own V1 owner strip whether or not the poly side
merges.
