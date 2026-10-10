# Canonical claim — the precise technical description (build positioning from this)

The maximal TRUE claim (Rodney). Every term is load-bearing, factual, defensible — not marketing
inflation. Nothing else in the Rack library (arguably in software instruments broadly) is this.

## The full technical claim
A **time-reversible random generator** based on **7 lanes x 16 channels of moving-average Gaussian
copulas**, with **slew and mix controls**, the ability to **construct musically interesting
correlation structures**, and to **modulate both the correlation STRUCTURE and the correlation
AMOUNTS** — plus **LOR (length/offset/rotation) and direction** per lane in Sands.

Unpacked:
- **Time-reversible** — scrub, reverse, bit-exact (Philox counter-addressed spine; AD-style exact
  reverse).
- **7 x 16 moving-average Gaussian copulas** — structured joint-distribution model per lane per voice,
  not scalar randomness.
- **Slew + mix controls** — temporal (slew) and interpolation (mix) shaping of the correlation.
- **Constructible correlation structures** — build meaningful voice relationships (via Change Alley),
  not one global randomness amount.
- **Modulate structure AND amount** — the correlation TOPOLOGY (who relates to whom) and the
  correlation STRENGTH (how much) are each independently modulatable.
- **Global pattern length + per-lane LOR within it + direction (Sands)** — a TWO-LEVEL time structure:
  the global length sets the loop/phrase boundary; per-lane LOR (length/offset/rotation) + direction
  operate INSIDE that frame, so lanes are POLYMETRIC/phased against each other and the global cycle
  (e.g. a length-7 melody lane against a length-5 rhythm lane inside a 16 global), drifting and
  realigning over the global loop — all reversible.

## The two-function claim: random number SOURCE + random number ARRANGER
Plain-language core (grasped instantly; depth underneath): **a random number SOURCE and a random
number ARRANGER.**
- SOURCE = the copula probability field (Monsoon/Sands generates it).
- ARRANGER = Intertropical — ONCE it routes the PROBABILITIES (not just notes/CV), it arranges the
  RANDOM SOURCE ITSELF (see ARRANGED_PROBABILITY_OUTS_IDEA — the feature that earns this claim).
Distinctive because almost nothing else arranges the underlying RANDOM NUMBERS — anyone arranges notes;
routing a generated probability field through a correlation-aware arranger is near-unique. Lead PLAIN
with this ('random generator + arranger'), reveal the depth (reversible copula field, correlation
structure) underneath — modest surface, over-delivers on inspection.

## Third functional identity: generative CLOCK/GATE MODULATOR
Gate mode takes an incoming clock/gate stream and transforms its rhythm — rests = probabilistic
SKIPS, ratchets/subgates = MULTIPLY/subdivide, ghosts = INSERT events in gaps, legato/tie = MERGE
consecutive gates — all probability-driven, correlated, reversible. Crucially the transformed streams
are USABLE OUTPUTS. **STATUS CHECK (verified in code — MonsoonOutputGenerator.cpp):**
- **GATE OUT emits** (gs.process -> the jack). Modulated MAIN gate (rests=skips, legato=merge). Real.
- **STEP GATE OUT + STEP LEGATO OUT DO emit** — gsStep.process() -> setGateWithMute_(outputs[
  STEP_GATE_OUTPUT]/[STEP_LEGATO_GATE_OUTPUT]) (lines 68/184-185; poly via STEP_GATE_OUT_0,
  139/144-145). Confirmed emitting (incl. clock mode). The subdivision/step stream IS on the output.
  NOTE the Monsoon.hpp:437/446 "ENGINE EMISSION PENDING" comment is MISLEADING/stale re: the jack —
  the jack works; what's pending is a NARROWER gate-mode refinement (tracking the gate BEFORE legato
  drops edges so the un-fused STEP gate is fully truthful in gate-mode legato cases). So STEP GATE
  OUT works today; only that gate-mode-legato edge-truthfulness refinement remains.

So the TWO-LEVEL claim (main GATE OUT + subdivision STEP GATE OUT) is essentially REAL today — with a
gate-mode-legato refinement to STEP's edge-truthfulness still to finish. Claim the two-level version,
noting the refinement if being strict.

Claim it QUALIFIED: **"generative / probabilistic clock-and-gate modulator"** — the qualifier is the
differentiator (plain dividers are commodity; a GENERATIVE, correlated, reversible one with
main+subdivision outs is distinctive). Don't claim bare "clock modulator" (implies divide/multiply/
swing).

