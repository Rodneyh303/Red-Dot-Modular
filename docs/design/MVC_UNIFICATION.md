
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
