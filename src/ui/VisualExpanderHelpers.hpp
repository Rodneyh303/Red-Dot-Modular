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

// ── Per-lane playhead ─────────────────────────────────────────────────────────
// activeBar = ((offset + globalStep + rotation) % length + length) % length
// Returns -1 if globalStep < 0 (sequencer not running).
inline int calcPlayhead(int globalStep, int length, int offset, int rotation) {
    if (globalStep < 0) return -1;
    length = std::max(1, std::min(length, 16));
    return ((offset + globalStep + rotation) % length + length) % length;
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
