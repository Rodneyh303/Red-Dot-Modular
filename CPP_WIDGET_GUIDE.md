# Red Dot Modular - C++ Widget Implementation Guide

## Box Size Configuration

### Critical Point

**SVG ViewBox is in millimeters. C++ box.size is automatically calculated by Rack.**

```cpp
// When you load an SVG panel, Rack reads the viewBox
// <svg viewBox="0 0 203.2 380">

// Rack automatically sets:
// widget->box.size.x based on viewBox width and display DPI
// widget->box.size.y based on viewBox height and display DPI

// DO NOT manually calculate or set box.size
// Rack handles this automatically
```

---

## Module Size Reference

### Monsoon Main (40 HP)
```cpp
struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        
        // Load panel (viewBox: 0 0 203.2 380)
        // box.size will be automatically set
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/Monsoon_Main_40HP.svg")
        ));
        
        // After this line, box.size is set correctly
        // Example: box.size ≈ (1052, 1963) pixels @ 96 DPI
        
        // Component positioning uses mm2px() helper
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(20.0f, 52.0f)),  // X=20mm, Y=52mm
            module,
            Monsoon::NOTE_VALUE_PARAM
        ));
    }
};
```

### Interchange (24 HP)
```cpp
struct InterchangeWidget : ModuleWidget {
    InterchangeWidget(Interchange* module) {
        setModule(module);
        
        // viewBox: 0 0 121.92 380
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/Interchange_Main_24HP.svg")
        ));
        
        // box.size automatically set from viewBox
    }
};
```

### Straits East (12 HP)
```cpp
struct StraitsEastWidget : ModuleWidget {
    StraitsEastWidget(StraitsEast* module) {
        setModule(module);
        
        // viewBox: 0 0 60.96 380
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/StraitsEast_Outputs_12HP.svg")
        ));
        
        // Note: Left edge has RED LINE (accent)
        // This is rendered by SVG, not C++
    }
};
```

### Straits West (12 HP)
```cpp
struct StraitsWestWidget : ModuleWidget {
    StraitsWestWidget(StraitsWest* module) {
        setModule(module);
        
        // viewBox: 0 0 60.96 380
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/StraitsWest_Outputs_12HP.svg")
        ));
        
        // Note: Right edge has RED LINE (accent)
        // This is rendered by SVG, not C++
    }
};
```

---

## Matte Metallic Knob Components

### Define Knob Classes

```cpp
// In your plugin's header file or widget.cpp

struct RDM_KnobLarge : SvgKnob {
    RDM_KnobLarge() {
        minAngle = -M_PI * 0.75f;  // -135°
        maxAngle = M_PI * 0.75f;   // +135°
        // Indicator line rotates automatically
        
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/Knob_Large_Matte.svg")
        ));
    }
};

struct RDM_KnobMedium : SvgKnob {
    RDM_KnobMedium() {
        minAngle = -M_PI * 0.75f;
        maxAngle = M_PI * 0.75f;
        
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/Knob_Medium_Matte.svg")
        ));
    }
};

struct RDM_KnobSmall : SvgKnob {
    RDM_KnobSmall() {
        minAngle = -M_PI * 0.75f;
        maxAngle = M_PI * 0.75f;
        
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/Knob_Small_Matte.svg")
        ));
    }
};

struct RDM_Port : SvgPort {
    RDM_Port() {
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/Port_PJ301M_Matte.svg")
        ));
    }
};
```

---

## Component Positioning

### Monsoon Main - 40 HP

Coordinates from SVG guide marks (in millimeters):