## Fourth functional identity: optionally-randomising PHASE-TO-GATE CONVERTER
Phase mode (verified: modeSelect==2, CV1 = phase input -> phase.pulseEdge/sixteenthEdge -> gates) takes
a PHASE RAMP in (0->1 position, from a master clock's phase out / LFO ramp / phase-distributed clock)
and produces GATES out. "OPTIONALLY RANDOMISING" is the continuum: at zero probability/variation it's a
CLEAN DETERMINISTIC phase-to-gate converter on a fixed grid; dial the generative controls and it becomes
generative.
Example (Rodney, the demo): set NOTE VALUE only, nothing else random -> deterministic phase-to-gate on
the note-value grid. Then add variation / legato / rest / etc. -> the SAME conversion blooms into up to
16 per-voice correlated random VARIATIONS. Same input, same conversion, continuously order->chaos — the
thesis demonstrated on one function.

## The clock/gate/phase modes ARE identities (by input type)
The 6->3 collapse isn't arbitrary — each surviving mode is a functional identity keyed to its input:
- **CLOCK** = generate from tempo -> the pure GENERATOR.
- **GATE** = reshape incoming gates -> the CLOCK/GATE MODULATOR.
- **PHASE** = convert incoming phase to gates -> the (optionally-randomising) PHASE-TO-GATE CONVERTER.
"generate / modulate / convert", by input type.

## The functional identities (one engine, many uses) — SIX, all verified
- **random number SOURCE** — the copula field.
- **random number ARRANGER** — Intertropical routing the probabilities.
- **generative CLOCK/GATE MODULATOR** — transforming an input clock -> GATE OUT (main) + STEP GATE OUT
  (subdivision) + STEP LEGATO OUT; all emit (verified). A gate-mode-legato STEP edge-truthfulness
  refinement remains, but the two-level outputs work today.
- **optionally-randomising PHASE-TO-GATE CONVERTER** — PHASE mode: phase ramp -> gates, clean
  deterministic at zero, generative when dialled (up to 16 correlated variations). (verified)
- **deterministic AND stochastic SEQUENTIAL SWITCH** — CV into Change Alley's poly-CV inputs (8 pairs)
  -> CA permutation remap (fixed OR dice-reshuffled) -> Intertropical -> outputs. Routing spans
  fixed->generative, correlated. The random switching is REVERSIBLE TWO WAYS (dice re-draw backward +
  true-reverse trajectory). SOME deterministic transforms are truly invertible (non-fan-in: rotate/
  reflect); fan-in (scatter/collapse) are lossy, reversible only via state-replay. (verified — see
  SEQUENTIAL_SWITCH_IDEA.md)
- **ARPEGGIATION as a PATCH TECHNIQUE** (constructed, not a mode — DOWNGRADED from 'arpeggiator') —
  achievable but it's a RECIPE, not a built-in arp. Quantise a poly CHORD, SPLIT its voices, step
  through them sequentially via Intertropical, set a SHORT master pattern length (1/2/4 notes), and
  MODULATE the master offset into the 16 steps so each short pattern starts at a different offset
  (the moving/cycling arp quality). A CONSTRUCTED generative arpeggiator. Claim "arpeggiation is
  achievable as a patch technique", NOT "it's an arpeggiator" / "arp mode" (there is none). NEEDS the
  proving patch to confirm it reads cleanly as an arp vs clunky — if clean, fair claim; if fiddly,
  weaker ("arp-like textures with effort"). Watch while building the patch: is the recipe a few
  sensible settings (document it) or a precarious combination (weak claim or a small UX win)?

NOT bolted-together features — SIX USES of the SAME generative/probabilistic/correlated/reversible
core, each defined by how you patch it, each verified against the code. Many entry points for many
users (generative-seq / ensemble / rhythm-clock / phase / switching / chord-arp person), each a
familiar hook that turns out to be the others.

## Three orthogonal axes (the full space)
All TIME-REVERSIBLE:
1. **Time structure** — global pattern length + per-lane LOR + direction (polymetric, phased).
2. **Probability** — the 7x16 copula field (order<->chaos in WHAT happens).
3. **Correlation** — constructible + modulatable structure AND amount (order<->chaos in HOW voices
   relate).
Time-structure is the compositional-time axis; probability is content; correlation is ensemble. Three
independent dimensions of a generative space, each navigable and reversible.

## Two registers — SAME machinery, different audiences
- **Technical audience** (developers, serious synthesists, the knowledgeable community corner): use the
  full claim verbatim. "Time-reversible 7x16 moving-average Gaussian copula with modulatable
  correlation structure and amount" is a credential AND a differentiator — it signals this is built on
  foundations the field doesn't use. LEAD with the rigour here. Manual architecture section, docs,
  dev-facing material.
- **Musician audience** (headline, demo, first thing a browsing musician sees): "copula" is a wall.
  Translate to musical words: "voices that relate to each other in ways you control and can change live
  — from locked-together to independent to interlocking — and you can rewind it." Same instrument,
  musical language. Do NOT water down the technical claim; just don't LEAD a general audience with it.

## Why this matters
dot.modular can make the strong technical claim HONESTLY — most "generative" tools would be
overclaiming to describe themselves this way; dot.modular would be UNDERclaiming not to. State the
precise version prominently SOMEWHERE (for the people who recognise what it means — the early adopters
and advocates who carry it), translated per audience elsewhere. Precision is the credential; the same
rigour that built the instrument should describe it.

---

## Positioning vs Vermona Melodicer — "mono is a correlation setting, not a limit" (Rodney)
Melodicer has GLOBAL variation and legato (one value, all voices). Monsoon has them PER-VOICE. But the
compelling claim is NOT "we added more knobs" — it is CATEGORICAL:
- **Monsoon is a Gaussian-copula CORRELATION engine; Melodicer's global behaviour is simply Monsoon at
  MAXIMUM variation/legato correlation.** At correlation +1 all voices' variation/legato draws are
  identical => effectively global/mono = the Melodicer coordinate. At 0 => independent per-voice; at −1
  => complementary. So "variation/legato mono-only" is ONE COORDINATE in a continuous space Monsoon
  spans — a correlation SETTING, not an architectural LIMIT.
- So Monsoon doesn't have "more variation than Melodicer" — it CONTAINS Melodicer as the +1-correlation
  corner of its copula space, and opens the whole axis around it.
- Same principle as the instrument's whole thesis: every GLOBAL-vs-PER-VOICE choice is a correlation
  setting. Variation/legato are just lanes in the same engine; "mono" is max-correlation on those lanes,
  exactly as "unison" is max-correlation on the value lanes. ONE engine; mono is its degenerate case.

Crisp line: "Melodicer forces you to the mono corner. Monsoon gives you the whole correlation axis —
mono is just what you get at maximum correlation."

### Why V1 is still an EXPLICIT spread target (honouring the Melodicer coordinate) (Rodney)
CA (Change Alley) is general — any voice can be a spread source/target, V1 included. So V1-as-an-explicit
spread-target could look redundant (just pick V1 in CA). It is NOT redundant: the Melodicer coordinate is
"everything relates to the MAIN / reference voice (V1)", and that natural anchor deserves FIRST-CLASS,
directly-reachable representation rather than being buried as one CA configuration. So:
- CA = the GENERAL case (any voice ↔ any voice).
- V1-as-explicit-target = the honoured SPECIAL case (the reference/anchor = the Melodicer coordinate),
  made directly addressable because it is the natural/default/lineage point users reach for.
This mirrors the whole trio: V1 is NOT special in IMPLEMENTATION (it's just voice 0 — "mono is not
special anymore" in the code), but it IS the meaningful REFERENCE coordinate the design chooses to
surface directly in the interface. Not special internally; honoured as the anchor externally.

**V1 is targetable DIRECTLY, without CA.** Not even "a convenient CA preset" — V1-as-spread-target is a
standalone direct path, independent of CA. So the Melodicer coordinate (spread toward the reference voice)
is reachable with NO CA configuration — fully first-class.

### Melodicer's QUANTISER MODES = a QMIX setting at the simplest rhythm corner (Rodney)
The argument generalises from variation/legato to Melodicer's CENTRAL feature — the quantiser (it's in
the name). QMIX is Monsoon's quantiser pitch-source blend: per note, qmixUseGenerated = (r_qmix >=
qmixLevel) decides generated vs quantiser-mode pitch (SequencerEngine.cpp:465). So:
- **Melodicer's quantiser modes = a particular (fixed/deterministic) QMIX setting** — e.g. qmixLevel at the
  extreme = always-quantise. Monsoon opens the PROBABILISTIC continuum (qmixLevel anywhere => per-note
  probability of quantise-vs-generated), which Melodicer cannot do.
- **...at the SIMPLEST CORNER of the rhythm settings** — Melodicer's rhythm = the degenerate/minimal point
  of Monsoon's rhythm space (basic probability, no correlation, no per-voice variation).
So **Melodicer in its ENTIRETY is a single CORNER of Monsoon's (rhythm × qmix × correlation) space** —
the simplest corner. Monsoon is the whole continuous space; Melodicer is one point in it. The copula +
qmix + rhythm lanes are what turn "Melodicer's features" into "coordinates in our space."

Crisp line (scaled up): "Melodicer isn't a smaller feature set — it's the simplest CORNER of Monsoon's
space. Its quantiser is one QMIX setting; its global variation is max correlation; its rhythm is the
minimal corner. We contain Melodicer as a coordinate and open the whole space around it."
