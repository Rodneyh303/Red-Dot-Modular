# Shophouse Micro -- tuning+scale scene modulation for the Colonnades family (post-V1)

A microtonal sibling of Shophouse: instead of switching (scale, root) slots within 12-TET, it switches
between full .dmtune tuning+scale slots, boundary-quantised, under CV. Makes TUNING ITSELF a modulation
destination. Post-V1 (V1 has the M4/engine-widening long poles; this must not expand that scope).

## The idea
Shophouse already IS the right architecture (MonsoonShophouseExpander.hpp): FOUR fronts, each a slot;
a CV input sampled AT THE PHRASE BOUNDARY selects the active front; the active slot writes to Monsoon's
shared state on the loop edge (ScaleList commitAtBoundary -- changes land on the loop edge, never
mid-phrase). Shophouse Micro widens what a "front" HOLDS: from (scale, root) within fixed 12-TET, to a
full .dmtune (cents[] + weight[] for N degrees).

A .dmtune IS a superset of a Shophouse front: today's front = "scale mask + implicit 12-TET tuning";
a .dmtune front = "scale mask + explicit N-degree tuning". So this is a generalisation of Shophouse,
not a new mechanism.

## What you're modulating depends on slot CONTENT -- tuning, scale, or both (Rodney)

A .dmtune carries BOTH cents[] and weight[]. So the SAME switch mechanism produces musically distinct
behaviours depending on what actually differs between the active slots. This is a feature, not an edge
case -- the musician chooses the axis by what they load.

- **Same cents, different weights = SCALE modulation within a tuning.** All slots share one tuning
  (identical cents); only the weight masks differ. Switching fronts changes WHICH DEGREES are active --
  moving between scales/modes of one fixed tuning. The microtonal generalisation of what the original
  Shophouse did (mode-switching), now in an arbitrary tuning. Pitch vocabulary stays; selection moves.
- **Different cents = TUNING modulation proper.** The cents differ between slots -- gliding between
  tuning SYSTEMS (12-TET -> maqam -> stretched). The novel capability; tuning as a modulation dimension.
- **Both at once = tuning + scale together**, or hold one constant and vary the other. It's a CONTINUUM,
  not two modes.

The mechanism doesn't know or care which: it just writes the active slot's cents[] + weight[] to the
table. The behaviour is EMERGENT from slot content. One mechanism; the musician decides whether they're
modulating tuning, scale, or both, purely by what they load.

Implication for switch-vs-morph (the deferred question): the two axes morph DIFFERENTLY.
- Weight morphing (scale) is well-behaved -- crossfading two masks is musically sensible (degrees fade
  in/out).
- Cents morphing (tuning) is the gorgeous-but-hard one -- continuously gliding pitch.
So if morph is ever built, a sensible split is: morph WEIGHTS (easy, safe), SWITCH cents at the boundary
(avoids "what does a half-glided tuning sound like mid-phrase") -- or offer both. Later call; this
cents-vs-weight distinction is the lens to make it through.

## Where it writes (the one architectural point)
Today Shophouse writes (scale, root) into Monsoon's ScaleManager. Shophouse Micro writes the active
front's full cents[] + weight[] into the SHARED TuningTable owned by Monsoon -- the SAME destination
the Colonnades/Duo faders write. So:
- Colonnades/Duo = the AUTHORING source (faders write the table).
- Shophouse Micro = the MODULATION source (scene switch writes the table).
- Both write the same shared TuningTable, boundary-quantised. The write-authority / WriteLedger work
  (MICROS_ENGINE_CLAUDE_CODE_GUIDE) already contemplates multiple writers; Shophouse Micro is one.

## Scope constraints (Rodney) -- who Shophouse Micro is and isn't for

