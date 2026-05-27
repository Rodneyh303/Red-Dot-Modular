# Sands Visual Editor Design

A visual sequencer widget for interactive manipulation of DNA data across 16-step patterns.

---

## Overview

**Goal:** Replace knob-based parameter editing with an intuitive visual interface for:
- Rest (probability of silence)
- Melody (pitch selection)
- Octave (range)
- Legato (sustain)
- Variation (randomization intensity)
- Rotation (pattern phase offset)

**Inspired by:**
- Midfield Patch Master (horizontal lane editing)
- Voxglitch Arpseq (step grid + probability)
- Mutable Instruments Marbles (density visualization)

---

## Visual Layout

### Per-Voice Editor

```
┌─────────────────────────────────────────────────────────────────────┐
│ VOICE 1                              [◄─ START   END ─►]            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│ REST       ▁ ▃ ▁ ▂ ▅ ▁ ▂ ▃ ▁ ▂ ▃ ▁ ▂ ▃ ▁ ▂                          │
│ MELODY    ▃ ▁ ▅ ▃ ▁ ▂ ▃ ▁ ▂ ▃ ▁ ▂ ▃ ▁ ▂ ▃                          │
│ OCTAVE    ▂ ▃ ▁ ▂ ▃ ▁ ▂ ▃ ▁ ▂ ▃ ▁ ▂ ▃ ▁ ▂                          │
│ LEGATO    █ ░ █ ░ █ ░ █ ░ █ ░ █ ░ █ ░ █ ░                          │
│ VARIATION ◆ ◇ ◆ ◇ ◆ ◇ ◆ ◇ ◆ ◇ ◆ ◇ ◆ ◇ ◆ ◇                          │
│                                                                       │
│ ROTATION  [◄─────────────●─────────────────────────────►] (32/64)   │
│                                                                       │
│  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16   [R] [?] [All]    │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘
```

### Components

**Probability Bars:**
- Height represents value (0-100%)
- One bar per step per lane
- 5 lanes total (Rest, Melody, Octave, Legato, Variation)
- Visual distinction between lanes (different colors/patterns)

**Length Handles:**
- Left handle: Start step (offset)
- Right handle: End step (length)
- Independent dragging
- Steps outside range are dimmed

**Rotation Slider:**
- Horizontal slider at bottom of voice
- Range: 0-63 steps
- Numeric display (e.g., "32/64")
- Real-time visual rotation effect
- Mouse wheel support (±1 or ±8 steps with Shift)

**Control Buttons:**
- `[R]` = Randomize this voice
- `[?]` = Load random seed preset
- `[All]` = Randomize all voices

---

## Interaction Model

### Editing Probability Bars

**Click on bar:**
- Sets value to height clicked (0-100%)
- Exact value: `(mouseY / barHeight) * 100`

**Drag horizontally:**
- Draw pattern across multiple steps
- Creates smooth ramps or repeated patterns
- Release to confirm

**Shift+Click:**
- Copy step to clipboard

**Ctrl+Click:**
- Clear step (set to 0)

**Alt+Drag:**
- Select range of steps
- Batch operation target

### Editing Length Handles

**Drag left handle:**
- Moves start step (offset from bar 1)
- Constrains right handle from moving past it

**Drag right handle:**
- Extends/reduces pattern length
- Never allows <1 step or >16 steps

**Double-click handle:**
- Reset to default (left=1, right=16)

### Editing Rotation

**Drag slider knob:**
- Rotates bars left/right in real-time
- Wraps at 64 steps
- Shows numeric feedback

**Mouse wheel on slider:**
- ±1 step per scroll
- Shift+wheel: ±8 steps per scroll

**Numeric input:**
- Click number to enter rotation value directly

---

## Visual Design

### Colors & Styling

**Lane colors (identifiable):**
```
Rest:       #505050 (gray)
Melody:     #d4af37 (gold)
Octave:     #b8860b (darker gold)
Legato:     #26a69a (teal)
Variation:  #ff6b6b (red)
```

**Bar styling:**
- Fill: Lane color
- Stroke: Darker shade of lane color
- Opacity: 0.8 (slightly transparent)
- Rounded corners: 1-2px radius

**Handle styling:**
- Left handle: Circle with "◄" indicator
- Right handle: Circle with "►" indicator
- Color: #888888 (gray)
- Highlight: #d4af37 (gold) on hover

