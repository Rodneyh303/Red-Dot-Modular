# CC PROMPT — INVESTIGATE: Monsoon CPU HIGHER without East (Macro-only 3% vs East-attached 1%)

CONTEXT: Rodney — Monsoon ~1% when East attached (or East+Macro); ~3% with JUST Macro attached. Monsoon
uses MORE CPU with FEWER expanders — backwards from intuition → something does expensive work when East
is ABSENT that East's presence short-circuits. Current origin build (b543a0a). MEASURE FIRST — the
profiler is already in the code; do NOT guess.

## STEP 1 — MEASURE (the profiler exists: MONSOON_PROFILE, already wired)
Monsoon.cpp has MONSOON_PROF section timers (MonsoonProfile.hpp, flag-gated, no-op when off). Build with
-DMONSOON_PROFILE, run in Rack in BOTH configs (Macro-only 3%, East+Macro 1%), read the per-section log.
The bracketed sections include "ctrl_raf" (= updateExpanderPointers + Raffles gate loop, Monsoon.cpp:1399)
and the control-rate sync. Report which section's ns/sample DIFFERS between the two configs — THAT is the
East-absence cost. Do NOT fix until the profile names the section.

## STEP 2 — PRIME SUSPECT (confirm or kill with the profile; do not assume)
updateExpanderPointers (Monsoon.cpp:121, control-rate) contains a pairId self-assign that RE-SCANS THE
WHOLE RACK every control cycle until it latches:
  if (!pairChecked) { for (mid : APP->engine->getModuleIds()) {...clash scan...}; ...; if (pairId>0)
  pairChecked = true; }   // ":145 a capped-out Monsoon keeps retrying"
So if pairChecked NEVER latches (pairId stuck <=0, or a persistent clash), the rack-wide getModuleIds()
scan runs EVERY control cycle = a per-control-cycle O(all modules in rack) cost. HYPOTHESIS: the expander
config (East absent vs present) changes whether pairId latches — East-attached → latches → scan stops →
1%; Macro-only → doesn't latch → scan every cycle → 3%. CONFIRM via the profile (ctrl_raf dominating in
Macro-only) AND by logging pairChecked / pairId in each config (does pairChecked stay false in Macro-only?).
NOTE: this is a HYPOTHESIS from reading — the East-link is NOT proven (pairId is Monsoon-PAIR identity,
not obviously East). The profile + the pairChecked log decide. If ctrl_raf is NOT the dominant diff, the
suspect is wrong — follow the profile to the real section (candidates: expanderManager.update doing more
on a broken chain; a fallback recompute when East's delegation params are absent; effectiveProbability's
MacroOwn path).

## STEP 3 — FIX (only what the profile confirms)
IF the pairChecked-retry scan is confirmed: the bug is pairChecked not latching when assignment is
SETTLED-BUT-UNSUCCESSFUL (capped/no-slot). Fix: latch pairChecked once the assignment is DECIDED (success
OR a stable "not participating" decision), so a settled Monsoon stops re-scanning every cycle — while
preserving the intended "promote automatically when a slot frees" (re-scan on a TOPOLOGY-CHANGE event, not
every control cycle). i.e. drive the re-scan off an expander/rack change signal, not an unconditional
every-cycle poll. Do NOT just latch-and-never-retry (that breaks the promote-on-free feature); gate the
retry on "something changed" instead of "every control cycle".
IF a different section: fix per what the profile shows — the principle is the same (something runs every
cycle on East-absence that should be event-driven or cached).

## CONTRACT / VERIFY
- Profile: the dominant section's ns/sample is EQUAL (within noise) between Macro-only and East+Macro after
  the fix (no East-absence penalty).
- Rack: Monsoon CPU is the SAME (~1%) in both configs; Macro-only no longer 3%.
- The pairId promote-on-free behaviour still works (remove a Monsoon → a capped one still promotes) — test
  or Rack-verify this wasn't broken.
- CPU overall unchanged-or-lower; run_all.sh green.

## DONE =
- Profile identifies the East-absence section (reported in PR).
- That section's per-cycle work is made event-driven/cached so East-absent == East-present CPU.
- promote-on-free preserved. Rack: both configs ~1%. run_all.sh green.
- PR states the measured section + before/after ns and Rack %.

## NOT THIS PROMPT
- The slew fix (done, b543a0a — separate; just needs Rack re-test).
- The strong-types audit findings (OutputGenerator off-by-one, Lantern comment).
- Leaving MONSOON_PROFILE on in the shipped build (flag OFF for release).
