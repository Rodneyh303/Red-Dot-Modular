# SandsModifierResolver — one authority for every per-(voice,lane) Macro/East shaper

Design authority for ending the recurring Macro/East delegation bugs (warp, direction) by giving
EVERY per-(voice,lane) delegated modifier ONE shared resolution skeleton instead of N hand-rolled
copies. Grounded in the master table (OUTPUT_LAYER_DISTRIBUTION_MAPPING.md) + verified against code.

## THE ROOT PROBLEM THIS SOLVES (code-verified)
There are currently FOUR hand-rolled copies of the SAME Macro/East per-(voice,lane) delegation
algebra (base + ownCv + eastCv + macroSend + per-voice-tap + delegated-branch + end-clamp):
  - SPREAD    → SpreadResolver::effective()        — ONE tested authority. CORRECT. (the model)
  - LOR       → MonsoonExpanderManager combineLOR  — near-verbatim DUPLICATE (15 send/tap refs). CORRECT but duplicated.
  - WARP      → WarpF::warpedValue()               — INCOMPLETE hand-copy (0 send/tap/cv terms). → BUGS (Macro→East leak, V1 asym).
  - DIRECTION → laneDir/macroLaneDir_ path          — INCOMPLETE hand-copy (missing reset half, :142-143). → BUGS (VAR/LEG, stale).
The two COMPLETE copies work; the two INCOMPLETE copies have exactly this session's bugs. The cure
is not "complete each copy" — it's ONE authority all route through, so a copy CAN'T be incomplete.

## THE ONE-SENTENCE MEMBERSHIP RULE (inclusion AND exclusion)
A modifier belongs in the resolver IFF it VARIES PER (voice, lane) AND DELEGATES between Macro and
East. That one rule includes/excludes everything:
  IN : spread, LOR, direction, warp, θ (distribution params), clamp (lo/hi), + the future offset/mask lane.
  OUT: slew-rho, A/B mix — UNIFORM across all voices/lanes (one value for the group; NO per-voice
       divergence, NO delegation). Their correctness comes from SIMPLICITY, not a resolver; forcing
       them through it would MANUFACTURE the 16×7 divergence surface the resolver exists to remove.
  OUT: Change Alley — a different LAYER (latent-assignment routing), its own module. Not a shaper.
  OUT (tangential): the Macro/East DELEGATION CONTROL ITSELF — that is SandsTopology's job (who owns
       (v,lane), the ownership/cede/CV-mod of ownership). The resolver CONSUMES the resolved
       `delegated` boolean; it NEVER re-derives it (SpreadResolver header: caller wires topology's
       booleans in). ONE topology authority feeds MANY value resolvers. Topology's own bugs
       (owner resolved wrong — "Macro controls East then vice versa") are TOPOLOGY bugs, fixed
       upstream, NOT resolver bugs (right owner, wrong value).

## THE SHAPE: a skeleton + per-modifier policies
RIGID (shared, where ALL the bugs live — written + tested ONCE):
  - Inputs skeleton: base, delegated, macroPresent, ownCv, eastCv (CvTerm: connected/voltage/atten),
    macroBase, macroSend, macroPreDelta, macroPostDelta, macroTap.
  - tappedDelta (per-voice pre/post lerp) — the P9b publish-both-endpoints contract.
  - effective() control flow: `if delegated → applyDelegated(macroBase, tappedDelta); else → base
    (+ownCv) (+eastCv) (+macroSend·tappedDelta); then constrain`. Skip-if-absent, end-clamp-once.
