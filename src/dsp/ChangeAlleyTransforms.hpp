#pragma once
// ChangeAlleyTransforms — the four restructure verbs (CHANGE_ALLEY_DESIGN.md §10).
// Pure functions over a 16-entry src[] table. Header-only, no Rack deps, so the
// standalone replica test compiles it directly.
//
// Block semantics: blocks TILE THE ACTIVE POOL [0, activeCount) (0 = mono, always
// active). Automatic transforms never source a dead voice. blockSize is clamped to
// the active count; a partial trailing block operates on its live members only.
// Rows >= activeCount are LEFT UNTOUCHED (manual pins persist beyond the live pool).
// All transforms preserve one-pin-per-row (they assign, never duplicate rows).
#include <cstdint>

namespace dotModular { namespace ca {

inline int clampBlock(int blockSize, int activeCount) {
    if (activeCount < 1) activeCount = 1;
    if (blockSize < 1) blockSize = 1;
    return blockSize > activeCount ? activeCount : blockSize;
}

// Identity: src[v] = v over the active pool. (= Collapse @ block 1.)
inline void identity(uint8_t* src, int activeCount) {
    for (int v = 0; v < activeCount && v < 16; ++v) src[v] = (uint8_t)v;
}

// Collapse: src[v] = blockLeader(v). Block 1 = Identity; block=active = total unison.
inline void collapse(uint8_t* src, int activeCount, int blockSize) {
    const int b = clampBlock(blockSize, activeCount);
    for (int v = 0; v < activeCount && v < 16; ++v) src[v] = (uint8_t)((v / b) * b);
}

// Rotate: src[v] advances +1 WITHIN its block (wraps at block edge). Applied to the
// CURRENT table (composes per trigger — that is the point: repeated triggers march).
inline void rotate(uint8_t* src, int activeCount, int blockSize) {
    const int b = clampBlock(blockSize, activeCount);
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int cur   = src[v];
        const int base  = (cur / b) * b;
        int blockEnd    = base + b;                       // tile edge…
        if (blockEnd > activeCount) blockEnd = activeCount; // …partial trailing block
        const int span  = blockEnd - base;
        src[v] = (uint8_t)(base + ((cur - base + 1) % (span > 0 ? span : 1)));
    }
}

// Scatter: seeded shuffle of sources WITHIN each block. Deterministic per seed
// (reproducible/undoable). Each row gets a random source drawn from ITS OWN block
// of the active pool (fan-in allowed — this is a re-source, not a permutation).
inline void scatter(uint8_t* src, int activeCount, int blockSize, uint32_t seed) {
    const int b = clampBlock(blockSize, activeCount);
    uint32_t s = seed ? seed : 0x9E3779B9u;
    auto nextU = [&s]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };  // xorshift32
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int base = (v / b) * b;
        int blockEnd   = base + b;
        if (blockEnd > activeCount) blockEnd = activeCount;
        const int span = blockEnd - base;
        src[v] = (uint8_t)(base + (int)(nextU() % (uint32_t)(span > 0 ? span : 1)));
    }
}

// Reflect: src reversed WITHIN each block (retrograde at the block grain). Applied to
// the current table via position mirror: row v takes the source currently held by its
// mirror row. Self-inverse per double trigger. Odd partial block centre self-maps.
inline void reflect(uint8_t* src, int activeCount, int blockSize) {
    const int b = clampBlock(blockSize, activeCount);
    uint8_t tmp[16];
    for (int v = 0; v < 16; ++v) tmp[v] = src[v];
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int base = (v / b) * b;
        int blockEnd   = base + b;
        if (blockEnd > activeCount) blockEnd = activeCount;
        const int mirror = base + (blockEnd - 1 - v) + base - base;  // base + (end-1 - (v-base))
        src[v] = tmp[mirror];
    }
}

