# CC prompt — East-role Distribution: per-voice modulation (channel = voice slot)

Context: `docs/design/SEAMLESS_FOLD_PLAN.md` ("East-role Distribution is voice-poly"). Rodney's
ruling (2026-11-22): the East-role Distribution jacks must be voice-poly irrespective of panel
direction, so every voice can have its OWN parameter modulation. 56 jacks (7 lanes x 8 params),
channel c of a jack = voice slot c. Rack cannot run here: audible/CPU-in-Rack claims are
Rodney's feel-test; the logic below is a Rack-free tested core.

## What exists (read these, verified 2026-11-22)
- Per-voice data plane is ALREADY there: `WarpBank::distOwnDelta[VOICES=16][LANES=7][DPARS=8]`
  (`src/dsp/WarpF.hpp` ~710), written via `Monsoon::setDistOwnDelta(v,l,p,x)` (`Monsoon.hpp` ~1308),
  folded in `resolveWarpCell` (`SandsModifierResolver.hpp` ~669) — reaches only that voice's OWN
  (non-delegated) cells (delegated cells mirror Macro entirely: the standing law, keep it).
- The publisher today: `SandsDistributionExpander::publishOwnMods` (`src/SandsDistributionExpander.cpp`
  ~118): reads `inputs[id].getVoltage()` = CHANNEL 0 ONLY, scales `* 0.1f`, writes ONLY the
  shared-selected (displayed) voice's row (`engine.pe.displayedVoice().v`), zeroes the previous
  voice's row on a voice switch, rebuilds with `rebuildWarpMirrorVoiceLane(slot, l)` per dirty lane,
  change-gated by `kPubEps = 1e-4`, heartbeat `++distPublishSeq[1]` per process() call,
  shadow `lastOwn_[lane][param]`.
- So today one voice is modulated at a time, from channel 0.

