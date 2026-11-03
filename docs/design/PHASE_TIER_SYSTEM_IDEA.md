# PHASE TIER SYSTEM — calibration + two phase inputs + distribution distortion (Rodney)

Three ideas that UNIFY into one model: phase becomes a TIERED, CALIBRATABLE, DISTRIBUTION-SHAPED addressing
system — the same tiered/addressable/distribution-driven philosophy as everything else, now on the PHASE
input. It also RESOLVES the hard 32-step phase question (D9) by refusing the 1-bar-vs-2-bar binary.

## THE PROBLEM IT SOLVES (D9 restated — Rodney's framing)
At 16 steps, phase mode is simple because phase IS the transport (0-10V = one bar = the whole pattern); the
"no jumps between BARS, only within" rule is well-defined (one bar). At 32 steps = 2 bars this forks:
- (A) phase = whole transport (2 bars, span 192): LOSES "no jumps between bars" (phase can cross bars).
- (B) phase = one bar (span 96), transport runs TWO phase cycles: KEEPS "no jumps between bars" but phase ≠
  transport — you must CLOCK THE CYCLES (count which bar), making it more like clock mode.
Neither is free. The doc's current D9 answer (hardcode span → 192 = option A) silently drops the no-jumps-
between-bars rule.

## IDEA 1 — PHASE CALIBRATION (span is a USER PARAMETER, not hardcoded)
A phase calibration input/knob: "how many BARS (or QUARTER NOTES) does 0-10V phase span?"
- span = 1 bar  → phase is bar-local (option B); no-jumps-between-bars preserved (the natural/default).
- span = 2 bars → phase spans the whole 32 (option A); jumps anywhere.
- span = 1 quarter → beat-local phase (finer).
Implementation: PPB = p16 * (calibration_in_sixteenths) — a MULTIPLIER, not a hardcoded 16-or-32. Dissolves
A-vs-B into a parameter. DEFAULT = 1 bar (preserves the 16-step musical meaning; 16-step behaviour
bit-identical). Supersedes D9's hardcoded 192.

## IDEA 2 — TRANSPORT PHASE + SUBPHASE, quotient/remainder (like subgates) — Rodney's tightened mechanism
TRANSPORT PHASE ALONE = the 192 model (option A): 0-10V spans the full transport (2 bars / 32). That's the
base — just the mapping.
ADD A SUBPHASE INPUT and transport phase becomes the SELECTOR: transport phase is QUANTISED to tell us
WHICH SUBPHASE REGION we are in; the SUBPHASE input positions WITHIN that region. A quotient/remainder split:
    position = quantise(transportPhase, regionSize) + subphase * regionSize   // region base + sub-position
- SUBPHASE CALIBRATION sets regionSize (= how finely transport phase is quantised): 1 bar → 2 regions
  (bar 1 / bar 2); 1 quarter → 8 regions (quarters across 2 bars); anything between (quarter-note → bar).
- The QUANTISATION IS THE NESTING (the precise mechanism): the quantised transport phase = the coarse
  address (which region); the subphase = the fine position within. Same outer-selects/inner-positions as
  ratchet/ghost within the main gate — but the coarse level is the QUANTISED transport phase.
- "NO JUMPS BETWEEN BARS" FALLS OUT STRUCTURALLY: with regionSize = 1 bar, the subphase only addresses
  within the selected region → it cannot cross the region boundary. Preserved by construction, not a rule.
- MID-STEP JUMPS: not a new problem. The 192 transport phase already lands mid-step and jumps (the existing
  within-bar snap :779 + drift-check :1148). The subphase split REUSES that same mid-step-landing machinery,
  applied per level (region landing + sub-region landing) — no new jump class, the existing one applied
  twice.

## IDEA 3 — PHASE DISTORTION through the DISTRIBUTION LAYER (pure reuse)
Phase (0-10V → 0-1) passes through a COPY of the distribution layer (warp / tilt / ATOMS), calibrated
0-10V = 0..1:
- REUSE not invention: the distribution layer already maps [0,1]→[0,1] with warp/tilt/atoms; phase is
  already a [0,1] position. "Distort phase" = run phase through the existing distribution transform. Zero
  new DSP.
- ATOMS ON PHASE = probabilistic phase JUMPS / STUTTER: an atom is a point mass → phase snaps to a definite
  position with some probability; mostly smooth, occasionally snaps. Warp = non-linear phase scan; tilt =
  biased early/late in the bar.
- COMPOSES downstream: distorted phase drives the playhead → warped/atomic SCANNING of the pattern → feeds
  all the per-voice / correlation / mask machinery. The dependence/distribution theory applied to the PHASE
  SCAN itself.

