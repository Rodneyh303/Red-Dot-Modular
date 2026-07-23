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
    json_object_set_new(root, "rhythmReversibleMode", json_integer(m->rhythmReversibleMode));
    json_object_set_new(root, "melodyReversibleMode", json_integer(m->melodyReversibleMode));
    json_object_set_new(root, "reseedOnModeChange", json_integer(m->reseedOnModeChange));
    json_object_set_new(root, "probOutScale", json_integer(m->probOutScale));
    json_object_set_new(root, "probOutSampleHold", json_boolean(m->probOutSampleHold));
    json_object_set_new(root, "resetIndexOnModeChange", json_integer(m->resetIndexOnModeChange));
    json_object_set_new(root, "cv2Mode", json_integer(m->cv2Mode));
    json_object_set_new(root, "gate1Assign", json_integer(m->gate1Assign));
    json_object_set_new(root, "gate2Assign", json_integer(m->gate2Assign));
    json_object_set_new(root, "cv3Target", json_integer(m->cv3Target));
    json_object_set_new(root, "gate3Target", json_integer(m->gate3Target));
    json_object_set_new(root, "rhythmLiveTrial", json_boolean(m->rhythmLiveTrial));
    json_object_set_new(root, "melodyLiveTrial", json_boolean(m->melodyLiveTrial));
    json_object_set_new(root, "invertMuteLogic", json_boolean(m->invertMuteLogic));
    json_object_set_new(root, "modVizMonsoonMelody", json_boolean(m->modVizMonsoonMelody));
    json_object_set_new(root, "restBeatsLegato",  json_boolean(m->engine.restBeatsLegato));
    json_object_set_new(root, "boundaryInterrupt", json_boolean(m->engine.boundaryInterrupt));
    json_object_set_new(root, "perVoiceArticulation", json_boolean(m->engine.perVoiceArticulation));
    // Per-lane direction: the 4-state LaneDir enum per strand (Forward/Reverse/Pendulum/PingPong).
    // Also saves the legacy sign + pendulum fields for backward compat with older patches.
    // Step 3: laneDirV is NOT persisted here any more — East's direction bank is poly
    // direction's home and Rack persists it as module params. Two persisted homes for one
    // datum is the bug this arc kept hitting.
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
            json_array_append_new(prarr, json_real(m->engine.pe.polyRandom(v, SequencerEngine::PL_REST)[i]));
            json_array_append_new(pmarr, json_real(m->engine.pe.polyRandom(v, SequencerEngine::PL_MELODY)[i]));
            json_array_append_new(poarr, json_real(m->engine.pe.polyRandom(v, SequencerEngine::PL_OCTAVE)[i]));
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
        saveArr("slA_rhythm", m->engine.pe.rhythmLockedA);
        saveArr("slA_variation", m->engine.pe.variationLockedA);
        saveArr("slA_legato", m->engine.pe.legatoLockedA);
        saveArr("slA_accent", m->engine.pe.accentLockedA);
        saveArr("slA_melody", m->engine.pe.melodyLockedA);
        saveArr("slA_octave", m->engine.pe.octaveLockedA);
        savePoly("slA_polyRhythm", m->engine.pe.polyRhythmLockedA);
        savePoly("slA_polyMelody", m->engine.pe.polyMelodyLockedA);
        savePoly("slA_polyOctave", m->engine.pe.polyOctaveLockedA);
        savePoly("slA_polyAccent", m->engine.pe.polyAccentLockedA);  // accent poly lane: irreducible A (B regenerated)
        json_object_set_new(root, "slLatchedR", json_real(m->engine.pe.rhythmSlewLatched));
        json_object_set_new(root, "slLatchedM", json_real(m->engine.pe.melodySlewLatched));
        json_object_set_new(root, "slFirstR", json_boolean(m->engine.pe.rhythmFirstDraw));
        json_object_set_new(root, "slFirstM", json_boolean(m->engine.pe.melodyFirstDraw));
        // Philox regeneration state: draw counter (stream position) + A<->B mix. With the
        // seed key (rhythm/melodySeedFloat), the committed A arrays above, and the latched
        // slew, this is the MINIMUM complete set to regenerate candidate B exactly on load
        // (see PatternEngine::regenerate*B). drawCtr is the addressable Philox index; saved
        // as string to preserve the full signed 64-bit range (can go negative under reverse).
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", (long long)m->engine.pe.rhythmDrawCtr);
            json_object_set_new(root, "drawCtrR", json_string(buf));
            snprintf(buf, sizeof(buf), "%lld", (long long)m->engine.pe.melodyDrawCtr);
            json_object_set_new(root, "drawCtrM", json_string(buf));
        }
        json_object_set_new(root, "mixLatchedR", json_real(m->engine.pe.rhythmMixLatched));
        json_object_set_new(root, "mixLatchedM", json_real(m->engine.pe.melodyMixLatched));
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

    // EditorState.laneDir (migrated out of params[]; params were auto-saved, fields are not).
    // See NUM_PARAMS_MIGRATION.md. 96 floats = 16 voice-bank slots x 6 lanes.
    json_t* ld = json_array();
    for (int i = 0; i < 96; ++i) json_array_append_new(ld, json_real(m->editor.laneDir[i]));
    json_object_set_new(root, "editorLaneDir", ld);

    // VARLEG deleg (30) + atten (96), migrated out of params[] (NUM_PARAMS_MIGRATION.md).
    json_t* vd = json_array();
    for (int i = 0; i < 30; ++i) json_array_append_new(vd, json_real(m->editor.varlegDeleg[i]));
    json_object_set_new(root, "editorVarlegDeleg", vd);
    json_t* va = json_array();
    for (int i = 0; i < 96; ++i) json_array_append_new(va, json_real(m->editor.varlegAtten[i]));
    json_object_set_new(root, "editorVarlegAtten", va);

    // MACRO owner (64) + send (256) + atten (256), migrated out of params[]. Tap re-homed to
    // an expander param, so it persists via the normal Rack param path -- not here.
    json_t* mo = json_array();
    for (int i = 0; i < 64; ++i) json_array_append_new(mo, json_real(m->editor.macroOwn[i]));
    json_object_set_new(root, "editorMacroOwn", mo);
    json_t* ms = json_array();
    for (int i = 0; i < 256; ++i) json_array_append_new(ms, json_real(m->editor.macroSend[i]));
    json_object_set_new(root, "editorMacroSend", ms);
    json_t* ma = json_array();
    for (int i = 0; i < 256; ++i) json_array_append_new(ma, json_real(m->editor.macroAtten[i]));
    json_object_set_new(root, "editorMacroAtten", ma);

    // V1 (East-alone) LOR/spread backup + its written-once guard.
    json_t* lb = json_array();
    for (int i = 0; i < 288; ++i) json_array_append_new(lb, json_real(m->editor.lorBase[i]));
    json_object_set_new(root, "editorLorBase", lb);
    json_t* sp = json_array();
    for (int i = 0; i < 64; ++i) json_array_append_new(sp, json_real(m->editor.spread[i]));
    json_object_set_new(root, "editorSpread", sp);

    // GLOBAL slice (MVC step 1). Macro's globals used to be params, which gave save/restore
    // for free; now they are store fields and must be persisted explicitly.
    auto saveArr = [&](const char* key, const float* a, int n) {
        json_t* j = json_array();
        for (int i = 0; i < n; ++i) json_array_append_new(j, json_real(a[i]));
        json_object_set_new(root, key, j);
    };
    saveArr("editorGlobalLor",    m->editor.globalLor,    12);
    saveArr("editorGlobalSpread", m->editor.globalSpread,  4);
    saveArr("editorGlobalAtten",  m->editor.globalAtten,  16);
    saveArr("editorGlobalTap",    m->editor.globalTap,     8);
    saveArr("editorGlobalDir",    m->editor.globalDir,     4);

    return root;
}

