# DeepStraitsSands - Coordinate Reference Table

## Quick Lookup: Exact X,Y Positions (in mm from top-left)

### Panel Dimensions
- **Viewbox:** `0 0 280 380`
- **Total Width:** 280mm (~55 HP)
- **Total Height:** 380mm (3U standard)

---

## HEADER SECTION (Y = 0-55mm)

### Branding
```
Main Title "DEEP STRAITS SANDS"
  Position: X=140, Y=18
  Font: Monospace bold 12px
  Anchor: center

Subtitle "per-voice dna control"
  Position: X=140, Y=30
  Font: Monospace 8px
  Anchor: center

Red Dot
  Position: X=270, Y=10
  Size: 2.5mm diameter
```

### Column Headers (Y=45mm)

| Header | X Position | Type |
|--------|-----------|------|
| V | 8 | Voice |
| REST | 30 | DNA Section |
| MELODY | 95 | DNA Section |
| OCTAVE | 160 | DNA Section |
| INTERP | 215 | Sliders |
| CTL | 260 | Controls |

### Sub-headers (Y=52mm) - DNA Window Labels

| Label | X Position | Section |
|-------|-----------|---------|
| L | 20 | Rest Length |
| O | 30 | Rest Offset |
| R | 40 | Rest Rotation |
| L | 85 | Melody Length |
| O | 95 | Melody Offset |
| R | 105 | Melody Rotation |
| L | 150 | Octave Length |
| O | 160 | Octave Offset |
| R | 170 | Octave Rotation |

### Sub-headers (Y=52mm) - Interpolation Labels

| Label | X Position | Type |
|-------|-----------|------|
| R | 205 | Rest Interp |
| M | 215 | Melody Interp |
| O | 225 | Octave Interp |

---

## VOICE ROWS (15 × rows, Y = 60-214mm)

### Y-Coordinate Lookup Table

```
Voice  Row#  Y Position (mm)  Formula
─────────────────────────────────────
  1     0        60          60 + (0 × 11)
  2     1        71          60 + (1 × 11)
  3     2        82          60 + (2 × 11)
  4     3        93          60 + (3 × 11)
  5     4       104          60 + (4 × 11)
  6     5       115          60 + (5 × 11)
  7     6       126          60 + (6 × 11)
  8     7       137          60 + (7 × 11)
  9     8       148          60 + (8 × 11)
 10     9       159          60 + (9 × 11)
 11    10       170          60 + (10 × 11)
 12    11       181          60 + (11 × 11)
 13    12       192          60 + (12 × 11)
 14    13       203          60 + (13 × 11)
 15    14       214          60 + (14 × 11)
```

### Voice Number Column (X=8mm, all rows)

```
Voice Label Positions:
  Voice 1: X=8, Y=65
  Voice 2: X=8, Y=76
  Voice 3: X=8, Y=87
  ...
  Voice 15: X=8, Y=219

Formula: X=8, Y = voice_y + 5
Where voice_y = 60 + (voice_index * 11)
```

### DNA Knob Positions (all 15 rows)

#### Rest DNA (X = 20, 30, 40 mm)

```
Rest Length Knob:
  All rows: X=20, Y = voice_row_y
  Size: ~3.5mm radius (medium knob)
  
Rest Offset Knob:
  All rows: X=30, Y = voice_row_y
  Size: ~3mm radius (small knob)
  
Rest Rotation Knob:
  All rows: X=40, Y = voice_row_y
  Size: ~3mm radius (small knob)

Example Voice 1:
  Rest Len:  X=20, Y=63
  Rest Off:  X=30, Y=63
  Rest Rot:  X=40, Y=63

Example Voice 5:
  Rest Len:  X=20, Y=104
  Rest Off:  X=30, Y=104
  Rest Rot:  X=40, Y=104
```

#### Melody DNA (X = 85, 95, 105 mm)

