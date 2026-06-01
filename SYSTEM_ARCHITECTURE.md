# Red Dot Modular - Complete System Architecture & Specifications

## System Overview

Red Dot Modular is a **polyphonic stochastic sequencer ecosystem** with clear voice layering and modulation capabilities.

### Voice Architecture

```
MONSOON (Mono Voice = Voice 1)
    ↓ Outputs (Gate, Note, CV, Accent, Legato, Tie)
    ↓
STRAITS EAST (Voices 2-8 expansion)
    ↓ Adds 7 voices (voices 2,3,4,5,6,7,8) 
    ↓
STRAITS WEST (Voices 9-16 expansion)
    ↓ Adds 8 voices (voices 9,10,11,12,13,14,15,16)
    ↓
Total: 1 (mono) + 7 (east) + 8 (west) = 16 voices maximum
```

### Module Hierarchy

```
PRIMARY:
  MONSOON (40 HP) - Main mono sequencer + poly output control

MODULATION:
  INTERCHANGE (24 HP) - Modulates Monsoon's note/octave sliders
                        (Does NOT operate per-voice; affects mono sequencer only)

EXPANSION - Voice Output Distribution:
  STRAITS EAST (12 HP) - Outputs voices 2-8 (gate/CV/accent)
  STRAITS WEST (12 HP) - Outputs voices 9-16 (gate/CV/accent)

CONTROL - DNA Pattern Control:
  SANDS MONO (12 HP) - Global mono DNA (Rest/Melody/Octave)
  
  STRAITS EAST SANDS (24 HP) - Per-voice DNA for voices 2-8
                               (Rest/Melody/Octave length/offset/rotation per voice)
  
  STRAITS WEST SANDS (24 HP) - Per-voice DNA for voices 9-16
                               (Rest/Melody/Octave length/offset/rotation per voice)
  
  STRAITS SANDS MACRO (16 HP) - Global interpolation for all 16 voices
                                (Blend per-voice toward average or mono)
```

---

## Module Specifications

### MONSOON Main Sequencer (40 HP, 203.2mm × 380mm)

**Role:** Voice 1 (mono). Generates stochastic notes, rhythms, accents. Controls all 16 voices when Straits expanders connected.

**Architecture:**
```
┌─ Stochastic Engine ─────────────────┐
│ • Note Value knob (random note)      │
│ • Variation knob (affect amount)     │
│ • Legato knob (note legato time)     │
│ • Rest knob (rest probability)       │
│ • Accent knob (accent probability)   │
│ • Transpose knob (base pitch shift)  │
└─────────────────────────────────────┘

┌─ Pattern Engine ────────────────────┐
│ • BPM knob (tempo)                  │
│ • Pattern Length knob (sequence len) │
│ • Pattern Offset knob (shift)       │
│ • Mode selector (timing mode)       │
└─────────────────────────────────────┘

┌─ DNA Control ──────────────────────┐
│ • Rest Length/Offset/Rotation       │
│ • Melody Length/Offset/Rotation     │
│ • Octave Length/Offset/Rotation     │
│ • Per-voice Spread sliders (R/M/O)  │
└─────────────────────────────────────┘

┌─ I/O ──────────────────────────────┐
│ Inputs:  Clock, Reset, Gate1, Gate2, CV │
│ Outputs: Gate, Note, CV, Accent,    │
│          Legato, Tie, Leg|Tie + 2   │
└─────────────────────────────────────┘
```

**Unique Aspect:** Monsoon drives ALL voice behavior via DNA controls. When connected to Straits expanders, Monsoon's DNA applies to all 16 voices.

---

### INTERCHANGE Modulation Expander (24 HP, 121.92mm × 380mm)

**Role:** Modulates Monsoon's pitch controls WITHOUT per-voice specifics.

**Key:** INTERCHANGE does NOT operate on individual voices. It modulates the **sliders on Monsoon** that all voices inherit.

**Architecture:**
```
┌─ Modulation Controls ──────────────┐
│ • Mode selector (modulation type)   │
│ • 8 scale selection knobs (1 per slot) │
│ • Scale selector knob               │
│                                     │
│ (Note: These modulate Monsoon's     │
│  Note/Octave controls globally)     │
└─────────────────────────────────────┘

┌─ Outputs ──────────────────────────┐
│ • 8 CV outputs (modulation signals) │
│ • Master quantize button            │
│ • Master output jack                │
└─────────────────────────────────────┘
```

