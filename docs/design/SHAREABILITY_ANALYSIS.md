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
