# BUILD BRIEF: enabled[] mask + .dmtune v2 + detach-restore + number-band gesture

Consolidated build brief for CC. Pulls together the enabled/weight split, the .dmtune format change,
the Shophouse-Micro / Colonnades dimming + detach-restore, and the number-strip enable band. Source
rulings: SHOPHOUSE_MICRO_SPEC (override/out-of-scale/dmtune), COLONNADES_PANEL_LIFT round 8/9 (band +
gesture). Where those disagree with each other, THIS brief is the reconciled truth.

## THE CORE CHANGE: enabled[] is a first-class per-degree array, separate from weight[]

Today (the bug): a degree's scale membership is derived from weight==0 (MicroTuning.cpp:57-59:
`sceneBlocked[i] = f.weight[i] <= 0.f`; tt has NO enabled[] array). This CONFLATES "out of scale"
with "in scale but turned to zero," so a degree turned down freezes as out-of-scale and can't be
raised back.

Fix: add a per-degree `enabled` bool, distinct from weight, everywhere weight currently carries mask
meaning.

Three states per degree:
- enabled=false            -> OUT OF SCALE. Zeroed at read, fader DIMMED, fader movement no effect.
- enabled=true,  weight=0  -> in scale, silent. Fader can raise it.
- enabled=true,  weight>0  -> in scale, sounding.

Two independent facts: enabled = scale membership (the mask), weight = loudness within scale (the mix).

## STEP 1 -- TuningTable: add enabled[]

src/dsp/TuningTable.hpp: add `bool enabled[MAXN];` alongside cents[] and weight[]. Read-time masking
keys off enabled (out-of-scale -> weight reads 0), INDEPENDENT of the stored weight value. The old
"weight<=0 == masked" logic is replaced by "enabled==false == masked."

Wherever the engine currently treats weight==0 as out-of-scale (rebuildScaleCache, getSemitoneWeight,
sceneBlocked), switch the mask source to enabled[]. Keep weight as pure loudness.

## STEP 2 -- .dmtune format v2: cents + enabled, NOT weight

.dmtune carries the TUNING (cents) + the SCALE MASK (enabled). It does NOT carry weight -- weight is
the user's live fader mix (Shophouse analogy: a scale masks, it never moves your faders). Loading a
.dmtune sets cents + enabled, leaves weights untouched.

Schema v2:
```json
{ "format":"dotmodular.tuning", "version":2, "n":12,
  "cents":[...], "enabled":[true,false,...], "name":"...", "notes":"..." }
```
- REMOVE weight[] from read AND write (MicroTuning.cpp dmtune load/save).
- v1 migration on load: cents kept; enabled[i] = (v1 weight[i] > 0); v1 weight discarded.
- On LOAD: set tt.cents[] + tt.enabled[]; DO NOT touch weight params (the faders).
- On SAVE: write cents[] + enabled[]; do not write weight.

## STEP 3 -- .scl is NOTES-based (tuning), independent of enabled

.scl = the tuning. Its length = the degree count = the NOTES knob. .scl has no mask concept.
- .scl EXPORT = the NOTES-knob degrees' cents. ALL of them, enabled-mask IGNORED (a masked-out degree
  still EXISTS in the tuning, so it exports).
- .scl READ = sets NOTES + cents. (Fresh scale: enabled defaults all-true for the N degrees.)
Do NOT gate .scl export on enabled OR weight. NOTES governs .scl; enabled governs .dmtune.

## STEP 4 -- Shophouse Micro <-> Colonnades/Duo: override, dim, detach-restore

### 4a. Host resolution (the "doesn't find Colonnades/Duo" bug)
Shophouse Micro currently reads mon->engine.pe.tuning.N (indirect, stale-prone). Resolve the target
the SAME way Monsoon does: read `mon->expanderManager.cachedColonnadesExpander` (set for BOTH
modelColonnades and modelColonnadesDuo, MonsoonExpanderManager.hpp:186-191) for (a) presence and (b)
which model -> mode (Colonnades=12 / Duo=24 via curr->model). tuning.N is downstream; don't infer from
it.

### 4b. Override semantics
When a Shophouse Micro front is ACTIVE, its cents[] + enabled[] override the Colonnades/Duo authored
cents + enabled in the shared TuningTable (boundary-quantised, at the phrase edge). Weight is NOT
overridden -- the .dmtune has no weight; the Colonnades faders (live mix) ride through unchanged.
When NO front is active, the table holds Colonnades' authored cents + enabled (base).

### 4c. Dimming
Out-of-scale (enabled=false, whether from Colonnades authoring OR the active Shophouse Micro front)
dims the Colonnades/Duo faders and zeroes them at read time. Shophouse Micro does NOT move the faders
-- read-time mask only (same non-destructive rule as Monsoon+Shophouse).

