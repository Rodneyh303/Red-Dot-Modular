# RATE MANAGER + RESOLVER-FOLDS-IN-UI — the structural fix that ENDS the recurring bug classes

Rodney, after a week of patch-spawns-bug (voice-0-special, bars-vs-output divergence, East↔Macro
delegation leaks, per-sample CPU spikes — the delegation fix itself caused the 124% CPU + cross-module
mess). STOP single-bug-patching. These bugs are ALL ONE DISEASE: the same quantity computed in more
than one PLACE or at the wrong RATE, and the copies drift. The cure is two missing AUTHORITIES:
(1) the resolver must be the value authority the UI reads too (not engine-only), and (2) a RATE
MANAGER that makes wrong-rate / duplicate computation UN-WRITABLE. Same shape as the strong-types
seam fix: one authority, every consumer reads it, structure prevents a second copy.

## WHY THE BUG CLASSES RECUR (name the disease)
- Resolver landed ENGINE-ONLY: it unified the engine's internal consumers into one resolve, but the
  UI (bars) kept its OWN parallel computation (macroOwnProbability in the VISUAL class). So after the
  resolver there were STILL two computations (engine's + UI's) → bars-vs-output still diverge.
- The delegation fix then tried to make them agree by routing the OUTPUT through a visual-ish accessor
  ACROSS the layer boundary — and ran step-rate work (full spread+warp resolution) PER OUTPUT CHANNEL
  PER SAMPLE → the Macro 124%. Also: we reach into OTHER modules' process() (Macro, expanders), so the
  recompute happened in a DIFFERENT module's CPU.
- Rate control is PIECEMEAL: controlDivider, lightDivider, a "/32 throttle", step/phrase detection, the
  per-sample-vs-latched split — each an ad-hoc `if (divider.process())` scattered through process().
  Nobody can SEE what runs at what rate → heavy work lands on the wrong rate by accident.
DISEASE: same thing decided in multiple places / at the wrong rate → drift. Every bug this week is an
instance. (Same family as the seam/voiceSlot class — scattered convention, no enforced authority.)

## THE RATE ENUM — ORDERED and NESTED-SUBSET
enum class Rate { Phrase, Step, Control, Frame, Sample };  // SLOWEST → FASTEST
Two properties, both load-bearing:
- ORDERED by frequency (Sample fastest/"worst"/highest, Phrase slowest).
- NESTED SUBSET: each slower rate's TICKS are a strict SUBSET of the faster rate's ticks
  (Phrase ⊂ Step ⊂ Control ⊂ Frame ⊂ Sample). Every Phrase boundary IS a Step boundary IS a Frame IS
  a Sample. THIS IS WHAT MAKES "slower consumers sample the same output" CORRECT AND STALE-FREE: a slow
  consumer's read-points always coincide with a fast tick the producer already ran, so it never reads
  between updates. Without the subset property the max-rate-producer rule would be unsafe.

## THE CORE RULE: ONE calculation at the WORST (highest) rate any consumer needs
A quantity is computed ONCE, at the MAX over its consumers' required rates. Slower consumers read the
SAME produced output (subset property → always fresh), never a second computation.
BUT a producer may DECOMPOSE into sub-quantities at DIFFERENT rates — each sub-quantity at the SLOWEST
rate that still serves ITS fastest consumer. "Compute as slow as you can; only the irreducibly-live
sliver runs fast." (The inverse of the bug: do NOT run the whole heavy resolution at frame/sample just
because one thin part is live.)

## THE RESOLVER FOLDS IN UI — slow-heavy / fast-thin decomposition
The resolver is a registered PRODUCER, and it DECOMPOSES itself by rate:
- PARAMETERS (spread values, warp curve+w, delegation/ownership) → STEP / ON-EDIT rate. They don't
  change between steps. Resolved ONCE per step for ALL (voice,lane) cells (batched — see below).
- ENGINE value = the STEP sample of the resolved result (latched).
- UI BARS value = the SAME step-resolved result + a THIN FRAME-rate live overlay (the continuous
  knob/CV position the sight-leads-sound feature shows). Only the thin overlay is frame-rate; the heavy
  resolution is NOT.