**Rotation indicator:**
- Slider track: #2a2a2a
- Slider knob: #555555
- Tick marks: Every 8 steps
- Numeric display: #ddd

**Active step indicator:**
- Thin vertical line or highlight
- Color: #cc2222 (red, animated during playback)
- Updates in real-time with sequencer

---

## Implementation Strategy

### Code Structure (VCV Rack)

```cpp
// In StraitsEastSands.hpp
struct StraitsEastSandsVisualEditor {
  struct VoiceEditor {
    int voiceNum;           // 0-6 (voices 2-8)
    float dragStartValue;   // For undo
    int selectedLane;       // 0-4 (for keyboard input)
    bool isDraggingHandle;  // Length handle drag state
    
    // Drawing
    void draw(NVGcontext* vg, Rect box, StraitsEastSands* module);
    
    // Per-lane drawing
    void drawLane(NVGcontext* vg, Rect box, int laneType, 
                  const DNA& dna, int offset);
    
    // Components
    void drawLengthHandles(NVGcontext* vg, Rect box, 
                          int startStep, int endStep);
    void drawRotationSlider(NVGcontext* vg, Rect box, 
                           int rotation);
    void drawControlButtons(NVGcontext* vg, Rect box);
    
    // Interaction
    void onMouseDown(const event::MouseDown& e, Rect box);
    void onMouseMove(const event::MouseMove& e, Rect box);
    void onMouseUp(const event::MouseUp& e, Rect box);
    void onMouseScroll(const event::MouseScroll& e, Rect box);
  };
  
  std::vector<VoiceEditor> voiceEditors;  // One per voice
};
```

### Drawing Implementation

```cpp
void drawProbabilityBar(NVGcontext* vg, Vec pos, float width, float height,
                        float value, Color laneColor) {
  // Background rect
  nvgBeginPath(vg);
  nvgRect(vg, pos.x, pos.y, width, height);
  nvgFillColor(vg, color::hex("#1a1a1a"));
  nvgFill(vg);
  
  // Value bar (height = value * total height)
  float barHeight = value * height;
  nvgBeginPath(vg);
  nvgRect(vg, pos.x, pos.y + (height - barHeight), width, barHeight);
  nvgFillColor(vg, laneColor);
  nvgFill(vg);
  
  // Border
  nvgBeginPath(vg);
  nvgRect(vg, pos.x, pos.y, width, height);
  nvgStrokeColor(vg, laneColor.darker());
  nvgStrokeWidth(vg, 0.5);
  nvgStroke(vg);
}
```

### Event Handling

```cpp
void onMouseDown(const event::MouseDown& e, Rect box) {
  Vec mousePos = e.pos - box.pos;
  
  // Check which lane/step was clicked
  int laneIndex = getLaneAtY(mousePos.y);
  int stepIndex = getStepAtX(mousePos.x);
  
  if (laneIndex >= 0 && stepIndex >= 0 && stepIndex < 16) {
    // Set value based on Y position within lane
    float laneHeight = getLaneHeight();
    float clickValue = 1.0f - (mousePos.y % laneHeight) / laneHeight;
    clickValue = clamp(clickValue, 0.f, 1.f);
    
    // Update parameter
    setStepValue(voiceNum, laneIndex, stepIndex, clickValue);
    
    isDragging = true;
  }
}

void onMouseMove(const event::MouseMove& e, Rect box) {
  if (isDragging) {
    Vec mousePos = e.pos - box.pos;
    int stepIndex = getStepAtX(mousePos.x);
    
    if (stepIndex >= 0 && stepIndex < 16) {
      float clickValue = 1.0f - (mousePos.y % laneHeight) / laneHeight;
      clickValue = clamp(clickValue, 0.f, 1.f);
      setStepValue(voiceNum, laneIndex, stepIndex, clickValue);
    }
  }
}
```

---

## Design Questions

### 1. Multi-Voice Display

**Option A: Vertical Scroll**
- Show all 7 voices at once
- Requires vertical scrolling if panel is compact
- Pro: See relationships between voices
- Con: Tall interface, might overflow typical 380mm height

**Option B: Tab System**
- 7 tabs, one per voice
- Dedicated space per voice
- Pro: Clean, focused editing
- Con: Can't compare voices easily

**Option C: Dropdown Selector**
- Combo box: "Voice: [1-7]"
- Show one voice at a time
- Pro: Very compact
- Con: Less intuitive than tabs/scroll

