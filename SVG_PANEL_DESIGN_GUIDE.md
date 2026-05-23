# Professional SVG Panel Design Guide
## Red Dot Modular - Aesthetic Upgrade Techniques

---

## 1. COMPONENT LIBRARY APPROACH

Instead of drawing knobs in every panel, create **reusable SVG components** in `res/components/`:

### File Structure
```
res/components/
├── ComponentLibrary.svg          # Master library with all assets
├── Knob_Large_Metallic.svg       # Individual exports (optional)
├── Knob_Medium_Metallic.svg
├── Knob_Small_Metallic.svg
├── Jack_PJ301M_Metallic.svg
└── Button_Push_Metallic.svg
```

### Using Gradients (The Secret to Realism)

**Metallic Knob - Radial Gradient:**
```xml
<defs>
  <radialGradient id="knobMediumMetallic" cx="35%" cy="35%" r="65%">
    <stop offset="0%" style="stop-color:#e8d4a8;stop-opacity:1"/>     <!-- Bright highlight -->
    <stop offset="25%" style="stop-color:#c4a57b;stop-opacity:1"/>    <!-- Mid bronze -->
    <stop offset="55%" style="stop-color:#8b6f47;stop-opacity:1"/>    <!-- Mid-dark bronze -->
    <stop offset="90%" style="stop-color:#4a3520;stop-opacity:1"/>    <!-- Shadow -->
  </radialGradient>
</defs>

<!-- Knob with 3D effect -->
<circle cx="0" cy="0" r="7.5" fill="url(#knobMediumMetallic)"/>
<!-- Indicator line (shows rotation) -->
<rect x="-0.6" y="-8.5" width="1.2" height="2.5" rx="0.3" fill="#f5f5f5" opacity="0.95"/>
<!-- Highlight (adds depth) -->
<circle cx="-2.2" cy="-2.2" r="1.8" fill="#ffffff" opacity="0.2"/>
<!-- Shadow (adds depth) -->
<circle cx="2.2" cy="2.2" r="1.8" fill="#000000" opacity="0.15"/>
```

**Key Principles:**
- `cx/cy` at 35%/35% = off-center light source (makes it look 3D)
- Multiple color stops = smooth transitions
- Indicator line shows which way knob points
- Subtle highlight/shadow adds depth

---

## 2. PANEL TEXTURES & EFFECTS

### Grain Overlay (Like MeloDicer v6)

```xml
<defs>
  <!-- Create subtle grain texture -->
  <filter id="grain">
    <feTurbulence type="fractalNoise" baseFrequency="0.8" numOctaves="5" result="noise"/>
    <feDisplacementMap in="SourceGraphic" in2="noise" scale="0.5"/>
  </filter>
  
  <!-- Brushed metal lighting effect -->
  <linearGradient id="panelBrush" x1="0%" y1="0%" x2="0%" y2="100%">
    <stop offset="0%" style="stop-color:#ffffff;stop-opacity:0.08"/>
    <stop offset="50%" style="stop-color:#ffffff;stop-opacity:0"/>
    <stop offset="100%" style="stop-color:#000000;stop-opacity:0.08"/>
  </linearGradient>
</defs>

<!-- Apply to panel background -->
<rect width="121.92" height="380" fill="#1a1a1a"/>
<rect width="121.92" height="380" fill="url(#panelBrush)"/>
```

**Result:** Panel has subtle 3D depth without being obvious.

---

## 3. DECORATIVE BACKGROUND PATTERNS

### Concept: Peranakan Tile Pattern (for Interchange-style modules)

Instead of Interchange's current guide circles, you could add a **symbolic background pattern**:

```xml
<defs>
  <!-- Define a decorative peranakan-inspired pattern -->
  <pattern id="peranakan" patternUnits="userSpaceOnUse" width="20" height="20">
    <!-- Simple geometric tile (actual design would be more ornate) -->
    <circle cx="10" cy="10" r="8" fill="none" stroke="#2a2a2a" stroke-width="0.3" opacity="0.3"/>
    <line x1="2" y1="10" x2="18" y2="10" stroke="#2a2a2a" stroke-width="0.2" opacity="0.2"/>
    <line x1="10" y1="2" x2="10" y2="18" stroke="#2a2a2a" stroke-width="0.2" opacity="0.2"/>
  </pattern>
</defs>

<!-- Apply pattern to panel background -->
<rect width="121.92" height="380" fill="url(#peranakan)"/>
```

