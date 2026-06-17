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

// ── Is `self` the expander the Monsoon actually CLAIMED for its type? ─────────
// findMonsoonEitherSide returns non-null for EVERY duplicate expander in a chain
// (they can all see the Monsoon), so it can't drive a per-module "connected"
// indicator — duplicates would all light up. The MonsoonExpanderManager already
// claims the first-of-each-type into a cached* pointer; an expander is "really
// connected" iff it IS that cached pointer. Returns false for duplicates.
inline bool isClaimedExpander(rack::Module* self, Monsoon* mon) {
    if (!self || !mon) return false;
    auto& em = mon->expanderManager;
    rack::Model* mdl = self->model;
    if (mdl == modelMonsoonInterchangeExpander)  return (void*)self == (void*)em.cachedScaleExpander;
    if (mdl == modelMonsoonCausewayExpander)     return (void*)self == (void*)em.cachedCausewayExpander;
    if (mdl == modelMonsoonSurgeExpander)        return (void*)self == (void*)em.cachedSurgeExpander;
    if (mdl == modelMonsoonSandsVisualExpander)  return (void*)self == (void*)em.cachedSandsVisualExpander;
    if (mdl == modelMonsoonStraitsEastExpander)  return (void*)self == (void*)em.cachedPolyVoiceExpander;
    if (mdl == modelMonsoonStraitWestExpander)   return (void*)self == (void*)em.cachedStraitWestExpander;
    if (mdl == modelStraitsEastSandsVisual)      return (void*)self == (void*)em.cachedEastSandsVisual;
    if (mdl == modelStraitsSandsMacroVisual)     return (void*)self == (void*)em.cachedMacroSandsVisual;
    return false;   // unknown type → not claimed
}

// Convenience: find Monsoon AND confirm self is the claimed expander of its type.
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