```cpp
struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/Monsoon_Main_40HP.svg")
        ));
        
        // ===== STOCHASTIC PARAMETERS (Y=52mm) =====
        // 6 Large knobs
        
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(20.0f, 52.0f)), module, Monsoon::NOTE_VALUE_PARAM));
        
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(50.0f, 52.0f)), module, Monsoon::VARIATION_PARAM));
        
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(80.0f, 52.0f)), module, Monsoon::LEGATO_PARAM));
        
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(110.0f, 52.0f)), module, Monsoon::REST_PARAM));
        
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(140.0f, 52.0f)), module, Monsoon::ACCENT_PARAM));
        
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(170.0f, 52.0f)), module, Monsoon::TRANSPOSE_PARAM));
        
        // ===== PATTERN ENGINE (Y=125mm) =====
        // 3 Medium knobs
        
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(35.0f, 125.0f)), module, Monsoon::BPM_PARAM));
        
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(75.0f, 125.0f)), module, Monsoon::PATTERN_LENGTH_PARAM));
        
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(115.0f, 125.0f)), module, Monsoon::PATTERN_OFFSET_PARAM));
        
        // ===== DNA CONTROLS =====
        // Rest DNA (Y=200mm)
        
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(20.0f, 200.0f)), module, Monsoon::DNA_REST_LEN_PARAM));
        
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(40.0f, 200.0f)), module, Monsoon::DNA_REST_OFF_PARAM));
        
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(50.0f, 200.0f)), module, Monsoon::DNA_REST_ROT_PARAM));
        
        // Melody DNA (Y=200mm)
        
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(65.0f, 200.0f)), module, Monsoon::DNA_MELODY_LEN_PARAM));
        
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(85.0f, 200.0f)), module, Monsoon::DNA_MELODY_OFF_PARAM));
        
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(95.0f, 200.0f)), module, Monsoon::DNA_MELODY_ROT_PARAM));
        
        // Octave DNA (Y=200mm)
        
        addParam(createParamCentered<RDM_KnobMedium>(
            mm2px(Vec(110.0f, 200.0f)), module, Monsoon::DNA_OCTAVE_LEN_PARAM));
        
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(130.0f, 200.0f)), module, Monsoon::DNA_OCTAVE_OFF_PARAM));
        
        addParam(createParamCentered<RDM_KnobSmall>(
            mm2px(Vec(140.0f, 200.0f)), module, Monsoon::DNA_OCTAVE_ROT_PARAM));
        
        // Interpolation sliders (vertical, Y=190-225mm)
        
        addParam(createParamCentered<Slider>(
            mm2px(Vec(159.5f, 207.5f)), module, Monsoon::DNA_REST_INTERP_PARAM));
        
        addParam(createParamCentered<Slider>(
            mm2px(Vec(174.5f, 207.5f)), module, Monsoon::DNA_MELODY_INTERP_PARAM));
        
        addParam(createParamCentered<Slider>(
            mm2px(Vec(189.5f, 207.5f)), module, Monsoon::DNA_OCTAVE_INTERP_PARAM));
        
        // ===== INPUTS (Y=290mm) =====
        
        addInput(createInputCentered<RDM_Port>(
            mm2px(Vec(25.0f, 290.0f)), module, Monsoon::CLOCK_INPUT));
        
        addInput(createInputCentered<RDM_Port>(
            mm2px(Vec(50.0f, 290.0f)), module, Monsoon::RESET_INPUT));
        
        addInput(createInputCentered<RDM_Port>(
            mm2px(Vec(75.0f, 290.0f)), module, Monsoon::GATE1_INPUT));
        
        addInput(createInputCentered<RDM_Port>(
            mm2px(Vec(100.0f, 290.0f)), module, Monsoon::GATE2_INPUT));
        
        addInput(createInputCentered<RDM_Port>(
            mm2px(Vec(125.0f, 290.0f)), module, Monsoon::CV2_INPUT));
        
        // ===== OUTPUTS (Y=360mm) =====
        
        addOutput(createOutputCentered<RDM_Port>(
            mm2px(Vec(20.0f, 360.0f)), module, Monsoon::GATE_OUTPUT));
        
        addOutput(createOutputCentered<RDM_Port>(
            mm2px(Vec(42.0f, 360.0f)), module, Monsoon::NOTE_OUTPUT));
        
        addOutput(createOutputCentered<RDM_Port>(
            mm2px(Vec(64.0f, 360.0f)), module, Monsoon::CV_OUTPUT));
        
        addOutput(createOutputCentered<RDM_Port>(
            mm2px(Vec(86.0f, 360.0f)), module, Monsoon::ACCENT_OUTPUT));
        
        addOutput(createOutputCentered<RDM_Port>(
            mm2px(Vec(108.0f, 360.0f)), module, Monsoon::LEGATO_OUTPUT));
        
        addOutput(createOutputCentered<RDM_Port>(
            mm2px(Vec(130.0f, 360.0f)), module, Monsoon::TIE_OUTPUT));
    }
};
```

---

## Important Notes

### 1. Never Manually Set box.size

