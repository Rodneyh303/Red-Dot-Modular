# CC PROMPTS — Phase 3 build sequence

Each prompt is self-contained (points CC at CLAUDE.md + CHAT_HANDOFF.md + its spec). Build in
order; each ships on its own branch, PRs into lane-expander-refactor, must be GREEN before the
next starts. The DETAILED what-and-why for all of them is SESSION_LOG_2026-10-18b.md + …-18.md.

| # | Prompt file | What it builds | Depends on | Testable? |
|---|-------------|----------------|------------|-----------|
| 01 | CC_PROMPT_01_send_tap_START.md | per-voice send-tap fix (bug). macroTap[224], dual-delta publish. | — (ships first, alone) | YES — test_send_tap, 4 asserts red→green |
| 02 | CC_PROMPT_02_lanedist_START.md | LaneDist POD bank + lower() + drawn pool. distmath:: shared-math refactor. | 01 merged (clean trunk) | YES — test_lanedist (agreement + persistence) |
| 03 | CC_PROMPT_03_mirror_warp_START.md | warp mirror (effective w-mixture), w[112], scoped dirty mask, warp composition at genPitchLive (melody+octave) + finalRandomByStrand. | 02 | YES — test_warp_mirror (no-op@w0, monotone, dirty-scope, consumer-routing) |
| 04 | CC_PROMPT_04_warp_ui_START.md | panel columns (w/param left; Macro send/tap right), popup editor (reuses DistributionCurveWidget), audit-guarded layout. | 03 + 01 | PARTIAL — audit_anchor_bind guards geometry; Rack render/feel is human-only |

## Verified seams (grounded this session — prompts rely on these, do not re-guess)
- Uniform store / display accessor: finalRandomByStrand → random_[0][strand][step]
  (PatternEngine.hpp:419); warp mirror sits at the same post-everything stage.
- Melody warp composes at genPitchLive (PatternEngine.cpp:145): pickSemitone(weights,N,r_semi)
  is a discrete fader inverse-CDF → warp = pickSemitone(weights,N,invCDF(r_semi)).
- Octave warp: the roll/range map in the same fn (warp r_oct before the map).
- melodySource[16][16] is a DISPLAY/PERSISTENCE mirror, NOT a consumer — warp does NOT go there.
- Per-voice banks are flat float arrays indexed v*7+lane (macroSend[448], spread[112]) — the
  house pattern LaneDist[112] and warpW[112] mirror.
- Geometry guard: test/audit_anchor_bind.py (CI via run_all.sh:251); east(99)/macro(126) pass.

## Still OPEN / deferred (NOT in these four prompts)
- Parameter modulation (theta mod of distribution params): latch-at-draw-time, depth-blend
  universal; needs the param columns (prompt 04) to exist first. Later prompt.
- w send/tap ROUTING as a dependence control (the frailty/common-factor structure): reuses the
  per-voice send-tap contract (01) + the w-send/tap columns (04). Later prompt.
- Boolean-lane response curves: deferred BY PROOF (-18b §3) — booleans have a RATE not a
  distribution; warp there is a modulation-response curve, different name + UI, not core warp.
- Effective-distribution VISUALISER module: separate module, not the popup, not the bars.
- Subpanel migration of the modulation tier: BLOCKED by the adjacency/user-insertion bug;
  deferred until warp proven AND degradation designed, then done whole-tier.
- Output-stage clamp (co-clipping): deferred; CV-outs only; affine-range safe, hard-clamp makes
  atoms. Not core warp.
