# Handoff: test_12tet_identity + a repo-hygiene bug found while writing it

## Two things, one urgent

### URGENT: src/tuning/TuningTable.hpp is NOT committed to the repo
While writing the 12-TET test, found that `src/tuning/TuningTable.hpp` (and possibly the whole
src/tuning/ dir) is included by PatternEngine.hpp (line 28: `#include "../../tuning/TuningTable.hpp"`)
and referenced by test_TuningTable / test_TuningList / test_TuningRoundTrip, but is NOT in the git tree
at HEAD (`git cat-file -e HEAD:src/tuning/TuningTable.hpp` -> NOT in HEAD tree; not gitignored, just
never `git add`ed).

Consequence: a FRESH CLONE of feat/microtonal does not compile -- the header is missing. It builds on
Rodney's machine only because the file exists locally. Since CC re-clones each session (filesystem
resets), CC must:
1. Verify which files under src/tuning/ are untracked (`git status --short src/tuning/` on Rodney's
   working tree, or `git ls-files src/tuning/`).
2. `git add` and commit TuningTable.hpp and any sibling tuning headers/sources the build needs.
3. Confirm a clean clone compiles.
This is almost certainly why any from-scratch build/test attempt of the tuning code would fail.

### The test: test_12tet_identity.cpp (needs CC compile-verify)
Written but NOT compile-verified in the container -- BECAUSE of the missing header above (couldn't
build here). CC must compile + run it once the header is committed, and adjust the exact TuningTable
API calls if they differ from the assumptions.

## What the test pins (and why this scope, not a whole-engine golden master)
The 12-TET promise lives in the DETERMINISTIC tuning mapping, NOT the RNG-driven step cascade:
- RNG (Philox) is tuning-independent and already covered by test_PhiloxRng. Tuning doesn't touch it.
- Every engine test in this repo MIRRORS the executeStep cascade (replica-style) rather than driving
  the real engine standalone -- a whole-engine golden master would fight that harness design.
So the test pins the two pure functions where 12-TET can diverge from legacy:
  (1) voltage->degree: SequencerEngine::degreeOf. At isDefault12TET it must EXACTLY equal legacy
      `round(frac*12)%12`. Verified by sweeping voltages across octaves 0..6 and comparing to the
      legacy formula (integer result -> exact).
  (2) degree->voltage: at 12-TET (cents[i]=i*100) degree i must map to EXACTLY i/12 volts, BIT-EXACT.
      Verified via round-trip (degreeOf(legacyVoltage(d))==d) AND bit-equality of the fractional part
      to (float)d/12.
"Bit-exact" is the actual promise -- the test compares raw IEEE-754 bits, not rounded values.

## Wired into the harness
- run_all.sh: added `-Isrc/tuning` to INCS (was missing -- another reason from-scratch builds fail),
  and registered `"test_12tet_identity|$SE $GS $PE"` (links SequencerEngine + GateState + PatternEngine).

## CC checklist
1. Commit src/tuning/ headers (the urgent bug) so a clean clone builds.
2. `test/run_all.sh test_12tet` -> compile + run the new test.
3. If the TuningTable degree->voltage API differs from the test's assumptions (it uses degreeOf +
   legacy-voltage round-trip to avoid guessing a getter name), adjust to call the real read.
4. Confirm PASS at current 12-TET, then this test guards every future engine change: if M1 (engine
   widening) or anything else ever perturbs 12-TET pitch mapping, it fails loudly.
5. Consider adding to CI / the pre-merge test gate so 12-TET identity can never silently regress.

## Why this matters (the point Rodney made)
"Assuming we achieve identical 12-TET" should be "until we've PROVEN identical 12-TET." This test
converts the assumption into a checked fact -- the load-bearing wall the whole microtonal V1 stands on.