**Better Approach:**
Create the pattern in Inkscape with actual Peranakan tile geometry (octagons + squares), 
export as pattern, reuse across modules. Much more refined than code-based patterns.

---

## 4. COLOR PALETTES

### Current Aesthetic (Monsoon/MeloDicer v6 derived)

**Dark Theme:**
- Background: `#1a1a1a` (very dark gray)
- Panel border: `#0a0a0a` (almost black)
- Header: `#141414` (dark)
- Accent red: `#dc2626` or `#cc2222` (saturated)
- Text: `#e4e4e7` (light gray) or `#ddd`
- Knob metallics: `#e8d4a8` → `#3a2410` (warm bronze tones)
- Jacks: `#d8d8d8` → `#1a1a1a` (neutral metallic)

### Light Theme Alternative (for Interchange, etc.)

- Background: `#e6e6e6`
- Panel border: `#ccc`
- Header: `#d0d0d0`
- Accent: `#dc2626` (same red)
- Text: `#333` (dark gray)
- Knobs: lighter metallics

---

## 5. COMPONENT SIZING GUIDE

### Standard Rack Dimensions

| Component | Diameter/Size | Typical Use |
|-----------|---------------|------------|
| Large Knob | 19mm (9.5mm radius) | Primary parameters |
| Medium Knob | 15mm (7.5mm radius) | Secondary controls |
| Small Knob | 11mm (5.5mm radius) | Fine adjustments |
| Jack (PJ301M) | 10.5mm diameter | I/O |
| Push Button (square) | 10×10mm | Triggers/toggles |
| Push Button (round) | 8mm diameter | Small toggles |

### SVG to Rack Conversion
- SVG is in **millimeters**
- Rack uses `mm2px(Vec(x, y))` to convert
- Keep proportions consistent

---

## 6. REALISTIC COMPONENT TECHNIQUE EXAMPLES

### Beveled Button (3D Look)

```xml
<!-- Button with bevel/highlight -->
<g transform="translate(25, 315)">
  <!-- Main button -->
  <rect x="-8" y="-4" width="16" height="8" rx="1.5" 
        fill="url(#button)" stroke="#0a0a0a" stroke-width="0.3"/>
  <!-- Top highlight (beveled look) -->
  <rect x="-7.5" y="-3.5" width="15" height="3.5" rx="1" 
        fill="#ffffff" opacity="0.1"/>
  <!-- Text -->
  <text x="0" y="1.5" font-size="3.5" font-family="monospace" 
        text-anchor="middle" fill="#ddd" font-weight="bold">SCR</text>
</g>
```

### Jack (Double Circle for Depth)

```xml
<!-- 3D jack with rim and hole -->
<g transform="translate(25, 375)">
  <!-- Outer rim (metallic) -->
  <circle cx="0" cy="0" r="3.5" fill="url(#jack)" 
          stroke="#0a0a0a" stroke-width="0.3"/>
  <!-- Inner hole (dark) -->
  <circle cx="0" cy="0" r="2.2" fill="#0a0a0a"/>
  <!-- Highlight on rim -->
  <circle cx="-1" cy="-1" r="0.8" fill="#ffffff" opacity="0.25"/>
</g>
```

---

## 7. WORKFLOW FOR PANEL GENERATION

### Process for East/West Sands Panels

1. **Create template row** (voice with all 9 knobs + 3 buttons)
2. **Copy+transform** for voices 2-8 (East) and 9-16 (West)
3. **Use consistent Y spacing** (30mm per voice recommended)
4. **Maintain column alignment** for Rest/Melody/Octave groups

### Quick SVG Generation Script (Python example)

```python
#!/usr/bin/env python3
# Generate voices 2-8 for StraitsEastSands

voice_start_y = 60  # mm
voice_spacing = 30  # mm between voices

for voice_num in range(2, 9):
    y = voice_start_y + (voice_num - 2) * voice_spacing
    
    # Voice number label
    print(f'<text x="5" y="{y+3}" font-size="4">"{voice_num}"</text>')
    
    # 9 knobs: Rest(L/O/R), Melody(L/O/R), Octave(L/O/R)
    x_positions = [10, 20, 30, 44, 54, 64, 78, 88, 98]
    knob_types = ['med', 'sm', 'sm', 'med', 'sm', 'sm', 'med', 'sm', 'sm']
    
    for i, (x, knob_type) in enumerate(zip(x_positions, knob_types)):
        r = 5 if knob_type == 'med' else 4
        gradient = 'knobMed' if knob_type == 'med' else 'knobSm'
        print(f'<g transform="translate({x}, {y})">')
        print(f'  <circle cx="0" cy="0" r="{r}" fill="url(#{gradient})" stroke="#0a0a0a" stroke-width="0.3"/>')
        print(f'</g>')
```

