# Which modules could be shareable that aren't already? (Rodney asked, pre-holiday)

## The criterion (Rodney's insight, sharpened)
"CA is really a MANY-TO-MANY MAPPING PATTERN." That IS the shareability test:
- **Mapping / relational resource** (defines relationships BETWEEN things; not owned by one endpoint)
  -> NATURALLY SHAREABLE. CA maps voice<->voice (correlation); a mapping isn't owned by one consumer.
- **Owned / authoring surface** (holds ONE Monsoon's state; a substitute for its own controls)
  -> 1:1, NOT shared. Colonnades = fader substitute (established).
- **Data / files** (.dmtune, .scl) -> shared AS DATA (load same file), not as a bound module.
So: share MAPPINGS and DATA; keep AUTHORING SURFACES and per-Monsoon STATE 1:1.

## Roster (from Monsoon.cpp addModel) classified by the criterion
NOTE: confidence varies. CONFIRMED = clear from role; CHECK = plausible, verify function before acting.

### Genuinely shared already (mappings / observation)
- **Change Alley (V2)**: voice<->voice correlation = many-to-many MAPPING. Shared. The archetype.
- **Intertropical**: scene sequencer / arrangement = a routing/ordering MAPPING. Shared.
- **Lantern**: note-output visualiser = OBSERVATION (observing doesn't own). Shareable/aggregating.

### 1:1 by nature (authoring surfaces / per-Monsoon state) -- do NOT share
- **Colonnades / Duo**: fader SUBSTITUTE for that Monsoon (established). 1:1. Share the .dmtune instead.
- **Shophouse Micro**: per-Monsoon tuning/scene modulator that writes that Monsoon's Colonnades. 1:1.
- **Sands visual editors** (SandsVisual / StraitsEastSandsVisual / StraitsSandsMacroVisual): DNA editors
  = authoring surfaces for that Monsoon's voices. 1:1.
- **Straits / Causeway / Sands**: poly VOICE / input expanders = that Monsoon's voices/inputs. 1:1
  (per-Monsoon state, not a between-things mapping).

### CANDIDATES: shareable but likely not yet -- the answer to Rodney's question (CHECK each)
- **Sikit (tuning source)**: tuning-ONLY (no fader-authoring role like Colonnades). A tuning is DATA /
  a pitch-set SOURCE, not an authoring surface. STRONGEST candidate to be shareable: many Monsoons could
  read ONE Sikit tuning (a shared tuning source), analogous to CA being a shared correlation source.
  CHECK: is Sikit a pure read-source (shareable) or does it hold per-Monsoon state? If pure source ->
  make it a shareable mapping/source like CA. (cachedSikitExpander is per-Monsoon now.)
- **Junction (JunctionExpander)**: a "junction" = routing/merging = a MAPPING between things by its very
  name. STRONG candidate: if Junction routes/merges signals, it's relational (many-to-many) like CA.
  CHECK its function -- if it's a router/merge, it's naturally shareable.
- **Changi (T1/T2/T3 expanders)**: CHECK function. If Changi is output/routing (a "spine"/distribution),
  routing is relational -> possibly shareable. If it's per-Monsoon output, 1:1.
- **Raffles (RafflesExpander)**: CHECK function (unknown role here). Classify by mapping-vs-owned once
  known.
- **Interchange (InterchangeExpander = cachedScaleExpander)**: named "Interchange" (a MAPPING/exchange
  concept) and aliased to scale -- CHECK: if it's a scale-INTERCHANGE (mapping between scales) it's
  relational/shareable; if it's a per-Monsoon scale holder, 1:1.
- **Keppel**: a CV->MPE UTILITY / transformer (downstream consumer). Already "any poly source" -- it's
  not Monsoon-bound. Not "shared" in the CA sense (it processes one stream) but it's already
  source-agnostic. Leave as utility; not a shared-mapping candidate.

## The pattern to look for (Rodney, post-holiday)
Any module whose NAME or FUNCTION is a MAPPING / JUNCTION / INTERCHANGE / CROSSING / ROUTING is a
shareability candidate -- because a mapping is inherently between-things and not owned by one endpoint,
exactly like CA. Singapore's geography gave you crossing/junction names (Causeway, Junction, Interchange,
Changi, Change Alley) -- the ones that are literally CROSSINGS are the natural shared resources. The test:
does it define a RELATIONSHIP (shareable) or hold a Monsoon's OWN state (1:1)?

Strongest "shareable but maybe not yet" candidates: **Sikit** (shared tuning source, like CA is a shared
correlation source), **Junction** (if a router/merge), **Interchange** (if a scale-mapping). Verify each
function post-holiday, then apply: mapping/source -> shareable via the existing binding mechanism;
owned/authoring -> 1:1.

## Honest caveat
Classifications marked CHECK are reasoned from names/roles, NOT verified function -- several expanders'
exact jobs weren't read in-container. Confirm each candidate's function before making it shareable;
don't share an owned-state module (would recreate the Colonnades display-contention class of bug).

Cross-ref: the Colonnades 1:1 resolution (share data not surfaces), Change Alley (the many-to-many
archetype), the shared-resource binding mechanism (CA/Intertropical/Lantern -- the path candidates would
join), MonsoonExpanderManager.hpp (the cached*Expander roster).

## DEFINITIVE pass (read the code): which not-shared modules COULD be shared

Rodney sharpened: CA is an N-to-M MAPPING -> shareable because a mapping is owned by neither end. Read
the modules' actual functions to give a real answer (not "check each"). Findings:

### Confirmed 1:1 -- NOT shareable (they're one Monsoon's own I/O, no mapping)
- **Changi (T1/T2/T3)**: "per-voice OUTPUT expander, 16 voices, gate/CV/accent jacks each" -- a 1-to-many
  fan-out of ONE Monsoon's voices = that Monsoon's OUTPUT. Owned. 1:1. (My earlier "maybe routing" was
  wrong -- it's output fan-out, not a mapping.)
- **Causeway**: "poly CV MODULATION expander, two 16ch poly CV INPUT jacks (rest, accent)" = input
  modulation to ONE Monsoon's voices. Owned. 1:1.
So Changi and Causeway are OUT -- they're per-Monsoon I/O, not N-to-M mappings.

### STRONG shareable candidates (confirmed relational/source shape)
- **Sikit -- strongest, and already half-shared-shaped.** Code: Sikit "publishes cents[] into the SHARED
  TuningTable when this Sikit is the claimed tuning source; the claim is resolved by
  updateExpanderPointers." So Sikit is a TUNING SOURCE publishing to SHARED state already. The shareable
  shape EXISTS -- the only question is whether MULTIPLE Monsoons can read that shared TuningTable (or
  whether the "claim" is exclusive to one). ACTION: allow N Monsoons to read one Sikit's published
  tuning = a shared tuning source, exactly analogous to CA as a shared correlation source. Likely a
  small change since the data already lands in a shared TuningTable; the gate is the single-claim model.
- **Interchange (InterchangeExpander)**: code shows it's a "pairing HUB for the Follow menu"
  (MicroTuning pairing + IntertropicalPairing). PAIRING is inherently RELATIONAL (N-to-M: which follows
  which). A pairing hub is a mapping by nature -> natural shared resource. ACTION: if Interchange maps
  follow/pairing relationships, it should be shareable (many Monsoons' pairings through one hub) -- verify
  it isn't already, then share via the binding mechanism.

### Could not classify from code read (UI-only headers) -- the only genuine "check" left
- **Raffles**: header is panel/layout only; function not revealed. CHECK its role (rl the raffles.json
  layout / process()).
- **Junction**: header panel-only; but the NAME (a junction = routing/merge) strongly implies relational.
  CHECK process() -- if it routes/merges signals it's N-to-M -> shareable.
(These two are the only real "verify function" items left; everything else is now classified.)

### Definitive answer to "which not-shared could be"
- **Sikit** (shared tuning source -- data already in a shared TuningTable, just needs multi-reader).
- **Interchange** (pairing/follow hub -- relational by function).
- **Junction** (likely, by name = routing/merge; confirm process()).
- **Raffles** (unknown; confirm).
- NOT Changi, NOT Causeway (per-Monsoon I/O, confirmed owned).
The rule held: the RELATIONAL/MAPPING/SOURCE modules (Sikit source, Interchange pairing, Junction
routing) are the shareable ones; the per-Monsoon I/O modules (Changi output, Causeway input) are not.
Sikit is the standout because its data ALREADY lands in shared state -- lowest-effort win.

Cross-ref: MonsoonExpanderManager.hpp (cachedSikitExpander, cachedJunctionExpander, cachedScaleExpander=
Interchange, cachedRafflesExpander -- currently per-Monsoon caches), Sikit shared-TuningTable publish
(the half-done sharing), CA (the N-to-M archetype), the mapping-vs-owned criterion above.

## CORRECTION (Rodney): Junction + Raffles are MODULATORS (1:1), not mappings. Sikit is the answer.

Rodney: "Junction and Raffles are specific modulators." I wrongly floated them as shareable candidates
on the strength of their NAMES ("junction = routing") -- classifying by what a word evokes instead of
what the module does. WRONG. They are specific MODULATORS -> per-Monsoon (a modulator drives THAT
Monsoon's parameters = owned state, not an N-to-M mapping) -> 1:1, NOT shareable. Same category as
Changi/Causeway.

Lesson: classify by FUNCTION, not by the Singapore place-name. The "crossings = mappings" pattern was
cute but not load-bearing -- the name isn't the function. (Don't let the naming seduce the architecture.)

### Corrected final classification
- **Shareable candidate -- ONE, the real one: SIKIT.** Tuning source already publishing to a shared
  TuningTable; only question = multi-reader vs exclusive claim. Lowest-effort win. (Rodney agrees.)
- **Already shared:** Change Alley (N-to-M correlation), Intertropical, Lantern. Interchange IF its
  "pairing hub for the Follow menu" is genuinely relational (that was a real function comment, not a
  name-guess -- but Rodney would know if pairing is shareable or per-Monsoon).
- **1:1 / owned (NOT shareable):** Junction (modulator), Raffles (modulator), Changi (per-voice output),
  Causeway (poly CV input mod), Colonnades/Duo (fader substitute), Shophouse Micro, Sands, the visual
  DNA editors. All per-Monsoon state.

### Net
The only "not-shared-now-but-could-be" module is **Sikit** (shared tuning source). Everything else is
either already shared (the genuine mappings: CA, Intertropical, Lantern) or genuinely per-Monsoon
(modulators, I/O, authoring surfaces). Junction/Raffles being modulators confirms: the shareable set is
small and specific, and the criterion is FUNCTION (mapping/source vs owned), never the name.

Supersedes the Junction/Raffles "candidate/check" entries above.

Cross-ref: Sikit shared-TuningTable publish (the one real candidate), CA (the N-to-M archetype),
the mapping-vs-owned criterion (apply to FUNCTION not name).
