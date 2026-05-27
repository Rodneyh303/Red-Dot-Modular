# Visual DNA Editor - Implementation Guide

## Code Structure

### Files

```
src/ui/
├── SandsVisualEditor.hpp          # Core visual editor widget
└── StraitsEastVisualWidget.hpp    # Example implementation (Straits East)
```

### Components

1. **SandsVisualEditor** - Standalone visual sequencer widget
   - Draws 5 probability lanes (Rest, Melody, Octave, Legato, Variation)
   - Handles 16-step bar chart interaction
   - Manages length handles and rotation blocks
   - Independent of module implementation

2. **StraitsEastVisualWidget** - Wrapper for module integration
   - Tab system (7 tabs for voices 2-8)
   - Syncs data to/from Monsoon parameters
   - Ready for extension to Straits West

---

## Usage

### Basic Integration

```cpp
// In StraitsEastSands.hpp (widget definition)
struct StraitsEastSandsWidget : rack::ModuleWidget {
  StraitsEastSands* module;
  StraitsEastVisualWidget* visualEditor;
  
  StraitsEastSandsWidget(StraitsEastSands* module) {
    setModule(module);
    
    // Create visual editor
    visualEditor = new StraitsEastVisualWidget();
    visualEditor->box.pos = rack::Vec(10, 50);
    addChild(visualEditor);
  }
  
  void step() override {
    // Sync data bidirectionally
    if (visualEditor) {
      visualEditor->syncFromModule(module);
      visualEditor->syncToModule(module);
    }
    ModuleWidget::step();
  }
};
```

### Parameter Mapping

Each DNA lane needs:
- **16 step values** (Monsoon.hpp: `POLY_DNA_VOICE_X_LEN`, `_OFF`, `_ROT`, etc.)
- **Start offset** (not yet in Monsoon, would need to add)
- **End offset** (not yet in Monsoon, would need to add)
- **Rotation** (already exists in Monsoon.hpp)

Current Monsoon parameter structure:
```cpp
POLY_DNA_VOICE_1_LEN,      // Step values (0-15)
POLY_DNA_VOICE_1_OFF,      // Offset rotation (0-63)
POLY_DNA_VOICE_1_ROT,      // Rotation amount (0-63)

POLY_MELODY_VOICE_1_LEN,   // Same for melody
POLY_MELODY_VOICE_1_OFF,
POLY_MELODY_VOICE_1_ROT,

// etc. for Octave, Legato
```

---

## Integration Steps

### Step 1: Add Length Range Parameters

If not already present, add start/end step parameters:

```cpp
// In Monsoon.hpp (plugin.hpp)
enum ParamId {
  // Existing DNA params...
  
  // New: Length range parameters (optional, alternative to start/end)
  POLY_DNA_VOICE_1_START,    // Start step (0-15)
  POLY_DNA_VOICE_1_LENGTH,   // Length (1-16)
  
  // ... etc for other lanes and voices
};
```

Or use existing offset parameters if they work for your use case.

### Step 2: Sync Data from Module to Editor

```cpp
void StraitsEastVisualWidget::syncFromModule(StraitsEastSands* module) {
  if (!module) return;
  
  int voice = selectedVoice + 2;  // Voices 2-8
  auto editor = editors[selectedVoice];
  
  // Sync Rest lane
  for (int step = 0; step < 16; ++step) {
    int paramId = POLY_DNA_VOICE_1_LEN + (voice - 1) * 3 + step;
    editor->voiceData.values[step][SandsVisualEditor::REST] = 
      module->params[paramId].getValue();
  }
  
  // Get start/end from offset and length parameters
  // (Depends on your parameter structure)
  editor->voiceData.startStep[SandsVisualEditor::REST] = 0;
  editor->voiceData.endStep[SandsVisualEditor::REST] = 16;
  
  // Sync rotation
  int rotParamId = POLY_DNA_VOICE_1_ROT + (voice - 1) * 3;
  editor->voiceData.rotation[SandsVisualEditor::REST] = 
    (int)module->params[rotParamId].getValue();
  
  // Repeat for Melody, Octave, Legato, Variation lanes
}
```

### Step 3: Sync Data from Editor to Module

```cpp
void StraitsEastVisualWidget::syncToModule(StraitsEastSands* module) {
  if (!module) return;
  
  int voice = selectedVoice + 2;  // Voices 2-8
  auto editor = editors[selectedVoice];
  
  // Only update if editor has focus (to avoid constant updates)
  if (!editor->dragState.isDragging) return;
  
  // Write Rest lane
  for (int step = 0; step < 16; ++step) {
    int paramId = POLY_DNA_VOICE_1_LEN + (voice - 1) * 3 + step;
    module->params[paramId].setValue(
      editor->voiceData.values[step][SandsVisualEditor::REST]
    );
  }
  
  // Write rotation
  int rotParamId = POLY_DNA_VOICE_1_ROT + (voice - 1) * 3;
  module->params[rotParamId].setValue(
    editor->voiceData.rotation[SandsVisualEditor::REST]
  );
}
```

---

## Key Design Points

### 1. Rotation Blocks

Rotation is **per-lane**, not per-voice:
- Each lane (Rest, Melody, Octave, Legato, Variation) has independent rotation
- Rotation value: 0-15 steps offset from start
- Visual: Colored teal block at position `(start + rotation) % 16`

