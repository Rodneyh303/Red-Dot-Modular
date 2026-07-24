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

// ── Collapse: three formulations, two of which we expose (§13a/13c) ──────────
// Formally, with f: row -> col:
//   absolute  src[v] = leader(v)          -- neither axis: row-determined, DISCARDS f
//   domain    src[v] = src[leader(v)]     -- f o leader : adopt the leader's source
//   codomain  src[v] = leader(src[v])     -- leader o f : quantise your source to a leader
// absolute is just domain applied to an IDENTITY board (f=id => f(leader(v)) == leader(v)),
// which is why the original collapse looked "absolute" -- it was only ever tested from
// identity. It is reachable as Identity -> collapseDomain across two phrase boundaries, so
// it gets no button of its own (§11: compose across boundaries).
//
// LEADER OFFSET picks WHICH member of the block is the leader (0 = first). Wraps within the
// block, so it is meaningful at every grain and, at the inter level, selects which BLOCK.
inline int blockLeader(int v, int b, int activeCount, int leaderOffset) {
    const int base = (v / b) * b;
    int end = base + b; if (end > activeCount) end = activeCount;
    const int span = end - base; if (span <= 0) return base;
    int off = leaderOffset % span; if (off < 0) off += span;
    return base + off;
}

// DOMAIN collapse: every row adopts the leader's source. Composes -- a preceding scatter is
// INHERITED (each block ends on a different source) rather than wiped.
inline void collapseDomain(uint8_t* src, int activeCount, int blockSize, int leaderOffset = 0) {
    const int b = clampBlock(blockSize, activeCount);
    uint8_t tmp[16];
    for (int v = 0; v < 16; ++v) tmp[v] = src[v];
    for (int v = 0; v < activeCount && v < 16; ++v)
        src[v] = tmp[blockLeader(v, b, activeCount, leaderOffset)];
}

// CODOMAIN collapse: quantise each row's SOURCE to its block's leader. Rows keep their
// individuality; what shrinks is the ALPHABET of sources (16 possible -> nBlocks).
inline void collapseCodomain(uint8_t* src, int activeCount, int blockSize, int leaderOffset = 0) {
    const int b = clampBlock(blockSize, activeCount);
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int cur = src[v];
        if (cur < 0 || cur >= activeCount || cur >= 16) continue;
        src[v] = (uint8_t)blockLeader(cur, b, activeCount, leaderOffset);
    }
}

// Legacy name: identical to collapseDomain from an identity board (see note above).
inline void collapse(uint8_t* src, int activeCount, int blockSize) {
    const int b = clampBlock(blockSize, activeCount);
    for (int v = 0; v < activeCount && v < 16; ++v) src[v] = (uint8_t)((v / b) * b);
}

// RotateValues: src[v] advances +1 WITHIN its block (CODOMAIN -- shifts the SOURCE) (wraps at block edge). Applied to the
// CURRENT table (composes per trigger — that is the point: repeated triggers march).
// STEP is a stepped-knob parameter, not CV: it is configuration (set-and-forget, latches
// under lock) and each trigger still applies exactly ONE discrete change. It alters the
// STRUCTURE, not merely the speed -- in a block of 4, step 2 yields two 2-cycles (0<->2,
// 1<->3) rather than one 4-cycle, and step span-1 runs the rotation BACKWARDS. Steps that
// share a factor with the span produce sub-cycles; coprime steps traverse the whole block.
// Clamped into [1, span-1]: 0 and span are both no-ops, so the knob cannot select nothing.
inline int clampStep(int step, int span) {
    if (span <= 1) return 0;
    int st = step % span;
    if (st < 0) st += span;
    return (st == 0) ? 1 : st;
}

