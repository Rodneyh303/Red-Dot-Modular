# Red Dot Modular - SVG Assets Library

## Overview

This directory contains SVG design assets for the Red Dot Modular Peranakan Tech Collection. All designs follow a cohesive visual system inspired by peranakan tiles and cyberpunk minimalism.

## Design System

### Color Palette

**Dark Theme:**
- Background: `#232323` (deep charcoal)
- Text: `#e4e4e7` (light slate)
- Accent: `#dc2626` (cyberpunk crimson)
- Secondary: `#444444` (medium gray)

**Light Theme:**
- Background: `#e6e6e6` (laboratory white)
- Text: `#232323` (dark charcoal)
- Accent: `#dc2626` (cyberpunk crimson, same as dark)
- Secondary: `#cccccc` (light gray)

### Typography

- **Font Family:** System monospace (Courier New fallback)
- **Letter Spacing:** +0.5px for technical precision
- **Font Sizes:**
  - Titles: 11px
  - Headers: 7px
  - Labels: 6px
  - Small labels: 5px

### Module Dimensions (HP to mm)

| Module | Width (HP) | Width (mm) | Viewbox |
|--------|-----------|-----------|---------|
| Monsoon | 34 | 172.72 | `0 0 172.72 380` |
| StraitsSands | 12-16 | 60-80 | `0 0 80 380` |
| DeepStraitsSands | 40-55 | 200-280 | `0 0 280 380` |
| Straits East | 12 | 60 | `0 0 60 380` |
| Strait West | 12 | 60 | `0 0 60 380` |
| Interchange | 24 | 120 | `0 0 120 380` |
| Sands (Mono) | 12 | 60 | `0 0 60 380` |

## File Structure

```
res/
├── DESIGN_SYSTEM.svg          # Design specifications (reference)
├── panels/
│   ├── StraitsSands_panel_light.svg
│   ├── StraitsSands_panel_dark.svg
│   ├── DeepStraitsSands_panel_light.svg
│   ├── DeepStraitsSands_panel_dark.svg
│   ├── MeloDicer_panel_light.svg (Monsoon, existing - refresh planned)
│   ├── MeloDicer_panel_v6.svg (Monsoon, existing - refresh planned)
│   └── [other module panels...]
├── knobs/
│   ├── knob_large.svg (RDM_KnobLarge)
│   ├── knob_medium.svg (RDM_KnobMedium)
│   └── knob_small.svg (RDM_KnobSmall)
└── components/
    ├── jack_pj301m.svg
    ├── button_toggle.svg
    ├── button_push.svg
    ├── light_small.svg
    └── light_medium.svg
```

## Panel Design Guidelines

### Component Placement

Panels use guide marks (dashed circles/rectangles) to show where controls will be placed:

- **Knob Locations:** Dashed circles (radius varies by knob size)
  - Large knobs: `r="8"`
  - Medium knobs: `r="6"`
  - Small knobs: `r="6"`

- **Port Locations:** Dashed circles `r="4.5"`

- **Button Locations:** Dashed rectangles with `rx="2"` (rounded corners)

- **Slider Locations:** Dashed vertical/horizontal rectangles

### Text Labels

Labels are positioned below or beside their controls:
- **Knob labels:** 20px below center
- **Port labels:** 15px below center
- **Button labels:** 25px below center

### Peranakan Lattice Integration

The lattice pattern is **NOT drawn in SVG**. It's rendered by C++ in the widget's `draw()` method using `PeranakanLatticePanel`. This allows:

1. **Dynamic scaling** based on actual module width
2. **Real-time theme switching** (dark/light)
3. **Performance optimization** (GPU-rendered)
4. **Consistent appearance** across all modules

SVG panels should:
- Provide a clean background (light or dark)
- Show component placements
- Include branding and labels
- Let the C++ lattice overlay provide texture

### Example Panel Structure

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 380">
  <!-- Background -->
  <rect width="80" height="380" fill="#e6e6e6"/>  <!-- Light theme -->
  
  <!-- Optional edge highlight for depth -->
  <rect width="80" height="380" fill="url(#panelEdgeGradient)" opacity="0.8"/>
  
  <!-- Header / Branding -->
  <text x="40" y="18" font-size="11" font-weight="bold" text-anchor="middle" fill="#232323">
    MODULE NAME
  </text>
  
  <!-- Component Guide Marks (dashed circles/rectangles) -->
  <circle cx="20" cy="65" r="8" fill="none" stroke="#ccc" stroke-dasharray="2,1"/>
  
  <!-- Labels -->
  <text x="20" y="88" font-size="6" text-anchor="middle" fill="#666">label</text>
  
  <!-- Footer -->
  <text x="40" y="378" font-size="5" text-anchor="middle" fill="#aaa">red dot</text>
</svg>
```

## Creating New Panel Assets

### Step-by-Step Process

1. **Determine Module Dimensions**
   - Calculate width in HP: `width_mm / 5.08`
   - Create viewbox: `0 0 {width_mm} 380`

2. **Create Light Theme Panel**
   - Copy template from `StraitsSands_panel_light.svg`
   - Update module name and branding
   - Place component guide marks based on widget code positions
   - Save as `ModuleName_panel_light.svg`

3. **Create Dark Theme Panel**
   - Copy light version
   - Change background: `#e6e6e6` → `#232323`
   - Change text color: `#232323` → `#e4e4e7`
   - Change guide stroke: `#ccc` → `#444`
   - Save as `ModuleName_panel_dark.svg`

