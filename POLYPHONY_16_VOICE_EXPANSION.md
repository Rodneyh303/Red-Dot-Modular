# 16-Voice Polyphony Expansion - Architecture & Implementation Plan

## Current State → New State Mapping

### Polyphony Expanders

**CURRENT:**
```
MeloDicer (mono, voice 1)
  └─ MeloDicerPolyVoiceExpander (voices 2-7, 6 voices)
```

**NEW:**
```
MeloDicer (mono, voice 1)
  ├─ Straits East Expander (voices 2-8, 7 voices)
  │  └─ Straits West Expander (voices 9-16, 8 voices)
  │     └─ (only valid if East also present)
```

**Total polyphony:** 1 (mono) + 7 (East) + 8 (West) = **16 voices** ✓

---

### DNA Expanders

**CURRENT:**
```
MeloDicer (mono)
  └─ MeloDicerDNAExpander (mono DNA ops only)
  └─ (no DNA for poly voices)
```

**NEW:**
```
MeloDicer (mono)
  ├─ Sands Expander (mono DNA ops)
  └─ Straits Sands Expander (DNA ops for voices 2-16)
     └─ Works with both East and West expanders
```

---

## Detailed Specifications

### 1. STRAITS EAST EXPANDER

**Purpose:** Voices 2-8 (7 independent voices)

**Outputs:**
- 8 voice poly gate outs
- 8 voice poly CV outs
- Mono gate out
- Mono CV out

**Inputs:**
- 8 × Voice probability modulation ins (one per voice 2-8)
- 8 × Voice probability attenuverters

**Behavior:**
- Handles voices 2-8 independently
- Can exist alone (voices 2-8 active)
- Must be left of main Melodicer

**Features to Consider:**
- Should voice 1 (mono) CV/gate also output here? Or only in main module?
- How are the 8 poly outputs numbered? (Gate 1-8 = voices 2-9? or 1-8 = voices 1-8?)

---

### 2. STRAITS WEST EXPANDER

**Purpose:** Voices 9-16 (8 independent voices)

**Outputs:**
- 8 voice poly gate outs (voices 9-16)
- 8 voice poly CV outs (voices 9-16)
- 16-voice aggregated gate outs (all voices 1-16?)
- 16-voice aggregated CV outs (all voices 1-16?)

**Inputs:**
- 8 × Voice probability modulation ins (voices 9-16)
- 8 × Voice probability attenuverters

**Validation:**
- **ONLY VALID if Straits East is also present**
- If East disconnected, West should disable itself
- Expander chain: Main → East → West

**Questions:**
- How to enforce "East must be present" requirement?
  - Check during expander detection?
  - Disable with visual indicator if East missing?
  - Both on same side, or West can be on opposite side?

---

### 3. SANDS EXPANDER (renamed from DNA Expander)

**Purpose:** DNA operations for mono voice only

**No changes from current MeloDicerDNAExpander**
- Just rename it to "Sands"
- Functionality identical

---

### 4. STRAITS SANDS EXPANDER (new)

**Purpose:** DNA operations for poly voices 2-16

**Inputs per Voice (voices 2-16):** 15 voices × 2 controls
```
For each voice 2-16:
  ├─ Length offset modulation in
  ├─ Length offset attenuverter
  ├─ Rotation modulation in
  └─ Rotation attenuverter
```

**CRITICAL QUESTION: Discrete vs Continuous?**

**Option A: Discrete (16 levels)**
- Like note input quantization
- User provides CV 0-10V → mapped to 16 levels
- Cleaner UI, predictable behavior
- Pros: Matches Rack conventions, easier to display
- Cons: Less expressive if user wants smooth modulation

**Option B: Continuous**
- Raw modulation signal applied directly
- Smoother parameter changes
- Pros: More expressive, musical
- Cons: Harder to display state, might be confusing