### 4d. Detach-restore (needs a cache)
When a Shophouse Micro DETACHES, the tuning must RESET to Colonnades' pre-override authored state. So
the Monsoon must CACHE Colonnades' authored cents[] + enabled[] (and N) BEFORE a Shophouse Micro
override is applied, and RESTORE from that cache on detach.
- There is already a detach hook: Monsoon.cpp:105 `if (!tuningSourceExpander_ && prevSource)` notices
  the source going away. The Colonnades-authored cache restore belongs on the Shophouse-Micro-detach
  path (analogous): when the Shophouse Micro leaves, re-publish the Colonnades authored cents+enabled.
- Simplest correct model: Colonnades ALWAYS publishes its authored cents+enabled every block as the
  BASE; the Shophouse Micro override is applied ON TOP when a front is active. Then "restore on detach"
  is automatic -- with no Shophouse Micro, Colonnades' base is simply what's published, no separate
  cache needed. PREFER THIS (base-always-published) over an explicit snapshot cache if the publish
  order allows it: Colonnades writes base -> Shophouse Micro (if present + active) overrides -> single
  writer fold. Detach just removes the override layer.
- If publish order can't guarantee that, fall back to: cache Colonnades authored cents+enabled on
  first override, restore on detach. Either way the REQUIREMENT is: detach -> Colonnades authored
  tuning+mask comes back intact.

## STEP 5 -- Colonnades/Duo panel: the enable BAND + swipe-paint gesture

### 5a. The band (panel art, gen_colonnades.py)
Draw a subtle BAND behind the number strip (centred NUM_Y=80.0, spanning FIRST_X..FIRST_X+(N-1)*PITCH
+ margin, N-parameterised so it reaches 24 on the Duo). Theme namewell/ink tone, low contrast -- reads
as an active gesture area without competing with faders/knobs. New kit marker: enable_band (the rect
the widget hit-tests).

### 5b. The gesture (widget)
Hit-test within enable_band; map X -> degree via round((x - FIRST_X)/PITCH), clamp 0..N-1.
- SINGLE CLICK on a degree -> toggle that degree's enabled.
- HORIZONTAL DRAG across the band -> PAINT a range. Paint state set by the FIRST degree touched:
  start-on-enabled -> DISABLE the swept range; start-on-disabled -> ENABLE the swept range. (Sets all
  to one state, not independent-flip -- predictable on mixed ranges. One gesture builds or clears a
  scale.)
- Root (degree 0) is ALWAYS enabled: paint SKIPS it, single-click on it is a no-op (can't disable the
  tonic; consistent with locked root cents).
- LIVE feedback: faders dim/undim across the range as you paint.

### 5c. Indicator
Disabled = DIMMED FADER (ColonnadesLightSlider already dims out-of-scale; drive that dim from
enabled[]). The fader is the primary indicator (consistent with out-of-scale dimming everywhere). The
number may also dim for clarity, but the fader is the signal.

### 5d. Relationship to NOTES knob
NOTES = bulk enable (enables degrees 1..N, disables rest -- COLONNADES round-5). Band swipe = the
actual pattern. Single-click = touch-ups. Three non-overlapping controls: NOTES (bulk enable), band
(per-degree/range enable), fader (weight).

## VERIFY
- A degree turned to weight 0 with the fader stays ENABLED (in scale) and can be raised back -- the
  round-8/9 bug is gone.
- Disabling a degree (band) dims its fader and zeroes it at read; raising the fader does nothing until
  re-enabled.
- .dmtune save/load round-trips cents+enabled; weights are NOT in the file and are untouched on load.
- .scl export includes all NOTES degrees regardless of enabled mask.
- Shophouse Micro finds Colonnades AND Duo (via cachedColonnadesExpander), overrides cents+enabled on
  the boundary, leaves faders alone, and on DETACH the Colonnades authored tuning+mask returns intact.
- Swipe-paint builds a 24-tone scale on the Duo in a few gestures; root never disables.

## Cross-refs
- src/dsp/TuningTable.hpp (add enabled[]), src/MicroTuning.cpp:57-59 (weight==0 mask -> enabled),
  src/Monsoon.cpp:101-105 (tuning-source resolve + detach hook), MonsoonExpanderManager.hpp:186-191
  (cachedColonnadesExpander for both models), panel_src/gen_colonnades.py NUM_Y=80 (band placement),
  src/MonsoonShophouseMicro.cpp:21-23 (host-resolve to fix).
- Design rulings: SHOPHOUSE_MICRO_SPEC.md, COLONNADES_PANEL_LIFT_SPEC.md rounds 8-9,
  TUNING_PRESET_FORMAT.md (bump to v2).
