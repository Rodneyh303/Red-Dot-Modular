# Doc prioritisation -- which of the ~130 docs are still LIVE / big challenges

Method: git last-commit DATE per doc + cross-check the described feature against CODE presence + scan each
doc's own status language (DONE/superseded/TODO/open). Confidence tagged. NOT a deep read of all 130 --
a triage. Caveat: recent-code-churn signal was unavailable (shallow clone), so "still open" = inferred
from doc-status + code-presence, not from watching live commits. Spot-check anything surprising.

**IMPORTANT (learned immediately): Tier-1 'open' calls are inferred from each doc's own status language,
which DRIFTS behind code. ENABLED_MASK_BUILD_BRIEF was listed open here but is actually DONE (Rodney
caught it; code-verified). Before trusting ANY Tier-1 'open' verdict, do a 30-second code grep for the
feature -- the doc describes the intended fix, the code may have since shipped it. Re-verify each Tier-1
entry against code before starting it.**

## The shape of it
Docs span late-Jun -> late-Aug 2026. Age predicts status strongly:
- Late Aug (the live layer): the docs we've been working -- planning + current open threads.
- Early-to-mid Aug: the main build period -- mostly BUILT (verify, don't assume).
- Jun / early Jul: foundational systems now IN the code (Sands arch, voice resolvers, lane models,
  reversible mode, CA design) -- overwhelmingly SETTLED. Treat as reference/history unless a newer doc
  points back to one as open.

## TIER 1 -- LIVE / real open work (start here; these are the actual challenges)
- **QUANTISER_MODES_UNIFICATION.md** [Aug18, HIGH conf] -- the spine. Modes + the whole editor concept
  + shared-CA. Everything Phase-1 leans on. LIVE.
- **MODES_C_D_QUANTIZER_PRERELEASE.md** [Aug06] -- self-says the quantizer side is MISSING the
  phase-driven equivalent ("Add it -- completes the..."). REAL open work: the C/D/F quantiser modes.
- **PHASE_ENGINE_AUDIT.md** [Aug25, HIGH conf] -- fwd+reverse DONE (signed Philox); OPEN = cross-phrase
  jump/scrub regeneration (within-phrase replay done, cross-draw clamped). Narrow but real.
- ~~ENABLED_MASK_BUILD_BRIEF.md~~ **-> DONE (Rodney corrected; code-verified).** Moved OUT of Tier 1.
  The enabled[] array + the whole fix is implemented: TuningPreset.hpp has bool enabled[MAXN] (v2 scale
  mask) with v1->v2 migration (enabled[i]=weight>0, weight discarded) + serialization; TuningTable.hpp
  has the semantics (enabled=false => out-of-scale, zeroed at read regardless of weight, fader dimmed;
  enabled=true => in-scale, weight=loudness); TuningList.hpp: .dmtune carries cents+enabled not weight.
  The enabled-vs-weight conflation the brief flagged is FIXED. Verify the read path is wired end-to-end
  in the engine, then archive the brief as DONE.
- **TONIC_TRANSPOSE_BUILD_BRIEF.md** [Aug10] -- **BUILT (12-TET), code-verified 2026-09-03.** rotateMask12/
  normaliseToTonic (ScaleMaskArbiter.hpp:36,44), transposable flag (save/load), Monsoon "Set as tonic"
  menu (MonsoonWidget.cpp:81-102 + setTonicAbsolute/unsetTonic), red root shutter -- all present. Only
  the non-12 generalisation (rotateMaskN, for Duo-24 / non-12 Micro) is deferred post-V1 (deliberate; not
  reachable via the 12-only Sikit path). Residual = Rack runtime-verify. NOT "Partial".
- **PROBABILITY_MODIFIER_MODEL.md** [Jul06] -- clamping open-question **RESOLVED in code (verified
  2026-09-03)**: SpreadResolver::effective() already sums-then-clamps ONCE (end-clamp), the resolver test
  asserts end-clamp, and the header documents it -- only a stale inline comment ("Clamps STEP-WISE") in
  SpreadResolver.hpp remains to fix. STILL OPEN: Rodney's feature-spine point 4 (exact probability
  counter-offset) -- awaits Rodney's mechanism spec, not code.
- **MPE_UTILITY_BUILD_SPEC.md** [Aug12, HIGH conf] -- Keppel.cpp exists but the spec's items (range
  1->48, re-articulation, within-legato gate input, reverse-calc monitor) are the live Keppel build.
- **PIANO_ROLL_MODULE_CONCEPT.md** [Aug29] -- fully specced, PARKED post-quantiser-mode. Not urgent but
  the reference when editors get built.

