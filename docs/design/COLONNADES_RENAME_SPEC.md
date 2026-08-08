# Colonnades rename -- Micro12 -> Colonnades (pre-freeze, do now)

Apply the real name. "Micro 12" was a working title; the module is COLONNADES (Micro-12, Phase 2 of
the microtonal family: Sikit / Colonnades / Colonnades Duo). The 24-tone Phase 3 module will be
COLONNADES DUO. Rodney also wants the FILES named colonnades (the current MonsoonMicro12 filenames
are a naming regret -- fixable now).

## TIMING: free now, permanent later
The slug `MonsoonMicro12` is ONLY on feat/microtonal -- never merged to master, never shipped to the
VCV library. So the slug is NOT frozen yet. Renaming now costs nothing (no migration, no broken
patches). After library submission the slug is permanent. DO IT NOW.

## Naming decision (Rodney): class is `Colonnades`, 12 lives as a degree constant
- Internal class name: `Colonnades` (NOT ColonnadesMicro12, NOT Colonnades12). The "12" is NOT in the
  class name.
- The degree count lives as the constant `N_DEGREES = 12` inside, where it is semantically meaningful.
- The future 24-tone module will be `ColonnadesDuo` with `N_DEGREES = 24` (or a shared/parameterised
  base -- decided when the Duo is built; this rename just keeps the name clean so that's possible).
- Rationale: keeps the class name clean AND preserves the 12-vs-24 distinction where it belongs (the
  constant), consistent with the lesson from the redDot/ChangeAlleyV2 accidental-internal-names cleanup
  -- don't let a working name with an embedded number ossify.

## Exact identifier mapping (7 distinct symbols, 52 occurrences)
| Old | New |
|-----|-----|
| `MonsoonMicro12` (class) | `Colonnades` |
| `MonsoonMicro12Widget` | `ColonnadesWidget` |
| `Micro12Ids` (namespace) | `ColonnadesIds` |
| `Micro12CentsDisplay` | `ColonnadesCentsDisplay` |
| `Micro12Labels` | `ColonnadesLabels` |
| `Micro12Expander` | `ColonnadesExpander` |
| `modelMonsoonMicro12` | `modelColonnades` |

## Files to rename
| Old | New |
|-----|-----|
| `src/MonsoonMicro12.cpp` | `src/Colonnades.cpp` |
| `src/MonsoonMicro12.hpp` | `src/Colonnades.hpp` |
| `panel_src/gen_monsoon_micro_12.py` | `panel_src/gen_colonnades.py` |
| `res/panels/MonsoonMicro12_panel_dark.svg` | `res/panels/Colonnades_panel_dark.svg` |
| `res/panels/MonsoonMicro12_panel_light.svg` | `res/panels/Colonnades_panel_light.svg` |
(use `git mv` to preserve history)

## Strings to change
- `plugin.json`: `"slug": "MonsoonMicro12"` -> `"slug": "Colonnades"`;
  `"name": "Micro 12"` -> `"name": "Colonnades"`.
- Panel wordmark: `MonsoonMicro12.cpp` (now Colonnades.cpp) draws "Micro 12" via nvgText
  (currently ~line 93) -> "Colonnades". Keep the "tuning + scale" subtitle.
- The panel-gen script's wordmark/output paths -> Colonnades + the new SVG filenames.
- `#include "MonsoonMicro12.hpp"` -> `#include "Colonnades.hpp"` (Colonnades.cpp + any other includer).
- Comment at Monsoon.cpp:1110 (`modelMonsoonMicro12 // microtonal Phase 2`) -> modelColonnades.

## Method (mechanical, but compile-verify)
1. `git mv` the 5 files to their Colonnades names.
2. sed the 7 identifiers across src/ + plugin.json (longest-first to avoid partial overlaps:
   MonsoonMicro12Widget before MonsoonMicro12; Micro12CentsDisplay/Labels/Expander/Ids before Micro12).
3. Change the two plugin.json strings + the wordmark text + the include paths + the gen-script output
   paths.
4. Regenerate the panels from gen_colonnades.py so the SVGs carry the Colonnades wordmark and land at
   the new res/panels/Colonnades_panel_*.svg paths.
5. Compile-verify (Rack build). The 52-occurrence count is the checklist: grep for any residual
   Micro12/MonsoonMicro12 after -- should be ZERO in src/ and plugin.json (docs may still reference
   "Micro-12" as the conceptual phase name; that's fine, but code/slug/assets must be clean).
6. Rack-verify the module still loads, panel shows "Colonnades", faders/knobs/display all bind.

## Note on docs
Design docs may keep saying "Micro-12" as the PHASE/CONCEPT name (Sikit=Phase1, Micro-12/Colonnades=
Phase2, Micro-24/Colonnades Duo=Phase3) -- that's the conceptual vocabulary and is fine. Only the
CODE, SLUG, ASSETS, and USER-FACING NAME must become Colonnades. The MONSOON_MICRO_SPEC /
MONSOON_MICRO_CLAUDE_CODE_GUIDE keep their names (they cover both Micros conceptually).

## Cross-refs
- plugin.json:110-111 -- slug + name.
- src/MonsoonMicro12.cpp:93 -- wordmark nvgText.
- src/Monsoon.cpp:1110 -- addModel registration.
- COLONNADES_PANEL_LIFT_SPEC.md -- the panel work this names.
- MICROTONAL_MASTER.md -- the naming state (Sikit/Colonnades/Colonnades Duo).
