#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

namespace SandsMonoVisualIds {

    // ── Panel ────────────────────────────────────────────────────────────
    static constexpr float W_MM     = 213.36f;  // 42HP (was 40HP; +2HP right strip for
                                                 // the per-lane probability CV out column)
    static constexpr float ED_X     = 88.f;
    static constexpr float PROB_OUT_X = 207.f;   // output jack column (new right strip)
    static constexpr float ED_W     = PROB_OUT_X - ED_X - 8.f;  // editor stops left of the outs
    static constexpr float ED_Y     = 16.f;
    static constexpr float ROW_TOP  = 14.f;
    static constexpr float ROW_BOT  = 108.f;
    static constexpr int   N_LANES  = 6;
    static constexpr int   N_SPREAD_LANES = 4;  // REST, MELODY, OCTAVE, ACCENT
    // Spread control index (0..3) → editor lane order (REST/MEL/OCT/LEG/ACC/VAR). Accent is
    // editor lane 4 (it skips LEGATO at 3, which is mono-only and has no spread).
    static constexpr int SPREAD_LANE_TO_EDITOR[4] = { 2, 0, 1, 3 };  // REST=2, MEL=0, OCT=1, ACC=3

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
        // Spread base trimpots: REST/MEL/OCT + ACCENT (now a poly lane) = 4
        SPR_REST, SPR_MELODY, SPR_OCTAVE, SPR_ACCENT,
        // Attenuverters: 18 LOR (6 lanes × 3) + 4 spread = 22
        ATTEN_START,                       // 21 .. 38  (18 LOR attens)
        SPR_ATTEN_START = ATTEN_START + 18, // 39 .. 42  (4 spread attens)
        NUM_PARAMS = SPR_ATTEN_START + 4
    };

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        // 18 LOR CV jacks (6 lanes × 3) + 4 spread CV jacks (REST/MEL/OCT/ACCENT) = 22
        CV_START = 0,                       // 0 .. 17
        SPR_CV_START = CV_START + 18,       // 18 .. 21
        NUM_INPUTS = SPR_CV_START + 4
    };

    // ── Output IDs ────────────────────────────────────────────────────────
    enum OutputId {
        // Per-lane probability CV out (editor lane order REST/MEL/OCT/LEG/ACC/VAR):
        // the final post-everything (A/B mix + spread + LOR) probability the playhead
        // goes over at the current step for that lane. S&H or continuous (menu).
        PROB_OUT_START = 0,                 // 0 .. 5
        NUM_OUTPUTS = PROB_OUT_START + 6
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

    // Probability CV out config (persisted): scale 0=0..1V, 1=0..5V, 2=0..10V;
    // sampleHold true = latch the value at each 16th step start (the decision value),
    // false = continuous (the live modulated surface within the bar).
    float probHeld[6] = {};         // latched per-lane value for S&H mode
    int   probLastStep[6] = {-1,-1,-1,-1,-1,-1};  // last step latched per lane

    MonsoonSandsVisualExpander() {
        using namespace SandsMonoVisualIds;
        config(SandsMonoVisualIds::NUM_PARAMS, SandsMonoVisualIds::NUM_INPUTS,
               SandsMonoVisualIds::NUM_OUTPUTS, 0);
        for (int l = 0; l < 6; ++l)
            configOutput(PROB_OUT_START + l, std::string("Probability ") +
                (const char*[]){"REST","MEL","OCT","LEG","ACC","VAR"}[l]);

        static const char* names[6]  = {"REST","MEL","OCT","LEG","ACC","VAR"};
        static const char* lnames[3] = {"Len","Off","Rot"};

        for (int l = 0; l < 6; ++l) {
            configParam(lenId(l), 1.f, 16.f, 16.f, std::string(names[l])+" Length");
            configParam(offId(l), 0.f, 15.f,  0.f, std::string(names[l])+" Offset");
            configParam(rotId(l), 0.f, 15.f,  0.f, std::string(names[l])+" Rotation");
            for (int p = 0; p < 3; ++p) {  // LEN/OFF/ROT
                configParam(attenId(l, p), -1.f, 1.f, 0.f,
                            std::string(names[l])+" "+lnames[p]+" depth");
                configInput(cvId(l, p),
                            std::string(names[l])+" "+lnames[p]+" CV");
            }
        }
        // Spread group: REST/MEL/OCT/ACCENT (the poly-derived lanes; LEG/VAR are mono-only)
        for (int l = 0; l < N_SPREAD_LANES; ++l) {
            const char* nm = names[SPREAD_LANE_TO_EDITOR[l]];
            configParam(sprId(l), -1.f, 1.f, 0.f, std::string(nm)+" Spread");
            configParam(sprAttenId(l), -1.f, 1.f, 0.f, std::string(nm)+" Spread depth");
            configInput(sprCvId(l), std::string(nm)+" Spread CV");
        }
    }
    void process(const ProcessArgs&) override;   // defined in .cpp (needs calcPlayhead)

    json_t* dataToJson() override {
        json_t* root = json_object();
        return root;
    }
    void dataFromJson(json_t* root) override { (void)root; }
};
