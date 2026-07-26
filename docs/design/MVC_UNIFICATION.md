
## Step 1 — implementation plan (branch: feat/mvc-step1-global-slice)

### Done (this commit): the model, inert
- `EditorState` gains the GLOBAL slice — `globalLor[12]`, `globalSpread[4]`, `globalAtten[16]`,
  `globalTap[8]`, `globalDir[4]` (44 floats, exactly the 44 engine-read Macro params).
  Lane order 0..3 = REST/MELODY/OCTAVE/ACCENT; Macro has no VAR/LEG scope.
- Bounds-guarded accessors on Monsoon (`getGlobalLor(lane,c)` … `setGlobalDir(lane,x)`).
  Guarded because the engine reads these every cycle on the audio thread.
- Persistence in `MonsoonPersistenceManager` — `configParam` was giving Macro save/restore
  for free; store fields must be saved explicitly or globals reset on reload.
Nothing reads the slice yet, so this commit changes no behaviour.

### The 11 engine read sites (44 params, most inside loops)
| file:line | reads | becomes |
|---|---|---|
| MonsoonSandsManager 473-475 | `Macro::lorId(lane,0..2)` | `getGlobalLor(lane, c)` |
| MonsoonSandsManager 476, 529 | `Macro::sprId(lane)` | `getGlobalSpread(lane)` |
| MonsoonSandsManager 114, 489, 498, 533 | `Macro::macroAttenId(lane,col)` | `getGlobalAtten(lane, col)` |
| MonsoonSandsManager 499 | `Macro::tapIdForItem(lane,item)` | `getGlobalTap(lane, item==3 ? 1 : 0)` |
| MonsoonExpanderManager 174 | `StraitsMacroVisualIds::dirDispId(el)` | `getGlobalDir(el)` |

### DO NOT lockstep-swap. Use the dual-write bridge.
Switching the engine to store reads while Macro still writes only params would read zeros —
a silent, total failure. The Causeway job had to move both sides at once because the params
were being deleted in the same change; here we can avoid that entirely:

1. **Dual-write** — Macro's widgets write the STORE *in addition to* their params. Additive;
   nothing reads the store, so behaviour is unchanged and the store becomes populated.
2. **Flip the readers** — switch the 11 engine sites to the accessors. Behaviourally a NO-OP
   if step 1 is correct, which makes it a clean A/B test: if anything changes, step 1 is
   wrong. Build-verify here.
3. **Delete** — remove Macro's 44 `configParam` calls, the param writes, and right-size
   `config()`. Only now does Macro stop being host-exposed.

Each step is independently safe and independently verifiable, and step 2 doubles as the
proof of step 1. This is the pattern to reuse for Mono and East.

### Verify at step 2 (behavioural — a wrong index is a silent wrong value, not a compile error)
Global LOR length/offset/rotation each move the Sands Helix display; global spread responds;
the four attenuverters scale their CV; PRE/POST taps still switch; direction cells still flip.

## Step 1d — full scope (three dependencies found while preparing it)

Macro binds its 60 params through SIX loops, not 60 call sites. But converting them is not
uniform — three separate couplings surfaced:

| # | Group | n | Widget | Coupling |
|---|---|---|---|---|
| 1 | Attenuverters | 16 | Trimpot | `leftAttenuverters` is `vector<Widget*>` — **StoreKnob fits, no change** |
| 2 | Spread | 4 | Trimpot | **BLOCKER** — `pendingSpreadArcs` is `vector<pair<ParamWidget*,int>>` and the ModArcOverlay reads `knob->paramId`. StoreKnob is not a ParamWidget. |
| 3 | LOR | 12 | Trimpot | clean |
| 4 | Taps | 8 | Trimpot ×2 rows | clean |
| 5 | Sends | 16 | Trimpot | clean; NOT engine-read (pure proxy) — convert in the same pass so Macro reaches 0 params |
| 6 | Direction | 4 | **DirCell** (custom) | not a knob — needs its own store-write path to `setGlobalDir`, mirroring how East's DirCell already calls `setLaneDir` |

### Prerequisite DONE
`SvgPanelKit::bindWidget<W>(name, config)` — binds a non-param widget to a named shape
(createParamCentered/addParam require a ParamWidget). Runs `config` before centring because
`setSvg` establishes `box.size`. Reusable by Mono and East.

