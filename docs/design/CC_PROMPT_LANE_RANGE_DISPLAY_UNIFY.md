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
unattached (the disconnected grab Rodney saw in Rack). There are THREE windows; draw all
three, collapsing to one when they coincide:

1. **base / EDIT window = the grabbable SOLID OUTLINE.** The grab handle MUST stay on the
   base (`length/offset/rotation`, `editStartBar`/`editBarInWindow`) or the user cannot
   drag/drop the range (decision A). This is the always-present draggable element.
2. **committed / effective window = the SOLID FILL** — what plays now, steps at the
   boundary (the latched `effective*Lor`, what `setDisplayLOR` feeds). After an edit this is
   where the released base + resumed CV settle (decision C — yes, it's the post-edit
   latched window).
3. **pending / preview window = the WIREFRAME** — the live video-rate CV
   (`effective*Preview` = base + pending) (decision D). Keep CC's three-marker form.

When base == committed == pending (no CV, not editing), collapse to a single window as
today.

**Edit-suspend (decision B — DECIDED, not a feel item; this closes point 3 above).** An
active LOR gesture on a scope (drag of START / END / WINDOW or ROTATION) DISCONNECTS
modulation for that scope — the engine already does this (C1b Stage C `setLorCvSuspend`,
keyed off the `lorDrag` / `lorGestureActive` snapshot). The DISPLAY must match: while that
scope's gesture is active, **HIDE its wireframe** (no live-CV preview — modulation is
disconnected), the grab/base OUTLINE follows the pointer, and the committed FILL holds the
suspended window; on RELEASE the CV resumes at the next boundary and the wireframe returns.
Key the hide off the SAME gesture signal the suspend uses — do not invent a second gesture
state.

**ACCEPTANCE (Stage B):** the preview pin fails-before/passes-after; battery still
byte-identical (the preview accessor is read-only, no audio reader); build + warn sweep
clean; Rodney Rack feel-test: (a) with CV and no drag — base-outline + committed-fill +
pending-wireframe all legible, grab never detaches; (b) drag a range — the grab stays on
the base and drags, no wireframe during the drag (modulation disconnected), on release the
fill commits and the wireframe reappears at the next boundary.

---

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
