# Mono vs Poly Sands - Configuration Guide

## Overview

The visual DNA editor now supports two distinct configurations:

### **Mono Sands** (MonsoonSandsExpander - 12 HP)
```
Single voice (Voice 1) with 6 lanes:
├─ REST       (probability of rest)
├─ MELODY     (pitch selection)
├─ OCTAVE     (octave variation)
├─ LEGATO     (note legato on/off)
├─ ACCENT     (accent on/off)
└─ VARIATION  (variation/gate pattern)

Configuration: SandsVisualEditorV3(Mode::MONO)
```

### **Poly Sands** (Straits East/West - 24 HP)
```
Multiple voices (7 for East, 8 for West) with 3 lanes per-voice:
Each Voice:
├─ REST       (probability of rest, per-voice)
├─ MELODY     (pitch selection, per-voice)
└─ OCTAVE     (octave variation, per-voice)

Global (not per-voice):
├─ LEGATO     (controlled globally, affects all voices)
├─ ACCENT     (controlled globally, affects all voices)
└─ VARIATION  (controlled globally, affects all voices)

Configuration: SandsVisualEditorV3(Mode::POLY)
```

---

## Architecture

### Mono Sands

```
SandsVisualEditorV3(Mode::MONO)
         ↓
   6 lanes, single voice
         ↓
   MonsoonSandsExpander
         ↓
   Monsoon (Voice 1)
```

**Storage:**
- 6 lanes × 16 steps = 96 probability values
- Per-lane: start, end, rotation offsets
- Total: ~180 parameters (or compressed into DNA structure)

**DNA Structure Example:**
```cpp
enum ParamId {
  // Rest lane
  SANDS_REST_LEN,
  SANDS_REST_OFF,
  SANDS_REST_ROT,
  
  // Melody lane
  SANDS_MELODY_LEN,
  SANDS_MELODY_OFF,
  SANDS_MELODY_ROT,
  
  // Octave, Legato, Accent, Variation (same pattern)
  // ...
};
```

### Poly Sands (East)

```
SandsVisualEditorV3(Mode::POLY) × 7 voices
         ↓
   3 lanes per-voice, 7 tabs
         ↓
   StraitsEastSands (Voices 2-8)
         ↓
   Monsoon (Voice 1, with Voice 2-8 poly)
```

**Storage per-voice:**
- 3 lanes × 16 steps = 48 probability values per voice
- Per-lane: start, end, rotation offsets
- Total per voice: ~60 parameters
- Total for 7 voices: ~420 parameters (or compressed)

**Global storage (separate):**
- LEGATO: 16 steps × 1 global value
- ACCENT: 16 steps × 1 global value
- VARIATION: 16 steps × 1 global value
- Per-lane: start, end, rotation for each
- Total: ~90 additional parameters

---

## Lane Colors & Meanings

### Shared Lanes (Mono & Poly)

