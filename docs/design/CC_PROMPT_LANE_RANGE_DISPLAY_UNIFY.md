# CC_PROMPT — Unify the lane-range DISPLAY onto the latched authority (+ Option-3 mod preview)

## READ THIS FIRST
- **CARDINAL RULE**: verify against code and tests, never a plausible narrative. If you
  fix behaviour, prove it with a test that FAILS before and PASSES after. A lying
  abstraction plus confident-but-wrong "fixed" claims cost this project weeks.
- **Widgets CANNOT compile headless.** The display repoints in Stage A are therefore
  verified *structurally* (grep + build) and by Rodney's Rack feel-test, NOT by a
  headless display test. The only headless-testable work here is the engine-side
  preview accessor in Stage B — that one gets a reproduce-first unit pin.
- **Branch discipline**: cut `fix/lane-range-display-unify` off `lane-expander-refactor`
  (the current merged tip — cadence C0–C4, the macro-lane-holder retirement, and the
  draw-cache bound are all in). PR back into `lane-expander-refactor`.
- Attribution: end commits with
  `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` +
  `Claude-Session: https://claude.ai/code/session_01NwkuvtoLbVAZwRhFfNo5t5`.

## THE BUG (reproduced in Rack, 2026-11-16)
One LFO modulating a **macro probability-lane offset** and an **east octave-lane offset**:
- The **Macro strip's lane range glides smoothly at video rate** (tracks the LFO; faster/
  slower with the LFO rate).
- The **East lane ranges (mono + poly tabs) do not move at all** — the directly-modulated
  east octave and a macro-following melody lane both frozen.

ROOT CAUSE — the lane-range DISPLAY reads three divergent sources, NONE of which is the
latched value the audio walks (`effectiveLaneLor = base + laneLorCv_`, committed at the
voice-frame edge):

| Site | Reads | Why it's wrong |
|---|---|---|
| `StraitsSandsMacroVisual.cpp` ~1067 / ~1375 / ~1400 | `macroBase[l] + macroCVDelta[l]` | the manager's `applyMacroCV` (`MonsoonSandsManager::publishGlobalProduce_`) recomputes this **live every control block**, un-latched → **glides**. A Controller-computed scratch stashed on the View widget — an MVC leak C1b never brought under the latch. |
| `StraitsEastSandsVisual.cpp` ~1898 (mono/V1 tab) | `eng.strandOffRef(el)` etc. | the engine **base store** — pre-C1b it was overwritten with base+CV every block (so it moved); C1b moved the CV into the separate latched `laneLorCv_`, so this now reads **static base** → **frozen**. |
| `StraitsEastSandsVisual.cpp` ~1930 (poly tab) | `eng.polyOffE(...)` etc. | same — raw base store, no CV. |

The dead `mirrorMonoExtraLanes` `[7,7)` loop (`StraitsEastSandsVisual.cpp` ~1033) is a
separate, pre-existing no-op — fix it in passing (see Stage A).

The **voice-range** path is already unified and latched (`effectiveMacroRowLor` /
`effectiveVoiceLor`, both strips) — it behaves. It is the TEMPLATE for this change. Do NOT
touch it.

## THE DECISION
Route **every lane-range DISPLAY read** through the engine's latched authority
(`effectiveLaneLor` / `effectiveMonoLaneLor`) for the SOLID window, and add a
`base + pending` **preview accessor** for the Option-3 modulation wireframe. One source of
truth for "the lane window"; the only video-rate motion left is the deliberate preview
outline.

**STAGING (Rodney's call): Stage A and Stage B land as SEPARATE COMMITS.** Stage A (the
fix — the lane ranges move when they are supposed to) commits and is Rack feel-tested on
its own FIRST; Stage B (the modulation overlay) is a later commit on top. Do not fold the
overlay into the fix commit.

This is the display-side completion of the C1b cadence AND an MVC/DRY unification (the view
reads the model authority, not a controller scratch or a raw model internal) — the same
move as the globalWalk_ / macro-lane-holder retirements, one level up.

---

## STAGE A — the fix (display-only; audio BYTE-IDENTICAL)

Repoint each lane-range display read to the latched accessor. These feed the View
(`setDisplayLOR` / `setRangeState`), never the engine, so no golden can change.

