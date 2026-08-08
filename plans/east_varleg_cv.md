# Straits East — VARIATION / LEGATO poly CV inputs

## Goal
Add poly CV input jacks + attenuverter depth for the VARIATION (editor lane 4) and
LEGATO (editor lane 5) lanes on `StraitsEastSandsVisual`, mirroring the existing
MELODY/OCTAVE/REST/ACCENT lane pattern — but **LEN/OFF/ROT only** (no SPR, no spread
knob — spread does not apply to VAR/LEG). Channel 1 (poly cable ch0) adds to mono
Monsoon's VAR/LEG modulation, exactly as the existing lanes' ch0 feeds the V1 mono
strand via the `v1Editable` write path.

## Current state (verified)
- Existing lanes (REST/MEL/OCT/ACC) each have 4 CV jacks `cvId(lane,col)=lane*4+col`
  (0..15), 4 atten display proxies `attenDispId=4+lane*4+col` (4..19), per-voice depth
  `attenId(v,lane,col)=MACRO_ATTEN_START+v*16+lane*4+col` (16-wide, slot0=mono), and 1
  spread trimpot. East `NUM_INPUTS=16`.
- VAR/LEG already have per-voice **LOR base** params (`POLY_VARIATION_VOICE_1_LEN+v*3+c`,
  `POLY_LEGATO_VOICE_1_LEN+v*3+c`) and a delegation toggle (`VARLEG_DELEG_START`), but
  **no CV inputs, no atten, no spread**. The expander manager VAR/LEG block
  (`MonsoonExpanderManager.cpp:167-185`) reads only the base params (no CV).
- CV convention (existing lanes): `combineLOR`/`eastLorVal` read
  `inputs[cvId].getPolyVoltage(v)` (v=poly voice 0..14) with per-voice `attenId(slot,..)`
  depth. The mono/V1 mix-in (ch0) is applied in the widget `v1Editable` path via
  `addCV(... getPolyVoltage(0) ...)` + the mono-slot (`kMonoSlot`) atten, which the widget
  mirrors from the display proxies each frame.
- Panel: `gen_east_clean.py` draws 4 control rows (N=4) at `rowY(0..3)`; editor draws 6
  lanes (ED_LANES=6) but only rows 0..3 have CV/atten/spread markers. VAR/LEG rows
  (editor 4/5) currently have only the `param_owner_4/5` delegation cell.

## Design (append-only — patch stability not required)

### 1. `StraitsEastSandsVisual.hpp` — IDs + config
- **Inputs** (append to `InputId`): `VARLEG_CV_START = 16`, `NUM_INPUTS = 16 + 6 = 22`.
  Helper `varlegCvId(lane, col) = VARLEG_CV_START + lane*3 + col` (lane 0=VAR, 1=LEG;
  col 0..2 = LEN/OFF/ROT).
- **Display-proxy atten** (append to `SpreadParamId`): `VARLEG_ATTEN_DISP_START = 20`
  (after `ATTEN_START+16`). `NUM_SPREAD_PARAMS` becomes `+22`. Helper
  `varlegAttDispId(lane,col) = VARLEG_ATTEN_DISP_START + lane*3 + col`.
- **Per-voice depth store** (new `MonsoonIds` bank, appended after current
  `NUM_PARAMS`): `VARLEG_ATTEN_START`, 16 voices × 6 = 96 params. Helper
  `varlegAttId(v, lane, col) = MonsoonIds::VARLEG_ATTEN_START + v*6 + lane*3 + col`
  (v=0..15, slot 0 = mono).
- In the ctor: `configInput(varlegCvId(...))` ×6; `configParam(varlegAttDispId(...))` ×6;
  `configParam(varlegAttId(v,...))` for v=0..15 ×6.

### 2. `Monsoon.hpp` — append `VARLEG_ATTEN_START` bank
- `VARLEG_ATTEN_START = NUM_PARAMS` (current), `VARLEG_ATTEN_END = + 96`,
  `NUM_PARAMS = VARLEG_ATTEN_END`. Add the enum entry near `VARLEG_DELEG_*`.

### 3. `MonsoonExpanderManager.cpp` — per-voice CV in the VAR/LEG block (lines 167-179)
Add a `varlegLorVal` lambda mirroring `eastLorVal` (no Macro blend — VAR/LEG never
Macro-owned):
```cpp
auto varlegLorVal = [&](int paramIdx, int vl, int c, float lo, float hi)->int {
    float base = eastLOR->params[paramIdx].getValue();
    if (eastVisual->inputs[StraitsEastVisualIds::varlegCvId(vl,c)].isConnected()) {
        float att = eastLOR->params[StraitsEastVisualIds::varlegAttId(slot,vl,c)].getValue();
        float cv  = eastVisual->inputs[StraitsEastVisualIds::varlegCvId(vl,c)]
                        .getPolyVoltage(v) / 10.f;          // poly voice v (0..14) → V(v+2)
        base = math::clamp(base + cv*att*(hi-lo), lo, hi);
    }
    return (int)std::lround(base);
};
```
Replace the six `rd(varBase/legBase + k, lo, hi)` writes with
`varlegLorVal(varBase/legBase + k, 0/1, k, lo, hi)`. `slot` already computed in scope.

