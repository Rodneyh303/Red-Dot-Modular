# CC PROMPT  Scale mask arbiter review (microtonal width)

Authority: src/dsp/ScaleMaskArbiter.hpp (resolveScaleMask, rotateMask12), src/dsp/managers/MonsoonScaleManager.*,
src/tuning/ScalaFile.hpp, TuningPreset.hpp, TuningTable.hpp. Build on lane-expander-refactor.

## WHAT IS THERE (verified)
- resolveScaleMask: priority override > authored > factory > all-12, returning a 12-bit mask (0x0FFF).
- rotateMask12: root-relative rotation of a 12-bit mask, modulo 12.
- The scala loader and the tuning table are not 12-limited: a .scl can have up to the expander's capacity of degrees.
- ColonnadesDuo states a 24-tone row ("honest to arbitrary 24-tone .scl, no 12+12 split").

## THE QUESTION (settle FIRST, before any change)
A 12-bit mask cannot represent a degree above 12. If a tuning has N > 12 degrees, the arbiter silently truncates
degrees 13..N. Decide the mask DOMAIN:
  (a) WIDTH = tuning degree count N (up to 24 or the expander's capacity). Masks become uint32 with N bits; rotation is
      modulo N. The 12-EDO case is N = 12 and must be bit-identical to today.
  (b) 12-EDO gate only. The 12-bit mask stays the pitch-class gate; microtonal degrees are gated through the tuning
      table's enabledState and never through the arbiter.
Report which the code does today, and recommend one. Do not change code until the domain is decided and written here.
Recommendation to evaluate: (a), since (b) splits the authority that this file exists to prevent.

## TASK 1  Make the arbiter width-aware (only after the decision)
- resolveScaleMask takes the tuning width N and returns an N-bit mask. Priority order UNCHANGED.
- rotateMask generalised to modulo N; rotateMask12 kept as the N = 12 case so existing callers are unchanged.
- Every mask stored or passed (authored, factory, override) carries its width, or the width is checked on read.

## TASK 2  Consistency
- ScaleManager and the engine read the SAME resolveScaleMask result. One arbiter; no second copy of the priority logic.
- The .scl short-file rule: degrees beyond the file's length are disabled (their bits 0). Test with a short and a long file.

## TASK 3  Tests (test/test_ScaleMaskArbiter.cpp)
- Priority: override beats authored beats factory beats all; each valid flag respected.
- N = 12: every result bit-identical to the current 12-bit arbiter (regression gate).
- N = 24: bits 12..23 survive resolve and rotation; a degree-13 bit is NOT truncated.
- Rotation: rotate by k modulo N for N in {12, 24}; rotate then inverse rotate is identity.
- Short .scl: degrees past the file length are 0 in the resolved mask.

## GATES
- run_all green on both SDK legs.
- N = 12 path bit-identical to today (no re-pins of existing scale goldens).
- No change to the priority order, the all-12 default, or Shophouse's override semantics.
- One arbiter: grep confirms no second implementation of the priority logic.

## DO NOT
- Change the priority order or the Shophouse override relationship.
- Add a new tuning file format. Use the existing .scl and .dmtune readers.
- Merge the feat/microtonal branch's doc and panel commits here. Separate those in another PR.
