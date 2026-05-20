# DeepStraitsSands - Detailed Design & Implementation Guide

## Module Overview

**DeepStraitsSands** is the dense, per-voice DNA control module for advanced sound design. With 180+ controls organized into 15 voice rows, it enables granular stochastic sequence manipulation at the individual voice level.

**Specifications:**
- **Panel Width:** 280mm (~55 HP)
- **Height:** 3U standard (380mm)
- **Total Controls:** 180+ (knobs, sliders, buttons, gates)
- **Voices:** 15 rows (Voices 1-15 in poly sequencer)
- **Theme Support:** Light (#e6e6e6) + Dark (#232323)

---

## Panel Architecture

### Overall Layout Strategy

The 55HP panel is organized as a **data matrix** optimized for dense information display:

```
┌─────────────────────────────────────────────────────────────────┐
│ DEEP STRAITS SANDS                                    ◆ red dot │
│ per-voice dna control                                           │
├─ V ── REST ─── MELODY ─── OCTAVE ─── INTERP ─── CTL ──────────┤
│    L O R    L  O  R     L  O  R     R  M  O   Scr Rst Gate    │
├─────────────────────────────────────────────────────────────────┤
│ 1  ◯ ◯ ◯    ◯  ◯  ◯     ◯  ◯  ◯     ▁  ▁  ▁   ▢  ▢  ◯  ◯  │
│ 2  ◯ ◯ ◯    ◯  ◯  ◯     ◯  ◯  ◯     ▁  ▁  ▁   ▢  ▢  ◯  ◯  │
│ 3  ◯ ◯ ◯    ◯  ◯  ◯     ◯  ◯  ◯     ▁  ▁  ▁   ▢  ▢  ◯  ◯  │
│ ... (13 more rows) ...                                          │
│ 15 ◯ ◯ ◯    ◯  ◯  ◯     ◯  ◯  ◯     ▁  ▁  ▁   ▢  ▢  ◯  ◯  │
├─────────────────────────────────────────────────────────────────┤
│ MASTER: Scramble All ▢  Reset All ▢  Scr-Gate ◯  Rst-Gate ◯  │
└─────────────────────────────────────────────────────────────────┘

Legend:
  ◯ = Knob (small) or Port (jack)
  ▢ = Button
  ▁ = Slider (vertical)
  V = Voice number column
  L = Length, O = Offset, R = Rotation
  R = Rest Interp, M = Melody Interp, O = Octave Interp
```

### Column Organization

**Total Width: 280mm**

| Section | Width (mm) | Count | Spacing | Purpose |
|---------|-----------|-------|---------|---------|
| Voice # | 16 | 1 | — | Voice numbering (1-15) |
| Rest DNA | 60 | 3 knobs | 10mm | Length, Offset, Rotation |
| Melody DNA | 60 | 3 knobs | 10mm | Length, Offset, Rotation |
| Octave DNA | 60 | 3 knobs | 10mm | Length, Offset, Rotation |
| Interp | 55 | 3 sliders | 10mm | Rest, Melody, Octave blending |
| Controls | 25 | 4 objects | 5mm | Scramble btn, Reset btn, 2 gates |
| Margins | 4 | — | 2mm each side | Padding |

**Total: 16 + 60 + 60 + 60 + 55 + 25 + 4 = 280mm** ✓

### Row Organization

**Total Height: 380mm (3U standard)**

| Section | Height (mm) | Count | Spacing | Purpose |
|---------|------------|-------|---------|---------|
| Header | 55 | 1 | — | Title, column headers, sub-labels |
| Voice Rows | 165 | 15 | 11mm | 1 row per voice |
| Master | 30 | 1 | — | Global controls |
| Footer | 50 | 1 | — | Spacing + branding |
| Margins | 80 | — | — | Top + bottom padding |

**Total: 55 + 165 + 30 + 50 + 80 = 380mm** ✓

---

## Component Specifications

### 1. Voice Number Column (16mm wide)

**Purpose:** Visual anchor, voice identification

**Layout per row:**
- Voice number label: Centered at Y = row_y + 5
- Font: Monospace 6px, color dark on light / light on dark
- Sequence: 1, 2, 3, ..., 15

**Spacing:** 11mm between rows (row center to row center)

**Example positions (Y coordinates):**
```
Voice 1:  Y = 65px
Voice 2:  Y = 76px (65 + 11)
Voice 3:  Y = 87px (65 + 22)
...
Voice 15: Y = 219px (65 + 154)
```

### 2. DNA Window Knobs (3 per DNA type × 3 DNA types = 9 per row)

**Purpose:** Control length, offset, rotation for each stochastic dimension

**Knob Specifications:**
- **Large knobs (Length):** Diameter ~7mm
  - Represents window size (1-16)
  - Most visually prominent
  - RDM_KnobMedium size

- **Small knobs (Offset, Rotation):** Diameter ~5.5mm
  - Represents start position (0-15) and rotation amount (0-15)
  - RDM_KnobSmall size

**Colors:**
- Body: Dark aluminum (#2a2a2a)
- Indicator line: Crimson (#dc2626)
- Bevel: Subtle white highlight

**Layout per row:**
```
Rest DNA:
  Length:   X = 20mm (from left edge)
  Offset:   X = 30mm
  Rotation: X = 40mm

Melody DNA:
  Length:   X = 85mm
  Offset:   X = 95mm
  Rotation: X = 105mm

Octave DNA:
  Length:   X = 150mm
  Offset:   X = 160mm
  Rotation: X = 170mm
```

**All at:** Y = row_y (centered vertically on row)

### 3. Interpolation Sliders (3 per row)

**Purpose:** Blend per-voice DNA vs average DNA (independent per dimension)

**Slider Specifications:**
- **Orientation:** Vertical
- **Type:** Fader slider (smooth operation)
- **Width:** 6mm
- **Height:** 14mm per row (tall enough for micro-adjustment)
- **Range:** [0, 1]
  - 0.0 = Per-voice random (independent)
  - 0.5 = 50/50 blend
  - 1.0 = Average random (synchronized)

**Labels:**
- **R** = Rest interpolation
- **M** = Melody interpolation
- **O** = Octave interpolation

**Layout per row:**
```
Rest Interp:   X = 205mm
Melody Interp: X = 215mm
Octave Interp: X = 225mm
```

**All at:** Y = row_y - 7 to row_y + 7 (vertical range)

### 4. Control Buttons (2 per row)

**Purpose:** Trigger scramble/reset for individual voice

**Button Specifications:**
- **Type:** Momentary push button
- **Size:** ~5mm × 5mm
- **Style:** Bezel design with tactile highlight
- **Labels:**
  - Scramble (Scr): Randomize length/offset for all DNA types
  - Reset (Rst): Restore to defaults

**Layout per row:**
```
Scramble Button: X = 245mm
Reset Button:    X = 260mm
```

**All at:** Y = row_y (centered)

### 5. Gate Inputs (2 per row)

**Purpose:** External CV triggers for scramble/reset

**Jack Specifications:**
- **Type:** PJ301M 3.5mm (standard Eurorack)
- **Diameter:** ~5mm
- **Appearance:** Dark aluminum ring with subtle highlight
- **Trigger Logic:**
  - Gate HIGH (>5V): Activate scramble/reset
  - Gate LOW: Inactive

**Labels:**
- Scr-Gate: Scramble gate
- Rst-Gate: Reset gate

**Layout per row:**
```
Scramble Gate: X = 245mm
Reset Gate:    X = 260mm
```

**All at:** Y = row_y + 8 (slightly below buttons)

---

## Header Section (First 55mm)

### Title & Branding

**Main Title:**
- Text: "DEEP STRAITS SANDS"
- Font: Monospace bold, 12px
- Color: Dark text (#232323) on light / Light text (#e4e4e7) on dark
- Position: X = 140mm (center), Y = 18mm
- Letter-spacing: +1px

**Subtitle:**
- Text: "per-voice dna control"
- Font: Monospace, 8px
- Color: Gray (#666 light theme, #999 dark theme)
- Position: X = 140mm (center), Y = 30mm
- Letter-spacing: +0.5px

**Brand Dot:**
- Shape: Filled circle
- Color: Crimson (#dc2626)
- Size: ~2.5mm diameter
- Position: X = 270mm (top right), Y = 10mm

### Column Headers (Single-level)

**Main Headers (Y = 45mm):**
- **V:** Voice number column
- **REST:** Rest DNA controls
- **MELODY:** Melody DNA controls
- **OCTAVE:** Octave DNA controls
- **INTERP:** Interpolation sliders
- **CTL:** Control buttons + gates

Font: Monospace bold, 6px
Color: Dark (#232323) on light / Light (#e4e4e7) on dark

### Sub-headers (Y = 52mm)

**DNA Window Sub-labels:**
```
For each DNA section:
  L = Length
  O = Offset
  R = Rotation
```

**Interpolation Sub-labels:**
```
R = Rest interpolation
M = Melody interpolation
O = Octave interpolation
```

Font: Monospace, 5px
Color: Gray (#999 light, #666 dark)

---

## Master Controls Section (Bottom, Y = 335-350mm)

### Master Section Label
- Text: "MASTER"
- Font: Monospace bold, 6px
- Position: X = 8mm, Y = 350mm
- Color: Dark/light theme aware

### Master Scramble All Button
- Purpose: Randomize all DNA for all voices in one action
- Size: 8×8mm, rounded corners (rx=1mm)
- Position: X = 30mm, Y = 345mm
- Label: "scramble-all" (5px, Y = 362mm)

### Master Reset All Button
- Purpose: Restore all DNA to defaults for all voices
- Size: 8×8mm, rounded corners
- Position: X = 95mm, Y = 345mm
- Label: "reset-all" (5px, Y = 362mm)

### Master Scramble Gate Input
- Purpose: External trigger for master scramble
- Type: PJ301M jack
- Size: ~6mm diameter
- Position: X = 160mm, Y = 349mm
- Label: "scr-gate" (5px, Y = 366mm)

### Master Reset Gate Input
- Purpose: External trigger for master reset
- Type: PJ301M jack
- Size: ~6mm diameter
- Position: X = 225mm, Y = 349mm
- Label: "rst-gate" (5px, Y = 366mm)

---

## Visual Design Guidelines

### Spacing & Alignment

**Horizontal Spacing:**
- Column gap: 10mm between DNA sections
- Knob-to-knob: 10mm (center to center)
- Slider width: 6mm with 1-2mm margins
- Button padding: 5mm from slider edge

**Vertical Spacing:**
- Row height: 11mm (center to center)
- Tight packing for density
- Visual rhythm aids navigation

**Margins:**
- Left edge: 2mm
- Right edge: 2mm
- Top edge: 55mm (header)
- Bottom edge: 80mm (footer + master controls)

### Visual Hierarchy

**Prominence Order:**
1. **Voice Numbers** (anchor, always visible)
2. **DNA Knobs** (primary controls, large + medium sizes)
3. **Interpolation Sliders** (important but secondary)
4. **Buttons** (utility, less frequent use)
5. **Gates** (integration, advanced users)
6. **Labels** (orientation guides, small text)

### Color Coding

**Theme Support:**
- Light Theme: #e6e6e6 background, #232323 text
- Dark Theme: #232323 background, #e4e4e7 text
- Accent Red: #dc2626 (both themes, consistent crimson)

**Component Colors:**
- Knobs: Dark aluminum body + crimson indicator (standard)
- Buttons: Bezel design with subtle highlights
- Jacks: Dark aluminum ring with soft highlight
- Sliders: Aluminum finish with knob indicator
- Labels: Monospace, theme-aware colors

### Typography

**Font Family:** System monospace (Courier New fallback)
**Letter-spacing:** +0.5-1px for technical precision

**Font Sizes:**
- Title: 12px bold
- Headers: 6-8px bold
- Sub-headers: 5px
- Labels: 5px (minimal, guideline only)

---

## Component Placement Algorithm

For developers integrating this panel into the widget code:

### Converting Widget Coordinates to SVG Guide Marks

**Widget code example:**
```cpp
float restLenX = 20.0f;  // mm from left
float restLenY = baseVoiceY + (v * 11.0f);  // mm from top
addParam(createParamCentered<RDM_KnobMedium>(mm2px(Vec(restLenX, restLenY)), mod, PARAM));
```

**SVG Placement:**
```xml
<!-- Place guide circle at exact coordinates -->
<circle cx="20" cy="65" r="3.5" fill="none" stroke="#ccc" stroke-dasharray="2,1"/>
<!-- Voice 1 at Y=65, Voice 2 at Y=76 (65 + 11), etc. -->
```

### Voice Row Y-Coordinate Lookup

```
Voice 1:  Y = 60  (header at 45, sub-header at 52, row starts at 60)
Voice 2:  Y = 71  (60 + 11)
Voice 3:  Y = 82  (60 + 22)
Voice 4:  Y = 93  (60 + 33)
Voice 5:  Y = 104 (60 + 44)
Voice 6:  Y = 115 (60 + 55)
Voice 7:  Y = 126 (60 + 66)
Voice 8:  Y = 137 (60 + 77)
Voice 9:  Y = 148 (60 + 88)
Voice 10: Y = 159 (60 + 99)
Voice 11: Y = 170 (60 + 110)
Voice 12: Y = 181 (60 + 121)
Voice 13: Y = 192 (60 + 132)
Voice 14: Y = 203 (60 + 143)
Voice 15: Y = 214 (60 + 154)

Master:   Y = 345 (335 + 10)
```

**Formula:** `voice_y = 60 + (voice_index * 11)` where voice_index = 0 for Voice 1

---

## Navigation & Usability

### Visual Organization

**Scanning Patterns:**
1. **Vertical Scanning:** Voice numbers (left) guide eye down rows
2. **Horizontal Scanning:** DNA controls group left-to-right (Rest → Melody → Octave)
3. **Secondary Scanning:** Interpolation sliders (visual break in the grid)
4. **Utility Anchors:** Buttons (right side) for quick access

### Voice Number Placement

Voice numbers form a **vertical anchor column** that:
- Guides user's eye down the 15 rows
- Quick reference: "I'm on Voice 7"
- Improves usability in dense 180-control environment

### Color Bands (Optional Enhancement)

Consider adding subtle background color bands per voice:
- Alternating light/dark bands (every 2 rows)
- Improves visual separation in dense layout
- Reduces cognitive load when tracing horizontally

Example:
```
Voices 1-2:   Light background
Voices 3-4:   Darker background
Voices 5-6:   Light background
...
```

---

## Accessibility & Testing

### Testing Checklist

Before finalizing panels:
- [ ] Load both light + dark SVG files in VCV Rack
- [ ] Verify all guide marks align with widget coordinates
- [ ] Check text legibility at 96dpi (small 5px text especially)
- [ ] Confirm peranakan lattice doesn't obscure guide marks
- [ ] Test theme switching (colors update correctly)
- [ ] Review voice numbering (1-15 clearly visible)
- [ ] Inspect master controls section (clear separation)
- [ ] Check margins (nothing cut off at edges)
- [ ] High-DPI display test (4K monitor at 200% scale)

### Legibility Considerations

**Critical for dense 55HP panel:**
1. **Voice Numbers:** Must always be readable (6px is borderline)
2. **Column Headers:** Should stand out from rows (bold 6px helps)
3. **Sub-labels (L/O/R):** Can be tiny (5px) since position conveys meaning
4. **Master Labels:** 5-6px but important for distinction

**Font Weight:** Use bold for headers, regular for everything else

**Contrast:** Ensure text passes WCAG AA contrast ratios:
- Light theme: Dark text (#232323) on light background (#e6e6e6) ✓ High contrast
- Dark theme: Light text (#e4e4e7) on dark background (#232323) ✓ High contrast

---

## Manufacturing Considerations

### Panel Printing

**Resolution:** 600 DPI minimum for text clarity
**Color Accuracy:** Ensure crimson (#dc2626) matches branding
**Substrate:** Aluminum or anodized steel (standard Eurorack)

### Component Layout Verification

**Before fabrication:**
1. Print full-scale mockup (280mm × 380mm)
2. Place actual knobs/jacks at guide mark positions
3. Verify no collisions or misalignments
4. Check button reachability and spacing
5. Test slider operation in situ

---

## Future Enhancements

### Version 2.0 Concepts

1. **Tabbed/Paginated View**
   - Split into Rest/Melody/Octave pages
   - Reduces horizontal scrolling needs
   - Click tabs to switch between sections

2. **Micro-displays**
   - Small OLED per-voice showing current DNA values
   - Displays selected voice stats
   - Adds cost but improves usability significantly

3. **Voice Grouping**
   - Color-code voice rows by group (1-5, 6-10, 11-15)
   - Master controls per group (scramble group 1-5, etc.)
   - Hierarchical control of complexity

4. **Hidden Master Expander**
   - Separate compact module for master interpolation
   - Controls global blend across all voices
   - Complements per-voice detail in Deep module

---

## Summary

DeepStraitsSands represents the **maximum density DNA control** available in Red Dot Modular. With 180+ controls in a 55HP panel, it's designed for:

- **Advanced Users:** Who need granular per-voice sequence sculpting
- **Sound Designers:** Creating complex polymetric sequences
- **Performance Artists:** Rapidly adjusting voice character during live play

The dense grid layout is intentional—it trades ease-of-use for comprehensive control. Users will benefit from:
- Printed quick-reference card (voice layout)
- Video tutorials showing workflow
- Clear visual hierarchy (voice numbers, DNA knobs prominence)
- Consistent peranakan lattice aesthetic (provides texture, not chaos)

---

**Design Finalized:** 2026-05-20
**Version:** 1.0 (SVG templates complete)
**Status:** Ready for widget integration testing