## The change
A NEW publisher (do not edit `publishOwnMods`'s behaviour for the existing module — see Q1):
for each jack, for each voice slot v in 0..15:
`delta[v][l][p] = (v < channels(jack)) ? polyVoltage(jack, v) * 0.1f : (channels == 1 ? broadcast : 0)`.
Details to settle and PIN IN TESTS:
- Channel count 0 (unconnected): delta 0 for all voices.
- Channel count 1 (mono cable): see Q2 (default proposal = BROADCAST to all 16 voices; that is
  the standard VCV poly convention).
- Channel count n (2..16): voice v<n gets channel v; voices v>=n get 0 (NOT held, NOT wrapped).
- A cable removed or its channel count shrinking MUST zero the now-unfed voices' deltas and rebuild
  those (voice,lane)s — nothing latches (the standing "publish overwrites, nothing latches" law).
- The voice-switch zeroing logic does not apply to this publisher (the mod no longer follows the
  displayed voice); the editor display reading `distOwnDelta[displayedVoice]` keeps working unchanged.
- Role flip / heartbeat / watchdog: unchanged semantics. Keep the heartbeat bump every process() call
  even when publishing is decimated.
- Shadows become `lastOwn[v][l][p]` (16*7*8 floats = 3.5 KB, fine).

## Structure: a Rack-free core, a thin module marshal
`src/dsp/DistOwnPublishCore.hpp` (pure; no rack.hpp):
- input: `volts[LANES][PARAMS][16]`, `channels[LANES][PARAMS]` (0..16), the shadow array;
  config: broadcast-mono on/off, `eps`.
- output: the list of `(v,l,p,newValue)` writes AND the set of dirty `(v,l)` pairs to rebuild.
- The module's process() only marshals ports -> the struct, applies the writes via
  `setDistOwnDelta`, calls `rebuildWarpMirrorVoiceLane(v,l)` once per dirty pair, and raises/lowers
  `distDeltasActive` through the existing accessors.

## CPU: this is the real risk — measure, do not assume
- Rack calls `process()` PER SAMPLE unless the module divides it. I saw no divider in
  `SandsDistributionExpander::process` (the comments say "per block"); VERIFY what actually runs
  per sample today. Worst case here is 56 jacks x 16 channels = 896 reads per sample, and an LFO on
  a poly cable dirties up to 16 voices x 7 lanes = 112 `rebuildWarpMirrorVoiceLane` calls on every
  publish — that is the cost that matters.
- Required: a CONTROL-RATE publish divider (start N = 32 samples, make it a named constant), with
  the heartbeat still bumped every call. Per-(voice,lane) dirty flags; each dirty pair rebuilt at
  most once per publish. Only scan jacks that are connected (cache the connected/channel snapshot
  and refresh it on port-change events if the SDK offers them, else per publish).
- Add a bench test (not a unit assertion on wall time — print and assert only a generous bound) that
  times the core + `rebuildWarpMirrorVoiceLane` for the worst case: all 56 jacks 16ch, every value
  changing every publish. Report: microseconds per publish, and the implied % of a 48 kHz
  sample budget at N = 16 / 32 / 64. If worst case is not comfortably under budget, say so and
  propose the mitigation (larger N, per-lane round-robin, a coarser `kPubEps` for per-voice mod)
  instead of shipping it.

## Tests — `test/test_dist_own_publish.cpp` (register in `run_all.sh`; failing first)
Independent oracle: expected `distOwnDelta` computed directly from the stated rule above (a tiny
loop in the test), not by calling the core.
1. 16ch cable on one jack, distinct volts per channel: each voice gets its own `volts/10`,
   other jacks/lanes untouched.
2. Mono cable: broadcast to all 16 (or per the Q2 ruling).
3. n-channel cable, n=5: voices 5..15 are 0; then n=3: voices 3,4 zeroed AND rebuilt (nothing latched).
4. Cable removed: all 16 zeroed; `distDeltasActive` falls to false when everything is zero.
5. Change gate: an unchanged block writes nothing and rebuilds nothing (count calls); a sub-`eps`
   change writes nothing; one voice changing dirties only that (v,l) pair.
6. End-to-end through the warp bank: with `distOwnDelta` published for voice 3 only, voice 3's
   resolved cell moves and voices 0..2,4..15 resolve bit-identically to the zero-delta path;
   delegated cells of voice 3 still mirror Macro (the standing law). Use the existing warp-bank test
   harness (`test_warp_f_bank` and the resolver oracle).
7. Zero jacks connected: no writes, byte-identical to today's idle state.
Mutation-check: (a) channel index off by one -> 1 fails; (b) no zeroing of unfed voices -> 3 fails;
(c) broadcast always on -> 3/5 fail; (d) drop the dirty-pair dedupe so a pair rebuilds twice -> 5 fails;
(e) delete the delegated-cell guard -> 6 fails. Show red first, then green, then the full suite.

## Open questions (Rodney's calls — default proposals in brackets; do NOT decide silently)
- Q1. Which module gets the per-voice publisher? [the NEW fold model only; the existing
  `SandsDistributionExpander` keeps today's displayed-voice/channel-0 behaviour byte-identical,
  consistent with "old modules are not deleted/changed". Alternative: also upgrade the old module,
  which changes how existing patches behave: a poly cable on an old East Distribution jack today
  modulates only the displayed voice from channel 0.]
- Q2. Mono cable on an East-role jack: [broadcast to all voices (standard poly convention). Note
  this differs from today's "mono cable modulates the displayed voice only".]
- Q3. Control-rate divider value N, after the bench numbers.
- Q4. Voice count: slots beyond the engine's active voice count — store and ignore, or skip the
  rebuild? [store and skip the rebuild for inactive voices; zero them when they deactivate.]

## Out of scope (separate prompts)
- The Macro-role packing (8 poly inputs, channel = lane): same pattern with `publishMacroSends`,
  but a different mapping; the core's structure should make it a sibling, not a rewrite.
- The depth knobs/attenuverter store (56 new values) and the Distribution band UI.
- The fold model's port declaration. Today's comment "42" in `SandsDistributionExpander.hpp` is
  stale (56 ids: 0..41 frozen + 42..55 appended); fix that comment if you touch the file.

## Report back
Red/green + mutations; the per-sample-vs-block finding; the bench numbers; the answers or
recommendations for Q1-Q4; and anything in the existing publish path you found that this core
must not break.
