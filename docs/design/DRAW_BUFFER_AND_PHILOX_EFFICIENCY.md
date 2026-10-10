# Draw buffering, bidirectional caches, Philox efficiency (reverse mode + 32 steps)

Conclusions from working through the MA/copula draw model, reversibility, and Philox usage. Applies when
expanding to 32 steps and implementing phase/reverse mode.

## The draw model (confirmed from code)
- Each DRAW at a position is a MULTIVARIATE raw draw: all lanes x voices x steps, drawn as one unit
  (that's what correlates them). You cannot cheaply draw "fewer voices/steps" — a draw IS the full vector.
- **MEMOISED = RAW draws + z (= PhiInv(raw))** — position-pure (Philox-addressable), REVERSIBLE,
  SLEW-INDEPENDENT. NOT copula-applied. (CachedRhythmDraw holds `draw` + `z[...]`, PatternEngine.hpp:729.)
- **Copula/slew (r) applied at READOUT, at CONTROL RATE** (recomputeEffective*, applyZ=sumZ+PhiFast,
  reads memoised z + current r). "r enters ONLY at readout, never the chain" (SLEW_COPULA_PLAN.md:44) ->
  the chain reverses exactly; copula updates at control rate on the SAME memoised z.
- The MA window for step i = step i's value across the last K DRAWS (per-step history across multivariate
  draws), K=64. Window is RE-READ from the current index each readout (cachedRhythmDraw(N-f-j)), not an
  incrementally-slid stateful buffer.

