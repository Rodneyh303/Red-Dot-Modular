# Fresh-clone build status (feat/microtonal) -- one real blocker

Checked a FRESH shallow clone of feat/microtonal for unresolved includes (the recurring uncommitted-
header bug). Container can't run the Rack SDK build, but CAN verify every live #include resolves to a
committed file -- which is exactly the class of bug that's bitten repeatedly.

## RESULT: fresh clone does NOT build -- 1 real blocker

### BLOCKER: src/ScaleMaskArbiter.hpp is NOT committed (3rd instance of this bug class)
- Included LIVE (uncommented) by src/dsp/managers/MonsoonScaleManager.hpp:6 and .cpp as
  "../ScaleMaskArbiter.hpp" -> resolves to src/ScaleMaskArbiter.hpp.
- It is the rotateMask12 / normaliseToTonic helper from the TONIC_TRANSPOSE work (CC built it).
- `git ls-files | grep ScaleMaskArbiter` -> ZERO matches. Absent from the tree, absent from a fresh
  clone, only on Rodney's local machine. NEVER git add-ed.
- Consequence: MonsoonScaleManager fails to compile on a clean clone -> whole plugin fails to build.
- FIX: CC must `git add src/ScaleMaskArbiter.hpp` and commit it.

### This is the THIRD uncommitted-header of this exact class
- TuningTable.hpp (fixed earlier), TuningList.hpp (fixed earlier), now ScaleMaskArbiter.hpp.
- Pattern: a header written locally, referenced by committed code, never added. Invisible while
  building on the machine that has the local file; breaks every fresh clone / CI / other user.
- ONE-TIME SWEEP (do this to close the class, not fix one-by-one): on Rodney's working tree run
  `git ls-files --others --exclude-standard 'src/**/*.hpp' 'src/**/*.cpp'` -- lists every untracked
  source file. Commit the ones referenced by live code. This is the recurring gap; a single sweep ends it.

## NOT blockers (false alarms from a first crude scan -- for the record)
- Retired expander headers (MonsoonSandsExpander, MonsoonStraitWestExpander, MonsoonStraitsSands,
  MonsoonDeepStraitsSands, etc.): included ONLY in COMMENTED-OUT lines in Monsoon.cpp (//#include).
  Not compiled. Their bodies live (committed) in src/deprecated/. Not a build issue.
- rack.hpp: the Rack SDK shim -- correctly absent in a plugin clone, provided by the SDK at build. Not
  a bug.

## Recommendation
1. Commit ScaleMaskArbiter.hpp NOW (unblocks the clean build).
2. Run the one-time untracked-source sweep to catch any 4th instance before it surfaces.
3. Then a fresh clone should have all includes resolving; CC should do ONE actual Rack build from a
   clean clone to confirm (the container can't -- this check only proves includes resolve, not that the
   full SDK compile succeeds).

Note: this check verifies INCLUDES RESOLVE, not that the Rack build fully succeeds (container has no
SDK). A clean SDK build by CC is still the final confirmation -- but ScaleMaskArbiter.hpp is a
guaranteed failure until committed.