```
Melody Length Knob:
  All rows: X=85, Y = voice_row_y
  Size: ~3.5mm radius (medium)
  
Melody Offset Knob:
  All rows: X=95, Y = voice_row_y
  Size: ~3mm radius (small)
  
Melody Rotation Knob:
  All rows: X=105, Y = voice_row_y
  Size: ~3mm radius (small)

Example Voice 1:
  Melody Len: X=85, Y=63
  Melody Off: X=95, Y=63
  Melody Rot: X=105, Y=63
```

#### Octave DNA (X = 150, 160, 170 mm)

```
Octave Length Knob:
  All rows: X=150, Y = voice_row_y
  Size: ~3.5mm radius (medium)
  
Octave Offset Knob:
  All rows: X=160, Y = voice_row_y
  Size: ~3mm radius (small)
  
Octave Rotation Knob:
  All rows: X=170, Y = voice_row_y
  Size: ~3mm radius (small)

Example Voice 1:
  Octave Len: X=150, Y=63
  Octave Off: X=160, Y=63
  Octave Rot: X=170, Y=63
```

### Interpolation Slider Positions (all 15 rows)

```
Rest Interpolation Slider:
  All rows: X = 202-208 (6mm wide), Y = voice_row_y - 7 to voice_row_y + 7
  Type: Vertical fader
  
Melody Interpolation Slider:
  All rows: X = 212-218 (6mm wide), Y = voice_row_y - 7 to voice_row_y + 7
  Type: Vertical fader
  
Octave Interpolation Slider:
  All rows: X = 222-228 (6mm wide), Y = voice_row_y - 7 to voice_row_y + 7
  Type: Vertical fader

Example Voice 1 (voice_row_y = 63):
  Rest Interp:   X=202-208, Y=56-70
  Melody Interp: X=212-218, Y=56-70
  Octave Interp: X=222-228, Y=56-70

Example Voice 15 (voice_row_y = 214):
  Rest Interp:   X=202-208, Y=207-221
  Melody Interp: X=212-218, Y=207-221
  Octave Interp: X=222-228, Y=207-221
```

### Control Buttons (Scramble & Reset per row)

```
Scramble Button (per voice):
  All rows: X = 245, Y = voice_row_y - 2 to voice_row_y + 2
  Size: 5×5mm, rounded corners
  
Reset Button (per voice):
  All rows: X = 260, Y = voice_row_y - 2 to voice_row_y + 2
  Size: 5×5mm, rounded corners

Example Voice 1 (voice_row_y = 63):
  Scramble: X=245, Y=59-67
  Reset:    X=260, Y=59-67

Example Voice 8 (voice_row_y = 137):
  Scramble: X=245, Y=133-141
  Reset:    X=260, Y=133-141
```

### Gate Inputs (Scramble & Reset per row)

```
Scramble Gate Input (per voice):
  All rows: X = 245, Y = voice_row_y + 8
  Size: ~2mm radius (jack)
  
Reset Gate Input (per voice):
  All rows: X = 260, Y = voice_row_y + 8
  Size: ~2mm radius (jack)

Example Voice 1 (voice_row_y = 63):
  Scramble Gate: X=245, Y=71
  Reset Gate:    X=260, Y=71

Example Voice 10 (voice_row_y = 159):
  Scramble Gate: X=245, Y=167
  Reset Gate:    X=260, Y=167
```

---

## MASTER CONTROLS SECTION (Y = 335-365mm)

### Master Label
```
Position: X=8, Y=350
Text: "MASTER"
Font: Monospace bold 6px
```

### Master Scramble All Button
```
Position: X=30, Y=345
Size: 8×8mm
Style: Rounded corners (rx=1mm)
Label: "scramble-all" at X=34, Y=362 (5px font)
```

### Master Reset All Button
```
Position: X=95, Y=345
Size: 8×8mm
Style: Rounded corners (rx=1mm)
Label: "reset-all" at X=99, Y=362 (5px font)
```

### Master Scramble Gate Input
```
Position: X=160, Y=349
Size: ~6mm diameter (PJ301M jack)
Label: "scr-gate" at X=160, Y=366 (5px font)
```

### Master Reset Gate Input
```
Position: X=225, Y=349
Size: ~6mm diameter (PJ301M jack)
Label: "rst-gate" at X=225, Y=366 (5px font)
```

