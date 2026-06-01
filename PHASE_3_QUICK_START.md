# Phase 3: Quick Start Implementation Checklist

## What You Have

### Production Code Files

| File | Lines | Purpose |
|------|-------|---------|
| SandsVisualEditorV2.hpp | 450 | Phase 2: Editor with keyboard/undo/presets |
| StraitsVisualEditorPanel.hpp | 350 | Phase 3: Main container, tabs, sync |
| SandsParameterManager | ~150 | Abstract parameter mapping |
| MonsoonParameterManager | ~200 | Concrete Monsoon implementation |
| StraitsModuleWidgets.hpp | ~100 | Module widget templates |
| StraitsEastWest.hpp | ~150 | Module classes |

**Total:** ~1,400 lines of production code

### Documentation

| Document | Purpose |
|----------|---------|
| PHASE_3_INTEGRATION.md | Complete technical reference |
| PHASE_3_TESTING_GUIDE.md | Testing scenarios & troubleshooting |
| PHASE_2_FEATURES.md | Phase 2 reference |
| SANDS_VISUAL_EDITOR_IMPLEMENTATION.md | Phase 1 reference |

---

## 5-Step Implementation Guide

### Step 1: Update Parameter Constants (15 min)

**File:** `src/ui/MonsoonSandsIntegration.hpp`

Check your `Monsoon.hpp` and update these constants:

```cpp
struct MonsoonParameterManager : SandsParameterManager {
  // CUSTOMIZE THESE!
  static constexpr int POLY_DNA_VOICE_1_LEN = 0;          // ← Your actual value
  static constexpr int POLY_DNA_VOICE_1_OFF = 1;          // ← Your actual value
  static constexpr int POLY_DNA_VOICE_1_ROT = 2;          // ← Your actual value
  
  static constexpr int POLY_MELODY_VOICE_1_LEN = 48;      // ← Your actual value
  // ... etc for other lanes
  
  static constexpr int VOICE_STRIDE = 3;                  // Usually 3
};
```

**How to find your values:**
1. Open `Monsoon.hpp`
2. Find `enum ParamId { ... }`
3. Look for `POLY_DNA_VOICE_1_LEN` (first one)
4. Count from start: this is your POLY_DNA_VOICE_1_LEN value
5. Next value is POLY_DNA_VOICE_1_OFF
6. Next value is POLY_DNA_VOICE_1_ROT
7. Repeat for MELODY, OCTAVE, LEGATO, VARIATION

**Verification:**
```cpp
// After updating, verify by checking Monsoon param count
if (POLY_DNA_VOICE_1_LEN >= module->params.size()) {
  WARN("Parameter ID out of range!");
}
```

### Step 2: Implement Module Methods (20 min)

**File:** Your `Monsoon.hpp` or module implementation

Add/update these methods:

```cpp
struct Monsoon : rack::Module {
  // ... existing code ...
  
  int currentPlayStep = -1;
  
  // Called each DSP cycle
  void process(const ProcessArgs& args) override {
    // ... normal DSP ...
    
    // Update current playback step (0-15)
    // This calculation depends on your sequencer design
    // Example (if you have playbackPhase):
    currentPlayStep = (int)(playbackPhase / (2.0f * M_PI) * 16.0f) % 16;
    
    // Tell expanders about current step
    if (rightExpander.module) {
      auto* straits = dynamic_cast<StraitsEastSands*>(rightExpander.module);
      if (straits) {
        straits->setCurrentStep(currentPlayStep);
      }
    }
  }
  
  // New method
  int getCurrentStep() { return currentPlayStep; }
};
```

### Step 3: Wire Module Widgets (20 min)

**File:** `src/ui/MonsoonSandsIntegration.hpp` (already mostly done)

**Just make sure:**
1. `StraitsEastSandsWidget` has proper panel path
2. `visualPanel` is created with correct module pointer
3. `step()` method calls `setCurrentPlayStep()`

