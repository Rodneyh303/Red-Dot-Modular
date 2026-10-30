# PARKED — right-size StraitsLaneExpander params (keep DAW-automatable, fix the ~1128 export overflow)

STATUS: DESIGNED, PARKED. Do NOT start until the in-flight branches (subgate fix, unified-addressing) land —
it touches the SAME engine read-sites (SequencerEngine / MonsoonExpanderManager) unified-addressing edits,
so a concurrent branch would conflict. Schedule AFTER.

## THE BUG (real, same class as NUM_PARAMS_MIGRATION's five modules)
StraitsLaneExpander.hpp:45 `config(MonsoonIds::NUM_PARAMS, 0, 0, 0)` — every lane expander declares ~1128
params (sized to the full MonsoonIds namespace) while USING only 16 (1 mono mirror + 15 poly). It exports
~1128 to the DAW = the overflow NUM_PARAMS_MIGRATION fixes. A sixth instance of that bug.

## WHY it's sized huge (the coupling to fix)
The knobs bind to MonsoonIds param IDs (monoParamId = MonsoonIds::QMIX_LEVEL_PARAM, polyParamIdBase =
MonsoonIds::POLY_QMIX_PARAM_1), and the ENGINE reads them back BY THAT MonsoonIds INDEX
(MonsoonExpanderManager ~:489 `deepEast->params[MonsoonIds::POLY_REST_PARAM_1 + v]`). So the array must be
big enough for those indices to be valid slots. The big size IS the interface.

## THE FIX (Rodney's decision: KEEP params / DAW-automatable, right-size — NOT de-param)
Per PARAM_CLASSIFICATION.md line 62-91 (per-voice REST/ACC = "KEEP param — fixed addressing"), the lane
per-voice knobs STAY real params (host-automatable; Rodney: "we can keep them DAW automatable"). The fix is
to RIGHT-SIZE, not de-param:
- Give the lane expander its OWN SMALL local param namespace (~16: 1 mono + 15 poly) instead of sizing to
  MonsoonIds::NUM_PARAMS. config(16, 0, 0, 0)-ish on local ids [0..15].
- REMAP the engine's expander-param READ sites from the MonsoonIds index to the local index: everywhere the
  engine does `expander->params[MonsoonIds::POLY_X_PARAM_1 + v]`, change to `expander->params[localBase+v]`
  (or a small accessor). This is the ~44-read-site class the migration doc names — find them all (grep
  POLY_*_PARAM + monoParamId reads of an EXPANDER's params[]).
- Export drops from ~1128 to ~16 per lane expander — DAW-automatable (each knob a real param) WITHOUT the
  overflow (host pool not blown).
- GOLDEN PIN: the per-voice values the engine reads must be BIT-IDENTICAL before/after (same values, new
  index) — the golden battery is the proof. Pure addressing change, no value/behaviour change.

## SCOPE NOTE
Only StraitsLaneExpander (the five NUM_PARAMS_MIGRATION modules are already done; distribution/send/subgate
are StoreKnob-de-parammed). This is the ONE remaining overflow module, and it keeps params (not StoreKnob)
because its per-voice knobs are the fixed-addressing keep-param category + Rodney wants them automatable.

## DONE =
- StraitsLaneExpander sized to its own ~16-param namespace (not NUM_PARAMS); exports ~16 not ~1128.
- Engine expander-param read sites remapped to the local index; grep confirms no `expander->params[
  MonsoonIds::...]` reach-ins remain.
- Per-voice values bit-identical (golden battery holds); DAW shows the lane knobs as automatable params.
- run_all + real-SDK + UNIFORMITY_STRICT green.

## SEQUENCE: after subgate-fix + unified-addressing land (shared engine read-site conflict zone).
