# 16-Voice Polyphony Expansion - All Decisions Finalized

## Final Architecture Decisions

### Q1: DNA Modulation ✅
**DECISION: CONTINUOUS**
- Continuous modulation for smooth control
- Raw CV applied to length offset and rotation
- Attenuverter for scaling/inverting the modulation

### Q2: Voice Probability Modulation ✅
**DECISION: ADDITIVE**
```
final_probability = base_probability + modulation
```
- Consistent with note sliders
- Consistent with mono variation (rest, legato mod)
- Attenuverter controls amount and inversion
- 0V = no modulation effect
- Positive CV = increase probability
- Negative CV = decrease probability

### Q3: Straits West Validation ✅
**DECISION: HARD REQUIREMENT (Option A)**
- West is DISABLED if East not present
- Visual indicator (grayed out/warning)
- Returns zero outputs if East missing
- Enforced at architectural level

### Q4: Voice Numbering & Outputs ✅

**Straits East Expander (voices 2-8):**
```
Mono Outputs:
  - Gate 2-8 (7 voices)
  - CV 2-8 (7 voices)
  
Poly Outputs:
  - 8-voice poly gate (voice 1 + voices 2-8)
    Voice 1 = mono, Voices 2-8 = poly
  - 8-voice poly CV (voice 1 + voices 2-8)
```

**Straits West Expander (voices 9-16):**
```
8-Voice Poly Outputs (voices 9-16):
  - 8-voice poly gate for voices 9-16
  - 8-voice poly CV for voices 9-16
  
16-Voice Aggregated Outputs (all voices):
  - 16-voice poly gate for voices 1-16
    (voice 1 = mono, voices 2-8 = East, voices 9-16 = West)
  - 16-voice poly CV for voices 1-16
    (voice 1 = mono, voices 2-8 = East, voices 9-16 = West)
```

**Visual Summary:**
```
STRAITS EAST Outputs:
  Gate Out 2-8 ────────┐
  CV Out 2-8 ──────────┤
  Poly Gate 1-8 ───────┼─ (voice 1 = mono, 2-8 = poly)
  Poly CV 1-8 ────────┘

STRAITS WEST Outputs:
  Poly Gate 9-16 ──────┐
  Poly CV 9-16 ────────┤
  Poly Gate 1-16 ──────┼─ (voices 1-16 all voices)
  Poly CV 1-16 ───────┘
```

### Q5: Mono Outputs on Straits East ✅
**DECISION: NO MONO OUTPUTS**
- East focuses on voices 2-8 only
- Voice 1 (mono) outputs stay on main module
- No duplication, cleaner design

### Q6: Straits West Output Structure ✅
**DECISION: As described in Q4**
- 8-voice poly outs for voices 9-16
- 16-voice poly outs for voices 1-16 (all voices)
- Provides both granular (just West voices) and aggregate (all voices)

---

## Implementation Roadmap - Ready to Execute

### Phase 1: Rename & Structure ✅ READY NOW
**Depends on:** Nothing - safe to do immediately
**Duration:** 2-3 commits
**Changes:**
- Rename MeloDicerDNAExpander → MeloDicerSandsExpander
- Rename MeloDicerPolyVoiceExpander → MeloDicerStraitEastExpander
- Update all references in MeloDicer.cpp/hpp
- Update model registration in plugin.cpp
- Update widget registration

**Files affected:**
- src/MeloDicerDNAExpander.cpp/hpp → src/MeloDicerSandsExpander.cpp/hpp
- src/MeloDicerPolyVoiceExpander.cpp/hpp → src/MeloDicerStraitEastExpander.cpp/hpp
- src/MeloDicer.cpp/hpp
- src/MeloDicerWidget.cpp/hpp
- plugin.cpp (model registration)

### Phase 2: Extend Engine for 16 Voices ✅ READY AFTER PHASE 1
**Depends on:** Phase 1
**Duration:** 4-6 commits
**Changes:**
- SequencerEngine: Voice voices[15] (was 7, now 15)
- PatternEngine: 16-voice seed arrays (1 mono + 15 poly)
- GateState: 15 instances for poly voices
- Voice probability array: 16 elements (1 mono + 15 poly)
- Add probabilityModulation field to Voice struct

**Files affected:**
- src/dsp/engines/SequencerEngine.hpp/cpp
- src/dsp/engines/PatternEngine.hpp/cpp
- src/dsp/gates/GateState.hpp/cpp
- src/MeloDicer.hpp/cpp

