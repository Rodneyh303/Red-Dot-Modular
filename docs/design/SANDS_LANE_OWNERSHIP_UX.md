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

## Option C — concrete concept: the owner cell as a "17th step"

Preferred placement: an **owner cell column immediately right of the step grid,
before the prob-out jacks** — one cell per lane, drawn in the lane's own colour,
reading as an extra step of the lane. Click the cell to toggle ownership.

### Geometry (East, real numbers)
- Editor: `ED_X=88`, `ED_W=111`, so the grid right edge is at **199mm**.
- Prob-out jacks: `PROB_OUT_X=207mm`. → there's already an **8mm gap** between
  the grid and the jacks.
- One editor step ≈ `ED_W/16 ≈ 6.9mm`. So a one-step-wide owner cell (~6.9mm)
  *nearly* fits the existing 8mm gap. Bumping `PROB_OUT_X` right by ~3–4mm (and
  widening the panel a hair, well within 42HP slack) gives clean breathing room
  on both sides of the cell.
- Vertically the cell uses the lane row exactly (`ED_LANE_H`), so it lines up
  with the lane it controls — no new vertical math.

### Why this placement is good
- **Collision-free:** it's outside `ED_W`, so the editor's `onButton`
  grid/handle hit-test (which is bounded by the grid) never sees it. The cell
  gets its own hit-test.