### The spread blocker, concretely
`ModArcOverlay` is attached over each spread knob and captures `pid = knob->paramId`, then
reads the param by id in `getSetNorm`. Converting spread to StoreKnob requires:
- retyping `pendingSpreadArcs` to `vector<pair<Widget*,int>>`, and
- rewriting `getSetNorm` to read `getGlobalSpread(lane)` from the store instead of the param.
Both are small, but they must land WITH the spread conversion or the arcs read a dead param.

### ATOMICITY RULE (do not slice by group)
Each group's three edits must land together: widget → StoreKnob, `configParam` deleted, and
that group removed from the step-1b dual-write mirror. Converting a group while the mirror
still copies its (now deleted) param into the same store field puts two writers on one value.
Practically this means step 1d is ONE commit covering all six groups, then `config()`
right-sized to 0 and the mirror deleted entirely.

### Verify (behavioural — silent failure, not compile errors)
Everything from the step-1c list, PLUS: mod arcs still track the spread knobs; direction
cells still flip; send grids still work; Macro shows ZERO parameters in the DAW; undo works
per drag on every converted control; patch save/reload preserves all 60 values.

## The general pattern for de-paramming: LAMBDA INJECTION, not templates

Rodney asked how overlays/displays can serve BOTH param-backed controls (Monsoon, Straits)
and store-backed ones — and whether templates or kit-style mixins are the better attack.

**Answer: the codebase already has the right idiom, and it is lambda injection.**
`Dimmable::displayValueFn`, `ConnectMark::lightTheme`, `GoldPolyPort::lightTheme`,
`ModArcOverlay::getSetNorm/getModNorm/isActive` — every one of these already takes its
value/state through a `std::function`, not through a param. That is the abstraction. Lean
on it rather than introducing a second mechanism.

### The rule
> A consumer widget (overlay, display, cell) must depend on a control only for GEOMETRY
> (`Widget*`: `box`, position). Every VALUE it needs comes through an injected lambda.
> Then param-backed and store-backed controls are interchangeable at zero cost.

`ModArcOverlay` already obeyed this — it contains ZERO references to `ParamWidget` or
`paramId`, and `attachOverKnob` already takes `Widget*`. The only coupling was Macro's
`pendingSpreadArcs` being typed `vector<pair<ParamWidget*,int>>` plus one lambda that read
`knob->paramId`. Both are now fixed: the vector is `Widget*`, and `getSetNorm` reads
`getGlobalSpread(lane)` from the store. Correct BEFORE step 1d (the mirror keeps store ==
param) and after (store is authoritative) — so it was safe to land early, and it removes
one of step 1d's three couplings ahead of time.

### Why not templates / policy classes

CORRECTION (Rodney): an earlier version of this argued that Controls.hpp being
GENERATOR-OWNED prevents rewriting the artwork classes. That reason is WRONG — the file is
produced by panel_src/gen_controls.py, so a policy shape would simply mean changing the
generator and regenerating. "Generator-owned" means do not HAND-edit; it does not freeze the
design. Withdrawn.

The conclusion still stands, on two reasons that survive:

1. **The two knob kinds no longer share a render path.** Param knobs are `SvgKnob`
   subclasses and draw through a FramebufferWidget. `StoreKnob` draws its SVG directly,
   because the framebuffer refresh is driven from inside Rack's `if (getParamQuantity())`
   and never fires for a param-less knob (see the StoreKnob history note). So this is not
   "same artwork, swappable binding" — the rendering differs too. A shared policy design
   would have to move the WORKING param knobs onto the direct-draw path as well, which is a
   large blast radius across every existing module for no user-visible gain.

2. **Consumers would need templating or type erasure anyway.** Overlays, cells and displays
   take values from controls. Under a policy design each would need a template parameter or
   an erased interface; with lambda injection they take `Widget*` for geometry and a
   `std::function` for value, and are indifferent to the binding. `ModArcOverlay` already
   demonstrates this — it needed ZERO changes to serve both kinds.

