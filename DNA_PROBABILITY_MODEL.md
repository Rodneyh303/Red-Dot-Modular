# DNA as Probability Distributions

## Core Concept

The DNA sequencer is **random**, not deterministic. Each lane stores **probability distributions**, not direct sequences.

### No Direct Step Setting
- ❌ You DON'T edit steps 1, 2, 3, 4... directly
- ✅ You edit **probability values** for random selection
- ✅ The sequencer randomly picks from these probabilities

### What Gets Stored

Each DNA lane contains:
```
Probability Array (64 values, 0.0-1.0)
[0.3, 0.5, 0.8, 0.2, 0.1, ... , 0.4]
                         64 values total
```

Plus indexing parameters:
- **length**: How many of these 64 values are "active" (window size)
- **offset**: Starting position in the array (window start)
- **rotation**: Rotate the window (shifts the starting point)

---

## Example: Rest Lane

### Raw Data
```
Probability Array (64 values):
Index:  0    1    2    3    4    5   ...   63
Value: 0.1  0.2  0.3  0.8  0.5  0.1  ...  0.2

Interpretation:
  Index 0 → 10% chance of rest
  Index 1 → 20% chance of rest
  Index 2 → 30% chance of rest
  Index 3 → 80% chance of rest (high!)
  ...
```

### Indexing Parameters

```
length = 16          Window size: 16 values
offset = 0           Window starts at index 0
rotation = 3         Rotate by 3 positions

Active window with rotation:
[0.8, 0.5, 0.1, ..., 0.3, 0.1, 0.2, 0.3, ...]
 ↑    ↑    ↑
 (0+3)%16  (1+3)%16  (2+3)%16  ...

When sequencer plays:
  Step 1: Randomly pick index 0-15
  Step 2: Randomly pick index 0-15
  Step 3: Randomly pick index 0-15
  ...
  
  For each index, use probability[(offset + index + rotation) % 64]
```

### Visual Editor Shows

```
Display: 16 bars (probability distribution)

Bar 0:  [████████      ]  80% (prob[(0+0+3)%64])
Bar 1:  [█████         ]  50% (prob[(0+1+3)%64])
Bar 2:  [█             ]  10% (prob[(0+2+3)%64])
...
Bar 15: [██            ]  20% (prob[(0+15+3)%64])
```

---

## Parameters: Length / Offset / Rotation

### Length
```
Controls the window size (1-64 steps)

length = 16:  Use 16 values (half the array)
length = 32:  Use 32 values (full array)
length = 8:   Use 8 values (quarter array)
```

**Interaction:**
- Left handle: Adjusts offset (start of window)
- Right handle: Adjusts length (end of window)
- Length handle position = offset + length

### Offset
```
Controls where the window starts in the probability array

offset = 0:   Window = [0, 1, 2, ..., 15]
offset = 8:   Window = [8, 9, 10, ..., 23]
offset = 16:  Window = [16, 17, ..., 31]
```

**Interaction:**
- Left handle: Drag left/right to change offset
- Updates which part of the array is used

### Rotation
```
Rotates/shifts the window within itself

rotation = 0:  [prob[0], prob[1], prob[2], ...]
rotation = 1:  [prob[1], prob[2], prob[3], ...]  (starts at 1)
rotation = 3:  [prob[3], prob[4], prob[5], ...]  (starts at 3)
rotation = 15: [prob[15], prob[0], prob[1], ...] (wraps around)
```

**Interaction:**
- Teal block shows rotation offset
- Drag left/right to rotate
- Shows which value is the "first" in the window

---

## Visual Editor Layout

### Display Resolution
```
Display: Always shows 16 bars (DISPLAY_SIZE = 16)
Storage: 64 probability values (PROB_ARRAY_SIZE = 64)

The 16 bars show the current window:
  bars[0-15] = probability[(offset + 0-15 + rotation) % 64]
```