**Interaction:**
- Click rotation block: cycles to next rotation value
- Drag rotation block left/right: adjust rotation offset
- Rotation wraps at lane length boundary

### 2. Length Handles

- Left handle: Start step (0-15)
- Right handle: End step (1-16, must be > start)
- Bars outside range are dimmed (opacity 0.3)
- Rotation block must be within range to display

### 3. Probability Bars

- Height represents value 0-100%
- Click to set value precisely
- Drag to paint pattern across multiple steps
- Only editable within length range

---

## Real-Time Playback Sync

To show active step during playback:

```cpp
// In StraitsEastSandsWidget::step()
void step() override {
  ModuleWidget::step();
  
  if (module) {
    // Get current playing step from Monsoon
    int currentStep = module->getCurrentStep();
    
    // Update visual indicator
    visualEditor->editors[selectedVoice]->currentPlayStep = currentStep;
  }
}
```

Then in `SandsVisualEditor::draw()`:
```cpp
void draw(const rack::DrawArgs& args) override {
  // ... existing drawing code ...
  
  // Draw active step indicator (vertical line)
  if (currentPlayStep >= 0) {
    float x = layout.getStepCenterX(currentPlayStep);
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, x, layout.topPadding);
    nvgLineTo(args.vg, x, box.size.y);
    nvgStrokeColor(args.vg, rack::color::hex("#cc2222"));
    nvgStrokeWidth(args.vg, 2.f);
    nvgStroke(args.vg);
  }
}
```

---

## Performance Optimization

### Dirty Flag System

Only redraw changed data:

```cpp
struct SandsVisualEditor {
  std::array<bool, LANE_COUNT> laneDirty;  // Mark dirty lanes
  
  void setBarValue(int lane, int step, float value) {
    if (voiceData.values[step][lane] != value) {
      voiceData.values[step][lane] = value;
      laneDirty[lane] = true;  // Mark as dirty
    }
  }
  
  void draw(const rack::DrawArgs& args) override {
    // Background and static elements
    // ...
    
    // Only redraw dirty lanes
    for (int lane = 0; lane < LANE_COUNT; ++lane) {
      if (laneDirty[lane] || needsFullRedraw) {
        drawLaneBars(args.vg, lane);
        laneDirty[lane] = false;
      } else {
        // Draw cached version
        drawLaneCached(args.vg, lane);
      }
    }
  }
};
```

### Batch Parameter Updates

Group edits before flushing to parameters:

```cpp
void onMouseUp(const rack::event::MouseUp& e) override {
  // ... end drag operations ...
  
  // Batch update parameters
  if (dragState.isDragging) {
    flushParameterUpdates();  // Single update instead of continuous
  }
}
```

---

## Extension Ideas

### Phase 2 Features

- [ ] Keyboard shortcuts (arrow keys to navigate steps)
- [ ] Numeric input (click value to type directly)
- [ ] Undo/redo (param history)
- [ ] Copy/paste between lanes
- [ ] Randomize buttons (per-lane + all)
- [ ] Preset seeds (load pattern presets)

### Phase 3 Features

- [ ] Drag-and-drop between voices
- [ ] Pattern library (save/load presets)
- [ ] Humanization slider (add randomness)
- [ ] Reverse pattern
- [ ] Mirror pattern (left-right flip)

### Phase 4 Features

- [ ] Arpeggiator mode (step increment controls)
- [ ] Euclidean rhythm generator
- [ ] Markov chain probability
- [ ] Pattern morphing (interpolate between patterns)

---

## Testing Checklist

- [ ] Visual editor displays all 5 lanes
- [ ] Probability bars update on click
- [ ] Dragging paints pattern smoothly
- [ ] Length handles drag independently
- [ ] Rotation blocks appear at correct position
- [ ] Rotation blocks drag to new position
- [ ] Data syncs to module on drag end
- [ ] Data syncs from module on load
- [ ] Bars dim outside length range
- [ ] Active step indicator animates with playback
- [ ] Tab system switches voices correctly
- [ ] No performance degradation with multiple editors

---

## Code References

### VCV Rack Drawing API
- `nvgBeginPath()` / `nvgRect()` / `nvgFill()`
- `nvgStrokeColor()` / `nvgStroke()`
- `nvgCircle()` for handles
- `nvgText()` for labels

### Event Handling
- `onButton()` - Mouse click/release
- `onDragMove()` - Dragging
- `onHover()` - Mouse over (optional)
- `onMouseScroll()` - Scroll wheel (optional)

### Color Management
```cpp
NVGcolor color = rack::color::hex("#26a69a");
color.a = 0.5f;  // Adjust alpha
color = color.brighter();  // Built-in adjustment
color = color.darker();
```

---

## Next Steps

1. **Integrate with StraitsEastSands module**
   - Add visual editor widget to module definition
   - Implement sync functions

2. **Test with Monsoon in VCV Rack**
   - Verify data flow
   - Check animation smoothness
   - Measure performance

3. **Add Polish (Phase 2)**
   - Keyboard shortcuts
   - Preset system
   - Visual refinements

4. **Extend to Straits West**
   - Adapt for 8 voices (9-16)
   - Share code with East variant

---

## Questions?

See `SANDS_VISUAL_EDITOR_DESIGN.md` for design rationale and UX decisions.

