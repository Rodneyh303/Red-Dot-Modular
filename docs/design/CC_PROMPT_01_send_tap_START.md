# CC PROMPT 01 — START HERE: per-voice send-tap fix (test-first)

You are Claude Code working on dot.modular / Monsoon (VCV Rack v2, C++17). This is the
FIRST build task of Phase 3. It is a self-contained bug fix that ships ALONE and FIRST,
before any warp/distribution code, because it hardens a publish contract later work reuses.

## Ground rules (read, do not skip)
- Branch: `git checkout lane-expander-refactor && git pull`, then cut
  `git checkout -b fix/per-voice-send-tap`. Work there. NOT master, NOT lane-expander-refactor
  directly.
- VERIFY AGAINST CODE AND TESTS, never a plausible narrative. If you "fix" something, prove it
  with a test that FAILS before and PASSES after. Do not claim a behavioural fix you cannot
  run. (Hard-won project lesson: confident-but-wrong "fixed" claims cost weeks.)
- You CANNOT test Rack behaviour. Everything you touch here is pure/testable without Rack —
  keep it that way. Rodney feel-tests in Rack after.
- The FULL SPEC is docs/design/CC_PROMPT_per_voice_send_tap.md. READ IT FIRST, in full. This
  file is the ordering + the exact test/build wiring; that file is the what-and-why.