---

## FOOTER SECTION (Y = 365-380mm)

### Branding Line
```
Position: X=140, Y=378
Text: "red dot | deep straits sands"
Font: Monospace 5px
Anchor: center
Color: Gray (#aaa light, #555 dark)
```

---

## CONVERSION FORMULAS

### Widget Code to SVG

**For any control at widget position `mm2px(Vec(X_mm, Y_mm))`:**

```
SVG X-coordinate = X_mm
SVG Y-coordinate = Y_mm
```

**For voice-indexed controls at row `v` (0-14):**

```
voice_y = 60 + (v * 11)

Rest Len Knob:   X=20,  Y=voice_y
Rest Off Knob:   X=30,  Y=voice_y
Rest Rot Knob:   X=40,  Y=voice_y
Melody Len Knob: X=85,  Y=voice_y
Melody Off Knob: X=95,  Y=voice_y
Melody Rot Knob: X=105, Y=voice_y
Octave Len Knob: X=150, Y=voice_y
Octave Off Knob: X=160, Y=voice_y
Octave Rot Knob: X=170, Y=voice_y

Rest Interp:     X=205, Y=voice_y (slider vertical range: ±7)
Melody Interp:   X=215, Y=voice_y (slider vertical range: ±7)
Octave Interp:   X=225, Y=voice_y (slider vertical range: ±7)

Scramble Btn:    X=245, Y=voice_y
Reset Btn:       X=260, Y=voice_y
Scramble Gate:   X=245, Y=voice_y + 8
Reset Gate:      X=260, Y=voice_y + 8
```

### SVG to Widget Code

**For guide mark at SVG position `(x, y)`:**

```cpp
// Determine if it's a voice row or master row
if (y >= 60 && y <= 214) {
    // Voice row
    int v = (y - 60) / 11;  // Calculate voice index (0-14)
    float voice_y = 60 + (v * 11);
    
    // Place control at corresponding widget position
    addParam(createParamCentered<KnobType>(mm2px(Vec(x, voice_y)), mod, PARAM_ID));
} else if (y >= 345) {
    // Master controls
    addParam(createParamCentered<ButtonType>(mm2px(Vec(x, y)), mod, PARAM_ID));
}
```

---

## VISUAL SPACING REFERENCE

### Horizontal Spacing (X-axis)

```
Left Margin:           0-2mm (padding)
Voice Number Col:      2-14mm (12mm wide)
Rest DNA Section:      14-54mm (40mm used, spacing 20/30/40)
Space:                 54-65mm (11mm gap)
Melody DNA Section:    65-105mm (40mm used, spacing 85/95/105)
Space:                 105-115mm (10mm gap)
Octave DNA Section:    115-155mm (40mm used, spacing 150/160/170)
Space:                 155-190mm (35mm gap)
Interp Sliders:        190-235mm (45mm used, spacing 205/215/225)
Space:                 235-245mm (10mm gap)
Control Buttons:       245-270mm (25mm used, spacing 245/260)
Right Margin:          270-280mm (10mm, but only 2mm padding used)
```

### Vertical Spacing (Y-axis)

```
Header:                0-55mm
  Title: 18mm
  Column headers: 45mm
  Sub-headers: 52mm

Voice Rows:            60-214mm
  Row height: 11mm (center-to-center)
  Voice 1: 60mm
  Voice 15: 214mm (60 + 154 = 214)

Master Controls:       335-365mm
  Label: 350mm
  Buttons: 345mm
  Gates: 349mm
  Gate labels: 366mm

Footer:                365-380mm
  Branding: 378mm
```

---

## TEST COORDINATES

### Quick Verification Checklist

Print this checklist and place actual hardware on panel mockup to verify:

