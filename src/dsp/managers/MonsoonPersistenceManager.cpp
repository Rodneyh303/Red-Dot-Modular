#include "MonsoonPersistenceManager.hpp"
#include "../../Monsoon.hpp"
#include <cstdio>
#include <cstdlib>

using namespace rack;

json_t* PersistenceManager::toJson(Monsoon* m) {
    if (!m) return nullptr;

    json_t* root = json_object();

    // ── Settings and Modes ──
    json_object_set_new(root, "cv1Mode", json_integer(m->cv1Mode));
    json_object_set_new(root, "auditionPolicyMode", json_integer(m->auditionPolicyMode));
    json_object_set_new(root, "cv2Mode", json_integer(m->cv2Mode));
    json_object_set_new(root, "gate1Assign", json_integer(m->gate1Assign));
    json_object_set_new(root, "gate2Assign", json_integer(m->gate2Assign));
    json_object_set_new(root, "cv3Target", json_integer(m->cv3Target));
    json_object_set_new(root, "gate3Target", json_integer(m->gate3Target));
    json_object_set_new(root, "rhythmLiveTrial", json_boolean(m->rhythmLiveTrial));
    json_object_set_new(root, "melodyLiveTrial", json_boolean(m->melodyLiveTrial));
    json_object_set_new(root, "invertMuteLogic", json_boolean(m->invertMuteLogic));
    json_object_set_new(root, "spreadInterpMono", json_boolean(m->spreadInterpMono));
    json_object_set_new(root, "modVizMonsoonMelody", json_boolean(m->modVizMonsoonMelody));
    json_object_set_new(root, "modVizMonsoonOther",  json_boolean(m->modVizMonsoonOther));
    json_object_set_new(root, "modVizEast",  json_boolean(m->modVizEast));
    json_object_set_new(root, "modVizWest",  json_boolean(m->modVizWest));
    json_object_set_new(root, "modVizMacro", json_boolean(m->modVizMacro));
    json_object_set_new(root, "modVizMono",  json_boolean(m->modVizMono));
    json_object_set_new(root, "restartOnUnmute", json_boolean(m->restartOnUnmute));
    json_object_set_new(root, "reseedOnRoll", json_boolean(m->reseedOnRoll));
    json_object_set_new(root, "reseedOnRestart", json_boolean(m->reseedOnRestart));
    json_object_set_new(root, "noteVariationMask", json_integer(m->noteVariationMask));
    json_object_set_new(root, "ppqnSetting", json_integer(m->ppqnSetting));
    json_object_set_new(root, "modeSelect", json_integer(m->modeSelect));
    json_object_set_new(root, "lightTheme", json_boolean(m->lightTheme));
    
    if (m->scaleManager) {
        json_object_set_new(root, "scaleRoot", json_integer(m->scaleManager->scaleRoot));
        json_object_set_new(root, "lastSelectedScale", json_integer(m->scaleManager->lastSelectedScale));
        json_object_set_new(root, "lockScaleNotes", json_boolean(m->scaleManager->lockScaleNotes));
    }

    json_object_set_new(root, "locked", json_boolean(m->locked));
    json_object_set_new(root, "muted", json_boolean(m->muted));

    // ── Engine State ──
    json_object_set_new(root, "rhythmMode", json_integer(m->rhythmMode));
    json_object_set_new(root, "melodyMode", json_integer(m->melodyMode));
    json_object_set_new(root, "startStep", json_integer(m->startStep));
    json_object_set_new(root, "endStep", json_integer(m->endStep));
    json_object_set_new(root, "dnaLength", json_integer(m->engine.dnaLength));
    json_object_set_new(root, "dnaOffset", json_integer(m->engine.dnaOffset));
    json_object_set_new(root, "numPolyVoices", json_integer(m->engine.numPolyVoices));

    // ── Seeds ──
    json_object_set_new(root, "rhythmSeedFloat", json_real(m->rhythmSeedFloat));
    json_object_set_new(root, "melodySeedFloat", json_real(m->melodySeedFloat));
    json_object_set_new(root, "rhythmSeedPending", json_boolean(m->rhythmSeedPending));
    json_object_set_new(root, "rhythmSeedPendingFloat", json_real(m->rhythmSeedPendingFloat));
    json_object_set_new(root, "melodySeedPending", json_boolean(m->melodySeedPending));
    json_object_set_new(root, "melodySeedPendingFloat", json_real(m->melodySeedPendingFloat));

    // ── DNA Random Buffers (Pattern Stability) ──
    json_t* rrarr = json_array();
    json_t* vrarr = json_array();
    json_t* lrarr = json_array();
    json_t* mrarr = json_array();
    json_t* orarr = json_array();
    json_t* ararr = json_array();
    for (int i = 0; i < 16; ++i) {
        json_array_append_new(rrarr, json_real(m->engine.pe.rhythmRandom[i]));
        json_array_append_new(vrarr, json_real(m->engine.pe.variationRandom[i]));
        json_array_append_new(lrarr, json_real(m->engine.pe.legatoRandom[i]));
        json_array_append_new(mrarr, json_real(m->engine.pe.melodyRandom[i]));
        json_array_append_new(orarr, json_real(m->engine.pe.octaveRandom[i]));
        json_array_append_new(ararr, json_real(m->engine.pe.accentRandom[i]));
    }
    json_object_set_new(root, "rhythmRandom", rrarr);
    json_object_set_new(root, "variationRandom", vrarr);
    json_object_set_new(root, "legatoRandom", lrarr);
    json_object_set_new(root, "melodyRandom", mrarr);
    json_object_set_new(root, "octaveRandom", orarr);
    json_object_set_new(root, "accentRandom", ararr);

    // ── Poly DNA Random Buffers ──
    json_t* prarr = json_array();
    json_t* pmarr = json_array();
    json_t* poarr = json_array();
    for (int v = 0; v < 15; v++) {
        for (int i = 0; i < 16; i++) {
            json_array_append_new(prarr, json_real(m->engine.pe.polyRhythmRandom[v][i]));
            json_array_append_new(pmarr, json_real(m->engine.pe.polyMelodyRandom[v][i]));
            json_array_append_new(poarr, json_real(m->engine.pe.polyOctaveRandom[v][i]));
        }
    }
    json_object_set_new(root, "polyRhythmRandom", prarr);
    json_object_set_new(root, "polyMelodyRandom", pmarr);
    json_object_set_new(root, "polyOctaveRandom", poarr);

    // ── Playable slew: locked (A) + candidate (B) endpoints + latched slew ──
    {
        auto saveArr = [&](const char* key, const float* a){
            json_t* j=json_array();
            for (int i=0;i<16;i++) json_array_append_new(j, json_real(a[i]));
            json_object_set_new(root, key, j);
        };
        auto savePoly = [&](const char* key, const float a[15][16]){
            json_t* j=json_array();
            for (int v=0;v<15;v++) for (int i=0;i<16;i++) json_array_append_new(j, json_real(a[v][i]));
            json_object_set_new(root, key, j);
        };
        saveArr("slA_rhythm", m->engine.pe.rhythmLockedA);   saveArr("slB_rhythm", m->engine.pe.rhythmCandB);
        saveArr("slA_variation", m->engine.pe.variationLockedA); saveArr("slB_variation", m->engine.pe.variationCandB);
        saveArr("slA_legato", m->engine.pe.legatoLockedA);   saveArr("slB_legato", m->engine.pe.legatoCandB);
        saveArr("slA_accent", m->engine.pe.accentLockedA);   saveArr("slB_accent", m->engine.pe.accentCandB);
        saveArr("slA_melody", m->engine.pe.melodyLockedA);   saveArr("slB_melody", m->engine.pe.melodyCandB);
        saveArr("slA_octave", m->engine.pe.octaveLockedA);   saveArr("slB_octave", m->engine.pe.octaveCandB);
        savePoly("slA_polyRhythm", m->engine.pe.polyRhythmLockedA); savePoly("slB_polyRhythm", m->engine.pe.polyRhythmCandB);
        savePoly("slA_polyMelody", m->engine.pe.polyMelodyLockedA); savePoly("slB_polyMelody", m->engine.pe.polyMelodyCandB);
        savePoly("slA_polyOctave", m->engine.pe.polyOctaveLockedA); savePoly("slB_polyOctave", m->engine.pe.polyOctaveCandB);
        json_object_set_new(root, "slLatchedR", json_real(m->engine.pe.rhythmSlewLatched));
        json_object_set_new(root, "slLatchedM", json_real(m->engine.pe.melodySlewLatched));
        json_object_set_new(root, "slFirstR", json_boolean(m->engine.pe.rhythmFirstDraw));
        json_object_set_new(root, "slFirstM", json_boolean(m->engine.pe.melodyFirstDraw));
        // RNG stream state (Xoroshiro128+ has two uint64 words). Saved as strings
        // to preserve full 64-bit precision (JSON reals can't). Restoring this
        // makes reload truly lossless: the NEXT roll continues the same stream it
        // would have, not a fresh one.
        auto saveU64 = [&](const char* key, uint64_t v){
            char buf[32]; snprintf(buf, sizeof(buf), "%llu", (unsigned long long)v);
            json_object_set_new(root, key, json_string(buf));
        };
        saveU64("rngR0", m->engine.pe.rhythmRng.state[0]);
        saveU64("rngR1", m->engine.pe.rhythmRng.state[1]);
        saveU64("rngM0", m->engine.pe.melodyRng.state[0]);
        saveU64("rngM1", m->engine.pe.melodyRng.state[1]);
    }

    // ── Rhythm and Pitch Arrays ──
    json_t* rarr = json_array();
    json_t* marr = json_array();
    for (int i = 0; i < 16; ++i) {
        json_array_append_new(rarr, json_integer(m->rhythmPattern[i] ? 1 : 0));
        json_array_append_new(marr, json_real(m->melodyPitchV[i]));
    }
    json_object_set_new(root, "rhythmPattern", rarr);
    json_object_set_new(root, "melodyPitchV", marr);

    return root;
}

