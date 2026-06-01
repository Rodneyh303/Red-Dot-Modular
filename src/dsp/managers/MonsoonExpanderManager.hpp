<<<<<<< HEAD
#pragma once
#include <rack.hpp>

// Forward declarations
struct MonsoonInterchangeExpander;
struct MonsoonSandsExpander;
struct MonsoonStraitsEastExpander;
struct MonsoonStraitWestExpander;    // NEW (Phase 4)
struct MonsoonStraitSandsExpander;   // NEW (Phase 6)
    
extern rack::Model* modelMonsoonInterchangeExpander;
extern rack::Model* modelMonsoonSandsExpander;
extern rack::Model* modelMonsoonStraitsEastExpander;
extern rack::Model* modelMonsoonStraitWestExpander;     // NEW (Phase 4)
extern rack::Model* modelMonsoonStraitSandsExpander;    // NEW (Phase 6)

/**
 * ExpanderManager handles the discovery and caching of Monsoon expander modules.
 * It walks the left and right expansion chains to identify connected modules.
 */
struct MonsoonExpanderManager {
    MonsoonInterchangeExpander* cachedScaleExpander = nullptr;
    MonsoonSandsExpander* cachedDnaExpander = nullptr;
    MonsoonStraitsEastExpander* cachedPolyVoiceExpander = nullptr;
    MonsoonStraitWestExpander* cachedStraitWestExpander = nullptr;     // NEW (Phase 4)
    MonsoonStraitSandsExpander* cachedStraitSandsExpander = nullptr;   // NEW (Phase 6)

    int scaleExpanderCount = 0;
    int dnaExpanderCount = 0;
    int polyExpanderCount = 0;
    int straitWestExpanderCount = 0;   // NEW (Phase 4)
    int straitSandsExpanderCount = 0;  // NEW (Phase 6)

    void update(rack::Module* module) {
        cachedScaleExpander = nullptr;
        cachedDnaExpander = nullptr;
        cachedPolyVoiceExpander = nullptr;
        cachedStraitWestExpander = nullptr;
        cachedStraitSandsExpander = nullptr;

        scaleExpanderCount = 0;
        dnaExpanderCount = 0;
        polyExpanderCount = 0;
        straitWestExpanderCount = 0;
        straitSandsExpanderCount = 0;

        auto scan = [&](rack::Module* start, bool left) {
            rack::Module* curr = start;
            int depth = 0;
            while (curr && depth < 8) {
                if (curr->model == modelMonsoonInterchangeExpander) {
                    // Model check ensures safe cast
                    if (!cachedScaleExpander) cachedScaleExpander = reinterpret_cast<MonsoonInterchangeExpander*>(curr);
                    scaleExpanderCount++;
                } else if (curr->model == modelMonsoonSandsExpander) {
                    if (!cachedDnaExpander) cachedDnaExpander = reinterpret_cast<MonsoonSandsExpander*>(curr);
                    dnaExpanderCount++;
                } else if (curr->model == modelMonsoonStraitsEastExpander) {
                    if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = reinterpret_cast<MonsoonStraitsEastExpander*>(curr);
                    polyExpanderCount++;
                } else if (curr->model == modelMonsoonStraitWestExpander) {
                    if (!cachedStraitWestExpander) cachedStraitWestExpander = reinterpret_cast<MonsoonStraitWestExpander*>(curr);
                    straitWestExpanderCount++;
                } else if (curr->model == modelMonsoonStraitSandsExpander) {
                    if (!cachedStraitSandsExpander) cachedStraitSandsExpander = reinterpret_cast<MonsoonStraitSandsExpander*>(curr);
                    straitSandsExpanderCount++;
                } else break;
                curr = left ? curr->leftExpander.module : curr->rightExpander.module;
                depth++;
            }
        };

        if (module) {
            scan(module->leftExpander.module, true);
            scan(module->rightExpander.module, false);
        }
    }
};
=======
#pragma once
#include <rack.hpp>

// Forward declarations
struct MonsoonInterchangeExpander;
struct MonsoonSandsExpander;
struct MonsoonSandsVisualExpander;       // Mono visual DNA editor
struct MonsoonStraitsEastExpander;
struct MonsoonStraitWestExpander;
struct MonsoonStraitsSands;
struct MonsoonDeepStraitsSandsEast;
struct MonsoonDeepStraitsSandsWest;
// Poly visual DNA editors
struct StraitsEastSandsVisual;
struct StraitsWestSandsVisual;
struct StraitsSandsMacroVisual;

