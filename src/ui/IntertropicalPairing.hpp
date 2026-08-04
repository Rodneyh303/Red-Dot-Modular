#pragma once
// ── Intertropical pairing — ONE shared mechanism for Lantern + Changi T3 ─────
// A tuple is 1 Intertropical (the hub) + 0/1 Lantern + 0/1 Changi T3 (consumers).
// The HUB owns the identity: each Intertropical self-assigns a small pair number
// (1..N, lowest unused), persisted. Consumers carry a "follow" setting:
//   followId == 0  -> AUTO: nearest Intertropical on either side (the legacy walk;
//                     zero behaviour change for the common single-tuple patch).
//   followId >  0  -> bind to the Intertropical whose pairId == followId, anywhere
//                     in the rack (disambiguates several tuples).
// Both consumers read at their own level over this one binding: Lantern reads the
// engine (note-type/colour), Changi T3 reads Intertropical's output jacks (faithful
// breakout). Build once, consume twice — per CHANGI_TERMINAL_SPLIT.md.

#include <rack.hpp>
#include "../Intertropical.hpp"
#include <set>

namespace redDot {

// Nearest Intertropical on either side (walk right then left, hop intermediates).
// This is the AUTO behaviour and the historical Lantern binding.
inline Intertropical* findIntertropicalEitherSide(rack::Module* self, int maxDepth = 12) {
    using rack::Module;
    if (!self) return nullptr;
    Module* curr = self->rightExpander.module;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (auto* m = dynamic_cast<Intertropical*>(curr)) return m;
        curr = curr->rightExpander.module;
    }
    curr = self->leftExpander.module;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (auto* m = dynamic_cast<Intertropical*>(curr)) return m;
        curr = curr->leftExpander.module;
    }
    return nullptr;
}

// Global lowest-unused pair id (1..N) across every Intertropical except `self`.
// Called by Intertropical on add (and lazily if it still reads 0). Stable numbers
// survive because each existing instance keeps its persisted id; a fresh module
// takes the smallest gap.
inline int assignPairId(rack::Module* self) {
    std::set<int> used;
    if (APP && APP->engine) {
        for (int64_t id : APP->engine->getModuleIds()) {
            rack::Module* m = APP->engine->getModule(id);
            if (!m || m == self) continue;
            if (auto* it = dynamic_cast<Intertropical*>(m))
                if (it->pairId > 0) used.insert(it->pairId);
        }
    }
    int n = 1;
    while (used.count(n)) ++n;
    return n;
}

// Resolve which Intertropical a consumer is bound to. followId 0 => nearest walk;
// else the global instance with the matching pairId (or nullptr if none present).
inline Intertropical* resolveFollowedIT(rack::Module* self, int followId) {
    if (followId <= 0) return findIntertropicalEitherSide(self);
    if (APP && APP->engine) {
        for (int64_t id : APP->engine->getModuleIds()) {
            rack::Module* m = APP->engine->getModule(id);
            if (!m) continue;
            if (auto* it = dynamic_cast<Intertropical*>(m))
                if (it->pairId == followId) return it;
        }
    }
    return nullptr;
}

// The set of pair ids currently present (for building "Follow" menus). Sorted.
inline std::vector<int> presentPairIds() {
    std::set<int> ids;
    if (APP && APP->engine) {
        for (int64_t id : APP->engine->getModuleIds()) {
            rack::Module* m = APP->engine->getModule(id);
            if (!m) continue;
            if (auto* it = dynamic_cast<Intertropical*>(m))
                if (it->pairId > 0) ids.insert(it->pairId);
        }
    }
    return std::vector<int>(ids.begin(), ids.end());
}

// Shared 8-hue pairing palette (matches Lantern/Intertropical voiceColour so the
// source and both consumers show the same colour for a given number). id is 1-based.
inline NVGcolor pairColour(int id) {
    static const NVGcolor P[8] = {
        nvgRGB(0x6c,0x8c,0xd4), nvgRGB(0x26,0xa6,0x9a), nvgRGB(0xd4,0x8a,0x3c),
        nvgRGB(0xb0,0x6c,0xd4), nvgRGB(0x5c,0xb8,0x5c), nvgRGB(0xd4,0x6c,0x8c),
        nvgRGB(0x4c,0xb0,0xc8), nvgRGB(0xc8,0xb0,0x4c),
    };
    int i = (id > 0) ? ((id - 1) % 8) : 0;
    return P[i];
}

}  // namespace redDot