### Bar Height = Probability
```
Bar height 100% → 100% chance of that state
Bar height 50%  → 50% chance of that state
Bar height 0%   → 0% chance of that state
```

### Handles

```
┌────────────────────────────────────────────┐
│ REST  [●···············●                   │  Teal block = rotation offset
│ PROB  ▃ ▄ ▁ ▆ ▃ ▂ ▅ ▃ ▄ ▁ ▂ ▆ ▃ ▂ ▅ ▃    │
└────────────────────────────────────────────┘
        ↑               ↑
      Left handle    Right handle
      (offset)      (offset + length)
```

---

## Sequencer Behavior

### How the Randomness Works

```
Sequencer process each step:
  1. Get current window index (step % length)
  2. Calculate actual array index: (offset + index + rotation) % 64
  3. Get probability from array[actualIndex]
  4. Roll random number 0.0-1.0
  5. If random < probability → output note
     Else → output rest/silence
```

### Example: Editing a Rest Probability

```
Visual editor:
  User clicks bar 5, drags up to 80%
  
Internal update:
  actual_index = (offset + 5 + rotation) % 64
  probability_array[actual_index] = 0.8
  
Sequencer behavior:
  Next time step 5 plays:
    Random # < 0.8 → 80% chance of note
    Random # ≥ 0.8 → 20% chance of rest
```

---

## Mono vs Poly Probability Distributions

### Mono Sands

Each lane has **one** probability distribution (64 values):

```
Rest:       [0.1, 0.2, 0.3, 0.8, ...]  64 values
Melody:     [0.5, 0.4, 0.6, 0.2, ...]  64 values
Octave:     [0.2, 0.5, 0.3, 0.1, ...]  64 values
Legato:     [0.9, 0.1, 0.9, 0.1, ...]  64 values
Accent:     [0.3, 0.4, 0.5, 0.6, ...]  64 values
Variation:  [0.2, 0.8, 0.2, 0.8, ...]  64 values

Total: 6 lanes × 64 values = 384 probability values
```

### Poly Sands (per-voice)

Each voice has **3** probability distributions:

```
Voice 2 Rest:     [0.1, 0.2, 0.3, ...]  64 values
Voice 2 Melody:   [0.5, 0.4, 0.6, ...]  64 values
Voice 2 Octave:   [0.2, 0.5, 0.3, ...]  64 values

Voice 3 Rest:     [0.4, 0.5, 0.6, ...]  64 values
...
(same for voices 4-8)

Plus global (shared by all voices):
Legato:           [0.9, 0.1, 0.9, ...]  64 values (global)
Accent:           [0.3, 0.4, 0.5, ...]  64 values (global)
Variation:        [0.2, 0.8, 0.2, ...]  64 values (global)

Total: 7 voices × 3 lanes × 64 + 3 global × 64 = 1,512 probability values
```

---

## Parameter Storage

### Mono Sands

```cpp
enum ParamId {
  // Rest lane (64 probability values)
  SANDS_REST_PROB_0,
  SANDS_REST_PROB_1,
  ...
  SANDS_REST_PROB_63,
  
  // Rest lane parameters
  SANDS_REST_LENGTH,
  SANDS_REST_OFFSET,
  SANDS_REST_ROTATION,
  
  // Melody lane (64 probs + 3 params)
  SANDS_MELODY_PROB_0,
  ...
  SANDS_MELODY_PROB_63,
  SANDS_MELODY_LENGTH,
  SANDS_MELODY_OFFSET,
  SANDS_MELODY_ROTATION,
  
  // Same for Octave, Legato, Accent, Variation
  // Total: 6 lanes × (64 probs + 3 params) = 402 parameters
};
```

### Poly Sands