```
Voice 1 Rest Length Knob:        Should be at X=20, Y=63mm ☐
Voice 1 Melody Offset Knob:      Should be at X=95, Y=63mm ☐
Voice 1 Octave Rotation Knob:    Should be at X=170, Y=63mm ☐
Voice 1 Rest Interp Slider:      Should be at X=205, Y=56-70mm ☐
Voice 1 Scramble Button:         Should be at X=245, Y=59-67mm ☐
Voice 1 Scramble Gate:           Should be at X=245, Y=71mm ☐

Voice 8 Rest Length Knob:        Should be at X=20, Y=137mm ☐
Voice 15 Octave Length Knob:     Should be at X=150, Y=214mm ☐

Master Scramble Button:          Should be at X=30, Y=345mm ☐
Master Reset Button:             Should be at X=95, Y=345mm ☐
Master Scramble Gate:            Should be at X=160, Y=349mm ☐
Master Reset Gate:               Should be at X=225, Y=349mm ☐
```

---

## IMPLEMENTATION NOTES

### For Widget Developers

1. **Voice Loop:**
   ```cpp
   for (int v = 0; v < 15; v++) {
       float voice_y = 60 + (v * 11.0f);  // Calculate Y position
       
       // Rest DNA
       addParam(createParamCentered<RDM_KnobMedium>(mm2px(Vec(20.f, voice_y)), mod, POLY_DNA_REST_LEN + v));
       addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(30.f, voice_y)), mod, POLY_DNA_REST_OFF + v));
       addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(40.f, voice_y)), mod, POLY_DNA_REST_ROT + v));
       
       // Melody DNA
       addParam(createParamCentered<RDM_KnobMedium>(mm2px(Vec(85.f, voice_y)), mod, POLY_DNA_MELODY_LEN + v));
       addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(95.f, voice_y)), mod, POLY_DNA_MELODY_OFF + v));
       addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(105.f, voice_y)), mod, POLY_DNA_MELODY_ROT + v));
       
       // Octave DNA
       addParam(createParamCentered<RDM_KnobMedium>(mm2px(Vec(150.f, voice_y)), mod, POLY_DNA_OCTAVE_LEN + v));
       addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(160.f, voice_y)), mod, POLY_DNA_OCTAVE_OFF + v));
       addParam(createParamCentered<RDM_KnobSmall>(mm2px(Vec(170.f, voice_y)), mod, POLY_DNA_OCTAVE_ROT + v));
       
       // Interpolation Sliders
       addParam(createParamCentered<Fader>(mm2px(Vec(205.f, voice_y)), mod, POLY_REST_INTERP + v));
       addParam(createParamCentered<Fader>(mm2px(Vec(215.f, voice_y)), mod, POLY_MELODY_INTERP + v));
       addParam(createParamCentered<Fader>(mm2px(Vec(225.f, voice_y)), mod, POLY_OCTAVE_INTERP + v));
       
       // Buttons
       addParam(createParamCentered<PushButton>(mm2px(Vec(245.f, voice_y)), mod, SCRAMBLE_VOICE_1 + v));
       addParam(createParamCentered<PushButton>(mm2px(Vec(260.f, voice_y)), mod, RESET_VOICE_1 + v));
       
       // Gates
       addInput(createInputCentered<PJ301MPort>(mm2px(Vec(245.f, voice_y + 8.f)), mod, SCRAMBLE_GATE_1 + v));
       addInput(createInputCentered<PJ301MPort>(mm2px(Vec(260.f, voice_y + 8.f)), mod, RESET_GATE_1 + v));
   }
   ```

2. **Master Controls:**
   ```cpp
   // Master buttons (Y = 345)
   addParam(createParamCentered<PushButton>(mm2px(Vec(30.f, 345.f)), mod, SCRAMBLE_ALL));
   addParam(createParamCentered<PushButton>(mm2px(Vec(95.f, 345.f)), mod, RESET_ALL));
   
   // Master gates (Y = 349)
   addInput(createInputCentered<PJ301MPort>(mm2px(Vec(160.f, 349.f)), mod, SCRAMBLE_ALL_GATE));
   addInput(createInputCentered<PJ301MPort>(mm2px(Vec(225.f, 349.f)), mod, RESET_ALL_GATE));
   ```

---

**Document Version:** 1.0
**Last Updated:** 2026-05-20
**Status:** Complete - Ready for implementation
