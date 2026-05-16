# Singapore Branding Refactoring Guide

## Overview

Rename MeloDicer modules to Singapore-inspired names that evoke the Red Dot Modular brand.

**Mapping:**
```
MeloDicer                    → Monsoon   (main sequencer - evokes tropical season)
MeloDicerExpander           → Interchange (modulation expander - evokes trading hub)
MeloDicerPolyVoiceExpander  → Straits    (poly expander - evokes Straits of Malacca)
MeloDicerDNAExpander        → Sands      (DNA expander - evokes Singapore sand/beaches)
```

---

## Files to Rename

```bash
# Main module
src/MeloDicer.hpp            → src/Monsoon.hpp
src/MeloDicer.cpp            → src/Monsoon.cpp
src/MeloDicerWidget.hpp       → src/MonsoonWidget.hpp
src/MeloDicerWidget.cpp       → src/MonsoonWidget.cpp

# Expanders
src/MeloDicerExpander.hpp     → src/Interchange.hpp
src/MeloDicerExpander.cpp     → src/Interchange.cpp

src/MeloDicerPolyVoiceExpander.hpp  → src/Straits.hpp
src/MeloDicerPolyVoiceExpander.cpp  → src/Straits.cpp

src/MeloDicerDNAExpander.hpp   → src/Sands.hpp
src/MeloDicerDNAExpander.cpp   → src/Sands.cpp
```

---

## Class Names to Update

### Monsoon (Main Sequencer)

**Header: src/Monsoon.hpp**
- `class MeloDicer : public rack::engine::Module` → `class Monsoon : public rack::engine::Module`
- `struct MeloDicerIds` → `struct MonsoonIds` (namespace for param/input/output/light IDs)
- `class MeloDicerWidget : public rack::app::ModuleWidget` → `class MonsoonWidget : public rack::app::ModuleWidget`

**Implementation: src/Monsoon.cpp**
- `Model* modelMeloDicer = createModel<MeloDicer, MeloDicerWidget>("MeloDicer");` 
  → `Model* modelMonsoon = createModel<Monsoon, MonsoonWidget>("Monsoon");`
- All `MeloDicerIds::` → `MonsoonIds::`
- All `MeloDicer::` function definitions → `Monsoon::`

**Widget: src/MonsoonWidget.cpp**
- All class and function references updated accordingly

### Interchange (Modulation Expander)

**Header: src/Interchange.hpp**
- `class MeloDicerExpander : public rack::engine::Module` → `class Interchange : public rack::engine::Module`
- `struct MeloDicerExpanderIds` → `struct InterchangeIds`

**Implementation: src/Interchange.cpp**
- `Model* modelMeloDicerExpander = createModel<MeloDicerExpander, InterchangeWidget>("MeloDicerExpander");`
  → `Model* modelInterchange = createModel<Interchange, InterchangeWidget>("Interchange");`
- All function definitions updated

### Straits (Poly Voice Expander)

**Header: src/Straits.hpp**
- `class MeloDicerPolyVoiceExpander : public rack::engine::Module` → `class Straits : public rack::engine::Module`
- `struct MeloDicerPolyVoiceExpanderIds` → `struct StraitsIds`

**Implementation: src/Straits.cpp**
- `Model* modelMeloDicerPolyVoiceExpander = createModel<MeloDicerPolyVoiceExpander, StraitsWidget>("MeloDicerPolyVoiceExpander");`
  → `Model* modelStraits = createModel<Straits, StraitsWidget>("Straits");`

### Sands (DNA Expander)

**Header: src/Sands.hpp**
- `class MeloDicerDNAExpander : public rack::engine::Module` → `class Sands : public rack::engine::Module`
- `struct MeloDicerDNAExpanderIds` → `struct SandsIds`

**Implementation: src/Sands.cpp**
- `Model* modelMeloDicerDNAExpander = createModel<MeloDicerDNAExpander, SandsWidget>("MeloDicerDNAExpander");`
  → `Model* modelSands = createModel<Sands, SandsWidget>("Sands");`

---

## Includes to Update

### In Monsoon.hpp/cpp
```cpp
// OLD:
#include "MeloDicerWidget.hpp"
#include "MeloDicerExpander.hpp"
#include "MeloDicerPolyVoiceExpander.hpp"
#include "MeloDicerDNAExpander.hpp"
#include "MeloDicerDNAManager.hpp"

// NEW:
#include "MonsoonWidget.hpp"
#include "Interchange.hpp"
#include "Straits.hpp"
#include "Sands.hpp"
#include "MonsoonDNAManager.hpp"
```

### In Widget files
```cpp
// OLD:
ExpanderHandle cachedExpander{RIGHT, "MeloDicer"};
ExpanderHandle cachedPolyVoiceExpander{RIGHT, "MeloDicer"};
ExpanderHandle cachedDnaExpander{LEFT, "MeloDicer"};

// NEW:
ExpanderHandle cachedExpander{RIGHT, "Monsoon"};
ExpanderHandle cachedPolyVoiceExpander{RIGHT, "Monsoon"};
ExpanderHandle cachedDnaExpander{LEFT, "Monsoon"};
```

