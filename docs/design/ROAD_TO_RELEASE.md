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
