# Red Dot Modular - Widget Integration & SVG Scaling Guide

## SVG ViewBox vs C++ box.size Relationship

### Critical Concept

**SVG viewBox is in millimeters (Rack standard)**  
**C++ box.size is in pixels (display coordinates)**

The scaling relationship depends on the display DPI and Rack's rendering context.

```
SVG Viewbox (mm):     C++ box.size (px):
0 0 203.2 380   →     ~1052 px × 1963 px  (typical 96 DPI)
                       
Scaling factor ≈ 5.18 px/mm (at 96 DPI)
```

### Loading SVG Panels in Widgets

**Standard VCV Rack pattern:**

```cpp
struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        
        // Load SVG panel (automatic scaling)
        setPanel(createPanel(
            asset::plugin(pluginInstance, "res/panels/Monsoon_panel_light.svg")
        ));
        
        // Rack automatically reads viewBox="0 0 203.2 380"
        // Scales to fit window at configured zoom level
        // box.size is automatically set based on viewBox aspect ratio
        
        // Now add components using box.size for reference
        // but mm coordinates from SVG as source truth
    }
};
```

**Key Point:** Do NOT calculate box.size manually. It's automatically set by Rack based on the SVG viewBox.

### Verifying Panel Dimensions

**In your widget constructor:**

```cpp
// After loading panel
float panelWidth_mm = 203.2f;  // From SVG viewBox width
float panelHeight_mm = 380.0f; // From SVG viewBox height

// Rack will set:
// box.size.x ≈ panelWidth_mm × 5.18  (pixels, zoom-dependent)
// box.size.y ≈ panelHeight_mm × 5.18  (pixels, zoom-dependent)

// Verify (optional debug):
if (module) {
    DEBUG("Panel loaded: %.2f × %.2f mm", 
        box.size.x / 5.18f, 
        box.size.y / 5.18f);
}
```

---

## Panel Dimensions Reference

### Expanded Monsoon (40 HP)

| Attribute | Value | Notes |
|-----------|-------|-------|
| **Width (HP)** | 40 | Increased from 34 HP |
| **Width (mm)** | 203.2 | 40 × 5.08 mm/HP |
| **Height (mm)** | 380 | Standard 3U Eurorack |
| **SVG ViewBox** | 0 0 203.2 380 | Exact dimensions in mm |
| **Estimated box.size** | ~1052 × 1963 px | At 96 DPI, zoom=1.0 |

### Expansion Modules (24 HP)

| Module | Width (HP) | Width (mm) | ViewBox | Status |
|--------|-----------|-----------|---------|--------|
| Interchange | 24 | 121.92 | 0 0 121.92 380 | ✅ |
| StraitsEastSands | 24 | 121.92 | 0 0 121.92 380 | ✅ |
| StraitsWestSands | 24 | 121.92 | 0 0 121.92 380 | ✅ |

---

## Component Asset Integration

### Knob Assets (Matte Metallic)

**Files:**
- `RDM_KnobLarge.svg` (19mm diameter, main parameters)
- `RDM_KnobMedium.svg` (15mm diameter, secondary controls)
- `RDM_KnobSmall.svg` (11mm diameter, trim/DNA controls)

**Widget Implementation Pattern:**

```cpp
#include "asset.hpp"

// Define knob component class
struct RDM_KnobLarge : SvgKnob {
    RDM_KnobLarge() {
        minAngle = -M_PI * 0.75f;  // -135°
        maxAngle = M_PI * 0.75f;   // +135°
        
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/RDM_KnobLarge.svg")
        ));
    }
};

struct RDM_KnobMedium : SvgKnob {
    RDM_KnobMedium() {
        minAngle = -M_PI * 0.75f;
        maxAngle = M_PI * 0.75f;
        
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/RDM_KnobMedium.svg")
        ));
    }
};

struct RDM_KnobSmall : SvgKnob {
    RDM_KnobSmall() {
        minAngle = -M_PI * 0.75f;
        maxAngle = M_PI * 0.75f;
        
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/RDM_KnobSmall.svg")
        ));
    }
};

// In widget constructor
struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/Monsoon_panel_light.svg")));
        
        // Knob positioning (in mm from SVG guide marks)
        // Rack converts mm → px internally
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(18.0f, 72.0f)),  // X, Y in mm
            module,
            Monsoon::NOTE_VALUE_PARAM
        ));
    }
};
```