void PersistenceManager::fromJson(Monsoon* m, json_t* root) {
    if (!m || !root) return;

    // ── Settings and Modes ──
    if (auto j = json_object_get(root, "cv1Mode")) m->cv1Mode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "rhythmReversibleMode")) m->rhythmReversibleMode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "melodyReversibleMode")) m->melodyReversibleMode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "reseedOnModeChange")) m->reseedOnModeChange = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "probOutScale")) m->probOutScale = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "probOutSampleHold")) m->probOutSampleHold = json_boolean_value(j);
    if (auto j = json_object_get(root, "restBeatsLegato"))   m->engine.restBeatsLegato   = json_boolean_value(j);
    if (auto j = json_object_get(root, "boundaryInterrupt"))  m->engine.boundaryInterrupt  = json_boolean_value(j);
    if (auto j = json_object_get(root, "perVoiceArticulation")) m->engine.perVoiceArticulation = json_boolean_value(j);
    // Step 4: laneDir and laneDirV are NOT restored here any more. Direction is now
    // expander-homed (Mono's dirDispId for mono, East's dirId/monoDirId bank for poly).
    // Rack restores the expander params, then MonsoonExpanderManager::sync() pushes them
    // to the engine. No legacy fallback needed (pre-release, no patches to preserve).
    // Step 3: laneDirV is NOT restored here any more — East's direction bank is poly
    // direction's home and Rack restores it as module params, after which
    // MonsoonExpanderManager::sync() pushes it into the engine (which is a derived cache).
    // With no East in the rack, sync() resets poly direction to Forward, so it no longer
    // outlives its editor.
    if (auto j = json_object_get(root, "laneFlipQuant"))
        m->engine.laneFlipQuant = (json_integer_value(j) == 1) ? SequencerEngine::LaneFlipQuant::Phrase
                                                               : SequencerEngine::LaneFlipQuant::StepEdge;
    if (auto j = json_object_get(root, "resetIndexOnModeChange")) m->resetIndexOnModeChange = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "cv2Mode")) m->cv2Mode = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate1Assign")) m->gate1Assign = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate2Assign")) m->gate2Assign = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "cv3Target")) m->cv3Target = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "gate3Target")) m->gate3Target = (int)json_integer_value(j);
    if (auto j = json_object_get(root, "rhythmLiveTrial")) m->rhythmLiveTrial = json_boolean_value(j);
    if (auto j = json_object_get(root, "melodyLiveTrial")) m->melodyLiveTrial = json_boolean_value(j);
    if (auto j = json_object_get(root, "invertMuteLogic")) m->invertMuteLogic = (bool)json_boolean_value(j);
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
    // target is the unified random_ store (written per-element via polyRandom, since random_[1..15]
    // isn't a contiguous [15][16] block); source is still a plain [15][16] slew-cache array.
    auto loadPolyArr = [&](const char* name, int engLane, float source[15][16]) {
        if (auto j = json_object_get(root, name)) {
            if (json_is_array(j)) {
                for (int v = 0; v < 15; v++) {
                    for (int i = 0; i < 16; i++) {
                        json_t* val = json_array_get(j, v * 16 + i);
                        if (val) {
                            float x = (float)json_real_value(val);
                            m->engine.pe.polyRandom(v, engLane)[i] = x;
                            source[v][i] = x;
                        }
                    }
                }
            }
        }
    };
    loadPolyArr("polyRhythmRandom", SequencerEngine::PL_REST,   m->engine.pe.polyRhythmSource);
    loadPolyArr("polyMelodyRandom", SequencerEngine::PL_MELODY, m->engine.pe.polyMelodySource);
    loadPolyArr("polyOctaveRandom", SequencerEngine::PL_OCTAVE, m->engine.pe.polyOctaveSource);

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
            loadA("slA_rhythm",m->engine.pe.rhythmLockedA);
            loadA("slA_variation",m->engine.pe.variationLockedA);
            loadA("slA_legato",m->engine.pe.legatoLockedA);
            loadA("slA_accent",m->engine.pe.accentLockedA);
            loadA("slA_melody",m->engine.pe.melodyLockedA);
            loadA("slA_octave",m->engine.pe.octaveLockedA);
            loadP("slA_polyRhythm",m->engine.pe.polyRhythmLockedA);
            loadP("slA_polyMelody",m->engine.pe.polyMelodyLockedA);
            loadP("slA_polyOctave",m->engine.pe.polyOctaveLockedA);
            loadP("slA_polyAccent",m->engine.pe.polyAccentLockedA);
            if (auto j=json_object_get(root,"slLatchedR")) m->engine.pe.rhythmSlewLatched=(float)json_real_value(j);
            if (auto j=json_object_get(root,"slLatchedM")) m->engine.pe.melodySlewLatched=(float)json_real_value(j);
            if (auto j=json_object_get(root,"mixLatchedR")) m->engine.pe.rhythmMixLatched=(float)json_real_value(j);
            if (auto j=json_object_get(root,"mixLatchedM")) m->engine.pe.melodyMixLatched=(float)json_real_value(j);
            m->engine.pe.rhythmFirstDraw = false; m->engine.pe.melodyFirstDraw = false;
            if (auto j=json_object_get(root,"slFirstR")) m->engine.pe.rhythmFirstDraw=(bool)json_boolean_value(j);
            if (auto j=json_object_get(root,"slFirstM")) m->engine.pe.melodyFirstDraw=(bool)json_boolean_value(j);
            // Restore the Philox draw counter (stream position). B is NOT restored here —
            // it is REGENERATED in Monsoon::fromJson's finalize, AFTER Philox is seeded
            // (seeding zeroes the counter, so the finalize re-applies it then regenerates).
            if (auto j=json_object_get(root,"drawCtrR")) m->engine.pe.rhythmDrawCtr=(int64_t)strtoll(json_string_value(j),nullptr,10);
            if (auto j=json_object_get(root,"drawCtrM")) m->engine.pe.melodyDrawCtr=(int64_t)strtoll(json_string_value(j),nullptr,10);
            m->engine.pe.rhythmSlewApplied = -1.f; m->engine.pe.melodySlewApplied = -1.f;
            m->pendingRegenB = true;   // tell the finalize to regenerate B post-seed
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
                    m->engine.pe.polyRhythmLockedA[v][i]=m->engine.pe.polyRhythmCandB[v][i]=m->engine.pe.polyRandom(v, SequencerEngine::PL_REST)[i];
                    m->engine.pe.polyMelodyLockedA[v][i]=m->engine.pe.polyMelodyCandB[v][i]=m->engine.pe.polyRandom(v, SequencerEngine::PL_MELODY)[i];
                    m->engine.pe.polyOctaveLockedA[v][i]=m->engine.pe.polyOctaveCandB[v][i]=m->engine.pe.polyRandom(v, SequencerEngine::PL_OCTAVE)[i];
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

    // EditorState.laneDir (NUM_PARAMS_MIGRATION.md). Missing key = pre-migration patch ->
    // fields keep their Forward (0) defaults, which matches the old default param value.
    if (auto j = json_object_get(root, "editorLaneDir")) {
        if (json_is_array(j)) {
            for (size_t i = 0; i < 96 && i < json_array_size(j); ++i) {
                float v = (float)json_real_value(json_array_get(j, i));
                m->editor.laneDir[i] = math::clamp(v, 0.f, 3.f);
            }
        }
    }
    if (auto j = json_object_get(root, "editorVarlegDeleg")) {
        if (json_is_array(j))
            for (size_t i = 0; i < 30 && i < json_array_size(j); ++i)
                m->editor.varlegDeleg[i] = (float)json_real_value(json_array_get(j, i));
    }
    if (auto j = json_object_get(root, "editorVarlegAtten")) {
        if (json_is_array(j))
            for (size_t i = 0; i < 96 && i < json_array_size(j); ++i)
                m->editor.varlegAtten[i] = (float)json_real_value(json_array_get(j, i));
    }
    if (auto j = json_object_get(root, "editorMacroOwn")) {
        if (json_is_array(j))
            for (size_t i = 0; i < 64 && i < json_array_size(j); ++i)
                m->editor.macroOwn[i] = (float)json_real_value(json_array_get(j, i));
    }
    if (auto j = json_object_get(root, "editorMacroSend")) {
        if (json_is_array(j))
            for (size_t i = 0; i < 256 && i < json_array_size(j); ++i)
                m->editor.macroSend[i] = (float)json_real_value(json_array_get(j, i));
    }
    if (auto j = json_object_get(root, "editorMacroAtten")) {
        if (json_is_array(j))
            for (size_t i = 0; i < 256 && i < json_array_size(j); ++i)
                m->editor.macroAtten[i] = (float)json_real_value(json_array_get(j, i));
    }
    if (auto j = json_object_get(root, "editorLorBase")) {
        if (json_is_array(j))
            for (size_t i = 0; i < 288 && i < json_array_size(j); ++i)
                m->editor.lorBase[i] = (float)json_real_value(json_array_get(j, i));
    }
    if (auto j = json_object_get(root, "editorSpread")) {
        if (json_is_array(j))
            for (size_t i = 0; i < 64 && i < json_array_size(j); ++i)
                m->editor.spread[i] = (float)json_real_value(json_array_get(j, i));
    }
    auto loadArr = [&](const char* key, float* a, int n) {
        if (auto j = json_object_get(root, key)) {
            if (json_is_array(j))
                for (size_t i = 0; i < (size_t)n && i < json_array_size(j); ++i)
                    a[i] = (float)json_real_value(json_array_get(j, i));
        }
    };
    loadArr("editorGlobalLor",    m->editor.globalLor,    12);
    loadArr("editorGlobalSpread", m->editor.globalSpread,  4);
    loadArr("editorGlobalAtten",  m->editor.globalAtten,  16);
    loadArr("editorGlobalTap",    m->editor.globalTap,     8);
    loadArr("editorGlobalDir",    m->editor.globalDir,     4);
}