- **Sikit uses the REGULAR Shophouse, NOT Shophouse Micro.** Sikit is the tuning-only Phase 1 expander
  -- it has no weight-fader scale-mask apparatus, so the .dmtune-slot machinery is irrelevant to it.
  Sikit pairs with the existing 12-TET Shophouse. Shophouse Micro is ONLY for Colonnades / Duo.
- **Monsoon has EXACTLY ONE of Sikit / Colonnades / Colonnades Duo.** They are mutually exclusive
  tuning providers, not stackable -- one tuning authority per Monsoon. (So a given Monsoon is either a
  Sikit rig, a Colonnades rig, or a Duo rig -- never two microtonal authorities at once.)
- **.dmtune N must match the Micro:** a 12-tone .dmtune works ONLY with Colonnades; a 24-tone .dmtune
  works ONLY with Duo. Follows from the shared TuningTable's N -- a 12-degree table can't take a
  24-degree tuning and vice versa.

## 12/24 is an EXPLICIT MODULE MODE -- no mixed-N slot set (Rodney, supersedes connection-only)

CRITICAL correction to "chosen by connection" below: a Shophouse Micro must NEVER hold a mix of 12-tone
and 24-tone .dmtunes across its slots. All fronts must be the same N. So mode is an EXPLICIT MODULE
PROPERTY (12 or 24), persisted in module state, and ALL slot loads validate against it -- you cannot
load a 12 into slot 1 and a 24 into slot 2 even while unattached.

- The module has a stored `mode` (12 or 24). It governs front count (4 at 12, 2 at 24), slot .dmtune
  degree count, and what a slot load accepts.
- A slot load of a .dmtune whose n != mode is REJECTED (brief notice). No exceptions -- this is what
  prevents a mixed-N slot set.
- Connection still MATTERS but does not silently override: attaching to a Colonnades (N=12) requires/
  sets 12-mode; attaching to a Duo (N=24) requires/sets 24-mode. If the module already has slots loaded
  in a mode that mismatches the newly-attached Micro, that's a conflict -> flag it (the module is in
  24-mode with 24-tone slots but you attached a Colonnades) rather than silently wiping slots.
- Setting/changing mode when slots are loaded: changing mode invalidates the loaded slots (they're the
  wrong N) -> require explicit confirm / clear, don't silently discard. Mode is normally set once (by
  first load or by the connected Micro) and left.
- If UNATTACHED: mode can be set explicitly (a 12/24 toggle) OR by the first .dmtune loaded (its n sets
  mode); subsequent loads must match. Either way the INVARIANT holds: one mode, all slots that N.

This supersedes the "first load defines mode / resolve on attach" wording below to the extent they
conflict -- the binding rule is: ONE explicit mode, ALL slots match it, mismatched loads rejected,
mode changes with loaded slots require confirm.

## ONE module, 12-or-24 (mode as above; connection informs it) (Rodney)
Do NOT ship two modules. Shophouse Micro is ONE module that auto-configures its mode from what it
feeds:
- Attached to a Colonnades-backed TuningTable (N=12): fronts are 12-tone .dmtunes. FOUR fronts.
- Attached to a Colonnades Duo-backed TuningTable (N=24): fronts are 24-tone .dmtunes. TWO fronts.
- It reads the target's N (the TuningTable's tt.N, already the single source of degree count) and
  configures front count + .dmtune degree count accordingly. Same way the Micros already resolve N.
- Mode is an EXPLICIT MODULE PROPERTY (see the "12/24 is an EXPLICIT MODULE MODE" section above) --
  connection informs/requires it but does not silently override loaded slots. The invariant is: one
  mode, all slots that N, mismatched loads rejected. Not purely connection-derived.

Why 4x12 vs 2x24 (forced by the format, not arbitrary): a front's payload is N cents + N weights, so a
24-degree front is twice the data of a 12-degree one. Four fronts of 24 is a lot of state + panel; two
fronts of 24 balances the total and matches the Duo's own 24=2x12 doubling logic. Two 24-tone fronts
also has musical sense: A/B maqam modulation, call-and-response between two 24-tone systems.