void PersistenceManager::fromJson(Monsoon* m, json_t* root) {
    if (!m || !root) return;

    // ── Settings and Modes ──
    if (auto j = json_object_get(root, "cv1Mode")) m->cv1Mode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "auditionPolicyMode")) m->auditionPolicyMode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "cv2Mode")) m->cv2Mode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate1Assign")) m->gate1Assign = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate2Assign")) m->gate2Assign = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "cv3Target")) m->cv3Target = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate3Target")) m->gate3Target = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "rhythmLiveTrial")) m->rhythmLiveTrial = json_boolean_value(j);
    if (auto j = json_object_get(root, "melodyLiveTrial")) m->melodyLiveTrial = json_boolean_value(j);
    if (auto j = json_object_get(root, "invertMuteLogic")) m->invertMuteLogic = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "spreadInterpMono")) m->spreadInterpMono = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "modVizMonsoonMelody")) m->modVizMonsoonMelody = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "modVizMonsoonOther"))  m->modVizMonsoonOther  = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "modVizEast"))  m->modVizEast  = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "modVizWest"))  m->modVizWest  = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "modVizMacro")) m->modVizMacro = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "modVizMono"))  m->modVizMono  = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "restartOnUnmute")) m->restartOnUnmute = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "reseedOnRoll")) m->reseedOnRoll = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "reseedOnRestart")) m->reseedOnRestart = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "noteVariationMask")) m->noteVariationMask = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "ppqnSetting")) {
        int v = (int)json_integer_value(j);
        // Migrate legacy values (1/4/24 = old input-divider semantics) onto the
        // new master grid (24/48/96). Anything below 24 → 24 (the new minimum,
        // which already resolves every note value).
        if (v != 24 && v != 48 && v != 96) v = 24;
        m->ppqnSetting = v;
    }
    if (auto j = json_object_get(root, "modeSelect")) m->modeSelect = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "lightTheme")) m->lightTheme = (bool)json_boolean_value(j);
    if (m->scaleManager) {
        if (auto j = json_object_get(root, "scaleRoot")) m->scaleManager->scaleRoot = (int)json_integer_value(j);
        if (auto j = json_object_get(root, "lastSelectedScale")) m->scaleManager->lastSelectedScale = (int)json_integer_value(j);
        if (auto j = json_object_get(root, "lockScaleNotes")) m->scaleManager->lockScaleNotes = (bool)json_boolean_value(j);
    }
    if (auto j = json_object_get(root, "locked")) m->locked = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "muted")) m->muted = (bool)json_boolean_value(j);

    // ── Engine State ──
    if (auto j = json_object_get(root, "rhythmMode")) m->rhythmMode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "melodyMode")) m->melodyMode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "startStep")) m->startStep = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "endStep")) m->endStep = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "dnaLength")) m->engine.dnaLength = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "dnaOffset")) m->engine.dnaOffset = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "numPolyVoices")) m->engine.numPolyVoices = pe_clamp((int)json_integer_value(j), 0, 15);

    // ── Seeds ──
    if (auto j = json_object_get(root, "rhythmSeedFloat")) m->rhythmSeedFloat = (float)json_real_value(j);
    if (auto j = json_object_get(root, "melodySeedFloat")) m->melodySeedFloat = (float)json_real_value(j);
    if (auto j = json_object_get(root, "rhythmSeedPending")) m->rhythmSeedPending = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "rhythmSeedPendingFloat")) m->rhythmSeedPendingFloat = (float)json_real_value(j);
    if (auto j = json_object_get(root, "melodySeedPending")) m->melodySeedPending = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "melodySeedPendingFloat")) m->melodySeedPendingFloat = (float)json_real_value(j);

    // ── DNA Random Buffers ──
    auto loadArr = [&](const char* name, float* target) {
        if (auto j = json_object_get(root, name)) {
            if (json_is_array(j)) {
                for (size_t i = 0; i < 16 && i < json_array_size(j); ++i)
                    target[i] = (float)json_real_value(json_array_get(j, i));
            }
        }
    };
    loadArr("rhythmRandom", m->engine.pe.rhythmRandom);
    loadArr("variationRandom", m->engine.pe.variationRandom);
    loadArr("legatoRandom", m->engine.pe.legatoRandom);
    loadArr("melodyRandom", m->engine.pe.melodyRandom);
    loadArr("octaveRandom", m->engine.pe.octaveRandom);
    loadArr("accentRandom", m->engine.pe.accentRandom);

    // ── Poly DNA Random Buffers ──
    auto loadPolyArr = [&](const char* name, float target[15][16], float source[15][16]) {
        if (auto j = json_object_get(root, name)) {
            if (json_is_array(j)) {
                for (int v = 0; v < 15; v++) {
                    for (int i = 0; i < 16; i++) {
                        json_t* val = json_array_get(j, v * 16 + i);
                        if (val) {
                            target[v][i] = (float)json_real_value(val);
                            source[v][i] = target[v][i];
                        }
                    }
                }
            }
        }
    };
    loadPolyArr("polyRhythmRandom", m->engine.pe.polyRhythmRandom, m->engine.pe.polyRhythmSource);
    loadPolyArr("polyMelodyRandom", m->engine.pe.polyMelodyRandom, m->engine.pe.polyMelodySource);
    loadPolyArr("polyOctaveRandom", m->engine.pe.polyOctaveRandom, m->engine.pe.polyOctaveSource);

    // ── Playable slew: restore A/B endpoints + latched slew, recompute effective ─
    {
        bool hasSlew = json_object_get(root, "slA_rhythm") != nullptr;
        auto loadA = [&](const char* name, float* t){
            if (auto j=json_object_get(root,name)) if (json_is_array(j))
                for (size_t i=0;i<16 && i<json_array_size(j);++i) t[i]=(float)json_real_value(json_array_get(j,i));
        };
        auto loadP = [&](const char* name, float t[15][16]){
            if (auto j=json_object_get(root,name)) if (json_is_array(j))
                for (int v=0;v<15;v++) for (int i=0;i<16;i++){ json_t* val=json_array_get(j,v*16+i); if(val) t[v][i]=(float)json_real_value(val); }
        };
        if (hasSlew) {
            loadA("slA_rhythm",m->engine.pe.rhythmLockedA);     loadA("slB_rhythm",m->engine.pe.rhythmCandB);
            loadA("slA_variation",m->engine.pe.variationLockedA);loadA("slB_variation",m->engine.pe.variationCandB);
            loadA("slA_legato",m->engine.pe.legatoLockedA);     loadA("slB_legato",m->engine.pe.legatoCandB);
            loadA("slA_accent",m->engine.pe.accentLockedA);     loadA("slB_accent",m->engine.pe.accentCandB);
            loadA("slA_melody",m->engine.pe.melodyLockedA);     loadA("slB_melody",m->engine.pe.melodyCandB);
            loadA("slA_octave",m->engine.pe.octaveLockedA);     loadA("slB_octave",m->engine.pe.octaveCandB);
            loadP("slA_polyRhythm",m->engine.pe.polyRhythmLockedA); loadP("slB_polyRhythm",m->engine.pe.polyRhythmCandB);
            loadP("slA_polyMelody",m->engine.pe.polyMelodyLockedA); loadP("slB_polyMelody",m->engine.pe.polyMelodyCandB);
            loadP("slA_polyOctave",m->engine.pe.polyOctaveLockedA); loadP("slB_polyOctave",m->engine.pe.polyOctaveCandB);
            if (auto j=json_object_get(root,"slLatchedR")) m->engine.pe.rhythmSlewLatched=(float)json_real_value(j);
            if (auto j=json_object_get(root,"slLatchedM")) m->engine.pe.melodySlewLatched=(float)json_real_value(j);
            m->engine.pe.rhythmFirstDraw = false; m->engine.pe.melodyFirstDraw = false;
            if (auto j=json_object_get(root,"slFirstR")) m->engine.pe.rhythmFirstDraw=(bool)json_boolean_value(j);
            if (auto j=json_object_get(root,"slFirstM")) m->engine.pe.melodyFirstDraw=(bool)json_boolean_value(j);
            m->engine.pe.rhythmSlewApplied = -1.f; m->engine.pe.melodySlewApplied = -1.f;
            m->engine.pe.recomputeEffectiveRhythm();
            m->engine.pe.recomputeEffectiveMelody();
            // Restore RNG stream state (full 64-bit, saved as strings). Only if
            // present and non-degenerate; an all-zero state would freeze the RNG.
            auto loadU64 = [&](const char* key, uint64_t& out)->bool{
                if (auto j = json_object_get(root, key)) {
                    if (json_is_string(j)) { out = strtoull(json_string_value(j), nullptr, 10); return true; }
                }
                return false;
            };
            uint64_t r0=0,r1=0,m0=0,m1=0;
            bool haveR = loadU64("rngR0", r0) & loadU64("rngR1", r1);
            bool haveM = loadU64("rngM0", m0) & loadU64("rngM1", m1);
            if (haveR && !(r0==0 && r1==0)) { m->engine.pe.rhythmRng.state[0]=r0; m->engine.pe.rhythmRng.state[1]=r1; }
            if (haveM && !(m0==0 && m1==0)) { m->engine.pe.melodyRng.state[0]=m0; m->engine.pe.melodyRng.state[1]=m1; }
        } else {
            // Old patch: no A/B saved. Seed both endpoints from the loaded
            // effective arrays so the groove is preserved and slew is a no-op
            // until the next reroll. Latched slew defaults to 1 (full).
            auto seed=[&](float* a,float* b,const float* eff){ for(int i=0;i<16;i++){a[i]=eff[i];b[i]=eff[i];} };
            seed(m->engine.pe.rhythmLockedA,m->engine.pe.rhythmCandB,m->engine.pe.rhythmRandom);
            seed(m->engine.pe.variationLockedA,m->engine.pe.variationCandB,m->engine.pe.variationRandom);
            seed(m->engine.pe.legatoLockedA,m->engine.pe.legatoCandB,m->engine.pe.legatoRandom);
            seed(m->engine.pe.accentLockedA,m->engine.pe.accentCandB,m->engine.pe.accentRandom);
            seed(m->engine.pe.melodyLockedA,m->engine.pe.melodyCandB,m->engine.pe.melodyRandom);
            seed(m->engine.pe.octaveLockedA,m->engine.pe.octaveCandB,m->engine.pe.octaveRandom);
            for(int v=0;v<15;v++){
                for(int i=0;i<16;i++){
                    m->engine.pe.polyRhythmLockedA[v][i]=m->engine.pe.polyRhythmCandB[v][i]=m->engine.pe.polyRhythmRandom[v][i];
                    m->engine.pe.polyMelodyLockedA[v][i]=m->engine.pe.polyMelodyCandB[v][i]=m->engine.pe.polyMelodyRandom[v][i];
                    m->engine.pe.polyOctaveLockedA[v][i]=m->engine.pe.polyOctaveCandB[v][i]=m->engine.pe.polyOctaveRandom[v][i];
                }
            }
            m->engine.pe.rhythmFirstDraw = false; m->engine.pe.melodyFirstDraw = false;
        }
    }

    // ── Rhythm and Pitch Arrays ──
    if (auto j = json_object_get(root, "rhythmPattern")) {
        if (json_is_array(j)) {
            for (size_t i = 0; i < 16 && i < json_array_size(j); ++i)
                m->rhythmPattern[i] = (json_integer_value(json_array_get(j, i)) != 0);
        }
    }
    if (auto j = json_object_get(root, "melodyPitchV")) {
        if (json_is_array(j)) {
            for (size_t i = 0; i < 16 && i < json_array_size(j); ++i)
                m->melodyPitchV[i] = (float)json_real_value(json_array_get(j, i));
        }
    }
}