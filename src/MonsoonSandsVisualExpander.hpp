#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

namespace SandsMonoVisualIds {

    // ── Panel ────────────────────────────────────────────────────────────
    static constexpr float W_MM     = 172.72f;  // 34HP
    static constexpr float ED_X     = 81.f;
    static constexpr float ED_W     = W_MM - ED_X - 4.f;  // 87.7mm
    static constexpr float ED_Y     = 16.f;
    static constexpr float ROW_TOP  = 14.f;
    static constexpr float ROW_BOT  = 108.f;
    static constexpr int   N_LANES  = 6;
    // Jack cols: LEN=6, OFF=16, ROT=26, SPR=36
    // Atten cols: LEN=46, OFF=55, ROT=64, SPR=73
    static constexpr float JACK_X[4]  = {6.f, 16.f, 26.f, 36.f};
    static constexpr float ATTEN_X[4] = {46.f, 55.f, 64.f, 73.f};
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
        // Spread params: 6 lanes (18-23)
        SPR_REST, SPR_MELODY, SPR_OCTAVE,
        SPR_LEGATO, SPR_ACCENT, SPR_VARIATION,
        // 24 attenuverters: 6 lanes × 4 params (24-47)
        ATTEN_START,
        NUM_PARAMS = ATTEN_START + 24
    };

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        // 24 mono CV jacks: 6 lanes × 4 params (LEN/OFF/ROT/SPR)
        CV_START = 0,
        NUM_INPUTS = CV_START + 24
    };

    // ── Helpers ───────────────────────────────────────────────────────────
    inline int lenId(int l) { return LEN_REST   + l * 3; }
    inline int offId(int l) { return LEN_REST   + l * 3 + 1; }
    inline int rotId(int l) { return LEN_REST   + l * 3 + 2; }
    inline int sprId(int l) { return SPR_REST   + l; }
    inline int attenId(int lane, int param) { return ATTEN_START + lane*4 + param; }
    inline int cvId   (int lane, int param) { return CV_START    + lane*4 + param; }
    // param: 0=LEN, 1=OFF, 2=ROT, 3=SPR
}

struct MonsoonSandsVisualExpander : Module {
    // Effective spread per lane — written by processDNA at control rate,
    // read by widget at UI rate. Same cross-thread pattern as Rack params.
    // Base spread lives in sprId(lane) params (set by trimpot, never mutated by CV).
    float spreadEffective[6] = {};

    MonsoonSandsVisualExpander() {
        using namespace SandsMonoVisualIds;
        config(NUM_PARAMS, NUM_INPUTS, 0, 0);

        static const char* names[6] = {"REST","MEL","OCT","LEG","ACC","VAR"};
        static const char* pnames[4] = {"Len","Off","Rot","Spread"};
        static const float phi[4]    = {16.f, 0.f, 0.f, 0.f};
        static const float plo[4]    = {1.f,  0.f, 0.f, 0.f};
        static const float phi2[4]   = {16.f,15.f,15.f, 1.f};

        for (int l = 0; l < 6; ++l) {
            configParam(lenId(l), 1.f, 16.f, 16.f,
                        std::string(names[l])+" Length");
            configParam(offId(l), 0.f, 15.f,  0.f,
                        std::string(names[l])+" Offset");
            configParam(rotId(l), 0.f, 15.f,  0.f,
                        std::string(names[l])+" Rotation");
            configParam(sprId(l), 0.f,  1.f,  0.f,
                        std::string(names[l])+" Spread");
            for (int p = 0; p < 4; ++p) {
                configParam(attenId(l, p), -1.f, 1.f, 1.f,
                            std::string(names[l])+" "+pnames[p]+" depth");
                configInput(cvId(l, p),
                            std::string(names[l])+" "+pnames[p]+" CV");
            }
        }
    }
    void process(const ProcessArgs&) override {}
};
