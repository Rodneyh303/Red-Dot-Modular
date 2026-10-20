# ZAI PROMPT — Resolver Step 1: EXTRACT SandsModifierResolver from SpreadResolver as <SpreadPolicy> (byte-identical)

CONTEXT FOR THE AGENT: you are working the repo Red-Dot-Modular, branch `lane-expander-refactor`.
Read FIRST, in this order: `CLAUDE.md` → `docs/design/SANDS_MODIFIER_RESOLVER_DESIGN.md` (the
authority; §BUILD ORDER step 1 is your spec) → `docs/design/SANDS_MODIFIER_RESOLVER_SKETCH.md`
(the target shape; the REAL header is the math truth) → `src/dsp/SpreadResolver.hpp` (in full) →
`docs/design/ZAI_PROMPT_stage0_uniformity_harness.md` (the net you build on; its exit contract
applies to you too).

## THE TASK (one commit, no behaviour change)

Turn SpreadResolver — today the ONE tested delegation authority — into the FIRST POLICY of the
shared skeleton, proving the skeleton generalizes by being BYTE-IDENTICAL to what ships:

1. NEW `src/dsp/SandsModifierResolver.hpp` (namespace `redDot`):
   - `concept SandsModifierPolicy` — the C++20 concept from the sketch (scaleCv / addCv / addSend /
     applyDelegated / constrain + `Value` + `Extra`). Real concept, NOT a static_assert fallback
     (C++20 confirmed CI-green, commit 8f763a94). `static_assert(SandsModifierPolicy<SpreadPolicy>)`
     in the header proves it bites.
   - `struct CvTerm` at NAMESPACE scope, shared: `{ connected, unitVoltage, atten;
     contribution() = connected ? unitVoltage * atten : 0.f }` — the ×2 LEAVES CvTerm (see §×2).
   - `template <SandsModifierPolicy Policy> struct SandsModifierResolver`: `Inputs` (base, delegated,
     macroPresent, ownCv, eastCv, macroBase, macroSend, macroPreDelta, macroPostDelta, macroTap,
     `Policy::Extra extra{}`), `tappedDelta()`, `effective()` — control flow EXACTLY SpreadResolver's
     (delegated → applyDelegated+constrain; else base → +ownCv → +eastCv → +send·tappedDelta →
     constrain; skip-if-absent, end-clamp-once).
   - The header comment carries the ONE-SENTENCE MEMBERSHIP RULE from the design doc (per-(voice,lane)
     AND delegates Macro/East; slew/AB are OUT — uniform across the group; topology is NOT here) so no
     future policy folds a wrong member in.
   - `struct SpreadPolicy`: Value=float, Extra={}, `scaleCv(raw)=raw*kBipolarSpan`, addCv/addSend = +,
     `applyDelegated(mb,td)=mb+td`, `constrain=clamp[-1,1]`. kBipolarSpan(2.f) lives HERE now.
