# ZAI PROMPT — Resolver Step 2: WARP as <WarpPolicy> + CLOSE the consumer gate

CONTEXT FOR THE AGENT: you are working the repo Red-Dot-Modular, branch `lane-expander-refactor`
(Step 1 landed at 152159ce + build fix e0a914bf — the skeleton EXISTS and is proven). Read FIRST:
`CLAUDE.md` → `docs/design/SANDS_MODIFIER_RESOLVER_DESIGN.md` §BUILD ORDER step 2 (your spec) →
`src/dsp/SandsModifierResolver.hpp` (the skeleton + SpreadPolicy as LANDED — your API) →
`docs/design/SANDS_MODIFIER_RESOLVER_SKETCH.md` §4 (WarpPolicy target shape) → `src/dsp/WarpF.hpp`
(the WarpBank section) → `docs/design/ZAI_PROMPT_resolver_step1_spread.md` (the conventions you
inherit: bridge-test pattern, honesty block, exit report).

## THE TASK (one commit)

Warp becomes the SECOND policy of the shared skeleton; the hand-rolled delegation branch dies; and
this step CLOSES the harness's biggest known seam — the consumer gate — so the manifest shrinks for
the first time. Unlike step 1 this step contains ONE deliberate behaviour change (the gate flip);
everything else is bit-identical. The distinction is pinned below — get it exactly right in the
exit report.

## THE SPEC

1. **WarpPolicy** in `src/dsp/SandsModifierResolver.hpp` (POLICIES section, beside SpreadPolicy):
   - `struct WarpCell { dotModular::WarpF f; float w; };` — the resolved (curve, depth) pair.
     WarpF is a trivially-copyable std::variant (test_warp_f_bank S-series pins it); by value is
     correct (resolution runs on the rebuild path, not per-sample).
   - `using Value = WarpCell; struct Extra {};`
   - `scaleCv(raw) = raw` (identity); `addCv(v,c)/addSend(v,s)` = `v.w = clamp01(v.w + c)`, f
     UNTOUCHED (CV/send modulate DEPTH only; the curve is set-time — sketch §4; curve-param
     modulation is a future θ policy, do not fuse it here).
   - `applyDelegated(mb, td, extra) = { mb.f, clamp01(mb.w + td) }` — bit-identical to today's PURE
     SELECTION because no warp send/tap plumbing exists (td ≡ 0, clamp01 identity): the general
     form is DORMANT, not a behaviour change. Say this in the commit body.
   - `constrain(v, extra)` = clamp `v.w` to [0,1], f untouched. Identity today (the mix's "w ∈ [0,1]
     is the caller's contract — no clamp" comment is about the MIX; the RESOLVER clamps the resolved
     w, a different layer, and it is identity on valid inputs).
   - `#include "WarpF.hpp"` at the top of SandsModifierResolver.hpp; the SKELETON section (CvTerm,
     concept, template) must stay free of dotModular types — only the POLICIES section touches them.
   - `static_assert(SandsModifierPolicy<WarpPolicy>)`.
