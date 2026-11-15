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

// True only if self is the expander Monsoon has actually CLAIMED for its type.
// Monsoon caches the FIRST module of each type in its chain (one pointer per
// type), so when several expanders of the same type are placed in a row, only one
// is functionally connected. The connect mark must reflect that — otherwise every
// duplicate lights up as if connected. Compares self against the matching
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
// Returns the PHYSICAL bar (0..kMaskRing-1) the sequencer actually reads after LOR, to
// match SequencerEngine::getStrandIdx exactly:
//     timelineIdx = (globalStep + rotation) mod length
//     physicalBar = (timelineIdx + offset) mod ring
// so the highlight lands on the real active block, not a window-relative index.
// Returns -1 if globalStep < 0 (sequencer not running).
// (bugs 17+18, LOR_EDITING_BUG_LIST) the law lives in the core (lanePlayheadBar —
// ONE law, every lane INCLUDING the VAR lane; the old VAR special-case drew the
// engine's global scan and left the lane's window); this delegates.
inline int calcPlayhead(int globalStep, int length, int offset, int rotation) {
    if (globalStep < 0) return -1;
    return redDot::SandsRangeGestureCore::lanePlayheadBar(
        globalStep, length, offset, rotation, dotModular::kMaskRing);
}

// CC_PROMPT_VARIATION_SCAN_DISPLAY — the VARIATION lane's chevron position: the
// ENGINE's scanned read index, never a parallel recompute. Gate-mode holds scan the
// variation window (scanOfsV_[0], +1 per held rise — the freeze fix's escape search,
// IN THE TRAVEL DIRECTION: the signed delta, backward when Variation < 0), so the
// chevron creeps the way the playhead is going during a hold and re-aligns on
// advance — the visual confirmation of the freeze fix. Outside holds (and in
// clock/phase — scanOfs_ stays 0 there) this equals the old calcPlayhead position:
// calcPlayhead matches getStrandIdx exactly, and the mono feeds' LOR comes from the
// engine's own lor() store, so the swap is bit-identical at rest. -1 when not running.
//
// RESTORED (fix/variation-scan-display): bug 18 retired these on the premise that the
// scan "could leave the lane's window". That premise is FALSE on the engine side —
// stepAdvanceStrandIdx delegates to getStrandIdx, whose result is
// (((tick+delta) mod len + len) mod len + off) mod ring, i.e. ALWAYS in [off, off+len)
// for ANY delta. The scan jumps to a DIFFERENT IN-WINDOW cell (ahead of the shared
// playhead), then rejoins on advance — that IS the feature, and it never leaves the
// lane's own window. Pinned headless: test_variation_scan_display (in-window across
// scan deltas + bit-identical to calcPlayhead at delta 0). If a symptom persists in
// Rack it is a DISPLAY-row-len vs ENGINE-var-strand-len mismatch, not the scan —
// a separate, isolatable cause.
inline int variationLanePlayhead(const SequencerEngine& eng) {
    if (eng.stepIndex < 0) return -1;
    const dotModular::VoiceSlot mono = dotModular::kMonoVoiceSlot;
    return eng.stepAdvanceStrandIdx(mono, eng.stepAdvanceScanDelta(mono));
}

// PER-VOICE STEP-ADVANCE — THIS voice's VAR chevron (every voice decides — the WANDER
// fix — so every voice has its own scan): the voice's OWN scanned position (its own
// window + its own scan — the same index its advance decision used; it creeps the way
// the voice is going during a hold and re-aligns when the voice moves). The mono
// helper above is the mono slot's case. Outside holds this equals the plain
// calcPlayhead position. -1 when not running.
inline int variationLanePlayheadForVoice(const SequencerEngine& eng, dotModular::VoiceSlot bank) {
    if (eng.stepIndex < 0) return -1;
    return eng.stepAdvanceStrandIdx(bank, eng.stepAdvanceScanDelta(bank));
}

// Read an integer param (rounded) from a module, clamped to [1, kLaneLengthMax]
// (G2: the lane-length bound by name — was the literal 16, the same D1-class miss).
// Returns fallback if the module pointer is null.
inline int readLenParam(rack::Module* mod, int paramId, int fallback = 16) {
    if (!mod) return fallback;
    return std::max(1, std::min(dotModular::kLaneLengthMax, (int)std::round(mod->params[paramId].getValue())));
}
inline int readOffRotParam(rack::Module* mod, int paramId, int fallback = 0) {
    if (!mod) return fallback;
    return (int)std::round(mod->params[paramId].getValue());
}

}  // namespace redDot