4. **Verify Component Positions**
   - Open widget source code (`*Widget.cpp`)
   - Find all `addParam()`, `addOutput()`, `addInput()` calls
   - Extract X coordinate from `mm2px(Vec(X, Y))`
   - Convert X from pixels back to mm (divide by ~5.5)
   - Verify guide marks match widget positions

5. **Test in Rack**
   - Load module in VCV Rack
   - Verify panel loads without errors
   - Check that lattice pattern appears correctly
   - Confirm component visual alignment
   - Test both themes

### Coordinate Conversion

```
Pixels to MM (for guide mark placement):
  x_mm = x_px / 5.5  (approximate, 96dpi standard)

MM to Rack Viewbox:
  Keep in mm units directly in viewbox
  E.g., viewBox="0 0 80 380" for 80mm wide module
```

## Knob Assets

Standard Eurorack knob sizes:

- **Large (D-shaft):** 18-21mm diameter (controls: Note Value, Variation, Legato, Rest)
- **Medium (3mm shaft):** 14-17mm diameter (controls: Pattern Length, Pattern Offset, Interpol)
- **Small (2mm shaft):** 10-13mm diameter (DNA window controls: Length, Offset, Rotation)

All knobs feature:
- Dark aluminum finish (`#2a2a2a` base, `#1a1a1a` shadows)
- Crimson indicator line (`#dc2626`)
- Bevel highlight for 3D effect
- Subtle gradient texture

### Knob File Naming

```
knob_large.svg        → RDM_KnobLarge / RDM_KnobCreamLarge / RDM_KnobDarkLarge
knob_medium.svg       → RDM_KnobMedium / RDM_KnobCreamMedium / RDM_KnobDarkMedium
knob_small.svg        → RDM_KnobSmall (various variants)
```

## Component Assets

### Ports/Jacks (PJ301M)

- **Diameter:** ~10mm (5mm radius)
- **Colors:** Dark aluminum (`#1a1a1a`) with slight highlight
- **Outline:** Subtle black center hole
- **Placement:** Typically bottom row of panel

### Buttons

- **Toggle Button:** 7×7mm, bezel design
- **Push Button:** 8×8mm diameter, domed face
- **Colors:** `#1a1a1a` body, `#2a2a2a` face

### Light Indicators

- **Small LED:** 5mm diameter (Red, Green, Yellow variants)
- **Medium LED:** 7mm diameter
- **Colors:**
  - Red: `#dc2626`
  - Green: `#22c55e`
  - Yellow: `#eab308`

## Maintenance & Updates

### Updating Existing Panels

When modifying Monsoon panel (`MeloDicer_panel_light.svg` / `MeloDicer_panel_v6.svg`):

1. Keep existing component positions (don't break widget alignment)
2. Update visual styling to match peranakan theme
3. Ensure text labels are clear and legible
4. Test theme switching to confirm both variants work
5. Verify peranakan lattice doesn't obscure important information

### Adding New Modules

For new modules (e.g., future expanders):

1. Create both light and dark theme SVG files
2. Use consistent naming convention: `ModuleName_panel_light.svg` / `_dark.svg`
3. Follow design guidelines (color palette, typography, spacing)
4. Document module dimensions in this README
5. Test thoroughly before merging to main

## Rendering Notes

- **Anti-aliasing:** Important for small text (< 8px). All SVG text should render smoothly
- **Stroke Weight:** Keep strokes thin (0.3-0.5px) to avoid overwhelming the lattice
- **Opacity:** Use transparency cautiously; test against lattice pattern to ensure readability
- **Gradients:** Use subtly; avoid visual noise that competes with lattice

## Tools & Software

**Recommended:**
- **Inkscape** (free, open-source): Full SVG support
- **Adobe Illustrator:** Professional alternative
- **Figma:** Web-based, collaborative design
- **VS Code + SVG Extension:** Quick editing

**Testing:**
- **VCV Rack:** Load and verify modules
- **Librsvg/ImageMagick:** Command-line rendering test
- **Web browsers:** Basic SVG preview

## References

- **Peranakan Design:** Geometric tile patterns, interlocked symmetry
- **Cyberpunk Aesthetic:** High contrast, crimson/charcoal, technical typography
- **VCV Rack Standards:** 5.08mm per HP, 3U height standard
- **SI Units:** All dimensions in mm for consistency with Rack conventions

## Version History

- **v1.0** (Initial): StraitsSands macro panels (light + dark), design system documentation
- **Planned v1.1**: DeepStraitsSands panels, Monsoon panel refresh
- **Planned v1.2**: Knob/component assets, Straits family expanders
- **Planned v2.0**: Complete asset library, all modules

---

**Last Updated:** 2026-05-20  
**Author:** Red Dot Modular Design System  
**License:** TBD (match plugin license)