Lambda injection is also already the codebase's idiom for every cross-cutting concern
(`Dimmable::displayValueFn`, `ConnectMark`/`GoldPolyPort::lightTheme`,
`ModArcOverlay::getSetNorm`), so it adds no new mechanism. `std::function` dispatch is
irrelevant for tens of widgets at frame rate.

### Where the generator SHOULD be used
Call sites currently pass raw asset paths (`"res/controls/RDM_Grey_Trim_Bar.svg"`), which is
stringly-typed and can silently mis-name a face. gen_controls.py already emits a Tag struct
per asset carrying exactly that path — so store-backed call sites should take a Tag instead
of a string, giving compile-time checking. That is a real, small generator change worth
making; the policy-class rewrite is not.

### On Rack's gap
Rodney is right that Rack lacks a clean "control, but not host-exposed" concept: ParamWidget
bundles value storage, persistence, undo, tooltips, MIDI-map AND host automation, and you
cannot take the last one away. Everything StoreKnob had to re-implement (drag, tooltip,
undo, persistence, hover) is a thing that came free from that bundle. Worth remembering when
estimating the remaining modules: the widget is never the hard part; the free services are.

## Step 1d progress (28 of Macro's 60 params done)

| Group | n | Status |
|---|---|---|
| Attenuverters | 16 | **DONE** — StoreKnob, Grey_Trim_Bar |
| Spread | 4 | **DONE** — mod-arcs needed no change once decoupled |
| Taps | 8 | **DONE** — + on-panel LOR/SPR labels added |
| LOR | 12 | **DONE** — saveLOR/loadLOR now read/write editor.globalLor via get/setGlobalLor (the array the engine already reads and persists); 12 globalDnaId configParams removed; dual-write LOR mirror deleted. Tested: edits + patch save/reload work. Undo: LOR was NEVER undoable (Macro/Mono/East all use the editor's own VoiceState history, not Rack's Ctrl+Z stack), so nothing was lost — not a regression. StoreEditAction is for the click-cell groups (sends/dir/owners) where proxy undo is subtly wrong. |
| Sends | 16 | **DONE** — 16 send trimpots now StoreKnobs reading/writing getMacroSend/setMacroSend for the LIVE view voice (slot = voiceSlot(viewVoice+1), resolved per-call so a tab switch re-targets the same knob). The whole per-voice load/store sync dance + clobber guard + lastSendVoice are deleted — no proxy to sync. 16 sendDispId configParams removed. macroSend[256] already engine-read + persisted; no engine-seed hazard (sends were never a display mirror). |
| Direction | 4 | **DONE** — store-backed DirCell (getStateFn/setStateFn on get/setGlobalDir, the array the engine reads + persists); 4 dirDispId configParams removed; dual-write mirror fully retired; gate-mod cycle + init-seed redirected to the store. No undo, matching East/Mono (still param-backed, also no undo). DirCell gained optional store callbacks so both forms share one widget. |

**config() = 0: DONE.** All six groups store-backed; the dual-write mirror is gone;
Macro now calls config(0, NUM_INPUTS, NUM_OUTPUTS, 0) and exposes NO host params. Id
constants stay declared (they name SVG shapes + index the store) but reserve no param slots.
Store binds use addChild not addParam, so config(0) is safe. Macro has fully left the host
param list -- the goal of the de-param.