2. **WarpBank routes through the resolver — the hand-rolled branch DELETED** (design doc's demand):
   - `WarpBank::warpedValue(v, lane, u, delegated)` (WarpF.hpp:361-367) keeps its SIGNATURE but the
     body becomes: build `SandsModifierResolver<WarpPolicy>::Inputs` (base = own {warpF[i],warpW[i]},
     macroBase = {globalWarpF[lane],globalWarpW[lane]}, delegated = the flag), take the resolved
     WarpCell, return `(1.0 - w)*u + w*invCDF(f, u)` on the RESOLVED pair. The `if (delegated)`
     hand-copy is gone.
   - The 2-arg `warpedValue(v, lane, u)` (:354-357): same routing with delegated=false (one authority).
   - `effectiveF/effectiveW` (:368-373): become one-line delegates returning the resolver's resolved
     cell's f/w. They have NO production callers (verified) but test_warp_f_bank:285-289 pins their
     selection semantics — they must keep passing UNCHANGED.
   - Signatures are frozen: the harness U2 oracle (test_lane_voice_uniformity.cpp:206, :358) calls
     `bank.warpedValue(v, lane, u, true)` and must keep passing UNCHANGED.
3. **CLOSE the consumer gate — both gates, they flip TOGETHER:**
   - Sounding: `polyRandomSrc` (SequencerEngine.hpp:542-547) — drop the MEL/OCT ternary, route ALL
     lanes through `pe.warpRow`. Every poly consumer (SequencerEngine.cpp:1080-1442: REST, VAR,
     QMIX, ACCENT, LEGATO, MELODY, OCTAVE) reads through this one choke point; nothing else changes.
   - Display: `finalRandomByStrand` (PatternEngine.hpp:492) — same flip (the Sands-bars + prob-CV
     funnel). The two gates must agree lane-for-lane (the "flips TOGETHER" rule, SE comment :540).
   - Update BOTH gate comments — they currently document the deferral ("boolean lanes deferred by
     proof; q-mix with the w send/tap routing"); rewrite them to state the all-lanes routing and why
     it is safe (w=0/identity ⇒ mirror ≡ raw, bit-exact, pinned).
   - **Manifest**: DELETE the `WARP-CONSUMER-GATE` entry from `knownSeams()`
     (test_lane_voice_uniformity.cpp:86-91) — the manifest's own closure text ("route ALL lanes
     through warpRow ... then delete") is exactly this step. `seam_warp_consumer` goes with it.
     STRICT mode must then red on EXACTLY ONE seam: DIR-SLOT0-ROW-INERT (step 4 closes it — do not
     touch it).
4. **Test updates — the ONLY allowed edits** beyond the manifest deletion and the new bridge test:
   - `test_warp_mirror.cpp` S6 pins the OLD routing on both gates (:332 display, :363 sounding —
     "RHYTHM stays raw" / "other lanes raw"). UPDATE both sub-tests to pin the NEW all-lanes routing
     (keep the null-bank sub-checks — they are the byte-identical-legacy pin and stay green as-is).
     Do NOT delete S6 — it remains the routing pin, now for the correct state.
   - NOTHING else in test/ changes. test_warp_f_bank (259 checks), U1/U2/U4, and ALL spread tests
     pass UNCHANGED — if you touch them you changed behaviour; stop and re-derive.
5. **NEW `test/test_resolver_warp_bridge.cpp`** (header-only; register as
   `test_resolver_warp_bridge|` — same compile shape as test_warp_f_bank): extends step 1's pin.
   Verbatim copy of the OLD 4-line delegated mix as oracle; seeded fuzz over banks (w ∈ {0, 1, mid},
   F roster incl. Identity/parametric/DrawnWarp-via-pool) × u ∈ [0,1] × delegated ∈ {false,true},
   asserting (a) `warpedValue(v,lane,u,deleg)` is BIT-IDENTICAL to the oracle, (b) the resolver's
   resolved (f,w) equals the old `effectiveF/effectiveW` selection (w same bits; f same alternative).
   Print the case count.

## WHAT IS BIT-IDENTICAL vs THE ONE DELIBERATE CHANGE (the honesty block — copy its shape into the commit body)

- BIT-IDENTICAL: spread everywhere (untouched); warp VALUE resolution (td ≡ 0 ⇒ resolver ≡ pure
  selection); no-warp and default hosts (w=0/identity ⇒ mirror row ≡ raw row, bit-exact, pinned by
  test_warp_f_bank S4/S7 + test_warp_mirror S1/S2); null-bank hosts (consumers read raw, unchanged).
- THE DELIBERATE CHANGE: an AUTHORED warp on QMIX/REST/ACC/VAR/LEG now reaches that lane's
  consumers — before, the mirror held it and every consumer silently dropped it (the seam). On THIS
  branch the warp UI cannot author those lanes yet (MEL/OCT only, feat/warp-ui), so practical
  exposure is nil until the merge — the flip makes this branch CORRECT rather than observable.

## VERIFIED FACTS (grounded 2026-10-20 at e0a914bf — re-verify if HEAD moved)

1. `PatternEngine::warpMixCell` (PatternEngine.cpp:131-135) is the ONLY production caller of the
   delegated `warpedValue` form; the mirror + dirty-mask scopes (rebuildWarpMirror*, :139-190) call
   only warpMixCell — value resolution has ONE production entry point.
2. `effectiveF/effectiveW` have no production callers; test-pinned only (test_warp_f_bank:285-289).
3. `polyRandomSrc` is the single poly consumer choke point (accessors at SequencerEngine.hpp:676/:685
   + the per-lane reads at SequencerEngine.cpp:1080-1442); `finalRandomByStrand` is the single
   display funnel (PatternEngine.hpp:492).
4. The harness U3 check compares ROW IDENTITY (`&src[0] == &pe.warpRow(v0,l0)[0]`) for all 16×7 —
   the flip makes it green with no harness edit.
5. The manifest entry WARP-CONSUMER-GATE (test_lane_voice_uniformity.cpp:86) names this exact
   closure and is the ONLY warp entry; DIR-SLOT0-ROW-INERT (:93) is direction's, step 4's.
6. WarpF lives in `dotModular`, the skeleton in `redDot` — the include is one-directional
   (SandsModifierResolver.hpp → WarpF.hpp); no cycle (WarpF.hpp does not include the skeleton).

## HOW TO RUN

- `bash test/run_all.sh` (RACK_DIR needs `dep/include/simde/`) → **67 suites**, all green, anchors green.
- `UNIFORMITY_STRICT=1 bash test/run_all.sh test_lane_voice_uniformity` → EXACTLY the
  DIR-SLOT0-ROW-INERT known seam (and nothing else), exit 1.
- Targeted: `bash test/run_all.sh test_resolver_warp_bridge` / `test_warp_f_bank` / `test_warp_mirror`
  / `test_lane_voice_uniformity`.
- Plugin build: `make` (Rack SDK) — REQUIRED. The rig compiles PatternEngine ($PE) so warpMixCell IS
  rig-covered, but the `warpDelegated_` mirror WIRING (MonsoonSandsManager.cpp:144-155) is not — only
  a real build proves it compiles. Report the result.

## COMMIT + EXIT REPORT

One commit, `refactor:` subject; body carries the honesty block (bit-identical surfaces vs the gate
flip), the manifest deletion (FIRST SHRINK — say so), the S6 updates, the bridge case count, and the
plugin-build result. Exit report: what the fuzz covered, any sketch-vs-code delta you reconciled,
anything that surprised you. Verify against code, never narrative.

## NOT THIS TASK

- Do NOT write LorPolicy/DirPolicy/DistributionPolicy — steps 3-4, each its own commit.
- Do NOT touch the DIR-SLOT0-ROW-INERT manifest entry or the laneDir path (step 4).
- Do NOT touch `MonsoonSandsManager.cpp:144-155` (the warpDelegated_ mirror wiring — Rack-layer;
  the Macro→East leak SITE lives there and is CC_PROMPT_07 territory on feat/warp-ui; this step
  fixes the VALUE side given the boolean, and the exit report must say so — the Rack bug is not
  declared dead until that wiring is verified in Rack).
- Do NOT merge feat/warp-ui (Rodney's call). NOTE for Rodney: this step re-implements CC_PROMPT_05's
  all-lanes routing on THIS branch — expect a trivial conflict there at merge time (take either side).
- Do NOT touch SpreadPolicy, the spread bridge, or any spread test.
