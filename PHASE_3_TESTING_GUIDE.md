# Phase 3: Complete Integration Example & Testing

## End-to-End Flow

```
User edits visual editor
         ↓
Editor updates internal VoiceState
         ↓
SandsParameterManager.syncEditorToModule()
         ↓
Monsoon.params[paramId].setValue()
         ↓
Monsoon DSP reads new parameter
         ↓
Voice playback changes in real-time
         ↓
Monsoon updates currentStep
         ↓
StraitsVisualEditorPanel.setCurrentPlayStep()
         ↓
Active step indicator animates
```

---

## Setup Walkthrough

### Step 1: Include Headers

```cpp
// In plugin.hpp or your main include
#include "ui/SandsVisualEditorV2.hpp"
#include "ui/StraitsVisualEditorPanel.hpp"
#include "ui/MonsoonSandsIntegration.hpp"
#include "modules/StraitsEastWest.hpp"
```

### Step 2: Verify Parameter Layout

Check your `Monsoon.hpp` enum ParamId:

```cpp
enum ParamId {
  POLY_DNA_VOICE_1_LEN,    // = 0 (example)
  POLY_DNA_VOICE_1_OFF,    // = 1
  POLY_DNA_VOICE_1_ROT,    // = 2
  
  POLY_DNA_VOICE_2_LEN,    // = 3
  POLY_DNA_VOICE_2_OFF,    // = 4
  POLY_DNA_VOICE_2_ROT,    // = 5
  
  // ... continues
  
  POLY_MELODY_VOICE_1_LEN, // = 48 (example offset)
  // ... etc
};
```

Then update `MonsoonParameterManager` constants:

```cpp
struct MonsoonParameterManager : SandsParameterManager {
  static constexpr int POLY_DNA_VOICE_1_LEN = 0;      // Your actual value
  static constexpr int POLY_MELODY_VOICE_1_LEN = 48;  // Your actual value
  // ... etc
};
```

### Step 3: Implement Module Methods

In your Monsoon module:

```cpp
struct Monsoon : rack::Module {
  // ... existing code ...
  
  int currentPlayStep = -1;
  
  void process(const ProcessArgs& args) override {
    // ... normal DSP ...
    
    // Update playback position
    currentPlayStep = (int)(playbackPhase / (2.0f * M_PI) * 16.0f) % 16;
    
    // Tell expanders about current step
    if (rightExpander.module) {
      auto* straits = dynamic_cast<redDot::StraitsEastSands*>(rightExpander.module);
      if (straits) {
        straits->setCurrentStep(currentPlayStep);
      }
    }
  }
  
  int getCurrentStep() { return currentPlayStep; }
};
```

### Step 4: Wire Up Module Widgets

In your `plugin.cpp`:

```cpp
#include "ui/MonsoonSandsIntegration.hpp"

struct StraitsEastSandsWidget : rack::ModuleWidget {
  redDot::StraitsEastSands* module;
  redDot::StraitsEastSandsPanel* visualPanel;
  
  StraitsEastSandsWidget(redDot::StraitsEastSands* mod) : module(mod) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, 
      "res/panels/StraitsEastSands_Visual_24HP.svg")));
    
    // Create visual editor
    visualPanel = new redDot::StraitsEastSandsPanel(module);
    visualPanel->box.pos = rack::Vec(10, 60);
    visualPanel->box.size = rack::Vec(210, 220);
    addChild(visualPanel);
  }
  
  void step() override {
    ModuleWidget::step();
    
    // Sync playback position from Monsoon
    if (module && visualPanel && module->monsoonModule) {
      auto* monsoon = static_cast<Monsoon*>(module->monsoonModule);
      visualPanel->setCurrentPlayStep(monsoon->getCurrentStep());
    }
  }
};

// Register models
Model* modelStraitsEastSands = createModel<
  redDot::StraitsEastSands,
  StraitsEastSandsWidget
>("StraitsEastSands");

Model* modelStraitsWestSands = createModel<
  redDot::StraitsWestSands,
  StraitsWestSandsWidget
>("StraitsWestSands");
```

---

## Test Scenario 1: Basic Parameter Sync

**Objective:** Verify editor ↔ module parameter binding works

### Setup
1. Load Monsoon + Straits East in VCV Rack
2. Connect audio output
3. Play a sequence

### Test Steps

**Editor → Module (changes propagate to playback)**

1. Select voice 2 (V2 tab)
2. Click on a REST lane bar to set it high (80%)
3. Check: Monsoon playback changes immediately
4. Change another bar in MELODY lane
5. Check: Melody voice changes in playback

**Module → Editor (changes appear in editor)**

1. In Rack UI, manually adjust POLY_DNA_VOICE_2_ROT parameter
2. Wait ~16ms for sync
3. Check: Rotation block in editor moves to new position
4. Adjust POLY_DNA_VOICE_2_OFF parameter
5. Check: Start handle in editor moves

### Expected Results

