# Step 6: Direction CV Modulation

## Overview

Add CV input jacks to all 3 Sands panels (East, Macro, Mono) that modulate the
per-lane direction via voltage thresholds. The DirCell remains the manual
control; the CV jack overrides when patched.

## CV Mapping

0-10V input divided into 4 equal ranges:

| Voltage       | Direction  |
|---------------|------------|
| 0.0 - 2.5V    | Forward    |
| 2.5 - 5.0V    | Reverse    |
| 5.0 - 7.5V    | Pendulum   |
| 7.5 - 10.0V   | Ping-pong  |

When the CV jack is **unpatched**, the DirCell's manual setting is used.
When **patched**, the CV voltage selects the direction (overrides manual).

## Panel Layout (+2HP)

All 3 panels: 223.52mm (44HP) -> 233.68mm (46HP)

New column layout (right side):
- OWNER_X = 205mm (unchanged)
- DIR_X = 212mm (unchanged)
- DIR_CV_X = 219mm (NEW — direction CV jacks)
- PROB_OUT_X = 226mm (shifted right by 7mm)

### East: 6 CV jacks (one per lane, MEL/OCT/REST/ACC/VAR/LEG)
### Macro: 4 CV jacks (poly lanes only, MEL/OCT/REST/ACC)
### Mono: 6 CV jacks (all lanes)

All CV jacks are mono (1 channel). They modulate the MONO direction
(laneDirPending_), same as the DirCell on the mono tab. Per-voice direction
on East's poly tabs remains manual (DirCell only).

## Engine Changes

None — the widget's step() already pushes to laneDirPending_. The only change
is in the widget: read the CV input and use it to override the DirCell value
when patched.

## Widget Changes (per module)

### Direction sync in step()

```
for each lane:
    if cvInput[lane].isConnected():
        float v = cvInput[lane].getVoltage() / 10.0f  // 0..1
        int dir = std::min(3, (int)(v * 4.0f));         // 0..3
        effective_dir = (LaneDir)dir
    else:
        effective_dir = (LaneDir)round(dirDispId[lane].getValue())
    
    se->laneDirPending_[strand] = effective_dir
```

### New params/inputs

Each module needs:
- 6 (East/Mono) or 4 (Macro) new InputIds for the CV jacks
- Panel SVG kit markers: `input_dir_cv_<lane>` at DIR_CV_X

## Implementation Steps

1. Panel generators: +2HP, add `input_dir_cv_<lane>` kit markers at DIR_CV_X
2. Headers: add InputIds for direction CV, update NUM_INPUTS
3. Widget constructors: bind CV input jacks
4. Widget step(): read CV, override DirCell when patched
5. Build + verify
