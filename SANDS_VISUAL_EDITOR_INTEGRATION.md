# Sands Visual Editor Integration Guide

## Overview

Three parameter managers handle wiring the visual editor to the DNA sequencer:

1. **MonoSandsParameterManager** - MonsoonSandsExpander (6 lanes, 1 voice)
2. **PolySandsParameterManager** - Global poly DNA (3 lanes, all voices)
3. **PolyVoiceSandsParameterManager** - Per-voice poly DNA (3 lanes × 7-8 voices)

All three managers:
- Keep existing knob controls intact (length/offset/rotation)
- Add spread control (interpolates probabilities)
- Sync visual editor ↔ PatternEngine without replacing sequencer logic

---

## Architecture

### Data Flow

```
Visual Editor (SandsVisualEditorV4)
    ↓ syncEditorToPatternEngine()
PatternEngine (16-step probability arrays)
    ↓ sequencer reads with length/offset/rotation
Mono Sequencer / Poly Sequencer (actual playback)
    ↓
Audio Output
```

### Spread Control

Spread is a new parameter (0.0-1.0) per lane that interpolates probabilities:

```
spread = 0.0:   No change to probabilities
spread = 0.5:   Probabilities pulled halfway toward 0.5
spread = 1.0:   All probabilities become 0.5 (flat distribution)

Formula:
  displayProb = prob + (0.5 - prob) * spread
  
Effect:
  Creates variation in the probability draws
  Higher spread = more random/uniform behavior
  Lower spread = more structured behavior
```

---

## Mono Sands Integration

### Expander Widget Setup

```cpp
#include "managers/MonoSandsParameterManager.hpp"

struct MonsoonSandsExpanderWidget : rack::ModuleWidget {
  MonsoonSandsExpander* module;
  SandsVisualEditorV4* visualEditor;
  MonoSandsParameterManager* paramManager;
  
  MonsoonSandsExpanderWidget(MonsoonSandsExpander* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(...));
    
    // Create visual editor in MONO mode
    visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::Mode::MONO);
    visualEditor->box.pos = rack::Vec(10, 60);
    addChild(visualEditor);
    
    // Create parameter manager
    // Note: Get monsoonModule from right neighbor
    rack::Module* monsoonMod = module->rightExpander.module;
    auto* patternEngine = /* get from monsoonMod */;
    paramManager = new MonoSandsParameterManager(patternEngine);
  }
  
  void step() override {
    ModuleWidget::step();
    
    if (paramManager && module) {
      // Sync PatternEngine → Editor (read current state)
      paramManager->syncPatternEngineToEditor(visualEditor->currentState);
    }
  }
  
  ~MonsoonSandsExpanderWidget() {
    delete paramManager;
  }
};
```

### Parameter Mapping

Mono Sands uses MonsoonIds enum directly:

```
REST lane:
  Probabilities:  (stored in PatternEngine.rhythmRandom[16])
  Length:         MonsoonIds::DNA_R_LEN_PARAM
  Offset:         MonsoonIds::DNA_R_OFF_PARAM
  Rotation:       MonsoonIds::DNA_R_ROT_PARAM

MELODY lane:
  Probabilities:  (stored in PatternEngine.melodyRandom[16])
  Length:         MonsoonIds::DNA_M_LEN_PARAM
  Offset:         MonsoonIds::DNA_M_OFF_PARAM
  Rotation:       MonsoonIds::DNA_M_ROT_PARAM

(Similar for OCTAVE, LEGATO, ACCENT, VARIATION)
```

### Spread Control (Optional)

Add 6 spread knobs (one per lane):

```cpp
configParam(SPREAD_REST_PARAM,   0.f, 1.f, 0.f, "Rest Spread");
configParam(SPREAD_MELODY_PARAM, 0.f, 1.f, 0.f, "Melody Spread");
configParam(SPREAD_OCTAVE_PARAM, 0.f, 1.f, 0.f, "Octave Spread");
// ... etc for Legato, Accent, Variation

// In process():
float restSpread = params[SPREAD_REST_PARAM].getValue();
float melodySpread = params[SPREAD_MELODY_PARAM].getValue();
// ... read all spreads
```

---

## Poly Sands - Global (Macro) Integration

### Expander Widget Setup