**Recommendation:** Tab system (compact, standard UX)

---

### 2. Rotation Visualization

**Option A: Horizontal Slider** ✓ RECOMMENDED
- Most intuitive for step-based patterns
- Live visual rotation effect
- Numeric feedback (e.g., "32/64")
- Mouse wheel support

**Option B: LED Block Indicator**
- Array of 8 small LED blocks
- Each block represents 8 steps
- Current rotation highlighted
- More visual but less precise

**Option C: Knob**
- Traditional circular control
- Compact
- Less intuitive for step rotation

**Recommendation:** Horizontal slider + numeric display

---

### 3. Random Seed Management

**Options:**
- `[R]` button = quick randomize
- `[?]` button = load preset seed variations
- `[Seed]` dropdown = select from saved seeds
- `[Save]` button = store current pattern as seed

**Recommendation:** All three (with preset bank of 4-8 common patterns)

---

### 4. Lane Representation

**Binary/Boolean lanes (Legato, Variation flags):**
- Option A: Filled/empty blocks (█ vs ░)
- Option B: Height bars like others (but only 0% or 100%)
- Option C: Toggle checkboxes per step

**Recommendation:** Height bars for consistency, but allow 2-state toggle mode

---

### 5. Active Step Indicator

**Show which step is currently playing:**
- Option A: Vertical line (animated)
- Option B: Pulse/highlight effect
- Option C: LED block (like Midfield)

**Recommendation:** Vertical line + subtle glow, synchronized with Monsoon tempo

---

## Integration with Monsoon

### Real-Time Sync

```cpp
// During process() in module
if (currentStep != lastStep) {
  visualEditor.setActiveStep(currentStep);  // Animate
  lastStep = currentStep;
}
```

### Parameter Binding

```cpp
// In StraitsEastSands widget init
for (int step = 0; step < 16; step++) {
  for (int lane = 0; lane < 5; lane++) {
    // Tie visualization to actual parameter
    paramHandles[voiceNum][lane][step] = 
      params[DNA_VOICE_X_LANE_Y_STEP_Z].getValue();
  }
}
```

---

## Performance Considerations

### Optimization

**Dirty flag system:**
- Only redraw changed steps/voices
- Skip inactive voices (not in length range)
- Cache bar positions/dimensions

**Batch updates:**
- Group parameter changes during drag
- Flush to parameters on mouse release
- Reduces parameter update overhead

**Memory:**
- ~7 voices × 5 lanes × 16 steps × 1 byte = 560 bytes (minimal)
- Cache bar dimensions: ~7 × 5 = 35 cached rects

---

## Code References

### VCV Rack Drawing
- `nvgBeginPath()` / `nvgRect()` / `nvgFill()`
- `nvgStrokeColor()` / `nvgStroke()`
- Font rendering: `nvgText()`

### Event Handling
- `onMouseDown()` / `onMouseMove()` / `onMouseUp()`
- `onMouseScroll()` for rotation slider
- Keyboard input via module (arrow keys for step selection)

### Reference Implementations
- **Midfield Patch Master:** `src/modules/PatchEditor.cpp`
- **Voxglitch Arpseq:** `src/Arpseq.cpp` / `ArpseqWidget`
- **ML Modules Foundry:** `src/widgets/SequencerWidget.cpp`

---

## Rollout Plan

### Phase 1: Core Editor
- [ ] Single voice editor widget
- [ ] Probability bar drawing
- [ ] Length handle dragging
- [ ] Basic mouse interaction
- [ ] Rotation slider

### Phase 2: Polish
- [ ] Keyboard shortcuts (arrow keys, number input)
- [ ] Undo/redo support
- [ ] Preset/seed system
- [ ] Active step animation

### Phase 3: Integration
- [ ] Multi-voice tabs
- [ ] Real-time playback sync
- [ ] Monsoon parameter binding
- [ ] Save/load DNA state

### Phase 4: Advanced
- [ ] Pattern library
- [ ] Drag-and-drop between voices
- [ ] Humanization controls
- [ ] Arpeggiator modes

---

## Next Steps

1. **Create prototype widget** in StraitsEastSands.hpp
2. **Implement drawing functions** for probability bars
3. **Add mouse interaction** for step editing
4. **Test with Monsoon** in VCV Rack
5. **Iterate on UX** based on playback feedback

