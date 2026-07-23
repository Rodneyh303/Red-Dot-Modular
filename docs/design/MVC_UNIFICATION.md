
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