**Recommendation:** Discrete (16 levels) makes sense because:
- DNA length/rotation are discrete parameters (you can't have 3.5 note rotation)
- Matches scale note quantization pattern already in codebase
- User can use quantizer before modulation in if they want that behavior
- Cleaner UI representation

**Behavior:**
- Works with Straits East (voices 2-8)
- Works with Straits West (voices 9-16)
- Works with both present (voices 2-16)
- No effect for voices without expander attached
- Each voice independent

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────┐
│ MeloDicer Main (Mono - Voice 1)                     │
├─────────────────────────────────────────────────────┤
│ • Gate/CV outputs                                   │
│ • Mono DNA operations (Sands required)              │
│ • Voice 1 probability modulation                    │
└─────────────────────────────────────────────────────┘
         │ RIGHT EXPANDER
         ▼
┌─────────────────────────────────────────────────────┐
│ Straits East Expander (Voices 2-8)                  │
├─────────────────────────────────────────────────────┤
│ Outputs:                                             │
│  • 8 poly gate (voices 2-8)                         │
│  • 8 poly CV (voices 2-8)                           │
│  • Mono gate, Mono CV                               │
│                                                      │
│ Inputs (one per voice 2-8):                         │
│  • Voice probability mod in + attenuverter (×8)     │
└─────────────────────────────────────────────────────┘
         │ RIGHT EXPANDER (if 16 voices needed)
         ▼
┌─────────────────────────────────────────────────────┐
│ Straits West Expander (Voices 9-16)                 │
├─────────────────────────────────────────────────────┤
│ Outputs:                                             │
│  • 8 poly gate (voices 9-16)                        │
│  • 8 poly CV (voices 9-16)                          │
│  • 16 poly gate (all voices 1-16)                   │
│  • 16 poly CV (all voices 1-16)                     │
│                                                      │
│ Inputs (one per voice 9-16):                        │
│  • Voice probability mod in + attenuverter (×8)     │
│                                                      │
│ Validation:                                          │
│  ⚠️ REQUIRES Straits East to be present!           │
└─────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────┐
│ Sands Expander (Mono DNA) [LEFT or RIGHT]           │
├─────────────────────────────────────────────────────┤
│ • DNA length offset modulation + attenuverter       │
│ • DNA rotation modulation + attenuverter            │
│ • Mono voice DNA control                            │
└─────────────────────────────────────────────────────┘
         │ RIGHT EXPANDER (for poly DNA)
         ▼
┌─────────────────────────────────────────────────────┐
│ Straits Sands Expander (Poly DNA - Voices 2-16)     │
├─────────────────────────────────────────────────────┤
│ For each voice 2-16:                                │
│  • Length offset mod in + attenuverter              │
│  • Rotation mod in + attenuverter                   │
│                                                      │
│ Discrete 16-level mapping for both controls        │
│                                                      │
│ No effect for voices without East/West attached     │
└─────────────────────────────────────────────────────┘
```

---

## Implementation Roadmap

### Phase 1: Rename & Structure
1. Rename MeloDicerDNAExpander → Sands Expander
2. Rename MeloDicerPolyVoiceExpander → Straits East Expander
3. Update model names in plugin registry
4. Update all references

### Phase 2: Extend Engine for 16 Voices
1. Update SequencerEngine to support 16 poly voices
2. Extend PatternEngine for 16 voice seeds
3. Create 16-voice gate/CV arrays
4. Update voice probability array to 16 elements

### Phase 3: Straits West Expander
1. Create MeloDicerStraitWestExpander class
2. Implement 8-voice outputs (voices 9-16)
3. Implement 16-voice aggregated outputs
4. Add validation: requires East expander
5. Add expander chaining detection

### Phase 4: Voice Probability Modulation
1. Add modulation inputs to Straits East (8 voices)
2. Add modulation inputs to Straits West (8 voices)
3. Add attenuverters for each
4. Integrate with voice probability calculation

### Phase 5: Straits Sands Expander
1. Create MeloDicerStraitSandsExpander class
2. Create DNA control for 15 poly voices (voices 2-16)
3. Implement discrete 16-level mapping for:
   - Length offset modulation
   - Rotation modulation
4. Add attenuverters for each control
5. No-op if expanders not present

### Phase 6: Integration & Testing
1. Update main expander detection to handle all three types
2. Test all combinations:
   - East alone
   - East + West
   - Sands alone
   - Sands + Straits Sands
   - All together
3. Validate West requires East
4. Verify 16-voice aggregation

---

## Technical Questions Needing Answers

### 1. Voice Numbering
- Main module: voice 1 (mono)
- Straits East outputs: labeled voices 1-8, 2-8, or just indices?
- Straits West outputs: labeled voices 9-16 or 1-8?
- Aggregated West outputs: voices 1-16 or just aggregation?

### 2. Straits West Validation
- How strict is the requirement for East?
- Should West be disabled/grayed if East not present?
- Can user ignore validation and use 8 voices separately?
- Visual indicator of missing East?

### 3. Straits Sands DNA Modulation
- Discrete (16 levels) or continuous?
- If discrete: user has 16 length offsets and 16 rotations?
- If continuous: how to represent scale vs actual value?
- Should there be visual display of current level?

### 4. Poly Voice Probability Modulation
- How does modulation affect probability?
- Is it added to base probability? (prob + mod)
- Is it multiplied? (prob × (1 + mod))
- Range of modulation inputs? (0-10V? -5 to 5V?)
- What does attenuverter control? (amount? inversion?)

### 5. Mono Out on Straits East
- Should mono outputs duplicate main module outputs?
- Or are they redundant/unnecessary?
- Why include them in East vs just in main?

### 6. 16-Voice Outputs on Straits West
- How are 16 voices packed into poly outputs?
- All on same output port (16-channel poly)?
- Or separate outs for voices 1-8 vs 9-16?
- For integration with external modules?

---

## Data Structure Changes

### Current SequencerEngine
```cpp
struct Voice {
    GateState gs;
    float restProb;
    // ... other state
};

Voice voices[7];  // Indices 0-6 for voices 2-7
int numPolyVoices = 6;
```

### New SequencerEngine
```cpp
struct Voice {
    GateState gs;
    float restProb;
    float probabilityModulation = 0.f;  // NEW: -1.0 to 1.0
    // ... other state
};

Voice voices[15];  // Indices 0-14 for voices 2-16
int numPolyVoices = 0-15;  // 0 = mono only, up to 15 = all voices
```

### DNA Structures
```cpp
struct DNAState {
    int lengthOffsets[15];      // Voice 2-16
    int rotations[15];           // Voice 2-16
    int monoLengthOffset;        // Voice 1 only
    int monoRotation;            // Voice 1 only
};
```

---

## File Structure Changes

**New expanders needed:**
```
src/
├── MeloDicerStraitEastExpander.cpp/hpp  (renamed from PolyVoice)
├── MeloDicerStraitWestExpander.cpp/hpp  (new)
└── MeloDicerStraitSandsExpander.cpp/hpp (new)
```

**Renamed:**
```
MeloDicerDNAExpander → MeloDicerSandsExpander
```

**Updated:**
```
src/MeloDicer.cpp/hpp           (extend to 16 voices)
src/dsp/engines/*               (extend for 16 voices)
src/MeloDicerWidget.cpp/hpp     (new expanders)
```

---

## Next Steps

Before implementation:

1. **Clarify discrete vs continuous** for DNA modulation
2. **Confirm voice numbering** scheme
3. **Define West validation** strictness
4. **Specify poly probability modulation** behavior
5. **Detail 16-voice output packing** on West expander

Once clarified, implementation can start with Phase 1.

