# Microtonal / tuning -- MASTER DOC (entry point)

The microtonal/tuning/scales work is spread across several docs. This is the map. All of it is
POST-LIBRARY (not a 1.0 blocker). Read in this order.

## The core idea (one paragraph)
12-TET conflates two things a general system must separate: the TUNING (the set of available pitches per
octave + their cents -- the superset) and the SCALE (a selected subset of the tuning -- the mode/raga/
key). Mechanism: the current 12-bit scale mask generalises to an N-BIT MASK (N = the tuning's degree
count). Per-degree enable/disable (VCV Scalar's model) is the always-available base; optional curated
named-scale presets per tuning are the convenience layer. 12-TET is just the best-curated instance.

## Design theme: East-West and the places between (working brief)
The microtonal work leans into the East-West-meeting-point narrative that Change Alley already anchors
in the collection. Change Alley was historically where currencies, languages, and cultures met and got
exchanged; the microtonal expanders extend that into the musical realm -- tuning systems from East and
West meeting and being exchanged. Maqam support (the Arabic modal system, quarter-tone-capable) fits
naturally under this framing rather than as an afterthought. Places-between includes literal cultural
crossovers like Kampong Glam's Malay-Arab tradition, which anchors the design in specific Singapore
geography rather than generic "microtonal" abstraction.

## Naming state (working titles, all changeable before release)
- SIKIT (Phase 1): working title. Malay for "a little". Semantically precise for cents-adjustment,
  culturally coherent with the collection, and philosophically honest as a small-first-step invitation
  into microtonal work. Availability checked clear at time of writing. Rodney's honest note: not from
  his own daily vocabulary but chosen because it fits the collection's Singapore-referring poetic
  register, which is a legitimate artistic stance for the whole collection (Monsoon isn't daily
  vocabulary either).
- MICRO-12 and MICRO-24 (Phases 2/3): working candidates emerging, none committed. Availability
  unchecked for all.
  * PRECINCT 12 / PRECINCT 24 (Rodney's return-to-earlier suggestion). Precinct is a real Singapore
    urban-planning unit (HDB estates organised into precincts). Multi-layer fit: (1) concrete Singapore
    referent, (2) bounded designed space with N residents ~ bounded octave with N degrees, (3)
    philosophy = Singapore's nudge-governance-through-designed-environment mirrors what a tuning does
    (composed set nudges character without coercing). Fits the collection's dominant naming register
    (English-word-with-Singapore-meaning). Clean 12/24 pairing per Changi T1/T2/T3.
    HONEST CAVEAT: Rodney has NOT lived in an HDB precinct -- his Singapore residences are private in
    central prime districts (Bugis Duo, Emerald Hill/Somerset, Yong An Park, Grange Road/Colonnades).
    So Precinct is chosen for collection-register + functional-philosophical fit, NOT from personal
    lived experience in an HDB precinct. Same category as Sikit (well-chosen borrowing for artistic
    stance) rather than a lived-experience name. Legitimate as an artistic choice, but doesn't clear
    the Bastl-line the way a lived-experience name would.
  * Kampong Glam remains a candidate -- Rodney lived at Bugis Duo NEAR Kampong Glam (not in it) for
    3 years, hearing the Sultan Mosque call to prayer. Closer to lived experience than Precinct
    (encountered daily rather than only conceptually) but still "near" not "in". Aligns more literally
    with the East-West-meeting-point theme; suits Micro-24 (maqam-capable) specifically.
  * ADDITIONAL CANDIDATES worth considering, from Rodney's actual Singapore residences (surfaced when
    the Precinct assumption was corrected):
    - COLONNADES (his current residence, Grange Road). A colonnade is a SEQUENCE OF REGULAR COLUMNS --
      structurally maps to a sequence of degrees in a tuning. From actual lived experience. Interesting.
    - EMERALD HILL (a former residence, the Peranakan shophouse street). But Peranakan taken by
      Interchange design; "Emerald" reads precious.
    - GRANGE (current street). Short, uncommon.
    None of these have been evaluated against the multi-layer bar yet.
  * Rodney's Bastl-line principle: names from lived experience beat names picked for cool factor. Both
    candidates satisfy this. Choose when closer to build.

## Naming register observation (surfaced during the Precinct discussion)
The collection's ACTUAL dominant naming pattern is English-words-with-Singapore-specific-meanings, NOT
Malay-word-borrowings. Monsoon, Straits, Shophouse, Lantern, Intertropical, Sands, Change Alley,
Junction, Causeway, Changi -- all English-language words whose meaning is Singapore-specific. Only
Peranakan (Interchange design, per Rodney) and Kampong (via Kampong Glam) are Malay. That means Sikit
would be the outlier in the actual pattern; Precinct sits comfortably in it. Not a reason to reject
Sikit (its warmth may earn it its place as an outlier), but a useful observation about what the
collection's naming register really is: English words used with Singapore's specific meanings.

Rodney's discipline on this: sit with working titles rather than force premature commitment. Nothing is
released, nothing is even remotely near release, so names can evolve as the design does. (Rodney's insight -- ship value incrementally)
Refactored: the microtonal work splits cleanly into three phases at the module-boundary layer.
- PHASE 1 -- SIKIT (TUNING_EXPANDER_SPEC.md): retune 12-TET with a 12-cents-knob expander;
  scale stays with Monsoon. Ships "retune keep-your-scale" alone, no engine widening. Small module.
- PHASE 2 -- MICRO-12: full 12-tone microtonal (custom scale + tuning), same TuningTable, still N=12.
- PHASE 3 -- MICRO-24 + engine widening: arbitrary Scala N=24, the pervasive %12/*12 audit.
Compositional principle at module-boundary: ONE JOB PER MODULE. Tuning = one job (Phase 1); tuning-AND-
scale-at-N-degrees = a different job (Phases 2-3). Different modules, different scopes, cleanly ordered.

## The docs, in reading order
1. TWELVE_TET_AUDIT.md -- where 12 is hardcoded in the engine; the N-bit-mask generalisation target.
   Framed for Monsoon Micro (12-slider + 24-slider fixed panels). THE ENGINE AUDIT.
2. SCALES_AND_QUANTIZER_TODO.md -- THE MAIN DOC. Scales-to-add list (Slendro priority, Chinese
   pentatonic, Carnatic); scales-within-tunings design (scale subset-of tuning subset-of octave; N-bit
   mask; hybrid manual + curated named scales); .scl role-agnostic (tuning OR scale by use); .kbm =
   keyboard mapping, irrelevant for slot-less CV quantizing BUT the right tool for Monsoon Micro's fixed
   12/24 faders (fader bank = keyboard). Curation = real ethnomusicological work.
3. TUNING_EXPANDER_SPEC.md -- PHASE 1: SIKIT. 12 cents knobs, retune 12-TET without touching the scale
   mask. Small, ships first. Common real-world microtonal case (well-tempered, meantone, stretch, expressive).
   Name Sikit = Malay 'a little', semantically precise for cents-adjustment.
4. MONSOON_MICRO_SPEC.md -- PHASES 2 & 3 modules: the 12/24 fixed-fader TUNING/SCALE AUTHORING expanders (Scalar-modelled:
   per-degree cents + enable/disable + .scl read/write; one cents dial + edit-mode selection; delegation
   rule = one Micro attached -> Monsoon faders blank, authority delegates). AUTHORING home.
5. SHOPHOUSE_SPEC.md -- Shophouse = the scale/tuning selector expander (CONSUMING: import/display/modulate). In the generalised world it
   loads a .scl (tuning) and its shutters become N tuning-width degree toggles (shutter = pitch).
6. SHOPHOUSE_FACADE_NOTES.md -- Shophouse panel/facade (Peranakan). Functional layer DONE on hardware;
   facade redesign decided-not-built.

## Key decisions already made (see the docs for detail)
- Scale = N-bit mask over the loaded tuning's degrees (generalises the 12-bit mask). [SCALES_AND_QUANTIZER]
- Hybrid: manual per-degree toggle (Scalar-style) always; curated named-scale presets per tuning where
  the ethnomusicology is done right (Pelog pathets etc.). Don't half-do cultural scales. [SCALES_AND_QUANTIZER]
- Root/transpose in UNEQUAL tunings is ABSOLUTE, not a rotatable mask (intervals differ per degree).
  [SCALES_AND_QUANTIZER]
- .scl = pitch list (tuning or scale by role). Consume for both tuning + named-mode presets. [SCALES_AND_QUANTIZER]
- .kbm = degree->slot map. SKIP for CV quantizing; USE for Monsoon Micro's fixed 12/24 faders. [SCALES_AND_QUANTIZER]
- Shophouse is the home: tuning loader + N-width degree toggles. [SHOPHOUSE_SPEC]

## Cross-cutting: the tuning table is SHARED across ALL modes
Monsoon Micro defines the tuning table for GENERATIVE output (modes A/B/E). But the QUANTIZER modes C & D
(MODES_C_D_QUANTIZER_PRERELEASE.md) must quantize to the SAME table -- a Micro that retunes A/B/E but
leaves C/D on 12-TET would be incoherent. Design the tuning table as one shared structure all modes read.
(C/D also need a NEGLECT pass pre-release, independent of microtonal -- see that doc.)

## Open / needs work (all post-library)
- Named-scale CURATION per tuning (the real ethnomusicological effort).
- Absolute root/transpose handling for unequal tunings (engine change).
- Monsoon Micro 12/24 fixed-fader variants + the .kbm-style degree->fader mapping. [TWELVE_TET_AUDIT]
- Scale additions: Slendro (priority), Chinese pentatonic, Carnatic. [SCALES_AND_QUANTIZER, small]

## Related but not core (incidental tuning mentions)
PITCH_PATCHABILITY_AND_DISTINCTION.md (East/West axis, Pelog as a pole), SEED_OFFSET_DESIGN.md
(unrelated, mentions in passing). Not part of the microtonal build.
