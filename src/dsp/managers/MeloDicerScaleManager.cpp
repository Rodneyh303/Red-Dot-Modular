#include "MeloDicerScaleManager.hpp"
#include "../../MeloDicer.hpp"
#include "MeloDicerParameterManager.hpp"

using namespace rack;

void ScaleManager::updateScaleMask() {
    if (!module) return;
    using namespace MeloDicerIds;

    activeScaleMask = calculateMask(scaleRoot, lastSelectedScale);

    if (lockScaleNotes) {
        float weights[12];
        for (int i = 0; i < 12; i++) {
            weights[i] = module->params[SEMI0_PARAM + i].getValue();
        }

        redistributeWeights(activeScaleMask, weights);

        // Apply the redistributed values and lock the faders in the UI
        for (int i = 0; i < 12; i++) {
            bool inScale = (activeScaleMask & (1 << i));
            ParamQuantity* pq = module->getParamQuantity(SEMI0_PARAM + i);
            if (pq) {
                if (!inScale) {
                    module->params[SEMI0_PARAM + i].setValue(0.f);
                    pq->minValue = 0.f;
                    pq->maxValue = 0.f;
                } else {
                    module->params[SEMI0_PARAM + i].setValue(weights[i]);
                    pq->minValue = 0.f;
                    pq->maxValue = 1.f;
                }
            }
        }
    } else {
        // Unlock logic: allow faders to move freely within 0..1 range
        for (int i = 0; i < 12; i++) {
            ParamQuantity* pq = module->getParamQuantity(SEMI0_PARAM + i);
            if (pq) {
                pq->minValue = 0.f;
                pq->maxValue = 1.f;
            }
        }
    }
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