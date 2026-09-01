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
  * COLONNADES / COLONNADES DUO (Rodney's refined candidate; the 24-tone module is TWO twelves, so
    the name reflects the actual doubling architecture -- not just a counted variant):
    - COLONNADES (Phase 2, 12-tone) -- Rodney's current residence, Grange Road. One colonnade = 12
      regular columns = 12 tuning degrees.
    - COLONNADES DUO (Phase 3, 24-tone) -- two colonnades stacked = 24 degrees. "Duo" also references
      Bugis Duo Residences, Rodney's former home near Kampong Glam. Both residences honored; the
      doubling architecture is EXPLICIT at word-level.
    (Alternative expression: "DUO 12" -- more mathematically spare, but "12 of what?" ambiguity.
    Ordering: "Colonnades Duo" not "Duo Colonnades" -- Rodney's ear test (Colonnades Duo sounds
    better than Duo Colonnades) and natural English "main noun + qualifier" convention (iPhone Pro,
    Model S, Cabernet Reserve). Note: Change Alley V2 and Visual Helper V4 in the current codebase
    are ACCIDENTAL names (working context that stuck) and will be renamed to "Change Alley" and
    "Visual Helper" before release -- they are NOT intentional naming precedent for anything. The
    collection's actual pattern is single clean names per module, no version markers (Changi T1/T2/T3
    is the only intentional numbered family, and only because real Changi terminals are numbered).
    Colonnades / Colonnades Duo would be the collection's first intentional base+variant pair.)
    Why this is a strong candidate -- clears the multi-layer bar at FIVE layers, all Rodney's own ground:
    (1) Concrete: two lived residences (current + former), both real Singapore-life places.
    (2) Functional: name TEACHES the module's structure -- Colonnades Duo = two Colonnades = the
        24-tone variant IS Micro-12 doubled. Matches the actual design (per MICRO_TUNING_INTEGRATION_PLAN,
        the 24-tone microtonal work IS conceptually two 12-degree banks; two Interchanges modulate it
        cooperatively, one per twelve -- so "Duo" fits that architecture precisely).
    (3) Architectural/philosophical: Colonnades on Grange Road is by PAUL RUDOLPH, the American
        modernist architect whose whole practice was about how repeated structural elements organise
        space and experience. That is EXACTLY what a tuning does with repeated pitch-degrees organising
        the octave. The correspondence isn't a metaphor -- it's the same design instinct applied to
        different media (space vs pitch). Naming a tuning module after a Rudolph building whose whole
        point is "rhythmic organisational structure of repeated columns" is intellectually coherent.
    (4) Biographical (only Rodney could bring this): Duo was Rodney's FIRST Singapore home; Colonnades
        is his current one and, rents permitting, the hoped-for LAST. So the pair encodes the ARC of
        the Singapore chapter -- beginning (Duo) + hoped-for-completion (Colonnades) -- and the
        24-tone module named "Colonnades Duo" literally CARRIES BOTH ENDPOINTS of that chapter,
        layered. Biography embedded in the module names.
    (5) Design-function-to-geography (the deepest layer, connects the biographical to the module's
        SPECIFIC purpose): Bugis Duo Residences sits right next to KAMPONG GLAM -- the Malay-Arab
        quarter of Singapore, where the Sultan Mosque is, where Arab-trade heritage lives. Rodney
        heard the Sultan Mosque CALL TO PRAYER daily for 3 years from that home. Colonnades Duo, the
        24-tone module, is SPECIFICALLY DESIGNED to facilitate MAQAM -- the Arabic modal system with
        its quarter-tone intervals. So "Duo" in the name is not just "former residence layered in";
        it is specifically the residence that anchors the ARAB-MUSICAL-TRADITION SUPPORT in the
        Micro-24's design, via the actual geography where Rodney encountered that culture daily. The
        mapping: Colonnades (12-tone, no specific tradition anchoring) = Grange Road / Rudolph
        modernism / neutral 12-tone base. Colonnades Duo (24-tone, adds maqam) = Grange Road PLUS
        Bugis Duo (adjacent to Kampong Glam, where maqam lives in Singapore's cultural fabric). The
        doubling in the name matches: doubled degrees + adding a specific cultural tradition, both
        anchored in the specific home where that cultural tradition entered Rodney's daily life.
    Clears Bastl-line completely: defensible from real story (I named the microtonal modules after the
    walls of my actual life in Singapore; the doubled name carries the whole chapter; the Duo half
    specifically grounds the Arab-musical-tradition support in the geography where I heard that call
    to prayer for three years). Rudolph connection makes the architectural-tuning parallel
    intellectually rigorous. The Kampong-Glam-adjacent grounding gives the maqam-support design
    decision a geographical anchor in Rodney's actual Singapore, not decorative appropriation.
    This also extends the collection's East-West-and-places-between design theme (originally anchored
    by Change Alley -- where East and West met historically for trade) with a place-based anchor for
    the MUSICAL crossover: Change Alley the street + Bugis Duo the residence = two specific Singapore
    places where East and West meet, one for correlation/exchange, one for microtonal-tuning-tradition.
    Both grounded in Rodney's actual lived Singapore geography.
    Note: public meaning of "Colonnades" reads as classical/architectural, NOT "Rodney's home" -- that's
    fine, meaning accumulates through use (Fender didn't name Stratocaster from private reference
    either). The private ground is what makes the naming honest, not what makes it legible.
    Practical: "Colonnades Duo" as a two-word panel name -- follows the family-first pattern of Change
    Alley V2; scans cleanly with natural English stress (main noun first, qualifier trails). Verify Library availability at build.
  * Earlier candidates (Precinct, Kampong Glam, 12/24 Colonnades) preserved above but
    Colonnades/Colonnades Duo supersedes them as current working titles: stronger lived-experience
    ground (two residences honoured), name TEACHES the doubling architecture, Rudolph connection adds
    architectural-philosophical rigour, biographical arc adds fourth layer only Rodney could bring.
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
7. SCALA_FILE_AND_LOAD_UI.md -- SHARED INFRASTRUCTURE. Reusable `dotModular::ScalaFile` class + file-picker
   UI, consumed by Sikit + both Micros. One .scl parser (with per-caller accept predicate for degree
   count) rather than three duplicated implementations. Standard Scala format
   (https://www.huygens-fokker.org/scala/scl_format.html). Also referenced from the CC build guides.
8. MICROS_ENGINE_CLAUDE_CODE_GUIDE.md -- Phase 2/3 engine build guide. Code-level how-to for widening
   the pitch/scale pipeline from 12-only to N-aware (N up to MAXN=24). Concrete TuningTable API spec,
   class-by-class widening list matching TWELVE_TET_AUDIT, 12-step build order with byte-identical
   regression at 12-TET as the safety guarantee, and WriteLedger integration for single-writer
   discipline on the shared table.

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

## SCOPING PRINCIPLE (Rodney): microtonality is opt-in via an attached tuning source; operations are tuning-agnostic

Quantise / generate / blend produce POTENTIALLY MICROTONAL results ONLY IF a tuning source (Colonnades,
Duo, or Sikit) is attached (and has claimed). Otherwise they resolve against default 12-TET.

### Mechanism (code-verified)
A tuning source PUBLISHES cents into the shared TuningTable when it is the claimed source
(Sikit.cpp:15-17,25: claimAsTuningSource -> publish cents[]; loser greys; claim resolved by
Monsoon::updateExpanderPointers). If nothing claims, the TuningTable holds its default = 12-TET (Sikit.cpp
:33 "default Sikit reproduces 12-TET exactly"; TuningTable default is 12-TET). So:
- Colonnades / Duo / Sikit attached + claiming -> TuningTable = their custom cents -> quantise/generate/
  blend produce potentially MICROTONAL results in that scale.
- None attached -> TuningTable = 12-TET -> same operations produce conventional 12-TET results.

### The architectural property
The core operations are TUNING-AGNOSTIC. They resolve against the shared TuningTable in DEGREE SPACE:
- Quantise snaps to the TuningTable's degrees. Generate draws degrees. Blend selects between sources.
- None contain tuning logic. Pure degree operations.
The tuning is a SEPARATE, SWAPPABLE layer (the TuningTable, populated by whoever claims). Microtonality is
INJECTED UNDERNEATH the operations, not built into them. So microtonality is OPT-IN and ORTHOGONAL: attach
a tuning source and everything the engine already does becomes microtonal for FREE (no change to quantise/
generate/blend); detach and everything reverts to 12-TET, still working.

### Same theme as the sayr mapping, one layer down
The operations are GENERAL (degree-space); the tuning SOURCE makes them microtonal -- exactly as the
general modulation engine's TARGETS make it express sayr. Microtonality isn't special-cased into the
sequencing; it's a SUBSTRATE the general operations resolve against. "General engine, specificity injected
underneath" -- the recurring dot.modular architecture.

### Practical consequence (on-ramp story)
Develop / test / demo the whole engine in 12-TET (no tuning source needed); microtonality is a DROP-IN --
attach Sikit/Colonnades/Duo and the same patch becomes microtonal. A user starts in familiar 12-TET;
microtonality is one module-attach away, not an up-front commitment. The instrument meets you where you
are and opens the door when you want it.

Cross-ref: TuningTable.hpp (the shared table, 12-TET default), Sikit.cpp:25/33 (claim + publish; 12-TET
default), Colonnades.hpp (authors cents+weight+mask, publishes when claimed), SHAREABILITY_ANALYSIS (one
claimed source; the multi-reader question), LAUNCH_INTENT_AND_STORY (on-ramp: 12-TET default, microtonal
drop-in), PROBABILITY_MODIFIER_MODEL (the sayr mapping = the same general-engine/injected-specificity
theme).
