#pragma once
#include <rack.hpp>
#include "../../ui/SandsVisualEditorV4.hpp"
#include "../engines/PatternEngine.hpp"
#include "SpreadManager.hpp"

namespace redDot {

struct MonoSandsParameterManager {
    PatternEngine* patternEngine = nullptr;

    // SpreadManager kept for the AVERAGE_POLY target enum + poly arrays.
    SpreadManager spreadMgr;

    // Base spread per SPREADABLE lane (REST, MELODY, OCTAVE only), 0..1.
    // Set by per-lane trimpots, may be CV-modulated.
    // LEGATO/ACCENT/VARIATION are mono-only — they have NO poly counterpart,
    // so spread does not apply to them at all.
    static constexpr int SPREAD_LANES = 3;   // REST, MELODY, OCTAVE
    float laneSpread[SPREAD_LANES] = {};

    SpreadManager::InterpolationTarget target = SpreadManager::AVERAGE_POLY;

    MonoSandsParameterManager(PatternEngine* pe = nullptr)
        : patternEngine(pe)
        , spreadMgr(pe, 1, 0) {}

    void setPatternEngine(PatternEngine* pe) {
        patternEngine = pe;
        spreadMgr.patternEngine = pe;
    }

    // lane: 0=REST, 1=MELODY, 2=OCTAVE only (others ignored — mono-only lanes)
    void setLaneSpread(int lane, float value) {
        if (lane >= 0 && lane < SPREAD_LANES)
            laneSpread[lane] = rack::math::clamp(value, 0.f, 1.f);
    }
    void setInterpolationTarget(SpreadManager::InterpolationTarget t) {
        target = t;
        spreadMgr.setInterpolationTarget(t);
    }

    static constexpr int LANE_COUNT = 6;

    float monoDraw(int lane, int step) const {
        switch (lane) {
            case 0: return patternEngine->rhythmRandom[step];
            case 1: return patternEngine->melodyRandom[step];
            case 2: return patternEngine->octaveRandom[step];
            case 3: return patternEngine->legatoRandom[step];
            case 4: return patternEngine->accentRandom[step];
            case 5: return patternEngine->variationRandom[step];
            default: return 0.5f;
        }
    }

    // Poly average for a spreadable lane (REST/MEL/OCT), INCLUDING the mono
    // voice itself. Only called for lane < 3.
    float polyAverageInclMono(int lane, int step) const {
        int n = 0; float sum = 0.f;
        int active = patternEngine ? 15 : 0;
        for (int v = 0; v < active; ++v) {
            switch (lane) {
                case 0: sum += patternEngine->polyRhythmRandom[v][step]; break;
                case 1: sum += patternEngine->polyMelodyRandom[v][step]; break;
                case 2: sum += patternEngine->polyOctaveRandom[v][step]; break;
            }
            ++n;
        }
        sum += monoDraw(lane, step); ++n;   // include the mono voice
        return (n > 0) ? sum / n : monoDraw(lane, step);
    }

    void syncEditorToPatternEngine(const SandsVisualEditorV4::VoiceState& editorState) {
        if (!patternEngine) return;
        for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
            patternEngine->rhythmRandom[i]    = editorState.lanes[SandsVisualEditorV4::REST      ].probabilities[i];
            patternEngine->rhythmSource[i]    = patternEngine->rhythmRandom[i];
            patternEngine->melodyRandom[i]    = editorState.lanes[SandsVisualEditorV4::MELODY    ].probabilities[i];
            patternEngine->melodySource[i]    = patternEngine->melodyRandom[i];
            patternEngine->octaveRandom[i]    = editorState.lanes[SandsVisualEditorV4::OCTAVE    ].probabilities[i];
            patternEngine->octaveSource[i]    = patternEngine->octaveRandom[i];
            patternEngine->legatoRandom[i]    = editorState.lanes[SandsVisualEditorV4::LEGATO    ].probabilities[i];
            patternEngine->legatoSource[i]    = patternEngine->legatoRandom[i];
            patternEngine->accentRandom[i]    = editorState.lanes[SandsVisualEditorV4::ACCENT    ].probabilities[i];
            patternEngine->accentSource[i]    = patternEngine->accentRandom[i];
            patternEngine->variationRandom[i] = editorState.lanes[SandsVisualEditorV4::VARIATION ].probabilities[i];
            patternEngine->variationSource[i] = patternEngine->variationRandom[i];
        }
    }

    // DSP → display.
    //   REST/MEL/OCT (lanes 0-2): apply spread interpolation.
    //     MONO_DRAW target → spread no-op (original IS the mono draw).
    //     AVERAGE_POLY     → interpolate toward poly average incl. mono voice.
    //   LEG/ACC/VAR (lanes 3-5): mono-only, NO spread — pass raw draw through.
    //   (Fixes the prior bug where these showed a flat 0.5 because SpreadManager
    //    only handled 3 lanes and returned its fallback for the rest.)
    void syncPatternEngineToEditor(SandsVisualEditorV4::VoiceState& editorState) {
        if (!patternEngine) return;
        static const int laneMap[6] = {
            SandsVisualEditorV4::REST, SandsVisualEditorV4::MELODY, SandsVisualEditorV4::OCTAVE,
            SandsVisualEditorV4::LEGATO, SandsVisualEditorV4::ACCENT, SandsVisualEditorV4::VARIATION
        };
        for (int l = 0; l < LANE_COUNT; ++l) {
            for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
                float v;
                if (l < SPREAD_LANES) {
                    float original = monoDraw(l, i);
                    if (target == SpreadManager::MONO_DRAW) {
                        v = original;  // spread no-op
                    } else {
                        float tgt = polyAverageInclMono(l, i);
                        v = original + (tgt - original) * laneSpread[l];
                    }
                } else {
                    v = monoDraw(l, i);  // mono-only lane: raw, no spread
                }
                editorState.lanes[laneMap[l]].probabilities[i] = v;
            }
        }
    }
};

}  // namespace redDot