inline void rotateValues(uint8_t* src, int activeCount, int blockSize, int step = 1) {
    const int b = clampBlock(blockSize, activeCount);
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int cur   = src[v];
        const int base  = (cur / b) * b;
        int blockEnd    = base + b;                       // tile edge…
        if (blockEnd > activeCount) blockEnd = activeCount; // …partial trailing block
        const int span  = blockEnd - base;
        if (span <= 1) continue;
        src[v] = (uint8_t)(base + ((cur - base + clampStep(step, span)) % span));
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

// ReflectRows: rows reversed WITHIN each block (DOMAIN -- swaps which VOICE holds a role) (retrograde at the block grain). Applied to
// the current table via position mirror: row v takes the source currently held by its
// mirror row. Self-inverse per double trigger. Odd partial block centre self-maps.
inline void reflectRows(uint8_t* src, int activeCount, int blockSize) {
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
    if (b <= 0) return;
    if (k == 0) return;   // explicit no-op before clamping
    const int nBlocks = (activeCount + b - 1) / b;          // includes a partial trailing block
    if (nBlocks <= 1) return;
    k = clampStep(k, nBlocks);                             // 1..nBlocks-1; 0/nBlocks are no-ops
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
// rotateValues()/scatter() above act on the CODOMAIN (which source a voice points at).
// These act on the DOMAIN (which VOICE holds a given sourcing role) — the axis our
// original four mixed by accident (rotate was codomain, reflect was domain).
// reflectRows() is already the domain-mirror, so its codomain twin is reflectValues().

// Rotate ROWS within each block: voices cycle their sourcing roles.
inline void rotateRows(uint8_t* src, int activeCount, int blockSize, int step = 1) {
    const int b = clampBlock(blockSize, activeCount);
    uint8_t tmp[16];
    for (int v = 0; v < 16; ++v) tmp[v] = src[v];
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int base = (v / b) * b;
        int end = base + b; if (end > activeCount) end = activeCount;
        const int span = end - base; if (span <= 0) continue;
        const int st   = clampStep(step, span);
        const int from = base + (((v - base) - st + span * 16) % span);   // take that row's source
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

// ── §13: INTER (across-block) variants ──────────────────────────────────────────────────
// Each intra verb has an inter twin that operates on BLOCKS as units rather than voices.
// The block grain at the inter level sets HOW MANY blocks form a super-block.

// interCollapseD: every block adopts the leader-block's assignments (domain version).
// leader = which block leads (offset within the super-block, wraps).
inline void interCollapseDomain(uint8_t* src, int activeCount, int blockSize, int leader = 0) {
    // Reuse collapseDomain with a super-block size = blockSize * blockSize (or activeCount).
    // The leader parameter selects which block within the super-block leads.
    const int b      = clampBlock(blockSize, activeCount);
    const int nBlk   = (activeCount + b - 1) / b;
    if (nBlk <= 1) { collapseDomain(src, activeCount, activeCount, 0); return; }
    // Re-express as collapseDomain at the block level: treat block indices as voices.
    uint8_t bsrc[16] = {};
    for (int v = 0; v < activeCount && v < 16; ++v) bsrc[v] = src[v] / b;  // block src
    const int leaderBlk = leader % nBlk;
    const uint8_t leaderVal = bsrc[leaderBlk * b];   // which block the leader points at
    for (int v = 0; v < activeCount && v < 16; ++v)
        src[v] = (uint8_t)(leaderVal * b + (v % b));
}

// interCollapseC: every block's sources quantise to their super-block leader's block (codom).
inline void interCollapseCodomain(uint8_t* src, int activeCount, int blockSize, int leader = 0) {
    const int b    = clampBlock(blockSize, activeCount);
    const int nBlk = (activeCount + b - 1) / b;
    if (nBlk <= 1) return;
    const int leaderBlk = leader % nBlk;
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int srcBlk = src[v] / b;
        const int within = src[v] % b;
        // quantise the block index to the leader block; preserve position within block
        (void)srcBlk;   // the snap IS to the leader block by definition
        src[v] = (uint8_t)(leaderBlk * b + within);
    }
}

// interRotate: = blockOffset at this level. Block i's sources move to block i+step.
inline void interRotate(uint8_t* src, int activeCount, int blockSize, int step = 1) {
    blockOffset(src, activeCount, blockSize, step);
}

// interRotateRows: voices move to a different block, preserving within-block position.
inline void interRotateRows(uint8_t* src, int activeCount, int blockSize, int step = 1) {
    const int b    = clampBlock(blockSize, activeCount);
    const int nBlk = (activeCount + b - 1) / b;
    if (nBlk <= 1) return;
    const int st   = clampStep(step, nBlk);
    uint8_t tmp[16]; for (int v = 0; v < 16; ++v) tmp[v] = src[v];
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int blk    = v / b, within = v % b;
        const int srcBlk = (blk - st + nBlk * 16) % nBlk;
        src[v] = tmp[srcBlk * b + within];
    }
}

// interReflect: block ORDER reversed (codomain -- sources point to mirrored block).
inline void interReflectCodomain(uint8_t* src, int activeCount, int blockSize) {
    const int b    = clampBlock(blockSize, activeCount);
    const int nBlk = (activeCount + b - 1) / b;
    if (nBlk <= 1) return;
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int srcBlk = src[v] / b, within = src[v] % b;
        const int mirBlk = nBlk - 1 - srcBlk;
        int cand = mirBlk * b + within;
        if (cand >= activeCount) cand = mirBlk * b + (within % (activeCount - mirBlk * b));
        src[v] = (uint8_t)cand;
    }
}

