# CA shared expander (Option A) -- implementation note for Claude Code

STATUS: Build AFTER the Intertropical+Lantern+T3 subgroup pairing system is complete (MASTER_PLAN
item 12). The pairing system establishes the "shared expander" pattern; CA sharing reuses it.

## What this does
Allows a second Monsoon instance to read the same Change Alley V2 expander as the first. Enables
correlated polyrhythm: two Monsoons on different clocks (or gate/phase inputs) sharing the same
CA pin positions (rhythmSrc/melodySrc correlation structure). Each Monsoon still runs its own
engine, Sands, articulation -- only the CA correlation matrix is shared.

## How CA is currently discovered (the seam to extend)
MonsoonExpanderManager.hpp:151 -- adjacency walk, reinterpret_cast<MonsoonChangeAlleyV2*>(curr),
stored as cachedChangeAlleyV2. Currently the first Monsoon in the chain claims it.

## Changes needed (all small and bounded)

### 1. CA V2: add "Allow shared access" context-menu toggle
Add a bool field to MonsoonChangeAlleyV2: `bool sharedAccess = false;`, persisted in JSON.
Context menu: "Allow shared access" checkmark (like Lantern's source-mode toggle).
When false (default): existing behaviour -- first Monsoon that finds it owns it exclusively.
When true: any Monsoon finding it in the adjacency walk can read rhythmSrc/melodySrc.

### 2. MonsoonExpanderManager discovery: don't stop at first CA when shared
Currently the adjacency walk stops as soon as it finds cachedChangeAlleyV2. When a CA has
sharedAccess=true, a second Monsoon should also be able to cache it. The walk already finds
it -- the only change is: if the found CA has sharedAccess=false AND another Monsoon is between
this Monsoon and the CA, skip it (existing behaviour). If sharedAccess=true, cache it regardless.
In practice: the second Monsoon's walk finds the CA directly (it's adjacent or nearby), caches
the pointer, and reads rhythmSrc/melodySrc. No change to the walk algorithm needed if the modules
are laid out so both Monsoons can find the CA -- the toggle is what permits it.

### 3. Owner vs reader distinction (CRITICAL -- only one Monsoon calls applyPendingTransforms)
rhythmSrc/melodySrc: written once by CA's process(), read-only from both Monsoons. SAFE.
applyPendingTransforms(vActive): MUTATES the CA's state (applies pending scatter/transforms).
Must only be called by ONE Monsoon per block -- the OWNER (the primary/adjacent Monsoon).
Implementation: add a bool to MonsoonChangeAlleyV2: `bool transformsAppliedThisBlock = false;`,
reset in CA's process() each block. MonsoonExpanderManager checks this flag before calling
applyPendingTransforms -- if already called this block, skip. First caller wins (the owner).
This is the only thread-safety concern and it resolves simply.

### 4. Active voice count for applyPendingTransforms
Currently: `v2->applyPendingTransforms(vActive)` where vActive = engine.numPolyVoices+1 from the
owner Monsoon. The second Monsoon may have a different numPolyVoices. Since transforms are applied
by the owner only, the owner's vActive is used -- which is correct (the owner defines the CA's
operating voice count). Document this: the shared CA uses the OWNER's voice count.

## What the second Monsoon gets
- Reads rhythmSrc/melodySrc each block via processDNA() (same as today -- no change to the read path).
- Does NOT call applyPendingTransforms (guarded by the transformsAppliedThisBlock flag).
- Gets the same correlation structure as the owner Monsoon, updated at the owner's phrase boundary.
- Runs its own Sands, engine, articulation, expanders independently.

## Suggested layout in Rack
[Monsoon A] -- [CA V2 (sharedAccess=true)] -- [Monsoon B]
or
[Monsoon A] -- [CA V2 (sharedAccess=true)] -- [Sands/other expanders] -- [Monsoon B]
The adjacency walk needs to reach the CA from both Monsoons. If other expanders sit between them,
the walk (which already hops intermediates) should find it -- confirm the walk depth is sufficient.

## Files to touch
- src/MonsoonChangeAlleyV2.hpp: add sharedAccess bool + transformsAppliedThisBlock bool.
- src/ChangeAlleyV2.cpp: reset transformsAppliedThisBlock each block; persist sharedAccess in JSON;
  add context-menu toggle.
