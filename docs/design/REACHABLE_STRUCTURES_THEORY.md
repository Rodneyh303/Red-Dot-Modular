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
