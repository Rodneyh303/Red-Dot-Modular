# Lane Expander — Phase 1 Reconciliation Report

## The gap: scaffold vs refined design

### 1. Registration state (THE KEY ISSUE)

**Current scaffold**: `modelStraitsLaneQMIX` IS registered in `plugin.cpp` (line 43: `p->addModel(modelStraitsLaneQMIX)`) and in `plugin.json` (slug `StraitsLaneQMIX`). This makes it **browser-draggable** — the user can add it from the browser like any module.

**Refined design**: Lane extenders must be **UNREGISTERED** — no `addModel`, no plugin.json entry. Only `StraitsBase` is browsable. The base spawns lane extenders in code via context menu.

**Impact**: This is why Rack hung when the user tried to add QMIX from the browser. The lane expander module was dragged from the browser without a base context — its `findMonsoonEitherSide()` and `leftExpander` chain found nothing, and the widget's binding/step logic deadlocked or crashed.

### 2. Spawning mechanism

**Current scaffold**: No spawning mechanism exists. Lane expanders are added via browser drag. The user manually places them next to the base.

**Refined design**: The base owns spawning. Context menu items ("Add QMIX Lane", "Add REST Lane", etc.) instantiate a `StraitsLaneExpander` module, set its descriptor, and dock it as rightExpander. Fixed-order: the base knows which lanes are attached by walking rightExpander.

### 3. Persistence

**Current scaffold**: Since the lane expander is registered, Rack auto-saves and restores it as a normal module. No special persistence needed.

**Refined design**: Since the lane expander is unregistered, Rack won't auto-restore it. The base must:
- Save which lanes are attached (in `dataToJson`/`dataFromJson`)
- On load, re-spawn the lane expander modules and set their descriptors

### 4. QMIX default-attached

**Current scaffold**: QMIX is a separate browser-draggable module. The user must add it manually.

**Refined design**: QMIX is the DEFAULT-attached lane — when the user adds a StraitsBase, it automatically spawns a QMIX lane expander docked right. The user can remove it via context menu.

### 5. Pointer chain and seamless rendering

**Current scaffold**: The base's `step()` walks `rightExpander` to sync lane expander params. This mechanism is CORRECT for the refined design — it just needs to work with spawned modules.

**Refined design**: Same pointer chain, same sync. No change needed here.

## What it would take to reconcile

### Step 1: Unregister the lane expander
- Remove `p->addModel(modelStraitsLaneQMIX)` from `plugin.cpp`
- Remove the `StraitsLaneQMIX` slug from `plugin.json`
- Keep the `Model* modelStraitsLaneQMIX` definition in `StraitsLaneExpander.cpp` — the base needs it to spawn instances

### Step 2: Add context-menu spawning to StraitsBase
- In `StraitsBaseWidget::appendContextMenu()`, add "Add Lane >" submenu with QMIX (and later REST, ACCENT)
- On selection: `modelStraitsLaneQMIX->createModule()` → set descriptor → `APP->engine->addModule()` → place as rightExpander → create widget

This requires understanding how Rack modules spawn other modules. The VCV Rack API for this is:
```cpp
engine::Module* m = model->createModule();
APP->engine->addModule(m);
ModuleWidget* w = model->createModuleWidget(m);
APP->scene->rack->addModuleAt(w, position);
```

### Step 3: Add persistence to StraitsBase
- In `MonsoonStraitsExpander::dataToJson()` (or the base's override): save the list of attached lane descriptors as a JSON array
- In `dataFromJson()`: read the list, re-spawn the lane expanders

Since `MonsoonStraitsExpander` already has `dataToJson`/`dataFromJson` (via Module), the base needs to override or extend these. This is the trickiest part — Rack's module spawning during load requires careful timing.

### Step 4: Auto-spawn QMIX on first creation
- In the base widget's constructor (or first `step()`): if no lane expanders are attached, spawn a QMIX lane expander

### Step 5: Add "Remove Lane" context menu
- On each lane expander (or via the base's context menu), add a "Remove this lane" option that removes the module from the engine

## What stays the same (no rework needed)
- `StraitsLaneExpander.hpp` — LaneDescriptor + module (correct design)
- `StraitsLaneExpander.cpp` — knob binding loop, mod-arc, dim/lock (all copied from Straits, correct)
- `gen_straits_lane.py` — panel generator (correct)
- The base's `step()` param-sync logic (correct mechanism)
- The null-guard for browser preview (still needed for `Model::createWidget()` in previews)

## Risk assessment
- The spawning/persistence mechanism is the main new work — it's Rack API plumbing, not domain logic
- The lane expander widget code is already correct — no rework needed
- The base widget needs context menu + persistence additions, but the IO/voice-count/step() logic stays
- Total reconciliation: moderate, focused on plugin.cpp (unregister) + StraitsBase.cpp (context menu + persistence + auto-spawn)
