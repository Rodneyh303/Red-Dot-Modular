# Lantern cross-row fix: resolve the Monsoon THROUGH the followed Intertropical

## Symptom
Changi T3 works on a separate row from its Intertropical (follows by pair number, reads IT jacks).
Lantern does NOT fully work cross-row the same way.

## Root cause: Lantern has TWO source dependencies, only ONE is cross-row capable
Lantern reads from two places (Lantern.cpp):
1. The INTERTROPICAL (line 224): redDot::resolveFollowedIT(this, followIT) -- this IS cross-row.
   resolveFollowedIT with followIT>0 does the rack-wide getModuleIds() scan (pairId match). Works.
2. The MONSOON ENGINE (line 216): redDot::findMonsoonEitherSide(this) -- ADJACENCY-ONLY. Does NOT
   cross rows.
Why Lantern needs BOTH: it reads the IT for the routed arrangement AND the Monsoon engine for the
richer per-note detail (note types, colours, articulation the IT jacks can't carry -- the reason
Lantern is engine-reading, not jack-reading). T3 only needs the IT jacks, so T3 crosses rows cleanly
with one pair link. Lantern's SECOND dependency (Monsoon, adjacency-only) is what breaks cross-row.

## Fix: read the Monsoon THROUGH the followed Intertropical (transitive, one pairing)
The IT already finds its host Monsoon in its own process() (Intertropical.cpp:64
findMonsoonEitherSide(this)) -- and the IT is adjacent to its Monsoon (local link). So:
- Lantern follows IT #N by pair number (rack-wide, already works).
- That IT knows its Monsoon (IT<->Monsoon adjacency is LOCAL, they sit together).
- Lantern reads engine state via the IT's Monsoon reference -- NOT via its own findMonsoonEitherSide.
One pairing (Lantern->IT) transitively gives BOTH the arrangement AND the engine, because the IT
carries its Monsoon. Works across rows because every link is either rack-wide (Lantern->IT) or local
(IT->Monsoon).

## Implementation (Claude Code)
1. Intertropical: CACHE + EXPOSE its host Monsoon. Currently findMonsoonEitherSide(this) is a local
   var in process(). Add a member (e.g. Monsoon* cachedHost = nullptr;), set it each process(), and a
   public accessor (Monsoon* getHost() const { return cachedHost; }).
2. Lantern: when in IT-source mode with a followed IT, get the Monsoon from the IT:
   - it = resolveFollowedIT(this, followIT);
   - mon = it ? it->getHost() : redDot::findMonsoonEitherSide(this);  // fall back to adjacency if no IT
   So a followed IT provides the Monsoon; Auto/adjacency still works standalone.
3. Guard: if it is non-null but it->getHost() is null (IT temporarily hostless), degrade gracefully
   (hold last / blank), same as the existing disconnection handling.
4. Keep the pure-adjacency path for followIT==0 (Auto) unchanged -- zero-config adjacent case still
   finds Monsoon directly.

## Result
Lantern gets the SAME cross-row freedom as T3: place it on any row, set its follow-pair to the IT's
number, and it resolves BOTH the arrangement (IT jacks/state) and the engine detail (Monsoon via the
IT) across rows. Completes the horizontal/vertical scaling story for the visualiser.

## Note: this is the general pattern for ANY future consumer that needs the Monsoon too
Any find-and-read consumer that needs BOTH the IT and the Monsoon should read the Monsoon THROUGH the
IT (it->getHost()), not via its own adjacency scan. Only modules that need the IT alone (like T3) can
rely on the single pair link. Document this so future consumers follow it.
