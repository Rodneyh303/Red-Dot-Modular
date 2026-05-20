# Red Dot Modular - Complete Panel Architecture & Design

## System Overview

Red Dot Modular is organized as a polyphonic stochastic sequencer ecosystem with clear separation of concerns:

```
                         ┌─────────────────────────────┐
                         │      MONSOON (Main)         │
                         │   Mono Stochastic Seq       │
                         └──────────────┬──────────────┘
                                        │
                ┌───────────────────────┼───────────────────────┐
                │                       │                       │
      ┌─────────▼────────┐    ┌────────▼────────┐   ┌─────────▼────────┐
      │ INTERCHANGE      │    │ SANDS (Mono)    │   │ STRAITS (East)   │
      │ Scale Modulator  │    │ Pattern DNA     │   │ Voices 1-8       │
      │ 24 HP            │    │ 12 HP           │   │ 24 HP            │
      └──────────────────┘    └─────────────────┘   └──────────────────┘
                                                             │
                                                    ┌────────▼────────┐
                                                    │ STRAITS (West)  │
                                                    │ Voices 9-16     │
                                                    │ 24 HP           │
                                                    └─────────────────┘

Optional:
  ┌──────────────────────┐
  │ STRAITS SANDS (East) │ (Per-voice DNA expander - Voices 1-8)
  │ 24 HP                │
  └──────────────────────┘
  
  ┌──────────────────────┐
  │ STRAITS SANDS (West) │ (Per-voice DNA expander - Voices 9-16)
  │ 24 HP                │
  └──────────────────────┘
```

### Module Responsibilities

| Module | HP | Role | Voice Control |
|--------|-----|------|--------------|
| **Monsoon** | 34 | Main sequencer engine | Mono (broadcast to all poly channels) |
| **Interchange** | 24 | Scale/quantization | Per-voice or global |
| **Sands (Mono)** | 12 | DNA pattern control (mono) | Global rhythm/melody/octave DNA |
| **Straits East** | 12 | Polyphonic expansion | Voices 1-8 gate/CV/accent outputs |
| **Straits West** | 12 | Polyphonic expansion | Voices 9-16 gate/CV/accent outputs |
| **StraitsEastSands** | 24 | Per-voice DNA (East) | Voices 1-8 detailed DNA control |
| **StraitsWestSands** | 24 | Per-voice DNA (West) | Voices 9-16 detailed DNA control |
| **StraitsSands (Macro)** | 16 | Global DNA interpolation | Global Rest/Melody/Octave interp |

---

## Panel Specifications

### MONSOON Main Sequencer (34 HP, 172.72mm)

**Files:**
- `Monsoon_panel_light.svg`
- `Monsoon_panel_dark.svg`

**Layout:**
```
Header: Title, Branding
Row 1:  [NOTE_VALUE] [VARIATION] [LEGATO] [REST] [ACCENT] [TRANSPOSE]
Row 2:  [BPM] [PATTERN_LEN] [PATTERN_OFF] [MODE]
Row 3:  Clock/Reset/Gate inputs (5 jacks)
Bottom: Outputs (11 jacks: seed, run, reset, gate, cv, legato, tie, accent, leg|tie, + 2 more)

Key Features:
  • 6 main knobs (stochastic parameters)
  • 3 sequencer knobs (timing/pattern)
  • 5 modulation inputs
  • 11 gate/CV outputs (NEW: legato, tie, accent, leg|tie)
  • Expander hint (right edge)
```

**Critical Additions (from recent merges):**
- Accent knob + accent output
- Legato output (separate)
- Tie output (separate)
- Combined Legato|Tie output

---

### INTERCHANGE Scale Modulator (24 HP, 120mm)

**Files:**
- `Interchange_panel_light.svg`
- `Interchange_panel_dark.svg`

**Layout:**
```
Header: Title, Branding
Row 1:  [MODE Selector] [SCALE Knob]
Grid:   8 voice rows × per-voice controls:
        • Voice # label
        • 3 scale selection buttons
        • Scale selector knob (8-position)
        • Output jack (CV)
        • Activity light
Bottom: Master quantize button, Master output

Dimensions:
  • Height per voice row: ~34mm
  • Total: 8 rows (voices 1-8)
  • Compact, dense matrix layout
  
Key Features:
  • Per-voice scale selection
  • 8 parallel scale outputs
  • Master global quantization mode
```

---

### SANDS Mono DNA Sequencer (12 HP, 60mm)

**Status:** Existing (not redesigned)
**Purpose:** Global DNA control for mono sequencer channel
**Controls:** 
- 3 DNA knobs (Rest/Melody/Octave length)
- 3 DNA knobs (Rest/Melody/Octave offset)
- 3 DNA knobs (Rest/Melody/Octave rotation)
- Scramble/Reset buttons
- Gate inputs