✅ Edits in editor affect Monsoon playback (< 50ms latency)
✅ Changes in Monsoon appear in editor (synced at 60Hz)
✅ No value drift (values match exactly)
✅ All 5 lanes sync correctly

---

## Test Scenario 2: Voice Switching

**Objective:** Verify tab system saves/restores state correctly

### Setup
1. Load Straits East with all 7 voices
2. Play Monsoon

### Test Steps

1. Select V2, edit REST lane (set every other bar high)
2. Click V3 tab
   - Check: V2 state saved to Monsoon
   - Check: V3 state loaded and displayed
3. Edit V3 MELODY lane (reverse pattern)
4. Click back to V2 tab
   - Check: V2 shows original edits (REST pattern restored)
5. Undo (Ctrl+Z) in V2
   - Check: Last action undone (undo history per-voice)
6. Click V4 tab
7. Click back to V2
   - Check: Undo/redo history still available

### Expected Results

✅ Each voice maintains independent state
✅ Tab switching saves/restores instantly
✅ No cross-contamination between voices
✅ Undo/redo works per-voice

---

## Test Scenario 3: Playback Sync Animation

**Objective:** Verify active step indicator tracks playback

### Setup
1. Monsoon playing
2. Visual editor visible
3. Playback tempo = 120 BPM

### Test Steps

1. Start Monsoon playback
2. Watch active step indicator (vertical red line)
   - Check: Appears at step 1
   - Check: Moves to step 2, 3, etc.
   - Check: Loops back at step 16
3. Check animation glow
   - Check: Glows/pulses (sine wave)
   - Check: Frequency feels natural (~6Hz)
4. Stop Monsoon
   - Check: Indicator disappears (currentPlayStep = -1)

### Expected Results

✅ Indicator appears at correct step
✅ Updates with playback (tracks Monsoon position)
✅ Animation is smooth, not stuttering
✅ No performance impact (CPU stays low)

---

## Test Scenario 4: State Persistence

**Objective:** Verify save/load works

### Setup
1. Straits East with some edited voices

### Test Steps

1. Edit multiple voices:
   - V2: Random REST pattern
   - V5: Ramp MELODY pattern
   - V8: Euclidean OCTAVE pattern
2. Save patch (Ctrl+S or File → Save)
3. Close VCV Rack
4. Reopen patch
5. Check:
   - V2 shows same REST pattern
   - V5 shows same MELODY pattern
   - V8 shows same OCTAVE pattern
   - Undo/redo history is cleared (new session)

### Expected Results

✅ All voice states restored exactly
✅ No data loss on save/load
✅ JSON format is readable (can inspect file)
✅ File size reasonable (~2KB per voice)

---

## Test Scenario 5: Keyboard Shortcuts

**Objective:** Verify all shortcuts work

### Setup
1. Visual editor focused (click on it)
2. Monsoon playing

### Test Shortcuts

```
Ctrl+Z         Undo last action
Ctrl+Shift+Z   Redo (or Ctrl+Y)
1-5            Select lanes (REST, MELODY, OCTAVE, LEGATO, VARIATION)
R              Randomize selected lane
Shift+R        Randomize all lanes
P              Toggle preset panel
Ctrl+C         Copy selected lane
Ctrl+V         Paste to selected lane
Shift+V        Reverse selected lane
←/→            Navigate steps (if implemented)
```

### Expected Results

✅ All shortcuts respond (no lag)
✅ Don't interfere with mouse interaction
✅ Status bar updates (lane, preset)
✅ Undo/redo counts correct

---

## Test Scenario 6: Preset System

**Objective:** Verify presets work

### Setup
1. Visual editor open
2. V2 selected

### Test Steps

1. Press P to toggle preset panel
   - Check: Panel appears
2. Click "Euclidean" preset
   - Check: REST lane changes to alternating pattern
   - Check: Status shows "Preset:Euclidean"
3. Edit the pattern (click a bar)
4. Click "Sine" preset
   - Check: Pattern changes to sine
   - Check: Status shows "Preset:Sine"
5. Press Ctrl+Z (undo)
   - Check: Goes back to Euclidean
   - Check: Undo count increases

### Expected Results

✅ Presets load correctly
✅ Loading preset creates undo point
✅ Undo works across preset loads
✅ Panel toggle works

---

## Performance Testing

### Metrics to Check

```
Metric                    Target      Method
─────────────────────────────────────────────────────────
Parameter sync latency    < 50ms      Edit bar, measure to playback change
Playback indicator sync   < 16ms      Visual sync with 60Hz update
Tab switching time        < 10ms      Measure frame time
Memory per voice          < 1KB       (VoiceState = ~80 values)
CPU during edit           < 2%        Check VCV Rack CPU meter
Active step glow FPS      60 FPS      Observe smoothness
```

### How to Test

1. **Latency:** Edit a parameter and listen for playback change
2. **Sync:** Tap a beat and watch indicator alignment
3. **Switching:** Count frames during tab click (should be instant)
4. **Memory:** Check RAM in system monitor before/after loading
5. **CPU:** Watch VCV Rack CPU meter while editing
6. **Smoothness:** Look for stuttering or jitter

