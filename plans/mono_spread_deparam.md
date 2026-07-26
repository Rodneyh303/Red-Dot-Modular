# Plan — Mono Sands de-param: spread base (4) + spread atten (4)

Goal of this pass: remove Mono's 8 spread-related host-exposed params
(`sprId(l)` ×4 base + `sprAttenId(l)` ×4 atten) by moving them to the store,
mirroring how Macro was de-parammed (MVC_UNIFICATION.md step 1d). This is the
atomic unit unblocked now that `StoreKnob` has `lockWhen`/`displayValueFn`.

VCV-as-VST caps a patch at 1024 host params; dot.modular exceeds that, so the
visual/editor expanders must expose nothing to host automation.

## Verified facts (no guessing)

- **Store APIs exist:**
  - `getSpread(slot, lane)` / `setSpread(slot, lane, x)` → `editor.spread[slot*4+lane]` (Monsoon.hpp:724).
  - `getMonoAtten(lane, col)` / `setMonoAtten(lane, col, x)` → `editor.monoAtten[lane*4+col]`, lane=EDITOR 0..5, col 3=spread atten (Monsoon.hpp:715).
- **Both already persist:** `editorSpread` (64) and `editorMonoAtten` (24) are in PersistenceManager (save 196-198/212, load 450-451/465). No persistence work needed.
- **StoreKnob has lock/display:** `lockWhen` + `displayValueFn` (StoreBound.hpp:53-69). `shownValue()` returns the display override while locked (non-NaN), else the stored value. Drag/reset are no-ops while locked. This is exactly what Mono's delegated spread base needs.
- **`kMonoSlot = 0`** (VoiceResolver.hpp:62). Mono's spread slice is `getSpread(kMonoSlot, l)`.
- **`gMon` is NOT in scope** at the Mono spread block (MonsoonSandsManager.cpp ~251-290). That block currently calls `redDot::findMonsoonEitherSide(monoVis)` inline at lines 282/287. Must declare `Monsoon* gMon = redDot::findMonsoonEitherSide(monoVis);` at the top of the block.
- **Mono's arc `getSetNorm` is the paramId coupling** (MonsoonSandsVisualExpander.cpp:53-58): `int pid = knob->paramId; ... paramQuantities[pid]->getScaledValue()`. This MUST be decoupled in the same change (atomicity rule) — once the knob is a StoreKnob it has no paramId and the arc would read a dead param.

## Reference: how Macro did it (done)

- `pendingSpreadArcs` is `vector<pair<Widget*, int>>` (StraitsSandsMacroVisual.cpp:66) — already `Widget*`.
- Spread base: `bindStoreKnob<Monsoon, Tag_Grey_Trim_Bar>` with `get/setGlobalSpread(lane)` (lines 198-202). NO lockWhen/displayValueFn (Macro is global, never delegates).
- Arc `getSetNorm` reads `getGlobalSpread(lane)` normalised `(v+1)/2` (line 91).

## Lane-convention trap (the signature de-param bug)

Two different conventions in play — DO NOT mix:
- **spread base** `editor.spread[slot*4+lane]`: `lane` is ENGINE/spread order (REST=0,MEL=1,OCT=2,ACC=3). Use `getSpread(kMonoSlot, l)` with `l` = engine spread lane.
- **spread atten** `editor.monoAtten[lane*4+col]`: `lane` is EDITOR order (MEL=0,OCT=1,REST=2,ACC=3). Use `getMonoAtten(SPREAD_LANE_TO_EDITOR[l], 3)` (== `ENGINE_LANE_TO_EDITOR[l]`).

The widget bind and the manager read MUST use the SAME convention per array.

## Steps

### 1. Mono widget: storeResolver + spread base StoreKnob (MonsoonSandsVisualExpander.cpp)
- Verify/add a `storeResolver()` lambda returning `Monsoon*` (lazy, via `getMonsoon()`/`findMonsoonEitherSide`), mirroring Macro's `storeResolver()` (StraitsSandsMacroVisual.cpp:68). Mono currently uses `getMonsoon()` (line 165) — wrap it.
- In the spread loop (l=0..3, ~lines 145-186):
  - Replace `createParamCentered<DimmableTrimpot>(mm2px(Vec(SPR_BASE_X, y)), mod, sprId(l))` + `addParam(sp)` with `placeStoreKnob<Monsoon, Tag_Grey_Trim_Bar>(this, Vec(SPR_BASE_X, y), storeResolver(), -1.f, 1.f, 0.f, std::string(nm)+" spread", get/setSpread(kMonoSlot, l))`. (Mono places by explicit mm coords → use `placeStoreKnob` Placement B, not `bindStoreKnob`.)
  - Carry the lock/display onto the returned `StoreKnob*` VERBATIM from the existing DimmableTrimpot wiring (lines 159-175):
    - `sp->lockWhen = [this, edLane]() { return buildV1Topo().lockedOn(MONO, 0, edLane); };`
    - `sp->displayValueFn = [this, spLane, edLane]() -> float { ... return clamp(macroVis->macroBase[spLane][3] + macroVis->macroSendDelta[spLane][3], -1, 1); }` (NaN when not delegated).
  - `pendingSpreadArcs.push_back({sp, l})` — `sp` is now `StoreKnob<Monsoon>*` (a `Widget*`).