---

### STRAITS EAST/WEST Polyphony Expansion (12 HP each, 60mm)

**Status:** Existing (not redesigned)
**Purpose:** Separate voice outputs for polyphonic sequencing
**Layout:**
- East: Voices 1-8 outputs
- West: Voices 9-16 outputs
- Each: Gate, CV, Accent per voice

---

### STRAITS EAST SANDS Per-Voice DNA (24 HP, 120mm)

**Files:**
- `StraitsEastSands_panel_light.svg`
- `StraitsEastSands_panel_dark.svg`

**Layout:**
```
Header: Title, Branding (east | voices 1-8)

Column Headers:
  V | R (Rest) | M (Melody) | O (Octave) | I (Interp) | Ctl

Voice Grid (8 rows):
  Row 1-8:
    • Voice # (left)
    • Rest DNA: L/O/R knobs (3 per voice)
    • Melody DNA: L/O/R knobs (3 per voice)
    • Octave DNA: L/O/R knobs (3 per voice)
    • Interpolation: R/M/O sliders (3 per voice)
    • Control buttons (Scramble, Reset)

Master Controls (bottom):
  • Scramble All button
  • Reset All button
  
Dimensions:
  • Width: 120mm (24 HP)
  • Height: 380mm (3U)
  • Voice row spacing: 40mm
  • Total controls: ~104 per module

Design Principles:
  • Dense, wide layout inspired by Mind Meld mixers
  • Reduced from 15 to 8 voices for better ergonomics
  • Controls spread horizontally (12+ per voice)
  • Voice numbers anchor left column
  • Master controls separate at bottom
  • Peranakan geometric lattice overlay
```

---

### STRAITS WEST SANDS Per-Voice DNA (24 HP, 120mm)

**Files:**
- `StraitsWestSands_panel_light.svg`
- `StraitsWestSands_panel_dark.svg`

**Layout:** Identical to East, but:
- Header: "west | voices 9-16"
- Controls for voices 9-16 instead of 1-8
- Same grid and control structure
- Same master controls

---

### STRAITS SANDS MACRO Global Interpolation (16 HP, 80mm)

**Files:**
- `StraitsSands_panel_light.svg`
- `StraitsSands_panel_dark.svg`

**Layout:**
```
Header: Title, Branding

3-Column Layout: Rest | Melody | Octave

Each column:
  • DNA Length knob
  • DNA Offset knob
  • DNA Rotation knob
  • Interpolation slider (vertical)

Master Controls (bottom):
  • Scramble All button
  • Reset All button
  • Scramble gate input
  • Reset gate input

Dimensions:
  • Width: 80mm (16 HP)
  • Height: 380mm (3U)
  • Compact, macro-level control
```

---

## Peranakan Geometric Lattice Pattern

### Technical Implementation

The lattice is rendered by **C++ at runtime** (not baked into SVG), providing:

**Layer A: Primary Orthogonal Grid**
- Vertical + horizontal lines
- Creates foundational square lattice
- Spacing: `width / 24.0f` mm
- Color: Primary (stronger opacity)
- Stroke weight: `width / 900.0f`

**Layer B: Secondary Diagonal Overlay**
- Forward-leaning (/) and backward-leaning (\) diagonals
- Creates diamond pattern on top of grid
- Spacing: `spacing × 1.5f` (sparser than primary)
- Color: Secondary (reduced opacity)
- Stroke weight: `width / 1000.0f` (thinner)

**Layer C: Intersection Nodes**
- Micro-dots at primary grid intersections
- Visual anchor points
- Radius: `width / 550.0f`
- Color: Accent (highest opacity)
- Placement: Every primary grid intersection

### Color Palette

**Dark Mode (Cyberpunk Crimson):**
```
Primary Grid:      #dc2626 @ 16% opacity (0x28 alpha)
Secondary Overlay: #dc2626 @ 9% opacity (0x18 alpha)
Intersection Dots: #dc2626 @ 23% opacity (0x3a alpha)
```

**Light Mode (Laboratory Watermark):**
```
Primary Grid:      #e4e4e7 @ 44% opacity (0x70 alpha)
Secondary Overlay: #e4e4e7 @ 31% opacity (0x50 alpha)
Intersection Dots: #e4e4e7 @ 60% opacity (0x99 alpha)
```

### Theme Switching

- Automatic based on VCV Rack theme toggle
- Real-time color update (no file I/O)
- Consistent crimson/slate color palette across both themes
- High contrast maintained for accessibility

---

## Interpolation Logic

### Spread Interpolation

**Algorithm:** Per-voice, per-dimension blending

