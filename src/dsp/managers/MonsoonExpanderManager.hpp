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