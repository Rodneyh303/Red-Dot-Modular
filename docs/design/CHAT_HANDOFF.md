# CHAT HANDOFF — read this first in a new chat
Purpose: get a fresh Claude productive on dot.modular WITHOUT re-explaining the project. The repo (code +
docs) is the durable memory; the chat is just the reasoning that produced them. When in doubt, CHECK THE
CODE — do not reason from a plausible story (hard-won lesson: a lying abstraction + plausible-but-wrong
narratives cost weeks; verify against code/tests, not vibes).

## WORKING MODEL (how Rodney and Claude work)
- THIS CHAT surface = HIGH-LEVEL ARCHITECTURE + design reasoning + writing CC prompts. Claude has repo
  access via git (token-based clone/commit/push); it reads code to ground answers and commits design docs.
- CLAUDE CODE (CC) = executes the plans/prompts this chat produces. CC is good at mechanical + TESTABLE
  work; it FAILS at behavioural bugs it can't verify (no Rack) and will confidently say "fixed" when not.
  So: give CC verifiable tasks with tests; Rodney feel-tests in Rack.
- Branch: lane-expander-refactor is the live branch (NOT master). Push there. CI is green via GitHub Actions.
- Repo: github.com/Rodneyh303/Red-Dot-Modular  (VCV Rack v2 plugin, C++).

## WHO RODNEY IS (for calibration, not flattery)
Quant finance (XVA), finance PhD + actuarial + maths, advanced C++ daily (adjoint AD). Deep probability/
numerics fluency — talk to him at that level (copulas, inverse-CDFs, PhiInv, conditioning are native).
Electronic musician; this is his instrument. Based in Singapore. Naming is Singapore geography (Straits,
Causeway, Raffles, Colonnades, Shophouse, Changi, Keppel, Fullerton, Sikit).

## WHAT THE INSTRUMENT IS (one paragraph)
dot.modular / Monsoon: a generative polyphonic sequencer built on a CORRELATIONAL randomness engine — a
GAUSSIAN COPULA couples voices (cross-voice = spread/CA) and time (temporal = SLEW, lag-1 corr r, signed
+/-), all REVERSIBLE (Philox-keyed, ~1e-15 round-trip -> scrub/reverse/offset). Plus microtonal tuning
authoring (Scala/.dmtune via Colonnades/Sikit/Duo), per-voice octave offsets (Straits), modulation
(Causeway). Positioning: "correlational randomness" — a 3rd category beyond subtractive (filter) and additive
(sum) randomness. Layered + optional: usable as an "enhanced Melodicer" (Vermona HARDWARE, not Vult) with the
lid closed, deep underneath. "Music is math" — the math objects ARE the musical gestures.

## HOW TO ORIENT (don't read 180 docs)
Read in this order:
1. docs/design/00_VISION.md, LAUNCH_INTENT_AND_STORY.md, MODULE_NAMING_AND_ROADMAP.md, ROADMAP_SEQUENCING.md
   — vision + module map + sequence.
2. The CURRENT-FOCUS docs (below) for whatever Rodney is working on now.
3. Grep/read specific code when a detail matters. plans/Monsoon_plugin_codebase_familiarization.md helps.
Then just ask Rodney "what are we working on" — he'll point you.

## CURRENT STATE (updated 2026-10-18b — verify with git log AND git ls-remote)
*** BRANCH REALITY — READ THIS BEFORE BELIEVING ANY "DONE" BELOW ***
Phase 2 (editor widget) and Phase 2.5 are NOT on lane-expander-refactor. They live on two UNMERGED
branches, and Phase 2.5 IS DONE on the second:
  - feat/dmcurve-widget-phase2  — the editor widget
  - feat/dmcurve-phase2-5       — contains the widget branch PLUS all of Phase 2.5
