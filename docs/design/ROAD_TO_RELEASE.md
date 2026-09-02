# Road to Release -- living planning doc

The single entry point for planning dot.modular's V1 release. Living doc: update as things move.
Order of work (Rodney, from the road home): **fix all envisaged FUNCTIONALITY first, then fix PANELS.**
Build the instrument, then paint it -- panels want the function frozen so they reflect what's actually
there, not a moving target.

## How to use this
Skim the phase you're in; open the linked docs for detail. Status tags: [DONE] [IN PROGRESS] [TODO]
[PARKED] [VERIFY]. This doc LINKS; it does not duplicate -- detail lives in the referenced docs.

---

## PHASE 1 -- FUNCTIONALITY (do first)

### Quantiser modes (the spine everything else waits on)
- [IN PROGRESS] Quantiser modes unification -- QUANTISER_MODES_UNIFICATION.md
- [VERIFY] Modes C/D prerelease -- MODES_C_D_QUANTIZER_PRERELEASE.md ; MODE_B_SPEC.md ;
  MODE_B_GATE_REGRESSION.md
- [DONE] Phase engine: continuous forward + within-phrase reverse (signed Philox) -- PHASE_ENGINE_AUDIT.md
- [TODO] Phase jump/scrub: cross-phrase-boundary regeneration (within-phrase replay DONE; cross-draw
  clamped) -- PHASE_ENGINE_AUDIT.md, PHASE_POSITION_SPAN_SPEC.md

### Core generative engine
- [DONE/VERIFY] Dice + scrub -- DICE_SCRUB_MODEL.md + IMPLEMENTATION_PLAN + STEP5_STATUS + FOLLOWUPS
- [TODO] Probability navigation exact counter-offset (Rodney to fill mechanism) -- PROBABILITY_MODIFIER_MODEL.md
- [DONE] Reversible signed counter -- REVERSIBLE_MODE_DECISIONS.md, LANE_DIRECTION_REVERSE.md
- [IN PROGRESS] Lock mode -- LOCK_MODE_PLAN.md / _AUDIT / _RESUME / _SEMANTICS / LOCK_QUEUE_STATUS.md

### Tuning / microtonal
- [DONE] Microtonal master + preset format -- MICROTONAL_MASTER.md, TUNING_PRESET_FORMAT.md
- [TODO] enabled[]/N mask build -- ENABLED_MASK_BUILD_BRIEF.md
- [TODO] tonic/transpose build (resolve: Monsoon-only?) -- TONIC_TRANSPOSE_BUILD_BRIEF.md
- [DONE] Maqam preset library -- presets/maqam/README.md ; TODO fill Bayati/Saba/Hijaz non-EDO cents
- [DONE] Monsoon authors scale / Shophouse custom-scale route / Sikit tuning-only --
  MONSOON_SCALE_AUTHORING_DIRECTION.md

### MPE / output
- [IN PROGRESS][V1] Keppel CV->MPE: reverse-calc monitor, range 1->48, legato re-articulation, within-
  legato gate input -- MPE_UTILITY_BUILD_SPEC.md, MICROTONAL_MIDI_MPE_DIRECTION.md. NOTE: also the OUT leg
  of the MPE-in round-trip test (needed regardless -- no stock microtonal-CV->MPE-out exists).
- [TODO] Accent-output family (step-accent output; confirm Straits/Intertropical carry-over) --
  ACCENT_POLY_LANE_PLAN.md

### Shareability (share sources/mappings, not authoring surfaces)
- [DONE] Criterion + classification -- SHAREABILITY_ANALYSIS.md
- [TODO] Sikit multi-reader (the one shareable-not-yet source) -- SIKIT_CLAUDE_CODE_GUIDE.md
- [DONE] Colonnades/Duo 1:1 resolution -- QUANTISER_MODES_UNIFICATION.md (shared-CA section)

### New modules (V1 -- build after quantiser modes final; the note-input path depends on them)
- [TODO][V1] Pitch piano-roll editor (**Esplanade** -- the arts centre / where melody is authored) + gate
  editor (**Zouk** -- the club / the beat) -- PIANO_ROLL_MODULE_CONCEPT.md (fully specced). Esplanade is
  also the DIRECT control for the region-ordering/pathway sayr axis.
- [TODO][V1] Per-voice random-vs-input melody router (16 knobs, deparam, Straits-like) -- RANDOM_VS_INPUT_
  MODULE_CONCEPT.md. Per-voice composed<->generated freedom gradient = controllable heterophony. Both
  sources (random engine + CV input) route via Change Alley; the knobs are per-voice CA melody-source
  selection.
