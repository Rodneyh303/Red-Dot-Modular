
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
