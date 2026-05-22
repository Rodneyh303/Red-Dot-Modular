# DNA & Spread Interpolation Implementation Guide

## System Overview

The Red Dot Modular system consists of three modules working together:

```
MONSOON (40 HP)              SANDS MONO (12 HP)           SANDS MACRO (16 HP)
├─ Stochastic params         ├─ Rest DNA controls         ├─ Spread controls
├─ Pattern controls          ├─ Melody DNA controls       ├─ Mode toggle
├─ Mono sequencer            ├─ Octave DNA controls       └─ Scramble/Reset
└─ Poly voice outputs        └─ Scramble/Reset buttons
     ↓                             ↓                             ↓
     └─────────────────────────────────────────────────────────────
                    Expander chain (right edge)
```

## Monsoon Widget Implementation

### Panel Setup
```cpp
struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, 
            "res/panels/Monsoon_Clean_40HP.svg")));
        
        // box.size is AUTO-SET by Rack from SVG viewBox
        // ViewBox: 0 0 203.2 380 (millimeters)
        // DO NOT manually set box.size
    }
};
```

### Component Positioning (from SVG)

**Stochastic Parameters (6 Large Knobs at Y=55mm):**
- Note (X=22mm): Stochastic pitch variation
- Variation (X=52mm): Amount of randomness
- Legato (X=82mm): Probability of legato connections
- Rest (X=112mm): Probability of rest steps
- Accent (X=142mm): Probability of accents
- Transpose (X=172mm): Pitch offset

```cpp
// Example: Note knob
addParam(createParamCentered<RDM_KnobLarge>(
    mm2px(Vec(22.0f, 55.0f)), module, Monsoon::NOTE_VALUE_PARAM));
```

**Pattern Controls (3 Medium Knobs at Y=120mm):**
- BPM (X=40mm): Tempo
- Length (X=82mm): Pattern length in steps
- Offset (X=124mm): Pattern start offset
- Mode (X=171mm): Sequencer mode selector

```cpp
addParam(createParamCentered<RDM_KnobMedium>(
    mm2px(Vec(40.0f, 120.0f)), module, Monsoon::BPM_PARAM));
```

**Inputs (5 Small Jacks at Y=275mm):**
- Clock (X=28mm): Clock input
- Reset (X=56mm): Reset trigger
- Gate1 (X=84mm): External gate 1
- Gate2 (X=112mm): External gate 2
- CV (X=140mm): Control voltage input

```cpp
addInput(createInputCentered<PJ301MPort>(
    mm2px(Vec(28.0f, 275.0f)), module, Monsoon::CLOCK_INPUT));
```

**Outputs (8 Small Jacks at Y=355mm):**
- Gate (X=22mm): Gate output
- Note (X=40mm): Note CV output
- CV (X=58mm): Control voltage output
- Accent (X=76mm): Accent output
- Legato (X=94mm): Legato output
- Tie (X=112mm): Tie output
- Spare1 (X=130mm): Spare output 1
- Spare2 (X=148mm): Spare output 2

```cpp
addOutput(createOutputCentered<PJ301MPort>(
    mm2px(Vec(22.0f, 355.0f)), module, Monsoon::GATE_OUTPUT));
```

## SandsMono Widget Implementation

Panel: `res/panels/SandsMono_12HP.svg`
ViewBox: `0 0 60.96 380`

### Component Positioning

**Rest DNA Column (X=15mm):**
- Length knob (Y=50mm, Medium)
- Offset knob (Y=90mm, Small)
- Rotation knob (Y=125mm, Small)

**Melody DNA Column (X=30mm):**
- Length knob (Y=50mm, Medium)
- Offset knob (Y=90mm, Small)
- Rotation knob (Y=125mm, Small)

**Octave DNA Column (X=45mm):**
- Length knob (Y=50mm, Medium)
- Offset knob (Y=90mm, Small)
- Rotation knob (Y=125mm, Small)

**Buttons (Y=200mm):**
- Scramble All (X=20mm)
- Reset All (X=40mm)

### Implementation

```cpp
struct SandsMonoWidget : ModuleWidget {
    SandsMonoWidget(SandsMono* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance,
            "res/panels/SandsMono_12HP.svg")));
        
        // Rest DNA
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(15.0f, 50.0f)), module, SandsMono::DNA_REST_LEN_PARAM));
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(15.0f, 90.0f)), module, SandsMono::DNA_REST_OFF_PARAM));
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(15.0f, 125.0f)), module, SandsMono::DNA_REST_ROT_PARAM));
        
        // Melody DNA (same pattern at X=30)
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(30.0f, 50.0f)), module, SandsMono::DNA_MELODY_LEN_PARAM));
        // ... etc
        
        // Octave DNA (same pattern at X=45)
        // ... etc
        
        // Buttons
        addParam(createParamCentered<PB61303>(
            mm2px(Vec(20.0f, 200.0f)), module, SandsMono::DNA_SCRAMBLE_ALL_PARAM));
        addParam(createParamCentered<PB61303>(
            mm2px(Vec(40.0f, 200.0f)), module, SandsMono::DNA_RESET_ALL_PARAM));
    }
};
```

