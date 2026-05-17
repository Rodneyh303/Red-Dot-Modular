#pragma once

#include "rack.hpp"

/**
 * CVRouter
 * 
 * Encapsulates CV1 input mode selection and output routing logic.
 * 
 * Centralizes:
 *   1. CV1 input mode processing (Add, Transpose Quantized, Mod Lo, Mod Hi)
 *   2. Transient offset tracking (cv1LoOffset, cv1HiOffset)
 *   3. Output voltage computation based on mode
 *   4. State for last CV1 voltage and quantized offset
 * 
 * Design: CVRouter reads CV1 input and selects/applies the appropriate
 * transformation to compute the final output voltage.
 */
class CVRouter {
public:
    CVRouter() {}
    
    // ──── CV1 Input Processing ──────────────────────────────────────────────
    
    /// Process CV1 input and return output voltage based on current mode
    float processCV1Input(int cv1Mode,
                          float cv1InputVoltage,
                          float baseOutputVoltage,
                          bool cv1Connected);
    
    // ──── Offset Accessors ──────────────────────────────────────────────────
    
    /// Get current Lo octave transient offset
    float getLoOffset() const { return cv1LoOffset; }
    
    /// Get current Hi octave transient offset
    float getHiOffset() const { return cv1HiOffset; }
    
    /// Reset offsets to zero
    void resetOffsets() {
        cv1LoOffset = 0.f;
        cv1HiOffset = 0.f;
        lastCv1V = 0.f;
        lastCv1Off = 0.f;
    }

private:
    // CV1 state
    float cv1LoOffset = 0.f;
    float cv1HiOffset = 0.f;
    float lastCv1V = 0.f;
    float lastCv1Off = 0.f;
    
    // Helper: Quantize CV1 voltage to nearest semitone
    float quantizeCV1_(float v);
};
