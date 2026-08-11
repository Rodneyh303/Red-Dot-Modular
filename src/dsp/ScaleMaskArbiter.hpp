#pragma once
// ── dotModular::resolveScaleMask — pure scale-mask priority arbitration (no Rack) ─────────────────
// The single source of truth for WHICH 12-bit scale mask the Monsoon engine reads, given the three
// possible authorities (MONSOON_SCALE_AUTHORING_DIRECTION). Split out as a header-only, stdlib-only
// unit so it's unit-testable standalone (test/test_ScaleMaskArbiter.cpp) and ScaleManager shares the
// EXACT same arbitration — no divergence between what's tested and what ships.
//
// Priority (highest first):
//   1. OVERRIDE   — pushed by (regular) Shophouse when a slot/front is active (boundary-quantised).
//   2. AUTHORED   — the Monsoon's own hand-authored enable-band mask (its BASE scale).
//   3. FACTORY    — the factory (scale,root) menu selection.
//   4. all-12     — 0xFFF (no restriction) when nothing above applies.
//
// This is the same base/override/revert relationship already used for Colonnades ↔ Shophouse Micro:
// Monsoon takes Colonnades' base-author role; Shophouse is the override layer. One arbiter, no
// two-owners problem. All masks are 12-bit (bits 0..11 = semitones C..B); the result is &0xFFF.

#include <cstdint>

namespace dotModular {

inline uint16_t resolveScaleMask(bool overrideValid, uint16_t overrideMask,
                                 bool authoredValid, uint16_t authoredMask,
                                 bool factoryValid,  uint16_t factoryMask) {
    if (overrideValid) return (uint16_t)(overrideMask & 0x0FFF);
    if (authoredValid) return (uint16_t)(authoredMask & 0x0FFF);
    if (factoryValid)  return (uint16_t)(factoryMask  & 0x0FFF);
    return 0x0FFF;
}

// ── TONIC_TRANSPOSE_BUILD_BRIEF: root-relative transposition of a 12-bit scale mask ───────────────
// Rotate a 12-bit pitch-class mask UP by `semis` (bit p → p+semis, mod 12). This is exactly how a
// built-in scale transposes: calculateMask uses (root+interval)%12, i.e. rotate the degree-0 pattern
// up by root. A user scale stored root-relative (tonic at degree 0) transposes the same way — apply
// the LIVE root control via rotateMask12(relMask, root). Semis is taken mod 12 (any int, +/-).
inline uint16_t rotateMask12(uint16_t mask, int semis) {
    semis = ((semis % 12) + 12) % 12;
    mask &= 0x0FFF;
    return (uint16_t)(((mask << semis) | (mask >> (12 - semis))) & 0x0FFF);
}

// Normalise an ABSOLUTE mask to ROOT-RELATIVE for saving: rotate DOWN so the tonic sits at degree 0
// (bit at `tonic` → bit 0). Inverse of applying a root. tonic must be 0..11.
inline uint16_t normaliseToTonic(uint16_t mask, int tonic) {
    return rotateMask12(mask, -tonic);
}

} // namespace dotModular