**Verify:**
```cpp
struct StraitsEastSandsWidget : rack::ModuleWidget {
  StraitsEastSands* module;
  StraitsEastSandsPanel* visualPanel;
  
  StraitsEastSandsWidget(StraitsEastSands* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, 
      "res/panels/StraitsEastSands_Visual_24HP.svg")));  // ← Check path
    
    visualPanel = new StraitsEastSandsPanel(module);     // ← Check created
    visualPanel->box.pos = rack::Vec(10, 60);
    visualPanel->box.size = rack::Vec(210, 220);
    addChild(visualPanel);
  }
  
  void step() override {
    ModuleWidget::step();
    if (module && visualPanel && module->monsoonModule) {
      auto* monsoon = static_cast<Monsoon*>(module->monsoonModule);
      visualPanel->setCurrentPlayStep(monsoon->getCurrentStep());  // ← Check called
    }
  }
};
```

### Step 4: Create SVG Panels (15 min)

**Files:** `res/panels/StraitsEastSands_Visual_24HP.svg` and `_West_`

Create minimal panels (24HP = 230.4mm width):

```xml
<svg viewBox="0 0 230 380" xmlns="http://www.w3.org/2000/svg">
  <!-- Background -->
  <rect width="230" height="380" fill="#141416"/>
  
  <!-- Title -->
  <text x="115" y="20" text-anchor="middle" font-size="12" fill="#d4af37">
    STRAITS EAST
  </text>
  
  <!-- Border -->
  <rect x="0" y="0" width="230" height="380" 
        fill="none" stroke="#333333" stroke-width="2"/>
</svg>
```

### Step 5: Register Models (10 min)

**File:** `plugin.cpp`

Add these model registrations:

```cpp
#include "ui/MonsoonSandsIntegration.hpp"

// At end of plugin.cpp or in initPlugin():
Model* modelStraitsEastSands = createModel<
  redDot::StraitsEastSands,
  StraitsEastSandsWidget
>("StraitsEastSands");

Model* modelStraitsWestSands = createModel<
  redDot::StraitsWestSands,
  StraitsWestSandsWidget
>("StraitsWestSands");

// In initPlugin() or similar:
void Plugin::registerModels() {
  // ... existing models ...
  p->addModel(modelStraitsEastSands);
  p->addModel(modelStraitsWestSands);
}
```

---

## Testing Checklist

After implementing all 5 steps:

### Quick Test (5 min)
- [ ] Compile without errors
- [ ] Load VCV Rack
- [ ] Find Straits East in browser
- [ ] Place module
- [ ] Module appears (panel loads)
- [ ] Visual editor visible (tabs, lanes)

### Sync Test (10 min)
- [ ] Load Monsoon + Straits East
- [ ] Play Monsoon
- [ ] Click a REST lane bar in editor
- [ ] Hear playback change (voice affected)
- [ ] Stop and adjust another voice
- [ ] Verify correct voice changes

### Full Test (30 min)
Follow **Test Scenario 1-6** in `PHASE_3_TESTING_GUIDE.md`:
1. Basic Parameter Sync ✓
2. Voice Switching ✓
3. Playback Sync Animation ✓
4. State Persistence ✓
5. Keyboard Shortcuts ✓
6. Preset System ✓

---

## Common Mistakes to Avoid

### ❌ Don't: Forget to update parameter constants

```cpp
// WRONG - uses placeholder values
static constexpr int POLY_DNA_VOICE_1_LEN = 0;
```

✅ Do:
```cpp
// Check your Monsoon.hpp and use actual values
static constexpr int POLY_DNA_VOICE_1_LEN = 142;  // Your actual value
```

### ❌ Don't: Calculate parameter IDs wrong

```cpp
// WRONG - doesn't account for stride
int paramId = laneBase + voiceNumber + step;
```

✅ Do:
```cpp
// Correct stride calculation
int voiceIndex = voice - 1;  // Convert 1-based to 0-based
int voiceOffset = voiceIndex * VOICE_STRIDE;
int paramId = laneBase + voiceOffset + step;
```

### ❌ Don't: Forget to sync on voice switch

```cpp
// WRONG - loses state when switching
void selectVoice(int v) {
  selectedVoice = v;
  syncModuleToEditor(v);  // Only loads new, doesn't save old
}
```

