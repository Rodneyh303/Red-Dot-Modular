#include <rack.hpp>
#include "MeloDicer.hpp"
#include <catch.hpp>

using namespace rack;

TEST_CASE("MeloDicer - Live Diced Logic", "[melodicer]") {
    MeloDicer* module = createModule<MeloDicer>();
    module->initialize();

    SECTION("Stability: Diced random buffers stay frozen across cycles") {
        module->rhythmMode = 0; // Dice mode
        module->rhythmSeedPendingFloat = 5.5f;
        module->rhythmSeedPending = true;
        
        // Phrase boundary logic
        module->onPhraseBoundary_(); 
        
        float firstStepVal = module->engine.pe.rhythmRandom[0];
        
        // Simulate phrase wrapping 5 times
        for (int i = 0; i < 5; ++i) {
            module->onPhraseBoundary_();
            // The underlying randomness must not change
            REQUIRE(module->engine.pe.rhythmRandom[0] == firstStepVal);
        }
    }

    SECTION("Live Reactivity: Rest knob filters frozen randomness instantly") {
        // Mock a specific random value for the first step
        module->engine.pe.rhythmRandom[0] = 0.5f;
        
        // Scenario A: Rest Probability is low (0.1). 
        // 0.5 diced value >= 0.1 threshold -> Note plays (cache is true)
        module->params[MeloDicer::REST_PARAM].setValue(0.1f);
        module->engine.pe.refreshVisualCache(module->makePatternInput());
        REQUIRE(module->rhythmPattern[0] == true);
        
        // Scenario B: Rest Probability is high (0.9). 
        // 0.5 diced value < 0.9 threshold -> Step is a rest (cache is false)
        module->params[MeloDicer::REST_PARAM].setValue(0.9f);
        module->engine.pe.refreshVisualCache(module->makePatternInput());
        REQUIRE(module->rhythmPattern[0] == false);
        
        // Verify we can return to Scenario A perfectly
        module->params[MeloDicer::REST_PARAM].setValue(0.1f);
        module->engine.pe.refreshVisualCache(module->makePatternInput());
        REQUIRE(module->rhythmPattern[0] == true);
    }

    SECTION("Melody: Semitone picking reacts live to fader weights") {
        // Set C as the only weight
        for(int i=0; i<12; ++i) module->params[MeloDicer::SEMI0_PARAM + i].setValue(0.0f);
        module->params[MeloDicer::SEMI0_PARAM].setValue(1.0f);
        
        int sem = -1;
        // Diced random value for melody doesn't matter if only one weight exists
        module->engine.pe.melodyRandom[0] = 0.9f; 
        module->genPitchV(sem);
        REQUIRE(sem == 0); // Must be C
        
        // Change weights live to only G
        module->params[MeloDicer::SEMI0_PARAM].setValue(0.0f);
        module->params[MeloDicer::SEMI7_PARAM].setValue(1.0f);
        
        module->genPitchV(sem);
        REQUIRE(sem == 7); // Must be G
    }

    SECTION("Serialization: Patch saving captures the random foundation") {
        module->engine.pe.rhythmRandom[5] = 0.444f;
        json_t* data = module->dataToJson();
        
        module->engine.pe.rhythmRandom[5] = 0.0f; // Clear it
        module->dataFromJson(data);
        REQUIRE(module->engine.pe.rhythmRandom[5] == 0.444f);
        json_decref(data);
    }

    delete module;
}