```cpp
// WRONG - don't do this
widget->box.size.x = 1052;
widget->box.size.y = 1963;

// RIGHT - Rack sets it automatically from SVG viewBox
setPanel(createPanel(asset::plugin(...)));
// box.size is now correct
```

### 2. Always Use mm2px() for Coordinates

```cpp
// WRONG - pixel coordinates won't match SVG
addParam(createParamCentered<RDM_KnobLarge>(
    Vec(103.6, 269.5),  // pixels, fragile
    module,
    Monsoon::NOTE_VALUE_PARAM
));

// RIGHT - mm coordinates scaled automatically
addParam(createParamCentered<RDM_KnobLarge>(
    mm2px(Vec(20.0f, 52.0f)),  // millimeters, robust
    module,
    Monsoon::NOTE_VALUE_PARAM
));
```

### 3. Extract Coordinates from SVG Guide Marks

When component guide marks in SVG are dashed circles at (X, Y), use `mm2px(Vec(X, Y))` in C++.

```
SVG Guide Mark:  <circle cx="20" cy="52" r="9.5" ... />
C++ Positioning: mm2px(Vec(20.0f, 52.0f))
```

### 4. Component Sizes

```
Large Knob (19mm):   radius in SVG ≈ 9.5mm  → scales to fit 19mm physical
Medium Knob (15mm):  radius in SVG ≈ 8mm    → scales to fit 15mm physical
Small Knob (11mm):   radius in SVG ≈ 6.5mm  → scales to fit 11mm physical
Port (10.5mm):       radius in SVG ≈ 5.25mm → scales to fit 10.5mm physical
```

---

## Debugging Box Size

To verify panel loaded correctly:

```cpp
struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Monsoon_Main_40HP.svg")));
        
        // After panel loads, check dimensions
        DEBUG("Panel loaded successfully");
        DEBUG("  ViewBox: 203.2 × 380 mm");
        DEBUG("  box.size: %.2f × %.2f px", box.size.x, box.size.y);
        DEBUG("  Approx: %.2f × %.2f mm", box.size.x / 5.18f, box.size.y / 5.18f);
        
        // Should match expected dimensions
    }
};
```

---

## SVG Panel ViewBox Reference

```
Monsoon Main (40 HP):           0 0 203.2 380
Interchange (24 HP):            0 0 121.92 380
Straits East (12 HP):           0 0 60.96 380   (RED line LEFT)
Straits West (12 HP):           0 0 60.96 380   (RED line RIGHT)
Sands Mono (12 HP):             0 0 60.96 380
Straits East Sands (24 HP):     0 0 121.92 380  (RED line LEFT)
Straits West Sands (24 HP):     0 0 121.92 380  (RED line RIGHT)
Straits Sands Macro (16 HP):    0 0 80 380

Height always 380 (standard 3U Eurorack)
```

---

## Complete Widget Template

```cpp
#include "plugin.hpp"

// Knob class definitions
struct RDM_KnobLarge : SvgKnob { /* ... */ };
struct RDM_KnobMedium : SvgKnob { /* ... */ };
struct RDM_KnobSmall : SvgKnob { /* ... */ };
struct RDM_Port : SvgPort { /* ... */ };

struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        
        // Load SVG panel (viewBox dimensions)
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/Monsoon_Main_40HP.svg")
        ));
        // box.size is AUTOMATICALLY SET here (no manual calculation)
        
        // Add all components using mm2px() from SVG guide marks
        // (See full implementation above)
    }
    
    void appendContextMenu(Menu* menu) override {
        menu->addChild(new MenuSeparator());
        
        // Add spread mode selector
        menu->addChild(createIndexPtrSubmenuItem(
            "Spread Mode",
            {"Poly Average", "Mono Source"},
            [this]() { return (int)(module->spreadMode); },
            [this](int i) { module->spreadMode = (SpreadMode)i; }
        ));
    }
};

Model* modelMonsoon = createModel<Monsoon, MonsoonWidget>("Monsoon");
```

---

## Summary

| Task | Value |
|------|-------|
| Set box.size manually? | ❌ NO (Rack does it automatically) |
| Use mm2px() for coordinates? | ✅ YES (always) |
| SVG unit? | ✅ Millimeters (Rack standard) |
| C++ unit? | ✅ Pixels (display-dependent) |
| Knob rotation range? | -135° to +135° (270° total) |
| Theme support? | ✅ YES (separate SVG files optional) |

---

**Status:** ✅ Ready to implement
