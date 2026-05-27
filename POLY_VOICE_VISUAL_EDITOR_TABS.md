# Poly Voice Visual Editor - Tabbed UI Implementation

## Overview

The visual editor uses a **tabbed interface** to switch between individual poly voices while displaying a single visual editor widget.

Three widget implementations:
1. **StraitsEastSandsWidget** - 7 voice tabs (V2-V8)
2. **StraitsWestSandsWidget** - 8 voice tabs (V9-V16)
3. **MonsoonStraitSandsWidget** - No tabs (global mode, shows representative voice)

---

## Architecture

### TabButton Widget

Small button showing voice number (V2, V3, etc.):
- Selected state: Highlighted in teal
- Unselected state: Dark gray
- Click to switch voice

```cpp
struct TabButton : OpaqueWidget {
  std::string label = "V2";  // Voice label
  int voiceIdx = 0;          // Index in paramManager
  bool isSelected = false;   // Visual state
  
  std::function<void(int)> onPressed;  // Click handler
};
```

### TabButtonGroup

Container managing multiple tabs:
- Creates 7 or 8 TabButton widgets
- Tracks selected tab
- Provides `selectTab(idx)` and `getSelectedTab()`

```cpp
struct TabButtonGroup : Widget {
  std::vector<TabButton*> tabs;
  int selectedIdx = 0;
  
  void selectTab(int idx);        // Select and highlight
  int getSelectedTab() const;     // Get current selection
};
```

### Widget Data Flow

```
User clicks V4 tab
  ↓
TabButtonGroup highlights V4
  ↓
StraitsEastSandsWidget::step() detects change
  ↓
onVoiceTabChanged(3)  [voice index 3]
  ↓
SAVE: syncEditorToPatternEngine(2, editorState)
  ↓
selectedVoice = 3
  ↓
LOAD: syncPatternEngineToEditor(3, editorState)
  ↓
Visual editor now shows V4 probabilities
  ↓
User edits visual editor
  ↓
next step(): syncPatternEngineToEditor(3, editorState)
  ↓
Visual editor updates with new interpolated values
```

---

## Straits East Implementation (7 Voices)

### Layout

```
┌────────────────────────────────────┐
│ [V2] [V3] [V4] [V5] [V6] [V7] [V8]│  ← Voice tabs (40px tall)
├────────────────────────────────────┤
│                                    │
│     VISUAL EDITOR (3 lanes)        │  ← 150px tall
│     Shows V4's probabilities       │
│                                    │
├────────────────────────────────────┤
│  [REST ----◼----]                 │
│ [MELODY ---◼-----]   ← Spread      │  ← Per-voice spreads
│  [OCTAVE ---◼----]                 │     (3 sliders)
│                                    │
│  AVERAGE_POLY / MONO_DRAW  [◼]    │  ← Target selector
└────────────────────────────────────┘
```

### Data Structure

```cpp
struct StraitsEastSands : Module {
  enum ParamIds {
    // 7 voices × 3 lanes = 21 spread params
    SPREAD_V0_R, SPREAD_V0_M, SPREAD_V0_O,  // Voice 2
    SPREAD_V1_R, SPREAD_V1_M, SPREAD_V1_O,  // Voice 3
    ...
    SPREAD_V6_R, SPREAD_V6_M, SPREAD_V6_O,  // Voice 8
    
    INTERP_TARGET,
    NUM_PARAMS
  };
};
```

### Tab to Voice Mapping

| Tab | Label | Voice | Index | polyRandom[index] |
|-----|-------|-------|-------|-------------------|
| 0   | V2    | 2     | 0     | polyRandom[0]     |
| 1   | V3    | 3     | 1     | polyRandom[1]     |
| 2   | V4    | 4     | 2     | polyRandom[2]     |
| 3   | V5    | 5     | 3     | polyRandom[3]     |
| 4   | V6    | 6     | 4     | polyRandom[4]     |
| 5   | V7    | 7     | 5     | polyRandom[5]     |
| 6   | V8    | 8     | 6     | polyRandom[6]     |

### Voice Switching Example