## Can we optimise over voices? NO (at the draw level)
The draw is multivariate (all voices per draw); skipping voices would need per-voice MA-history
regeneration on activation (expensive, and now bidirectional). So: NEVER optimise the DRAW over voices
(buffer is all-voices regardless; voice count doesn't change its size). Optimise ONLY downstream
(spread/output per-voice processing), which reads the all-voices draws with no history dependency — that
is where the earlier perf cost actually was.

## BIDIRECTIONAL CACHES — needed for reverse mode (rhythm, melody, qmix)
Reverse mode (phase input): the MA window must reach the OTHER index direction. Forward play: window
reaches BACK (N-j). Reverse play: window reaches FORWARD (N+j). The turnaround TRANSITIONS over the slew
depth (slew = "how far back"; slew=1 -> instant turnaround, slew=64 -> blends over 64), after which
reverse RETRACES the forward windows exactly (= exact reversibility, r reproducible).
Problem: forward play only buffers [N-j..N]; a turnaround suddenly needs [N..N+j] (up to 64 draws) -> spike.
**Solution (Rodney): maintain a BIDIRECTIONAL buffer [N-j .. N+j] for rhythm, melody, qmix.**
- INIT the full bidirectional window ONCE at reset (into NEGATIVE counters too — Philox is a bijection
  over signed space, so [N-j] near the origin is valid; no truncation). One-time cost, off audio-critical.
- MAINTAIN: each step/dice-roll, extend the LEADING edge by the new draws (a couple per step) — the
  trailing edge is already buffered. So ~a couple of draws per step, amortised.
- TURNAROUND is then FREE (forward window already warm, no spike).
- Rebuild the buffer only at PHRASE-BOUNDARY dice rolls (a full regen point); otherwise just maintain
  (extend leading edge).
- Prune the trailing edge beyond [N-j..N+j] to bound the cache (currently an unbounded map).
- Scope: bidirectional only where instant reversal happens (PHASE mode). Clock/gate (reversal at phrase
  boundary / not instant) can stay unidirectional-backward.
- j = slew depth (or max j=64 if slew is modulatable and can jump deep).
- **REVERSE-MODE MAINTENANCE — the leading edge and dice meaning FLIP (Rodney):** which edge to extend
  follows the current PHASE/PLAY DIRECTION, and the meaning of forward/backward dice rolls inverts in
  reverse:
  - FORWARD mode: playhead index INCREMENTS; play-forward = index++ -> extend the FAR-FORWARD edge
    (N+j+1); step-back/undo = index--.
  - REVERSE mode (phase driving backward): playhead index DECREMENTS; "play-forward" in the REVERSE
    timeline = index-- -> extend the FAR-BACKWARD edge (N-j-1); "step-back/undo" in reverse = index++.
  So a FORWARD dice roll means index-increment in forward mode but index-DECREMENT in reverse mode (and
  backward dice the opposite) — the index-direction of forward/backward dice FLIPS with phase direction.
  The buffer always covers [current-j .. current+j]; the LEADING edge (extended each step) is the one in
  the current play direction; the TRAILING edge (behind play direction) is already buffered and pruned
  beyond j. At a TURNAROUND (phase flips), the new leading direction is already buffered (both sides kept)
  -> free; then continue extending the NEW leading edge as play proceeds in the new direction.

## 32 STEPS + BIDIRECTIONAL funded by the 4x Philox win
Buffer grows 64 -> 128 wide (bidirectional) AND steps 16 -> 32 (2x) = 4x more draws. This is EXACTLY
funded by reclaiming the 4x Philox waste below -> NO net Philox-block cost increase. We DO still pay more
PhiInv-LUT (per z) and the bigger one-time init, but the init is once (at reset), and the LUT is cheap.

## PHILOX 4x EFFICIENCY — the 75% waste is REAL (do this)
`philox4x32_10` returns 4 outputs/call (the 10 rounds are the cost; 4 outputs are free). `at(pos)` =
`block(pos>>2)[pos&3]` — correct per-primitive, BUT `rawDraw*PatternAt` (PatternEngine.hpp:694-718) calls
`atUniform(pos*DRAW_CHUNK + cursor)` PER CURSOR in a loop (c++). Each call RECOMPUTES block(x>>2) and uses
1 of 4 outputs -> **4x Philox-block computations, 75% discarded** (~1024 calls per multivariate draw vs
~256).
**FIX: batch via fillBlock/drawBlock** (exist, PhiloxRng.hpp:175/177) over the consecutive cursor range ->
each block computed ONCE, all 4 outputs used -> **4x fewer Philox blocks, BIT-IDENTICAL output.** No
algorithm change.

## NONCE — indexing convenience, NOT compute efficiency
ctr[2..3] nonce is UNUSED (fixed 0; "UNUSED and stays so", PhiloxRng.hpp:116). Using it does NOT add
speed (each nonce value = a full Philox call). It is addressing cleanliness only (counter = pure
position for reverse-mode N±j windows; lane/voice via nonce vs the current packed pos*DRAW_CHUNK+cursor).
- Cross-stream (rhythm/melody/qmix/CA): keep SEPARATE KEYS (S, S+1, S+2, S+3 additive — decorrelated,
  deliberately bug-fixed). Do NOT refactor to nonce. No speed gain, touches bug-sensitive key code.
- Within-stream packed counter -> nonce: DEFER to reverse-mode build; adopt ONLY if the packed counter
  (pos*DRAW_CHUNK+cursor) complicates the N+j / negative-position addressing. Tidiness, not necessity.

## SIMD Philox — optional, on top of the 4x
Philox is built for SIMD (parallel counters). Would give another ~4-8x on raw-draw gen. Candidate for the
turnaround/init burst, but likely unnecessary after the 4x batching + bidirectional buffer (which makes
turnarounds free). Decide by measurement; the 4x batching is the priority.

## Action order
1. 4-output batching (fillBlock/drawBlock in rawDraw*PatternAt) — the 4x, do first, mechanical.
2. Bidirectional buffer for rhythm/melody/qmix (reset-init incl. negative counters, extend-leading-edge
   per step, rebuild only at phrase-boundary dice rolls, prune trailing, scope to phase mode).
3. 32 steps — funded by (1).
4. Nonce / SIMD — defer, only if measurement/reverse-mode needs them.