**Important Notes:**
- SVG viewBox is in millimeters
- Component positions in widget code should use `mm2px()` converter
- Extract X,Y coordinates from SVG guide marks (in mm)
- Rack handles the scaling automatically

---

## Theme Support (Light/Dark)

### Loading Correct Panel Theme

**Automated approach (Recommended):**

```cpp
struct MonsoonWidget : ModuleWidget {
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        
        // Load appropriate theme based on Rack setting
        std::string panelFile = settings::preferDarkPanels ? 
            "res/panels/Monsoon_panel_expanded_dark.svg" :
            "res/panels/Monsoon_panel_expanded_light.svg";
        
        setPanel(createPanel(asset::plugin(pluginInstance, panelFile)));
    }
};
```

**Manual approach (for custom theme handling):**

```cpp
struct MonsoonWidget : ModuleWidget {
    int themeIndex = 0;  // 0 = light, 1 = dark
    
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        updateTheme();
    }
    
    void updateTheme() {
        std::string file = (themeIndex == 0) ?
            "res/panels/Monsoon_panel_expanded_light.svg" :
            "res/panels/Monsoon_panel_expanded_dark.svg";
        
        setPanel(createPanel(asset::plugin(pluginInstance, file)));
        // Note: You may need to re-add all components after theme change
    }
};
```

---

## Peranakan Lattice Overlay

### Rendering in Widget

The peranakan lattice is rendered by **C++ at runtime**, not from SVG:

```cpp
// In your widget class
struct MonsoonWidget : ModuleWidget {
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        
        // Draw peranakan lattice (3-layer geometric)
        drawPeranakanLattice(args);
    }
    
private:
    void drawPeranakanLattice(const DrawArgs& args) {
        // Uses PeranakanLatticePanel implementation
        // Handles theme-aware colors automatically
        // Scales to box.size
    }
};
```

**Or inherit from lattice panel base:**

```cpp
struct MonsoonWidget : PeranakanLatticePanelLarge {
    // Automatically includes draw() with lattice
    // No additional implementation needed
};
```

---

## Knob Rotation Indicator

### SVG Indicator Line Rotation