**Musical Purpose:**
- Modulate Monsoon's note-value knob via external controls
- Add harmonic constraints (scales, quantization)
- Create global transposition/modulation without voice-level control

---

### STRAITS EAST Output Expander (12 HP, 60mm × 380mm)

**Role:** Outputs voices 2-8 (7 voices, added to Monsoon's voice 1).

**Architecture:**
```
┌─ Outputs for 7 Voices ─────────────┐
│ Each voice has 3 outputs:          │
│  • Gate (voice active)              │
│  • CV (pitch)                       │
│  • Accent (accent signal)           │
│                                     │
│ Voice 2: Gate, CV, Accent          │
│ Voice 3: Gate, CV, Accent          │
│ Voice 4: Gate, CV, Accent          │
│ Voice 5: Gate, CV, Accent          │
│ Voice 6: Gate, CV, Accent          │
│ Voice 7: Gate, CV, Accent          │
│ Voice 8: Gate, CV, Accent          │
└─────────────────────────────────────┘
```

**Visual Distinction:**
- **Left edge:** Red vertical line (separates East from rest)
- Layout: Voices 2-8 in vertical column

---

### STRAITS WEST Output Expander (12 HP, 60mm × 380mm)

**Role:** Outputs voices 9-16 (8 voices, added to Straits East's 7 voices).

**Architecture:**
```
┌─ Outputs for 8 Voices ─────────────┐
│ Each voice has 3 outputs:          │
│  • Gate (voice active)              │
│  • CV (pitch)                       │
│  • Accent (accent signal)           │
│                                     │
│ Voice 9: Gate, CV, Accent          │
│ Voice 10: Gate, CV, Accent         │
│ Voice 11: Gate, CV, Accent         │
│ Voice 12: Gate, CV, Accent         │
│ Voice 13: Gate, CV, Accent         │
│ Voice 14: Gate, CV, Accent         │
│ Voice 15: Gate, CV, Accent         │
│ Voice 16: Gate, CV, Accent         │
└─────────────────────────────────────┘
```

**Visual Distinction:**
- **Right edge:** Red vertical line (separates West from rest)
- Layout: Voices 9-16 in vertical column

---

### SANDS MONO DNA Control (12 HP, 60mm × 380mm)

**Role:** Global mono DNA control (applies to all 16 voices).

**Architecture:**
```
┌─ Rest DNA ──────────────────────────┐
│ • Length knob (sequence length)      │
│ • Offset knob (sequence shift)       │
│ • Rotation knob (sequence rotate)    │
└─────────────────────────────────────┘

┌─ Melody DNA ────────────────────────┐
│ • Length knob                       │
│ • Offset knob                       │
│ • Rotation knob                     │
└─────────────────────────────────────┘

┌─ Octave DNA ────────────────────────┐
│ • Length knob                       │
│ • Offset knob                       │
│ • Rotation knob                     │
└─────────────────────────────────────┘

┌─ Controls ──────────────────────────┐
│ • Scramble All (randomize patterns) │
│ • Reset All (reset patterns)        │
│ • Gates for both                    │
└─────────────────────────────────────┘
```

---

### STRAITS EAST SANDS DNA Expander (24 HP, 121.92mm × 380mm)

**Role:** Per-voice DNA control for voices 2-8 (7 voices).

**Architecture:**
```
┌─ Per-Voice Controls (7 rows) ──────┐
│ Voice 2-8:                         │
│  • Rest: Length, Offset, Rotation  │
│  • Melody: Length, Offset, Rotation│
│  • Octave: Length, Offset, Rotation│
│  • Spread sliders: R, M, O         │
│  • Scramble, Reset buttons         │
└─────────────────────────────────────┘

┌─ Master Controls ──────────────────┐
│ • Scramble All (all 7 voices)      │
│ • Reset All (all 7 voices)         │
│ • Scramble gate input              │
│ • Reset gate input                 │
└─────────────────────────────────────┘
```

**Visual Distinction:**
- **Left edge:** Red vertical line (separates East from rest)
- Voice numbers anchor left column (visual scanning aid)

**Total Controls:** ~98 (7 voices × 12 DNA + 2 spreads per voice + master)

---

### STRAITS WEST SANDS DNA Expander (24 HP, 121.92mm × 380mm)

**Role:** Per-voice DNA control for voices 9-16 (8 voices).

**Architecture:**
```
┌─ Per-Voice Controls (8 rows) ──────┐
│ Voice 9-16:                        │
│  • Rest: Length, Offset, Rotation  │
│  • Melody: Length, Offset, Rotation│
│  • Octave: Length, Offset, Rotation│
│  • Spread sliders: R, M, O         │
│  • Scramble, Reset buttons         │
└─────────────────────────────────────┘

┌─ Master Controls ──────────────────┐
│ • Scramble All (all 8 voices)      │
│ • Reset All (all 8 voices)         │
│ • Scramble gate input              │
│ • Reset gate input                 │
└─────────────────────────────────────┘
```

**Visual Distinction:**
- **Right edge:** Red vertical line (separates West from rest)
- Voice numbers anchor left column (visual scanning aid)

**Total Controls:** ~112 (8 voices × 12 DNA + 2 spreads per voice + master)

---

### STRAITS SANDS MACRO Global Interpolation (16 HP, 80mm × 380mm)

**Role:** Global interpolation for all 16 voices (blends toward average or mono).

**Architecture:**
```
┌─ 3-Column Layout ──────────────────┐
│ REST | MELODY | OCTAVE             │
│                                    │
│ Each column:                       │
│  • DNA Length knob                 │
│  • DNA Offset knob                 │
│  • DNA Rotation knob               │
│  • Interpolation slider (vertical) │
└─────────────────────────────────────┘

┌─ Master Controls ──────────────────┐
│ • Spread Mode selector (context)   │
│ • Scramble All                     │
│ • Reset All                        │
│ • Scramble gate input              │
│ • Reset gate input                 │
└─────────────────────────────────────┘
```

**Spread Modes:**
1. **Poly Average:** Blend toward average of all 16 voices
2. **Mono Source:** Blend toward Monsoon's (mono) values

---

## Signal Flow

### From Monsoon to Poly Voices

```
Monsoon (Voice 1)
    ↓
    ├─ Gate output ──→ Straits East/West
    ├─ Note output ──→ Straits East/West
    ├─ CV output ──→ Straits East/West
    ├─ Accent output ──→ Straits East/West
    ├─ Legato output ──→ Straits East/West
    └─ Tie output ──→ Straits East/West
    
These outputs are REPLICATED to all connected voices
(Straits East outputs them for voices 2-8)
(Straits West outputs them for voices 9-16)
```

### DNA Modulation

```
Monsoon DNA Controls (Rest/Melody/Octave)
    ↓
Applied to Voice 1 (mono)
    ↓
Replicated to Voices 2-8 (via Straits East Sands or mono DNA)
Replicated to Voices 9-16 (via Straits West Sands or mono DNA)
    ↓
Per-voice DNA (East/West Sands) can OVERRIDE mono DNA
    ↓
Global Spread (Macro Sands) can BLEND voices toward average/mono
```

### Modulation via Interchange

```
Interchange (Modulation Expander)
    ↓
Modulates Monsoon's Note/Octave sliders
    ↓
This affects ALL voices (1-16)
    ↓
Does NOT provide per-voice control
```

---

## Module Connection Example

### Minimal Setup (Voices 1-8)
```
Monsoon ─────→ Straits East
  ↓              ↓
 DNA            Outputs (Gate/CV/Accent × 7)
```

### Full 16-Voice Setup
```
Monsoon ─────→ Straits East ─────→ Straits West
  ↓              ↓                   ↓
 DNA            Outputs             Outputs
                (voices 2-8)        (voices 9-16)
  ↓
Sands Mono (optional: global DNA)
  ↓
Straits East Sands (optional: per-voice DNA for voices 2-8)
  ↓
Straits West Sands (optional: per-voice DNA for voices 9-16)
  ↓
Straits Sands Macro (optional: global interpolation blend)
  ↓
Interchange (optional: modulate Monsoon pitch)
```

---

## Control Hierarchy

### Top Level (Monsoon)
1. Stochastic parameters (Note, Variation, Legato, Rest, Accent, Transpose)
2. Pattern engine (BPM, Length, Offset, Mode)
3. DNA controls (Rest/Melody/Octave L/O/R)

### Mid Level (DNA Expansion)
1. Sands Mono: Override Monsoon DNA globally
2. East/West Sands: Override Monsoon DNA per-voice

### Fine Level (Interpolation)
1. Per-voice Spread sliders (blend toward source)
2. Macro Spread: Global interpolation

### External Modulation
1. Interchange: Modulate Monsoon's pitch controls

---

## Visual Design Language

### Panel Styling
- **Background:** #1a1a1a (dark charcoal, professional)
- **Component Primary:** #3a3a3a (light charcoal)
- **Component Fill:** #2f2f2f (mid charcoal)
- **Accents:** #1a1a1a (center rings), #dc2626 (crimson indicator)
- **Guide Marks:** #444 @ 60% opacity (dashed circles for component placement)
- **Text:** #666 (light gray)

### Red Line Accents
- **Straits East:** Vertical red line on LEFT edge (separates output group)
- **Straits West:** Vertical red line on RIGHT edge (separates output group)
- **Straits East Sands:** Vertical red line on LEFT edge (matches East)
- **Straits West Sands:** Vertical red line on RIGHT edge (matches West)
- **Color:** #dc2626 (crimson, visible in both light and dark modes)
- **Position:** Edge of panel, full height, 1-2px wide

### Component Aesthetics
- **Knobs:** Matte metallic (brushed aluminum, not shiny)
- **Ports:** Same matte aesthetic, subtle rim light
- **Depth:** Rim lighting + inset shadow on all components
- **Indicator:** Crimson line on all knobs (rotates with parameter)
- **Consistency:** Same knob/port style across all modules

---

## Size Reference

| Module | HP | Width (mm) | Height (mm) | Voices |
|--------|-----|-----------|------------|---------|
| Monsoon | 40 | 203.2 | 380 | 1 (voice 1) |
| Interchange | 24 | 121.92 | 380 | — (modulation only) |
| Straits East | 12 | 60.96 | 380 | 7 (voices 2-8) |
| Straits West | 12 | 60.96 | 380 | 8 (voices 9-16) |
| Sands Mono | 12 | 60.96 | 380 | — (global control) |
| Straits East Sands | 24 | 121.92 | 380 | 7 (voices 2-8) |
| Straits West Sands | 24 | 121.92 | 380 | 8 (voices 9-16) |
| Straits Sands Macro | 16 | 80 | 380 | — (interpolation) |

**Total:** 164 HP (8 modules)

---

## Technical Specifications

### SVG Panels
- **ViewBox Unit:** Millimeters (Rack standard)
- **Panel Height:** 380mm (standard 3U Eurorack)
- **Component Guides:** Dashed circles indicating knob/port placement
- **Clean SVG:** No Inkscape metadata, no rendering errors

### C++ Widgets
- **box.size:** Automatically set by Rack from SVG viewBox (no manual calculation)
- **Component Positioning:** Uses mm2px(Vec(x_mm, y_mm)) helper
- **Knob Rotation:** -135° to +135° (270° total range)
- **Theme Support:** Separate SVG files for light/dark (optional)

### Rendering
- **Peranakan Lattice:** 3-layer geometric pattern (C++ rendered, theme-aware)
- **Component Lighting:** Rim light + inset shadow (SVG-rendered)
- **Activity Lights:** addLight() in C++ (hardware-rendered glow)

---

## Design Philosophy

### Clarity
- **Visual hierarchy:** Controls organized by function (stochastic, pattern, DNA)
- **Voice numbering:** Left-aligned for vertical scanning
- **Column headers:** Guides horizontal scanning
- **Section dividers:** Clear separation between major control areas

### Usability
- **Ergonomic spacing:** 40 HP Monsoon allows comfortable access to all controls
- **Red accents:** East/West distinction guides patch cabling
- **Consistent aesthetic:** Matte metallic knobs create unified visual language
- **Professional appearance:** Matches modern Eurorack design standards

### Flexibility
- **Modular expansion:** Each Straits expander adds voices independently
- **Optional DNA control:** Can use mono DNA or per-voice DNA
- **Spread modes:** Two musical approaches (poly average vs mono source)
- **Modulation via Interchange:** Add external pitch control without voice-level complexity

---

## Status

✅ Architecture defined  
✅ Module responsibilities clarified  
✅ Voice layering explained (1 + 7 + 8)  
✅ Visual design language established (red lines, matte aesthetic)  
✅ All specifications documented  

**Next:** Clean panel SVGs for all 8 modules + C++ widget code