FLEXIBLE (per-modifier Policy — the ONLY things that vary, all value-math not structure):
  1. Value type        — float(spread) | int-triple(LOR) | enum(direction) | (WarpF,w)(warp) |
                          scalar/vec(θ) | (lo,hi)(clamp) | (offset,mask)(future lane).
  2. combine op        — additive sum (spread/LOR/warp-depth) | PURE SELECTION, no blend (direction:
                          owner picks, no send-blend of an enum) | parameter-set (θ). effective()
                          MUST support no-blend policies (addCv/addSend = no-op, only applyDelegated).
  3. constrain         — single: clamp[-1,1](spread) | clamp w[0,1](warp) | valid-param-region(θ) |
                          none. PLUS joint: lo<hi(clamp) — a jointConstrain hook (no-op for others).
  4. extra Inputs      — a policy may DECLARE extra inputs: clamp's joint bound; the FUTURE lane's
                          GLOBAL PLAYHEAD reference (it resolves offset/mask RELATIVE to playhead +
                          its voice equivalent — the one member whose value is reference-relative,
                          not self-contained). Skeleton unchanged; policy's Inputs gains the field.

## THE TWO SUB-FAMILIES (same skeleton, different pipeline consumption)
- PROBABILITY-SHAPING: spread(latent coupling), warp + θ(marginal), clamp(output atoms). Take the
  post-slew/post-AB probability and shape its VALUE.
- STRUCTURAL/POSITIONAL: LOR(len/off/rot), direction(fwd/rev/…), + the FUTURE offset/mask-vs-playhead
  lane. Shape WHERE/WHICH-STEPS a voice reads, not the probability value.
Both are per-(voice,lane) + delegated → both are resolver members. The sub-family only affects how
the ENGINE consumes the resolved value, not how the resolver produces it.

## THE FUTURE OFFSET/MASK LANE (Rodney — the final planned Sands/East lane)
NOT a probability lane (draws no uniform). Applies OFFSETS + MASKS relative to the GLOBAL PLAYHEAD
and the voice equivalent. Classification: STRUCTURAL sub-family (like LOR — positional). Fits the
resolver as a policy: Value=(offset,mask); combine = add-offset / select-or-override-mask; Inputs +=
global-playhead reference (its specific wrinkle — the only reference-relative member). PAYOFF made
concrete: because the resolver exists, adding this lane = "write a policy", not a 5th hand-rolled
delegation copy waiting to become a 5th bug. This is the argument for doing the resolver BEFORE more
lanes land: every future addition is then a cheap safe policy, not an expensive buggy hand-roll.

## BUILD ORDER (each a commit, uniformity-harness gate, Rack check between)
0. (prereq) Build the per-(voice,lane) UNIFORMITY HARNESS (SEAM_ERADICATION_PLAN Stage 0): drives the
   REAL write path (UI/manager, not a direct-index bypass — the gap that made 05 green while Rack
   broke), asserts each (voice,lane) gets ITS OWN value, no leak, V1+V2 and VAR/LEG explicit.
1. EXTRACT SandsModifierResolver from SpreadResolver as `<SpreadPolicy>`; reparent spread; prove
   BYTE-IDENTICAL (spread works → it's the proof the skeleton is correct). No behaviour change.
2. WARP as `<WarpPolicy>` through the resolver; delete WarpF::warpedValue's hand-rolled deleg branch.
   Fixes the warp Macro→East/V1 bugs by construction. Harness green for warp, all cells.
3. LOR as `<LorPolicy>`; DELETE combineLOR's duplicate algebra (route through the resolver). Removes a
   verified ~40-line duplicate; LOR keeps working (harness byte-identical where spread=0).
4. DIRECTION as `<DirPolicy>` (pure-selection, no blend); route the laneDir path through the resolver.
   Fixes the VAR/LEG direction bug + the stale-reset bug by construction.
5. (future) θ, clamp, offset/mask lane as policies when those features are built. Interface already
   admits them (constrain/jointConstrain/extra-Inputs hooks present from step 1).

## NAMING
`SandsModifierResolver` (or `DelegatedLaneVoiceResolver`). The header carries the one-sentence rule so
no future person folds a uniform-across-group modifier (slew/AB) or a topology concern into it.

## PROCESS (from the war's lessons — non-negotiable)
Verify against code not narrative (this session: 4 AI misdiagnoses from asserting before reading).
Harness drives the REAL path. Fix against the whole map, never symptom-by-symptom. Spread is the
reference: any step that isn't byte-identical to spread where it should be is wrong.
