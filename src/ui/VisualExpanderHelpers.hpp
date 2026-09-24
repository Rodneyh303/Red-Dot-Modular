#pragma once
#include <rack.hpp>
#include "../Monsoon.hpp"
#include "SandsVisualEditorV4.hpp"
#include "MonsoonDiscovery.hpp"   // segment-rule walk (CONNECTION_MODEL_SPEC.md §1)

using namespace rack;
using namespace MonsoonIds;

namespace redDot {

// ── Chain-walk: find Monsoon anywhere to the right ───────────────────────────
inline Monsoon* findMonsoon(rack::Module* startRight, int maxDepth = 12) {
    Module* curr = startRight;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (auto* m = dynamic_cast<Monsoon*>(curr)) return m;
        curr = curr->rightExpander.module;
    }
    return nullptr;
}

// ── Chain-walk: find Monsoon on EITHER side (SEGMENT RULE) ───────────────────
// CONNECTION_MODEL_SPEC.md §1: an expander binds to the Monsoon in its SEGMENT —
// the run of modules reachable by expander hops, bounded on each side by the
// first FOREIGN module (a non-suite module) OR the next Monsoon. Walk right, then
// left; each side stops at a foreign boundary and returns the first Monsoon it
// reaches. Recognised suite modules (Interchange, Lantern, Sikit, …) are hopped
// THROUGH, so a suite module between this expander and its Monsoon does not block
// binding — but a foreign module does (correctly: they're in different segments).
//
// This REPLACES the old right-first walk-through-anything behaviour whose result
// depended on row ORDER (CONNECTION_UI_MODEL.md §4 bug class). The result is now a
// function of TOPOLOGY, not position: reordering the row cannot change the lit set.
inline Monsoon* findMonsoonEitherSide(rack::Module* self, int maxDepth = 12) {
    rack::Module* host = findHostBothSides(
        self,
        [](rack::Module* m) { return m->leftExpander.module; },
        [](rack::Module* m) { return m->rightExpander.module; },
        [](rack::Module* m) { return dynamic_cast<Monsoon*>(m) != nullptr; },  // isHost
        [](rack::Module* m) { return isSuiteChainModel(m->model); },           // isSuite (hop-through)
        maxDepth);
    return dynamic_cast<Monsoon*>(host);
}

// True only if `self` is the expander Monsoon has actually CLAIMED for its type.
// Monsoon caches the FIRST module of each type in its chain (one pointer per
// type), so when several expanders of the same type are placed in a row, only one
// is functionally connected. The connect mark must reflect that — otherwise every
// duplicate lights up as if connected. Compares `self` against the matching
// cached pointer by address (the cached slots are stored as the concrete module
// pointers, so an address compare is valid across the reinterpret_cast).
inline bool isClaimedExpander(rack::Module* self, Monsoon* mon) {
    if (!self || !mon) return false;
    const auto& em = mon->expanderManager;
    const void* s = static_cast<const void*>(self);
    return s == (const void*)em.cachedScaleExpander
        || s == (const void*)em.cachedRafflesExpander
        || s == (const void*)em.cachedJunctionExpander
        || s == (const void*)em.cachedSandsVisualExpander
        || s == (const void*)em.cachedPolyVoiceExpander
        || s == (const void*)em.cachedCausewayPolyExpander
        || s == (const void*)em.cachedChangiExpander
        || s == (const void*)em.cachedChangiT2Expander
        || s == (const void*)em.cachedShophouseExpander
        || s == (const void*)em.cachedEastSandsVisual
        || s == (const void*)em.cachedMacroSandsVisual
        || s == (const void*)em.cachedChangeAlleyV2;
}

// Convenience for the connect mark: this expander is "connected" iff a Monsoon is
// reachable AND it is the claimed one of its type.
inline bool isConnectedAndClaimed(rack::Module* self) {
    Monsoon* mon = findMonsoonEitherSide(self);
    return mon && isClaimedExpander(self, mon);
}

// ── Per-lane playhead ─────────────────────────────────────────────────────────
// Returns the PHYSICAL bar (0..15) the sequencer actually reads after LOR, to
// match SequencerEngine::getStrandIdx exactly:
//     timelineIdx = (globalStep + rotation) mod length
//     physicalBar = (timelineIdx + offset) mod 16
// so the highlight lands on the real active block, not a window-relative index.
// Returns -1 if globalStep < 0 (sequencer not running).
inline int calcPlayhead(int globalStep, int length, int offset, int rotation) {
    if (globalStep < 0) return -1;
    length = std::max(1, std::min(length, 16));
    int timelineIdx = ((globalStep + rotation) % length + length) % length;
    return (timelineIdx + offset) % 16;
}

// Read an integer param (rounded) from a module, clamped to [1, 16].
// Returns `fallback` if the module pointer is null.
inline int readLenParam(rack::Module* mod, int paramId, int fallback = 16) {
    if (!mod) return fallback;
    return std::max(1, std::min(16, (int)std::round(mod->params[paramId].getValue())));
}
inline int readOffRotParam(rack::Module* mod, int paramId, int fallback = 0) {
    if (!mod) return fallback;
    return (int)std::round(mod->params[paramId].getValue());
}

}  // namespace redDot
