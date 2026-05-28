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

extern rack::Model* modelMonsoonInterchangeExpander;
extern rack::Model* modelMonsoonSandsExpander;
extern rack::Model* modelMonsoonSandsVisualExpander;    // Mono visual DNA editor
extern rack::Model* modelMonsoonStraitsEastExpander;
extern rack::Model* modelMonsoonStraitWestExpander;
extern rack::Model* modelMonsoonStraitsSands;
extern rack::Model* modelMonsoonDeepStraitsSandsEast;
extern rack::Model* modelMonsoonDeepStraitsSandsWest;

/**
 * ExpanderManager handles the discovery and caching of Monsoon expander modules.
 * It walks the left and right expansion chains to identify connected modules.
 */
struct MonsoonExpanderManager {
    MonsoonInterchangeExpander*  cachedScaleExpander              = nullptr;
    MonsoonSandsExpander*        cachedDnaExpander                = nullptr;
    MonsoonSandsVisualExpander*  cachedSandsVisualExpander        = nullptr;  // Visual DNA editor
    MonsoonStraitsEastExpander*  cachedPolyVoiceExpander          = nullptr;
    MonsoonStraitWestExpander*   cachedStraitWestExpander         = nullptr;
    MonsoonStraitsSands*         cachedStraitsSandsExpander       = nullptr;
    MonsoonDeepStraitsSandsEast* cachedDeepStraitsSandsEastExpander = nullptr;
    MonsoonDeepStraitsSandsWest* cachedDeepStraitsSandsWestExpander = nullptr;

    int scaleExpanderCount              = 0;
    int dnaExpanderCount                = 0;
    int sandsVisualExpanderCount        = 0;
    int polyExpanderCount               = 0;
    int straitWestExpanderCount         = 0;
    int straitsSandsExpanderCount       = 0;
    int deepStraitsSandsEastExpanderCount = 0;
    int deepStraitsSandsWestExpanderCount = 0;

    void update(rack::Module* module) {
        cachedScaleExpander              = nullptr;
        cachedDnaExpander                = nullptr;
        cachedSandsVisualExpander        = nullptr;
        cachedPolyVoiceExpander          = nullptr;
        cachedStraitWestExpander         = nullptr;
        cachedStraitsSandsExpander       = nullptr;
        cachedDeepStraitsSandsEastExpander = nullptr;
        cachedDeepStraitsSandsWestExpander = nullptr;

        scaleExpanderCount              = 0;
        dnaExpanderCount                = 0;
        sandsVisualExpanderCount        = 0;
        polyExpanderCount               = 0;
        straitWestExpanderCount         = 0;
        straitsSandsExpanderCount       = 0;
        deepStraitsSandsEastExpanderCount = 0;
        deepStraitsSandsWestExpanderCount = 0;

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
