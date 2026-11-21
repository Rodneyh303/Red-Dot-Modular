/**
 * star_seam.hpp — the D2 test seam, shared (RHYTHM_DECISION_MANAGER_PLAN §3.1).
 *
 * F-1 re-pointed the engine at ParentSource::GRAPH — graph truth everywhere, no engine
 * legacy mode (D2). The suites that pin the LEGACY SOUND (the all-follow-V1 star) author
 * that graph INTO pe.containmentParents — the same words the Cantonment preset carries —
 * so the battery gates the GRAPH read itself, not a legacy source toggle. reset() wires
 * the RDM to the live array, so authoring AFTER reset() is visible at every edge.
 *
 * The plan's compat-pin rule (§5): the V1-semantics suites run with the star injected
 * explicitly — the legacy sound is one authored graph among many, and these suites keep
 * pinning exactly that graph. Mono-only suites never read the graph (the mono cascade is
 * untouched until F-3), so they need no injection.
 */
#pragma once
#include "engines/SequencerEngine.hpp"

inline void injectLegacyStarGraph(SequencerEngine& eng) {
    for (int v = 0; v < 16; ++v)
        eng.pe.containmentParents[v] = RhythmDecisionCore::LEGACY_STAR_PARENTS[v];
}