// interReflectRows: block role assignment reversed (domain -- rows swap blocks).
inline void interReflectRows(uint8_t* src, int activeCount, int blockSize) {
    const int b    = clampBlock(blockSize, activeCount);
    const int nBlk = (activeCount + b - 1) / b;
    if (nBlk <= 1) return;
    uint8_t tmp[16]; for (int v = 0; v < 16; ++v) tmp[v] = src[v];
    for (int v = 0; v < activeCount && v < 16; ++v) {
        const int blk    = v / b, within = v % b;
        const int mirBlk = nBlk - 1 - blk;
        const int from   = mirBlk * b + within;
        if (from < activeCount && from < 16) src[v] = tmp[from];
    }
}

// interScatter: seeded re-assignment of whole blocks (codomain -- new block sources).
// Uses a Philox counter for reversibility (§12e / §13e).
inline void interScatter(uint8_t* src, int activeCount, int blockSize, uint64_t counter,
                         uint64_t key = 0xCA7E4A3EC0FFEE77ULL) {
    const int b    = clampBlock(blockSize, activeCount);
    const int nBlk = (activeCount + b - 1) / b;
    if (nBlk <= 1) return;
    // Inline Philox-style scramble (no Rack dependency -- this file is header-only/no-SDK).
    // Each block draws a source block from a keyed mix of (counter, block index).
    for (int blk = 0; blk < nBlk; ++blk) {
        uint64_t h = counter ^ ((uint64_t)blk * 0x9E3779B97F4A7C15ULL) ^ key;
        h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 27; h *= 0x94D049BB133111EBULL;
        h ^= h >> 31;
        const int srcBlk = (int)(h % (uint64_t)nBlk);
        for (int w = 0; w < b; ++w) {
            const int v = blk * b + w;
            if (v < activeCount && v < 16) {
                int cand = srcBlk * b + w;
                if (cand >= activeCount) cand = srcBlk * b + (w % (activeCount - srcBlk*b));
                src[v] = (uint8_t)cand;
            }
        }
    }
}

// Unified Temasek apply -- called by MonsoonExpanderManager at phrase boundary.
// verb: V_COLLAPSE=0, V_ROTATE=1, V_REFLECT=2, V_SCATTER=3
// isDomain: true=domain button, false=codomain button
// isInter:  false=intra (left panel), true=inter (right panel)
inline void applyTemasek(int verb, bool isDomain, bool isInter,
                         uint8_t* src, int activeCount, int grain,
                         int leaderOrStep, uint64_t scatterCounter) {
    if (!isInter) {
        // INTRA: use the existing within-block functions
        switch (verb) {
            case 0: isDomain ? collapseDomain  (src, activeCount, grain, leaderOrStep)
                             : collapseCodomain(src, activeCount, grain, leaderOrStep); break;
            case 1: isDomain ? rotateRows  (src, activeCount, grain, leaderOrStep)
                             : rotateValues(src, activeCount, grain, leaderOrStep); break;
            case 2: isDomain ? reflectRows  (src, activeCount, grain)
                             : reflectValues(src, activeCount, grain); break;
            case 3: {
                // Scatter: Philox counter-based (§12e -- currently still LCG, TODO swap)
                uint32_t seed = (uint32_t)(scatterCounter & 0xFFFFFFFF);
                scatter(src, activeCount, grain, seed); break;
            }
        }
    } else {
        // INTER: across-block versions
        switch (verb) {
            case 0: isDomain ? interCollapseDomain  (src, activeCount, grain, leaderOrStep)
                             : interCollapseCodomain(src, activeCount, grain, leaderOrStep); break;
            case 1: isDomain ? interRotateRows(src, activeCount, grain, leaderOrStep)
                             : interRotate    (src, activeCount, grain, leaderOrStep); break;
            case 2: isDomain ? interReflectRows   (src, activeCount, grain)
                             : interReflectCodomain(src, activeCount, grain); break;
            case 3: interScatter(src, activeCount, grain, scatterCounter); break;
        }
    }
}

// Dispatch. transform: 0=Collapse 1=Rotate 2=Scatter 3=Reflect (ChangeAlleyIds order).
inline void apply(int transform, uint8_t* src, int activeCount, int blockSize, uint32_t seed,
                  int step = 1) {
    switch (transform) {
        case 0: collapse(src, activeCount, blockSize); break;
        case 1: rotateValues (src, activeCount, blockSize, step); break;
        case 2: scatter (src, activeCount, blockSize, seed); break;
        case 3: reflectRows  (src, activeCount, blockSize); break;
        default: break;
    }
}

}} // namespace dotModular::ca