extern rack::Model* modelMonsoonInterchangeExpander;
extern rack::Model* modelMonsoonSandsExpander;
extern rack::Model* modelMonsoonSandsVisualExpander;
extern rack::Model* modelMonsoonStraitsEastExpander;
extern rack::Model* modelMonsoonStraitWestExpander;
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
    MonsoonSandsExpander*        cachedDnaExpander                = nullptr;
    MonsoonSandsVisualExpander*  cachedSandsVisualExpander        = nullptr;
    MonsoonStraitsEastExpander*  cachedPolyVoiceExpander          = nullptr;
    MonsoonStraitWestExpander*   cachedStraitWestExpander         = nullptr;
    MonsoonStraitsSands*         cachedStraitsSandsExpander       = nullptr;
    MonsoonDeepStraitsSandsEast* cachedDeepStraitsSandsEastExpander = nullptr;
    MonsoonDeepStraitsSandsWest* cachedDeepStraitsSandsWestExpander = nullptr;
    // Poly visual DNA editors
    StraitsEastSandsVisual*      cachedEastSandsVisual            = nullptr;
    StraitsWestSandsVisual*      cachedWestSandsVisual            = nullptr;
    StraitsSandsMacroVisual*     cachedMacroSandsVisual           = nullptr;

    int scaleExpanderCount              = 0;
    int dnaExpanderCount                = 0;
    int sandsVisualExpanderCount        = 0;
    int polyExpanderCount               = 0;
    int straitWestExpanderCount         = 0;
    int straitsSandsExpanderCount       = 0;
    int deepStraitsSandsEastExpanderCount = 0;
    int deepStraitsSandsWestExpanderCount = 0;
    int eastSandsVisualCount            = 0;
    int westSandsVisualCount            = 0;
    int macroSandsVisualCount           = 0;

    void update(rack::Module* module) {
        cachedScaleExpander              = nullptr;
        cachedDnaExpander                = nullptr;
        cachedSandsVisualExpander        = nullptr;
        cachedPolyVoiceExpander          = nullptr;
        cachedStraitWestExpander         = nullptr;
        cachedStraitsSandsExpander       = nullptr;
        cachedDeepStraitsSandsEastExpander = nullptr;
        cachedDeepStraitsSandsWestExpander = nullptr;
        cachedEastSandsVisual            = nullptr;
        cachedWestSandsVisual            = nullptr;
        cachedMacroSandsVisual           = nullptr;

        scaleExpanderCount              = 0;
        dnaExpanderCount                = 0;
        sandsVisualExpanderCount        = 0;
        polyExpanderCount               = 0;
        straitWestExpanderCount         = 0;
        straitsSandsExpanderCount       = 0;
        deepStraitsSandsEastExpanderCount = 0;
        deepStraitsSandsWestExpanderCount = 0;
        eastSandsVisualCount            = 0;
        westSandsVisualCount            = 0;
        macroSandsVisualCount           = 0;

        auto scan = [&](rack::Module* start, bool left) {
            rack::Module* curr = start;
            int depth = 0;
            while (curr && depth < 8) {
                if (curr->model == modelMonsoonInterchangeExpander) {
                    if (!cachedScaleExpander) cachedScaleExpander = reinterpret_cast<MonsoonInterchangeExpander*>(curr);
                    scaleExpanderCount++;
                } else if (curr->model == modelMonsoonSandsExpander) {
                    if (!cachedDnaExpander) cachedDnaExpander = reinterpret_cast<MonsoonSandsExpander*>(curr);
                    dnaExpanderCount++;
                } else if (curr->model == modelMonsoonSandsVisualExpander) {
                    if (!cachedSandsVisualExpander) cachedSandsVisualExpander = reinterpret_cast<MonsoonSandsVisualExpander*>(curr);
                    sandsVisualExpanderCount++;
                } else if (curr->model == modelMonsoonStraitsEastExpander) {
                    if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = reinterpret_cast<MonsoonStraitsEastExpander*>(curr);
                    polyExpanderCount++;
                } else if (curr->model == modelMonsoonStraitWestExpander) {
                    if (!cachedStraitWestExpander) cachedStraitWestExpander = reinterpret_cast<MonsoonStraitWestExpander*>(curr);
                    straitWestExpanderCount++;
                } else if (curr->model == modelMonsoonStraitsSands) {
                    if (!cachedStraitsSandsExpander) cachedStraitsSandsExpander = reinterpret_cast<MonsoonStraitsSands*>(curr);
                    straitsSandsExpanderCount++;
                } else if (curr->model == modelMonsoonDeepStraitsSandsEast) {
                    if (!cachedDeepStraitsSandsEastExpander) cachedDeepStraitsSandsEastExpander = reinterpret_cast<MonsoonDeepStraitsSandsEast*>(curr);
                    deepStraitsSandsEastExpanderCount++;
                } else if (curr->model == modelMonsoonDeepStraitsSandsWest) {
                    if (!cachedDeepStraitsSandsWestExpander) cachedDeepStraitsSandsWestExpander = reinterpret_cast<MonsoonDeepStraitsSandsWest*>(curr);
                    deepStraitsSandsWestExpanderCount++;
                } else if (curr->model == modelStraitsEastSandsVisual) {
                    if (!cachedEastSandsVisual) cachedEastSandsVisual = reinterpret_cast<StraitsEastSandsVisual*>(curr);
                    eastSandsVisualCount++;
                } else if (curr->model == modelStraitsWestSandsVisual) {
                    if (!cachedWestSandsVisual) cachedWestSandsVisual = reinterpret_cast<StraitsWestSandsVisual*>(curr);
                    westSandsVisualCount++;
                } else if (curr->model == modelStraitsSandsMacroVisual) {
                    if (!cachedMacroSandsVisual) cachedMacroSandsVisual = reinterpret_cast<StraitsSandsMacroVisual*>(curr);
                    macroSandsVisualCount++;
                } else break;
                curr = left ? curr->leftExpander.module : curr->rightExpander.module;
                depth++;
            }
        };

        if (module) {
            scan(module->leftExpander.module, true);
            scan(module->rightExpander.module, false);
        }
    }
};

>>>>>>> 091ed97df88f5f836c12b99b805c203028fdcdf8