### 4. `StraitsEastSandsVisual.cpp` — widget
- **Bind** the 6 jacks (`input_16..21` → `varlegCvId`) and 6 atten proxies
  (`param_20..25` → `varlegAttDispId`) with `bindInput`/`bindParam` (gold poly port +
  `DimmableTrimpot`), in the same loop style as the existing lanes. Apply the same
  theme cfg + a `lockWhen`/`dimWhen` consistent with VAR/LEG delegation (live on poly
  tabs; locked on the mono tab when Mono owns V1).
- **`v1Editable` VAR/LEG write (lines 989-995):** add an `addCV` lambda reading
  `varlegCvId(vl,item).getPolyVoltage(0)` + `varlegAttId(kMonoSlot,vl,item)` depth, and
  apply to length/offset/rotation before `eng.setStrand(EAST, el, ...)`. This is the
  ch1→mono mix-in.
- **V1 mono-slot atten mirror** (tab1Mono ~861-864 and v1Editable ~909-912): add a
  `vl=0..1, c=0..2` loop mirroring `varlegAttDispId` → `varlegAttId(kMonoSlot,..)` so V1
  CV depth is non-zero.
- **`saveVoiceMacro`/`loadVoiceMacro`:** add `vl=0..1, c=0..2` copy between
  `varlegAttDispId` and `varlegAttId(slot,..)` (mirrors the existing lane atten copy).

### 5. `gen_east_clean.py` — panel markers
- Add 2 control rows for VAR (row 4) / LEG (row 5) at `rowY(4)`, `rowY(5)`.
- Place LEN/OFF/ROT jacks at `JACK_X[0..2]` (6,15,24) and attens at `ATTEN_X[0..2]`
  (43,52,61). **No SPR jack, no SPR atten, no spread knob.**
- Emit kit markers: `input_<varlegCvId>` (16..21) and `param_<varlegAttDispId>` (20..25)
  via the existing `kit_shape(...)` helper, using `DISPLAY_ORDER`-independent rows 4/5
  (VAR/LEG are editor lanes 4/5 directly, not engine lanes).
- Extend the left control recess box (`gx,gy,gw,gh`) and the editor lane dividers
  (`range(1, ED_LANES)` with `ED_H/ED_LANES`) to cover all 6 rows (currently only 4).

## CV flow (per voice + mono mix-in)
```mermaid
flowchart LR
  subgraph Poly voice v = 0..14
    CV[VAR/LEG CV jack ch v+1] --> MGR[ExpanderManager varlegLorVal]
    ATT1[varlegAttId slot,v] --> MGR
    MGR --> ENG[engine.polyLORRef v, VAR/LEG]
  end
  subgraph Mono V1 ch0 mix-in
    CV0[VAR/LEG CV jack ch0] --> W[v1Editable addCV]
    ATT0[varlegAttId kMonoSlot] --> W
    W --> STR[engine.setStrand EAST, VAR/LEG]
  end
```

## Behaviour notes / to-verify
- **Delegated (follow-mono):** engine ignores per-voice VAR/LEG LOR, so per-voice CV is
  inaudible for that voice (it follows mono's CV-modulated strand). Mono ch0 CV still
  applies via the widget. Matches the existing "delegated lane reads the owner" rule.
- **Local East:** per-voice CV applies (manager) + mono ch0 CV applies on the V1 tab.
- **No Macro blend** for VAR/LEG (unchanged — Macro can't own these lanes).
- Locking: VAR/LEG jacks/attens live on poly tabs; on the V1 mono tab they lock when
  Mono owns V1 (tab1MonoMirror), matching the existing lanes' lock predicates.

## Files touched
1. `plugins/Melodicer/src/Monsoon.hpp` — append `VARLEG_ATTEN_START` bank.
2. `plugins/Melodicer/src/StraitsEastSandsVisual.hpp` — IDs + config loops.
3. `plugins/Melodicer/src/StraitsEastSandsVisual.cpp` — bind, v1Editable addCV, mirror, save/load.
4. `plugins/Melodicer/src/dsp/managers/MonsoonExpanderManager.cpp` — `varlegLorVal` in VAR/LEG block.
5. `plugins/Melodicer/panel_src/gen_east_clean.py` — 2 new control rows + markers + recess/divider fix.
6. Regenerate `res/panels/StraitsEastSandsVisual_40HP{,_light}.svg`.
