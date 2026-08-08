# Plan: Eliminate the dual lane-ordering system

## Problem

The codebase has TWO lane-ordering schemes that coexist and are crossed
without consistent use of the conversion tables, causing recurring silent
permutation bugs (found 5+ times in the direction feature alone):

| Ordering | Indices | Used by |
|---|---|---|
| **Editor/Strand order** | MEL=0, OCT=1, REST=2, ACC=3, VAR=4, LEG=5 | `MONO_LANE_TO_STRAND` (identity), `dirDispId`, `dirModId`, `delegModId`, DirCell, tick arrays (`laneTick_`, `laneTickV_`, `macroLaneTick_`), `strandLen/Off/Rot`, `finalRandomByStrand`, panel rows |
| **Engine/PL order** | REST=0, MEL=1, OCT=2, ACC=3 | `macroBase`, `macroCVDelta`, `cvId`, `attenId`, `PL_*` enum, `polyLenE/OffE/RotE`, `macroSendDelta`, `ENGINE_LANE_TO_EDITOR` |

The conversion tables `EDITOR_TO_ENGINE_LANE[4]` and `ENGINE_LANE_TO_EDITOR[4]`
exist in `LaneMapping.hpp` but are used inconsistently. Every crossing without
them is a latent bug.

## Target

**One ordering everywhere: editor/strand order (MEL=0, OCT=1, REST=2, ACC=3, VAR=4, LEG=5).**

This is already the strand order (`STRAND_MELODY=0`, etc.), the editor lane order,
the panel row order, and the `MONO_LANE_TO_STRAND` identity. Making everything use
this order eliminates the conversion tables entirely.

The only place engine order survives is the `PL_*` enum (`PL_REST=0, PL_MELODY=1, ...`)
and the `macroBase`/`macroCVDelta` arrays. Both can be reordered.

## Steps

### Step 1: Audit (read-only)
Grep for every access to engine-ordered arrays and identify all call sites:
- `macroBase[l]` / `macroCVDelta[l]` — 4-wide, engine-ordered
- `cvId(lane, c)` / `attenId(lane, c)` — East/Macro CV jack/atten, 4-wide engine-ordered
- `PL_REST` / `PL_MELODY` / `PL_OCTAVE` / `PL_ACCENT` — enum
- `polyLenE(v, lane)` / `polyOffE(v, lane)` / `polyRotE(v, lane)` — engine-ordered
- `ENGINE_LANE_TO_EDITOR` / `EDITOR_TO_ENGINE_LANE` — conversion tables
- `DISPLAY_ORDER` in `gen_macro_mono.py` — Python panel generator

### Step 2: Reorder `macroBase` / `macroCVDelta` to editor order
Change `macroBase[4][4]` and `macroCVDelta[4][4]` from engine-ordered to
editor-ordered. Every write site (processDNA) and read site (widget, manager)
must be updated. After this, `macroBase[0]` = MEL, `macroBase[1]` = OCT, etc.

### Step 3: Reorder `PL_*` enum to editor order
Change `PL_REST=0, PL_MELODY=1, PL_OCTAVE=2, PL_ACCENT=3` to
`PL_MELODY=0, PL_OCTAVE=1, PL_REST=2, PL_ACCENT=3` (matching strand order).
Update all switch statements and array accesses that use PL_* indices.

### Step 4: Reorder `cvId` / `attenId` to editor order
These are currently `CV_START + lane*4 + col` where `lane` is engine-ordered.
Change to editor-ordered. The panel SVG kit markers (`input_<n>`, `param_<n>`)
are already editor-ordered on East (sequential `lane = 0..5`). On Macro they
use `DISPLAY_ORDER` — change the generator to sequential editor order.

### Step 5: Remove conversion tables
Delete `ENGINE_LANE_TO_EDITOR` and `EDITOR_TO_ENGINE_LANE` from `LaneMapping.hpp`.
Any remaining references are bugs (compile errors will catch them).

### Step 6: Update Python panel generators
Change `DISPLAY_ORDER` in `gen_macro_mono.py` to `[0,1,2,3]` (identity — already
editor order). Remove the `DISPLAY_ORDER` indirection entirely.

### Step 7: Build + verify + commit
Build, verify all 3 panels work, commit with a clear message.

## Risk

This is a large refactor touching many files. The risk is breaking existing
working code. Mitigations:
- Do it in steps, building after each
- The conversion tables catch remaining bugs as compile errors in step 5
- Pre-release — no patch compatibility needed

## Naming convention (enforce during sweep)

- `el` = editor lane (0=MEL, 1=OCT, 2=REST, 3=ACC, 4=VAR, 5=LEG)
- `strand` = strand index (same as editor lane — identity)
- `engLane` = engine lane (being eliminated)
- `l` = generic loop var — must be annotated in comments if ambiguous

After the sweep, `engLane` should not exist anywhere.
