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

## CURRENT STATE (updated 2026-10-18 — verify with git log AND git ls-remote)
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

PHASE 3 ARCHITECTURE — SETTLED 2026-10-18 (full reasoning: SESSION_LOG_2026-10-18.md):
- STORAGE: warped_[16][7][16] MIRROR alongside random_ (the insertion seam is polyRandom() -> random_, the
  final post-everything uniform). Raw random_ retained as the pre-toggle source, so the uniform<->transformed
  toggle is two array selections and the LIVE-PREVIEW KEYSTONE falls out of the storage shape (drag -> dirty
  -> rebuild -> Sands bars redistribute).
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

STILL OPEN — BLOCKS CC:
1. Distribution OWNERSHIP: value variant vs unique_ptr. Modulation made it urgent — the structure holds
   THREE lifetimes (per-voice modulatable scalar w written on the CONTROL path; per-voice curve object read
   on rebuild; Macro-delegated default behind both) and the dirty mask keys off all three. Up to 119
   instances. unique_ptr means ALLOCATION DURING MODULATION => points at a value variant, but READ THE
   SANDS/MACRO DELEGATION CODE BEFORE COMMITTING.
2. melodySource[16][16] is WRITTEN in four places and READ in NONE outside persistence. The degree mapping
   does not consume it. Since faders ARE a discrete inverse-CDF (widths = mass), faderMap . F^-1 is where
   melody warp lands — PIN THIS SEAM BEFORE DESIGNING IT. Do not design it on a guess.
3. UNVERIFIED: Change Alley routing (the claim that CA routing w's mod source = routing the FACTOR
   STRUCTURE). Read the CA docs before relying on it.

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