1. **Macro strip** (`StraitsSandsMacroVisual.cpp` ~1067, ~1375, ~1400): replace the
   `macroBase[l] + macroCVDelta[l]` L/O/R triples feeding `setDisplayLOR` with
   `eng.effectiveLaneLor(<the macro row's LOR-representative voice>, LaneId{l}, ...)` —
   the SAME representative the retired-holder view uses (`macroLaneDisplay` already reads
   `effectiveLaneLor` of the lowest-indexed macro-follower; reuse that exact choice so the
   range and the playhead agree). The `macroBase`/`macroCVDelta` fields STAY (the
   send/tap deltas `macroPreDelta`/`macroPostDelta` still need them) — only the *range
   display* stops reading them.
2. **East mono/V1 tab** (`StraitsEastSandsVisual.cpp` ~1898): replace
   `strandLenRef/strandOffRef/strandRotRef` with `eng.effectiveMonoLaneLor(el, ...)`.
3. **East poly tab** (~1930): replace `polyLenE/polyOffE/polyRotE` with
   `eng.effectiveLaneLor(slot(pv+1), LaneId{l}, ...)`.
4. **Dead mirror loop** (~1033 `mirrorMonoExtraLanes`): the `[POLY_LANES, EAST_LANES)` ==
   `[7,7)` loop never runs. Either delete it (preferred — it's dead) or, if a VAR/LEG
   mono-tab display path is intended, fix the bound; document which and why.

Keep the clamps the display already applied (`std::max(1, L)` etc.). `effectiveLaneLor`
already runs `lorRowClamp_`, so the row invariants hold.

**ACCEPTANCE (Stage A):**
- Battery BYTE-IDENTICAL on BOTH legs (`RACK_DIR=rack-sdk-mock UNIFORMITY_STRICT=1` and
  `RACK_DIR=rack-sdk-lin/Rack-SDK UNIFORMITY_STRICT=1`) — there is NO engine change in
  Stage A, so every golden/digest must be untouched. If any changes, you edited an audio
  path — stop and revisit.
- `make RACK_DIR=rack-sdk-lin/Rack-SDK -j4` links (the full `plugin.so`, 0 errors).
- warn sweep 0 diagnostics; `test_16_triage` GATE 1 green.
- GREP: no lane-RANGE display read of `macroCVDelta` / `strandOffRef` / `polyOffE` /
  `strandLenRef` / `polyLenE` remains (the send/tap `macroPreDelta`/`macroPostDelta` reads
  are NOT range reads — they stay).
- Rodney Rack feel-test: the macro lane range STEPS at the voice-frame boundary (no glide),
  the east lane ranges MOVE (no freeze), both show what PLAYS.

---

## STAGE B — Option 3 (the mod preview; additive display)

Rodney's design: the committed window as the SOLID selection + a red wireframe outline
tracking the live incoming modulation, snapping into the solid at each boundary. "A bit
like mod arcs."

1. **The preview accessor (engine, reproduce-first, headless-testable):**
   `effectiveLanePreview(...) = base + PENDING` — the lane twin of the effective accessors
   but reading `laneLorCvPend_` instead of `laneLorCv_`, with the SAME `lorRowClamp_`.
   (`base + pending` == the live-modulated window exactly: the manager pushes
   `pending = round(knob+CV) − base` every block.) Add the voice/macro twins too
   (`voiceLorCvPend_` / `voiceLorCvPendMacro_`) for the voice-range wireframe.
   - PIN (new suite or the cadence suite): with no CV patched, preview == effective
     (pending 0); with a CV delta pending-but-not-committed, preview == base+pending while
     effective still == base+committed (the A/B divergence) — FAILS before the accessor
     exists, PASSES after.
2. **The wireframe overlay (widgets; Rack feel-test):** draw the preview window as an
   outline in both visuals (Macro strip + East). Start/end markers = offset / offset+length
   (easy). ROTATION needs its OWN marker (it moves the content within the window, not the
   window) — a tick where content-step-1 sits; the straight mod-arc-line variant with three
   markers (start/end/rotate) reads rotation better than a box outline, Rodney's call.

   **REUSE the existing EDIT/DISPLAY split — do NOT add a third range source (the
   disconnected-grab bug).** `SandsLaneWindowCore` already holds the range TWICE on purpose:
   the EDIT values (`length/offset/rotation`, used by `editStartBar`/`editBarInWindow` — the
   grab/hit-test, the user's authored/base window) and the DISPLAY values
   (`dispLength/dispOffset/dispRotation`, overwritten each frame by `setDisplayLOR` with the
   engine's CV-applied value — the tint/bars). Stage A made DISPLAY = effective; EDIT stays at
   base, so under CV they diverge — correct and non-destructive, but nothing draws the
   difference, so the grab handle (EDIT) detaches from the tinted window (DISPLAY) and reads
   as a bug (Rodney saw this live: grab box off its range, plus a second apparent "jump" one
   step after the commit as the two re-settle). The machinery to render it properly already
   exists, built for the LOCK case: `editBarInWindow` ("render the live window as the solid
   one"), `lorDiverged()`, and the "ghost echo". The overlay must EXTEND that to the
   CV-modulation case: **EDIT window = the SOLID grab box; DISPLAY (effective) window = the
   wireframe.** No new source, no third copy. When EDIT == DISPLAY (no CV) draw one window as
   today. ACCEPTANCE adds: Rodney feel-test that the grab box stays on the base window and the
   modulated window shows only as the wireframe — no detached grab, no phantom second jump.
3. **Gesture/suspend feel item (flag, don't over-build):** during an LOR drag, C1b Stage C
   suspends the CV commit but the pending keeps churning, so `base+pending` during a drag
   shows the dragged base + live CV. Decide in Rack whether the wireframe should track that
   or freeze during the gesture; leave both cheap to switch.
4. **Gate on the EXISTING per-surface modViz flags (Rodney's requirement — reuse, don't
   invent):** the wireframe overlay draws only when the surface's existing "Modulation
   arcs" toggle is on — the same submenu in `MonsoonWidget.cpp` (~1082) that governs every
   other mod visualisation. Use the bool flags already there: `modVizMacro` for the Sands
   Macro strip, `modVizMono` for Sands Mono, `modVizEast` for Straits East (and
   `modVizWest` if the West strip grows the same overlay). Do NOT add a new menu item or a
   new flag. Update the submenu LABELS, which currently read "(spread)"
   (`add("Sands Macro (spread)", &m->modVizMacro)` etc.), to reflect that the flag now also
   governs the LOR-range modulation overlay — e.g. "Sands Macro (spread + range)". The
   overlay must respect the flag's default-on and the user's toggle live, exactly like the
   spread arcs.
5. **Honest connection to the mask anchor:** under R-CV the wireframe shows effective R
   while the painted mask stays on base R (the UAS Stage-4 law). They diverge — and that's
   correct/illustrative here (the wireframe shows modulation sliding the read position
   against a fixed mask). Do NOT repoint the mask anchor.

**STAGE B FOLLOW-UP — WINDOW MODEL DECIDED (Rodney, 2026-11-16). Supersedes the
two-window phrasing in point 2 and resolves point 3's open item.** CC's Stage B
(`c24f4eab`) built committed = solid + pending = wireframe and left the grab/base
unattached (the disconnected grab). **FINAL MODEL (Rodney, 2026-11-17) — supersedes the
three-window text; collapse to TWO elements, not three.**

There is NO separate "base/authored" display copy. Authored and committed are the SAME
window offset by the latched CV (`committed = base + committed CV`); with no CV they are
identical — the only difference is the modulation. So draw exactly two things:

1. **THE SOLID WINDOW = committed/effective** (`effective*Lor`, what `setDisplayLOR` feeds —
   what actually PLAYS). This IS the range: the grab sits on it, the lit/dim bars and the
   fill are it, and under modulation it STEPS to where the CV committed. One window, always
   present. DELETE the separate base-outline AND the divergence "echo" entirely — the echo
   is the flicker-on-drag (it toggles on `lorDiverged()`); with no echo there is nothing to
   toggle.
2. **THE WIREFRAME = pending** (`effective*Preview` = base + pending) — the live video-rate
   CV preview, CC's three-marker form.

**GRAB THE COMMITTED WINDOW WHERE IT IS DRAWN — no snap.** Hit-test against the committed
window, NOT the base edges (CC currently hit-tests the base — a cause of the split feel). On
press, modulation suspends (per-item, below) and the window follows the pointer FROM the
grab point.

NO-SNAP REQUIRES "FORGET THE SUSPENDED SAMPLE" (Rodney): the reducer's `grabOff`/`grabLen`
must be seeded from the COMMITTED window at press (base + the CV displacing it), NOT from the
base. That folds the suspended item's grab-time CV into the new base — the drag sets the
base absolutely from the committed position, so nothing jumps, and that one CV sample is
DISCARDED (forgotten), not restored. Scope is exact: ONLY the suspended item(s) (the ones
under the finger) forget their grab-time CV; every non-suspended item keeps modulating
uninterrupted through the drag. On release, the drop position IS the new base; fresh CV
reconnects on all items at the next boundary and applies from the new base (so with CV still
patched the window shifts by the CURRENT CV at that boundary — correct, modulation resuming
on the new base, not a bug). Consequence to accept: a grab+release without moving shifts the
suspended item's base by its momentary CV (you keep what you grabbed) — the honest price of
no-snap.

**PER-ITEM SUSPEND — suspend EXACTLY what the gesture writes (mirror the reducer, drop the
coarse blanket).** `SandsRangeGestureCore`'s reducer defines the per-zone writes;
`lorGestureItems` must match them (today it is `{zone != ROTATION, zone != ROTATION, zone ==
ROTATION}` — too coarse: it freezes offset+length for EVERY window drag, including a plain
move that only touches offset and an end-drag that never touches offset):
- `Z_WINDOW` (move the range) writes OFFSET only → suspend OFFSET mod; length AND rotation
  mod keep running (rotation keeps moving the content inside the window as it slides — that
  is the answer to "is rotation disabled while dragging": no).
- `Z_START` (front edge) writes OFFSET + LENGTH (the end is anchored, so both move) →
  suspend OFFSET + LENGTH.
- `Z_END` (back edge) writes LENGTH only (start anchored) → suspend LENGTH.
- `Z_ROTATION` writes ROTATION only → suspend ROTATION.
Only the dimension under the finger stops; the others keep modulating live. The engine's
`setLorCvSuspend` already takes per-item bools — feed it the reducer's per-zone item set.

**WIREFRAME OFF-PISTE BUG (CC Stage B `c24f4eab`, `drawLanePreviewMarks` in
`SandsVisualEditorV4.hpp` ~443) — fix in this follow-up.** The END tick is drawn at
`getStepX(pv.off + pv.len)` with NO wrap. `STEP_COUNT == kMaskRing == 32`, but `off` ∈
[0,31] and `len` ∈ [1,32], so `off+len` reaches 63; any window that wraps past the ring
(`off+len > 32`) draws the end line RIGHT of the 32-cell grid — into the direction/
delegation buttons (Rodney saw this live). START (`off`) and ROTATION (`% ring`) are
bounded; only END is raw. FIX: wrap the end — `endStep = (pv.off + pv.len) % STEP_COUNT`,
mapping `0 → STEP_COUNT` so a window ending exactly at the boundary stays at the right edge
(`getStepX(STEP_COUNT)` = `padding + gridW`); AND add a guard so NO tick ever draws outside
`[padding, padding + gridW]` (clamp or skip), belt-and-braces for every marker. Rack
feel-test: modulate offset/length so the window wraps — the end marker stays inside the
grid, never over the buttons.

**ACCEPTANCE (Stage B):** the preview pin fails-before/passes-after; battery still
byte-identical (the preview accessor is read-only, no audio reader); build + warn sweep
clean; Rodney Rack feel-test (per the FINAL MODEL above): (a) with CV and no drag — the ONE
solid window (committed) steps to what plays, the pending wireframe tracks the live CV, no
marker escapes the grid; (b) grab the window where it's drawn — it follows the pointer with
NO snap, only the dimension(s) under the finger stop modulating while the others keep going,
and on release the drop is the new base with CV/wireframe resuming at the next boundary.

---

## REMAINING BUGS — CC's follow-up (`b0008944`) missed these (Rodney, Rack, 2026-11-16)

**BUG 1 — grab separated from the highlighted bars — RESOLVED BY THE FINAL MODEL ABOVE, not
by the earlier "dimming follows base" note (that note is SUPERSEDED — do not implement it).**
The split existed because the grab and the lit bars read DIFFERENT windows (base vs
committed). The FINAL MODEL removes the split at the source: there is ONE solid window =
committed, and the grab, the lit/dim bars, the fill AND the hit-test all read it. So route
every "the window" display read (`barInWindow`/`startBar`/`endBar`, ~885 and ~944) AND the
grab hit-test to the committed window; delete the base-outline and the divergence echo.
`editBarInWindow`/the EDIT edges are used ONLY transiently during a drag, where suspend has
frozen committed to the dragged base anyway. Rack feel-test: grab and lit bars are the same
window at all times; under CV that one window steps to what plays; the wireframe shows the
live CV.

**BUG 2 — the double commit (the "two jumps one step apart", 16/8/8) [ENGINE — its own
commit, reproduce-first].** NOT a Stage-A regression — a latent C1b engine bug Stage A made
visible. `voiceFramePhase_` is written twice in `tickHolders` (SequencerEngine.cpp): the
closed form at ~886, then ADVANCED by `kV` at ~1004. The lane-commit edge
(`voiceFramePhase_[v]==0`) is checked at tickHolders ~992 (BEFORE the advance) AND at
deriveHolders ~1122 (AFTER it — deriveHolders is called at ~1036, past the advance). So the
deriveHolders re-check tests the NEXT step's phase and fires one step early — the lane
commits at TWO ADJACENT steps per voice-frame cycle (the display jumps twice; the AUDIO
double-commits too, uncaught because no golden modulates lane LOR mid-run). The C1b comment
calls it "idempotent re-check under the same gate" — false once the phase has advanced.
FIX: the deriveHolders re-check must evaluate the SAME pre-advance edge tickHolders used —
either a per-step "lane edge already committed this step event" flag set at the advance
head and honoured in the derive, or snapshot the pre-advance `voiceFramePhase_` for the
derive's check — so the lane commits EXACTLY ONCE per cycle, keeping the anchor-only /
offsets-only flows the derive re-check was added for. REPRODUCE-FIRST (headless): a pin that
modulates a lane's LOR under a mapped voice frame (e.g. the 16/8/8 rig) and asserts the
committed lane window changes once per voice-frame cycle, not twice — fails before, passes
after. Battery must stay byte-identical (no golden modulates lane LOR, so the single-commit
fix changes nothing the goldens cover).

**BUG 3 — the rotation indicator is the lone EDIT-window reader; it will detach under
modulation [display, one-line-each fix; found in `da66989d` after the FINAL-MODEL rework].**
`drawRotationIndicator` (`SandsVisualEditorV4.hpp` ~1185-1205) draws the faint rotation track
and the rotation chevron off `L.offset` / `L.length` / `L.rotation` — the EDIT/authored
values. EVERYTHING else in the window now reads the COMMITTED (`disp*`) window: the lit/dim
bars (`barInWindow` → `startBar()`/`endBar()` → `dispOffset`/`dispLength`), the fill, the
hit-test (`windowEdgeW`/`hitTestHandle` → `dispLength`), and the grab press-seed
(`win = dispLength/dispOffset`). So under LOR offset/rotation modulation the rotation track +
chevron DETACH from the lit bars and fill (committed moves, this layer stays on base) — the
exact split the FINAL MODEL killed, surviving in one element. The function's own comment ("in
step with the lit bars + the grab (the EDIT L/O)") is WRONG — the bars read committed, not
edit; `da66989d`'s "zero edit-window reads remain — grep-verified" claim missed this function
(it is three edit reads).

FIX: repoint `drawRotationIndicator` to the committed window — `L.dispOffset` / `L.dispLength`
/ `L.dispRotation` in the `startBar` / `lenC` / `rotBar` computations (~1187/1188/1203), so it
matches `drawStep` / `windowEdgeW`. Correct the comment to say the authored-window layer now
rides the committed window like the rest. The LIVE modulated rotation stays the wireframe's
rotation tick (unchanged). VERIFY: grep the editor draw path — no bare `L.offset` / `L.length`
/ `L.rotation` reads remain (only `disp*`), excepting the drag reducer's own
`L.length/offset = lorDrag.window[...]` writes (~1626) and the equality/reset sites. Rack
feel-test: patch CV to a lane's ROTATION (and OFFSET); the chevron and faint track stay glued
to the lit bars and fill at every modulation value — no drift; during a drag the rotation
layer follows the committed window the drag-guard draws, same as the bars.

## NON-SCOPE
- No audio/engine behaviour change in Stage A; Stage B adds only a read-only accessor.
- The **voice-range** display (`effectiveMacroRowLor` / `effectiveVoiceLor`) is already
  unified — leave it.
- The send/tap deltas (`macroPreDelta` / `macroPostDelta` / `macroCVDelta` as the send
  source) stay — only the lane-RANGE display stops reading `macroCVDelta`.
- C3 direction + reverse-all, the draw-cache, and the 32-step UI are untouched.

## MVC NOTE (for the commit message)
This completes the lane-range display's MVC migration (the "mirror DELETED, store-backed"
arc): the View now reads the Model's one published authority (`effectiveLaneLor`), not a
Controller scratch cached on the widget (`macroCVDelta`) nor a raw Model internal (the base
store). One definition of "the lane window" — DRY/ODR and MVC closed in the same move.
