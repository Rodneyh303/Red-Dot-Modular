#pragma once
#include <rack.hpp>

// Forward declarations
struct MeloDicerExpander;
struct MeloDicerDNAExpander;
struct MeloDicerPolyVoiceExpander;

extern rack::Model* modelMeloDicerExpander;
extern rack::Model* modelMeloDicerDNAExpander;
extern rack::Model* modelMeloDicerPolyVoiceExpander;

/**
 * ExpanderManager handles the discovery and caching of MeloDicer expander modules.
 * It walks the left and right expansion chains to identify connected modules.
 */
struct MeloDicerExpanderManager {
    MeloDicerExpander* cachedScaleExpander = nullptr;
    MeloDicerDNAExpander* cachedDnaExpander = nullptr;
    MeloDicerPolyVoiceExpander* cachedPolyVoiceExpander = nullptr;

    int scaleExpanderCount = 0;
    int dnaExpanderCount = 0;
    int polyExpanderCount = 0;

    void update(rack::Module* module) {
        cachedScaleExpander = nullptr;
        cachedDnaExpander = nullptr;
        cachedPolyVoiceExpander = nullptr;

        scaleExpanderCount = 0;
        dnaExpanderCount = 0;
        polyExpanderCount = 0;

        auto scan = [&](rack::Module* start, bool left) {
            rack::Module* curr = start;
            int depth = 0;
            while (curr && depth < 8) {
                if (curr->model == modelMeloDicerExpander) {
                    // Model check ensures safe cast
                    if (!cachedScaleExpander) cachedScaleExpander = reinterpret_cast<MeloDicerExpander*>(curr);
                    scaleExpanderCount++;
                } else if (curr->model == modelMeloDicerDNAExpander) {
                    if (!cachedDnaExpander) cachedDnaExpander = reinterpret_cast<MeloDicerDNAExpander*>(curr);
                    dnaExpanderCount++;
                } else if (curr->model == modelMeloDicerPolyVoiceExpander) {
                    if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = reinterpret_cast<MeloDicerPolyVoiceExpander*>(curr);
                    polyExpanderCount++;
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