- **Reads as part of the lane:** same colour, same row height, sitting at the
  end of the row — it looks like where the lane "resolves to", which is exactly
  what ownership means (where this lane's value comes from).
- **Indicator and control in one:** the cell's appearance *is* the ownership
  state; clicking it flips it. No separate legend needed.

### Visual treatments for the cell (the rendered mock shows A and B)
- **A — fill vs outline:** solid lane-colour cell = **global/Macro-owned**
  (value comes from the shared base); hollow outline in lane colour =
  **per-voice/East-owned** (this voice's own edit). Cleanest, most glanceable —
  "filled = filled-in-for-you globally, outline = you draw it."
- **B — dim + glyph:** cell always present; bright + "G" = global, dim + "V" =
  per-voice. More explicit but busier; the glyph is redundant if the fill
  already encodes it.
- **C — split tab/notch:** the cell rendered as a little tab that visually
  "connects" to the grid when per-voice (this voice owns the pattern) and
  detaches/greys when global. Cute but subtle; A communicates faster.
- **D — left-edge bar variant:** instead of a right-side cell, a colour bar at
  the lane's left edge that's solid (global) or striped (per-voice). Works, but
  the right-side cell is more obviously a *button* and doesn't crowd the lane
  labels.

**Lean: treatment A** (fill = global, outline = per-voice), one cell per lane,
~one step wide, right of the grid, panel widened a few mm so it sits cleanly
before the prob-outs. Pair with the **faint lane-background tint** (global-owned
lanes get a subtle wash) so ownership is legible even peripherally, not only at
the cell.

### Implementation sketch (kit-friendly, low custom-code)
Two ways, both avoiding editor-internal mouse code:
1. **Kit-bound button param per lane** (truest to Option C): the gen script
   emits an `owner cell` marker per lane at `(grid_right + ~3.5mm, laneCenterY)`;
   bind a custom momentary/latch `ParamWidget` there (`param_owner_{lane}` →
   `ownerDispId(lane)` proxy). The widget draws itself in lane colour as
   fill/outline per its value. Automatable, saves/restores free, no editor
   changes. The per-voice sync rides the existing `ownerDispId` ↔ per-voice
   `ownerId` proxy pattern (same as attens/sends on tab change).
2. **Editor-drawn cell** (if we want it visually inside the editor widget): the
   editor draws the cell in its `draw()` right of the grid and hit-tests
   `e.pos.x > gridRight` in `onButton` before the grid logic, calling
   `onLaneOwnershipToggle(lane)`. More cohesive visually but adds editor mouse
   code — the thing we've been trying to keep thin.

Sketch 1 is preferred: it keeps the editor a pure data surface and treats
ownership as just another bound control, consistent with the kit refactor. The
cell *looks* like part of the lane without the editor having to own it.

Same cell appears on **Mono** (for V1, `ownerId` against Mono's edit vs Macro
global) and **East** (for V2–V16). Both get the marker + bound widget; Mono's is
always visible (it's V1's owner), East's is per poly tab.


## Option C mocked on the real East panel — enclosure decision

Two in-context mocks (docs/design/img/):
- `owner_cell_on_east_v1_enclosed.png` — owner column with a hard enclosure box
  + dashed separator + "OWN" header. Unambiguously a separate control group,
  but the box reads a bit heavy against the clean editor.
- `owner_cell_on_east_v2_subtle.png` — faint backing strip (5% white) + one
  thin separator line + small "SRC" label, PLUS a faint whole-lane-row tint for
  global-owned lanes. Quieter, more consistent; the row tint (not the cell) does
  most of the indicating.

**The cell needs a visual break from the grid** (a bare cell reads as a 17th
playable step). Both mocks add one; the question is how heavy:
- Minimum viable break: a single separator line + small gap is enough to stop
  the "17th step" misread, especially with a header label ("SRC"/"OWN").
- The whole-lane-row tint (v2) is the bigger win for legibility — ownership is
  readable across the lane, peripherally, with the end cell as the toggle and
  the row tint as the indicator.

**Lean:** v2's restraint (separator + label + gap, no hard box) + the lane-row
tint — but make the tint a **faint desaturated version of the lane's own colour
or a neutral grey**, NOT the blue used in the mock. Blue risks reading as
"selected/active" rather than "global-owned"; staying in-palette keeps it
unintrusive and consistent (the stated priorities).

**Spacing:** the column lands at ~199.5mm, prob-outs at 207mm — snug. Nudge
PROB_OUT_X right ~3–4mm and widen the panel slightly so the owner group has
clean air on both sides. Well within 42HP slack.

**Header label:** "SRC" (source) or "OWN" — SRC pairs better if the PRE/POST
tap (also a "source" notion) lands later; OWN is more literal. Minor.

## Fine-tuning observations (to action WITH the owner-cell build)

These are coupled to implementing the owner cell, so they should land together
in one coordinated change, not piecemeal:

1. **East's bottom BASE-latch blocks become redundant.** East currently draws
   per-lane BASE opt-in latch groups at `BLEND_TOP=74` (markers `param_owner_{l}`,
   bound by the widget). The owner cell in the editor REPLACES these. When the
   owner cell is built, move the `param_owner_{l}` binding to the new cell and
   **delete the bottom latch groups** — they free a ~22mm-tall band under the
   editor on East. (Do NOT delete the markers before the cell binds them, or
   the bind goes unbound / the widget loses the param.)
   - Frees East's lower band → more room if the editor later grows down
     (SANDS_PANEL_LAYOUT.md option) or for artwork.

2. **Owner-cell enclosure must not overlap the editor widget.** In the v1/v2
   mocks the enclosure sat snug against the editor's right edge (grid right
   ≈197mm, enclosure starting ≈199.5mm) and visually kissed the lane widget.
   When building: give the enclosure a clear gap from `ED_X+ED_W` (the editor
   widget's box), i.e. start it a few mm clear, and pull the prob-out jacks
   right to make room — do not let the enclosure stroke touch the editor recess
   border.

3. **Same observations apply to the other Sands panels.** Macro's bottom is the
   send grids (those stay — they're the sends, not ownership). Mono has no
   bottom ownership blocks but DOES need the owner cell for V1 (per the
   "ownership follows the editing surface" conclusion), so Mono's editor gets
   the same right-of-grid owner cell treatment.

4. **HP headroom for lane width.** Lanes are currently ~6.9mm/step. If we want
   nicer lane width (and clean spacing for the owner cell + prob-out gap), we
   can widen the panel — add HP. This is the natural moment to do it since the
   owner cell already wants ~3-4mm more on the right. A coordinated spacing pass
   (add N HP, re-space columns for breathing room, place owner cell, drop East's
   bottom blocks) is cleaner than nudging single mm values. Decide HP count when
   building: enough to give the step grid a comfortable width AND fit the owner
   column + prob-out strip without anything snug.

**Sequencing:** still gated on confirming the Macro kit build first. Then the
owner-cell build is the coordinated change that also does (1)-(3) and ideally
(4).