```cpp
#include "managers/PolySandsParameterManager.hpp"

struct MonsoonStraitSandsExpanderWidget : rack::ModuleWidget {
  MonsoonStraitSandsExpander* module;
  SandsVisualEditorV4* visualEditor;
  PolySandsParameterManager* paramManager;
  
  MonsoonStraitSandsExpanderWidget(MonsoonStraitSandsExpander* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(...));
    
    // Create visual editor in POLY mode (3 lanes)
    visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::Mode::POLY);
    visualEditor->box.pos = rack::Vec(10, 60);
    addChild(visualEditor);
    
    // Create parameter manager
    rack::Module* monsoonMod = module->rightExpander.module;
    auto* patternEngine = /* get from monsoonMod */;
    paramManager = new PolySandsParameterManager(patternEngine, module);
  }
  
  void step() override {
    ModuleWidget::step();
    
    if (paramManager && module) {
      // Update spread values from parameters
      float restSpread = module->params[SPREAD_REST_PARAM].getValue();
      float melodySpread = module->params[SPREAD_MELODY_PARAM].getValue();
      float octaveSpread = module->params[SPREAD_OCTAVE_PARAM].getValue();
      
      paramManager->spreadRest = restSpread;
      paramManager->spreadMelody = melodySpread;
      paramManager->spreadOctave = octaveSpread;
      
      // Sync PatternEngine → Editor
      paramManager->syncPatternEngineToEditor(visualEditor->currentState);
    }
  }
  
  ~MonsoonStraitSandsExpanderWidget() {
    delete paramManager;
  }
};
```

### Parameter Mapping

Poly Sands (Global) uses same PatternEngine strands as Mono:

```
Rhythmic lane (shared by all voices):
  Probabilities:  PatternEngine.rhythmRandom[16]
  Length/Offset/Rotation: (same MonsoonIds as Mono)

Melody lane (shared by all voices):
  Probabilities:  PatternEngine.melodyRandom[16]
  Length/Offset/Rotation: (same MonsoonIds as Mono)

Octave lane (shared by all voices):
  Probabilities:  PatternEngine.octaveRandom[16]
  Length/Offset/Rotation: (same MonsoonIds as Mono)
```

### Spread Control

Add 3 spread knobs (one per lane, affects all voices):

```cpp
enum ParamId {
  SPREAD_REST_PARAM,
  SPREAD_MELODY_PARAM,
  SPREAD_OCTAVE_PARAM,
  NUM_PARAMS
};

// These spread parameters affect all 15 voices equally
```

---

## Poly Sands - Per-Voice Integration

### Expander Widget Setup (StraitsEastSands or StraitsWestSands)

```cpp
#include "managers/PolyVoiceSandsParameterManager.hpp"

struct StraitsEastSandsWidget : rack::ModuleWidget {
  StraitsEastSands* module;
  std::array<SandsVisualEditorV4*, 7> visualEditors;  // 7 voices
  PolyVoiceSandsParameterManager* paramManager;
  
  int selectedVoice = 0;
  
  StraitsEastSandsWidget(StraitsEastSands* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(...));
    
    // Create visual editors for 7 voices
    for (int v = 0; v < 7; ++v) {
      auto editor = new SandsVisualEditorV4(SandsVisualEditorV4::Mode::POLY);
      editor->box.pos = rack::Vec(10, 60);
      addChild(editor);
      visualEditors[v] = editor;
    }
    
    // Create parameter manager
    rack::Module* monsoonMod = module->rightExpander.module;
    auto* patternEngine = /* get from monsoonMod */;
    paramManager = new PolyVoiceSandsParameterManager(patternEngine, 7);  // 7 voices
  }
  
  void step() override {
    ModuleWidget::step();
    
    if (paramManager && module) {
      // Update spread values from parameters (per voice, per lane)
      for (int v = 0; v < 7; ++v) {
        for (int l = 0; l < 3; ++l) {
          int paramId = getSpreadParamId(v, l);
          float spread = module->params[paramId].getValue();
          paramManager->setSpread(v, l, spread);
        }
      }
      
      // Sync PatternEngine → Editor for selected voice
      paramManager->syncPatternEngineToEditor(selectedVoice, visualEditors[selectedVoice]->currentState);
      
      // Show selected voice editor, hide others
      for (int v = 0; v < 7; ++v) {
        visualEditors[v]->visible = (v == selectedVoice);
      }
    }
  }
  
  int getSpreadParamId(int voice, int lane) {
    // Map voice/lane to parameter ID
    // Implementation depends on how you organize params
    return SPREAD_BASE_PARAM + voice * 3 + lane;
  }
  
  ~StraitsEastSandsWidget() {
    delete paramManager;
  }
};
```