| Lane | Color | Meaning |
|------|-------|---------|
| REST | Gray (#505050) | Probability of silence/rest |
| MELODY | Gold (#d4af37) | Pitch/note selection (0-11 semitones) |
| OCTAVE | Dark Gold (#b8860b) | Octave offset (-4 to +4) |

### Mono-Only Lanes

| Lane | Color | Meaning |
|------|-------|---------|
| LEGATO | Teal (#26a69a) | Legato on/off (0=off, 1=on) |
| ACCENT | Orange (#ff9500) | Accent on/off or velocity |
| VARIATION | Red (#ff6b6b) | Variation/alt pattern mode |

**Note:** In Poly mode, LEGATO, ACCENT, and VARIATION are controlled separately (globally), not per-voice in the visual editor.

---

## Usage: Mode Selection

### Mono Sands (Simple)

```cpp
// In MonsoonSandsExpander widget
struct MonsoonSandsExpanderWidget : rack::ModuleWidget {
  MonsoonSandsExpander* module;
  redDot::SandsVisualEditorV3* editor;
  
  MonsoonSandsExpanderWidget(MonsoonSandsExpander* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(...));
    
    // Create editor in MONO mode
    editor = new redDot::SandsVisualEditorV3(redDot::SandsVisualEditorV3::Mode::MONO);
    editor->box.pos = rack::Vec(10, 60);
    addChild(editor);
  }
};
```

### Poly Sands (Tabbed)

```cpp
// In StraitsVisualEditorPanel
struct StraitsVisualEditorPanel : rack::Widget {
  std::array<redDot::SandsVisualEditorV3*, NUM_VOICES> editors;
  
  StraitsVisualEditorPanel(rack::Module* m) {
    // Create 7 (or 8) editors in POLY mode
    for (int v = 0; v < NUM_VOICES; ++v) {
      auto editor = new redDot::SandsVisualEditorV3(redDot::SandsVisualEditorV3::Mode::POLY);
      editor->box.pos = rack::Vec(10, 40);
      addChild(editor);
      editors[v] = editor;
    }
  }
};
```

---

## Parameter Mapping

### Mono Sands Parameter Manager

```cpp
struct MonoSandsParameterManager : SandsParameterManager {
  // Map mono sands lanes to parameters
  int getStepParamId(int lane, int voice, int step) {
    // voice = 0 (always, mono has only 1 voice)
    // lane = 0-5 (REST, MELODY, OCTAVE, LEGATO, ACCENT, VARIATION)
    
    int laneOffsets[] = {0, 16, 32, 48, 64, 80};  // Each lane is 16 params
    return SANDS_REST_PARAM_BASE + laneOffsets[lane] + step;
  }
  
  int getRotationParamId(int lane, int voice) {
    // Rotation stored separately (per-lane)
    int laneRotOffsets[] = {0, 1, 2, 3, 4, 5};
    return SANDS_ROTATION_BASE + laneRotOffsets[lane];
  }
};
```

### Poly Sands Parameter Manager

```cpp
struct PolySandsParameterManager : SandsParameterManager {
  // Map poly sands lanes to parameters
  int getStepParamId(int lane, int voice, int step) {
    // voice = 0-14 (editor voices, mapped to Monsoon 2-16)
    // lane = 0-2 only (REST, MELODY, OCTAVE - no LEGATO/ACCENT/VARIATION)
    
    if (lane >= 3) return -1;  // Only 3 lanes in POLY mode
    
    int laneOffsets[] = {0, 16, 32};
    int voiceOffset = voice * 3 * 16;  // 3 lanes, 16 params each, per voice
    return POLY_DNA_BASE + voiceOffset + laneOffsets[lane] + step;
  }
  
  int getRotationParamId(int lane, int voice) {
    if (lane >= 3) return -1;
    
    int voiceOffset = voice * 3;
    return POLY_ROTATION_BASE + voiceOffset + lane;
  }
};
```

---

## State Structure

### VoiceState (unified for both modes)

```cpp
struct VoiceState {
  // Values: [step][lane]
  // Mono: uses all 6 lanes
  // Poly: uses only lanes 0-2 (REST, MELODY, OCTAVE)
  std::array<std::array<float, 6>, 16> values;
  
  // Per-lane parameters (all 6 lanes, even if not used in POLY mode)
  std::array<int, 6> startStep;
  std::array<int, 6> endStep;
  std::array<int, 6> rotation;
};
```

**Benefits:**
- Same data structure for both modes
- Easy to copy/convert between modes
- Flexible for future expansion

---

## Control Bar Display

### Mono Mode
```
MONO    U:5 R:2    Lane:LEGATO    Preset:Even
```

### Poly Mode
```
POLY    U:5 R:2    Lane:MELODY    Preset:Euclidean
```

**Note:** Shows mode, undo/redo counts, selected lane, and preset.

---

## Keyboard Shortcuts (Both Modes)

| Shortcut | Action |
|----------|--------|
| `1-6` | Select lane (or 1-3 in POLY mode) |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `R` | Randomize selected lane |
| `Shift+R` | Randomize all lanes |
| `P` | Toggle preset panel |
| `Ctrl+C` | Copy selected lane |
| `Ctrl+V` | Paste to selected lane |
| `Shift+V` | Reverse selected lane |

---

## Lane Count Auto-Adjustment

The editor automatically adjusts to the mode:

```cpp
SandsVisualEditorV3::setMode(Mode m) {
  mode = m;
  laneCount = (mode == MONO) ? 6 : 3;
  
  // Adjust box height
  box.size.y = 35.f + (laneCount * 30.f) + 40.f;
  // MONO: 35 + 180 + 40 = 255px
  // POLY: 35 + 90 + 40 = 165px
}
```

---

## Global Controls (Poly Only)

In Poly mode, LEGATO/ACCENT/VARIATION are controlled separately.

**Option 1: Separate Panel**
```
┌─────────────────────────┐
│   GLOBAL CONTROLS       │
├─────────────────────────┤
│ LEGATO    ▃ ▁ ▅ ▃ ▁...  │
│ ACCENT    █ ░ █ ░...   │
│ VARIATION ◆ ◇ ◆ ◇...   │
└─────────────────────────┘
```

**Option 2: UI Sliders/Buttons**
```
[LEGATO]  [ACCENT]  [VARIATION]
├─ Value  ├─ Value  ├─ Value
└─ Range  └─ Range  └─ Range
```

**Option 3: Embedded in Monsoon**
- If Monsoon has space, add 3 mini-editors
- Otherwise, require separate module

---

## Serialization

### Mono Sands

```json
{
  "mode": "MONO",
  "voice": {
    "values": [0.5, 0.3, 0.7, ...],  // 96 values
    "startSteps": [0, 0, 0, 0, 0, 0],
    "endSteps": [16, 16, 16, 16, 16, 16],
    "rotations": [0, 2, 1, 0, 3, 2]
  }
}
```

### Poly Sands (7 voices)

```json
{
  "mode": "POLY",
  "selectedVoice": 2,
  "voices": [
    {
      "values": [0.5, 0.3, 0.7, ...],  // 48 values (3 lanes)
      "startSteps": [0, 0, 0, 0, 0, 0],
      "endSteps": [16, 16, 16, 16, 16, 16],
      "rotations": [0, 2, 1, 0, 0, 0]
    },
    // ... 6 more voices
  ],
  "global": {
    "legato": {...},     // 16 steps, global
    "accent": {...},     // 16 steps, global
    "variation": {...}   // 16 steps, global
  }
}
```

---

## Integration Checklist

### For Mono Sands

- [ ] Create `SandsVisualEditorV3(Mode::MONO)` in widget
- [ ] Implement `MonoSandsParameterManager`
- [ ] Map all 6 lanes to Monsoon parameters
- [ ] Wire sync in widget step()
- [ ] Create SVG panel (narrow, ~12-14 HP)
- [ ] Test all 6 lanes

### For Poly Sands (East/West)

- [ ] Create `SandsVisualEditorV3(Mode::POLY)` × 7 (or 8) in panel
- [ ] Implement `PolySandsParameterManager`
- [ ] Map 3 lanes × N voices
- [ ] Implement separate global control UI (LEGATO, ACCENT, VARIATION)
- [ ] Wire sync for per-voice + global parameters
- [ ] Create SVG panels (24 HP each, East/West)
- [ ] Test all 7 (or 8) voices
- [ ] Test global controls

---

## Color Reference

| Lane | Mono | Poly | Color | Hex |
|------|------|------|-------|-----|
| REST | ✓ | ✓ | Gray | #505050 |
| MELODY | ✓ | ✓ | Gold | #d4af37 |
| OCTAVE | ✓ | ✓ | Dark Gold | #b8860b |
| LEGATO | ✓ | ✗ | Teal | #26a69a |
| ACCENT | ✓ | ✗ | Orange | #ff9500 |
| VARIATION | ✓ | ✗ | Red | #ff6b6b |

---

## Example: Quick Integration

### Mono (MonsoonSandsExpander)

```cpp
#include "ui/SandsVisualEditorV3.hpp"

struct MonsoonSandsExpanderWidget : rack::ModuleWidget {
  redDot::SandsVisualEditorV3* editor;
  
  MonsoonSandsExpanderWidget(MonsoonSandsExpander* mod) {
    setModule(mod);
    setPanel(...);
    
    // Create mono editor
    editor = new redDot::SandsVisualEditorV3(
      redDot::SandsVisualEditorV3::Mode::MONO
    );
    editor->box.pos = rack::Vec(5, 50);
    addChild(editor);
  }
  
  void step() override {
    ModuleWidget::step();
    if (editor && module) {
      // Sync editor ↔ module parameters
      syncEditorToModule(editor, module);
      editor->setCurrentPlayStep(module->getCurrentStep());
    }
  }
};
```

### Poly (StraitsEastSands)

```cpp
struct StraitsEastSandsPanel : StraitsVisualEditorPanel {
  StraitsEastSandsPanel(rack::Module* m) {
    // Create 7 poly editors
    for (int v = 0; v < 7; ++v) {
      auto editor = new redDot::SandsVisualEditorV3(
        redDot::SandsVisualEditorV3::Mode::POLY
      );
      editors[v] = editor;
      addChild(editor);
    }
  }
};
```

---

## Summary

| Aspect | Mono | Poly |
|--------|------|------|
| **Lanes** | 6 | 3 (+ global) |
| **Voices** | 1 | 7-8 |
| **UI Size** | Compact (12-14 HP) | 24 HP (with tabs) |
| **Complexity** | Medium | High |
| **Per-voice control** | All 6 lanes | Only 3 lanes |
| **Global control** | N/A | LEGATO, ACCENT, VARIATION |