=> bars and engine read the SAME step-resolved value → they CANNOT diverge on the expensive part
   (kills bars-vs-output). The bars additionally get the thin live overlay → sight-leads-sound,
   correctly costed. The CPU spike was running the WHOLE resolution per-frame/sample; the fix is the
   decomposition: heavy=step, thin-live=frame.
The macroOwnProbability duplication DELETED — there is ONE resolve, in the engine/resolver, that BOTH
the bars and the output read (UI is a registered consumer, not a re-implementer).

## THE RATE MANAGER CONTRACT (five properties — all enforced, not conventional)
1. ONE PRODUCER per quantity (no duplicate computation — the resolver, now including UI).
2. AT THE MAX-CONSUMER RATE (ordered enum; slower consumers sample the same output; subset = fresh).
   A producer may decompose into sub-quantities each at the slowest sufficient rate.
3. SHARED ACROSS MODULES: ONE rate manager injected into EVERY module's process() (Monsoon, Macro,
   expanders). Cross-module consumers of a quantity hit the SAME producer at ONE rate — the thing the
   Macro 124% violated (Macro re-did per-channel what should be produced once).
4. BATCHED OVER LANES/CHANNELS: a per-(voice,lane) quantity is ONE op for ALL cells at the quantity's
   rate — NOT per-cell per-call. (macroOwnLaneProbability × 5 lanes × channels per-sample is the
   anti-pattern: it made the lane/channel dimension a per-sample MULTIPLIER instead of one batched op.)
5. ENFORCED / UN-WRITABLE: the MANAGER owns dispatch. Work only DECLARES (quantity, rate). A consumer
   can read a quantity ONLY at its declared rate or slower (subset-guaranteed fresh); it CANNOT cause a
   compute at a faster rate than the producer's target. "Step work running per-sample" cannot be
   written — there is no raw `if (divider.process())` to get wrong; the manager dispatches by
   declaration. (Same leap as strong VoiceSlot types: wrong usage doesn't compile/dispatch.)

## BUILD: the MEASUREMENT is the first step (and it's the manager's input, not just the CPU fix)
We cannot build the rate manager from a guess — and guessing is what caused the week of whack-a-mole.
Profile process() by SECTION (cheap control-rate-logged timers — the OPPOSITE of the per-sample chrono
that caused the 124%): the output is the INVENTORY the manager is built from — every quantity × its
current rate × its consumers. "Find the 124%" and "design the rate manager" are the SAME first step:
the profile produces the registration table. (Measure once → fix once → build the manager from the
measured reality, never a 5th guess.)

## RELATION TO THE SEAM / STRONG-TYPES PLAN (same disease, three axes)
- SEAMS (SEAM_ERADICATION_PLAN): scattered INDEX conventions → strong VoiceSlot/LaneId types,
  wrong index un-writable.
- RESOLVER (now incl. UI): scattered VALUE computation → one resolve all consumers read, divergence
  un-writable.
- RATE MANAGER: scattered TIMING → one rate authority, wrong-rate/duplicate compute un-writable.
Three faces of "same thing decided in multiple places drifts." Building all three retires the entire
bug-class family that ate this week. The patches don't — each patch is a NEW place to drift (the
delegation patch spawned the CPU spike spawned the cross-layer mess). STOP patching; build the
authorities.

────────────────────────────────────────────────────────────────────────────────
## CORRECTION + CORE STRUCTURE: PER-MODE RATE CHAINS (Rodney) — supersedes the earlier framing

The earlier "ORDERED and NESTED-SUBSET single hierarchy" framing is WRONG/incomplete (Rodney):
rates are NOT one universal nested hierarchy derived from a single clock — that would couple them
(e.g. deriving Step from frame count → frame-rate changes break musical timing). The correct
structure is PER-MODE RATE CHAINS, computed in ONE place, with independent sources per tier.

### RATES ARE INDEPENDENT CLOCKS (freshness via published slots, NOT tick-subset)
Each rate has its OWN source: Sample←audio clock; Frame←video clock (user-settable in standalone —
MUST NOT drive any other rate); Control←a wall-clock division (NOT frames); PPQN/Step/Phrase←the
transport (mode-dependent — see chains). A producer writes its result to a stable SLOT at its rate;
a consumer reads the latest published slot value at ITS rate → always fresh, no tick-nesting needed.
=> Changing FRAME rate perturbs ONLY frame consumers. Changing SAMPLE rate scales ONLY sample-bound
work. No rate is defined as "every N of another rate's ticks" (that coupling is UN-WRITABLE).