### Parameter Mapping

Per-voice DNA stored in PatternEngine arrays:

```
Voice 0 (Monsoon voice 2):
  Rest:    PatternEngine.polyRhythmRandom[0][16]
  Melody:  PatternEngine.polyMelodyRandom[0][16]
  Octave:  PatternEngine.polyOctaveRandom[0][16]

Voice 1 (Monsoon voice 3):
  Rest:    PatternEngine.polyRhythmRandom[1][16]
  Melody:  PatternEngine.polyMelodyRandom[1][16]
  Octave:  PatternEngine.polyOctaveRandom[1][16]

... (repeat for voices 2-6)

Length/Offset/Rotation: (existing MonsoonIds params)
```

### Spread Control

Add spread knob per voice per lane (21 parameters total for East):

```cpp
enum ParamId {
  SPREAD_V0_REST,    SPREAD_V0_MELODY,    SPREAD_V0_OCTAVE,
  SPREAD_V1_REST,    SPREAD_V1_MELODY,    SPREAD_V1_OCTAVE,
  // ... (for all 7 voices)
  NUM_PARAMS
};
```

---

## Visual Editor Display with Spread

The visual editor can display spread-adjusted probabilities:

```cpp
// In SandsVisualEditorV4::draw()

void drawStep(NVGcontext* vg, int lane, int step) {
  // Use display probability (accounts for spread)
  float displayProb = getDisplayProbability(lane, step);
  
  // Draw bar at this height
  float barHeight = displayProb * rect.size.y;
  nvgRect(vg, ..., barHeight);
}

// Add method to get display probability
float getDisplayProbability(int lane, int step) {
  // If paramManager is available:
  if (paramManager) {
    return paramManager->getDisplayProbability(lane, step);
  }
  // Otherwise use raw probability
  return probabilities[step];
}
```

---

## Testing Both Approaches

To test visual editor vs knob approach:

### Option 1: Dual Mode

Keep both visual editor and knobs, allow user to choose:

```cpp
bool useVisualEditor = true;  // or false

if (useVisualEditor) {
  // Read from visual editor
  paramManager->syncEditorToPatternEngine(...);
} else {
  // Read from knobs (existing logic)
  // PatternEngine already handles this
}
```

### Option 2: Blended Mode

Both work simultaneously:

```cpp
// Visual editor changes
paramManager->syncEditorToPatternEngine(...);

// Knobs still work (existing logic reads/writes params)
// Both update the same PatternEngine, no conflict
```

---

## Integration Checklist

### Mono Sands
- [ ] Create visual editor widget
- [ ] Create MonoSandsParameterManager
- [ ] Wire sync calls in step()
- [ ] Add spread knobs (optional)
- [ ] Test parameter syncing
- [ ] Verify length/offset/rotation still work

### Poly Sands - Global
- [ ] Create visual editor widget
- [ ] Create PolySandsParameterManager
- [ ] Wire sync calls in step()
- [ ] Add spread knobs (3, one per lane)
- [ ] Test all voices respond to global changes
- [ ] Verify spread affects all voices

### Poly Sands - Per-Voice
- [ ] Create 7-8 visual editor widgets (tabbed)
- [ ] Create PolyVoiceSandsParameterManager
- [ ] Implement voice tab switching
- [ ] Wire sync calls in step()
- [ ] Add spread knobs (21 total, per voice per lane)
- [ ] Test voice-specific editing
- [ ] Verify spread affects individual voices

---

## Existing Logic Not Touched

✅ Monsoon.cpp process() still handles length/offset/rotation indexing  
✅ PatternEngine still generates and caches sequences  
✅ Scramble/reset buttons still work  
✅ Scale manager still restricts notes  
✅ Accent strand still works  
✅ Legato gate still works  

The visual editor is purely an alternative UI for editing the same 16 probability values that already exist.
