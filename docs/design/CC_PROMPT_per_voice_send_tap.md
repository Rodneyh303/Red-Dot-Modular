# CC TASK — Per-voice send tap (fix global-tap regression)

Branch: cut `fix/per-voice-send-tap` **off `lane-expander-refactor`**. Not master. Do NOT
start Phase 3 work on top of this; this ships alone, with its test, first.

## THE BUG (one sentence)
The Macro→East **send** is per-voice (`getMacroSend(slot,lane,item)`), but the **PRE/POST
tap** that shapes what the send carries is Macro-global (`getGlobalTap(lane,which)`,
`globalTap[14]`). So every voice receiving a lane's send is forced onto the *same* tap.
Given the send is per-voice, a global tap on it has no design rationale — it is a
regression from SANDS CONSOLIDATION Step 1. Make the tap per-voice, exactly like the send.

## WHAT THE TAP IS (read before touching anything)
Publish site: `src/dsp/managers/MonsoonSandsManager.cpp`, lambda `tappedDelta` (~L631-638):

    effAtt = 1 + (att - 1)*tap          // tap∈[0,1] LERP, NOT a toggle
    delta  = cv * effAtt * (hi - lo)

tap=1 (POST) → `cv*att*(hi-lo)` (attenuated). tap=0 (PRE) → `cv*(hi-lo)` (raw, pre-atten).
The two endpoints are the **pre** and **post** deltas. The current code bakes Macro's single
global tap into one published `macroSendDelta[lane][item]`, so the choice is made upstream of
the per-voice send and cannot vary by voice.

## THE FIX — the non-mechanical heart is the PUBLISH CONTRACT
Because per-voice tap moves the pre/post choice **downstream** of the publish, Macro can no
longer pre-apply the tap. It must publish **both endpoints** and let each voice select.

### 1. Publish contract (MonsoonSandsManager.cpp publishGlobal / tappedDelta)
Replace the single `macroSendDelta[lane][item]` publish with TWO arrays on the macro visual:

    macroPreDelta [7][4]   // raw cv * (hi-lo)          — tap=0 endpoint
    macroPostDelta[7][4]   // cv * att * (hi-lo)        — tap=1 endpoint

Compute both unconditionally at the publish site (both are already derivable in `tappedDelta`
— just emit the two endpoints instead of the lerp). Keep `macroCVDelta` (Macro's own POST
display) exactly as is — it is unaffected.
DELETE `macroSendDelta[7][4]` only AFTER every reader below is migrated (step 3).

### 2. Store: globalTap[14] → per-voice, mirror macroSend's shape
`macroSend` is the exact template to copy (`src/Monsoon.hpp`):

    float macroSend[448] = {0};                         // 16 × 7 × 4
    getMacroSend(v,lane,item) = macroSend[(v*7+lane)*4+item]

Add alongside it:

    float macroTap[224] = {0};                          // 16 × 7 lanes × 2 kinds (LOR, SPR)
    getMacroTap(v,lane,which)  // which: 0=LOR(items 0-2), 1=SPR(item 3)
    setMacroTap(v,lane,which)

KEEP `globalTap[14]` + `getGlobalTap`/`setGlobalTap` for now as the Macro-panel DEFAULT that
seeds per-voice values on unlock (same delegation pattern as send/atten). Do not delete it.
Default per-voice tap = 1.0 (POST) to preserve current behaviour for un-touched patches.

### 3. Every reader selects per-voice (THIS IS WHERE A MISS SILENTLY COMPILES)
At each site, replace the direct `macroSendDelta[lane][item]` read with a per-voice lerp:

    float tap_v = getMacroTap(slot, lane, (item==3?1:0));
    float delta = macroPreDelta[lane][item]
                + (macroPostDelta[lane][item] - macroPreDelta[lane][item]) * tap_v;
    // then, as before: blend = delta * send_v;