## TIER 2 -- probably done, VERIFY (don't assume; quick check each)
- **LOCK_*.md** (many) -- LOCK_QUEUE_STATUS self-says "DONE... No work remaining." Lock looks COMPLETE;
  no lockMode symbol even grepped in src (may be named differently). VERIFY lock is done, then archive
  the lock docs as history.
- **UNDO_IMPLEMENTATION_ROADMAP.md** [Jul30] -- self-says infrastructure done, Item 3 DONE; "scrub not
  yet built" interim note. Mostly done; verify remaining items.
- **DICE_SCRUB_* (many)** -- STEP5_STATUS is the latest; scrub largely built. Verify final state, collapse
  the step-plan docs into one status.
- **ACCENT_POLY_LANE_PLAN.md** [Jun23] -- accent output family; code present in OutputGenerator. Verify
  what's built vs the step-accent-output todo.
- **SCALES_AND_QUANTIZER_TODO.md** [Aug05] -- a real TODO list (Slendro priority etc). Content scales to
  add; live but low-risk (data, not architecture).

## TIER 3 -- SETTLED / reference (foundational, built; read only if a Tier-1 doc points back)
Sands family (ARCHITECTURE_CONSOLIDATION, LANE_RESOLVER, OWNERSHIP_SPEC, LANE_INDEX_AUDIT, CV_ROUTING_BUGS,
COMBINATIONS, TOPOLOGY_RESOLVER, PANEL_LAYOUT, SCATTER_FLICKER) -- the Sands system, built.
Voice/lane core (VOICE_RESOLVER_SPEC, UNIFIED_VOICE0_SPEC, LANE_POSITION_MODEL, LANE_DIRECTION_REVERSE,
PLAYHEAD_ALGORITHM, REVERSIBLE_MODE_DECISIONS) -- built.
CA family (CHANGE_ALLEY_DESIGN, CA_SHARED_EXPANDER_*, CA_REUSES_DICE_RNG, PHILOX_KEY_DERIVATION,
CA_DICE_COUNTER_MODEL) -- built.
Rate/dataflow (RATE_TABLE, RATE_DISCIPLINE_UNIFICATION, PROCESS_RATE_AUDIT, DATAFLOW_DISCIPLINE_PLAN,
RATE_AND_DATAFLOW_ENTRY, MODULATION_CLAMP_INVARIANT) -- built discipline.
Module specs already built (INTERTROPICAL_SPEC, LANTERN_SPEC, SHOPHOUSE_SPEC/_MICRO_SPEC, STRAITS_*,
CHANGI_*, TUNING_EXPANDER_SPEC, SIKIT_GUIDE, MONSOON_MICRO_*). Refactor/migration histories (CODEBASE_
REFACTOR_REVIEW, MVC_UNIFICATION, NUM_PARAMS_MIGRATION, GUI_THREAD_FINALS, PARAM_CLASSIFICATION) -- done.

## TIER 4 -- panels (Phase 2, deliberately deferred until functionality frozen)
COLONNADES_PANEL_* , MONSOON_PANEL_CLEANUP/_SOURCE_OF_TRUTH, SANDS_PANEL_LAYOUT, PANEL_LIGHT_CONTRAST,
PANEL_WIRING_FEEDBACK, SHOPHOUSE_FACADE_NOTES. All Phase-2; don't touch until Phase-1 done.

## Suggested first-week order (from ROAD_TO_RELEASE Phase 1)
1. MODES_C_D_QUANTIZER_PRERELEASE -- finish the quantiser modes (the spine; editors + much else wait on it).
2. ENABLED_MASK_BUILD_BRIEF -- the enabled[]-vs-weight conflation bug (concrete, self-specified).
3. TONIC_TRANSPOSE_BUILD_BRIEF -- BUILT (12-TET); Rack runtime-verify only. Non-12 (rotateMaskN) post-V1.
4. MPE_UTILITY_BUILD_SPEC -- Keppel range/re-articulation/gate-input.
5. PHASE_ENGINE_AUDIT -- cross-phrase jump/scrub (only if you want scrub across phrases now; else defer).
6. PROBABILITY_MODIFIER_MODEL -- fill point 4 (counter-offset mechanism). Clamping question already resolved in code (end-clamp shipped).
Then VERIFY Tier 2 (lock/undo/dice-scrub/accent) to confirm what's actually done, archive the settled
step-plan docs, and move to Phase 2 panels.

## Housekeeping suggestion
Many docs are step-plans of finished work (DICE_SCRUB_STEP3/4/5, LOCK_MODE_* x8, UNDO_* x3). Once verified
done, COLLAPSE each family into a single STATUS doc + move the rest to a docs/design/history/ folder.
Would cut ~40 docs from the active set without losing anything. (Do NOT delete -- move.)

Cross-ref: ROAD_TO_RELEASE.md (the phase plan this prioritises within).
