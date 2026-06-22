#pragma once
#include <rack.hpp>
#include "../Monsoon.hpp"
#include "SandsVisualEditorV4.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace redDot {

// ── Chain-walk: find Monsoon anywhere to the right ───────────────────────────
inline Monsoon* findMonsoon(rack::Module* startRight, int maxDepth = 8) {
    Module* curr = startRight;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (auto* m = dynamic_cast<Monsoon*>(curr)) return m;
        curr = curr->rightExpander.module;
    }
    return nullptr;
}

// ── Chain-walk: find Monsoon on EITHER side ──────────────────────────────────
// Walks right first, then left, hopping any intermediate modules (e.g. an
// Interchange placed between a Sands editor and Monsoon). Use this from visual
// expanders so they bind to the host regardless of which side they sit on and
// regardless of what sits between them and Monsoon.
inline Monsoon* findMonsoonEitherSide(rack::Module* self, int maxDepth = 8) {
    if (!self) return nullptr;
    Module* curr = self->rightExpander.module;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (auto* m = dynamic_cast<Monsoon*>(curr)) return m;
        curr = curr->rightExpander.module;
    }
    curr = self->leftExpander.module;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (auto* m = dynamic_cast<Monsoon*>(curr)) return m;
        curr = curr->leftExpander.module;
    }
    return nullptr;
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
        || s == (const void*)em.cachedCausewayExpander
        || s == (const void*)em.cachedSurgeExpander
        || s == (const void*)em.cachedSandsVisualExpander
        || s == (const void*)em.cachedPolyVoiceExpander
        || s == (const void*)em.cachedStraitWestExpander
        || s == (const void*)em.cachedEastSandsVisual
        || s == (const void*)em.cachedMacroSandsVisual;
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

// ── Poly per-voice playhead (3 lanes: REST/MELODY/OCTAVE) ────────────────────
// voiceIdx: 0-based index into POLY_DNA_VOICE_n params (voice 2 = index 0).
// Reads L/O/R for each of the 3 lanes from the module that owns those params.
// `paramMod` is the module holding POLY_DNA/POLY_MELODY/POLY_OCTAVE params.
inline void setPolyVoicePlayheads(SandsVisualEditorV4* editor,
                                   int globalStep,
                                   rack::Module* paramMod,
                                   int voiceIdx) {
    if (!editor) return;

    // REST lane: POLY_DNA_VOICE_n_LEN/OFF/ROT
    {
        int base = POLY_DNA_VOICE_1_LEN + voiceIdx * 3;
        int len  = readLenParam   (paramMod, base);
        int off  = readOffRotParam(paramMod, base + 1);
        int rot  = readOffRotParam(paramMod, base + 2);
        editor->setLanePlayStep(0, calcPlayhead(globalStep, len, off, rot));
    }
    // MELODY lane: POLY_MELODY_VOICE_n_LEN/OFF/ROT
    {
        int base = POLY_MELODY_VOICE_1_LEN + voiceIdx * 3;
        int len  = readLenParam   (paramMod, base);
        int off  = readOffRotParam(paramMod, base + 1);
        int rot  = readOffRotParam(paramMod, base + 2);
        editor->setLanePlayStep(1, calcPlayhead(globalStep, len, off, rot));
    }
    // OCTAVE lane: POLY_OCTAVE_VOICE_n_LEN/OFF/ROT
    {
        int base = POLY_OCTAVE_VOICE_1_LEN + voiceIdx * 3;
        int len  = readLenParam   (paramMod, base);
        int off  = readOffRotParam(paramMod, base + 1);
        int rot  = readOffRotParam(paramMod, base + 2);
        editor->setLanePlayStep(2, calcPlayhead(globalStep, len, off, rot));
    }
}

// ── Macro poly playhead (global L/O/R, same for all voices) ─────────────────
// Reads GLOBAL_REST/MELODY/OCTAVE_DNA_LEN/OFF/ROT from paramMod.
inline void setMacroPolyPlayheads(SandsVisualEditorV4* editor,
                                   int globalStep,
                                   rack::Module* paramMod) {
    if (!editor) return;

    struct { int lenId, offId, rotId; } lanes[3] = {
        { GLOBAL_REST_DNA_LEN,   GLOBAL_REST_DNA_OFF,   GLOBAL_REST_DNA_ROT   },
        { GLOBAL_MELODY_DNA_LEN, GLOBAL_MELODY_DNA_OFF, GLOBAL_MELODY_DNA_ROT },
        { GLOBAL_OCTAVE_DNA_LEN, GLOBAL_OCTAVE_DNA_OFF, GLOBAL_OCTAVE_DNA_ROT },
    };
    for (int l = 0; l < 3; ++l) {
        int len = readLenParam   (paramMod, lanes[l].lenId);
        int off = readOffRotParam(paramMod, lanes[l].offId);
        int rot = readOffRotParam(paramMod, lanes[l].rotId);
        editor->setLanePlayStep(l, calcPlayhead(globalStep, len, off, rot));
    }
}

}  // namespace redDot