```cpp
void StraitsEastSandsWidget::onVoiceTabChanged(int newVoiceIdx) {
  // User clicked V5 (index 3)
  
  // 1. Save V4 (previous) edits
  paramMgr->syncEditorToPatternEngine(
    2,                          // Previous: index 2 = V4
    visualEditor->currentState
  );
  
  // 2. Update selection
  selectedVoice = 3;
  
  // 3. Load V5 probabilities
  paramMgr->syncPatternEngineToEditor(
    3,                          // New: index 3 = V5
    visualEditor->currentState
  );
  
  // 4. Update spread knobs to V5's values
  for (int l = 0; l < 3; ++l) {
    int paramId = SPREAD_V0_R + 3*3 + l;  // V5's spreads
    float spread = paramMgr->getSpread(3, l);
    module->params[paramId].setValue(spread);
  }
}
```

---

## Straits West Implementation (8 Voices)

### Identical to East, but:

```cpp
// 8 voices × 3 lanes = 24 spread params
SPREAD_V0_R through SPREAD_V7_O

// TabButtonGroup created with 8 tabs starting at V9
tabGroup = new TabButtonGroup(8, 9);  // 8 tabs, start label V9

// Data spans voices 9-16
```

| Tab | Label | Voice | Index |
|-----|-------|-------|-------|
| 0   | V9    | 9     | 0     |
| 1   | V10   | 10    | 1     |
| ... | ...   | ...   | ...   |
| 7   | V16   | 16    | 7     |

---

## Macro Poly (Global) Implementation

### Layout

```
┌────────────────────────────────────┐
│   Monsoon Strait Sands (Macro)     │
├────────────────────────────────────┤
│                                    │
│     VISUAL EDITOR (3 lanes)        │  ← Representative voice
│     Shows voice 2's view           │     (but all voices use same spread)
│                                    │
├────────────────────────────────────┤
│  [REST ----◼----]                 │
│ [MELODY ---◼-----]   ← Spread      │  ← Same spread applies to all
│  [OCTAVE ---◼----]                 │     15 voices (2-16)
│                                    │
│  AVERAGE_POLY / MONO_DRAW  [◼]    │  ← Target for all voices
└────────────────────────────────────┘
```

### Data Structure

```cpp
struct MonsoonStraitSands : Module {
  enum ParamIds {
    // 3 spread params (global)
    SPREAD_REST,     // Applied to all voices
    SPREAD_MELODY,   // Applied to all voices
    SPREAD_OCTAVE,   // Applied to all voices
    
    INTERP_TARGET,
    NUM_PARAMS
  };
};
```

### Global Spread Behavior

```
All 15 voices (2-16) interpolate using same spread:

User sets SPREAD_REST = 0.5

Voice 2: REST = 0.2 → interpolated = 0.2 + (avg - 0.2) * 0.5
Voice 3: REST = 0.8 → interpolated = 0.8 + (avg - 0.8) * 0.5
Voice 4: REST = 0.6 → interpolated = 0.6 + (avg - 0.6) * 0.5
...
Voice 16: REST = X → interpolated = X + (avg - X) * 0.5

All interpolate toward SAME average
All use SAME spread value
Effect: Global cohesion control
```

---

## Per-Voice vs Global Spread

### Per-Voice (East/West)

```
7 or 8 spread knobs visible (one per lane for selected voice)
Switch voices → knobs update to that voice's spreads
Each voice has independent spread values
V2 REST spread ≠ V3 REST spread
V2 MELODY spread ≠ V3 MELODY spread
(Can tune cohesion independently per voice)
```

### Global (Macro)

```
3 spread knobs visible (one per lane)
Same knobs apply to ALL 15 voices (2-16)
V2 REST spread = V3 REST spread = ... = V16 REST spread
(All voices use same cohesion level)
```

---

## Voice Switching in step()

### StraitsEastSandsWidget::step()

```cpp
void step() override {
  if (!module || !paramMgr) return;
  
  // 1. Check if tab changed
  int newSelected = tabGroup->getSelectedTab();
  if (newSelected != selectedVoice) {
    onVoiceTabChanged(newSelected);
  }
  
  // 2. Update spreads from knobs
  for (int v = 0; v < 7; ++v) {
    for (int l = 0; l < 3; ++l) {
      int paramId = SPREAD_V0_R + v*3 + l;
      float spread = module->params[paramId].getValue();
      paramMgr->setSpread(v, l, spread);  // ← Sets in SpreadManager
    }
  }
  
  // 3. Update target
  bool useMono = module->params[INTERP_TARGET].getValue() > 0.5f;
  auto target = useMono ? 
    SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY;
  paramMgr->setInterpolationTarget(target);
  
  // 4. Sync to visual editor
  paramMgr->syncPatternEngineToEditor(selectedVoice,  // ← Selected voice
                                      visualEditor->currentState);
  
  // Visual editor now shows interpolated values
  // with spread applied toward target
}
```