## Changes FROM the original Shophouse (Rodney)
1. **DROP the root-note setting.** Root-note was a 12-TET affordance (pick which chromatic pitch is
   degree 1). A .dmtune already encodes the full degree set with degree 0 as the root at 0 cents -- the
   file IS the pitch set, so a separate root offset is redundant and would fight the file. No ROOT_PARAM,
   no shutter-click-to-set-root. A front is just: which .dmtune.
2. **PANEL: alternating-colour cells, NOT piano keys.** The original Shophouse facades resemble a piano
   octave (the shutter-click-root metaphor). That bakes in the 12-TET-scale interpretation -- the SAME
   false interpretation dropped for the Lantern (no keyboard at N!=12) and the Micro panel (no
   primary/inflection split). Replace the piano-key facade with plain ALTERNATING-COLOUR cells (a
   neutral scene-slot strip). Consistent: microtonal surfaces never imply 12-TET structure, from panel
   to roll to this.
3. **Front content = a loaded .dmtune** (cents[] + weight[]) instead of (scale index, root). Loaded into
   the slot (baked into module state, travels with the patch -- self-contained), same as a front holds
   its scale today.

## What stays from Shophouse (the proven mechanism)
- FOUR-front street model (but N-derived: 4 at 12, 2 at 24).
- CV input sampled at the phrase boundary selects the active front.
- Boundary-quantised commit (ScaleList commitAtBoundary equivalent) -- tuning changes land on the loop
  edge, never mid-phrase. THIS IS THE KEY MUSICAL PROPERTY: tuning modulation quantised to phrase edges.
- CONSERVATION toggle (guide vs enforce) -- still meaningful (the weight mask can guide or enforce).
  RESOLVED SEMANTICS (Rodney): a front's ZERO-weight degrees are always silent in BOTH modes (the engine
  never generates or snaps to a weight-0 degree). ENFORCE additionally makes that mask HARD against
  MODULATION: Interchange CV driving a front-masked degree is IGNORED (re-zeroed after the CV fold),
  mirroring Monsoon's lockScaleNotes where an out-of-scale note reads zero regardless of CV. Under GUIDE
  the modulation stays additive, so an Interchange can lift a masked degree back in. Implemented in the
  Colonnades/Duo Model-Q fold (MicroTuning.cpp) -- re-assert the front mask after the Interchange fold when
  the bound Shophouse Micro's CONSERVATION = Enforce. No engine/TuningTable change (byte-identical at N=12).
- Menu-free direct panel (the whole point of Shophouse -- faster than the menu dance).

## Open design decisions (leans, confirm at build)
- **Switch vs morph.** Lean SWITCH first (discrete, boundary-quantised, matches Shophouse exactly).
  Interpolating cents between fronts (a tuning that GLIDES from A to B) is musically gorgeous but a much
  bigger feature and raises "what does interpolating a weight mask mean." Add morph as a later option;
  switch is already valuable and is the natural extension.
- **Loaded vs referenced .dmtunes.** Lean LOADED (baked into slot state, patch self-contained), same as
  a Shophouse front holds its scale now.
- **Variant vs mode of Shophouse.** This is a SEPARATE module (Shophouse Micro), not a mode of the
  12-TET Shophouse -- mixing (scale,root) fronts and .dmtune fronts in one module muddies what a front
  is. The 12-TET Shophouse stays as-is.

## The payoff (why this is worth building)
Tuning modulation as a first-class, boundary-quantised musical gesture -- not "switch scales" (note
selection) but "modulate between tuning SYSTEMS" (12-TET -> maqam -> stretched, on the phrase edge,
under CV). Combined with the cross-tuning canon (two Monsoons, differently tuned, shared CA --
PITCH_PATCHABILITY 12/13), tuning itself becomes a modulation dimension in the correlation architecture.
Most microtonal tools treat tuning as static setup; this makes it a modulation target. Novel.

