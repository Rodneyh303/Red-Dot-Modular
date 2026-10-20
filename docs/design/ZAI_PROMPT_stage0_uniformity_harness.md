# ZAI PROMPT — Stage 0: the per-(voice,lane) UNIFORMITY HARNESS (drafted; verify + commit)

CONTEXT FOR THE AGENT: you are working the repo Red-Dot-Modular, branch `lane-expander-refactor`.
Read FIRST, in this order: `CLAUDE.md` → `docs/design/CHAT_HANDOFF.md` (skim) →
`plans/SEAM_ERADICATION_PLAN.md` (Stage 0 is your spec) → the "BUILD ORDER step 0" section of
`docs/design/SANDS_MODIFIER_RESOLVER_DESIGN.md`. This prompt is self-contained; those give the WHY.

This work precedes the SandsModifierResolver extraction (the priority): the resolver's every
step is gated on this harness, so the safety net ships first.

## STATUS — the harness is ALREADY DRAFTED (review + verify + commit, do not rewrite)

- `test/test_lane_voice_uniformity.cpp` — the harness (drafted 2026-10-20, compiles + runs).
- `test/run_all.sh` — registered as `test_lane_voice_uniformity|$PE $SE $GS` (alphabetical,
  after `test_lane_types`). This is the only runner edit.
- Both changes are UNCOMMITTED in the working tree. Your job: review against the spec below,
  run, confirm the expected-red matrix, then commit (one commit, message style: the repo's
  `test(harness): ...` convention). Do NOT commit anything else lying around.

## WHAT THE HARNESS DOES (the Stage-0 spec, implemented)

For EACH per-(voice,lane) feature, loop ALL 16 voiceSlots × ALL 7 lanes; set a DISTINCT value
on ONE cell through the real headless write path; drive the real rebuild/promote; assert
(a) the consumer for (v,l) reads IT — no DROP, (b) every other (v',l') is UNCHANGED — no LEAK.
Slot 0 (V1) and lanes 5,6 (VAR/LEG) are inside every loop, not special-cased — they are the
cells the current bugs distinguish.

Sections (all engine/manager headless-real paths, NO behaviour change anywhere):
- U1 warp own-slot: WarpBank author → rebuildWarpMirrorAll → warpRow. 112 cells.
- U2 warp delegation: setWarpDelegated + globalWarpF/W (the flag MonsoonSandsManager.cpp:144-155
  mirrors from topo.owner==MACRO). Oracle = WarpBank::warpedValue(...,delegated=true). All 7 lanes.
- U3 warp consumer gate: polyRandomSrc must return the MIRROR row for every (v,l).
- U4 warp dirty-mask scopes: rebuildWarpMirrorVoice/VoiceLane/Cell/LaneDelegating must reproduce
  the full rebuild exactly INSIDE scope and move NOTHING outside it, positive and empty cases.
- U5 spread isolation: SpreadManager::setSpread (real Sands write surface) + getInterpolatedValue
  vs the SpreadInterp oracle, per-cell, incl. V1's mono self-target (positive = no-op BY DESIGN;
  exercised via negative spread which must INVERT).
- U6 SpreadResolver is lane-FREE: identical Inputs → identical effective() for all 7 lanes,
  both branches + the algebra pinned against SpreadResolver.hpp:90-103.
- U7 direction promotion: laneDirVPending_ → laneDirV_ via advancePlayhead (StepEdge), per (v,l),
  + the mono channel + polyLaneSign uniformity. The exact store MonsoonExpanderManager.cpp:398 writes.
- U8 SandsTopology owner matrix: one ceded cell → exactly that cell is MACRO; MACRO_SOLE uniform 16×7.
- U9 LOR store round-trip: polyLORRef/polyLOR, 16×7×3, voiceSlot base.

## EXIT CONTRACT (the gate mechanics — non-negotiable)

- Failures matching a KNOWN_SEAMS manifest entry print as "KNOWN SEAM" and do NOT fail CI.
- UNEXPECTED failures exit 1 — that is the harness FINDING a seam nobody mapped. Treat any
  unexpected failure as the day's bug: diagnose against code (never guess-patch), then either
  fix or, with Rodney's call, add a manifest entry WITH its closing stage named.
