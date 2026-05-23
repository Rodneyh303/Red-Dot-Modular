#include "MonsoonPersistenceManager.hpp"
#include "../../Monsoon.hpp"

using namespace rack;

json_t* PersistenceManager::toJson(Monsoon* m) {
    if (!m) return nullptr;

    json_t* root = json_object();

    // ── Settings and Modes ──
    json_object_set_new(root, "cv1Mode", json_integer(m->cv1Mode));
    json_object_set_new(root, "cv2Mode", json_integer(m->cv2Mode));
    json_object_set_new(root, "gate1Assign", json_integer(m->gate1Assign));
    json_object_set_new(root, "gate2Assign", json_integer(m->gate2Assign));
    json_object_set_new(root, "invertMuteLogic", json_boolean(m->invertMuteLogic));
    json_object_set_new(root, "restartOnUnmute", json_boolean(m->restartOnUnmute));
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
    if (auto j = json_object_get(root, "cv2Mode")) m->cv2Mode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate1Assign")) m->gate1Assign = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate2Assign")) m->gate2Assign = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "invertMuteLogic")) m->invertMuteLogic = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "restartOnUnmute")) m->restartOnUnmute = (bool)json_boolean_value(j);
    if (auto j = json_object_get(root, "noteVariationMask")) m->noteVariationMask = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "ppqnSetting")) m->ppqnSetting = (int)json_integer_value(j);
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