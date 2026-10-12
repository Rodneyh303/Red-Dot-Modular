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
   - **BUILD THE SEED/COUNTER OFFSET HERE too (already scoped — SEED_OFFSET_DESIGN.md).** Steps 4-5
     restructure the Philox counter addressing (batching, bidirectional N±j windows), which is exactly the
     "apply offset at the address site (draw N -> draw N+offset)" code the seed-offset needs. So fold it in.
     It's a RELATIVE SIGNED COUNTER offset (effectiveCtr = drawCtr + offset) — param + CV, unified
     rhythm+melody by default. No edge cases (signed-bijection over the whole space). Covers crab/canon
     alignment, scrubbable timeline (CV-driven), and jump. NOTE the key-vs-counter rule (MASTER_PLAN:421):
     this feature uses a COUNTER offset DELIBERATELY (same key, time-shifted = correlated — the canon
     behaviour); stream INDEPENDENCE uses KEY offset (that's separate, already done: S/S+1/S+2).
   - **Nonce migration (optional, consider here):** moving lane/voice into ctr[2..3] (nonce) gives a
     pure-position ctr[0..1], making both the N±j reverse windows AND the counter-offset cleaner (offset
     shifts pure position, not the packed pos*CHUNK+cursor). NOT required — the seed-offset and reverse
     mode both work with the packed counter — but 4-5 is the natural time if doing it (addressing is
     already being restructured; avoids a later separate refactor). Lean: do it if the packed-counter
     stride proves awkward in the reverse windows; else defer. (Cross-stream separate KEYS stay — don't
     fold those into the nonce.)
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

---

## Forward sequence (post slew/mix fix) — Rodney's list + dependencies
Order: SLEW+MIX fix (in progress) -> then the rest. Slew/mix must be cheap first because everything that
MODULATES (counter-scrub, addressing) sweeps slew/mix -> blocked until fixed.

1. **Slew + mix fix** (decomposition + horizontal-K SIMD). Unblocks all modulation-heavy work. (NOW)
2. **Counter + bidirectional buffer — TOGETHER** (Rodney). The counter-scrub's "scrub back to return" IS
   reverse navigation -> needs the bidirectional buffer; build them as one. (Counter = offset-scrub; see
   SEED_OFFSET_DESIGN.md. Buffer per DRAW_BUFFER_AND_PHILOX_EFFICIENCY.md.)
3. **Unified addressing** — incl. the RANGE LANE (Rodney: range lane lands with ADDRESSING, not Straits;
   the lane UI may be built in Straits but its semantics are the addressing system's). Voice LOR,
   cycle/abs1-3 masks, correlation x reset-phase. Big; on the counter machinery.
4. **K (window-depth param)** — slot after slew-fix + SIMD + buffer-cost measured (is it an optimisation
   or just use of the 64 buffer — see DRAW_BUFFER doc). Low-risk param exposure, anytime after.
5. **Octave (per-voice octave offset)** — Straits feature; mostly independent; pairs with finish-Straits.
6. **Expander connection polish** — the interconnection work (owner back-pointer etc.); foundational;
   before/during Straits.
7. **Finish Straits** — panel art, VAR/LEG lanes. (Range lane moved to #3 addressing.)
8. **Causeway** — LAST. CV-mod expander; inherits Straits wave/seam helpers + owner model; modulates
   features that must exist first.

### Counter-scrub display idea (maybe) — override display slew during scrub, via LOCK MODE
During counter-scrub you navigate by POSITION in the probability sequence -> want to SEE the underlying
per-position draws, not whatever the current slew smooths them into. Idea: FIX the DISPLAY slew to a
constant during scrub (reuse LOCK MODE as the hold/override) so the scrub view is consistent/legible;
AUDIO keeps the real slew. OPEN: which fixed value — slew=0 (raw, distinct per-position draws — navigate
the actual content) or slew=1 (fully-smoothed structural view)? Instinct: 0 (see the real draws); confirm.
Ties to the counter digital expander (its job = show position/content legibly).

### K-modulation — RESOLVED: phrase-boundary, as the intertemporal sibling of CA (Rodney)
K is the INTERTEMPORAL correlation structure (how far the temporal correlation reaches). CA modulates the
CROSS-VOICE correlation structure, and CA-mod is PHRASE-BOUNDARY. K is the same KIND of thing (correlation
structure) on the TIME axis -> K-modulation follows the CA precedent: PHRASE-BOUNDARY. Principled
(correlation structure coheres at the phrase), not just a perf rate-limit. Symmetry: CA = cross-voice
correlation structure, K = intertemporal correlation structure, BOTH modulate at phrase boundary — two
axes (voice x time) of correlation-structure, same cadence. So K-mod is phrase-boundary modulatable,
the time-axis sibling of CA-mod.
