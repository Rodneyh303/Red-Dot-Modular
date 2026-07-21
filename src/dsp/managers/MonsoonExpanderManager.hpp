#pragma once
#include <rack.hpp>
#include "../SandsTopology.hpp"   // for SandsTopology::Inputs (the presence authority target)

// Forward declarations
class SequencerEngine;

// Forward declarations
struct MonsoonInterchangeExpander;
struct MonsoonChangeAlleyExpander;
struct MonsoonSandsExpander;
struct MonsoonSandsVisualExpander;       // Mono visual DNA editor
struct MonsoonStraitsExpander;
struct MonsoonCausewayPolyExpander;
struct MonsoonChangiExpander;
struct MonsoonShophouseExpander;
struct MonsoonStraitsSands;
struct MonsoonDeepStraitsSandsEast;
struct MonsoonDeepStraitsSandsWest;
// Poly visual DNA editors
struct StraitsEastSandsVisual;
struct StraitsSandsMacroVisual;

extern rack::Model* modelMonsoon;
extern rack::Model* modelMonsoonInterchangeExpander;
extern rack::Model* modelMonsoonRafflesExpander;
extern rack::Model* modelMonsoonJunctionExpander;
extern rack::Model* modelMonsoonChangeAlleyExpander;
extern rack::Model* modelMonsoonSandsExpander;
extern rack::Model* modelMonsoonSandsVisualExpander;
extern rack::Model* modelMonsoonStraitsExpander;
extern rack::Model* modelMonsoonCausewayPolyExpander;
extern rack::Model* modelMonsoonChangiExpander;
extern rack::Model* modelMonsoonShophouseExpander;
extern rack::Model* modelMonsoonStraitsSands;
extern rack::Model* modelMonsoonDeepStraitsSandsEast;
extern rack::Model* modelMonsoonDeepStraitsSandsWest;
extern rack::Model* modelStraitsEastSandsVisual;
extern rack::Model* modelStraitsSandsMacroVisual;
extern rack::Model* modelLantern;   // note-output visualiser — suite member, hop-only (see scan)

/**
 * ExpanderManager handles the discovery and caching of Monsoon expander modules.
 * It walks the left and right expansion chains to identify connected modules.
 */
struct MonsoonExpanderManager {
    // ── Single source of truth for WHO owns V1/mono direction on a lane ───────────
    // The manager READS this to push laneDirPending_; East's V1 direction gate-mod WRITES
    // through it. Both must agree, or the mod writes one store while the manager pushes
    // another and the modulation silently does nothing (which is exactly what happened:
    // East's ch0 mod wrote East's monoDirId while the manager was pushing Macro's cell at
    // control rate, so V1 lanes 0..3 never moved). Encoding the precedence once is the point.
    struct MonoDirSrc {
        rack::Module* mod = nullptr;   // owning expander, or null if nothing owns the lane
        int paramId = -1;              // its direction param for this lane (param-backed sources)
        // East's V1 direction migrated from a param to Monsoon::editor.laneDir
        // (NUM_PARAMS_MIGRATION.md). When the owner is East, the source is a FIELD, not a param:
        // eastMonoLane >= 0 marks that, and the value is read/written via Monsoon's
        // getMonoLaneDir/setMonoLaneDir(eastMonoLane) instead of params[paramId].
        int eastMonoLane = -1;         // >=0 => field-backed (East V1); read via Monsoon accessor
        bool valid() const { return mod && (paramId >= 0 || eastMonoLane >= 0); }
        bool isField() const { return eastMonoLane >= 0; }
    };
    // lane = STRAND index 0..5. Precedence: Mono (if it owns the lane; it always owns
    // VAR/LEG) → Macro (lanes 0..3 only; it has no VAR/LEG) → East's V1 slot → nothing.
    MonoDirSrc monoDirAuthority(int lane) const;

    MonsoonInterchangeExpander*  cachedScaleExpander              = nullptr;
    rack::Module*                cachedRafflesExpander           = nullptr;
    rack::Module*                cachedJunctionExpander          = nullptr;
    MonsoonChangeAlleyExpander*  cachedChangeAlleyExpander       = nullptr;
    int  caPrevStep_   = 0;      // phrase-boundary detect for restructure queue
    bool caPrevLocked_ = false;  // unlock-edge detect for restructure queue
    //MonsoonSandsExpander*        cachedDnaExpander                = nullptr;
    MonsoonSandsVisualExpander*  cachedSandsVisualExpander        = nullptr;
    MonsoonStraitsExpander*      cachedPolyVoiceExpander          = nullptr;
    MonsoonCausewayPolyExpander* cachedCausewayPolyExpander       = nullptr;
    MonsoonChangiExpander*       cachedChangiExpander             = nullptr;
    MonsoonShophouseExpander*    cachedShophouseExpander          = nullptr;
    MonsoonStraitsSands*         cachedStraitsSandsExpander       = nullptr;
    //MonsoonDeepStraitsSandsEast* cachedDeepStraitsSandsEastExpander = nullptr;
    //MonsoonDeepStraitsSandsWest* cachedDeepStraitsSandsWestExpander = nullptr;
    // Poly visual DNA editors
    StraitsEastSandsVisual*      cachedEastSandsVisual            = nullptr;
    StraitsSandsMacroVisual*     cachedMacroSandsVisual           = nullptr;

