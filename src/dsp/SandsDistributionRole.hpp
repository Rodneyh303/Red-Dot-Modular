// SandsDistributionRole — the distribution sub-expander's DUAL-ROLE walk, as a
// rack-free pure template (the unit test drives fake nodes through the SAME walk
// the module runs — the house's testable-walk law, the walkSegmentForHost shape).
// Spec: docs/design/CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md §DUAL ROLE.
//
// Walks LEFT from the expander through the family chain: the first family host
// reached decides the role; a foreign boundary, a walked-out chain, or a bare
// left link resolves PER-VOICE (the safe default — the spec defines exactly two
// hosts and CROSS-VOICE must be CLAIMED by finding the Macro visual first).
// Never walks PAST a host looking for a farther one (the segment rule).
#pragma once
#include <cstdint>

namespace SandsDistributionRole {

enum class Host : uint8_t { NONE = 0, MACRO, EAST };
static constexpr int kMaxDepth = 12;   // the lookup authority's segment depth

template <class Node, class NextFn, class HostFn, class SuiteFn>
inline Host resolve(Node* left, NextFn next, HostFn hostOf, SuiteFn isSuite) {
    Node* curr = left;
    for (int d = 0; curr && d < kMaxDepth; ++d) {
        const Host h = hostOf(curr);
        if (h != Host::NONE) return h;           // first host wins — no walking past
        if (!isSuite(curr)) return Host::NONE;   // foreign module → boundary
        curr = next(curr);
    }
    return Host::NONE;                            // chain end / depth exhaustion
}

// The production predicates, parameterised on the two host models so the module
// header stays the only rack-dependent site (IsModel is a callable: model ptr in,
// bool out — the module passes lambdas comparing rack::Model*).
template <class Node, class NextFn, class IsMacroModel, class IsEastModel, class IsSuiteModel>
inline Host resolveProduction(Node* left, NextFn next,
                              IsMacroModel isMacroModel, IsEastModel isEastModel,
                              IsSuiteModel isSuiteModel) {
    return resolve(left, next,
        [&](const Node* m) -> Host {
            if (isMacroModel(m)) return Host::MACRO;
            if (isEastModel(m))  return Host::EAST;
            return Host::NONE;
        },
        [&](const Node* m) -> bool { return isSuiteModel(m); });
}

} // namespace SandsDistributionRole