## Cross-refs
- src/MonsoonShophouseExpander.hpp -- the four-front + boundary-commit mechanism this generalises.
- TUNING_PRESET_FORMAT.md -- the .dmtune a front holds.
- MICROS_ENGINE_CLAUDE_CODE_GUIDE.md -- the shared TuningTable + write-authority this writes into.
- PITCH_PATCHABILITY_AND_DISTINCTION.md 12/13 -- cross-tuning canon this composes with.

## PER-SLOT: load a .dmtune + show its abbreviated name (Rodney)

Each front needs (a) a way to LOAD a .dmtune into that slot, and (b) an abbreviated NAME readout where
the original Shophouse shows the scale name. Both map onto existing Shophouse structure.

### The name band already exists -- reuse it
Shophouse draws a live name readout per front on a "name band" widget: findNamed("name_band_" + f),
NameBandWidget (MonsoonShophouseExpander.cpp:183), currently drawing "<root> <scale name>"
(nvgText at :205,211). Shophouse Micro reuses this band, but:
- Draws the loaded .dmtune's NAME instead of "<root> <scale name>".
- No root (root-note dropped) -- so the label is just the abbreviated tuning name, not "<root> <name>".
- Empty slot -> a neutral placeholder ("--" or "empty"), not a scale.

### Abbreviated name -- reuse the .scl menu-width fix
The .dmtune "name" field (TUNING_PRESET_FORMAT schema) can be long, and the name band is narrow. Same
problem, same fix as the .scl context-menu width bug (SCALA_FILE_AND_LOAD_UI): truncate to fit the
band with an ellipsis. Truncate to the band width (~12-16 chars given four fronts across the panel);
full name in a hover tooltip if the widget supports it. The name is a label, not a contract -- the
tuning is loaded regardless of display.

### Loading a .dmtune into a slot
Per-front load affordance:
- Lean: a small "load" target per front (a button, or a click on the name band itself) that opens the
  osdialog .dmtune file picker (reuse the Colonnades/Duo load path -- dotModular::TuningPreset reader,
  the SAME loader, so it validates n against the module's current mode: 12-mode slot rejects a 24-tone
  .dmtune and vice versa, or auto-adapts -- see below).
- On load: read the .dmtune's cents[] + weight[] into the slot's stored state (baked into module state,
  travels with the patch). Store the .dmtune name for the band readout.
- Context-menu "Load .dmtune into slot N..." is the low-surface fallback; a per-front click target is
  more discoverable and matches Shophouse's menu-free ethos.

### N-mismatch on load (design decision, confirm at build)
Shophouse Micro is in 12-mode or 24-mode by connection (target tt.N). A slot load must handle a
.dmtune whose n disagrees with the current mode:
- Lean: REJECT with a brief notice ("24-tone tuning can't load into a 12-tone slot") -- the mode is set
  by the connected Micro, and a front must match the table it writes. Clean, predictable.
- Alternative (later): auto-truncate/pad, but that silently mangles the tuning -- reject is more honest.
- If UNATTACHED (no mode resolved yet): accept the .dmtune and set mode from ITS n, then require
  matching loads for the other slots. First load defines the mode until attached.

### Summary of per-front UI
- Name band: abbreviated .dmtune name (truncated + ellipsis), placeholder when empty. No root.
- Load target: per-front click/button -> osdialog .dmtune picker -> bake cents[]+weight[]+name into slot.
- The SCALE knob and ROOT shutter of the original Shophouse are GONE (no scale-stepping, no root) --
  a front is just "which .dmtune", loaded per slot.

Cross-ref: MonsoonShophouseExpander.cpp:183-211 (NameBandWidget -- the readout to reuse),
SCALA_FILE_AND_LOAD_UI.md (the truncation fix), TUNING_PRESET_FORMAT.md (the .dmtune name field +
the loader to reuse).

