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

## CURRENT STATE (as of this handoff — verify with git log)
HEAD ~2789a923 on lane-expander-refactor. RECENTLY DONE & VERIFIED:
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
- Scope (PROVISIONAL/experimental): warp clearly belongs on VALUE/RANGE lanes (melody, octave, note-length-
  in-clock-mode). THRESHOLD/boolean lanes (rest, legato, accent, qmix; variation IN GATE MODE is binary ->
  subgate rest/legato) warp = only a marginal modulation-response curve. DECISION: add warp to ALL lanes
  experimentally (cheap add/remove), FEEL it (esp. interactions: rest+legato gap-distribution, gate/subgate
  density), peel back on cost + FUN. Provisional cut = "warp-worthy <=> visualiser-worthy" (both need a real
  outcome distribution). Reasoning sets the default; Rodney's ears set the verdict.
- Warp PURPOSE: melody = per-voice handle on polyphonic CHAOS<->ORDER (global warp ~= fader adjust); octave =
  distributional layer of a 4-layer octave stack (Straits offset / Causeway mod / warp / copula correlation)
  that answers the Melodicer "limited octave control" criticism.
- t-copula NOT needed: shared-extreme-warp + Gaussian copula ~= t-copula's tail-clustering MUSICALLY, without
  the numerical pain (t-quantile unstable at high df, threatens reversibility).
- .dmcurve = JSON (consistent w/ .dmtune, shares the patch dataToJson schema). Distribution choice = patch
  state (no .dmdist file). Shipped defaults + user curves all .dmcurve.
NEXT: Phase 2.5 (add Mobius + Logit-sigmoid to Distribution.hpp, remove Arcsine, param knobs, convert-to-
PCHIP) then Phase 3 (integration: wire warp into a lane, Macro/Sands scope, .dmcurve load/save, and the
LIVE-PREVIEW KEYSTONE: drag curve -> Sands bars redistribute, uniform<->transformed toggle).

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