- src/dsp/managers/MonsoonExpanderManager.cpp: applyPendingTransforms guard (check flag first);
  confirm the discovery walk permits a second Monsoon to cache the pointer when sharedAccess=true.

## Seed derivation (companion to Option A, see MASTER_PLAN)
For full correlated polyrhythm, pair with the unified external seed (Option B):
  rhythmKey = externalSeed
  melodyKey = externalSeed + 1
  caKey     = externalSeed + 2
Feed the same seed CV to both Monsoons and the shared CA. Philox key-separation guarantees the
three streams are statistically independent. One seed -> entire correlated system reproducible.

## Test in Rack after building
1. Place Monsoon A -- CA V2 -- Monsoon B. Enable "Allow shared access" on CA.
2. Drive Monsoon A with 1/16 straight, Monsoon B with 1/16-triplet.
3. Feed identical seeds to both Monsoons + CA.
4. Set pins in CA and confirm BOTH Monsoons' voice correlation follows those pins.
5. Scatter from either Monsoon's CA controls: confirm the transform applies once (owner only),
   and BOTH Monsoons see the updated rhythmSrc/melodySrc.
6. Feed a simultaneous dice gate to both: confirm probability spaces stay in sync.

---

## UPDATE: use the PAIRING rack-wide scan for shared CA (not adjacency)
The IT+Lantern+T3 pairing v1 (already in master, see PAIRING_CROSS_ROW_NOTE.md) established the
rack-wide discovery model: followIT==0 = adjacency default; followIT>0 = pairId match ANYWHERE in the
rack via APP->engine->getModuleIds(). This is the SAME technique other Rack devs use for cross-rack
connections, and it's already proven in this codebase.

APPLY IT TO SHARED CA: instead of (or in addition to) the adjacency-based sharedAccess toggle, give
Change Alley a pair number and let a second Monsoon bind to it by number via the rack-wide scan. Then
two Monsoons on DIFFERENT ROWS can share one CA -- exactly the cross-row capability pairing already
gives Lantern/T3.

Mechanism (mirror pairing exactly):
- CA V2: assign a caPairId (lowest free) in process() -- NOT constructor/audio-locked path
  (getModuleIds re-lock deadlock, same gotcha as Intertropical pairId). Immutable, persisted, gaps ok.
- Monsoon: a "shared CA follow" field -- 0 = Auto (adjacency, current behaviour); >0 = the CA whose
  caPairId matches, anywhere in the rack.
- Owner rule unchanged: only ONE Monsoon calls applyPendingTransforms per block
  (transformsAppliedThisBlock flag, first-caller-wins). Second Monsoon reads rhythmSrc/melodySrc.
- Reuse ui/IntertropicalPairing.hpp patterns; don't write new discovery code.

This unifies ALL cross-module discovery in the plugin onto ONE mechanism (adjacency default + rack-wide
pair override), used by Lantern, Changi T3, and shared Change Alley alike.

## The capability this unlocks (correlated polymeter/polyrhythm across rows)
With shared CA (correlation structure) + unified seed (S / S+1 / S+2 key offsets) + independent
clocking, two (or more) Monsoons become ONE correlated generative system expressed across multiple
metric worlds:
- Same seed + shared CA => same probability space AND same correlation structure.
- Different clocks/gates/phase => different time bases (polymeter / polyrhythm).
- Result: correlated content, independent time -- the definition of heterophony / gamelan layering,
  now at the multi-instance scale, across rows.

Example patches (all become possible):
- Two Monsoons, shared CA, same seed, one phase-driven FORWARD and one phase-driven BACKWARD ->
  the same correlated material as a canon against its own retrograde. (A retrograde canon generated
  live from one stochastic source.)
- 3:2 polyrhythm (1/16 straight vs 1/16-triplet), shared CA, same seed -> the same generative voice
  in two metric grids, correlated note-for-note.
- Dice on both at the polymetric re-align (LCM) point -> the correlated system re-rolls together at
  the musical downbeat of the combined cycle.
This is the "polyphony of arrangements" (MULTIGROUP_CONSERVATION doc) extended to cross-instance
correlated polymeter. Few if any generative systems can express retrograde/inversion canons from a
single live stochastic source. Worth a headline pitch line + a demo patch.
