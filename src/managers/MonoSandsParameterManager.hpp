#pragma once
#include <rack.hpp>
#include "../ui/SandsVisualEditorV4.hpp"
#include "../dsp/engines/PatternEngine.hpp"
#include "SpreadManager.hpp"

namespace redDot {

struct MonoSandsParameterManager {
    PatternEngine* patternEngine = nullptr;

    // SpreadManager for mono (1 voice, voice index 0)
    SpreadManager spreadMgr;

    MonoSandsParameterManager(PatternEngine* pe = nullptr)
        : patternEngine(pe)
        , spreadMgr(pe, 1, 0) {}

    void setPatternEngine(PatternEngine* pe) {
        patternEngine = pe;
        spreadMgr.patternEngine = pe;
    }

    // Lane index → PatternEngine array mapping
    static constexpr int LANE_COUNT = 6;

    // Sync visual editor → PatternEngine (editor drag → DSP)
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

    // Sync PatternEngine → visual editor (DSP → display)
    // Uses SpreadManager.getInterpolatedValue() for blended display
    void syncPatternEngineToEditor(SandsVisualEditorV4::VoiceState& editorState) {
        if (!patternEngine) return;

        // Lane→PatternEngine array mapping (voice 0 = mono voice 1)
        // SpreadManager blends between raw draw and interpolation target
        static const int laneMap[6] = {
            SandsVisualEditorV4::REST,
            SandsVisualEditorV4::MELODY,
            SandsVisualEditorV4::OCTAVE,
            SandsVisualEditorV4::LEGATO,
            SandsVisualEditorV4::ACCENT,
            SandsVisualEditorV4::VARIATION
        };

        for (int l = 0; l < LANE_COUNT; ++l) {
            for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
                // Use SpreadManager interpolated value for display
                float blended = spreadMgr.getInterpolatedValue(0, l, i);
                editorState.lanes[laneMap[l]].probabilities[i] = blended;
            }
        }
    }
};

}  // namespace redDot
