# Line-level ODR / DRY Audit — Red-Dot-Modular

**Origin:** an external audit (a second AI), produced 2026-11-16 at
`feat/cadence-c4-sands-display @4d971232`. Chat-delivered; this is the committed, code-verified
version — it is the "line-level F1–F9 audit" the decomposition note (`:42-48`) and
`CONCEPT_ODR_AUDIT.md` cross-reference. **These F-numbers are a different namespace** from
`CODEBASE_REFACTOR_REVIEW.md` / `REFACTOR_PRIORITY.md`, whose F1–F9 are the RT-safety list.

**Re-verified @`fix/bug6-stable-edge` `964acc2a` (2026-11-18):** every finding re-checked
against the current tree (the display-unify merge is now an ancestor). Each point carries a
**Verified** line: *HELD* (stands, cites refreshed), *DISSOLVED* (no longer present),
*REDUCED* (partly gone). Original claims/fixes preserved verbatim from the external audit;
the verification lines are this repo's.

**Lens:** line-level DRY — the same conceptual thing with multiple *undeserving* definitions.
Concept-level duplications (abstractions that deserve ONE skeleton) are in `CONCEPT_ODR_AUDIT.md`
(CA1–CA5); several F-points below are the line-level face of a CA (noted per point).

---

## F1 — HIGH · The lane↔strand mapping is defined four times, one copy dead AND divergent

The declaration (`SpreadInterp.hpp:278`, "laneToStrand is now IDENTITY"). Four survive:
`SpreadInterp::laneToStrand` (the ONE mapping), `polyLaneStrand` (7-arm, value-identity),
`strandLaneId` (7-case, value-identity), and `polyLaneToStrand` — a 5-arm ternary where
**PL_VARIATION/PL_LEGATO fall back to STRAND_RHYTHM** (the pre-consolidation law). Dead +
divergent: if revived (the name reads canonical) lanes 5/6 silently walk RHYTHM. Only QMIX
of the seven pairs is value-pinned.

**Fix (small):** delete `polyLaneToStrand`; have `polyLaneStrand`/`strandLaneId` delegate to
`SpreadInterp::laneToStrand`; add `static_assert(PL_x == STRAND_x)` for all seven pairs.

**Verified @964acc2a — HELD.** `polyLaneToStrand` now `SequencerEngine.hpp:1543`, **still zero
callers** (repo-wide grep: definition only), body still VAR/LEG→RHYTHM divergent.
`polyLaneStrand` `:1798`, `strandLaneId` `:1812`, `SpreadInterp::laneToStrand` `:279`. Pins
grew but are still not the seven PL_x==STRAND_x pairs: `LaneMapping.hpp:131` (QMIX),
`PatternEngine.hpp:224` (PL_QMIX==STRAND_QMIX), plus `kLaneIdCount` count-asserts. The F1
danger (dead + divergent twin) is intact. Highest-value, lowest-risk fix on the list.

## F2 — HIGH · "Macro's own CV-applied L/O/R" had three definitions with different cadences

At the audit baseline the Macro lane rows (`StraitsSandsMacroVisual.cpp:1067-1069`) and the
sweep-fingerprint cells (`:1375-1377`) composed `lround(macroBase + macroCVDelta)` live,
disagreeing with the committed-cadence `effectiveMacroRowLor` on adjacent rows of the same strip.

**Fix:** route the widget sites through `effectiveMacroRowLor` (committed; matches the range
row and what plays); a diagnostic fingerprint may stay live but should say so.

**Verified @964acc2a — DISSOLVED.** `macroBase[`/`macroCVDelta[` no longer appear in
`StraitsSandsMacroVisual.cpp` (grep: 0). The display-unify merge repointed the macro lane-row
reads to the committed accessor — exactly CA3's prediction. What remains of the *concept* dup
(the manager-side `applyMacroCV` verbatim composition) is tracked as **CA3**, not here.

## F3 — MED-HIGH · The "who owns this lane's direction" selection exists twice

