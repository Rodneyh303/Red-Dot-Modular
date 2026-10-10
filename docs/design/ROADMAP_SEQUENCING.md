# Roadmap sequencing — the dependency chain (as of V1 keystone landing)

Context: the Straits refactor started as PANEL work + legato/variation lanes, then a cascade of issues
arose (the V1/mono bug class — spread knob-lock, East/Macro bar misalignment, QMIX arc, the crash — plus
the big performance work). The V1->voice-0 keystone cleared the mono bug class. This doc fixes the order
of the remaining work so the dependency chain is explicit.

## The chain (each step enables the next)
1. **[DONE] V1 -> voice-0.** The hard SEMANTIC unification (collapse the parallel mono data model to
   voice 0). "Mono is not special anymore." Killed the V1-only bug class. Merged to
   lane-expander-refactor.
2. **Finish Straits** — the original in-flight work, now on the uniform voice foundation:
   - Panel art (seamless-panel runtime draw-over seam; see STRAITSR_PANEL_ART_PORT.md).
   - VAR/LEG lanes (variation/legato as lane expanders — the per-voice params now exist post-V1).
   - Range lane (half-height LED lane; RANGE_LANE_PANEL_NOTE.md).
   **Open ordering question (see below): lane-index unification may go BEFORE the NEW lanes.**
3. **Lane-index unification** — the MECHANICAL sibling to V1 (~1/3-1/2 the effort; LANE_INDEX_UNIFICATION.md).
   One canonical VISUAL-order index, delete EL2ENG -> kills the 2nd recurring bug class (lane crosswires).
   The visual order is STREAM-CONTIGUOUS (melody 0-1, qmix 2, rhythm 3-6).
4. **Philox 4x efficiency** — reclaim the 75% waste (batch via fillBlock/drawBlock;
   DRAW_BUFFER_AND_PHILOX_EFFICIENCY.md). WANTS the stream-contiguous indices from step 3 -> do AFTER
   lane-index. 4x fewer Philox blocks.
5. **32 steps + draw-buffer / reverse-mode** — FUNDED by the 4x (step 4). Bidirectional buffer, reverse/
   slew turnaround, the light-ring-at-32 graphics.
6. **Forward features** on the fully-clean foundation: unified addressing system (voice LOR, cycle/abs1/2/3
   mask modes), per-voice octave offset (Straits), the modulation canvas, tie-across-rests, Causeway.

## Why THIS order (the couplings)
- **Both unifications first (voice=1 done, lane=3):** they clean the foundation (one voice index, one
  lane index) -> every later feature composes uniformly, no index/representation mismatches to fight.
- **Lane-index (3) BEFORE Philox (4):** the 4x batching reads stream-contiguous ranges; lane-index
  PRODUCES that contiguity. Doing Philox first = batch against the old order, then the reorder shifts it
  underneath = redone work. Lane-index -> Philox is non-redundant.
- **Philox 4x (4) BEFORE 32-steps (5):** the 4x FUNDS the 2x (32 steps) x 2x (bidirectional buffer) at no
  net Philox-block cost.

## Open ordering question: lane-index vs the NEW Straits lanes
The new lanes (VAR/LEG, range) are being ADDED; lane-index reorders the lane index. Options:
- **Lane-index BEFORE adding VAR/LEG/range (lean):** the new lanes are born into the one-index layout, not
  added to the dual-index (EL2ENG) system and then reordered. Avoids building them twice.
- **Finish all Straits first, then lane-index:** simpler if the panel work is nearly done, but the reorder
  then re-touches the just-finished Straits code.
Decide by how much Straits remains: if VAR/LEG/range lanes are still to build, do lane-index first so they
land clean; if Straits is nearly done, finish it then reorder. (Panel art / seam is index-independent —
can proceed either way.)