### Services a de-parammed control must re-supply (all now central in configureStoreKnob)
Found one at a time, each only on a real build — the full list, so later modules inherit
rather than rediscover: render path (SvgKnob's framebuffer is driven from inside
`if (getParamQuantity())`), hover (a plain Widget must consume onHover to become hovered),
tooltip text (incl. a value fallback for continuous knobs), undo coalescing, persistence,
lazy store resolution (the store may live on another module attached later), and
double-click-to-default (Rack's DoubleClick does not reach a plain Widget).

### Aesthetic note
Rodney: the Grey control faces read better on the LIGHT panel — a metallic look against the
light body. Faces are Tag types now, so changing family is a one-token edit per call site;
revisit once more of the panel is converted.

## LOR de-param — trace in progress (resume here)

Goal: remove Macro's 12 LOR params (4 lanes x 3 items), mirroring East, which already uses
the STORE not params.

What's mapped:
- Macro's LOR round-trips through params: saveLOR() writes params[lorId(l,c)],
  loadLOR() reads them back (StraitsSandsMacroVisual.cpp ~304-320). lorId ->
  StraitsMacroVisualIds::globalDnaId(lane,c) (StraitsSandsMacroVisual.hpp:132).
- The editor holds LOR in visualEditor->currentState.lanes[ENGINE_LANE_TO_EDITOR[l]].{length,
  offset,rotation}; the engine holds it in strandLen/Off/Rot (mono lane store, lorRef),
  written via the clamping setter at SequencerEngine.hpp:386.
- East is the reference: setLorBase(slot,bank,c,v) / getLorBase(slot,bank,c) into
  editor.lorBase[288] (16 slots x 6 banks x 3), StraitsEastSandsVisual.cpp:488/503. Macro is
  GLOBAL (not per-slot), so it needs a fixed global/mono slot+bank convention -- NOT yet
  pinned. Guessing it is the exact "plausible wrong value" trap; must be verified against
  the call site, not assumed.
- syncPatternEngineToEditor (MonoSandsParameterManager.hpp:128) only syncs PROBABILITIES, not
  LOR -- so params are currently LOR's only editor->engine path. Still need to find the
  param->strandLen read site (the setter at SequencerEngine.hpp:386 is called from somewhere
  that currently reads Macro's globalDnaId params) before redirecting.

Requirements carried from DAW_PARAM_AUDIT.md 5b (per de-parammed group):
- UNDO: wire StoreEditAction (helper landed, src/ui/StoreEditAction.hpp, 20/20). LOR is grid-
  edited, so capture old on drag/commit, push one action -- but LOR grid may currently rely on
  param undo; confirm and replace.
- Persistence: store must serialise (lorBase already persists via the editor store JSON).
- Tooltip/typed-entry: keep where it matters.
- Host/MIDI map: intentionally forfeited.

NEXT: pin the global slot/bank for Macro LOR in lorBase[], find the param->engine read site,
then redirect saveLOR/loadLOR to setLorBase/getLorBase and delete the 12 param configs.

## Direction de-param — plan pinned (resume here)

Store target VERIFIED (the LOR lesson: find what the ENGINE reads, don't guess):
- Macro's global direction: engine reads getGlobalDir(lane) at MonsoonExpanderManager.cpp:221
  (the non-East-owned branch), clamped 0..3. globalDir[4] already persists
  (PersistenceManager editorGlobalDir, 211/462). So globalDir is the target -- the dirDispId
  params are the redundant mirror, exactly like LOR's globalDnaId.
- East-owned mono lanes read getMonoLaneDir instead (line 206) -- NOT Macro's concern here.

Why this is bigger than LOR (do it fresh, not at session tail):
- DirCell is a rack::ParamWidget (src/ui/OwnerCell.hpp) -- it reads/writes getParamQuantity().
  De-paramming needs a STORE-BACKED variant: read via a getFn() callback, write via
  applyAndPushStoreEdit<Monsoon>(mon, "direction", setter, old, new). No existing store-backed
  cell widget yet -- this is the first, and owners/sends will reuse it.
- This IS the 5b undo case (unlike LOR): click-cell edit, discrete before/after, and the
  current proxy undo is voice-INCORRECT. applyAndPushStoreEdit gives voice-correct undo.

Steps:
1. Add StoreDirCell to OwnerCell.hpp (or a new header): getFn()/lockWhen, cycle() calls
   applyAndPushStoreEdit with setGlobalDir setter. Mirror DirCell's draw exactly.
2. Macro: bind StoreDirCell instead of bindParam<DirCell>, wired to get/setGlobalDir(lane).
3. Delete the dir mirror line (setGlobalDir(lane, pv(dirDispId))) at ~392 -- the last line of
   the dual-write mirror block, so the whole block goes.
4. Remove the 4 dirDispId configParams (keep ids declared, per attenuverter/LOR pattern).
5. Verify: engine still gets direction via getGlobalDir; edits persist; undo works and is
   voice-correct.

Then only SENDS (16) remains on Macro -- the per-voice one needing view-voice context.

## Undo backlog — LOR + direction (revisit later)

LOR and direction are store-backed with NO undo, matching East/Mono (whose still-param-backed
cycling cells also have no meaningful undo). This is consistent, not a regression. TODO for a
later, uniform pass: add undo across ALL the Sands cycling/grid cells at once (LOR grid,
direction DirCells on Macro/East/Mono) rather than per-module, using StoreEditAction for the
store-backed ones. Deferred by Rodney; not owed by the de-param work.

## De-param playbook (distilled from Macro: LOR, direction, sends)

A repeatable recipe for the remaining groups (Mono 54, East 38). Each group cost a distinct
mistake the first time; this front-loads them.

### The recipe
1. **Find the store target by the ENGINE READ, never by name.** Multiple plausible store
   arrays exist (e.g. LOR had both globalLor[12] AND East's per-slot lorBase[288]). The
   RIGHT one is whatever the manager/engine already reads each cycle. Grep the manager for
   get<Thing> and use THAT array. Guessing compiles clean and silently disconnects the
   control -- the signature failure of this codebase.
2. **Confirm it already persists.** The correct store array is almost always already in
   PersistenceManager (editor<Thing>). If it isn't, add save/load FIRST -- a de-param without
   persistence loses the value on reload.
3. **Enumerate ALL param touch-points before editing.** A group is never just the widget bind.
   Direction had FOUR: the bind, the dual-write mirror, the init engine->store seed, and the
   gate-mod cycle. Missing one leaves the control half-working. Grep every read/write of the
   param id (getValue/setValue AND the id accessor) and account for each.
4. **Pick the widget path by what the control IS:**
   - grid-edited (LOR): redirect the existing save/load helper to the store; no new widget.
   - knob (attenuverters, sends): bindStoreKnob with get/set lambdas. For PER-VOICE controls
     (sends), resolve the slot LIVE inside the lambda (slot = voiceSlot(viewVoice+1)) so a
     view switch re-targets the same knob -- do NOT capture a fixed slot at bind time.
   - cycling cell (direction): give the widget optional get/setStateFn callbacks and bind via
     bindWidget (bare, no paramId, same pattern as the shipped StoreKnob). Leave the callbacks
     unset elsewhere so still-param-backed siblings are untouched -- one widget, both modes.
5. **Delete the sync machinery, don't preserve it.** The proxy pattern's load/store dance +
   clobber guard + last<X>Voice latch exist ONLY to keep a display proxy in sync. Once the
   widget edits the store directly there is no proxy, so delete the whole apparatus -- it's
   removing a failure mode, not losing a feature.
6. **Remove configParams, KEEP the ids declared.** Deleting enum entries renumbers every later
   id. Drop the configParam calls, leave the id accessors, add a "STORE-BACKED, id kept" note.
   config() uses a fixed NUM_ constant so the count is unchanged.
7. **Match sibling undo behaviour, don't exceed it.** East/Mono cycling cells have no undo, so
   Macro's don't either -- adding voice-correct undo to one module alone makes it inconsistent.
   Uniform undo is a separate cross-module pass (see undo backlog).

### Two traps that bit us (watch for both on Mono/East)
- **Inverted-seed on load.** When a param that was a DISPLAY MIRROR of the engine becomes the
  AUTHORITATIVE persisted store, any "seed store FROM engine on init" code inverts meaning: it
  now CLOBBERS the loaded value with the engine's default (direction didn't survive save/load
  until this seed was removed). Audit every engine->param init sync when de-paramming; if the
  param is now the store, the seed must go.
- **Comment lane-convention drift.** Editor lane vs engine lane (ENGINE_LANE_TO_EDITOR /
  EDITOR_TO_ENGINE_LANE). The code was right but a comment I wrote claimed the wrong one --
  a future-bug seed. State the lane basis explicitly and verify against the manager's read
  index, not the loop variable name.

### Assembly discipline (this whole session's recurring cost)
Find the WORKING instance in the codebase and match its exact idiom rather than writing from
memory: the ModArcOverlay namespace (redDot::), the shadowed-`module` ctor param (capture the
local, not [this]), bindStoreKnob's Tag + resolver args. Every scripted edit needs a match-
count assertion; walk braces programmatically (strip comments/strings first -- em-dashes and
`{` in comments give false mismatches).
