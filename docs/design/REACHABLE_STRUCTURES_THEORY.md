# REACHABLE DEPENDENCE STRUCTURES — the coherent theory (Rodney)

Capstone synthesizing the output-layer copula corpus. Detailed derivations live in
OUTPUT_LAYER_DISTRIBUTION_MAPPING.md; this is the ORGANIZING map. The structure a user can reach
decomposes into TWO ORTHOGONAL AXES — keep them separate (that's what keeps the theory coherent):

  AXIS I  — WITHIN-DRAW (instantaneous) dependence: how voices/lanes relate AT EACH STEP.  (harmony/voicing)
  AXIS II — TEMPORAL (moving-average / slew) dependence: how each stream flows ACROSS STEPS. (phrasing/flow)

Any Axis-I structure composes with any Axis-II structure. They are different maths, different musical
concerns, independently set.

════════════════════════════════════════════════════════════════════════════════
## AXIS I — WITHIN-DRAW DEPENDENCE (the instantaneous copula over the voice×lane grid)
Built in layers; each layer is a control the user turns.

### Layer 0 — THE PARTITION (delegation / Sands unlock) → authority BLOCKS
Delegation partitions the voice×lane GRID into AUTHORITY cells (Macro-owned vs East-owned). Per-lane
delegation = clean lane-aligned blocks; per-(voice,lane) unlock = RAGGED cells. Each block has its own
AUTHORITY (its modulation, distribution, w). The copula is BLOCK-STRUCTURED: within-block structure per
block + between-block relationship. (Global-Macro = the degenerate ONE-block case = the whole grid.)

### Layer 1 — PER-BLOCK FACTOR (the shared modulation) → the Archimedean generator
Within a block, at SPREAD=0, the shared modulation is the ONLY coupling → conditional independence given
it → a GENUINE ARCHIMEDEAN copula (requires EXCHANGEABLE voices — EARNED by the voice-0-eradication/seam
work: V1 a true peer). The GENERATOR = inverse-MGF of the modulation signal's AMPLITUDE DISTRIBUTION:
  Gamma→CLAYTON (lower tail) · positive-stable→GUMBEL (upper tail) · exponential→AMH ·
  constant→INDEPENDENCE · bimodal/max-variance→COMONOTONE.
FACTOR SCOPE: modulation shared WITHIN a lane only → per-lane Archimedean, lanes independent; shared
ACROSS lanes (one signal → all lanes' w) → a SINGLE factor spanning the whole block's grid. (Modulation
RATE is orthogonal — it sweeps the generator's PARAMETER over time, doesn't change the family.)

### Layer 2 — PER-BLOCK ATOMS (ℓ, m — injected atom) → MO / DMO / comonotone
Add atoms to the block's distribution:
  - CORNER atom (ℓ=0/1) + the factor → Archimedean continuous + EXCHANGEABLE MARSHALL-OLKIN singular
    diagonal = the PURE common-shock DMO structure (THE PAPER: ACHS). λ>0, calibratable via
    atom-mass-vs-modulation. (The one config reproducing the DMO copula EXACTLY — spread=0 gives the
    conditional independence DMO needs.)
  - INTERIOR atom (ℓ∈(0,1)) → comonotone COINCIDENCE on the interior diagonal, λ=0 (NOT MO).
  - ℓ SLIDES the singular mass along the diagonal: corner=MO/DMO(λ>0), interior=comonotone(λ=0).

### Layer 3 — PER-BLOCK GAUSSIAN (spread) → departs Archimedean
spread=0 → pure factor (Archimedean, Layers 1-2). spread>0 → ADD Gaussian-MA latent coupling → NOT
Archimedean any more (frailty + a correlated Gaussian part). Still: λ>0 only via atoms; spread>0 gives a
SECOND route to atom co-occurrence (spread-correlated voices rectified through the clamp/atom cross the
rail together). Per-voice spread (SIGN too) → heterogeneous/leader-follower → also leaves exchangeability.

### Layer 4 — CA (correlation routing) — cuts ACROSS the authority partition
CA routes correlation WITHIN blocks (leader-follower / routing tree among a block's voices — the signed
pairwise CA tree, rho^2 sibling propagation) AND ACROSS blocks (CA can couple voices in DIFFERENT
authority blocks). So the AUTHORITY partition (delegation) and the CORRELATION partition (CA/spread) are
DIFFERENT partitions that CAN CUT ACROSS EACH OTHER — a Macro-block voice and an East-block voice can be
CA-correlated even though they're authored separately. CA at spread=0 is INERT (nothing to route);
CA shapes the correlation that spread CARRIES.

### Layer 5 — BETWEEN-BLOCK relationship
Set by (a) the inter-authority FACTOR relationship (East's modulation vs Macro's: independent → blocks
independent; shared/correlated → cross-block factor coupling) AND (b) whatever CA routes across blocks.
With CORNER atoms + a factor SHARED ACROSS blocks → GRID-WIDE common-shock DMO (the whole ensemble
co-hits the extreme). With independent block factors → blocks independent (product copula).

### AXIS-I SUMMARY (the reachable within-draw copula is fully described by):
  { partition (blocks) } × { per-block: generator(modulation dist) + atoms(ℓ,m) + spread + per-voice } ×
  { CA routing (within + across, may cross the partition) } × { between-block factor relationship }.
Global-Macro = one block, spread=0, shared factor, no atoms, no CA = the ARCHIMEDEAN HOME. Everything
else is a departure: delegation→blocks; spread→Gaussian; per-voice/CA→asymmetric; atoms→MO/DMO.

════════════════════════════════════════════════════════════════════════════════
## AXIS II — TEMPORAL (MOVING-AVERAGE / SLEW) DEPENDENCE — a SEPARATE concern
The MA (slew-rho) is CROSS-STEP, not cross-voice: it sets how a stream's draw at step t relates to t-1
via the finite MA window (geometric weights, L2-normalised). This is ORTHOGONAL to all of Axis I — any
within-draw copula composes with any temporal structure.
  - slew-rho per (voice,lane): the autocorrelation / smoothness of that stream over steps.
  - SHARED slew-rho modulation across voices → a COMMON TEMPORAL-SMOOTHNESS factor (stochastic-vol-of-
    smoothness) — voices' temporal flow co-varies, distinct from their instantaneous coupling.
  - Cadence: slew-rho is PHRASE-gated (per-step change breaks in-flight window uniformity → Sklar).
  - λ temporal = 0 (steps jointly Gaussian → temporally tail-independent); only atoms would add temporal
    atoms, not in scope.
MUSICALLY: Axis II is PHRASING/FLOW (slow rho = smooth evolving lines; fast = jittery/independent steps);
Axis I is the HARMONIC/VOICING relationship at each moment. Set independently.

════════════════════════════════════════════════════════════════════════════════
## POINTS OF MUSICAL INTEREST (where a configuration SOUNDS like something)
- GLOBAL-MACRO, spread=0, no atoms = the ARCHIMEDEAN HOME: a coherent ensemble moving together under one
  gentle shared drift; the symmetric baseline.
- GLOBAL-MACRO + Gamma-ish modulation = Clayton-flavoured (lower-tail lean); + bimodal mod = voices
  swing toward comonotone (strong unison tendency); mod RATE = how fast the ensemble's coherence breathes.
- GLOBAL-MACRO + CORNER atom, factor shared across lanes = GRID-WIDE DMO: rare TUTTI SLAM — the whole
  ensemble hits the extreme together (calibratable rate). The paper's common shock, as a gesture.
- INTERIOR atom (same config) = a recurring ANCHOR/PEDAL the ensemble converges on (λ=0, no slam).
- HETEROGENEOUS ℓ per voice + rho-modulation = a CHORD-ANCHOR that LOCKS on demand (different-pitch
  anchors coincide when you drive the correlation) — a performable structured chord.
- DELEGATE some lanes to EAST = independent (or CA-coupled) SUB-ENSEMBLES: Macro lanes one coherent
  group, East lanes another (different generator), between = independent unless CA/shared-factor bridges.
- CA ACROSS blocks = cross-GROUP coupling that defies the authority split — e.g. an East voice shadowing
  a Macro voice.
- spread>0 (per-voice, signed) = leader-follower / anti-phase voicings (the signed CA tree: + same-rail,
  - opposite-rail co-extremes).
- AXIS II slow slew = flowing legato evolution; fast = pointillist; shared slew-mod = the ensemble's
  flow breathing together independent of its harmonic coupling.
- THE "FROZEN DICE" mode (shaping-dominates): freeze the draw, play the DISTRIBUTION knobs → sculpt a
  fixed relational skeleton's realization live (latent/marginal separation made performable).

════════════════════════════════════════════════════════════════════════════════
## THE COHERENT ONE-PARAGRAPH THEORY
The reachable dependence = (AXIS I) an instantaneous copula over the voice×lane grid, determined by the
AUTHORITY PARTITION (delegation → blocks), each block's FACTOR (shared modulation; its amplitude
distribution = the Archimedean generator), ATOMS (ℓ,m; corner=MO/DMO λ>0, interior=comonotone λ=0),
GAUSSIAN coupling (spread; 0=pure Archimedean, >0=frailty+Gaussian), and CA correlation ROUTING (which
can cut ACROSS the authority partition), with BETWEEN-BLOCK structure set by inter-factor relationship +
CA; COMPOSED WITH (AXIS II) an independent TEMPORAL MA structure (slew-rho; cross-step autocorrelation,
phrase-gated, λ_temporal=0). Global-Macro/spread=0/shared-factor/no-atoms is the exchangeable ARCHIMEDEAN
HOME (earned by the voice-0 eradication); every control is a symmetry-breaking departure; corner-atom +
grid-shared-factor + spread=0 reproduces the paper's DMO common-shock copula EXACTLY.

════════════════════════════════════════════════════════════════════════════════
# ENUMERATION OF REACHABLE STRUCTURES (the catalog — named, unnamed, and how reached)
Synthesizes OUTPUT_LAYER_DISTRIBUTION_MAPPING.md + the 2026-10 thread discussion. Paper-grade.

## A. THE GENERATOR FAMILY — reachable by MODULATION AMPLITUDE DISTRIBUTION (global-Macro, spread=0)
At the Archimedean home (global-Macro, exchangeable, spread=0, no atoms), the shared modulation signal's
AMPLITUDE DISTRIBUTION is the frailty mixing law; the copula generator = its inverse MGF. So the
distribution of your modulation source LITERALLY SELECTS which named copula you get:
  | Modulation amplitude distribution | Copula          | Tail            | How reached                         |
  | Gamma                             | CLAYTON         | lower (λ_L>0)*  | shape the mod CV to Gamma           |
  | Positive-stable                   | GUMBEL          | upper (λ_U>0)*  | positive-stable mod                 |
  | Exponential / geometric-ish       | ALI-MIKHAIL-HAQ | weak/none       | exponential mod                     |
  | Constant (zero variance)          | INDEPENDENCE    | none            | DC / no modulation                  |
  | Bimodal / maximal variance        | → COMONOTONE    | →full           | mod swinging between extremes       |
  | (generic LFO/CV/noise)            | UNNAMED Archim. | none (λ=0)      | any other mod shape — see F         |
  * CAVEAT: these named tails (Clayton λ_L, Gumbel λ_U) are the CONTINUOUS-frailty tails. In THIS
    architecture continuous λ is bounded-factor → effectively 0 / perceptually inert unless a decision
    tests the exact extreme (clamp verdict). The MUSICALLY REAL tail comes from ATOMS (section C), which
    is Marshall-Olkin, NOT the Archimedean generator's tail. So "reaches Clayton" = reaches the Clayton
    GENERATOR SHAPE (the dependence curvature), not a usable Clayton tail event.
MODULATION RATE is ORTHOGONAL to all of the above: amplitude-distribution picks the FAMILY MEMBER; rate
SWEEPS that member's PARAMETER over time (slow = long stretches in one regime; fast = cycling the
parameter — the ensemble's coherence "breathes").

## B. THE PARTITION FAMILY — reachable by DELEGATION / UNLOCK (authority blocks)
  | Config                                  | Structure                                           |
  | Global-Macro (one block, whole grid)    | single Archimedean/DMO world (the HOME)             |
  | Delegate some lanes to East             | BLOCK-STRUCTURED: Macro block (its generator) +     |
  |                                         | East block (ITS generator) + between-block relation |
  | Per-(voice,lane) unlock (Sands)         | RAGGED cells — finest partition; per-cell authority |
  | Clean lane-delegation                   | lane-aligned blocks (the special case of ragged)    |
BETWEEN-BLOCK: independent IF block factors independent (product copula); coupled IF factors shared/
correlated OR CA routes across (section E). So delegation reaches: independent sub-ensembles, OR coupled
sub-ensembles (via shared factor / CA), OR — same factor both blocks — effectively one block again.

## C. THE SINGULAR FAMILY — reachable by ATOMS (ℓ, m) — MO / DMO / comonotone / discrete
  | Atom config                              | Joint singular structure            | λ    | Named?        |
  | CORNER atom (ℓ=0/1) + shared factor      | exchangeable MARSHALL-OLKIN diagonal | >0  | MO / DMO      |
  |   └ + spread=0 (cond. indep.)           | = PURE common-shock DMO (THE PAPER)  | >0  | DMO (ACHS)    |
  |   └ + factor shared ACROSS lanes        | GRID-WIDE common shock (tutti slam)  | >0  | DMO, grid     |
  | INTERIOR atom (ℓ∈(0,1)) same ℓ voices    | comonotone COINCIDENCE on diagonal   | 0   | (comonotone@ℓ)|
  | HETEROGENEOUS ℓ (voice i≠j) + ρ-mod      | OFF-DIAGONAL mass at (ℓ_i,ℓ_j),      | 0   | UNNAMED —     |
  |                                          | ρ-MODULATED → controllable chord-lock|     | "chord-anchor"|
  | ALL CDFs step functions (full discrete)  | Gauss-MA copula on the BIN GRID      | grid| discrete copula|
  |   └ 2-bin limit                         | PROBIT / Gaussian-copula default     | —   | Li/CreditMetrics|
  | Opposite-corner atoms (±spread sign)     | countermonotone corner (split tutti) | >0* | MO (lower-Fréchet)|
ℓ SLIDES the singular mass along the diagonal: corner→MO/DMO(λ>0), interior→comonotone(λ=0). The
reachable λ>0 is ALWAYS Marshall-Olkin-family (atoms), NEVER the continuous Archimedean tail.

## D. THE GAUSSIAN FAMILY — reachable by SPREAD (within a block)
  | spread                                   | Structure                                           |
  | spread = 0                               | PURE factor → Archimedean (home); cond. independence|
  | spread > 0 (homogeneous)                 | FRAILTY: factor + Gaussian-MA latent coupling       |
  |                                          | (leaves Archimedean); 2nd route to atom co-occur    |
  | spread > 0, PER-VOICE, SIGNED            | leader-follower / anti-phase (signed CA tree);      |
  |                                          | +=same-rail, −=opposite-rail co-extremes; follower- |
  |                                          | follower = ρ² ≥ 0 always                            |
  | spread → ρ=1                             | comonotone (Fréchet M) for any warp                 |
The Gaussian factor is CONTINUOUS (bounded → λ=0 alone). It gives λ>0 ONLY by rectification through an
atom (spread-correlated voices cross the rail together = a 2nd DMO route, composing with the shared-w
route).

## E. CORRELATION ROUTING — CA — cuts ACROSS the authority partition
CA routes correlation WITHIN blocks (leader-follower tree, pairwise signed edges, ρ² sibling propagation)
AND ACROSS blocks (an East-block voice CA-correlated to a Macro-block voice). So the CORRELATION partition
(CA/spread) is a DIFFERENT partition from the AUTHORITY partition (delegation) and CAN CUT ACROSS IT.
Reachable: cross-group coupling that defies the authority split (e.g. East voice shadows a Macro voice).
CA is INERT at spread=0 (routes correlation that spread CARRIES; none to route). Depth-1 single-anchor =
rank-1-per-block (SPREAD_TARGET_MODES); reachable correlation = diagonal / signed rank-1 star / block-
diagonal rank-1 / neighbour-banded. NOT reachable: arbitrary PSD matrices (rank-1-per-block limit).

## F. WHAT'S REACHABLE BUT NOT SPECIFICALLY NAMED (the un-named region — most of the space)
Most of the reachable space is NOT a textbook-named copula — and that's expected (named families are
special points). The un-named reachable structures, each genuinely distinct:
  1. GENERIC-MODULATION ARCHIMEDEAN — any mod shape that isn't Gamma/stable/exponential gives an
     Archimedean copula with a generator = inverse-MGF of THAT distribution. A continuum of un-named
     Archimedean members between the named ones.
  2. CHORD-ANCHOR (heterogeneous-ℓ + ρ-mod) — off-diagonal controllable co-occurrence; a modulatable
     structured chord-lock. No standard name (it's a factor-modulated off-diagonal singular copula).
  3. BLOCK-MIXED (delegation) — a copula that is Archimedean-A on one block, Archimedean-B on another,
     coupled-or-not between — a nested/hierarchical factor copula with per-block generators. (Closest
     named relative: multi-group/nested factor copulas from credit — but with DIFFERENT per-block
     generators it's un-named.)
  4. FRAILTY+GAUSSIAN (spread>0 + shared mod) — neither pure Archimedean nor pure Gaussian; a frailty
     mixture with Gaussian conditional coupling. (NOT Krupskii-Joe — cond. coupling remains.)
  5. REGIME-MIXTURE (A/B-mix modulated) — a mixture over draw-source regimes; a regime-switching copula,
     un-named in this exact form.
  6. TEMPORAL (Axis II) composed with ANY of the above — the within-draw copula × the MA autocorrelation
     is a space-time dependence object with no single name.
  7. ATOM+TILT COMPOSITES — reinforced rail concentration (tilt feeds the atom) at a chosen ℓ, mass model
     (insert/displace vs absorb) selecting coupled-vs-independent tilt/mass — a parametrized singular+
     continuous family.

## G. WITHIN-LANE vs CROSS-LANE — the SCOPE matrix (what's reachable where)
Lanes are DOMAIN-SEPARATED independent streams → cross-lane needs a SHARED FACTOR (the only bridge).
  | Mechanism                        | WITHIN-LANE (voices share stream) | CROSS-LANE (streams independent)        |
  | Gaussian spread/CA correlation   | YES (real latent ρ)               | NO (no cross-lane latent ρ exists)      |
  | Shared w-modulation (atom mass)  | couples (if atoms)                | couples (if atoms + mod shared ACROSS   |
  |                                  |                                   | lanes) — the ONLY cross-lane λ route    |
  | Shared θ / tilt (continuous)     | co-move λ=0                       | co-move λ=0 (if shared across lanes)    |
  | Shared ℓ-modulation              | moves shared anchor               | moves shared anchor across lanes        |
  | Corner atom + shared-across-lane |                                   | GRID-WIDE DMO (λ>0) — whole ensemble    |
  | A/B-mix (shared source)          | regime factor (lane-group)        | regime co-switch across lane-group      |
  | slew-rho (TEMPORAL)              | own autocorrelation               | shared-mod = common smoothness factor   |
KEY: cross-lane λ>0 is reachable ONLY via shared-w(-or-clamp)-modulation of atom-bearing lanes — NEVER by
static atom placement on independent lanes (product rate). Cross-lane continuous coupling (θ/tilt shared)
is λ=0. CA/spread give NOTHING cross-lane (no latent ρ there). The factor's SCOPE (within-lane vs across-
lane) is the single switch for per-lane-vs-grid-wide structure.

## H. THE BOOLEAN-LANE SPECIAL CASE (rest/accent/legato/qmix — threshold consumers)
On threshold (boolean) lanes the atom/warp is marginally INVISIBLE (threshold-relativity: an atom not AT
the threshold = a knob-move). So: STATIC warp/atom on booleans = redundant with the knob (λ-irrelevant);
coupling appears ONLY under SHARED MODULATION, and ONLY as the JOINT effect (cross-lane MO of the gate
decisions — "articulation snaps to a configuration together"). The CV-OUT consumer (continuous) DOES see
the atom on every lane. So boolean-lane structure is reachable but per-CONSUMER: inert internally
(static), live jointly (shared-mod) and on the CV out always.

## I. PERCEPTION BOUND (what's reachable vs what's AUDIBLE)
Musical perception is CLUSTER/THRESHOLD, never measure-zero. So: continuous-tail λ (Archimedean
generators, bounded-Gaussian) is reachable but perceptually INERT (no decision tests the exact extreme).
ATOM λ (MO/DMO) is audible only where a consumer sees the exact value (value lanes: anchor; CV out: exact
voltage; NOT boolean-internal). So the AUDIBLE reachable set is narrower than the mathematical one — the
named Archimedean TAILS (Clayton/Gumbel) are reachable as GENERATOR SHAPES but their tails are inaudible;
the audible tail dependence is atom/MO/DMO only. This is WHY continuous distribution-mod suffices and
rail-clamp was demoted: the perception bound, not the math, sets what's worth reaching.

════════════════════════════════════════════════════════════════════════════════
# PIPELINE-LAYER CONFIRMATION — CA=copula/latent, warp=marginal, containment=support (Rodney)

Confirms the copula↔marginal separation is IMPLEMENTED by the pipeline stages:
- CA ROUTING (correlation/containment source) operates at the LATENT/UNIFORM layer = the COPULA
  (dependence) layer. Correlation is uniform→uniform.
- WARP (tilt + injected atom + w-mix) is the per-voice MARGINAL, applied AFTER CA. So two correlated voices
  (shared latent via CA) can have DIFFERENT output distributions (different warps) — correlated RANKS,
  different VALUES. Textbook copula/marginal factoring, implemented by stage order.
CONSEQUENCE for the atom-coordination gestures (§C, cross-voice/cross-lane): atoms are MARGINAL (warp
layer, per-voice-post-CA), so the coordination gestures (shared atoms → unison; correlated rail-hits → MO)
require SHARED ATOMS, not just shared CA routing. CA shares the LATENT; atoms are the MARGINAL — sharing CA
correlates the draws, but the atoms must ALSO be shared for the atom co-occurrence. (Consistent with
AXIS-I: the singular-part coupling needs the atom present on both, driven by a shared modulation.)
NEW DEPENDENCE RELATION — CONTAINMENT (support nesting), NOT a copula: containment clamps a voice to the
SOURCE's REALIZED GATES (post-uniform, post-warp, post-reaction-surface) = a hard SUPPORT constraint at the
OUTPUT/gate layer, a different pipeline stage from the copula (latent) and the marginal (warp). So the full
dependence map is THREE layers: COPULA (latent, CA) + MARGINAL (warp, per-voice) + SUPPORT-NESTING
(containment, realized gates). Containment is the one that is NOT a copula and NOT a marginal — a
support-domain constraint. Composable with both. (See ARTICULATION_ENVELOPE_VS_CORRELATION.md.)

════════════════════════════════════════════════════════════════════════════════
# CROSS-LANE DEPENDENCE — the two routes, and the ATOM KILLER APP (Rodney)

Clarifies §G for the PRACTICAL modulation controls. Observing the lane output-probability sequences:

## General (BODY) dependence ← shared CONTINUOUS param modulation
Shared-across-lanes modulation of a CONTINUOUS distribution param (θ/tilt/mean) makes the lane marginals
CO-MOVE through time — an observer of the sequences sees GENERAL correlation (bodies rise/fall together).
But λ=0: NO tail dependence (the co-movement is in the bodies; rail/extreme co-occurrence stays at product
rate). So continuous shared mod = observable general dependence, ZERO tail dependence.

## TAIL dependence ← shared ATOM modulation (two sub-routes)
Cross-lane λ>0 is reachable ONLY via shared-across-lanes modulation of ATOM-bearing lanes (§G: "the ONLY
cross-lane λ route"; never static atoms = product rate). Two practical sub-routes, with DIFFERENT feel:

### Route A — shared ATOM-MASS (m) modulation, w pinned at 1 (atom ALWAYS live)
At w=1 the atom is a TRUE point mass (exact 0/1 rail event). Modulating m directly scales the rail-event
PROBABILITY. Shared m-mod across lanes → the co-rail rate co-moves → cross-lane tail dependence that
tracks m CONTINUOUSLY. No dwell condition (the atom is always categorical at w=1). FEEL: smooth control
of coordination STRENGTH — dial cross-lane tie/rest co-occurrence up/down.

### Route B — shared WARP (w) modulation — REQUIRES DWELL AT w≈1
warped(u) = (1-w)·u + w·invCDF(u). The atom is a true point mass ONLY at w=1; at w<1 the identity blend
SMEARS it into a near-rail continuous cluster (NOT a rail event — the earlier w-formula conclusion). So
modulating w GATES the atom live (w≈1) vs smeared (w low). Shared w-mod → cross-lane tail dependence ONLY
during the w≈1 DWELLS (when atoms are live on all lanes together and actual 0/1 rail events can co-occur).
NECESSARY CONDITION: the w-modulation must DWELL at/near w=1 — a w signal that only sweeps THROUGH 1 (no
dwell) produces few/no rail events = little observable λ. Between dwells (low w) = smeared clusters =
body-correlation, λ=0. FEEL: GESTURAL — coordination arrives in BURSTS tied to the warp dwells
(coordinated-articulation moments punctuating smeared stretches), not smooth.

## THE CONCLUSION — the atom KILLER APP is CROSS-LANE TAIL DEPENDENCE
- CONTINUOUS mod (θ/tilt) → cross-lane GENERAL (body) dependence, λ=0.
- ATOM mod (m at w=1, or w with dwell) → cross-lane TAIL dependence, λ>0.
Cross-lane tail dependence is reachable ONLY via atoms (shared m-mod or dwelling w-mod) — NOT via any
continuous mechanism, NOT via static atoms, NOT via CA/spread (no cross-lane latent ρ). That UNIQUENESS is
the atom's killer app: coordinated EXTREME/categorical events across lanes (voices rest-together / tie-
together at the shared-atom moments) — unreachable any other way. Route A = smooth coordination strength;
Route B = gestural coordination bursts. (The theory's §G "shared-w-modulation of atom-bearing lanes" is
these two routes made concrete; Route B's dwell-at-w≈1 is the sharpened necessary condition.)

════════════════════════════════════════════════════════════════════════════════
# THE KILLER APP, GENERALIZED — the atom coordinates DEFINITE/EXTREME moments (Rodney)

Cross-lane tail dependence applies to ALL lanes (continuous AND threshold), and needs NO special atom
meanings. The atom lives at the RAIL = the "definite/extreme" end of any lane. So shared-atom modulation
coordinates the rail across lanes, whatever the lane means:

## CONTINUOUS lanes (scale degree, octave, note length) — EXTREME VALUES co-occur
Shared atom mod → the EXTREME values (top/bottom octave, longest/shortest note, edge of scale-degree
range) co-occur across lanes MORE than their MIDRANGE values do. The "big moments" coordinate: when a
voice hits its octave extreme it's more likely to also hit its note-length extreme (shared-mod lanes), so
extremes arrive as COORDINATED GESTURES (a dramatic high-and-long moment) rather than scattered independent
extremes. Midrange (ordinary notes, normal lengths) stays independent.

## THRESHOLD lanes (legato, rest, accent, qmix) — DEFINITE T/F co-occur
The atom at the rail = the DEFINITE T or F (certain legato, certain rest). Shared atom mod → the DEFINITE
decisions co-occur across lanes (certain-legato on one coincides with certain-rest on another more than
chance); the UNCERTAIN (midrange 0.3-0.7) decisions stay independent. The CERTAIN decisions coordinate;
the probabilistic ones don't.

## THE UNIFYING STATEMENT
Both lane types are the SAME phenomenon — the atom is at the rail, the rail is the DEFINITE/EXTREME end,
so tail dependence = coordination of the definite/extreme, whatever the lane means. Threshold "definite
T/F" and continuous "extreme value" are both just THE RAIL; the atom coordinates the rail across lanes.
=> THE KILLER APP, stated musically: the atom makes the ensemble's DEFINITE/EXTREME moments COORDINATE
across lanes, while ORDINARY/UNCERTAIN moments stay INDEPENDENT. Climaxes cohere; business-as-usual
scatters. Music is largely about coordinating the significant moments and letting the ordinary breathe —
and this falls out of atom-at-the-rail + shared modulation, with NO special meanings required.
NO SPECIAL MEANINGS NEEDED: the bare atom (a point mass at the rail, no semantic override) already gives
this dependence structure. Special meanings (atom=tie, rest-beats-legato) only add specificity to WHAT the
coordinated definite/extreme DOES — they do not create the dependence, which comes from the rail position
alone.

════════════════════════════════════════════════════════════════════════════════
# CROSS-VOICE too — the symmetric completion; tail dependence is ATOM-ONLY in EVERY direction (Rodney)

Everything about cross-LANE tail dependence applies symmetrically cross-VOICE, with one mechanism
difference (voices SHARE the stream within a lane; lanes don't).

## Cross-VOICE tail dependence (same atom-at-rail structure, across the ensemble)
Shared atom modulation across voices → extreme values + definite decisions coordinate ACROSS THE ENSEMBLE:
- CONTINUOUS lanes: octave extremes co-occur across voices = TUTTI REGISTER CLIMAX (ensemble leaps high
  together); note-length extremes = TUTTI SUSTAIN; scale-degree extremes = ensemble at scale edge
  together. Midrange octaves/lengths stay independent (ordinary voicing scatter).
- THRESHOLD lanes: definite decisions co-occur across voices — tutti certain-rest (unison silence),
  tutti certain-legato (unison sustain), tutti certain-accent (ensemble hit). Uncertain decisions scatter.

## THE MECHANISM DIFFERENCE — body vs tail, by direction
Within a lane voices SHARE the stream → CA/spread (Gaussian ρ) give cross-voice BODY dependence (real ρ).
BUT that is λ=0 (CA/spread give NO tail dependence, cross-voice OR cross-lane). So:
  |              | BODY dependence              | TAIL dependence            |
  | Cross-VOICE  | CA/spread (real ρ)           | shared ATOM mod ONLY       |
  | Cross-LANE   | shared CONTINUOUS mod        | shared ATOM mod ONLY       |
TAIL dependence — coordination of the extremes/definite-values — is ALWAYS atom-only, in BOTH directions.
CA/spread coordinate bodies across voices; continuous mod coordinates bodies across lanes; the EXTREMES
coordinate only through atoms, every direction.

## THE FULL ENSEMBLE PICTURE (both directions compose)
- CROSS-VOICE atom coordination → tutti climaxes (all voices extreme/definite together).
- CROSS-LANE atom coordination → coherent per-voice gestures (a voice's high-AND-long-AND-certain moment).
- BOTH (shared mod across voices AND lanes) → the whole GRID coordinates its extremes = the §A grid-wide
  DMO / TUTTI SLAM (entire ensemble hits the extreme as one gesture).
KILLER APP, FULLY STATED: the atom coordinates the DEFINITE/EXTREME moments across both voices and lanes —
tutti climaxes (cross-voice), coherent per-voice gestures (cross-lane), grid-wide slams (both) — while all
ordinary/uncertain moments stay independent (CA/spread may correlate their bodies; the EXTREMES are
atom-coordinated). Tail dependence is atom-only in every direction: the unique defining capability.

════════════════════════════════════════════════════════════════════════════════
# THE NAME — Dynamic Marshall-Olkin over moving-average Gaussian copulas (Rodney)

## One-line characterization
A generative sequencer / quantiser / gate-modifier / random-source compositional engine, driven by a
DYNAMIC MARSHALL-OLKIN COPULA — derived from modulation and atomic distributions — laid over MOVING-AVERAGE
GAUSSIAN COPULAS with musically interesting correlation structures.

## Why "DYNAMIC Marshall-Olkin" (not static MO)
Classical MO is static (fixed shock rates, fixed exchangeable tail). Here the shocks (atoms) are MODULATED
through time by shared/moving modulation, over a Gaussian copula body that ITSELF moves (the moving-average
Gaussian correlation from CA/spread). So λ (tail dependence) is TIME-VARYING, driven by shared atom
modulation, sitting on a time-varying Gaussian body. Dynamic MO = MO common-shock with time-dependent
shock intensities over a dynamic Gaussian copula.

## The two layers, NESTED BY THRESHOLD (and why both names appear)
- BODY / near-extreme co-exceedance = the moving-average GAUSSIAN COPULA. For a bivariate Gaussian the
  joint-orthant probability follows the ARCSIN LAW: P(both > median) = 1/4 + arcsin(ρ)/(2π). Real,
  ρ-driven co-exceedance at FINITE thresholds — but λ=0 (Gaussian copula has ZERO asymptotic tail
  dependence; the arcsin orthant is a finite-threshold BODY effect, not a tail limit). This is the
  quantitative form of "CA/spread give body dependence, λ=0".
- TAIL / asymptotic co-occurrence = the MO ATOM common-shock. λ>0. The genuine tail dependence the
  Gaussian body canNOT provide.
NESTED BY THRESHOLD: at finite thresholds (ordinary high notes) the arcsin/Gaussian body dominates
co-exceedance; pushing to the TRUE extreme (the rail) the Gaussian contribution vanishes (λ=0) and ONLY
the MO atom survives (λ>0). Gaussian arcsin governs the near-extreme; MO governs the asymptotic tail.
They are different phenomena at different scales, not competing descriptions.

## The precise statement (for the paper)
A DYNAMIC MARSHALL-OLKIN tail structure (time-modulated common-shock atoms, λ>0) over a MOVING-AVERAGE
GAUSSIAN COPULA body (finite-threshold co-exceedance by the arcsin orthant law, λ=0). Dynamic-MO is the
defining/distinctive layer (the unique tail-coordination mechanism); the arcsin-Gaussian is the body it is
layered on. Honest about both mechanisms at their correct scales — and the distinctive term for the engine.