---

## 8. BEFACO-STYLE REALISM (What Makes It Look Good)

### Key Techniques Befaco Uses

1. **Precise gradient stops** - Not just 2-3 colors, but 5-6 for smooth transitions
2. **Offset light source** - Light from top-left (cx/cy at 35%/35% or 40%/40%)
3. **Consistent shadow direction** - All shadows fall same direction
4. **Texture overlays** - Subtle grain doesn't overpower (opacity 0.03-0.08)
5. **Component hierarchy** - Larger knobs draw attention, smaller assist
6. **Color consistency** - Same metallics reused, not randomized per component

### Achievable in SVG

✅ Yes! Everything above is achievable in pure SVG.
- No rasterization needed
- Scales perfectly
- Print-ready
- Professional appearance

---

## 9. TEMPLATE CHECKLIST

When creating a new panel:

- [ ] Dark background (#1a1a1a) with panelBrush overlay
- [ ] Red line accent on left or right edge (2.5mm wide)
- [ ] Grain texture filter defined in <defs>
- [ ] All gradients using metallic knob palettes
- [ ] Consistent component sizing (Lrg/Med/Sm knobs, PJ301M jacks)
- [ ] Indicator lines on all knobs
- [ ] Highlight/shadow circles on knobs and jacks
- [ ] Column headers in golden/brass tone (#b8860b)
- [ ] Voice numbers in red (#cc2222)
- [ ] Buttons with beveled appearance
- [ ] Professional typography (monospace, consistent weights)
- [ ] Section separators (dividing lines)
- [ ] Footer with module name and HP

---

## 10. CREATING DECORATIVE BACKGROUNDS

### Option A: SVG Patterns (Like Peranakan Tiles)

Best done in **Inkscape**:
1. Design tile unit (20×20mm or 25×25mm)
2. Export as pattern definition
3. Apply to panel background
4. Adjust opacity for subtlety

### Option B: Symbolic Elements

For **Sands module** (stochastic theme):
- Subtle scattered dots or circles
- Probability curve outlines
- Random walk patterns

For **Interchange** (modulation theme):
- Wave forms
- Connection nodes
- Signal flow visualization

For **Monsoon** (sequencing theme):
- Step circles
- Pattern grids
- Tempo indicators

---

## 11. NEXT STEPS FOR YOUR PANELS

### For Monsoon (Main Sequencer)
Start from MeloDicer v6 aesthetic:
- Use its grain texture approach
- Adapt its knob gradients
- Keep step circle display
- Add more metallic depth

### For East/West Sands
Use upgraded StraitsEastSands_Pro_24HP.svg as template:
- Metallic knobs (Medium for Length, Small for Offset/Rotation)
- Beveled buttons
- Professional jacks
- Clean typography

### For All Modules
- Consistent red (#dc2626 or #cc2222) for accents
- Dark theme (#1a1a1a background)
- Grain texture overlay
- Professional gradients on all components

---

## RESOURCES

**Reference Files in Repository:**
- `res/components/ComponentLibrary.svg` - Master asset library
- `res/panels/StraitsEastSands_Pro_24HP.svg` - Example professional panel
- `res/panels/MeloDicer_panel_v6.svg` - Reference aesthetic

**Tools:**
- **Inkscape** - Best for pattern creation, SVG editing
- **XML editor** - For batch gradient/color changes
- **Python scripts** - For generating repetitive voice rows

**Color Reference:**
```
Dark theme: #1a1a1a (bg), #0a0a0a (border), #141414 (header)
Accent red: #dc2626 or #cc2222
Knob bright: #e8d4a8, Mid: #c4a57b, Dark: #4a3520
Jack bright: #d8d8d8, Mid: #a8a8a8, Dark: #505050
```

---

**Professional appearance is achievable with:**
1. Proper gradients (5+ color stops each)
2. Texture overlays (grain filters)
3. Realistic component assets
4. Consistent lighting direction
5. Careful color palette

No photoshop or rasterization needed - pure SVG!