`MonsoonExpanderManager::monoDirAuthority` is the declared ONE selection
(macroGlobal → field → param). But `StraitsEastSandsVisual.cpp` re-implements the same
three-arm selection inline for its display — even though it already holds `auth` (a
`MonoDirSrc`). Twin at `StraitsSandsMacroVisual.cpp` (`getVoiceLorMacroDir` + lround/clamp).

**Fix:** hoist `MonoDirSrc`/`monoDirAuthority` (or a `resolveDirSrc(cur)` member) into the
shared resolver header next to `resolveDirItem`; widgets call it. Fold into the direction track.

**Verified @964acc2a — HELD, shifted.** The widget re-resolve is now
`StraitsEastSandsVisual.cpp:1408` (`monoDirAuthority(lane)`) → `:1413-1414` (the inline
`auth.macroGlobal`/`auth.isField()` three-arm, was `:1441-1443`; the file was reworked by the
branch's BUG-4/BUG-5 display commits). `monoDirAuthority` still declared at
`MonsoonExpanderManager.hpp:117`. This is the line-level face of **CA1**'s direction lifecycle.

## F4 — MED · The playhead read-cell law exists twice across the audio/UI boundary

Audio: `SequencerEngine::getStrandIdx` = `((tick+mutation) mod len + off) mod kMaskRing`.
Draw: `SandsRangeGestureCore::lanePlayheadBar` (via `calcPlayhead`). Same law, but the len
guards already diverge textually — `getStrandIdx` uses `max(1,len)`, `lanePlayheadBar` uses
`clamp(len,1,ring)` (differ for len > ring) — and nothing pinned the numeric equivalence.

**Fix (pick one):** (a) one GLFW-free law header both include; or (b) a test-time equivalence
pin over a (tick,len,off,rot,ring) grid, with identical len guards.

**Verified @964acc2a — HELD (partly mitigated).** Guard divergence persists:
`getStrandIdx` `max(1,len)` (`SequencerEngine.cpp:55/682/3092`) vs `lanePlayheadBar`
`clamp(len,1,ring)` (`SandsRangeGestureCore.hpp:131/145`). A *structural* pin now exists
(`test_variation_scan_display.cpp:144` asserts `calcPlayhead` delegates to `lanePlayheadBar`;
`test_lane_direction.cpp:38` re-states `getStrandIdx`), but **no numeric equivalence grid**
and the guards are still not the same guard. Fix (b) still wanted.

## F5 — MED · Two "clamp a lane index" helpers with different out-of-range semantics

`editorLane` → in-range else **0 (MELODY)**, literal `7`. `strandClamp` → in-range else
**STRAND_RHYTHM (3)**. Same index space, different fallback; a bogus index lands on a different
lane depending on which helper it reached.

**Fix:** one clamp, one documented fallback (RHYTHM, matching `laneToStrand`), named `NUM_STRANDS`.

**Verified @964acc2a — HELD.** `editorLane` `SequencerEngine.hpp:1232`:
`(engLane>=0 && engLane<7) ? engLane : 0` (literal 7, fallback MELODY). `strandClamp` `:689`:
`(s>=0 && s<NUM_STRANDS) ? s : STRAND_RHYTHM`. Both now documented "identity, kept for
readability" — but the divergent fallbacks remain.

## F6 — MED-LOW · The wrap law is a quad: one canonical helper, three shadow idioms

`wrapMod` is canonical. Also: `PositionResolver::wrapRing` (a kMaskRing-specialized duplicate),
the `& (kMaskRing-1)` power-of-two idiom, and ~47 inline `((x%n)+n)%n` sites — of which the
kMaskRing-family ones inside `SequencerEngine` should be `wrapMod(x, kMaskRing)` (the `%12`
pitch-family ones are a different domain, exempt).

**Fix:** route the kMaskRing sites through `wrapMod` (or bless the bitmask idiom with one
comment naming the law); fold `wrapRing` into `wrapMod` with the constant applied by the caller.

**Verified @964acc2a — HELD.** `PositionResolver::wrapRing` `PositionResolver.hpp:59`, in use at
`:70`/`:133`. Low-risk mechanical unification.

## F7 — MED-LOW · The effective-LOR accessor family: one *shape* written eight times

`effectiveVoiceLor`/`effectiveMacroRowLor`/`effectiveMonoLaneLor`/`effectiveLaneLor` + their four
`...Live` halves each re-write "read 3 base items → add 3 deltas → `lorRowClamp_` → dir row".
The clamp is shared (`lorRowClamp_`, good); the shape is copied per store tier. C1b's Stage B
had to be applied in four places at once.

**Fix (judgement call):** a private `composeLor_(bL,bO,bR, dL,dO,dR, d, L&,O&,R&)` collapses each
body; or pin the family with one test asserting all four share the committed-CV cadence.

**Verified @964acc2a — HELD. = CA4 / the `composeLor_` candidate.** Eight accessors confirmed at
`SequencerEngine.hpp:1023/1038/1052/1059/1263/1271/1282/1292`. This is the line-level face of
concept-audit **CA4**; `composeLor_` is the shared primitive both ask about
(decomposition note `:43`).

## F8 — LOW · Declaration defaults vs reset defaults

`macroVoiceLor_[LOR_ITEMS] = {}` and the store arrays zero-init at declaration; `reset()` writes
the real defaults (cap/0/0). `Monsoon.cpp` calls `reset()` before use, so it is latent — but any
new construction path (a second host, a test that forgets reset) reads L=0, a different engine
than the documented default.

**Fix:** make the in-class initializers the real defaults, or add a cheap `assert(inited_)` guard.

**Verified @964acc2a — HELD.** `macroVoiceLor_[LOR_ITEMS] = {}` `SequencerEngine.hpp:709`;
`lorStore_ … = {}` `:684`; real defaults in `reset()`. Latent, low priority.

## F9 — LOW · Literal LOR indexing — `[0]/[1]/[2]` where `LOR_LEN/LOR_OFF/LOR_ROT` exists

A future `LOR_ITEMS` re-order would silently flip the literal-indexed widget compositions.

**Fix:** mechanical enum swap.

**Verified @964acc2a — REDUCED.** The main cited sites (`macroBase[l][0..2]` in
`StraitsSandsMacroVisual.cpp`) went with F2 — now 0 there. ~3 literal LOR-index sites remain in
`StraitsEastSandsVisual.cpp`. The engine already uses the enum (10 `[LOR_LEN/OFF/ROT]` sites).

## F10 — INFO · Widget-layer repetition (noted, not actionable as one fix)

`lightTheme` reads + colour ternaries per widget (~15 files); `findNamed`/`centerOf` anchor
helpers per visual (kept local on purpose — SVG anchors are per-panel). Typical Rack idiom cost;
centralise only if a theme-law change ever has to touch 15 files. **Not re-verified** (INFO).

---

## Examined and found NOT to be violations (external audit, carried over)
- `laneDir_ ← laneDirPending_` at StepEdge vs Phrase — two intentional quantizations, not copies.
- `monoSlewedPub`/`polySlewedPub` (+ PreRemap) — deliberate mono/poly specialisations.
- The tiered LOR stores — the design; the effective accessors are the chokepoints.
- Test suites re-stating closed forms — pins of the laws, the desired redundancy.

## Map to the concept audit (`CONCEPT_ODR_AUDIT.md`)
- **F3** → line-level face of **CA1**'s direction lifecycle (do under the direction track).
- **F7** → line-level face of **CA4** (`composeLor_` the shared primitive).
- **F2** → the widget half dissolved; the manager-side remainder is **CA3**.

## Suggested order (external, adjusted for what's dissolved)
1. **F1** — delete the dead divergent mapping + full static_asserts (tiny, highest danger removed).
2. ~~F2~~ — DISSOLVED (widget half); the manager remainder rides CA3.
3. **F5 + F6** — one clamp, one wrap (small mechanical unifications).
4. **F4** — the read-cell equivalence pin + unify the len guards (small code, big guarantee).
5. **F3** — hoist the direction authority (fold into the direction track, with CA1).
6. **F7–F9** — housekeeping; F7 rides CA4's `composeLor_`.

*External audit 2026-11-16 @`4d971232`; re-verified and committed @`964acc2a` 2026-11-18.*
