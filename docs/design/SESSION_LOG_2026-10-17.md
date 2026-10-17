# SESSION LOG — 2026-10-17 (long architecture chat; digest, not verbatim)
This is the distilled REASONING of one very long chat — the connective narrative the per-topic docs lack
(how threads related, what order, what got CORRECTED). Verbatim chat not saved (couldn't export); its
durable output is the design docs committed throughout + CHAT_HANDOFF.md. Read that handoff first; this is
"how we got here, this session".

## ARC (chronological threads)

### 1. Subgate variation -> the REST SOURCE design (early)
Started: "why does variation gate ghost but not ratchet?" Worked through: rationale (departure vs
articulation) -> uniform "variation filter" checkboxes -> realized the tension meant the ROLE was wrong ->
landed on per-subgate REST SOURCE selector {None, MainRest, Variation, Both-OR}. One rest per cell from a
chosen source dissolves the two-gate tension + the missing "rest beats variation" toggle. Traced facts:
ratchet in-gate only (main rest closes gate -> no ratchet, STRUCTURAL); ghost in-gap (gate-shape gaps, not
rest lane). Then: variation = the assignable GATE-MODE spare lane (note length = gate length in gate mode);
subgate legato gets a symmetric source picker; one-lane-two-roles dissolves because rest-beats-legato
resolves the overlap. CAPSTONE: subgates inherit the FULL 2D copula (cross-voice spread + temporal slew) for
free by routing through first-class lanes -> subgate density breathes/clusters. Docs: GATE_SUBDIVISION_STEP_
GATE.md.

### 2. The TECH-DEBT war (the brutal core of the session)
Rodney: "nice ideas once we clear the debt causing constant stress + inexplicable chaos." Audited: root =
VOICE-INDEX CONVENTION fragmentation (2 bases: voiceSlot 0=V1 vs voiceIdx/polyIdx 0=V2; ~8 names; storage
split [15]+mono). Then LANE-axis split too (SpreadInterp etc. old order vs unified PL_*). Collapse to
[16][16] + voiceSlot. Also CLASS-level vestigial debt (MonoSandsParameterManager, LaneMapping+SandsLaneNames,
identity-conversions) = the THIRD level. "mono is not special" must hold at storage + convention + class.
Parallelized + precompiled test harness (run_all.sh) — fixed an MSYS2 hang I introduced.
THE BUGS: V2 melody-bar flicker (fixed via collapse) then it MOVED to V16. 6+ CC rounds failed; 3 of MY
static fixes failed (2 were no-ops — polyVoice()==currentSlot() for poly). ROOT finally found: VoiceResolver
::polyBankIndex LIED — code returns v-1 but name/comments said "0=V2, +1". The self-contradiction is why
nobody could reason about it. THE BREAKTHROUGH: verification-first. Built contract tests (test_voice_
conventions.cpp) that caught the LANE-convention split STATICALLY (turned an unfixable Rack-only ghost into
a red test). Fix = unify the lane convention (SpreadInterp speaks PL_*). This TERMINATED the V16 flicker AND
caught a HIDDEN bug (spread knobs drove the WRONG lane, 5 of 7 crossed when spread!=0). polyBankIndex ->
polyVoiceSlot (truth-fixed). Verified 3 ways (display + tests + output jacks); CI green; merged.
LESSON (the meta-lesson of the whole session): VERIFICATION is the bottleneck, not the fix. A flaky/missing
verification layer costs weeks; a test that makes a bug STATIC fixes it in one pass. Docs: convention_
unification_INCOMPLETE.md, V16_FLICKER_root_smell.md, VOICE_CONVENTION_TESTS.md, COLLAPSE_15_TO_16.md.

### 3. Morale pivot -> the OUTPUT-DISTRIBUTION WARPING feature (the fun)
Prompted by a VCV forum post (Morris, "subtractive and additive randomness"). Insight: dot.modular is a
THIRD category — CORRELATIONAL randomness (shape dependence, preserve marginals = the copula). The forum's
wishes (import external values/scales) are already shipped (QMIX, quantiser mode, gate mode, Scala/.dmtune
via Colonnades/Sikit/Duo) + more. THE FEATURE: because the copula keeps output UNIFORM(0,1) end-to-end (the
invariant defended in the slew/mix war), apply any F^-1 at the OUTPUT to reshape the per-lane MARGINAL —
correlation- + reversibility-preserving (Sklar). "Knew there was a reason to insist on uniform-preserving
modulation" — the correctness invariant turned out to ENABLE a capability. Docs: OUTPUT_LAYER_DISTRIBUTION_
MAPPING.md, POSITIONING_correlational_randomness.md.

### 4. Distribution design decisions (settled this session)
- Edit the CDF directly (not F^-1); PDF = slope; F^-1 = reflect+re-fit PCHIP. NO LUT (control-rate direct
  eval; Beta^-1 harder than PhiInv, only OK if amortizable to set-time; not in hot/reversible paths).
- Precision = DOUBLE (distributions are siblings of the double PhiInv/copula math layer; cast to float at
  storage). No template (fights the polymorphic interface; YAGNI).
- Palette (8, all tunable, cheap inverse): Power, Mobius, Triangular, Kumaraswamy, Logit-normal, Logit-
  sigmoid, Truncated-normal, Drawn(PCHIP). Dropped Arcsine (no params). Rejected Bates/true-Beta/t-copula
  (iterative inverse). Mobius pdf = a/((1-a)x+a)^2 (bounded gentle tilt, identity at a=1).
- The Distribution interface IS a monotone [0,1]->[0,1] bijection -> stat dists, warps, drawn curves are ONE
  object. Selection principle (3 clauses): distinct shape + cheap inverse + TUNABLE (fits morph-UI).
- .dmcurve = JSON (shares patch dataToJson schema; .dmtune consistency). Distribution = patch state (no
  .dmdist file). Shipped defaults + user = all .dmcurve. t-copula not needed: shared-extreme-warp ~=
  t-copula's tail clustering MUSICALLY, without the numerical pain. Docs: DISTRIBUTION_LAYER.md, DMCURVE_
  EDITOR.md.

### 5. Warp PURPOSE + SCOPE (reasoned carefully; some of my claims CORRECTED by Rodney)
- Melody warp = per-voice handle on polyphonic CHAOS<->ORDER (global warp ~= fader adjust, redundant). Mono
  value iff the lane LACKS per-outcome faders (octave/note-length have none -> warp is the only shaper ->
  mono value; melody has faders -> polyphonic-only).
- Octave warp = distributional layer of a 4-LAYER octave stack (Straits deterministic offset / Causeway mod
  / warp / copula cross-voice correlation) -> comprehensively answers the Melodicer "limited octave control"
  criticism.
- CORRECTED: I invented a "warp=where / slew=how" orthogonality — WRONG (slew is shared/per-lane, not
  per-voice). Rodney caught it. The truth: it's all chaos-level control; faders/warp mediate how shared slew
  becomes note-motion.
- Threshold/boolean lanes (rest, legato, accent, qmix; variation in GATE mode is binary = subgate rest/
  legato): warp only reshapes a modulation-response curve (marginal, redundant with Straits knob+mod).
- DECISION (provisional/experimental): add warp to ALL lanes (cheap add/remove), FEEL it (esp. interactions
  — rest+legato gap-distribution, gate/subgate density, which isolated-lane reasoning can't call), peel back
  on cost + FUN. Provisional cut = warp-worthy <=> visualiser-worthy. Reasoning sets the default; Rodney's
  EARS set the verdict. Docs: OUTPUT_LAYER_DISTRIBUTION_MAPPING.md (warp purpose + scope).

### 6. The EFFECTIVE-DISTRIBUTION VISUALISER (where to show warped output)
Can't go under the global faders (per-voice warps -> up to 16 distributions). Not Sands (that's the
GENERATION layer, not the response-curve layer). -> OWN module, Lantern-style (display-only, snapshots host
state). Layout = Sands transposed: ROWS of VOICES (scrollable, ~8 visible) x bar groups (melody 12-24 +
octave ~8 + note-length). Mechanism: faders set WIDTHS; the Sands output (non-uniform draw) lands on them;
effective = width x draw-density. Useful EVEN WITHOUT tilt (relative faders -> absolute prob). KEYSTONE: a
switchable UNIFORM<->TRANSFORMED view makes every layer a visible difference -> the answer to "too deep to
be interesting" (depth-as-discovery). Docs: EFFECTIVE_DISTRIBUTION_VISUALISER.md.

### 7. Build progress (Phase 1-2 DONE this session)
Phase 1 (MERGED): Distribution.hpp (double) + MonotoneCurve.hpp (PCHIP, double) + contract tests. Phase 2
(built, delightful): square dual-axis CDF/PDF editor, all distributions render, live param morph, drag-edit,
"convert parametric -> editable PCHIP", devhost module. Iterated render-by-render (postage-stamp -> proper
large dual-axis). NEXT: Phase 2.5 (Mobius+Logit-sigmoid, -Arcsine, param knobs, convert) -> Phase 3
(integration: Macro/Sands scope, .dmcurve load/save, the LIVE-PREVIEW keystone).

## META (the epistemic through-line)
Both halves of the session taught the SAME lesson: don't trust a plausible narrative over the actual
thing. For CODE, the actual thing is the test/engine (polyBankIndex lied; contract tests caught it). For
DESIGN, the actual thing is Rodney's reasoning + ears (he caught my invented orthogonality, my float/double
wrong default, my "Beta for copula" tangent). For MUSIC, the actual thing is playing it (perceptual
questions -> feel, don't reason). VERIFY. Keep LAYERS distinct. Reasoning sets defaults; evidence/experience
sets verdicts.
