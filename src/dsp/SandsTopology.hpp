#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// SandsTopology — the single authority for Sands ownership / lock / editable /
// write-guard decisions. See docs/design/SANDS_TOPOLOGY_RESOLVER_PLAN.md.
//
// STATUS: STEP 2 SKELETON. Built + (debug) cross-checked against the existing
// predicates, but NOT YET CONSUMED anywhere. Behaviour-inert. Migration of the
// scattered predicates onto this happens in later steps (3+), one site at a time.
//
// DESIGN DECISIONS (settled — see plan §5):
//  1. API speaks EDITOR LANE only; all engine/PE/mono-param conversions are baked
//     inside. Callers never convert.
//  2. Per-consumer construction; must stay lightweight (no allocation). The single
//     -source guarantee is in this one build path, not one shared instance.
//  3. Config enumerates ALL reachable combinations (collapse only in derivations).
//  4. writesEngine + the strand-write ledger extend to poly arrays + spread (5b).
//  5. Patch-load: rebuilt each control block, so it reflects loaded params; a debug
//     check should confirm config/owner on the first block after load.
//
// Ownership param convention (from ui/OwnerCell.hpp):
//   value  > 0.5  ==  LOCAL owns  (Mono on V1 / East on its voice)  → OUTLINE cell
//   value <= 0.5  ==  MACRO owns  (delegated to the shared base)    → FILLED  cell
//
// Voice numbering (VoiceResolver): voice 0 == V1 == the mono slot. Poly voices are
// 1..N here (kept as the editor "voice index"; map to banks via VoiceResolver when
// touching engine poly arrays — done inside this resolver, not by callers).
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>

namespace dotModular {

struct SandsTopology {
    // Who can be the producer/owner/editor of a lane's base value.
    // SANDS CONSOLIDATION Step 7: MONO removed (Mono visual module killed in Step 6).
    enum class Role : uint8_t { NONE = 0, EAST, MACRO };

    // The NAMED configuration. SANDS CONSOLIDATION Step 7: simplified to 2-module
    // (East+Macro) reality. Dead MONO configs removed (was 8, now 4).
    enum class Config : uint8_t {
        EMPTY = 0,        // no Sands visual present
        EAST,             // East only (+ poly base active)
        MACRO_SOLE,       // Macro only (+ base), NO East visual
        EAST_PLUS_MACRO,  // East + Macro (+ base)
    };

    // ── Inputs the builder needs (filled by the caller; keeps this header free of
    //    the heavy widget headers / include cycles). Pure data. ────────────────
    struct Inputs {
        // SANDS CONSOLIDATION Step 7: monoPresent + monoV1Owner removed (Mono killed Step 6).
        bool eastPresent  = false;   // cachedEastSandsVisual    != nullptr
        bool macroPresent = false;   // cachedMacroSandsVisual   != nullptr
        bool polyBaseActive = false; // cachedPolyVoiceExpander != null && numPolyVoices >= 1
        int  polyVoiceCount = 0;     // engine.numPolyVoices

        // Ownership params. EDITOR-lane indexed, 7 lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG).
        //   eastV1Owner[l]      : East's   ownerDispId(l)  > 0.5  (true = East local-owns)
        //   eastPolyOwner[v][l] : East's   ownerId(v,l)    > 0.5  (true = East local-owns; v = poly index 0..14)
        bool eastV1Owner[7]      = { true, true, true, true, true, true, true };
        bool eastPolyOwner[15][7] = {};   // default false → Macro-owned until set; caller fills when East present
    };

    Config config = Config::EMPTY;
    Inputs in;     // kept for the derivations + debug

    // ── Construction (lightweight, allocation-free) ──────────────────────────
    static SandsTopology build(const Inputs& i) {
        SandsTopology t;
        t.in = i;
        t.config = classify(i);
        return t;
    }

    static Config classify(const Inputs& i) {
        // SANDS CONSOLIDATION Step 7: simplified to 2-module (East+Macro) — was 8 cases, now 4.
        const bool e = i.eastPresent, x = i.macroPresent;
        if (!e && !x) return Config::EMPTY;
        if ( e && !x) return Config::EAST;
        if (!e &&  x) return Config::MACRO_SOLE;
        return Config::EAST_PLUS_MACRO;   // e && x
    }

    // ── OWNS(voice, editorLane) — the single source of ownership ──────────────
    // voice 0 = V1/mono slot. Editor order: 0 MEL, 1 OCT, 2 QMIX, 3 REST, 4 ACC, 5 VAR, 6 LEG.
    // SANDS CONSOLIDATION Step 7: Mono branch removed — V1 owned by East or Macro.
    static constexpr int kPolyLanes = 7;   // MEL/OCT/QMIX/REST/ACC/VAR/LEG
    Role owner(int voice, int editorLane) const {
        if (editorLane < 0 || editorLane > 6) return Role::NONE;

        // V1 / mono slot.
        if (voice == 0) {
            if (in.eastPresent) {
                // East is the V1 editor. Per-lane: East or delegated-to-Macro.
                if (in.macroPresent && !in.eastV1Owner[editorLane]) return Role::MACRO;
                return Role::EAST;
            }
            if (in.macroPresent) return Role::MACRO;   // MACRO_SOLE owns V1
            return Role::NONE;
        }

        // Poly voices (voice >= 1). (VAR/LEG are poly now; the old `>= kPolyLanes → NONE` is dead.)
        if (in.eastPresent) {
            const int pv = voice - 1;   // poly index 0..14
            if (pv < 0 || pv >= 15) return Role::NONE;
            if (in.macroPresent && !in.eastPolyOwner[pv][editorLane]) return Role::MACRO;
            return Role::EAST;
        }
        if (in.macroPresent) return Role::MACRO;   // Macro drives all poly voices when sole poly editor
        return Role::NONE;
    }

    // ── Derived answers (PURE functions of config + owner) ────────────────────
    // EDITABLE(panel, voice, lane): may THIS panel's editor edit this cell?
    // A panel edits a cell iff it is the owner of that cell.
    bool editableOn(Role panel, int voice, int editorLane) const {
        return owner(voice, editorLane) == panel;
    }

    // LOCKED == the display-lock matrix == inverse of editable, PLUS the "nothing to
    // delegate to" case is still editable (handled by owner() returning the local
    // role when Macro absent). So locked iff this panel is NOT the owner but COULD
    // host this cell (i.e. the cell exists for that panel/voice).
    bool lockedOn(Role panel, int voice, int editorLane) const {
        Role o = owner(voice, editorLane);
        if (o == Role::NONE) return false;          // no cell here → not "locked", just absent
        return o != panel;                          // someone else owns it → locked on this panel
    }

    // WRITES_ENGINE(producer, voice, lane): may THIS producer write the engine
    // strand/poly value for this cell? Exactly one producer per cell — enforced at
    // runtime by SequencerEngine's strand-write ledger.
    bool writesEngine(Role producer, int voice, int editorLane) const {
        return owner(voice, editorLane) == producer;
    }

    // Convenience: the single producer for a cell (for ledger role tagging).
    Role engineWriter(int voice, int editorLane) const { return owner(voice, editorLane); }
};

} // namespace dotModular
