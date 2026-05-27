# Visual DNA Editor - Probability Model (Corrected)

## Core Concept

The visual editor is a **UI replacement for editing 16 probability values**. It does NOT replace the length/offset/rotation knobs - those still exist and control how the sequencer indexes into these 16 values.

### Two Separate Systems

```
System 1: Visual Editor (UI)
  ├─ Shows 16 probability bars (one per step)
  ├─ Lets you drag bars up/down to set probabilities (0-100%)
  └─ Just for editing: randomize, copy, paste, reverse

System 2: Sequencer Indexing (Knobs)
  ├─ LENGTH knob: How many of the 16 steps are active (0-15)
  ├─ OFFSET knob: Where to start reading from (0-15)
  ├─ ROTATION knob: Rotate within the active window
  └─ These determine WHICH probability gets used at each step
```

---

## The 16 Probability Values

Each lane stores exactly **16 probability values**:

```
Visual Editor Display:
┌──────────────────────────────────────────────────────────────┐
│ REST                                                         │
│ Step:  0    1    2    3    4    5    6    7    8    9  10 11 12 13 14 15
│ Bar:   ▃    ▁    ▅    ▂    ▆    ▁    ▃    ▁    ▂    ▄   ▁  ▅  ▂  ▃  ▁  ▂
│ Prob: 60%  20%  85%  40%  95%  20%  60%  20%  40%  75% 20% 85% 40% 60% 20% 40%
└──────────────────────────────────────────────────────────────┘

Memory:
  probabilities[0]  = 0.60
  probabilities[1]  = 0.20
  probabilities[2]  = 0.85
  ...
  probabilities[15] = 0.40
```

---

## Sequencer Indexing: The Knobs

The three knobs **select which probability is used at each step**:

### LENGTH Knob
```
Controls how many steps are active:

LENGTH = 16  → All 16 steps (steps 0-15)
LENGTH = 8   → Only 8 steps (steps 0-7, loops)
LENGTH = 4   → Only 4 steps (steps 0-3, loops)
```

### OFFSET Knob
```
Controls where the window starts (circular):

OFFSET = 0   → Start at step 0, read steps 0,1,2,3,...
OFFSET = 4   → Start at step 4, read steps 4,5,...,15,0,1,...
OFFSET = 8   → Start at step 8, read steps 8,9,...,15,0,1,...
```

### ROTATION Knob
```
Rotates the window within itself:

ROTATION = 0  → Read in order: 0,1,2,3,...
ROTATION = 3  → Read shifted: 3,4,5,6,...
ROTATION = 5  → Read shifted: 5,6,7,8,...
```

---

## How They Work Together

### Default: LENGTH=16, OFFSET=0, ROTATION=0

```
16 Probabilities (from visual editor):
  [0.60, 0.20, 0.85, 0.40, 0.95, 0.20, 0.60, 0.20, 
   0.40, 0.75, 0.20, 0.85, 0.40, 0.60, 0.20, 0.40]

Sequencer reads at each step:
  Step 0: (0+0+0) % 16 = 0   → prob[0] = 0.60 (60%)
  Step 1: (0+1+0) % 16 = 1   → prob[1] = 0.20 (20%)
  Step 2: (0+2+0) % 16 = 2   → prob[2] = 0.85 (85%)
  ...
  Step 15: (0+15+0) % 16 = 15 → prob[15] = 0.40 (40%)
```

### With OFFSET=4

```
Knobs: LENGTH=16, OFFSET=4, ROTATION=0

Sequencer reads:
  Step 0: (4+0+0) % 16 = 4   → prob[4] = 0.95 (95%)
  Step 1: (4+1+0) % 16 = 5   → prob[5] = 0.20 (20%)
  Step 2: (4+2+0) % 16 = 6   → prob[6] = 0.60 (60%)
  ...
  Step 11: (4+11+0) % 16 = 15 → prob[15] = 0.40 (40%)
  Step 12: (4+12+0) % 16 = 0  → prob[0] = 0.60 (wraps)
```

### With LENGTH=8, OFFSET=0, ROTATION=0

```
Knobs: LENGTH=8 (only 8 steps active)

Sequencer reads:
  Step 0: (0+0+0) % 16 = 0 → prob[0] = 0.60
  Step 1: (0+1+0) % 16 = 1 → prob[1] = 0.20
  ...
  Step 7: (0+7+0) % 16 = 7 → prob[7] = 0.20
  Step 8: Loop back (only 8 steps, so return to step 0)
```

### With ROTATION=3

```
Knobs: LENGTH=16, OFFSET=0, ROTATION=3

Sequencer reads:
  Step 0: (0+0+3) % 16 = 3  → prob[3] = 0.40
  Step 1: (0+1+3) % 16 = 4  → prob[4] = 0.95
  Step 2: (0+2+3) % 16 = 5  → prob[5] = 0.20
  ...
  Step 13: (0+13+3) % 16 = 0 → prob[0] = 0.60 (wraps)
```

---

## Visual Editor: Pure Editing

The visual editor is **only for editing the 16 probability values**. It does NOT show or control LENGTH/OFFSET/ROTATION.

### What It Does

✅ Display 16 bars (one per probability value)  
✅ Drag bars to set probabilities (0-100%)  
✅ Randomize (R key)  
✅ Copy/paste lanes (Ctrl+C/V)  
✅ Reverse pattern (Shift+V)  
✅ Preset seeds (Flat, Peak, Ramp, Random)  
✅ Undo/redo (Ctrl+Z)  

### What It Does NOT Do

❌ Show LENGTH/OFFSET/ROTATION parameters  
❌ Control which steps are "active" (LENGTH is a knob)  
❌ Show the window or selection (those are knob parameters)  
❌ Have handles for length/offset/rotation  

---

## Complete Example

### Hardware Layout

```
┌──────────────────────────────────────────────────────┐
│ MONSOON SANDS EXPANDER                               │
├──────────────────────────────────────────────────────┤
│                                                      │
│  REST LANE:                                          │
│  [LENGTH=8]  [OFFSET=2]  [ROTATION=1]  (3 knobs)  │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │ Visual Editor (16 bars, all editable)        