Verified in code (not commit messages): Distribution.hpp on phase2-5 declares 8 classes with Mobius and
LogitSigmoid present, Arcsine GONE; MonotoneCurve::fromDistribution exists; DmcurveDevHost has the generic
param knobs. test_distribution.cpp built and run from that branch: 56,395 passed, 0 failed. git merge-tree
says the merge into lane-expander-refactor is CONFLICT-FREE.
>>> ACTION OUTSTANDING: merge feat/dmcurve-phase2-5 into lane-expander-refactor. Phase 3 builds on ONE
    trunk; a second unmerged layer is how doc/code drift compounds. <<<

RECENTLY DONE & VERIFIED:
- The "brutal war" is WON: V16 melody-bar flicker + a hidden spread-lane crossing bug both TERMINATED at
  root (cause: lying voice/lane conventions — polyBankIndex returned v-1 but was NAMED/commented as 0=V2).
  Eradicated on BOTH axes (voice: [16][16] storage + voiceSlot + polyBankIndex->polyVoiceSlot; lane:
  SpreadInterp/SpreadManager/MonsoonSandsManager now speak unified PL_* order). Guarded FOREVER by
  CONTRACT TESTS (test_voice_conventions.cpp — the thing that caught it statically after 6 failed CC
  rounds). Verified 3 ways: display + tests + output jacks. LESSON: verification is the bottleneck; the
  contract tests turned an unfixable Rack-only ghost into a static failure.
- Phase 1 of the distribution feature MERGED: Distribution.hpp (double-precision probability layer) +
  MonotoneCurve.hpp (PCHIP, double) + tests. See DISTRIBUTION_LAYER.md.
- Phase 2 (dmcurve editor widget) BUILT & delightful: square dual-axis CDF/PDF display, all distributions
  render (parametric + drawn), live param morphing, drag-to-edit, "convert parametric -> editable PCHIP".
  See DMCURVE_EDITOR.md.

## THE ACTIVE FEATURE — output-layer distribution WARPING (the fun current work)
The big idea (READ THESE if working on it): OUTPUT_LAYER_DISTRIBUTION_MAPPING.md (the core + warp purpose
+ warp per-lane scope), DISTRIBUTION_LAYER.md (the 8-dist palette + precision), DMCURVE_EDITOR.md (editor),
EFFECTIVE_DISTRIBUTION_VISUALISER.md (the visualiser).
Key settled decisions:
- Because the copula keeps the output UNIFORM(0,1) end-to-end (the invariant defended in the slew/mix war),
  the output is a clean substrate: apply any F^-1 (inverse-CDF) at the output to reshape the MARGINAL per
  lane, correlation- + reversibility-preserving (Sklar: monotone marginal transforms leave copula invariant).
- Edit the CDF directly (not F^-1); PDF = its slope; F^-1 derived by reflecting + re-fitting PCHIP. No LUT —
  direct eval at control rate.
- 8-dist palette (all tunable, all cheap/closed-form or PhiInv-based inverse, NO LUT): Power, Mobius,
  Triangular, Kumaraswamy, Logit-normal, Logit-sigmoid, Truncated-normal, Drawn(PCHIP). (Dropped Arcsine:
  no params. Dropped Bates/true-Beta/t-copula: need iterative inverse — Beta^-1 harder than PhiInv, fine
  only if amortizable to set-time, disqualifying in hot/reversible paths.)
- The Distribution interface IS a monotone [0,1]->[0,1] bijection (cdf=fwd, invCDF=inv, pdf=slope) -> stat
  distributions, warps, and drawn curves are ONE object.
