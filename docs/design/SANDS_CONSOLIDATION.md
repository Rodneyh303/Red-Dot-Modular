# Sands consolidation — three modules to one + thin expanders (Rodney)

STATUS: DESIGN DIRECTION, confirmed. Gated on a capability audit (below). Reprioritises Sands panel
craft: REDESIGN one unified module, do NOT polish Mono/East/Macro that are to be dissolved.

Companion: **SANDS_ARCHITECTURE_CONSOLIDATION.md** is the earlier ANALYSIS (where the complexity lives —
`combineLOR`/`combineSpread` — and the 3/2/1 option comparison). It predates the correlation matrix and
so could weigh options but not decide; THIS doc is the decision, on the new argument (copula dissolves
the soup fear) + the maintainability evidence. Its `combineLOR`/`combineSpread` analysis and option-1
"single module, paged" sketch are directly useful for the BUILD.

## Why (the case, strongest argument first)
1. **Three implementations = three bug surfaces.** This session's Sands bugs were ALL single-module
   divergences: the lane-index bug hit one visual not the others; the Mono spread no-op was Mono-only;
   the MIX IN label drift was Macro-only. The same conceptual operation implemented three times
   slightly differently — and the divergence is where the bugs lived. One implementation cannot disagree
   with itself. The collapse REMOVES the structural cause of a whole bug class we have been fighting.
2. **The correlation matrix dissolves the "soup" fear** that justified the three-way split. Variation and
   legato were kept MONO because uncorrelated per-voice variation = mush (AVERAGE_POLY). With graded
   Gaussian-copula correlation, per-voice variation is tamed: +100% = locked to mono (== today), 0% =
   independent, -100% = interlocking. Soup is one corner of a continuous, CA-group-structured space, not
   the default. So the lane-uniformity that split Sands into reference(Mono)/poly(East)/global(Macro) is
   obsolete — ALL lanes can be poly + correlated.
3. **Advances past the meloDICER lineage.** MEX3 (the inspiration) made legato+rest poly but kept
   variation mono — correct FOR THEM because they had no correlation matrix. Our copula removes exactly
   that limitation, which is what makes the split obsolete. Not copy-then-diverge; resolving a limitation
   baked into the inspiration.
4. **The "lost weekends" are retired CODE, not lost KNOWLEDGE.** The hard Sands work established what the
   per-voice lane model must do (editing, LOR, lock, blend, latch) and where the three diverged — i.e.
   the spec the unified module must reconcile.

## Proposed topology
- **Sands (ONE)** — all lanes, poly, correlated (reference voice + correlated field in one editor).
  Absorbs Mono's V1-editing and East's V2-16 editing (same thing at different voice indices).
- **+ Global modulation expander** — Macro's global-LOR layer (Macro: `global` x40), for those who want it.
- **+ Taps expander** — Macro's tap outputs (Macro: `tap` x31), for those who want them.
Maps to how people think: per-voice lanes (base), modulate globally (expander), tap out (expander).

## Capability audit (absorbed / relocated / redundant / LOST)
From a code scan of the three visuals:
- **ABSORBED (no loss):** strand/lane editing, lock, LOR, base — the per-voice lane representation the
  unified editor provides by definition.
- **RELOCATED (no loss):** global modulation + taps -> the two expanders.
- **REDUNDANT (simplification):** the `latch` that arbitrates which module OWNS a lane's base when
  multiple Sands are present — with one module there is nothing to arbitrate. Confirm nothing downstream
  depends on the latch STATE existing.
- **THE ONE REAL LOSS:** **lane-owner switching across PLAYHEADS** (East's distinctive `lane owner` +
  `blend`). East can switch which lane/strand a voice follows, and this can switch between different
  PLAYHEADS. The unified model assumes ONE mono playhead (Mono already uses the same playhead per voice).
  Whether this matters depends entirely on whether multi-playhead lane ownership is a sound anyone uses.
- **VERIFY before committing:** (a) `blend` (East x9) is the SAME as the correlation/reference blend
  (reference + correlated deviation IS a blend) and not a second mechanism; (b) `lock` semantics (heavy
  in all three) move cleanly into the unified editor and don't depend on the three-module split.

Verdict: large complexity reduction (3 modules -> 1 + 2 thin expanders) for the cost of one niche
capability (lane-owner across playheads) + a migration (pre-release, allowed). Decisive trade.

## Sequencing
- Do the capability audit resolution (lane-owner decision; blend/lock verification) FIRST.
- Then redesign ONE Sands module (this is where the panel-craft effort for Sands goes — not into
  polishing the three).
- Migration: Mono/East/Macro become legacy; pre-release so breaking is acceptable.
