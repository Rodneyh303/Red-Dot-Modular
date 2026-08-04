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