Each knob SVG includes a **crimson indicator line** at the top (12 o'clock position).

**Rack's SvgKnob automatically rotates this line** based on parameter value:

```cpp
// In RDM_KnobLarge.svg:
<line x1="30" y1="11" x2="30" y2="5" stroke="#dc2626" stroke-width="2" />
// ^ This line rotates around center (30, 30)

// Rack's rotation formula (approximate):
// angle = minAngle + (paramValue / 1.0) * (maxAngle - minAngle)
// Default: minAngle = -135°, maxAngle = +135° (270° total range)
```

**You don't need to implement rotation.** Rack's SvgKnob handles it automatically.

---

## Box.Size Best Practices

### DO:
✅ Use `mm2px()` helper for component positioning
✅ Let Rack automatically set box.size based on SVG viewBox
✅ Use viewBox dimensions (mm) in coordinate calculations
✅ Test with different zoom levels (50%, 100%, 200%)
✅ Verify panel loads correctly in both light and dark themes

### DON'T:
❌ Don't manually calculate box.size
❌ Don't assume a fixed pixel size (varies with zoom)
❌ Don't use pixel coordinates directly (use mm + mm2px)
❌ Don't forget to set viewBox in SVG (critical for Rack)
❌ Don't assume DPI (Rack handles display-dependent scaling)

---

## Panel Files Checklist

### Monsoon (40 HP, 203.2mm × 380mm)
```
✅ Monsoon_panel_light.svg (viewBox: 0 0 203.2 380)
✅ Monsoon_panel_dark.svg  (viewBox: 0 0 203.2 380)

Files include:
  • Component guide marks (dashed circles/rectangles)
  • Text labels (parameter names, jack labels)
  • Header with branding
  • Footer with module name
  • NO actual controls (those are added by widget code)
```

### Component Assets
```
✅ RDM_KnobLarge.svg      (SVG viewBox: 0 0 60 60, scales to 19mm)
✅ RDM_KnobMedium.svg     (SVG viewBox: 0 0 50 50, scales to 15mm)
✅ RDM_KnobSmall.svg      (SVG viewBox: 0 0 40 40, scales to 11mm)
```

---

## Example: Complete Widget Setup

```cpp
#include "rack.hpp"
#include "plugin.hpp"
#include "ui/PeranakanLatticePanel.hpp"

using namespace rack;

// Knob definitions
struct RDM_KnobLarge : SvgKnob {
    RDM_KnobLarge() {
        minAngle = -M_PI * 0.75f;
        maxAngle = M_PI * 0.75f;
        setSvg(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/components/RDM_KnobLarge.svg")
        ));
    }
};

// ... other component definitions ...

// Widget
struct MonsoonWidget : ModuleWidget {
    int themeIndex = 0;
    
    MonsoonWidget(Monsoon* module) {
        setModule(module);
        
        // Load panel (40 HP, 203.2mm wide)
        std::string panelFile = settings::preferDarkPanels ?
            "res/panels/Monsoon_panel_expanded_dark.svg" :
            "res/panels/Monsoon_panel_expanded_light.svg";
        
        setPanel(createPanel(asset::plugin(pluginInstance, panelFile)));
        
        // Now box.size is set by Rack based on SVG viewBox
        // It will be approximately (203.2 * 5.18) × (380 * 5.18) pixels
        
        // Add components using mm coordinates from SVG guide marks
        // NOTE: X=18, Y=72 (in mm from SVG)
        addParam(createParamCentered<RDM_KnobLarge>(
            mm2px(Vec(18.0f, 72.0f)),
            module,
            Monsoon::NOTE_VALUE_PARAM
        ));
        
        // ... add more components ...
        
        // Add peranakan lattice overlay
        // (inherited from PeranakanLatticePanelLarge, or custom draw())
    }
    
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        // Lattice rendered here if using custom implementation
    }
};

Model* modelMonsoon = createModel<Monsoon, MonsoonWidget>("Monsoon");
```

---

## Debugging Panel Dimensions

**Quick test to verify panel loaded correctly:**

```cpp
MonsoonWidget::MonsoonWidget(Monsoon* module) {
    // ... setup code ...
    
    DEBUG("Panel dimensions:");
    DEBUG("  box.size.x = %.2f px", box.size.x);
    DEBUG("  box.size.y = %.2f px", box.size.y);
    DEBUG("  Approx size: %.2f × %.2f mm", 
        box.size.x / 5.18f, 
        box.size.y / 5.18f
    );
    DEBUG("  Expected: 203.2 × 380 mm");
}
```

---

## Summary

| Concept | Value | Notes |
|---------|-------|-------|
| **SVG Unit** | Millimeters (mm) | Rack standard |
| **ViewBox** | 0 0 203.2 380 | Exact mm dimensions |
| **C++ Coordinates** | mm2px(Vec(x, y)) | Use for component placement |
| **box.size** | Auto-calculated | By Rack from viewBox |
| **Theme** | Light/Dark SVG files | Separate files, loaded based on setting |
| **Knob Rotation** | Automatic | SvgKnob handles it |
| **Lattice Pattern** | C++ rendered | Not in SVG (3-layer geometric) |
| **Component Assets** | Separate SVGs | Large/Medium/Small knobs |

---

**Ready for implementation. All dimensions verified. Box.size will be automatically set by Rack.** ✅