### Phase 3: Update Straits East Expander ✅ READY AFTER PHASE 2
**Depends on:** Phase 2
**Duration:** 3-4 commits
**Changes:**
- Update MeloDicerStraitEastExpander for voices 2-8 (7 voices)
- Add 8-voice poly gate outputs (voice 1-8)
- Add 8-voice poly CV outputs (voice 1-8)
- Add mono gate/CV outputs for voices 2-8
- Add 8 × probability modulation inputs
- Add 8 × attenuverters for probability mod

**Note:** East still only handles voices 2-8, but outputs include voice 1

### Phase 4: Create Straits West Expander ✅ READY AFTER PHASE 3
**Depends on:** Phase 3
**Duration:** 4-5 commits
**Changes:**
- Create MeloDicerStraitWestExpander class
- Implement 8-voice poly gate outputs (voices 9-16)
- Implement 8-voice poly CV outputs (voices 9-16)
- Implement 16-voice aggregated poly gate outputs (1-16)
- Implement 16-voice aggregated poly CV outputs (1-16)
- Add 8 × probability modulation inputs (voices 9-16)
- Add 8 × attenuverters
- Add validation: check East expander presence
- Add visual indicator if East missing

### Phase 5: Voice Probability Modulation Integration ✅ READY AFTER PHASE 4
**Depends on:** Phase 4
**Duration:** 2-3 commits
**Changes:**
- Update voice probability calculation: final_prob = base_prob + modulation
- Read modulation inputs from East and West
- Apply attenuverter scaling
- Handle missing/disconnected modulation inputs
- Clamp final probability to [0, 1]

### Phase 6: Create Straits Sands Expander ✅ READY AFTER PHASE 5
**Depends on:** Phase 5
**Duration:** 3-4 commits
**Changes:**
- Create MeloDicerStraitSandsExpander class
- Implement continuous modulation inputs for:
  - Length offset (15 voices × 1 input)
  - Rotation (15 voices × 1 input)
- Implement 15 × 2 attenuverters
- Apply modulation to DNA operations
- Handle cases where not all expanders present

### Phase 7: Update Sands Expander ✅ READY AFTER PHASE 6
**Depends on:** Phase 6
**Duration:** 2-3 commits
**Changes:**
- Update MeloDicerSandsExpander (renamed from DNA)
- Add continuous modulation inputs:
  - Length offset modulation in
  - Rotation modulation in
- Add 2 × attenuverters for mono DNA control
- Ensure only affects voice 1

### Phase 8: Integration & Testing ✅ READY AFTER PHASE 7
**Depends on:** All phases
**Duration:** 3-5 commits
**Test scenarios:**
1. East alone (voices 2-8 active)
2. East + West (voices 2-16 active, East required)
3. West without East (disabled/warning)
4. Sands alone (mono DNA)
5. Sands + Straits Sands (mono + poly DNA)
6. All expanders together
7. Probability modulation working
8. DNA continuous modulation working
9. Voice outputs correct (1-8 on East, 9-16 on West, 1-16 aggregated)

---

## Summary of Changes

### Total Implementation
- **8 phases** (well-structured, testable at each step)
- **~25-30 commits** total
- **6 files renamed/created** (Phase 1)
- **3 new expanders** (Straits West, Straits Sands, updated Sands)
- **Extended to 16 voices** across all engines
- **Continuous modulation** for DNA parameters
- **Additive probability modulation** from inputs

### Voice Count
```
Current: 1 (mono) + 6 (East) = 7 voices
After Ph2: 1 (mono) + 15 (poly) = 16 voices
After Ph3: Same (East handles 2-8)
After Ph4: Same + West outputs (9-16)
After Ph8: Full 16-voice implementation
```

### Expanders After Implementation
```
Main Module (Mono - Voice 1)
  ├─ Sands Expander (Mono DNA with continuous mod)
  └─ Straits Sands Expander (Poly DNA 2-16 with continuous mod)
     └─ Requires East/West for voices it affects

Polyphony Chain:
  Main (voice 1) → Straits East (voices 2-8) → Straits West (voices 9-16)
```

---

## Ready for Phase 1?

✅ **YES** - All decisions locked in
✅ **Phase 1 is safe** - Just renaming, no new functionality
✅ **Can start immediately** - No dependencies
✅ **Low risk** - Mechanical refactoring

**Recommendation:** Start Phase 1 right away
- Rename DNA → Sands
- Rename PolyVoice → Straits East
- Get all references updated
- Build and verify

Then Phase 2 extends to 16-voice engine.