- [TODO][V1] CA scatter-correlation grids ("Change Change Alley") -- a SMALL strip on one side of CA: three
  per-axis grids (3x3 rhythm/melody/q-mix = 27; 2x2 domain/codomain = 4; 2x2 Intra/Inter = 4), one pin per
  row (each scatter stream reads one source, diagonal = independent/default), reusing CA's pin idiom one
  meta-level up. Correlates CA's OWN scatter draws. RANDOM_VS_INPUT_MODULE_CONCEPT.md. (Promoted to V1.)
- [TODO][V1] Source-select GREEN pin plane on CA's 16x16 voice matrix (per-voice input-vs-generated
  correlation) -- white=rhythm, red=melody, GREEN=source-select, positional-in-cell (EMS-authentic; yellow
  reserved for a future per-voice stream). Takes CA's scatter draw 8 -> 12 (2x3x2).
- [TODO][V1] External microtonal melody IN: MPE controller/DAW -> microtonal CV -> quantiser mode.
  Likely a THIRD-PARTY MPE-in module (a couple exist -- Rodney to check; VCV CORE does NOT do it cleanly),
  validated by the ROUND-TRIP TEST (write out via Keppel, read back, require round-trip error <~1-2 cents,
  well under the ~5-cent ear threshold). -- RANDOM_VS_INPUT_MODULE_CONCEPT.md