    int scaleExpanderCount              = 0;
    int dnaExpanderCount                = 0;
    int sandsVisualExpanderCount        = 0;
    int polyExpanderCount               = 0;
    int straitsSandsExpanderCount       = 0;
    int deepStraitsSandsEastExpanderCount = 0;
    int deepStraitsSandsWestExpanderCount = 0;
    int eastSandsVisualCount            = 0;
    int macroSandsVisualCount           = 0;

    void update(rack::Module* module) {
        cachedScaleExpander              = nullptr;
        cachedRafflesExpander           = nullptr;
        cachedJunctionExpander          = nullptr;
        cachedChangeAlleyExpander       = nullptr;
        //cachedDnaExpander                = nullptr;
        cachedSandsVisualExpander        = nullptr;
        cachedPolyVoiceExpander          = nullptr;
        cachedCausewayPolyExpander       = nullptr;
        cachedChangiExpander             = nullptr;
        cachedShophouseExpander          = nullptr;
        cachedStraitsSandsExpander       = nullptr;
       // cachedDeepStraitsSandsEastExpander = nullptr;
        //cachedDeepStraitsSandsWestExpander = nullptr;
        cachedEastSandsVisual            = nullptr;
        cachedMacroSandsVisual           = nullptr;

        scaleExpanderCount              = 0;
        //dnaExpanderCount                = 0;
        sandsVisualExpanderCount        = 0;
        polyExpanderCount               = 0;
        straitsSandsExpanderCount       = 0;
        //deepStraitsSandsEastExpanderCount = 0;
        deepStraitsSandsWestExpanderCount = 0;
        eastSandsVisualCount            = 0;
        macroSandsVisualCount           = 0;

        auto scan = [&](rack::Module* start, bool left) {
            rack::Module* curr = start;
            int depth = 0;
            // Depth cap = max expanders that can chain on ONE side. The suite has 12 modules:
            // Monsoon (parent) + 11 expander types, so up to 11 could all be on one side. Cap at
            // 12 for a little headroom (duplicates, future modules) without unbounded walking.
            while (curr && depth < 12) {
                // Rule 3: a Monsoon is NOT an expander of another Monsoon. Treat
                // it (and anything unrecognised) as foreign and stop this side.
                if (curr->model == modelMonsoon) break;

                // Rule 2: at most one pointer per type — only record into an
                // empty slot. (Counts kept for diagnostics.)
                if (curr->model == modelMonsoonInterchangeExpander) {
                    if (!cachedScaleExpander) cachedScaleExpander = reinterpret_cast<MonsoonInterchangeExpander*>(curr);
                    scaleExpanderCount++;
                } else if (curr->model == modelMonsoonRafflesExpander) {
                    if (!cachedRafflesExpander) cachedRafflesExpander = curr;
                } else if (curr->model == modelMonsoonJunctionExpander) {
                    if (!cachedJunctionExpander) cachedJunctionExpander = curr;
                } else if (curr->model == modelMonsoonChangeAlleyExpander) {
                    if (!cachedChangeAlleyExpander)
                        cachedChangeAlleyExpander = reinterpret_cast<MonsoonChangeAlleyExpander*>(curr);
                // } else if (curr->model == modelMonsoonSandsExpander) {
                //     if (!cachedDnaExpander) cachedDnaExpander = reinterpret_cast<MonsoonSandsExpander*>(curr);
                //     dnaExpanderCount++;
                } else if (curr->model == modelMonsoonSandsVisualExpander) {
                    if (!cachedSandsVisualExpander) cachedSandsVisualExpander = reinterpret_cast<MonsoonSandsVisualExpander*>(curr);
                    sandsVisualExpanderCount++;
                } else if (curr->model == modelMonsoonStraitsExpander) {
                    if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = reinterpret_cast<MonsoonStraitsExpander*>(curr);
                    polyExpanderCount++;
                } else if (curr->model == modelMonsoonCausewayPolyExpander) {
                    if (!cachedCausewayPolyExpander) cachedCausewayPolyExpander = reinterpret_cast<MonsoonCausewayPolyExpander*>(curr);
                } else if (curr->model == modelMonsoonChangiExpander) {
                    if (!cachedChangiExpander) cachedChangiExpander = reinterpret_cast<MonsoonChangiExpander*>(curr);
                } else if (curr->model == modelMonsoonShophouseExpander) {
                    if (!cachedShophouseExpander) cachedShophouseExpander = reinterpret_cast<MonsoonShophouseExpander*>(curr);
                // } else if (curr->model == modelMonsoonStraitsSands) {
                //     if (!cachedStraitsSandsExpander) cachedStraitsSandsExpander = reinterpret_cast<MonsoonStraitsSands*>(curr);
                //     straitsSandsExpanderCount++;
                // } else if (curr->model == modelMonsoonDeepStraitsSandsEast) {
                //     if (!cachedDeepStraitsSandsEastExpander) cachedDeepStraitsSandsEastExpander = reinterpret_cast<MonsoonDeepStraitsSandsEast*>(curr);
                //     deepStraitsSandsEastExpanderCount++;
                // } else if (curr->model == modelMonsoonDeepStraitsSandsWest) {
                //     if (!cachedDeepStraitsSandsWestExpander) cachedDeepStraitsSandsWestExpander = reinterpret_cast<MonsoonDeepStraitsSandsWest*>(curr);
                //     deepStraitsSandsWestExpanderCount++;
                } else if (curr->model == modelStraitsEastSandsVisual) {
                    if (!cachedEastSandsVisual) cachedEastSandsVisual = reinterpret_cast<StraitsEastSandsVisual*>(curr);
                    eastSandsVisualCount++;
                } else if (curr->model == modelStraitsSandsMacroVisual) {
                    if (!cachedMacroSandsVisual) cachedMacroSandsVisual = reinterpret_cast<StraitsSandsMacroVisual*>(curr);
                    macroSandsVisualCount++;
                } else if (curr->model == modelLantern) {
                    // Lantern is a suite note-output visualiser, NOT an expander Monsoon claims
                    // (it reads FROM Monsoon via findMonsoonEitherSide, never the reverse). But it
                    // lives in the chain — often placed between Monsoon and other expanders — so
                    // recognise it here to HOP through, instead of stopping at it (Rule 1). Without
                    // this, modules to the right/left of Lantern were never cached → ConnectMarks
                    // showed disconnected. No cache slot: Monsoon reads no expander data from it.
                } else break;   // Rule 1: stop at first foreign module.

                // (No early-out. Chains are at most depth-8 per side, so the cost of always
                //  walking both sides is negligible, and it keeps discovery ORDER-STABLE: the
                //  result never depends on whether all types happened to be found on one side
                //  first. Each type still caches the FIRST instance found — left side, then right.)

                curr = left ? curr->leftExpander.module : curr->rightExpander.module;
                depth++;
            }
        };

        if (module) {
            // Always scan BOTH sides fully. First-match-wins per type (left before right) is the
            // one intentional order rule; everything else is order-independent.
            scan(module->leftExpander.module, true);
            scan(module->rightExpander.module, false);
        }
    }

