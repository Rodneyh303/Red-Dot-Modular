#pragma once
#include <rack.hpp>
#include "../../ui/SandsVisualEditorV4.hpp"
#include "../engines/PatternEngine.hpp"
#include "SpreadManager.hpp"

namespace redDot {

struct MonoSandsParameterManager {
    PatternEngine* patternEngine = nullptr;

    // SpreadManager kept for the poly arrays / spread plumbing.
    SpreadManager spreadMgr;

    // Base spread per SPREADABLE lane. Indexed by this manager's BUFFER lane order:
    //   REST0 / MEL1 / OCT2 / LEG3 / ACC4 / VAR5 / QMIX6   (QMIX appended at buffer lane 6).
    // Set by per-lane trimpots, may be CV-modulated.
    // LEGATO/VARIATION are mono-only — they have NO poly counterpart, so spread does not
    // apply to them at all. REST/MEL/OCT/ACCENT/QMIX are the poly-derived spreadable lanes.
    static constexpr int QMIX_BUFFER_LANE = 6;   // this manager's buffer-lane slot for QMIX
    static constexpr int SPREAD_LANES = 7;   // index by buffer lane order; eligibility via isSpreadLane
    // Spreadable lanes: REST(0), MELODY(1), OCTAVE(2), ACCENT(4) and QMIX(6) — the poly-derived
    // lanes. LEGATO(3) and VARIATION(5) remain mono-only (raw draw, no spread).
    static constexpr bool isSpreadLane(int lane) {
        return lane == 0 || lane == 1 || lane == 2 || lane == 4 || lane == QMIX_BUFFER_LANE;
    }
    float laneSpread[SPREAD_LANES] = {};


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
            laneSpread[lane] = rack::math::clamp(value, -1.f, 1.f);
    }

    static constexpr int LANE_COUNT = 7;   // REST/MEL/OCT/LEG/ACC/VAR + QMIX(6)

    float monoDraw(int lane, int step) const {
        switch (lane) {
            case 0: return patternEngine->slewedRhythm[step];
            case 1: return patternEngine->slewedMelody[step];
            case 2: return patternEngine->slewedOctave[step];
            case 3: return patternEngine->slewedLegato[step];
            case 4: return patternEngine->slewedAccent[step];
            case 5: return patternEngine->slewedVariation[step];
            case 6: return patternEngine->slewedQmix[step];   // QMIX (buffer lane 6)
            default: return 0.5f;
        }
    }

    // Poly average for a spreadable lane (REST/MEL/OCT/ACCENT), INCLUDING the mono
    // voice itself. Reads SLEWED poly draws. Only called for spreadable lanes.
    // Ensemble = mono + active poly voices (numPolyVoices), matching the audio
    // path in processDNA so the display equals what plays.
    float polyAverageInclMono(int lane, int step) const {
        int nPoly = patternEngine ? rack::math::clamp(patternEngine->numPolyVoicesHint, 0, 15) : 0;
        float sum = monoDraw(lane, step);   // mono voice
        for (int v = 0; v < nPoly; ++v) {
            switch (lane) {
                case 0: sum += patternEngine->slewedPolyRhythm[v][step]; break;
                case 1: sum += patternEngine->slewedPolyMelody[v][step]; break;
                case 2: sum += patternEngine->slewedPolyOctave[v][step]; break;
                case 4: sum += patternEngine->slewedPolyAccent[v][step]; break;
                case 6: sum += patternEngine->slewedPolyQmix[v][step]; break;   // QMIX
            }
        }
        return sum / (float)(1 + nPoly);
    }

    // Post-spread value for (lane, step). REST/MEL/OCT get spread; LEG/ACC/VAR
    // are mono-only → raw slewed draw.
    float spreadValue(int lane, int step) const {
        if (isSpreadLane(lane)) {
            float original = monoDraw(lane, step);

            // Spread target is always the mono (voice-1) draw; self-target is a
            // positive no-op and a negative invert.
            return redDot::SpreadInterp::interpolate(original, original, laneSpread[lane]);
        }
        return monoDraw(lane, step);
    }

    // Sands owns the spread→final stage (audio thread): write spread(slewedDraw)
    // into the FINAL mono arrays the sequencer reads, and set sandsActive so slew
    // does not copy the un-spread draw over the top.
    void writeFinal() {
        if (!patternEngine) return;
        patternEngine->setSandsActive(true);
        for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
            patternEngine->rhythmRandom[i]    = spreadValue(0, i);
            patternEngine->melodyRandom[i]    = spreadValue(1, i);
            patternEngine->octaveRandom[i]    = spreadValue(2, i);
            patternEngine->legatoRandom[i]    = spreadValue(3, i);
            patternEngine->accentRandom[i]    = spreadValue(4, i);
            patternEngine->variationRandom[i] = spreadValue(5, i);
            patternEngine->qmixRandom[i]      = spreadValue(QMIX_BUFFER_LANE, i);  // QMIX (buffer lane 6)
        }
    }

    // MODEL 2 (DORMANT): freehand probability editing. V4 editor is a
    // transparent widget in Model 1 (handles set L/O/R only). Kept for a
    // possible future "draw your own probability" direction.
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

    // DSP → display. Editor shows the FINAL probability (post-slew, post-spread),
    // exactly what the sequencer thresholds. LEG/ACC/VAR show their raw slewed
    // draw (no spread). LOR handles set the index window separately.
    void syncPatternEngineToEditor(SandsVisualEditorV4::VoiceState& editorState) {
        if (!patternEngine) return;
        // Buffer lane → editor lane. Buffer order is REST0/MEL1/OCT2/LEG3/ACC4/VAR5/QMIX6
        // (QMIX appended at buffer lane 6, QMIX_BUFFER_LANE), mapped to its editor row.
        static const int laneMap[LANE_COUNT] = {
            SandsVisualEditorV4::REST, SandsVisualEditorV4::MELODY, SandsVisualEditorV4::OCTAVE,
            SandsVisualEditorV4::LEGATO, SandsVisualEditorV4::ACCENT, SandsVisualEditorV4::VARIATION,
            SandsVisualEditorV4::QMIX
        };
        for (int l = 0; l < LANE_COUNT; ++l) {
            for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
                editorState.lanes[laneMap[l]].probabilities[i] = spreadValue(l, i);
            }
        }
    }
};

}  // namespace redDot