ALL readers that MUST migrate (verified via grep — if you find more, migrate those too):
  - src/dsp/managers/MonsoonExpanderManager.cpp: L293, L321, L665-669, L718-722
  - src/dsp/managers/MonsoonSandsManager.cpp:    L231-233, L283, L362, L505, L549-552
  - src/StraitsEastSandsVisual.cpp:              L134, L1156, L1258, L1369
  - src/StraitsSandsMacroVisual.cpp:             L405  (Macro's OWN display)
  - src/dsp/SpreadResolver.hpp:                  field `macroSendDelta` (L64,76,81) +
                                                 every caller that fills `Inputs`

  *** Macro's own display (StraitsSandsMacroVisual.cpp:405) and the Sands-manager
      display sites that show MACRO's own value (not a voice's) must use macroPostDelta
      (== the old true-POST display). They are NOT per-voice. Do not lerp those with a
      voice tap — they show Macro, which has no receiving voice. Get this distinction
      right: display-of-Macro = POST; blend-into-voice = per-voice lerp. ***

### 4. SpreadResolver (tested pure fn — the clean test seam)
`src/dsp/SpreadResolver.hpp` carries `macroSendDelta` as an `Inputs` field. Replace it with
`macroPreDelta` + `macroPostDelta` + `macroTap`, and do the lerp inside `effective()` so the
dependence of the result on the per-voice tap is unit-testable without Rack.

### 5. UI: per-voice tap control on East's receive side
Parallel the per-voice SEND knobs already in the East Sands UI. Per-voice tap is a float
[0,1] (continuous PRE↔POST), so a small trimpot or a store-knob like the send. The Macro
panel keeps its global tap trimpots as the delegation default. Store-backed (StoreKnob),
not host-exposed, same as send/atten.

### 6. Persistence
`macroTap[224]` round-trips like `macroSend`. Add to `MonsoonPersistenceManager`
save/load alongside the send bank. Old patches (no macroTap key) must load with tap
defaulting to 1.0 (POST) so existing patches sound identical.

## CONTRACT TEST (write FIRST, must fail on current build, pass only when done)
`test/test_send_tap.cpp` — pure, no Rack. Drive `SpreadResolver::effective` (and/or the
publish+select math factored into a testable free function):

  Setup: one lane, Macro CV present, attenuverter att ≠ 1 (e.g. 0.5), two voices with
         EQUAL send gain, OPPOSITE taps (voiceA tap=0 PRE, voiceB tap=1 POST).

  ASSERT 1 (the bug):  att ≠ 1  ⟹  blend_A ≠ blend_B
                       (fails today: global tap forces them equal)
  ASSERT 2 (identity): att == 1 ⟹  blend_A == blend_B
                       (PRE and POST coincide when atten is unity — sanity bound)
  ASSERT 3 (endpoints): blend at tap=1 == old tappedDelta(tap=1); tap=0 == raw cv term
                       (proves the lerp endpoints match the pre-regression semantics)
  ASSERT 4 (persistence): save→load a patch with per-voice taps {0.0, 0.3, 1.0} across
                       3 voices → all three survive exactly; a patch with NO macroTap key
                       loads all voices at 1.0.

Wire into the existing test runner (same harness as test_voice_conventions.cpp /
test_distribution.cpp). CI must be green.

## DEFINITION OF DONE
- macroSendDelta DELETED; macroPreDelta/macroPostDelta published; every reader migrated.
- globalTap retained as delegation default; macroTap[224] per-voice store added + persisted.
- East UI exposes per-voice tap.
- test_send_tap.cpp: 4 asserts, fails on HEAD~1, passes on HEAD, CI green.
- NO Phase 3 / warp / distribution code in this branch. This is the tap fix ONLY.

## WHY THIS ORDER (for the human reviewer, not CC)
This hardens the publish-both-endpoints contract that the forthcoming warp-depth `w` send
and per-param sends will inherit for free. Building Phase 3 on an unfixed single-delta
publish means redoing the contract later under more code. Fix the seam first.
