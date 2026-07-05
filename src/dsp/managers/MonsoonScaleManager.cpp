#include "MonsoonScaleManager.hpp"
#include "../../Monsoon.hpp"
#include "MonsoonParameterManager.hpp"

using namespace rack;

const std::vector<ScaleType> MONSOON_SCALES = {
    {"Chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"Major", {0, 2, 4, 5, 7, 9, 11}},
    {"Natural Minor", {0, 2, 3, 5, 7, 8, 10}},
    {"Harmonic Minor", {0, 2, 3, 5, 7, 8, 11}},
    {"Melodic Minor", {0, 2, 3, 5, 7, 9, 11}},
    {"Dorian", {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygian", {0, 1, 3, 5, 7, 8, 10}},
    {"Lydian", {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydian", {0, 2, 4, 5, 7, 9, 10}},
    {"Locrian", {0, 1, 3, 5, 6, 8, 10}},
    {"Pentatonic Major", {0, 2, 4, 7, 9}},
    {"Pentatonic Minor", {0, 3, 5, 7, 10}},
    {"Blues", {0, 3, 5, 6, 7, 10}},
    {"Whole Tone", {0, 2, 4, 6, 8, 10}},
    {"Diminished", {0, 2, 3, 5, 6, 8, 9, 11}},
    {"Super Locrian", {0, 1, 3, 4, 6, 8, 10}},
    {"Bhairav", {0, 1, 4, 5, 7, 8, 11}},
    {"Hungarian Minor", {0, 2, 3, 6, 7, 8, 11}},
    {"Hirojoshi", {0, 2, 3, 7, 8}},
    {"In-Sen", {0, 1, 5, 7, 10}},
    {"Iwato", {0, 1, 5, 6, 10}},
    {"Kumoi", {0, 2, 3, 7, 9}},
    {"Pelog", {0, 1, 3, 7, 8}},
    {"Spanish", {0, 1, 4, 5, 7, 8, 10}}
};

void ScaleManager::updateScaleMask() {
    if (!module) return;
    using namespace MonsoonIds;

    activeScaleMask = calculateMask(scaleRoot, lastSelectedScale);

    // NON-DESTRUCTIVE enforcement (see SHOPHOUSE_SPEC.md / DISPLAY_STORE_ENGINE_SEPARATION.md):
    // Scale enforcement is done ENTIRELY at READ time in getSemitoneWeight (out-of-scale
    // semitones read as zero probability when locked). We therefore do NOT touch the fader
    // VALUES or ranges here — no setValue(0), no min=max=0 pinning, no redistributeWeights.
    // Faders keep the user's values (the STORE); the mask gates the READ (the ENFORCEMENT);
    // toggling lock or changing scale reveals the faders unchanged. Out-of-scale faders are
    // DIMMED in the UI (the fader widget reads activeScaleMask) rather than forced/pinned.
    // Ranges stay full 0..1 in all cases so a value can never be clamped away and lost.
    for (int i = 0; i < 12; i++) {
        ParamQuantity* pq = module->getParamQuantity(SEMI0_PARAM + i);
        if (pq) {
            pq->minValue = 0.f;
            pq->maxValue = 1.f;
        }
    }
    // (redistributeWeights is retained as a static helper for a possible explicit,
    //  user-invoked "nudge to scale" action later — but it is NO LONGER applied
    //  automatically by enforcement, because that mutated the user's fader values.)
}

float ScaleManager::getSemitoneWeight(int semIdx, const ParameterManager& pm) const {
    if (semIdx < 0 || semIdx > 11) return 0.f;
    
    // Enforce lock: if note is not in scale mask, probability is zero regardless of CV
    if (lockScaleNotes && !(activeScaleMask & (1 << semIdx))) {
        return 0.f;
    }
    
    return pm.getSemitone(semIdx);
}

uint16_t ScaleManager::calculateMask(int root, int scaleIdx) {
    if (scaleIdx < 0 || scaleIdx >= (int)MONSOON_SCALES.size()) return 0xFFF;
    uint16_t mask = 0;
    for (int interval : MONSOON_SCALES[scaleIdx].intervals) {
        mask |= (1 << ((root + interval) % 12));
    }
    return mask;
}

void ScaleManager::redistributeWeights(uint16_t mask, float* weights) {
    float originalWeights[12];
    for (int i = 0; i < 12; i++) {
        originalWeights[i] = weights[i];
        weights[i] = 0.0f;
    }

    // Pass 1: Keep original in-scale weights
    for (int i = 0; i < 12; i++) {
        if ((mask & (1 << i))) {
            weights[i] = originalWeights[i];
        }
    }

    // Pass 2: Redistribute out-of-scale weights to neighbors
    for (int i = 0; i < 12; i++) {
        if (!(mask & (1 << i)) && originalWeights[i] > 0.0001f) {
            float val = originalWeights[i];
            int dist_up = 13, dist_down = 13;
            for (int d = 1; d <= 6; d++) {
                if (mask & (1 << ((i + d) % 12))) { dist_up = d; break; }
            }
            for (int d = 1; d <= 6; d++) {
                if (mask & (1 << ((i - d + 12) % 12))) { dist_down = d; break; }
            }
            if (dist_up < dist_down) weights[(i + dist_up) % 12] += val;
            else if (dist_down < dist_up) weights[(i - dist_down + 12) % 12] += val;
            else if (dist_up <= 6) { weights[(i + dist_up) % 12] += val * 0.5f; weights[(i - dist_down + 12) % 12] += val * 0.5f; }
        }
    }
    for (int i = 0; i < 12; ++i) if (weights[i] > 1.0f) weights[i] = 1.0f;
}