### 2. Mono widget: spread atten StoreKnob (same file/loop)
- Replace `createParamCentered<Trimpot>(mm2px(Vec(SPR_ATTEN_X, y)), mod, sprAttenId(l))` + `addParam` with `placeStoreKnob<Monsoon, Tag_Grey_Trim_Bar>(this, Vec(SPR_ATTEN_X, y), storeResolver(), -1.f, 1.f, 0.f, std::string(nm)+" spread depth", get/setMonoAtten(editorLane, 3))` where `editorLane = SPREAD_LANE_TO_EDITOR[l]`.
- Plain (no lockWhen) — matches current Trimpot (the atten is not locked today). Optional later: lock when delegated to match base.

### 3. Mono widget: arc decouple (same file, flushSpreadArcs ~44-93)
- Retype `pendingSpreadArcs` to `vector<pair<rack::widget::Widget*, int>>`.
- Delete `int pid = knob->paramId;` (line 53).
- Rewrite `getSetNorm` (54-58) to read the store, mirroring Macro:
  ```cpp
  arc->getSetNorm = [mm, sprIdx]() -> float {
      Monsoon* mon = mm ? redDot::findMonsoonEitherSide(mm) : nullptr;
      if (!mon) return 0.5f;
      return rack::math::clamp((mon->getSpread(dotModular::VoiceResolver::kMonoSlot, sprIdx) + 1.f) * 0.5f, 0.f, 1.f);
  };
  ```
- `getModNorm`/`isActive` unchanged (already read `engine.spreadE` + inputs, not params).
- `attachOverKnob(knob, ...)` already takes `Widget*` — works unchanged.

### 4. Mono hpp: drop the 8 configParams (MonsoonSandsVisualExpander.hpp ~166-171)
- Remove `configParam(sprId(l), ...)` (line 168) and `configParam(sprAttenId(l), ...)` (line 169).
- KEEP `configInput(sprCvId(l), ...)` (line 170) — the CV jack is an input, not a param.
- KEEP the id enum entries (`SPR_REST..`, `SPR_ATTEN_START`, `sprId`, `sprAttenId`) declared — deleting them renumbers later ids. Add a "STORE-BACKED, id kept" note.
- Do NOT right-size `config()`/`NUM_PARAMS` yet — incremental migration keeps NUM_PARAMS=54 so id math is stable; right-size to 0 only when all Mono groups are done.

### 5. Manager redirect (MonsoonSandsManager.cpp, Mono spread block ~251-290)
- At the top of the block add: `Monsoon* gMon = redDot::findMonsoonEitherSide(monoVis);`
- Line 271: `sin.base = monoVis->params[Mono::sprId(l)].getValue();` → `sin.base = gMon ? gMon->getSpread(dotModular::VoiceResolver::kMonoSlot, l) : 0.f;`
- Line 276: `sin.ownCv.atten = monoVis->params[Mono::sprAttenId(l)].getValue();` → `sin.ownCv.atten = gMon ? gMon->getMonoAtten(dotModular::ENGINE_LANE_TO_EDITOR[l], 3) : 0.f;`
- This is the A/B proof: behaviourally inert if steps 1-2 are correct (store holds what the param held).

### 6. Build + verify
- Build clean.
- Behaviour (silent-failure checks — a wrong index compiles but gives a wrong value):
  - Spread base knobs respond and write the store (drag moves the arc set-line).
  - Spread atten knobs scale Mono's own spread CV.
  - Mod-arcs track both set and mod values; arcs still draw (getSetNorm no longer dead).
  - Delegating a lane to Macro: base knob locks + shows Macro's spread (base+sendDelta); reclaiming reverts to Mono's stored value.
  - Patch save/reload preserves all 8 values (editorSpread + editorMonoAtten persist).
  - Mono's host param count drops by 8.

## Traps carried from the playbook (watch)
- **Inverted-seed on load:** none here — Mono spread was never an engine→param display mirror (the param WAS the base). No seed code to remove.
- **Comment lane-convention drift:** state the lane basis (engine for spread base, editor for atten) in any comment, verified against the manager read index.
- **Atomicity:** steps 1+3 (widget swap + arc decouple) MUST land together — an arc reading a dead paramId is the failure mode. Step 5 (manager redirect) lands with them (the param is gone, so the reader must move in the same change).
- **Assembly discipline:** match Macro's exact `placeStoreKnob`/`bindStoreKnob` Tag + resolver idiom; don't write from memory.

## After this pass
Mono remaining: LOR (18), attens (18), owners (4), direction (6) — per MVC_UNIFICATION.md census. Then East (38). Same recipe.
