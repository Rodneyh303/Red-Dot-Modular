#pragma once
#include <rack.hpp>
#include "../../ui/SandsVisualEditorV4.hpp"
#include "../engines/PatternEngine.hpp"
#include "../engines/SequencerEngine.hpp"
#include "../VoiceResolver.hpp"   // kVoiceSlots (voiceSlot convention)
#include "SpreadManager.hpp"

namespace redDot {

/**
 * SandsParameterManager — the ONE Sands editor↔engine sync manager.
 *
 * ERADICATION Phase 3d: PolySandsParameterManager (Global/Macro) and
 * PolyVoiceSandsParameterManager (Per-Voice/East) are MERGED here. Post
 * voiceSlot-unification they were the same bridge at two scopes; every live
 * method is voiceSlot-parameterized, so one class serves both call sites.
 *
 * Dropped in the merge (zero callers, verified):
 *   - the global syncEditorToPatternEngine(VoiceState) (Macro's editor is
 *     store-backed; the write path goes through the store, never this)
 *   - getDisplayProbability (both arities)
 *   - the per-voice setSpread/getSpread wrappers (callers use spreadMgr directly)
 *   - monsoonModule (write-only field)
 *   - MacroSpreadManager (its 2-arg fan-out lives on as setSpreadAll below —
 *     clamp and loop copied verbatim; SpreadManager::setMacroSpread was deleted
 *     as dead code — it clamped to [0,1] while spread values are bipolar)
 *
 * Live surface:
 *   - East  (per-voice): ctor + syncEditorToPatternEngine(voiceSlot, state)
 *                        + spreadMgr per-voice setSpread push (each frame).
 *   - Macro (global):    ctor (monoContext=true) + setSpreadAll
 *                        + syncPatternEngineToEditor(state, viewVoiceSlot).
 *
 * monoContext=true (Macro) makes spreadMgr's V1 interpolation read the MONO
 * draw (true self-target), regardless of numVoices. East passes false.
 */

struct SandsParameterManager {
  PatternEngine* patternEngine = nullptr;
  SequencerEngine* sequencerEngine = nullptr;  // For active voice count
  int numVoices = 7;  // voice-slot count this manager guards (East: 15 = V2..V16)

  // Spread manager handles interpolation
  // Uses SequencerEngine to determine average from active voices only
  SpreadManager spreadMgr;

  SandsParameterManager(PatternEngine* pe = nullptr, SequencerEngine* se = nullptr,
                        int nVoices = 7, int startVoice = 0, bool monoContext = false)
    : patternEngine(pe), sequencerEngine(se), numVoices(nVoices),
      spreadMgr(pe, nVoices, startVoice) {
    // monoContext: this manager IS the mono master (V1) display — mark its
    // spreadMgr so V1 interpolates from the MONO draw (true self-target).
    spreadMgr.monoContext = monoContext;
    // Set SequencerEngine so SpreadManager can track active voices
    // Voice-count bookkeeping uses only requested polyphony
    if (se) {
      spreadMgr.setSequencerEngine(se);
    }
  }

  // Global spread (Macro): the same value on EVERY slot (0..15) — the global
  // mode has no per-voice variation. (Was MacroSpreadManager::setSpread, which
  // fanned only to its ctor's numVoices (7) — voices 8+ read 0 spread.)
  void setSpreadAll(int lane, float value) {
    if (lane >= 0 && lane < 7) {
      float clamped = rack::math::clamp(value, -1.0f, 1.0f);
      for (int v = 0; v < dotModular::VoiceResolver::kVoiceSlots; ++v) {
        spreadMgr.setSpread(v, lane, clamped);
      }
    }
  }

  // ── WRITE direction (East): editor grid → engine raw draws for ONE voice ────
  // voiceIdx is voiceSlot (1=V2, ..., 15=V16) — the caller (polyVoice())
  // returns voiceSlot via polyVoiceSlot. Use directly (no +1).
  void syncEditorToPatternEngine(int voiceIdx, const SandsVisualEditorV4::VoiceState& editorState) {
    // voiceIdx is a voiceSlot: 0..15 (V1..V16). Was `>= numVoices` (15), which silently
    // dropped V16 (slot 15) — its tab-exit probability sync never ran (LOR/spread still
    // persisted via saveSlot()). Fixed on Rodney's call after the Phase 3d audit flagged it.
    if (!patternEngine || voiceIdx < 0 || voiceIdx >= dotModular::VoiceResolver::kVoiceSlots) return;

    const int vs = voiceIdx;  // voiceSlot
    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyRandom(vs, SequencerEngine::PL_REST)[i] = editorState.lanes[SandsVisualEditorV4::REST].probabilities[i];
      patternEngine->rhythmSource[vs][i] = patternEngine->polyRandom(vs, SequencerEngine::PL_REST)[i];
    }

    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyRandom(vs, SequencerEngine::PL_MELODY)[i] = editorState.lanes[SandsVisualEditorV4::MELODY].probabilities[i];
      patternEngine->melodySource[vs][i] = patternEngine->polyRandom(vs, SequencerEngine::PL_MELODY)[i];
    }

    for (int i = 0; i < SandsVisualEditorV4::STEP_COUNT; ++i) {
      patternEngine->polyRandom(vs, SequencerEngine::PL_OCTAVE)[i] = editorState.lanes[SandsVisualEditorV4::OCTAVE].probabilities[i];
      patternEngine->octaveSource[vs][i] = patternEngine->polyRandom(vs, SequencerEngine::PL_OCTAVE)[i];
    }

    // Warp mirror (CC_PROMPT_03 STEP 3 — the "Sands (East) edit" dirty scope): the
    // three loops above rewrote random_[vs] for REST/MELODY/OCTAVE; refresh exactly
    // those lanes' 16 mirror cells each, so the bars/redistribution stay live. No-op
    // without a bank.
    patternEngine->rebuildWarpMirrorVoiceLane(vs, SequencerEngine::PL_REST);
    patternEngine->rebuildWarpMirrorVoiceLane(vs, SequencerEngine::PL_MELODY);
    patternEngine->rebuildWarpMirrorVoiceLane(vs, SequencerEngine::PL_OCTAVE);
  }

  // (syncPatternEngineToEditor DELETED — CC_PROMPT_UI_TO_RESOLVER_SPEC step 2: the
  //  engine→editor probability READ path was the last SpreadInterp-in-UI bar
  //  composition outside the ONE accessor. Its only caller (the Macro widget's
  //  step()) overwrote all 7 lanes from SequencerEngine::effectiveProbability the
  //  same frame, so the writes were dead; the per-voice variant was already deleted
  //  pre-merge for the same reason (cleanup/dead-poly-sync). The WRITE direction
  //  above (syncEditorToPatternEngine) is live and load-bearing. Do not reintroduce
  //  an engine→editor probability fill — the bars read the ONE accessor.)
};

}  // namespace redDot