✅ Do:
```cpp
// Correct - save before switching
void selectVoice(int v) {
  syncEditorToModule(selectedVoice);  // Save current voice
  selectedVoice = v;
  syncModuleToEditor(selectedVoice);  // Load new voice
}
```

### ❌ Don't: Call setCurrentStep() every frame

```cpp
// WRONG - unnecessary updates
void step() override {
  visualPanel->setCurrentPlayStep(module->getCurrentStep());  // Every frame!
}
```

✅ Do:
```cpp
// Correct - but okay to call every frame at 60Hz
// (StraitsVisualEditorPanel handles the animation, not continuous updates)
void step() override {
  if (module && visualPanel) {
    visualPanel->setCurrentPlayStep(module->getCurrentStep());  // Fine at 60Hz
  }
}
```

---

## File Checklist

Make sure these exist:

```
src/ui/
├── SandsVisualEditorV2.hpp              ✓ (Phase 2)
├── StraitsVisualEditorPanel.hpp         ✓ (Phase 3)
├── SandsParameterManager (in Panel)     ✓
├── MonsoonSandsIntegration.hpp          ✓ (Phase 3)
├── StraitsModuleWidgets.hpp             ✓ (Phase 3, templates)
└── (your existing UI files)

src/modules/
├── StraitsEastWest.hpp                  ✓ (Phase 3)
└── (your existing modules)

res/panels/
├── StraitsEastSands_Visual_24HP.svg     ✓ (You create this)
├── StraitsWestSands_Visual_24HP.svg     ✓ (You create this)
└── (your existing panels)

Documentation/
├── PHASE_3_INTEGRATION.md               ✓
├── PHASE_3_TESTING_GUIDE.md             ✓
├── PHASE_2_FEATURES.md                  ✓
└── SANDS_VISUAL_EDITOR_IMPLEMENTATION.md✓
```

---

## Estimated Time

| Step | Time | Complexity |
|------|------|-----------|
| 1. Update constants | 15 min | Very easy |
| 2. Module methods | 20 min | Easy |
| 3. Widget wiring | 20 min | Easy |
| 4. SVG panels | 15 min | Easy |
| 5. Model registration | 10 min | Very easy |
| **Testing & debug** | 30-60 min | Medium |
| **Total** | 1.5-2.5 hours | Easy-Medium |

---

## Success Criteria

Phase 3 is complete when:

✅ **All modules compile without errors**
✅ **Visual editor appears in VCV Rack**
✅ **Editing editor changes playback**
✅ **All 7 (or 8) voices can be edited**
✅ **Voice switching works**
✅ **Active step indicator animates**
✅ **Patches save/load correctly**
✅ **No crashes or memory leaks**
✅ **All 6 test scenarios pass**

---

## Debugging Tips

### Enable Verbose Logging

```cpp
// At top of MonsoonParameterManager
#define DEBUG_PARAMS 1

int getStepParamId(int lane, int voice, int step) override {
  int paramId = /* calculate */;
  
  #ifdef DEBUG_PARAMS
  DEBUG("Lane %d Voice %d Step %d → ParamId %d", lane, voice, step, paramId);
  #endif
  
  return paramId;
}
```

### Inspect Parameter Values

```cpp
// In widget step()
void step() override {
  if (module && module->monsoonModule) {
    auto monsoon = static_cast<Monsoon*>(module->monsoonModule);
    
    // Print first few param values
    for (int i = 0; i < 10; ++i) {
      DEBUG("Param[%d] = %f", i, monsoon->params[i].getValue());
    }
  }
  ModuleWidget::step();
}
```

### Watch Parameter Sync in Real-Time

In Rack UI:
1. Place Monsoon + Straits East
2. Right-click on a parameter → "Edit value"
3. Change value in real-time
4. Watch editor to see if it updates (should update every ~16ms)

---

## Next: Phase 4

After Phase 3 is stable:

- [ ] Pattern interpolation (morph between presets)
- [ ] Humanization controls (add controlled randomness)
- [ ] Drag-and-drop between voices
- [ ] Pattern library UI
- [ ] Cross-module coordination

See `PHASE_3_INTEGRATION.md` "Next Steps (Phase 4)" for details.