2. SLIM `src/dsp/SpreadResolver.hpp` into an API-COMPATIBLE shell — every name used by production
   call sites and tests must keep resolving unchanged: `SpreadResolver`, `SpreadResolver::CvTerm`
   (alias to the shared one), `::Inputs` (= the resolver's), `::effective`, `::tappedDelta`,
   `::clampSpread`, `::kSpreadLanes`, `::kBipolarSpan`. `effective()`/`tappedDelta()` become one-line
   delegates to `SandsModifierResolver<SpreadPolicy>`. Single definition per constant (define in the
   policy, alias in the shell) — no duplicate magic numbers.
3. The ×2 RELOCATION (the one deliberate semantic move, outputs unchanged):
   - BEFORE: `CvTerm::contribution()` multiplies by kBipolarSpan (SpreadResolver.hpp:47).
   - AFTER: `contribution()` is raw uV·atten; `SpreadPolicy::scaleCv` applies ×2.
   - EFFECT ON CALLERS: `SpreadResolver::effective()` outputs are UNCHANGED (scaleCv re-applies the
     ×2 inside the skeleton). The ONLY code that must be touched is the harness U6 inline oracle —
     `test/test_lane_voice_uniformity.cpp:494-495` calls `in.ownCv.contribution()` directly; make it
     `... * SpreadResolver::kBipolarSpan` (or route through the policy) and flip the NOTE comment at
     :488 to the new convention. That is the ONLY allowed test edit. `test_spread_resolver.cpp` and
     `test_send_tap.cpp` assert on effective() OUTPUTS and must pass UNCHANGED — if you edit them,
     you changed behaviour; stop and re-derive.

## THE BYTE-IDENTICAL GATE (the whole point of step 1 — non-negotiable)

- NEW `test/test_resolver_spread_bridge.cpp` (header-only; register in run_all.sh as
  `test_resolver_spread_bridge|` — no companion sources): SEEDED deterministic fuzz (fixed seed,
  printed) over ≥100k Inputs covering every branch: delegated×free; macroPresent×absent;
  ownCv/eastCv connected×not; macroTap ∈ {0, 0.25, 0.5, 1, random}; rails and beyond (base ±2, sums
  overflowing ±1, large-then-opposite pairs — the end-clamp case). The oracle is the OLD 5-line
  inline formula DUPLICATED IN THE TEST (tests may duplicate; production may not). Assert the new
  resolver's float is BIT-IDENTICAL to the oracle (same bits, not |a-b|<eps) and that the compat
  shell `SpreadResolver::effective` returns the identical bits. This test is the permanent
  byte-identical pin — later steps (warp/LOR/direction) extend the same pattern for their policies.
- Existing pins stay green UNCHANGED: `test_spread_resolver`, `test_send_tap`, harness `U5`/`U6`.
- Full suite 65 → 66 suites green + anchor audit green (east 99 / macro 126 / keppel 19 / causeway 55).
- `UNIFORMITY_STRICT=1` still fails on EXACTLY the two known seams (WARP-CONSUMER-GATE,
  DIR-SLOT0-ROW-INERT) — this step touches neither warp nor direction; the manifest is FROZEN.

## THE COMPILE-COVERAGE GAP (the "05 green while Rack broke" lesson — do not skip)

The test rig does NOT compile `MonsoonSandsManager.cpp` — the three PRODUCTION call sites of
`SpreadResolver::effective` (MonsoonSandsManager.cpp:393 slot-0 Sands write, :659 Macro publishGlobal,
:723 Macro standalone) are only proven by a real plugin build. REQUIRED: build the plugin against the
Rack SDK (`make` with RACK_DIR set) and report the result. If the SDK is unavailable in your
environment, SAY SO in the exit report — an unverified production compile is a red flag, not a pass.

## VERIFIED FACTS (grounded 2026-10-20 on this branch — re-verify if HEAD moved)

1. Production callers of SpreadResolver::effective: EXACTLY MonsoonSandsManager.cpp:393, :659, :723.
   All default-construct Inputs and assign fields by name — the added `Extra extra{}` member is
   source-compatible with all three.
2. No production code outside SpreadResolver.hpp calls `tappedDelta`, `clampSpread`, or
   `CvTerm::contribution()` (the MonsoonSandsManager ~L631 `tappedDelta` is the PUBLISH-side lambda
   that PRODUCES macroPreDelta/macroPostDelta — unrelated, do not touch). Test-side direct
   `contribution()` users: ONLY harness U6 (:494-495).
3. kBipolarSpan has no references outside SpreadResolver.hpp and docs.
4. The sketch's Inputs/policy shape was validated against all six future policies (SKETCH §WHAT IT
   PROVED); where sketch and real header disagree, the REAL header wins for spread's math and the
   delta goes in the commit body (sketch is target shape, not code to ship verbatim).
5. CI runs the full unit suite on all 3 platforms (build.yml §"unit tests") — suite green is a hard
   gate, not a nicety.

## HOW TO RUN

- `bash test/run_all.sh` (RACK_DIR needs `dep/include/simde/`; CI sets it) → 66 suites, all green.
- `UNIFORMITY_STRICT=1 bash test/run_all.sh test_lane_voice_uniformity` → the same 2 known seams, unchanged.
- Plugin build: `make` (Rack SDK) — must succeed; the 3 call sites are your compile proof.

## COMMIT + EXIT REPORT

One commit, `refactor:` style subject, body lists: the ×2 move, the new files, the U6 oracle edit,
plugin-build result, bridge-test case count. Exit report: what you verified vs what you could not
(headless vs Rack), any sketch-vs-code delta you reconciled, and anything that surprised you — the
process rule is non-negotiable: verify against code, never narrative.

## NOT THIS TASK

- Do NOT write WarpPolicy / LorPolicy / DirPolicy / DistributionPolicy — steps 2-4, each its own
  commit gated on this net. The concept must ADMIT them (sketch §4/§5/§6) but none ships now.
- Do NOT touch WarpF::warpedValue, combineLOR, or the laneDir path — their bugs close in their steps.
- Do NOT merge feat/warp-ui (Rodney's call; it changes the harness manifest).
- Do NOT touch the KNOWN_SEAMS manifest. It only ever shrinks, at stage close, never to make a test pass.
- No behaviour change anywhere in src/ — this step is extraction + proof, nothing else.
