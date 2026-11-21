# CC PROMPT — MEASURE: Monsoon 3% with Macro-ONLY vs 1% with East+Macro (reframed; SweepSig ruled out)

CONTEXT REFRAME (Rodney): the "0.4%/1%" good perf earlier today was observed with EAST+MACRO attached
(he was testing delegation). He never saw Macro-ONLY at 0.4% — so Macro-only being 3% may NOT be a
regression; it may have been 3% all along. The symptom is: **MONSOON's own process() uses MORE CPU when
East is ABSENT** (Macro-only 3%, East+Macro 1%). The SweepSig gate restore (feat/restore-sweepsig-gate,
a94a8bf) did NOT fix Macro-only → the cost is NOT the prob-out sweep → look in MONSOON::process, not the
Macro-module sweep. This is the backwards "more CPU with fewer expanders" signature.

## STOP GUESSING — MEASURE (the profiler is already in Monsoon::process)
Monsoon.cpp has MONSOON_PROFILE section timers with FINE control-rate brackets already:
  ctrl_raf  = updateExpanderPointers + Raffles gate loop
  ctrl_pat  = modeController->updatePatternInput
  ctrl_dna  = spreadTargetMode mirror + dnaManager.processDNA
  ctrl_sync = lockManager.tick + expanderManager.sync
  ctrl_rest = cached* reads + CV logic + updateWindow + CV3/Raffles tail
plus per-sample sections (head, clockPhase, gatesTrig, shophouse, stepExec, ...).
TASK: build -DMONSOON_PROFILE, run in Rack in BOTH configs (Macro-only 3%, East+Macro 1%), read the
section log. Report which section's ns/sample DIFFERS between the two. THAT section is the East-absence
cost. One run answers it. Do NOT fix before the profile names the section.

## SUSPECTS (confirm/kill with the profile — do NOT assume)
Most likely in ctrl_raf or ctrl_sync (the expander/topology control-rate work):
1. pairChecked rack-scan (Monsoon.cpp:131): re-scans ALL module IDs every control cycle UNTIL pairId
   latches (":145 capped-out keeps retrying"). If East-absence somehow keeps pairChecked false → scan
   every cycle. BUT pairId is Monsoon-PAIR identity, not obviously East-linked — the profile + a
   pairChecked/pairId log per config decide.
2. resolveFollowedT (Monsoon.cpp:153, if followCA>0): a rack-wide scan at control rate.
3. updateExpanderPointers / expanderManager.update / findMonsoonEitherSide chain walks: an expander-
   chain walk that is LONGER or re-walks when a link (East) is absent (a missing cache that keeps
   re-resolving). The chain-walk memoization (4e51501) may not cover the East-absent case.
4. A fallback recompute somewhere that fires when East's delegation params are absent.
If a per-sample section (not ctrl_*) dominates in Macro-only, follow THAT instead.

## FIX (only what the profile confirms)
The principle regardless of which section: something runs every cycle on East-ABSENCE that should be
event-driven or cached. Make it config-robust — East's absence must not trigger extra per-cycle work
(a Macro-only patch is a NORMAL config, not a degraded one). Preserve any feature the current code has
(e.g. pairId promote-on-free: drive its re-scan off a topology-change event, not an every-cycle poll).

## CONTRACT / VERIFY
- Profile: the identified section's ns/sample EQUAL (within noise) Macro-only vs East+Macro after fix.
- Rack: Monsoon ~1% in BOTH configs; Macro-only no longer 3%.
- Any feature touched (pairId promote-on-free, CA follow) still works.
- run_all.sh green; MONSOON_PROFILE flag OFF for release.

## DONE =
- Profile report: which section differed, ns before/after, both configs (in the PR).
- That section made config-robust (East-absent == East-present CPU).
- Rack: both configs ~1%. run_all.sh green.

## NOT THIS PROMPT
- The SweepSig gate (done, a94a8bf — it was the East+Macro perf, a different thing; ruled out for Macro-only).
- Slew (separate, needs Rack re-test). Strong-types audit findings (OutputGenerator off-by-one, Lantern).