```cpp
// Pseudo-code for interpolation in process():
for (int v = 0; v < numConnectedVoices; v++) {
    for (int dim = 0; dim < 3; dim++) {  // Rest, Melody, Octave
        
        // Calculate average of all voice values
        float average = calculateAverage(allVoices, dim);
        
        // Get interpolation amount for this voice
        float interp = interpSliders[v][dim];  // Range [0, 1]
        
        // Blend: 0.0 = per-voice, 1.0 = average
        dnaWindow[v][dim] = lerp(voiceDNA[v][dim], average, interp);
    }
}
```

**Key Features:**
- Independent interpolation per dimension (Rest/Melody/Octave)
- Per-voice control (8+ sliders per module)
- Works with `min(requestedVoices, connectedVoices)`
- Enables rich timbral variations

---

## Design Evolution Timeline

### Phase 1: Foundation ✅
- Created peranakan lattice pattern system
- Designed initial StraitsSands macro panels
- Established color palette & typography

### Phase 2: Deep Expanders (Initial) ✅
- Created DeepStraitsSands monolithic design (15 voices)
- Comprehensive documentation & coordinates

### Phase 3: Optimization (Current) ✅
- Split DeepStraitsSands → StraitsEastSands + StraitsWestSands
- Reduced voices per module: 15 → 8 (better ergonomics)
- Increased horizontal density: 9 controls → 12+ per voice
- Improved peranakan lattice: diamond only → orthogonal + diagonal + nodes
- Maintained consistent aesthetic across all modules

### Phase 4: Integration (Next)
- Widget implementation for all new panels
- Compilation and testing
- Rack verification with lattice overlay
- Physical mockup fabrication

---

## File Organization

```
res/
├── panels/
│   ├── Monsoon_panel_light.svg              ✅ (172.72mm × 380mm)
│   ├── Monsoon_panel_dark.svg               ✅
│   ├── Interchange_panel_light.svg          ✅ (120mm × 380mm)
│   ├── Interchange_panel_dark.svg           ✅
│   ├── Sands_panel_light.svg                (existing, 60mm × 380mm)
│   ├── Sands_panel_dark.svg                 (existing)
│   ├── StraitsEast_panel_light.svg          (existing, 60mm × 380mm)
│   ├── StraitsEast_panel_dark.svg           (existing)
│   ├── StraitWest_panel_light.svg           (existing, 60mm × 380mm)
│   ├── StraitWest_panel_dark.svg            (existing)
│   ├── StraitsSands_panel_light.svg         ✅ (80mm × 380mm)
│   ├── StraitsSands_panel_dark.svg          ✅
│   ├── StraitsEastSands_panel_light.svg     ✅ (120mm × 380mm)
│   ├── StraitsEastSands_panel_dark.svg      ✅
│   ├── StraitsWestSands_panel_light.svg     ✅ (120mm × 380mm)
│   └── StraitsWestSands_panel_dark.svg      ✅
├── DESIGN_SYSTEM.svg                        (specifications)
├── SVG_ASSETS_GUIDE.md                      (asset creation guide)
├── DEEPSTRAITSSANDS_DESIGN_GUIDE.md         (initial design, now archived)
└── DEEPSTRAITSSANDS_COORDINATES.md          (initial coords, now archived)
```

---

## Aesthetic Philosophy

### Peranakan Modernism
- Inspired by geometric peranakan tile patterns
- Orthogonal grids + diagonal overlays
- Creates depth without visual chaos
- Micro-dots anchor the eye

### Cyberpunk Minimalism
- Crimson (#dc2626) accent red (consistent across themes)
- Dark backgrounds (charcoal #232323)
- Light typography (slate #e4e4e7)
- Technical monospace font (+0.5-1px letter-spacing)

### Functional Density
- Wide panel layouts (24-34 HP for expanders)
- Efficient space utilization (8-9 rows)
- Controls packed horizontally (12+ per voice)
- Visual hierarchy guides navigation

### Accessibility
- High contrast (WCAG AA+)
- Voice number anchor column (vertical scanning)
- Column headers (horizontal scanning)
- Master controls clearly separated

---

## Next Steps

### Immediate (Integration)
1. Finalize Monsoon & Interchange panels
2. Commit lattice improvement + all new SVG panels
3. Create widget classes for new modules
4. Integration testing in Rack

### Short-term (Refinement)
1. Physical mockup fabrication (300 DPI prints)
2. Component placement verification
3. Hardware compatibility testing
4. User feedback incorporation

### Long-term (Expansion)
1. Additional expander modules (e.g., Sands stereo version)
2. Preset management UI
3. Performance/optimization improvements
4. Community contribution guidelines

---

**Design System Version:** 2.0 (Improved Lattice + Split Expanders)
**Last Updated:** 2026-05-20
**Status:** Ready for widget implementation & Rack integration