// ── §12: cross-block source offset ───────────────────────────────────────────
// Block i sources from block i+k instead of itself: "section A follows section B".
// Applied AFTER a transform, it re-points whole blocks without disturbing the
// within-block structure the transform just created. Well-defined by construction
// (one source per row in, one out), and wraps over the ACTIVE pool.
inline void blockOffset(uint8_t* src, int activeCount, int blockSize, int k) {
    const int b = clampBlock(blockSize, activeCount);
    if (k == 0 || b <= 0) return;
    const int nBlocks = (activeCount + b - 1) / b;          // includes a partial trailing block
    if (nBlocks <= 1) return;
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int cur      = src[v];
        const int curBlock = cur / b;
        const int within   = cur - curBlock * b;
        int tgtBlock = (curBlock + k) % nBlocks;
        if (tgtBlock < 0) tgtBlock += nBlocks;
        int cand = tgtBlock * b + within;
        // The trailing block may be short; fold back into it rather than off the pool.
        if (cand >= activeCount) cand = tgtBlock * b + (within % (activeCount - tgtBlock * b));
        src[v] = (uint8_t)cand;
    }
}

// ── §12: domain (row) variants ───────────────────────────────────────────────
// rotate()/scatter() above act on the CODOMAIN (which source a voice points at).
// These act on the DOMAIN (which VOICE holds a given sourcing role) — the axis our
// original four mixed by accident (rotate was codomain, reflect was domain).
// reflect() is already the domain-mirror, so its codomain twin is reflectValues().

// Rotate ROWS within each block: voices cycle their sourcing roles.
inline void rotateRows(uint8_t* src, int activeCount, int blockSize) {
    const int b = clampBlock(blockSize, activeCount);
    uint8_t tmp[16];
    for (int v = 0; v < 16; ++v) tmp[v] = src[v];
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int base = (v / b) * b;
        int end = base + b; if (end > activeCount) end = activeCount;
        const int span = end - base; if (span <= 0) continue;
        const int from = base + (((v - base) - 1 + span) % span);   // take the previous row's source
        src[v] = tmp[from];
    }
}

// Reflect VALUES within each block: sources mirror, rows stay put.
inline void reflectValues(uint8_t* src, int activeCount, int blockSize) {
    const int b = clampBlock(blockSize, activeCount);
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int cur  = src[v];
        const int base = (cur / b) * b;
        int end = base + b; if (end > activeCount) end = activeCount;
        const int span = end - base; if (span <= 0) continue;
        src[v] = (uint8_t)(base + (span - 1 - (cur - base)));
    }
}

// ── §12: transpose — invert the relation ─────────────────────────────────────
// "Whoever I was copying now copies me." For an injective board this is exactly f⁻¹.
// Collapse produces FAN-IN (many rows -> one source), which has no inverse: rows with
// no pre-image keep their current source, and where several rows share a source the
// LOWEST wins (deterministic, and it preserves that source's own mapping).
inline void transpose(uint8_t* src, int activeCount) {
    uint8_t inv[16];
    for (int v = 0; v < 16; ++v) inv[v] = src[v];          // default: unchanged
    bool seen[16] = {};
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int t = src[v];
        if (t < 0 || t >= activeCount || t >= 16) continue;
        if (!seen[t]) { inv[t] = (uint8_t)v; seen[t] = true; }   // lowest pre-image wins
    }
    for (int v = 0; v < activeCount && v < 16; ++v) src[v] = inv[v];
}

// Dispatch. transform: 0=Collapse 1=Rotate 2=Scatter 3=Reflect (ChangeAlleyIds order).
inline void apply(int transform, uint8_t* src, int activeCount, int blockSize, uint32_t seed) {
    switch (transform) {
        case 0: collapse(src, activeCount, blockSize); break;
        case 1: rotate  (src, activeCount, blockSize); break;
        case 2: scatter (src, activeCount, blockSize, seed); break;
        case 3: reflect (src, activeCount, blockSize); break;
        default: break;
    }
}

}} // namespace dotModular::ca