---

## Troubleshooting Common Issues

### Issue: Parameters Not Syncing

**Symptom:** Editor changes don't affect playback

**Diagnosis:**
1. Check parameter IDs in MonsoonParameterManager
2. Verify POLY_*_BASE constants match Monsoon.hpp
3. Add debug logging to syncEditorToModule()
4. Check that getStepParamId() returns valid IDs (>= 0)

**Solution:**
```cpp
// Add debug output
int paramId = getStepParamId(lane, voice, step);
if (paramId < 0) {
  WARN("Invalid param ID for lane=%d voice=%d step=%d", lane, voice, step);
}
```

### Issue: Active Step Not Showing

**Symptom:** Playback indicator (red line) doesn't appear

**Diagnosis:**
1. Check Monsoon.getCurrentStep() returns valid value
2. Verify setCurrentPlayStep() is called in widget step()
3. Check currentPlayStep isn't always -1

**Solution:**
```cpp
void step() override {
  if (module && module->getCurrentStep) {
    int step = module->getCurrentStep();
    if (step >= 0 && step < 16) {
      visualPanel->setCurrentPlayStep(step);
    }
  }
}
```

### Issue: Voice Switching Loses State

**Symptom:** Switching tabs and back loses edits

**Diagnosis:**
1. Check syncEditorToModule() is called on tab switch
2. Verify paramManager is syncing all lanes
3. Check module parameters actually change (inspect in Rack UI)

**Solution:**
```cpp
void selectVoice(int voiceNum) {
  if (voiceNum != selectedVoice) {
    syncEditorToModule(selectedVoice);  // ← Save before switching
    selectedVoice = voiceNum;
    syncModuleToEditor(selectedVoice);  // ← Load new voice
  }
}
```

### Issue: Undo/Redo Doesn't Work

**Symptom:** Ctrl+Z doesn't undo or goes too far back

**Diagnosis:**
1. Check saveToHistory() is called on drag end, not continuous
2. Verify VoiceState::operator== works correctly
3. Check undoHistory isn't empty (check size in status bar)

**Solution:**
```cpp
void saveToHistory() {
  // Only save if state actually changed
  if (undoHistory.empty() || !(undoHistory.back() == currentState)) {
    undoHistory.push_back(currentState);
    redoHistory.clear();  // Clear redo on new action
  }
}
```

### Issue: Presets Not Saving

**Symptom:** Custom presets disappear when reopening

**Diagnosis:**
1. Check dataToJson() includes preset bank
2. Verify dataFromJson() restores presets
3. Check JSON format is correct

**Solution:**
```cpp
// In module's dataToJson()
json_t* presets = json_array();
for (int p = 0; p < 8; ++p) {
  json_t* preset = serializeVoiceState(presetBank[p].state);
  json_array_append(presets, preset);
  json_decref(preset);
}
json_object_set_new(root, "presets", presets);
```

---

## Checklist: Phase 3 Complete

### Code Quality
- [ ] All parameter IDs verified against Monsoon.hpp
- [ ] MonsoonParameterManager constants set correctly
- [ ] Stride calculation validated (usually 3)
- [ ] Bounds checking on all param access
- [ ] Null pointer checks in syncers

### Integration
- [ ] Widgets created and added to module
- [ ] Models registered in plugin.cpp
- [ ] Expander interface wired (if applicable)
- [ ] dataToJson/dataFromJson implemented
- [ ] getCurrentStep() method available

### Features
- [ ] Parameter sync works both directions
- [ ] Voice switching saves/loads state
- [ ] Playback sync animation works
- [ ] State persistence saves/loads correctly
- [ ] All keyboard shortcuts functional
- [ ] Preset system works

### Testing
- [ ] Scenario 1: Parameter sync ✓
- [ ] Scenario 2: Voice switching ✓
- [ ] Scenario 3: Playback sync ✓
- [ ] Scenario 4: State persistence ✓
- [ ] Scenario 5: Keyboard shortcuts ✓
- [ ] Scenario 6: Preset system ✓

### Performance
- [ ] Parameter sync < 50ms latency
- [ ] Playback indicator updates at 60Hz
- [ ] Tab switching instant (< 10ms)
- [ ] No audio glitches during editing
- [ ] CPU usage stays low (< 2%)

---

## Next Steps

After Phase 3 is complete:

### Phase 4: Polish & Features
- [ ] Pattern interpolation (morph between presets)
- [ ] Humanization slider (add controlled randomness)
- [ ] Drag-and-drop between voices
- [ ] Pattern library (load/save preset banks from file)
- [ ] Cross-module coordination (Monsoon ↔ Straits East/West)

### Optimization
- [ ] Profile CPU usage
- [ ] Optimize redraw (dirty rect system)
- [ ] Cache frequently accessed parameters
- [ ] Batch parameter updates where possible

### Documentation
- [ ] Video tutorial
- [ ] User guide (how to use visual editor)
- [ ] API documentation (for extensions)
- [ ] Parameter reference (all 5 lanes explained)