- Scope — SETTLED 2026-10-18 BY PROOF (supersedes the old "add to all lanes, FEEL it, peel back" plan;
  no peel-back needed). WARP APPLIES TO EVERY LANE'S UNIFORM — one mechanism, uniformly applied — and the
  per-lane verdict FALLS OUT OF WHAT EACH CONSUMER DOES:
  * THRESHOLD/boolean consumers (rest, legato, accent, qmix, variation-in-gate-mode): {F^-1(U) < t} ==
    {U < F(t)} EXACTLY, so with a STATIC threshold warp is identically a knob move — same realisations
    step for step, and the JOINT law of a boolean pair is fixed by the two thresholds (so the "rest+legato
    emergent gap distribution" blind spot is EMPTY). Subgates test a FLAT threshold => E[count]=J*F(t):
    also a knob move. Under MODULATION it IS real (dP/dm = d*f(t) — the warp's PDF is the gain profile;
    affine is a strict subset of monotone) but is externally reachable by a poly-cable waveshaper EXCEPT
    per-voice. Ship that later as a RESPONSE CURVE on the modulation input, different name + UI location:
    a boolean lane has no distribution, it has a RATE.
  * FADER-MAPPED consumers (melody, octave): full marginal effect via faderMap . F^-1.
  * CV OUTS: the IRREDUCIBLE consumer — continuous, rank survives to the jack, no knob equivalent.
  Same uniform, two consumers, opposite verdicts: the redundancy was never a property of the LANE, only of
  the thresholding.
- Warp PURPOSE: melody = per-voice handle on polyphonic CHAOS<->ORDER (global warp ~= fader adjust); octave =
  distributional layer of a 4-layer octave stack (Straits offset / Causeway mod / warp / copula correlation)
  that answers the Melodicer "limited octave control" criticism.
- t-copula REJECTED (2026-10-18, real reason): not cost and not "unstable at high df" — HEAVY TAILS ARE
  INCOMPATIBLE WITH A SATURATING FIXED-GRID LUT. PhiLUT works because Phi saturates (clamp beyond |z|>=6,
  truncation error 2e-9). t_nu has polynomial tails: nu=3 gives P(T>6)~5e-3, so clamping makes ~1% of draws
  ATOMS at the rails — the exact pathology the strict-increase constraint designs out. Reaching 1e-7 needs
  |t|~200-300, which wrecks body resolution on a uniform grid. Also REJECTED: a Bernoulli comonotone "spike"
  regime (gives lambda=p exactly, but it's a TUTTI UNISON not graded co-extremity, it makes realised
  follower-follower rank corr = p+(1-p)rho^2 which BREAKS the exact-correlation headline claim, and it
  punches a hole in the MA structure the OU bridge and slew readout assume). If tutti hits are wanted,
  build them as an explicit MUSICAL feature applied AFTER the copula.
- MODULATION OF DISTRIBUTION PARAMS: IN (reversed — the old "no param modulation" scope was holding warp to
  a standard nothing else meets; Monsoon's reversibility covers the DRAW STREAM, not knob/CV state). theta
  LATCHED AT DRAW TIME (~112 evals/step; continuous between-draw tracking is a separate future MODE).
  Universal handle = warp DEPTH w: out = (1-w)*U + w*F^-1(U), monotone for all w, works on the WHOLE palette
  INCLUDING drawn curves. w is PER-VOICE (Macro-delegated default, Sands unlock), a LANE PARAM not curve
  data. LEVEL and MOD-SOURCE should unlock SEPARATELY (mod-source heterogeneity is what changes the copula
  class). w MUST be bipolar or per-voice attenuverted, or negative common-factor dependence is unreachable.
- Shared modulation makes the copula GENUINELY RICHER (one-factor/frailty mixture; Archimedean at r=0 with
  Power). lambda (tail dep) is STILL ZERO and is not reachable without atoms. Say "common-factor dependence"
  and "co-spiking at audible quantiles", NEVER "tail dependence". See OUTPUT_LAYER_DISTRIBUTION_MAPPING.md
  "MODULATED WARP IS A REAL COPULA CHANGE" + SESSION_LOG_2026-10-18.md sections 5-7.
- .dmcurve = JSON (consistent w/ .dmtune, shares the patch dataToJson schema). Distribution choice = patch
  state (no .dmdist file). Shipped defaults + user curves all .dmcurve.
NEXT: Phase 2.5 is DONE on feat/dmcurve-phase2-5 (see CURRENT STATE) — MERGE IT FIRST. Then Phase 3
(integration: wire warp into a lane, Macro/Sands scope, .dmcurve load/save, LIVE-PREVIEW KEYSTONE).

PHASE 3 ARCHITECTURE — SETTLED 2026-10-18 (full reasoning: SESSION_LOG_2026-10-18.md + …-18b.md):
- STORAGE: warped_[16][7][16] MIRROR alongside random_ (the insertion seam is polyRandom() -> random_, the
  final post-everything uniform). UPDATED 2026-10-18b: the mirror holds the EFFECTIVE w-MIXTURE (one array,
  the single value BOTH engine and display read), NOT a separate full-warp array. Raw random_ is retained as
  the w=0 BASIS the mixture is built from. (This DROPS session-a's raw<->full-warp display toggle — see
  TIMING below — which removes the latched-vs-live hazard: there is only one value to show.) The LIVE-PREVIEW
  KEYSTONE still falls out (drag -> dirty -> rebuild -> Sands bars redistribute).
- TIMING (SETTLED 2026-10-18b, …-18b.md §2): DISPLAY rebuilds IMMEDIATELY on any curve/w edit and ALWAYS
  shows the effective w-mixture — NO raw↔warped toggle (it would show full-warp-at-w=1, a value not in
  effect = the plausible-wrong-display failure class). ENGINE latches w at DRAW TIME (step). Step is correct
  BY the phrase-boundary rule, not an exception: phrase boundary is reserved for items that break Sklar/
  uniformity per step; the w-mixture (1-w)·id+w·F^-1 is MONOTONE → Sklar-SAFE per step → step granularity.
  PROTECTED PROPERTY: sight leads sound by ONE STEP (edit→see instantly→hear next step). Do NOT quantise the
  display to the step "to sync" — that breaks the feel. w:1→0 IS the warp on/off (no separate mode switch =
  no second source of truth); continuous w gives a smooth uniform→warped morph.
- DOUBLE COMPUTE, FLOAT STORE. Exact double invCDF on the rebuild path — NO new numerical path, NO new LUT.
  (Do NOT borrow PhiInvFast: it's a 4097-entry binary search, plausibly SLOWER than the branch-light Acklam
  rational, and it exists for mix2 BIT-IDENTITY, not throughput.)
- WHY the mirror, not at-read: the project's OWN cacheability rule (GaussianCopula.hpp:37-46 — LUT for
  UNCACHEABLE hot callers, exact erfc for CACHED ones). The warp is a CACHED caller and the mirror IS that
  cache. Also single-source-of-truth: with at-read every consumer must REMEMBER to warp, and any site
  reading polyRandom() directly silently gets unwarped values while looking plausible — the polyBankIndex
  failure shape. (At-read is a legitimate alternative IF guarded by a contract test on every consumption site.)
- SCOPED DIRTY MASK, not a bool: Macro edit = one lane x delegating voices (<=256 cells, SKIP unlocked
  voices); Sands-east edit = one lane, one voice (16 cells). 1792 is the WHOLE array — pattern regeneration
  only, never a UI interaction.
- VISUALISER NEEDS FORWARD F, NOT INVERSIONS: P(bin k) = F(c_k) - F(c_{k-1}) at the fader boundaries, and it
  TELESCOPES (46 evals -> 45 masses). ~736 evals/frame worst case = ~0.35% of a core, so frame-rate
  modulation is affordable exactly, and a sampled-CDF LUT is actively WRONG for a modulated curve.
- LATCHED vs LIVE theta on the display: SHOW BOTH. Latched = solid bars (what's in effect), live = ghosted
  outline (what the next draw uses). The gap IS the information. Engine reads warped_ (latched); the
  visualiser path takes live theta and NEVER reads the mirror — crossing them = a plausible display of the
  wrong thing.
- OUTPUT-STAGE CLAMP: DEFERRED. On fader-mapped lanes a clamp is EXACTLY a fader setting (proved); on
  thresholded lanes a threshold shift; only the CV outs are irreducible. If ever built: AFFINE RANGE (no
  atoms, all claims intact) is the safe one; HARD CLAMP creates atoms. Params are LANE params, NOT .dmcurve.

UI ATTACHMENT — SETTLED 2026-10-18b (full detail: SESSION_LOG_2026-10-18b.md §7):
- EDITOR = a POPUP bound to the lane's LaneDist (Macro: lane; East: lane+voice). NOT per-cell bar editing
  (the deprecated per-cell code was unintentional, never used). THREE surfaces, each shows ONE thing: bars
  = engine output (on module), curve editor = authored F (popup), effective-distribution = warped marginal
  (SEPARATE visualiser module).
- WARP DISPLAY injection = ONE accessor: finalRandomByStrand (PatternEngine.hpp:419) feeds every bar AND
  the prob outs. Mirror holds the effective w-mixture → bars + CV outs + visualiser all read it. Prob outs
  carry the w-mix automatically.
- LAYOUT: DO NOT resize the probability bars. Insert columns LEFT (w-level, w-depth, w-mod jack, param1/2
  jack+attenuverter = 7 cols ≈63mm, BOTH panels) and RIGHT (Macro ONLY: w-mod-send, w-mod-tap ≈18mm).
  Full-size controls (no shrinking). Interim widths ~60HP east / ~74HP macro — accepted; subpanel exit later.
- SEND/TAP ALL ON MACRO, authored per-voice via Macro's TAB; East RECEIVES only (no East send/tap controls).
  macroTap[224] authored on Macro, read by East, never written by East — fold into the send-tap CC prompt.
- POPUP TRIGGER button near deleg/dir cells: Macro ALWAYS; East ONLY when lane delegated to East.
- GEOMETRY INTEGRITY = test/audit_anchor_bind.py (CI via run_all.sh:251): asserts every generator anchor
  has a widget bind + counts match SandsGrid constants. east(99)/macro(126) pass today. Width task =
  add anchors in panel_src gen + binds in widget + `counts` IN THE SAME COMMIT (the audit's one gap: a
  wrong/absent count is only caught if you added it). Only 4 modules migrated (east/macro/keppel/causeway);
  UNMIGRATED={} is empty (others just absent) — GOAL: migrate ALL panels eventually.
- SUBPANELS (param mods' right long-term home) BLOCKED by the adjacency problem, which is NOT a bespoke
  bug — it is the GENERAL connection/binding problem the suite already has a LOCKED SPEC + partial
  migration for. See docs/design/CONNECTION_MODEL_MIGRATION_NOTE.md (added 2026-10-18b) and the spec
  CONNECTION_MODEL_SPEC.md. Short version: Rack has no API to forbid a user inserting a module between a
  host and its expanders/subpanels; the adjacency walk then breaks. The FIX is the id-pairing model
  (hostPairId, resolveFollowedT<Monsoon>, graceful unbind) — already built + tested as generic templates
  (ui/IntertropicalPairing.hpp), used by Lantern/Changi/MicroTuning/ChangeAlley, and being rolled out per
  CONNECTION_MODEL_SPEC §7 stages (1-2 appear DONE: segment-rule findMonsoonEitherSide + Monsoon.pairId;
  3-4 = explicit selection + CA primary = PENDING). The subpanel-loses-adjacency fix IS stage 3, not a
  new mechanism. NEW STRAITS has the SAME exposure (StraitsBase.cpp spawns lanes + tracks them by walking
  rightExpander — insert-between breaks it; StraitsLaneExpander.hpp). So: do NOT write a bespoke subpanel
  fix (it would be a 5th mechanism — exactly what the spec forbids). Warp modulation-tier → subpanel
  migration is GATED on connection-model stages 3-4 DONE *and* warp proven. Warp ships on WIDE PANELS now,
  independent of all this (prompts 01-04).

STILL OPEN — BLOCKS CC:
1. Distribution OWNERSHIP: ANSWERED 2026-10-18b (SESSION_LOG_2026-10-18b.md §1). NOT unique_ptr (allocates
   on the modulation control path) and NOT variant (wrecks the flat array). Split identity+params (per
   voice) from behaviour (not): store a FLAT POD `LaneDist{uint8_t kind; float p[4];} laneDist[112]` beside
   spread[112] — kind is a TAG, params ≤4 doubles, behaviour is a switch(kind) → the SAME closed-form math
   as Distribution.hpp exposed as free fns. Distribution.hpp STAYS as the authored/editor/set-time form;
   add a tested one-way `lower(const Distribution&)` at the engine-entry seam (same shape as macroBase→
   published-deltas). Drawn curves = index into a set-time PCHIP pool (not inline). Guard the lying-
   abstraction: ONE inline fn per curve shared by both the subclass and the switch; contract test
   lower(d).invCDF(u)==d.invCDF(u). Remaining = the CC build + tests, not a decision.
2. melodySource[16][16] is WRITTEN in four places and READ in NONE outside persistence. The degree mapping
   does not consume it. Since faders ARE a discrete inverse-CDF (widths = mass), faderMap . F^-1 is where
   melody warp lands — PIN THIS SEAM BEFORE DESIGNING IT. Do not design it on a guess. STILL OPEN.
3. (CLOSED 2026-10-18b) Change Alley is a SEPARATE layer (latent-assignment operator; different files, no
   crossover). CA and w-modulation are INDEPENDENT dependence handles that COMPOSE; neither routes the
   other. (Session-a's "CA routes w's factor structure" claim WITHDRAWN.)

IMMEDIATE CC WORK (ships FIRST, alone, off lane-expander-refactor, before any Phase 3):
- docs/design/CC_PROMPT_per_voice_send_tap.md — fixes a REAL regression found by code-reading: the
  Macro→East SEND is per-voice (macroSend[448]) but the PRE/POST TAP is Macro-GLOBAL (globalTap[14]),
  forcing every receiving voice onto one tap. Fix = per-voice tap (macroTap[224]); the non-mechanical
  heart is that per-voice tap moves the choice DOWNSTREAM of the publish, so Macro must publish BOTH
  endpoints (macroPreDelta/macroPostDelta) and each voice lerps with its own tap. Hardens the publish-
  both-endpoints contract the w-send + per-param sends inherit. 4-assert contract test on SpreadResolver
  (confirmed compiles standalone). NB the tap is a CONTINUOUS lerp effAtt=1+(att-1)·tap, not a toggle.

## RECURRING HARD-WON PRINCIPLES (so you don't relearn them)
- VERIFY against code/tests, never a plausible narrative. (polyBankIndex lied; so did some of my design
  stories until Rodney caught them.) Check the code when in doubt.
- Keep the LAYERS distinct: base(knob) / correlation-intensity(slew+spread) / F^-1(marginal) / copula(the
  dependence we don't touch). Conflating them is where bugs AND confused reasoning hide.
- Perceptual questions (does it sound good) -> build cheap + FEEL, don't settle analytically. Reasoning sets
  the default, experience sets the verdict.
- "Tractable inverse or it's out" governs both distributions (closed-form/PhiInv or set-time LUT) and the
  copula (Gaussian's PhiInv, not t's Beta-inversion).
- Precision: the copula/PhiInv MATH layer is double (reversibility-critical, U_EPS 1e-12); storage is float;
  cast at the storage boundary. Distributions are double (siblings of PhiInv).
- CC can't verify behavioural/Rack things — give it tested/mechanical tasks; don't let it declare victory on
  feel. Rodney feel-tests.

## TRANSITION NOTE
This is handoff #N — context keeps getting lost across chats despite the docs. The fix: docs ARE the memory;
this file is the index. When starting fresh, read 00_VISION + this file + the current-focus docs, confirm
state with `git log`, and ask Rodney what's current. Don't re-derive; read + verify.