- `UNIFORMITY_STRICT=1` fails on ANY failure — this is the per-stage gate. A stage is DONE
  when its manifest entries are DELETED and strict passes.
- The manifest only ever SHRINKS. Never add an entry to make a test pass.

## VERIFIED FACTS (grounded 2026-10-20 on this branch — do not re-derive, but DO re-verify if HEAD moved)

1. polyRandomSrc (SequencerEngine.hpp:542) routes ONLY PL_MELODY/PL_OCTAVE to pe.warpRow; the
   other 5 lanes read the raw bank. → the harness's ONE known-red family: WARP-CONSUMER-GATE,
   exactly 80 cells (16 voices × QMIX/REST/ACC/VAR/LEG). Closed by routing all lanes through
   warpRow (feat/warp-ui CC_PROMPT_05, currently UNMERGED — 12 commits ahead of this branch).
2. advancePlayhead promotes laneDirV_ for v=1..numPolyVoices ONLY (SequencerEngine.cpp:189/306);
   slot 0's poly row is inert (V1 promotes via the mono laneDirPending_ channel). The harness
   PINS this as DIR-SLOT0-ROW-INERT; Stage 3 (execution-path unification) deletes the entry.
3. CvTerm::contribution() ALREADY carries the ×2 kBipolarSpan (SpreadResolver.hpp:47) — this
   DIFFERS from SANDS_MODIFIER_RESOLVER_SKETCH.md, which moves ×2 into SpreadPolicy::scaleCv.
   Reconcile in the extraction step (the sketch is the target shape; the header is the math truth).
4. The stale-comment smell is LIVE in the direction path: MonsoonExpanderManager.cpp:385-386 says
   "Lanes 4/5 (VAR/LEG) have no Macro ownership" but the code at :390 loops `l < POLY_LANES` (7)
   and resolves ownership for all lanes. Do not trust that comment; Stage 1's audit maps the truth.

## HOW TO RUN

- Full suite: `bash test/run_all.sh` (needs simde — it lives in the Rack SDK `dep/include`;
  CI sets RACK_DIR. Headless: any dir with `dep/include/simde/` works as RACK_DIR).
- One test: `bash test/run_all.sh test_lane_voice_uniformity` (prints PASS; the map prints too).
- The map: run the test binary directly, or `UNIFORMITY_STRICT=1 bash test/run_all.sh
  test_lane_voice_uniformity` to see the strict gate fail with the full map.
- Expected on today's HEAD: `checks: 1494  clean: 1414  KNOWN-SEAM: 80  UNEXPECTED: 0`,
  exit 0 (strict: exit 1). Suite stays 65/65 green + anchor audit green.

## RESIDUALS (the harness says these out loud at the end of every run — they are Stage-1 surface)

Not reachable headless (need live expander modules / Rack):
- MonsoonSandsManager.cpp:144-155 — the warpDelegated_ mirror from topo.owner (the Macro→East
  warp leak site; fixed on unmerged feat/warp-ui by filling topology owner rows — CC_PROMPT_07).
- MonsoonExpanderManager.cpp:378-400 — the per-voice DIRECTION push (VAR/LEG follow, CC_PROMPT_08)
  + combineLOR (~:264, the verified ~40-line duplicate of spread's algebra).
- Monsoon editor store accessors — editor.laneDir[96] is poly-bank indexed v*6+lane
  (Monsoon.hpp:790-795, flagged "scheduled for reconciliation").
- executeStep (V1) vs executePolyVoice (V2+) — the Seam-1 split itself (Stage 3 closes it).

## NOT THIS TASK

- No behaviour changes to src/ — the harness is observation only. Any "fix" you spot goes in the
  report, not the diff.
- Do NOT start SandsModifierResolver extraction (that is the NEXT prompt; it consumes this net).
- Do NOT merge feat/warp-ui (Rodney's call, and it changes the manifest entries above).
- Do not touch test/run_all.sh beyond the single registry line already added.