    // True once one pointer of every ACTIVE expander type has been cached. Only lists pointers
    // whose cache branches exist in scan() — previously it also required cachedStraitsSandsExpander
    // and cachedWestSandsVisual, whose branches are commented out (deprecated modules), so it could
    // NEVER be true and the scan early-outs were dead. Now it reflects the live module set, so the
    // "stop once everything's found" optimisation actually works and the walk is honest.
    bool allTypesFound() const {
        return cachedScaleExpander && cachedRafflesExpander && cachedJunctionExpander
            && cachedSandsVisualExpander && cachedPolyVoiceExpander
            && cachedCausewayPolyExpander && cachedChangiExpander && cachedShophouseExpander
            && cachedEastSandsVisual && cachedMacroSandsVisual;
    }

    /// Synchronizes data between the engine and specific expanders (Deep Straits, Visual Editors, etc.)
    void sync(SequencerEngine& engine, bool spreadInterpMono = false);

    // ── Single presence authority for SandsTopology ─────────────────────────────
    // THE one place the topology's presence/base fields are populated. Every consumer
    // (managers AND widgets) calls this instead of hand-setting eastPresent/macroPresent/
    // monoPresent/polyBaseActive — which previously diverged (widgets hard-coded their own
    // presence as `true`, managers read the cache; polyBaseActive was defined 3 ways). The
    // authoritative source of "is X present" is ALWAYS the expander-scan cache pointer, never
    // a widget's self-knowledge: a widget that Monsoon's scan hasn't cached is NOT topologically
    // present (this is what produced the MACRO-then-EAST strand clobber). numPolyVoices is passed
    // because this manager doesn't hold the engine.
    void fillPresence(dotModular::SandsTopology::Inputs& in, int numPolyVoices) const {
        in.monoPresent     = (cachedSandsVisualExpander != nullptr);
        in.eastPresent     = (cachedEastSandsVisual     != nullptr);
        in.macroPresent    = (cachedMacroSandsVisual    != nullptr);
        in.polyBaseActive  = (cachedPolyVoiceExpander   != nullptr) && (numPolyVoices >= 1);
        in.polyVoiceCount  = numPolyVoices;
    }
};
