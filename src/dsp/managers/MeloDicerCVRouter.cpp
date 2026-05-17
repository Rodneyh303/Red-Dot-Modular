#include "MeloDicerCVRouter.hpp"
#include "../../MeloDicer.hpp"

using namespace rack;

// ──── Helper: Quantize to nearest semitone ─────────────────────────────────

float CVRouter::quantizeCV1_(float v) {
    // Quantize to nearest semitone (1V = 12 semitones)
    return std::round(v * 12.f) * (1.f / 12.f);
}

// ──── CV1 Input Processing ──────────────────────────────────────────────────

float CVRouter::processCV1Input(int cv1Mode,
                                 float cv1InputVoltage,
                                 float baseOutputVoltage,
                                 bool cv1Connected) {
    if (!cv1Connected) {
        resetOffsets();
        return baseOutputVoltage;
    }
    
    float v = cv1InputVoltage;
    
    switch (cv1Mode) {
        case 0: // Add Seq — direct addition
            return baseOutputVoltage + v;
            
        case 1: // Transpose Quantized — quantize CV1, add to base
        {
            // Only recompute if CV1 changes significantly
            if (std::abs(v - lastCv1V) > 1e-5f) {
                lastCv1V = v;
                lastCv1Off = quantizeCV1_(v);
            }
            return baseOutputVoltage + lastCv1Off;
        }
        
        case 2: // Mod Lo — transient offset on octave Lo
        {
            cv1LoOffset = clampv<float>(v * 0.25f, -8.f, 8.f);
            return baseOutputVoltage;
        }
        
        case 3: // Mod Hi — transient offset on octave Hi
        {
            cv1HiOffset = clampv<float>(v * 0.25f, -8.f, 8.f);
            return baseOutputVoltage;
        }
        
        default:
            return baseOutputVoltage;
    }
}
