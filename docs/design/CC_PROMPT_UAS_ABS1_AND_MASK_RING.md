# CC PROMPT — abs1 + painted-overlay: evaluate on the global-phrase ring

Implements the **abs1** and **painted-overlay** rulings from
`docs/design/CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC.md` (read its "Absolute (abs1 …) mode",
"Why all three modes exist", "Painted overlay — addressing (DECIDED)", the A-series and
P-series tables, and the "Where it lives / Reproduction / guard" sections — those are the
authority; this prompt is the task + surface + tests).

## Preconditions
- The **cycle-mode live-anchor fix lands first** (separate work). This change is independent
  of that machinery (different code paths) but shares `SequencerEngine`, so branch off
  `lane-expander-refactor` **after** cycle merges (or stack on the cycle branch) to avoid
  conflicts. Do **not** touch the cycle live-anchor code here.
- Non-negotiables (CLAUDE.md): verify against code + tests, never a narrative; reproduce-first
  (a test that FAILS before, PASSES after); branch off `lane-expander-refactor`, PR into it,
  never master. Build/test headless with `bash test/run_all.sh`.

## THE RULING IN ONE LINE
The painted-mask **storage stays on the `kMaskRing` (32) buffer** (authoring/display
unchanged), but the **evaluation / indexing / rotation ring becomes the current global phrase
length `G`**. A window or mask longer than `G` fills/wraps `G`; authored bits beyond `G` are
**dormant** (preserved, reappear when `G` grows). This is an **intended behaviour change** to
abs1 and the overlay whenever `G < 32` — existing goldens that encode full-32 behaviour must
be **updated to the new ruling (the A/P tables are the truth), not reverted to make old tests
pass.**

## Part A — abs1 (MASK_DYNAMIC): ring = the current global phrase
Fix surface (verify exact lines against the tree):
- `voiceFrameTickAt` abs1 branch (`SequencerEngine.cpp`, the `voiceFrameReadsFullPhrase`
  block, ~`:849`): `L = kMaskRing` → `L = <current global phrase length>` (the `lenG` already
  in scope); `rho` and `wnow` mod that length.
- `maskWindowContains` (`SequencerEngine.hpp` ~`:797`): parameterise the ring — `% kMaskRing`
  → `% G`, and `if (L >= kMaskRing)` → `if (L >= G)`. Thread `G` from the caller
  (`outputMaskPasses`, `effectiveOutputMask`).
- `windowMaskBits` / the abs1 arm of `effectiveOutputMask` (display): same ring, so the strip
  shows what actually sounds on the `G`-ring.
Behaviour (from the spec): sounds at phrase steps in `[O, O+L)` on the `G`-ring (gate =
offset+length, rotation ABSENT); content at a sounding step `s` = `(s + R − O) mod G` (region
start plays cell `R`). abs1 is a pure function of `(phraseStep mod G, O, L)` — no drift fix
needed.

## Part B — painted overlay: absolute on the `G`-ring
Fix surface:
- `rotMaskRing` (`SequencerEngine.hpp` ~`:793`): rotate on the evaluation ring `G`, not a
  hardcoded `kMaskRing` (add a ring arg or a `G` variant; keep the 32-wide storage).
- `effectiveOverlayPattern` (~`:847`) and `effectiveOutputMaskMacro` (~`:911`): the follow-R
  rotation `rotMaskRing(anchored, R − anchorR)` uses ring `G`.
- `outputMaskPasses` overlay apply (`pat >> phraseStep`) and its `phraseStep >= kMaskRing`
  guard: bound/index on `G`.
Behaviour: overlay ANDed onto the base gate per phrase step, **absolute on the `G`-ring**.
abs2 (`followR` off) static (muted step R-invariant); abs3 (`followR` on) muted step =
`(k − (R − anchorR)) mod G`. Anchor/re-pin law unchanged.

## Tests (reproduce-first) — the A/P tables become assertions
Add a headless test (register in `test/run_all.sh`) that encodes:
- **abs1 A1–A7** (spec table): A1 region+region-start-content; A2 rotation moves content not
  the sounding set; A4 `L ≥ G` ⇒ whole phrase; A6 the cycle-vs-abs1 contrast pair; A7 abs1
  drift-free under `G` 8→4 (sounding set a pure function of the current `G`, zero residue).
- **overlay P1–P5** (spec table): P1 period-`L` mask on cycle = locked; **P2 period≠`L` =
  phasing — assert the two loop iterations DIFFER (this is correct, not a failure)**; P3 abs2
  R-invariant vs abs3 `(k−ΔR) mod G`; P4 mute over an abs1 region ⇒ non-contiguous sounding
  inside the region; P5 `G` 8→4 ⇒ overlay re-interprets on the `G`-ring, bits beyond `G`
  dormant and restored when `G` grows back.

## Guardrails (the two traps already sprung once — do not re-spring)
1. **Do not assert raw cross-engine `A==B` for anything that wanders.** The correct property
   is within-engine / pure-function-of-current-`G` (abs1 and the overlay are stateless, so
   this is clean for them). Allow legitimate divergence.
2. **P2's per-repetition divergence is the feature.** A guard that flags "iteration 2 ≠
   iteration 1" would be wrong — assert the *shape* (same loop, mask-determined mutes), not
   byte-equality across iterations.
3. **Update goldens to the ruling, never revert the ruling to the goldens.** If an existing
   abs1/mask golden encodes full-32 behaviour and now fails, the golden is stale — recompute
   it on the `G`-ring per the A/P tables.

## Out of scope
Cycle-mode live anchor (separate, landing first). The authoring/display plane and the
16-lamp "twice-around" mapping (`kRingLamps`) — only the EVALUATION ring changes here.

## Done when
`run_all.sh` green with the A-series + P-series assertions added; existing abs1/mask goldens
recomputed on the `G`-ring (not reverted); no change to cycle or to the 32-wide storage;
"needs Rack feel-test" called out for anything not headless-verifiable.