---

## Visual Feedback

### At spread=0 (Original)

```
Probabilities vary (original poly draws)

V2 REST: ▂ ▆ ▆ ▄ ▉ ▂ ▆  (original variance)
V2 MELODY: ▁ ▃ ▂ ▁ ▃ ▁ ▄  (original variance)
```

### At spread=0.5 (Halfway)

```
Probabilities move toward target

V2 REST: ▃ ▇ ▇ ▅ ▉ ▃ ▇  (interpolated halfway)
V2 MELODY: ▂ ▄ ▃ ▂ ▄ ▂ ▅  (interpolated halfway)
```

### At spread=1.0 (Target)

```
Probabilities all equal (or match target reference)

With AVERAGE_POLY:
V2 REST: ▄ ▄ ▄ ▄ ▄ ▄ ▄  (all equal to average)

With MONO_DRAW:
V2 REST: ▅ ▅ ▅ ▅ ▅ ▅ ▅  (all equal to mono)
```

---

## Keyboard Shortcuts (Optional Enhancement)

```cpp
void onKey(const rack::event::Key& e) override {
  if (e.action == GLFW_PRESS) {
    // Left/Right arrows: Previous/Next voice
    if (e.key == GLFW_KEY_LEFT && selectedVoice > 0) {
      onVoiceTabChanged(selectedVoice - 1);
      e.consume(this);
      return;
    }
    
    if (e.key == GLFW_KEY_RIGHT && selectedVoice < 6) {
      onVoiceTabChanged(selectedVoice + 1);
      e.consume(this);
      return;
    }
    
    // Number keys: 1-7 for voices V2-V8
    if (e.key >= GLFW_KEY_1 && e.key <= GLFW_KEY_7) {
      int voiceIdx = e.key - GLFW_KEY_1;
      onVoiceTabChanged(voiceIdx);
      e.consume(this);
      return;
    }
  }
}
```

Users can press:
- `1` → Switch to V2
- `2` → Switch to V3
- `←` / `→` → Previous/Next voice

---

## Integration Checklist

- [ ] TabButton.hpp created
- [ ] TabButtonGroup implemented
- [ ] StraitsEastSandsWidget created (7 voices)
- [ ] StraitsWestSandsWidget created (8 voices)
- [ ] MonsoonStraitSandsWidget created (macro, no tabs)
- [ ] Voice tabs wired to onVoiceTabChanged()
- [ ] Spread knobs update on voice switch
- [ ] Visual editor shows selected voice
- [ ] Interpolation target selector works
- [ ] Active voice averaging works (via SequencerEngine)
- [ ] Per-voice spreads save/load correctly
- [ ] Keyboard shortcuts (optional)
- [ ] SVG panels created
- [ ] Models registered in plugin.cpp

---

## Testing Strategy

### Tab Switching Test

1. Click V3 tab
   - ✅ V2 probabilities saved to PatternEngine
   - ✅ V3 probabilities loaded to visual editor
   - ✅ Spread knobs update to V3's values
   - ✅ Visual editor shows V3 (different bars)

2. Click V5 tab
   - ✅ V3 edits saved
   - ✅ V5 loaded
   - ✅ Knobs update again

3. Click V3 again
   - ✅ Loads previously saved V3 state
   - ✅ Shows changes from step 1

### Spread Test

1. Select V2, set REST spread = 0.5
   - ✅ Only V2's REST interpolates
   - ✅ Visual editor shows interpolated values

2. Select V3, set REST spread = 0.0
   - ✅ V3's REST shows original (no interpolation)
   - ✅ V2 still at 0.5 (independent)

### Polyphony Test

1. Set polyphony = 4 voices (2-5)
   - ✅ Average uses only 4 voices
   - ✅ Spread interpolates toward 4-voice average

2. Change polyphony = 7 (2-8)
   - ✅ Average recalculates with all 7
   - ✅ Visual editor values shift
   - ✅ No clicks/pops

### Macro Poly Test

1. MonsoonStraitSands: Set SPREAD_REST = 0.5
   - ✅ ALL 15 voices interpolate
   - ✅ Visual shows voice 2 representative
   - ✅ Each voice interpolates per its own draw

2. Switch INTERP_TARGET to MONO_DRAW
   - ✅ All voices interpolate toward mono
   - ✅ Values change in visual editor