## THE UNIFYING FRAME — a PHASE TIER system paralleling the GATE TIER system
Gates: main / ratchet / ghost, per-level decisions (the subgate tier). Phase: MASTER / BAR, per-tier span
(calibration) + per-tier DISTRIBUTION distortion (warp/tilt/atoms). Same architectural shape, reused:
nested tiers, per-level controls, distribution-driven. The subgate CONCEPT generalises from gates to phase.
- Per-tier distortion: master phase through its own warp/tilt/atoms (transport-level scan) + bar phase
  through its own (bar-level scan) → two independently-distortable, atom-punctuated, nested phase scans.

## STATUS
IDEAS — not scheduled. Idea 1 (calibration) is the immediate better answer to D9 (default 1 bar preserves
16-step meaning; the hardcoded-192 D9 answer drops no-jumps-between-bars). Ideas 2+3 are the richer future
(a full phase tier system). Capture now; sequence after 32 steps lands. Distribution reuse (idea 3) makes it
cheaper than it looks — the machinery exists.


════════════════════════════════════════════════════════════════════════════════
## [ARCHITECTURE — Rodney] The distribution transform WANTS TO BE AN EXTRACTED PRIMITIVE
Phase distortion (idea 3) reveals a latent architecture: the distribution transform (warp / tilt / atoms)
is currently EMBEDDED in Sands/East, coupled to lane draw-generation. But phase distortion needs the SAME
transform on a DIFFERENT input (a phase voltage) for a DIFFERENT purpose (scanning position, not draws).
That's the signature of a REUSABLE PRIMITIVE trapped inside a feature. It is a pure [0,1]→[0,1] transform,
mathematically self-contained, wanted in multiple contexts.
DIRECTION (a refactor-to-primitive, not just the phase feature):
- EXTRACT the distribution transform into a reusable CORE primitive.
- Consumers: (a) the embedded LANE draws (current), (b) PHASE distortion (built ON the primitive, not a
  second embedded copy), (c) a STANDALONE "distribution shaper" MODULE — any 0-10V CV in → warp/tilt/atoms
  → out. Atom-punctuated CV shaping as a utility: distinctive (nobody else has atoms in a CV shaper), very
  VCV-idiomatic (composable primitive), AND it demonstrates the instrument's core thesis in isolation (good
  for the paper/demo). Usable as an EXPANDER and as an INDEPENDENT module.
PAYOFF: DRY (one implementation, 3+ consumers, not copies) + a sellable standalone module + phase distortion
for free on top. SCOPE: a real refactor (extract without breaking Sands/East, golden-pinned). It affects HOW
phase distortion is built — build it ON the extracted primitive. Capture as the architectural direction;
sequence the extraction before/with the phase-distortion build.

════════════════════════════════════════════════════════════════════════════════
## [Rodney] CO-JUMPS — tail dependence in the POSITION/TIME domain (the killer-app, for phase)
If BOTH tiers use the atom-distortion (idea 3), you get CO-JUMPS: transport phase jumps to a different
REGION of the 2 bars WHILE subphase simultaneously jumps to a different STAGE within its bar/quarter — two
jumps, two scales, one moment. A single modulation event → a COMPOUND multi-scale relocation ("jump to the
other bar AND a different beat within it"; "a different quarter AND a different position within it").

### This is the ATOM KILLER-APP applied to PHASE (the reachable-structures theory, position domain)
Two atoms (transport + subphase), each a point mass. Their COORDINATION is governed by the dependence
machinery — the SAME Marshall-Olkin tail structure as cross-voice/cross-lane tail dependence, now over
WHERE THE PLAYHEAD LEAPS:
- INDEPENDENT atoms → the two tiers jump at different times (scattered multi-scale relocation).
- CO-FIRING (tail-coupled) atoms → they jump TOGETHER — the dramatic compound leap (everything relocates at
  once). Reachable ONLY through shared/simultaneous atom modulation (exactly as cross-voice tail dependence
  was) — not via continuous phase mod, not static atoms.
- ANTI-CORRELATED → transport jumps while subphase HOLDS, and vice-versa — a conservation (one scale moves,
  the other anchors).
So phase becomes a STRUCTURED-STOCHASTIC, MULTI-SCALE, TAIL-DEPENDENT scanner. The four-quadrant richness
(independent / co-jump / anti-jump / held) of the atom killer-app, in the TEMPORAL-SCANNING domain.

### Why it matters
A continuous space from smooth playback → warped scan → single-scale jumps → coordinated multi-scale leaps
→ conservation patterns ("structured teleportation across the pattern"), all from one voltage pair through
the distribution primitive, playable in real time. Most sequencers scan linearly or with simple random
jumps; two-tier atom-punctuated CO-JUMPING phase scanning is genuinely novel — the instrument's core thesis
(structured stochastic dependence, MO tails) extended into the TIME/POSITION dimension. Paper-worthy in its
own right (tail-dependent temporal addressing), and pure reuse (distribution primitive + atoms + the
existing dependence/coordination structure).
