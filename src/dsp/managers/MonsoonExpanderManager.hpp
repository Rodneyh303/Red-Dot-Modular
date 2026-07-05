#pragma once
#include <rack.hpp>

// Forward declarations
class SequencerEngine;

// Forward declarations
struct MonsoonInterchangeExpander;
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
struct StraitsWestSandsVisual;
struct StraitsSandsMacroVisual;

extern rack::Model* modelMonsoon;
extern rack::Model* modelMonsoonInterchangeExpander;
extern rack::Model* modelMonsoonRafflesExpander;
extern rack::Model* modelMonsoonJunctionExpander;
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
extern rack::Model* modelStraitsWestSandsVisual;
extern rack::Model* modelStraitsSandsMacroVisual;

/**
 * ExpanderManager handles the discovery and caching of Monsoon expander modules.
 * It walks the left and right expansion chains to identify connected modules.
 */
struct MonsoonExpanderManager {
    MonsoonInterchangeExpander*  cachedScaleExpander              = nullptr;
    rack::Module*                cachedRafflesExpander           = nullptr;
    rack::Module*                cachedJunctionExpander              = nullptr;
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
    StraitsWestSandsVisual*      cachedWestSandsVisual            = nullptr;
    StraitsSandsMacroVisual*     cachedMacroSandsVisual           = nullptr;

    int scaleExpanderCount              = 0;
    int dnaExpanderCount                = 0;
    int sandsVisualExpanderCount        = 0;
    int polyExpanderCount               = 0;
    int straitsSandsExpanderCount       = 0;
    int deepStraitsSandsEastExpanderCount = 0;
    int deepStraitsSandsWestExpanderCount = 0;
    int eastSandsVisualCount            = 0;
    int westSandsVisualCount            = 0;
    int macroSandsVisualCount           = 0;

    void update(rack::Module* module) {
        cachedScaleExpander              = nullptr;
        cachedRafflesExpander           = nullptr;
        cachedJunctionExpander              = nullptr;
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
        cachedWestSandsVisual            = nullptr;
        cachedMacroSandsVisual           = nullptr;

        scaleExpanderCount              = 0;
        //dnaExpanderCount                = 0;
        sandsVisualExpanderCount        = 0;
        polyExpanderCount               = 0;
        straitsSandsExpanderCount       = 0;
        //deepStraitsSandsEastExpanderCount = 0;
        deepStraitsSandsWestExpanderCount = 0;
        eastSandsVisualCount            = 0;
        westSandsVisualCount            = 0;
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
                // } else if (curr->model == modelStraitsWestSandsVisual) {
                //     if (!cachedWestSandsVisual) cachedWestSandsVisual = reinterpret_cast<StraitsWestSandsVisual*>(curr);
                //     westSandsVisualCount++;
                } else if (curr->model == modelStraitsSandsMacroVisual) {
                    if (!cachedMacroSandsVisual) cachedMacroSandsVisual = reinterpret_cast<StraitsSandsMacroVisual*>(curr);
                    macroSandsVisualCount++;
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
};