## SandsMacro Widget Implementation

Panel: `res/panels/SandsMacro_16HP.svg`
ViewBox: `0 0 80 380`

### Component Positioning

**Spread Sliders (Vertical):**
- Rest (X=15mm, Y=100-250mm): Vertical slider
- Melody (X=40mm, Y=100-250mm): Vertical slider
- Octave (X=65mm, Y=100-250mm): Vertical slider

**Controls:**
- Buttons at bottom (Y=300mm)
- Mode indicator light

### Implementation

```cpp
struct SandsMacroWidget : ModuleWidget {
    SandsMacroWidget(SandsMacro* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance,
            "res/panels/SandsMacro_16HP.svg")));
        
        // Vertical sliders for spread amounts
        addParam(createParamCentered<Slider>(
            mm2px(Vec(15.0f, 175.0f)), module, SandsMacro::SPREAD_REST_PARAM));
        addParam(createParamCentered<Slider>(
            mm2px(Vec(40.0f, 175.0f)), module, SandsMacro::SPREAD_MELODY_PARAM));
        addParam(createParamCentered<Slider>(
            mm2px(Vec(65.0f, 175.0f)), module, SandsMacro::SPREAD_OCTAVE_PARAM));
        
        // Light to indicate spread mode
        addChild(createLightCentered<SmallLight<RedLight>>(
            mm2px(Vec(40.0f, 20.0f)), module, SandsMacro::SPREAD_MODE_LIGHT));
    }
};
```

## Expander Message Chain

### Monsoon → SandsMono
```cpp
// In Monsoon::process()
if (leftExpander.module && leftExpander.module->model == modelSandsMono) {
    SandsMono::ExpanderMessage* msg = 
        (SandsMono::ExpanderMessage*)leftExpander.module->rightExpander.producerMessage;
    
    // Copy DNA windows from SandsMono
    std::memcpy(voiceDNAWindows[0].restDNA, msg->restDNAWindow, 
        sizeof(msg->restDNAWindow));
    // ... copy melody and octave
}
```

### SandsMono → SandsMacro
```cpp
// In SandsMono::process()
if (rightExpander.module && rightExpander.module->model == modelSandsMacro) {
    SandsMacro::ExpanderMessage* msg = 
        (SandsMacro::ExpanderMessage*)rightExpander.producerMessage;
    
    // Message is automatically updated in rightExpander
}
```

### SandsMacro → Monsoon
```cpp
// Spread state flows back to Monsoon via SandsMono
// Used in applySpreadInterpolation()
SpreadInterpolationEngine::applySpread(
    voiceDNA,
    monoValue,
    numVoices,
    spreadAmount,
    spreadState.mode
);
```

## Critical Implementation Notes

### 1. Box Size is Automatic
- Rack reads ViewBox from SVG
- Do NOT manually set box.size
- Conversion from mm to pixels is automatic

### 2. Component Positioning
- Use mm2px(Vec(x_mm, y_mm)) for all coordinates
- Extract from SVG guide marks (dashed circles)
- All measurements in millimeters

### 3. Spread Interpolation Call Order
1. Mono sequencer generates rest/melody/octave DNA
2. Per-voice DNA overrides applied (from East/West Sands)
3. Spread interpolation applied (from SandsMacro)
4. Final DNA windows used by all voices

### 4. DNA Parameter Defaults
- Rest: Length=0.5, Offset=0.0, Rotation=0.0
- Melody: Length=0.375, Offset=0.0, Rotation=0.0
- Octave: Length=0.25, Offset=0.0, Rotation=0.0

### 5. Spread Mode Selection
- Toggle via context menu on Monsoon
- Visual indicator light on SandsMacro
- Colors: Blue (Poly Average), Red (Mono Source)

## Testing Checklist

- [ ] Monsoon loads panel correctly (no errors)
- [ ] All knobs/jacks visible and positioned correctly
- [ ] SandsMono expander connects to Monsoon right edge
- [ ] SandsMacro expander connects to SandsMono right edge
- [ ] DNA parameters affect sequencer output
- [ ] Scramble/Reset buttons work
- [ ] Spread mode toggle works
- [ ] Spread amounts affect voice coherence
- [ ] Context menu toggles spread mode
- [ ] Save/load preserves all parameter values