### In Expander headers
```cpp
// In Interchange.hpp
// OLD: rack::engine::ExpanderHandle handle{LEFT, "MeloDicer"};
// NEW: rack::engine::ExpanderHandle handle{LEFT, "Monsoon"};

// Same for Straits.hpp and Sands.hpp
```

---

## Model Registration in plugin.cpp / init() function

**OLD:**
```cpp
void init(rack::Plugin* p) {
    pluginInstance = p;
    p->addModel(modelMeloDicer);
    p->addModel(modelMeloDicerExpander);
    p->addModel(modelMeloDicerDNAExpander);
    p->addModel(modelMeloDicerPolyVoiceExpander);
}
```

**NEW:**
```cpp
void init(rack::Plugin* p) {
    pluginInstance = p;
    p->addModel(modelMonsoon);
    p->addModel(modelInterchange);
    p->addModel(modelSands);
    p->addModel(modelStraits);
}
```

---

## External References to Update

### ExpanderMessage struct
If present in headers, update:
```cpp
// OLD: struct MeloDicerExpanderMessage
// NEW: struct InterchangeMessage, StraitsMessage, SandsMessage
```

### Plugin.hpp / plugin.hpp
```cpp
// Add extern declarations:
extern Model* modelMonsoon;
extern Model* modelInterchange;
extern Model* modelStraits;
extern Model* modelSands;
```

---

## Step-by-Step Execution Plan

### Phase 1: File Renames
```bash
git mv src/MeloDicer.hpp src/Monsoon.hpp
git mv src/MeloDicer.cpp src/Monsoon.cpp
git mv src/MeloDicerWidget.hpp src/MonsoonWidget.hpp
git mv src/MeloDicerWidget.cpp src/MonsoonWidget.cpp
git mv src/MeloDicerExpander.hpp src/Interchange.hpp
git mv src/MeloDicerExpander.cpp src/Interchange.cpp
git mv src/MeloDicerPolyVoiceExpander.hpp src/Straits.hpp
git mv src/MeloDicerPolyVoiceExpander.cpp src/Straits.cpp
git mv src/MeloDicerDNAExpander.hpp src/Sands.hpp
git mv src/MeloDicerDNAExpander.cpp src/Sands.cpp
git mv src/MeloDicerDNAManager.hpp src/MonsoonDNAManager.hpp
git mv src/MeloDicerDNAManager.cpp src/MonsoonDNAManager.cpp
```

### Phase 2: Update Class/Namespace Names

**In each file, systematically replace:**
- `class MeloDicer` → `class Monsoon`
- `MeloDicerIds` → `MonsoonIds`
- `MeloDicerWidget` → `MonsoonWidget`
- `MeloDicerExpander` → `Interchange`
- `MeloDicerExpanderIds` → `InterchangeIds`
- `MeloDicerPolyVoiceExpander` → `Straits`
- `MeloDicerPolyVoiceExpanderIds` → `StraitsIds`
- `MeloDicerDNAExpander` → `Sands`
- `MeloDicerDNAExpanderIds` → `SandsIds`
- `MeloDicerDNAManager` → `MonsoonDNAManager`

### Phase 3: Update Model Variables & Registration
- Replace all `modelMeloDicer*` variables with new names
- Update plugin init() function
- Update model registrations

### Phase 4: Update All Includes
- `#include "MeloDicer*"` → `#include "Monsoon*"` or appropriate new name
- Update include guards: `#pragma once` sections

### Phase 5: Test & Commit
```bash
# Should build cleanly
make

# If all good:
git commit -m "refactor: rename modules to Singapore branding

Monsoon (main sequencer) - evokes tropical monsoon seasons
Straits (poly expander) - evokes Straits of Malacca
Interchange (modulation expander) - evokes trading hub character
Sands (DNA expander) - evokes Singapore beaches

No functional changes, pure naming refactoring."
```

---

## Verification Checklist

After completing all renames:

- [ ] All `#include` statements resolve correctly
- [ ] No compilation errors
- [ ] All class definitions match their new names
- [ ] All model registrations use new model variables
- [ ] Expander handles reference correct module name ("Monsoon" not "MeloDicer")
- [ ] Parameter/input/output IDs use correct namespace (MonsoonIds, etc.)
- [ ] Documentation updated with new names

---

## Risk Mitigation

This refactoring is **pure naming** with zero functional changes. Key risk:
- **Include loops**: Ensure all `#include` directives are updated consistently
- **Expander references**: Must use correct parent module name for left/right expander connections

**Testing strategy:**
1. Build without errors
2. Instantiate main module (Monsoon) - should work
3. Attach each expander - should connect correctly
4. Test DNA strand operations
5. Test poly voices
6. Test parameter modulation

---

## Success Criteria

✅ All modules load correctly  
✅ Expanders connect to parent module  
✅ All operations work identically  
✅ No new compiler warnings  
✅ Git history shows clean refactoring commit  

---

**Estimated effort:** ~60–90 minutes  
**Risk level:** Low (pure refactoring, zero functional changes)  
**Testing time:** ~15 minutes (basic smoke test)