## PANEL SPEC (definitive -- stop the panel dance)

CC's first attempt (gen_shophouse_micro.py, the plain grey 4-rectangle stack) OVER-CORRECTED: it threw
away the entire shophouse building and made a generic slot strip. WRONG. Keep the shophouse. Change
ONLY the window contents.

### Keep from gen_shophouse.py (the whole aesthetic)
- The TWO-HOUSE, TWO-STOREY building: hipped tiled roof, dormers, arched louvred windows, pilasters,
  the Peranakan majolica tile panels flanking the windows, the arcade/five-foot-way at the base.
- The name band per front, the per-front scale... no -- see changes.
- The active-front lantern (lit indicator over the active window).
- Start from gen_shophouse.py and MODIFY it; do NOT start from the blank micro attempt.

### Change ONLY the window contents: piano keys -> uniform mask cells
The original window is 12 shutters laid out as a PIANO KEYBOARD (BLACK={1,3,6,8,10}, WHITE_ORDER,
BLACK_AFTER -- varying-width white/black keys). This is the 12-TET resemblance to kill. Replace with:
- UNIFORM-WIDTH cells, all the SAME size (no wide/narrow, no white/black key widths).
- ALTERNATING COLOUR black/blue (two colours, equal cells) -- so there is NO piano-key resemblance.
- The cells INDICATE MASK STATE: masked (inactive, weight 0) vs unmasked (active) degrees. The widget
  lights/dims each cell by the .dmtune's weight[] for that degree (active = lit, masked = dim), the
  same active/masked semantic as the Colonnades faders -- not a keyboard.
- Drop BLACK/WHITE_ORDER/BLACK_AFTER entirely; the cells are just N equal cells across the window.

### Story/panel layout maps to MODE (Rodney)
The building's stories/windows ARE the front layout, and how they subdivide depends on mode:
- **12 mode = 4 fronts:** two windows per story (top-left, top-right, bottom-left, bottom-right) --
  the existing 2x2 shophouse arrangement. Each window = 12 uniform mask cells.
- **24 mode = 2 fronts:** ONE window per story spanning the full width -- top story = one 24-cell
  window (one 24-note front), bottom story = one 24-cell window (one front). The two 12-windows in a
  story MERGE into one continuous 24-cell strip.

So the same building holds either four 12-cell windows (2x2) or two 24-cell windows (one wide per
story). The story structure is constant; what changes is whether each story is split into two
12-cell windows or is one 24-cell window. Cell count per window = N (12 or 24).

Panel generator: parameterise gen_shophouse (micro variant) so mode drives it -- 12-mode draws the 2x2
four-window layout, 24-mode draws the two-full-width-window layout, each window filled with N uniform
alternating black/blue cells. If the panel needs to support both modes from one SVG, draw the 12-mode
2x2 and let the widget MERGE each story's two windows into one 24-strip in 24-mode (or generate two
SVGs, one per mode -- cleaner, since a Micro instance is one mode at a time).

### Changes from Shophouse already ruled (carry into the panel)
- NO root shutter / no scale knob per front (dropped -- a front is just "which .dmtune").
- Name band shows the abbreviated .dmtune name (truncated), doubles as the load click target.
- Tile panels, roof, arches, lantern: KEEP (the shophouse identity).

### The one instruction
Start from gen_shophouse.py. Keep the building. Replace the piano-keyboard window shutters with N
equal-width alternating black/blue cells that show mask state. Lay out windows by mode (12 -> 2x2 four
windows of 12; 24 -> two full-width windows of 24, one per story). Drop root + scale knob. That's it --
do not redesign the module, do not start from a blank panel.

Cross-ref: panel_src/gen_shophouse.py (the building to keep + the piano-key window to replace --
BLACK/WHITE_ORDER/BLACK_AFTER at lines 39-41 are what to remove), gen_shophouse_micro.py (the
over-corrected attempt to discard/redo from gen_shophouse).