## The bug (one line)
Macro→East SEND is per-voice (`getMacroSend(slot,lane,item)`, `macroSend[448]` in
src/Monsoon.hpp) but the PRE/POST TAP that shapes it is Macro-GLOBAL (`getGlobalTap(lane,which)`,
`globalTap[14]`). So every receiving voice is forced onto one tap. Make the tap per-voice,
exactly like the send. Send/tap are authored on MACRO (per-voice via Macro's selected tab)
and READ by East — East never writes them.

## DO IT IN THIS ORDER (test-first; each step must build + the suite stay green)

### STEP 0 — reproduce the bug as a FAILING test (before changing any src)
Write `test/test_send_tap.cpp`, HEADER-ONLY, modelled EXACTLY on test/test_spread_resolver.cpp
(same TEST macro, same includes style; it includes only "SpreadResolver.hpp"). Drive the
pre/post lerp math through SpreadResolver (the pure fn that already carries the send delta).
Factor the per-voice selection into a pure free function if it isn't reachable yet — it MUST
be unit-testable without Rack.

The 4 asserts (full detail in the spec file):
  1. BUG: atten att≠1, two voices equal send, OPPOSITE taps ⟹ blend_A ≠ blend_B.
     (This FAILS on the current global-tap code — that failure is the point of step 0.)
  2. IDENTITY: att==1 ⟹ blend_A == blend_B (pre/post coincide at unity atten).
  3. ENDPOINTS: tap=1 == old POST (cv·att); tap=0 == PRE (raw cv).
  4. PERSISTENCE: per-voice taps {0.0,0.3,1.0} round-trip save→load; a patch with NO macroTap
     key loads all voices at 1.0 (POST, preserves old patches).

Register it in test/run_all.sh TESTS[] as a HEADER-ONLY entry (no companion .cpp), matching
line 107's form exactly:
    "test_send_tap|"
Run `bash test/run_all.sh test_send_tap`. Assert 1 must FAIL now. Commit this as
"test: send-tap per-voice (red)" so the red state is in history.

### STEP 1 — store: globalTap[14] → per-voice macroTap[224]
In src/Monsoon.hpp, mirror the macroSend shape:
    float macroTap[224] = {0};              // 16 × 7 lanes × 2 kinds (0=LOR items0-2, 1=SPR item3)
    getMacroTap(v,lane,which) / setMacroTap(v,lane,which)   // index (v*7+lane)*2+which
KEEP globalTap[14] + getGlobalTap/setGlobalTap as the Macro-panel DELEGATION DEFAULT that
seeds per-voice values. Default per-voice tap = 1.0 (POST). Do NOT delete globalTap yet.

### STEP 2 — publish BOTH endpoints (the non-mechanical heart)
Per-voice tap moves the PRE/POST choice DOWNSTREAM of Macro's publish, so Macro can no longer
bake one tapped delta. In src/dsp/managers/MonsoonSandsManager.cpp publishGlobal/tappedDelta
(~L631-644): publish TWO arrays on the macro visual instead of the single macroSendDelta:
    macroPreDelta [7][4]   = raw cv·(hi-lo)        (tap=0 endpoint)
    macroPostDelta[7][4]   = cv·att·(hi-lo)        (tap=1 endpoint)
Both are the two ends of the EXISTING lerp effAtt=1+(att-1)·tap — just emit the endpoints.
Leave macroCVDelta (Macro's own POST display) untouched. Do NOT delete macroSendDelta until
STEP 3 migrates every reader.

### STEP 3 — every reader selects per-voice (WHERE A MISS SILENTLY COMPILES)
At each blend site, replace `macroSendDelta[lane][item]` with the per-voice lerp:
    float tap = getMacroTap(slot, lane, (item==3?1:0));
    float d   = macroPreDelta[lane][item] + (macroPostDelta[lane][item]-macroPreDelta[lane][item])*tap;
    // then blend = d * send_v   (as before)
EXHAUSTIVE reader list (grep-verified; the spec file enumerates by line — if you find more,
migrate them too):
  - src/dsp/managers/MonsoonExpanderManager.cpp  (blend sites + the spR/spM/… display rows)
  - src/dsp/managers/MonsoonSandsManager.cpp      (baseLen/Off/Rot, the ·send sites, Inputs fill)
  - src/StraitsEastSandsVisual.cpp
  - src/StraitsSandsMacroVisual.cpp  (:405 — Macro's OWN display)
  - src/dsp/SpreadResolver.hpp         (Inputs field + effective())
*** DISTINCTION — get this right: display-of-MACRO (StraitsSandsMacroVisual.cpp:405, and the
    Sands-manager rows that show Macro's own value) uses macroPostDelta, NOT a per-voice lerp —
    Macro has no receiving voice. blend-into-a-VOICE uses the per-voice lerp. ***
Then DELETE macroSendDelta[7][4] and its declaration.

### STEP 4 — persistence
macroTap[224] round-trips in MonsoonPersistenceManager alongside macroSend. Missing key ⟹
default 1.0 (POST) for every voice (old patches sound identical). This is what assert 4 checks.

### STEP 5 — UI (LAST; it's the only part you can't fully test)
Per-voice tap control on MACRO (authored via Macro's tab — NOT on East). It's a CONTINUOUS
[0,1] knob (PRE↔POST lerp), not a toggle. Follow the per-voice SEND knob pattern already in
the Macro Sands UI. Macro panel keeps its global-tap trimpots as the delegation default.
Store-backed (StoreKnob), not host-exposed.
IF this needs a new panel anchor: add it in panel_src/gen_macro_mono.py AND bind it by the
same name in the widget AND add its family to test/audit_anchor_bind.py `counts` IN THIS SAME
STEP (the audit catches anchor↔bind divergence automatically but a wrong/absent count only if
you added it). Run `python3 test/audit_anchor_bind.py` — sands_macro must stay green.

## DONE =
- test/test_send_tap.cpp: 4 asserts, was RED at step 0, now GREEN; registered in run_all.sh.
- macroSendDelta deleted; macroPreDelta/macroPostDelta published; every reader migrated with
  the Macro-display vs voice-blend distinction correct.
- globalTap kept as default; macroTap[224] per-voice, persisted, defaults 1.0.
- Macro UI exposes per-voice tap; audit_anchor_bind green if any anchor changed.
- `bash test/run_all.sh` ALL GREEN. No Phase 3 / warp / distribution code in this branch.
- Open a PR to lane-expander-refactor. In the PR body, state which reader sites you migrated
  and paste the test_send_tap red→green output.

## What NOT to do
- Do NOT widen LaneDist, add warp, w, mirror, or distribution anything — that's a later prompt.
- Do NOT delete globalTap or macroSendDelta before their replacements are wired + green.
- Do NOT claim the UI "works" — you can't test Rack. Say "UI added, needs Rack feel-test."
- Do NOT touch master or push straight to lane-expander-refactor.