### SAMPLE-RATE AWARENESS (the CPU invariant + a test)
Rates are EVENT/TIME-anchored, not sample-count-anchored. So at higher SR, non-Sample work fires the
SAME number of times per SECOND → its per-SAMPLE cost FALLS. Only the irreducible Sample sliver scales
with SR. INVARIANT (and regression test): raising SR must raise CPU ONLY by the Sample-tier work; if
non-Sample CPU-per-second rises with SR, something heavy is wrongly sample-bound.

### THE PER-MODE RATE CHAIN — the single authority (Rodney: "one place to calculate them")
Each MODE declares its CHAIN: the ordered tiers Sample→…→Phrase that the mode actually uses, each
tier WITH ITS SOURCE (what triggers it). ONE declaration per mode = the single source of truth for
that mode's timing. Everything else CONSUMES the chain; nothing re-derives timing.
  CLOCK mode:  Sample → PPQN(periodic clock) → Step(= N PPQN) → Phrase
  PHASE mode:  Sample → PPQN(mapped from phase position) → Step(= N PPQN) → Phrase
  GATE  mode:  Sample(edge detect) → Step(= gate rise, nominal 1/16) → Phrase     [NO PPQN TIER]
KEY FACTS:
- Step/Phrase are MODE-INDEPENDENT at the top (the HEAVY work lives here); only the Step TRIGGER
  SOURCE differs (PPQN-count for clock/phase, direct gate-edge for gate).
- PPQN is a MODE-CONDITIONAL tier: PRESENT (clock/phase), ABSENT (gate — gates advance the step
  directly, there is no sub-step pulse grid). Absence is STRUCTURAL (not in the chain), not a runtime
  flag → "ask for PPQN in gate mode" is UN-EXPRESSIBLE, not a check that can be forgotten.
- PHASE drive is PPQN-QUANTIZED, NOT continuous: phase position → PPQN, so the heavy resolve fires on
  PPQN/Step ticks, NOT per-sample. The ONLY sample-rate work in phase mode is read-phase→map-PPQN→
  detect-tick (cheap). *** CHECK IN THE MEASUREMENT: does phase mode resolve per-PPQN (correct) or on
  RAW continuous phase per-sample (CPU bug)? If heavy work rides the per-sample phase read, that's a
  prime suspect for the 8%/124%. ***
- GATE/PHASE Step is TRIGGERED (aperiodic, emitted by a Sample-rate detector: gate-edge / phase-tick);
  CLOCK Step is CLOCKED. BOTH: the cheap detector is Sample-rate, the heavy Step work is Step-rate —
  they MUST be separated (detector emits trigger; heavy work runs on trigger). Tangling them = heavy
  work pays the per-sample tax (likely a current CPU contributor — verify the step-compute isn't on
  the per-sample edge/phase path).

### MODE-SWITCH = SWAP THE ACTIVE CHAIN
Producers (per tier) and consumers (declare a tier) do NOT change with mode. Switching mode swaps the
active chain → changes which tiers exist and each tier's source. Heavy Step work is mode-independent,
so NO downstream mode-specific code. A mode's ENTIRE timing identity = its chain declaration.

### THE CONTRACT (folds the 5 properties onto the chain)
1. ONE producer per tier, computed in ONE place at the tier's rate/source (the chain).
2. ALL modules (Monsoon, Macro, expanders) CONSUME the same computed tier values — never recompute,
   never reach into another module's timing (the Macro 124% was Macro re-deriving timing/values).
3. Consumer declares which tier (IN THE ACTIVE CHAIN) it reads; reads the published slot at that rate.
4. Batched over lanes/channels (one op all cells per tier, not per-cell-per-call).
5. ENFORCED: manager owns dispatch; can't request a tier absent from the active chain; can't force a
   faster rate than the tier's; no raw `if(divider.process())` to get wrong. Wrong-rate / cross-module
   recompute / duplicate-compute are UN-WRITABLE.
