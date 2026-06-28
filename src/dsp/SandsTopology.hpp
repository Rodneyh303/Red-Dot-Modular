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
    enum class Role : uint8_t { NONE = 0, MONO, EAST, MACRO };

    // The NAMED configuration. Distinct names so a guard can never accidentally
    // match two topologies (the exact shape of the Mono+Macro clobber bug, where a
    // hand-built `macro && polyBase` matched both MACRO_SOLE and MONO_PLUS_MACRO).
    // ALL reachable combos are listed even where behaviour is currently identical.
    enum class Config : uint8_t {
        EMPTY = 0,        // no Sands visual present
        MONO,             // Mono only (no base needed for its own V1)
        EAST,             // East only (+ poly base active)
        MACRO_SOLE,       // Macro only (+ base), NO East visual, NO Mono  ← clobber guard lives here
        MONO_PLUS_EAST,   // Mono + East (no Macro)
        MONO_PLUS_MACRO,  // Mono + Macro (+ base)
        EAST_PLUS_MACRO,  // East + Macro (+ base)
        MONO_EAST_MACRO,  // all three (+ base)
    };

    // ── Inputs the builder needs (filled by the caller; keeps this header free of
    //    the heavy widget headers / include cycles). Pure data. ────────────────
    struct Inputs {
        bool monoPresent  = false;   // cachedSandsVisualExpander != nullptr
        bool eastPresent  = false;   // cachedEastSandsVisual    != nullptr
        bool macroPresent = false;   // cachedMacroSandsVisual   != nullptr
        bool polyBaseActive = false; // cachedPolyVoiceExpander != null && numPolyVoices >= 1
        int  polyVoiceCount = 0;     // engine.numPolyVoices

        // Ownership params, already read by the caller (it has the typed module ptrs;
        // we don't, to stay header-light). EDITOR-lane indexed, lanes 0..3 (MEL/OCT/
        // REST/ACC — the 4 poly/delegable lanes; VAR/LEG are mono-only, never ceded).
        //   monoV1Owner[l]      : Mono's   ownerDispId(l)  > 0.5  (true = Mono local-owns)
        //   eastV1Owner[l]      : East's   ownerDispId(l)  > 0.5  (true = East local-owns)
        //   eastPolyOwner[v][l] : East's   ownerId(v,l)    > 0.5  (true = East local-owns; v = poly index 0..14)
        bool monoV1Owner[4]      = { true, true, true, true };
        bool eastV1Owner[4]      = { true, true, true, true };
        bool eastPolyOwner[15][4] = {};   // default false → Macro-owned until set; caller fills when East present
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
        const bool m = i.monoPresent, e = i.eastPresent, x = i.macroPresent;
        if (!m && !e && !x) return Config::EMPTY;
        if ( m && !e && !x) return Config::MONO;
        if (!m &&  e && !x) return Config::EAST;
        if (!m && !e &&  x) return Config::MACRO_SOLE;
        if ( m &&  e && !x) return Config::MONO_PLUS_EAST;
        if ( m && !e &&  x) return Config::MONO_PLUS_MACRO;
        if (!m &&  e &&  x) return Config::EAST_PLUS_MACRO;
        return Config::MONO_EAST_MACRO;   // m && e && x
    }

    // ── OWNS(voice, editorLane) — the single source of ownership ──────────────
    // voice 0 = V1/mono slot. Lanes 4/5 (VAR/LEG) are mono-only → MONO when Mono
    // present, else NONE. Lanes 0..3 follow the per-surface owner params.
    Role owner(int voice, int editorLane) const {
        if (editorLane < 0 || editorLane > 5) return Role::NONE;

        // V1 / mono slot.
        if (voice == 0) {
            // VAR/LEG are mono-only and always Mono-owned (never delegable).
            if (editorLane >= 4) return in.monoPresent ? Role::MONO : Role::NONE;

            if (in.monoPresent) {
                // Mono owns V1 unless it has ceded this lane to Macro (and Macro present).
                if (in.macroPresent && !in.monoV1Owner[editorLane]) return Role::MACRO;
                return Role::MONO;
            }
            if (in.eastPresent) {
                // East is the V1 editor when no Mono. Per-lane: East or delegated-to-Macro.
                if (in.macroPresent && !in.eastV1Owner[editorLane]) return Role::MACRO;
                return Role::EAST;
            }
            if (in.macroPresent) return Role::MACRO;   // MACRO_SOLE owns V1
            return Role::NONE;
        }

        // Poly voices (voice >= 1). VAR/LEG don't exist on poly → NONE.
        if (editorLane >= 4) return Role::NONE;
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