```cpp
enum ParamId {
  // Per-voice (7 voices × 3 lanes)
  
  // Voice 2
  POLY_REST_PROB_V2_0,
  ...
  POLY_REST_PROB_V2_63,
  POLY_REST_LENGTH_V2,
  POLY_REST_OFFSET_V2,
  POLY_REST_ROTATION_V2,
  
  // Melody for Voice 2 (same pattern)
  POLY_MELODY_PROB_V2_0,
  ...
  
  // (Repeat for voices 3-8)
  
  // Global
  POLY_LEGATO_PROB_GLOBAL_0,
  ...
  POLY_LEGATO_PROB_GLOBAL_63,
  POLY_LEGATO_LENGTH_GLOBAL,
  POLY_LEGATO_OFFSET_GLOBAL,
  POLY_LEGATO_ROTATION_GLOBAL,
  
  // Same for Accent and Variation
  
  // Total: 7 × 3 × (64 + 3) + 3 × (64 + 3) = 1,659 parameters
};
```

---

## Data Structure

### V4 ProbabilityLane

```cpp
struct ProbabilityLane {
  float probabilities[64];    // Raw probability data
  int length;                 // Window size (1-64)
  int offset;                 // Window start position
  int rotation;               // Rotation offset
  
  // Get probability at display bar with rotation applied
  float getProbability(int displayBar) {
    int actualIndex = (offset + displayBar + rotation) % 64;
    return probabilities[actualIndex];
  }
  
  // Set probability at display bar
  void setProbability(int displayBar, float value) {
    int actualIndex = (offset + displayBar + rotation) % 64;
    probabilities[actualIndex] = value;
  }
};

struct VoiceState {
  ProbabilityLane lanes[6];  // All 6 lanes
};
```

---

## Editing Workflow

### User Edits in Visual Editor

```
User clicks bar 5, drags up to 80%

Visual editor:
  setProbabilityAtDisplayBar(lane=REST, bar=5, value=0.8)
  
Internally:
  actual_index = (length + 5 + rotation) % 64
  probabilities[actual_index] = 0.8
  
Module reads:
  SANDS_REST_PROB_5 = 0.8
```

### Sequencer Uses the Data

```
Sequencer step 5:
  window_index = 5
  actual_index = (offset + 5 + rotation) % 64
  probability = probabilities[actual_index] = 0.8
  
  Random # 0.0-1.0
  If random < 0.8 → Note plays (20% silence)
  If random ≥ 0.8 → Rest (silence)
```

---

## Presets (Probability Distributions)

### Flat
```
All values = 0.5
[0.5, 0.5, 0.5, ..., 0.5]
Result: 50% chance at every step
```

### Peak
```
High in middle, low on edges
[0.1, 0.2, 0.4, 0.7, 1.0, 0.7, 0.4, 0.2, 0.1, ...]
Result: Higher probability toward center of window
```

### Ramp
```
Increasing probability
[0.0, 0.1, 0.2, 0.3, 0.4, ..., 1.0]
Result: Increasingly likely as you move through window
```

### Random
```
Random values
[0.3, 0.8, 0.1, 0.9, 0.2, ...]
Result: Unpredictable probability pattern
```

---

## Summary

| Aspect | Value |
|--------|-------|
| **Storage per lane** | 64 probability values |
| **Display resolution** | 16 bars (64 ÷ 4 for display) |
| **Window controls** | length, offset, rotation |
| **Bar height** | Probability (0-100%) |
| **Sequencer behavior** | Random selection using probabilities |
| **No direct sequences** | Pure probability distributions |
| **Mono lanes** | 6 (Rest, Melody, Octave, Legato, Accent, Variation) |
| **Poly lanes per-voice** | 3 (Rest, Melody, Octave) |
| **Poly global lanes** | 3 (Legato, Accent, Variation) |

---

## Key Insight

The visual editor doesn't show "what notes play on each step". Instead, it shows **the probability distribution** that controls the randomness. The sequencer uses length/offset/rotation to index into these probabilities and generate random sequences.

This is fundamentally different from a deterministic sequencer where you set each step directly.

