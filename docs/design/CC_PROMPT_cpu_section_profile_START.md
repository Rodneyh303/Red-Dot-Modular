# CC PROMPT — SECTION-PROFILE process() to find the clock-mode CPU fire (Monsoon 8%, Macro 124%)

CONTEXT: CPU regressed from ~1% to Monsoon 8% / Macro 124%, observed in CLOCK MODE. Multiple
read-based guesses (chrono-removed, tickPulse gated, rebuild gated, drive() no heavy calls) have NOT
found it. STOP guessing — MEASURE. This prompt adds CHEAP section timers so one Rack run names the hot
section. Work on lane-expander-refactor.

## THE SAFE PROFILING PATTERN (do NOT repeat the per-sample chrono that CAUSED the 124%)
The earlier bug was std::chrono::now() called PER SAMPLE. The profiler must NOT do that. Pattern:
- Accumulate elapsed ns per SECTION into static accumulators, but only call now() AROUND SECTIONS
  (bracketing), and only LOG at a control-rate counter (every ~50000 samples). The now() calls still
  run per sample (bracketing each section), so to keep the profiler's OWN cost low: use rdtsc / a cheap
  monotonic counter if available, OR accept the profiler adds some overhead but READ THE RELATIVE
  SHARES (which section dominates), not absolute numbers — the DOMINANT section is the target regardless
  of profiler overhead. REMOVE the profiler once the hot section is found (it is diagnostic scaffolding,
  not permanent).
- Guard it behind a compile flag: #ifdef MONSOON_PROFILE ... #endif, so it is trivially removable and
  never ships.

## WHAT TO BRACKET (the major sections of Monsoon::process, 678-1494)
Wrap each in a MONSOON_PROFILE accumulator (name → ns):
  - "clock/phase"  : clock.process + phase.process block (~730-760)
  - "gates/trig"   : the gate3/trig edge-detection block
  - "shophouse"    : the expanderManager.cachedShophouseExpander blocks
  - "step-exec"    : the main step-execution block (shouldExecute → executeStep/poly) (~920-1010)
  - "outputGen"    : outputGenerator->drive(...) (line 1151 — RUNS PER SAMPLE, prime suspect)
  - "lightDivider" : the lightDivider.process() block
  - "controlDivider": the controlDivider.process() block (1347+)
  - "tail"         : anything after
Log at control rate: if (++n % 50000 == 0) INFO("PROFILE ns/sample: clock=%.1f step=%.1f outputGen=%.1f
control=%.1f ...", accClock/n, accStep/n, accOut/n, ...); then it is OBVIOUS which dominates.

## ALSO PROFILE THE MACRO MODULE (the 124% is a DIFFERENT module)
The Macro 124% is NOT Monsoon — it is the Macro/Sands module's own process(). Add the SAME
MONSOON_PROFILE bracketing to the Macro module's process() (find it — likely StraitsSandsMacroVisual
or its module class). PRIME SUSPECT there: the macroOwnLaneProbability × (lanes × channels) output
sweep (CC_PROMPT_macro_output_east_leak added it; its own comment at StraitsSandsMacroVisual.cpp:845
says "5 lanes × (1+numPolyVoices) channels × macroOwnLaneProbability — each call" — if that sweep runs
PER SAMPLE and macroOwnLaneProbability does SpreadInterp (SequencerEngine.hpp:756-766, it does), that
is the 124%). Bracket the Macro output sweep specifically.

## HYPOTHESES THE PROFILE WILL CONFIRM OR KILL (don't assume — measure)
1. outputGenerator->drive runs per-sample (confirmed structurally) — IS it heavy? Profile says.
2. Macro macroOwnLaneProbability sweep per-sample doing SpreadInterp per channel = the 124% (strong).
3. Something ungated in process() between the dividers. Profile the sections, the big one shows.

## DONE =
- MONSOON_PROFILE section timers in Monsoon::process AND the Macro module process (compile-flag guarded).
- PR says: "Rodney — build with -DMONSOON_PROFILE, run in Rack CLOCK MODE, read the log, tell me which
  section dominates for Monsoon (the 8%) and for Macro (the 124%)." That section is the fix target.
- No functional change; profiler removable. bash test/run_all.sh green.

## NOT THIS PROMPT
- Fixing the hotspot (do that AFTER the profile names it — targeted, not guessed).
- The rate manager / resolver-UI structural work (that is the long-term cure; this is the fire).
