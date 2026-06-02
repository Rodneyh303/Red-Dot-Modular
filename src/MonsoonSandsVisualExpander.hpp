#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

namespace SandsMonoVisualIds {

    // ── Panel ────────────────────────────────────────────────────────────
    static constexpr float W_MM     = 203.2f;   // 40HP (was 34HP; +space for a
                                                 // dedicated spread-base column)
    static constexpr float ED_X     = 88.f;
    static constexpr float ED_W     = W_MM - ED_X - 4.f;  // ~111.2mm
    static constexpr float ED_Y     = 16.f;
    static constexpr float ROW_TOP  = 14.f;
    static constexpr float ROW_BOT  = 108.f;
    static constexpr int   N_LANES  = 6;
    static constexpr int   N_SPREAD_LANES = 3;  // REST, MELODY, OCTAVE only

    // Column X positions (mm)
    // LOR CV jacks (all 6 lanes): LEN/OFF/ROT
    static constexpr float JACK_X[3]  = {6.f, 15.f, 24.f};
    // LOR attenuverters (all 6 lanes)
    static constexpr float ATTEN_X[3] = {34.f, 43.f, 52.f};
    // Spread group (REST/MEL/OCT lanes only): base trimpot, CV jack, atten
    static constexpr float SPR_BASE_X  = 62.f;
    static constexpr float SPR_CV_X    = 71.f;
    static constexpr float SPR_ATTEN_X = 80.f;

    static inline float rowY(int lane) {
        return ROW_TOP + (lane + 0.5f) * (ROW_BOT - ROW_TOP) / N_LANES;
    }

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum ParamId {
        // LOR handle params: 6 lanes × 3 (LEN/OFF/ROT) = 18 (0-17)
        LEN_REST = 0, OFF_REST, ROT_REST,
        LEN_MELODY,   OFF_MELODY,   ROT_MELODY,
        LEN_OCTAVE,   OFF_OCTAVE,   ROT_OCTAVE,
        LEN_LEGATO,   OFF_LEGATO,   ROT_LEGATO,
        LEN_ACCENT,   OFF_ACCENT,   ROT_ACCENT,
        LEN_VARIATION,OFF_VARIATION,ROT_VARIATION,
        // Spread base trimpots: REST/MEL/OCT only (18-20)
        SPR_REST, SPR_MELODY, SPR_OCTAVE,
        // Attenuverters: 18 LOR (6 lanes × 3) + 3 spread (REST/MEL/OCT) = 21
        ATTEN_START,                       // 21 .. 38  (18 LOR attens)
        SPR_ATTEN_START = ATTEN_START + 18, // 39 .. 41  (3 spread attens)
        NUM_PARAMS = SPR_ATTEN_START + 3
    };

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        // 18 LOR CV jacks (6 lanes × 3) + 3 spread CV jacks (REST/MEL/OCT) = 21
        CV_START = 0,                       // 0 .. 17
        SPR_CV_START = CV_START + 18,       // 18 .. 20
        NUM_INPUTS = SPR_CV_START + 3
    };

    // ── Helpers ───────────────────────────────────────────────────────────
    inline int lenId(int l) { return LEN_REST + l * 3; }
    inline int offId(int l) { return LEN_REST + l * 3 + 1; }
    inline int rotId(int l) { return LEN_REST + l * 3 + 2; }
    inline int sprId(int l) { return SPR_REST + l; }          // l: 0-2 only

    // LOR atten/CV: lane 0-5, param 0=LEN,1=OFF,2=ROT
    inline int attenId(int lane, int param) { return ATTEN_START + lane*3 + param; }
    inline int cvId   (int lane, int param) { return CV_START    + lane*3 + param; }
    // Spread atten/CV: lane 0-2 (REST/MEL/OCT)
    inline int sprAttenId(int l) { return SPR_ATTEN_START + l; }
    inline int sprCvId   (int l) { return SPR_CV_START    + l; }
}

struct MonsoonSandsVisualExpander : Module {
    // Effective spread per lane — written by processDNA at control rate,
    // read by widget at UI rate. Same cross-thread pattern as Rack params.
    // Base spread lives in sprId(lane) params (set by trimpot, never mutated by CV).
    float spreadEffective[6] = {};

    MonsoonSandsVisualExpander() {
        using namespace SandsMonoVisualIds;
        config(NUM_PARAMS, NUM_INPUTS, 0, 0);

        static const char* names[6]  = {"REST","MEL","OCT","LEG","ACC","VAR"};
        static const char* lnames[3] = {"Len","Off","Rot"};

        for (int l = 0; l < 6; ++l) {
            configParam(lenId(l), 1.f, 16.f, 16.f, std::string(names[l])+" Length");
            configParam(offId(l), 0.f, 15.f,  0.f, std::string(names[l])+" Offset");
            configParam(rotId(l), 0.f, 15.f,  0.f, std::string(names[l])+" Rotation");
            for (int p = 0; p < 3; ++p) {  // LEN/OFF/ROT
                configParam(attenId(l, p), -1.f, 1.f, 1.f,
                            std::string(names[l])+" "+lnames[p]+" depth");
                configInput(cvId(l, p),
                            std::string(names[l])+" "+lnames[p]+" CV");
            }
        }
        // Spread group: REST/MEL/OCT only (mono-only lanes have no spread)
        for (int l = 0; l < N_SPREAD_LANES; ++l) {
            configParam(sprId(l), 0.f, 1.f, 0.f, std::string(names[l])+" Spread");
            configParam(sprAttenId(l), -1.f, 1.f, 1.f, std::string(names[l])+" Spread depth");
            configInput(sprCvId(l), std::string(names[l])+" Spread CV");
        }
    }
    void process(const ProcessArgs&) override {}
};