- [CONTINGENT][V1] reverse-Keppel (**Woodlands** -- the Causeway land-crossing IN) = MPE MIDI IN ->
  microtonal CV (inverse of Keppel's note+bend split). BUILD ONLY IF the round-trip test fails the cents
  tolerance (esp. an unconfigurable bend-range mismatch). Self-contained, zero engine coupling.
- [PARKED] Emerald Hill (wide 4x24-slot Shophouse) -- QUANTISER_MODES_UNIFICATION.md (Emerald Hill notes)

---

## PHASE 2 -- PANELS (do after functionality frozen)
- [TODO] Mode-relevant display dimming -- QUANTISER_MODES_UNIFICATION.md (UI refinement note)
- [TODO] Monsoon panel cleanup / source of truth -- MONSOON_PANEL_CLEANUP.md, MONSOON_PANEL_SOURCE_OF_TRUTH.md
- [TODO] Colonnades/Duo panel + Emerald Hill shutters -- COLONNADES_DUO_PANEL_SPEC.md,
  COLONNADES_PANEL_ART_DIRECTION.md, SHOPHOUSE_FACADE_NOTES.md
- [TODO] Sands panel layout -- SANDS_PANEL_LAYOUT.md
- [TODO] Panel light contrast / wiring feedback -- PANEL_LIGHT_CONTRAST.md, PANEL_WIRING_FEEDBACK.md

---

## PHASE 3 -- RELEASE DELIVERABLES
- [DONE] CI: Win/Mac/Linux green -- .github/workflows/build.yml + README
- [TODO] Release-on-tag job (v1.0.0 -> GitHub Release with 3 binaries)
- [TODO] Fix Makefile -march=native (distribution bug: crashes on older CPUs; CI works around it)
- [TODO] Per-DAW MPE recording guide + demo patches (Bitwig/Cubase/Ableton proven)
- [DONE] Launch story -- LAUNCH_INTENT_AND_STORY.md ; feature spine -- COOL_POINTS_FEATURE_SPINE.md
- [TODO] Define "V1 ships" cut line explicitly (what's in, what's post-V1)

---

## DEFERRED / POST-V1
- Quantiser A/B/C + D/E/F mode-letter refactor (+ patch migration)
- Poly CV in on Straits
- MIDI 2.0 Keppel (gated on VCV SDK exposing MIDI 2.0/UMP)
- CV-indexed jins bank (if maqam becomes central)
- Phase Option B beyond within-phrase (cross-draw regeneration)

## Backlog / idea docs
LOR_BACKLOG.md, LOR_BACKLOG, IDEAS_PARKED.md, WORK_ROADMAP.md, MASTER_PLAN.md,
MODULE_NAMING_AND_ROADMAP.md, REFACTOR_PRIORITY.md, SCALES_AND_QUANTIZER_TODO.md

## Meta
Keep this doc current as the planning hub. When a doc is superseded, note it here. The near-term arc:
finish PHASE 1 functionality (quantiser modes are the pole everything leans on), then PHASE 2 panels,
then PHASE 3 release deliverables + the explicit V1 cut line.

## STRATEGIC NOTE: the growth is quantiser-mode scope CORRECTION, not creep (Rodney)
The recent scope growth is driven by proper attention finally going to the QUANTISER MODES -- and that
attention is warranted, because the quantiser modes are more FOUNDATIONAL than they'd been treated.

### Why it's correction, not creep
Quantiser mode can take ANY external mono gate pattern AND any mono/poly CV pattern and VARY it. So it
turns Monsoon into a UNIVERSAL TRANSFORMER of external material, not just a self-contained generator. The
session's "extra" features are NOT bolt-ons -- they're the natural surface area of a properly-realised
quantiser capability:
- q-mix (blend external CV with generated) = a quantiser-mode capability.
- source-select pin plane = correlating the quantiser blend.
- octave range / fold = how quantiser handles external CV register.
- external-melody-in (MPE/round-trip) = feeding the quantiser external material.
- external-input symmetry (gates <- rhythm/legato/rest/accent; CV <- q-mix) = the quantiser's treatment of
  both external streams.
Every one is quantiser-mode surface area -- they all orbit "the quantiser takes external material and
transforms it". COHERENCE TEST PASSES: a single capability being fully realised, not many features
accreted. Creep is centrifugal (features flying off in all directions); this is centripetal (everything
pulling toward one under-realised pillar). So it's scope CORRECTION.

### Why it's release-important
If the quantiser varies ANY external mono gate + ANY mono/poly CV, Monsoon is not just "a generative
sequencer" but ALSO "a transformer you drop into any patch to vary/quantise/blend whatever you feed it".
That DOUBLES the addressable use (generative AND transformative), and the transformative half is arguably
the MORE broadly useful one (every patch has gates + CV to feed it; not everyone wants a self-contained
generator, but nearly everyone can use "vary this pattern"). So the quantiser modes are ~HALF the value
proposition -- the broadly-useful half -- and under-shipping them would leave the release lopsided (strong
generator, weak transformer). This justifies the attention and the V1 scope.

### But the discipline the reframe does NOT dissolve
Correct scope-correction still has to SHIP. "Coherent + important" makes it worth doing; not free to do.
So the reframe changes the DISPOSITION (defend these as pillar-realisation, don't cut them on suspicion of
creep) but not the DISCIPLINE (still need a V1 cut line; "important" != "all of it in release 1"). Healthy
version: the quantiser modes are a V1 PILLAR, AND within the pillar there's still a minimum-viable vs
full-realisation cut. E.g. q-mix core = V1, CCA scatter-grids = possibly V1.1 -- NOT because they're creep,
but because even a coherent pillar can ship in stages. The reframe RESCUES these from the "cut as creep"
bin and RECLASSIFIES them as "pillar, stage sensibly".

### For the V1 cut-line task (Phase 3)
When defining the explicit V1 cut line, treat the quantiser-transformer pillar as core (not creep to
trim), then stage WITHIN it: what's the minimum quantiser-transformer that ships as V1, and what's a
fast-follow. Don't trim the pillar; stage it.

Cross-ref: RANDOM_VS_INPUT_MODULE_CONCEPT (the quantiser-mode features = the pillar's surface area),
QUANTISER_MODES_UNIFICATION (the pillar itself), LAUNCH_INTENT_AND_STORY (value prop: generative AND
transformative -- the transformer half is the broadly-useful one), the New Modules V1 list above (stage
within, don't trim)." 

## Second de-risking: the growth rides HARD-WON PATTERNS (Rodney)
Beyond being coherent (a quantiser-mode pillar, not creep), the growth is also TECHNICALLY DE-RISKED: it
lands in areas already hard-won and thought-out (Sands, CA) and FOLLOWS existing hard-fought patterns
rather than inventing machinery -- deparam, undo, lock mode, UI MVC.

Two DIFFERENT safeties, both needed before accepting growth:
- Coherence (prior note): it's the RIGHT stuff to build (a pillar).
- Pattern-reuse (this note): it's CHEAP/SAFE to build (rides proven patterns).

### Why pattern-reuse makes it low-risk
The new features are new ARRANGEMENTS of existing engineering, not new engineering:
- deparam: modulatable-value-with-edit-vs-modulated-display is built + battle-tested. q-mix knobs, octave
  min/max faders, source-select = "just another deparam value". The hard part (modulatable/gettable/lock-
  aware/clean edit-display split) is DONE; new controls inherit it.
- undo: StoreEditAction + the CA ring-buffer groundwork exist. New pins/knobs/grids get undo for free by
  following the established action pattern.
- lock mode: Big5 lock scope is settled; new modulatables (q-mix as Big6) JOIN the existing scope, no
  bespoke lock handling.
- UI MVC: the model/view/controller separation (MVC_UNIFICATION history) is followed, not reinvented, for
  new UI (pin planes, scatter grids, knobs).
A new pin plane = "CA's pin idiom again". A new modulatable knob = "deparam again". The scatter grids =
"CA's pin matrix again, smaller". Undo/lock/MVC = followed, not invented. So scope grows WITHOUT risk
growing proportionally -- risk lives in NOVEL patterns, and there are almost none here.

### Connects to the session's recurring discovery
The code is further along than the docs and the patterns are already there (reverse-phase, jump/scrub
replay, enabled mask, deparam, reversible signed counter, CA's own scatter streams -- all already done).
The growth lands on a MATURE, PATTERNED codebase where "the new feature" is mostly "apply pattern X to
case Y" = the safest ground for scope growth.

### Implication: it RAISES the V1 ceiling
If new features ride proven patterns, each costs LESS (no new infrastructure) -> MORE of the pillar can
realistically ship in V1 than if each were novel. The staging question (q-mix core V1, CCA V1.1?) is
SOFTENED because CCA is "CA's pin matrix, smaller" -- cheap, pattern's there. Pattern-reuse doesn't just
de-risk; it raises how much of the pillar V1 can hold.

### The one caution (keep honest)
"Follow the pattern" is the right discipline but requires actually following it, not half-following +
improvising under time pressure. The patterns are hard-won BECAUSE deviating caused pain before (MVC
unification, deparam migration were corrections of earlier mess). The safety is real ONLY IF the patterns
are followed FAITHFULLY: deparam every new value properly, wire undo through the established action,
respect MVC, put new modulatables in lock scope. The risk isn't the patterns -- it's the temptation to
shortcut them.

Cross-ref: the scope-correction note above (coherence = the other safety), SANDS_ARCHITECTURE_CONSOLIDATION
+ MVC_UNIFICATION + deparam/undo/lock docs (the hard-won patterns to follow), RANDOM_VS_INPUT_MODULE_
CONCEPT (the new features = arrangements of these patterns)." 

## STANDING PRINCIPLE: pre-release -- NO backward-compat / patch-breaking concern (Rodney)
Nothing is released. There are NO existing patches in the wild. So backward-compatibility, patch-breaking,
and save-format migration are NON-ISSUES until the first public release. Do NOT raise them.

Implication (a real freedom of this stage): anything can change freely -- param IDs, serialization format,
module layouts, the mode-letter scheme, deparam internals, panel HP, pin schemes -- with ZERO migration
burden. Make the RIGHT structural choice now, not a compatible-but-compromised one. "Would this break
existing patches?" is simply not a question yet. Refactor freely; rename what wants renaming; restructure
formats cleanly.

Migration / backward-compat discipline begins ONLY at the first public release (then saved patches must be
honoured). Until then: choose correctness over compatibility, always.

Cross-ref: the pattern-reuse note above (follow patterns for QUALITY, not for compat -- these are different
reasons; pre-release there's no compat reason at all), REVERSIBLE_MODE_DECISIONS / NUM_PARAMS_MIGRATION
(past internal migrations were dev-time refactors, not user-patch migrations -- same freedom)." 

## PRE-RELEASE: breaking changes are FREE -- "needs migration / don't break patches" cautions are VOID (Rodney)
Nothing is released. There is NO installed user base, NO saved patches in the wild, NO migration burden. So
backward compatibility / breaking existing patches is a NON-CONCERN. Do not raise it. You are free to
change ANYTHING -- param IDs, mode letters, panel layout/HP, serialization, preset format -- without any
thought for "existing users".

### This is the WINDOW to do the breaking things (strategic)
Pre-release is the ONE window where breaking changes cost NOTHING. Once released, the window closes. So
this is the RIGHT TIME to make any structural change previously deferred OUT OF migration-caution. Get
param IDs, mode letters, preset format, panel HP all RIGHT now, unconstrained.

### Stale cautions to STRIP / treat as do-it-now
- The quantiser A/B/C + D/E/F mode-letter refactor was "DEFERRED: needs patch migration". That reasoning
  is VOID -- no patches to migrate. If wanted, DO IT NOW while free.
- .dmtune / TuningPreset format: change it freely if a cleaner design wants it (the v1->v2 enabled[]
  migration exists, but there are no user presets bound by it -- only the shipped ones you control).
- Any "don't break saved patches" / "for backward compat" note anywhere: treat as NON-BINDING pre-release.
- Param renumbering, NUM_PARAMS changes, panel-layout changes: free.

### The only real constraints (NOT compat)
Code correctness, the hard-won PATTERNS (deparam/undo/lock/MVC -- follow them, per the de-risking note),
and shipping. Compatibility is NOT a constraint until release. When release approaches, LOCK the formats/
param-IDs/mode-letters deliberately as a release step -- that's when compat starts to matter, and doing
the breaking cleanups NOW is what makes that lock clean.

Cross-ref: DOC_PRIORITISATION / DEFERRED sections (strip "needs migration" as a deferral reason), the
pattern de-risking note (patterns ARE a constraint; compat is NOT, pre-release), TUNING_PRESET_FORMAT
(free to change pre-release)." 
