// =============================================================================
//  MonsoonSandsVisualExpander — RETIRED (sands-consolidation Step 6)
// =============================================================================
//  The Mono Sands visual module is subsumed by the Macro visual (now 7 lanes +
//  mono mode). The full source — widget + process() + createModel — has been
//  moved to src/deprecated/MonsoonSandsVisualExpander.cpp and is no longer
//  compiled. The slug is removed from plugin.json and the model is unregistered
//  in plugin.cpp, so the module cannot appear in the browser nor be created.
//
//  WHY THIS STUB STILL EXISTS
//  -------------------------
//  The module is still *detected* by topology / expander-manager code:
//    • MonsoonExpanderManager  — `curr->model == modelMonsoonSandsVisualExpander`
//      populates cachedSandsVisualExpander.
//    • MonsoonDiscovery         — `m == modelMonsoonSandsVisualExpander`.
//    • MonsoonSandsManager / StraitsEast / StraitsSandsMacroVisual — read
//      cachedSandsVisualExpander and the SandsMonoVisualIds namespace.
//  Per the consolidation plan those checks are left in place as inert no-ops:
//  modelMonsoonSandsVisualExpander is nullptr (never matched), so
//  cachedSandsVisualExpander is never populated and every `!= nullptr` guard
//  short-circuits. No dependency breaks.
//
//  The header MonsoonSandsVisualExpander.hpp stays in src/ because it owns the
//  SandsMonoVisualIds namespace (id enums + inline helpers) and the struct type
//  still referenced by the managers and the East/Macro visuals.
//
//  process() is defined empty so the vtable's key function resolves; it is
//  never invoked because no instance can be constructed.
// =============================================================================
#include "MonsoonSandsVisualExpander.hpp"

void MonsoonSandsVisualExpander::process(const ProcessArgs&) {
    // Retired — no behaviour. Cannot be instantiated (model unregistered).
}

rack::Model* modelMonsoonSandsVisualExpander = nullptr;
