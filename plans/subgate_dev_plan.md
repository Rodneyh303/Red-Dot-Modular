# subGate (gate subdivision) — dev plan (branch off master)

Source spec: [`docs/design/GATE_SUBDIVISION_STEP_GATE.md`](../docs/design/GATE_SUBDIVISION_STEP_GATE.md).
User name for the feature: **subGate** — a resolution clock, deliberately NOT called
STEP_GATE to avoid the near-homonym with the existing generated step-legato OUTPUTs
(`STEP_GATE_OUTPUT`, `gsStep`, Straits `POLY_STEP_GATE_OUT`, Changi T2/T3).

**Input source: Gate 3** (`GATE3_MOD_INPUT`) — already on the Monsoon panel. In modes B
(sequencer, main gate = Gate 1) and D (quantiser, main gate = Gate 2), Gate 3 becomes the
subGate clock. In other modes (A/C/E/F) Gate 3 keeps its current die-action role
(`gate3Target` menu / `gate3Trig`). **No new jack, no panel change, no new input enum.**

## Scope (confirmed with Rodney)
1. **Auto-raise.** Patching subGate automatically makes the playhead advance at the subGate
   rate; every main gate is subdivided at that resolution. Unpatched = today's behaviour
   (playhead advances one step per main-gate edge).
2. **All Sands lanes draw at subGate onsets.** At each subGate onset inside a main gate the
   engine does exactly what it does at a clock step: rest/legato/accent evaluated,
   melody/octave/q-mix drawn, Tie/Legato emergent from pitch equality, quantiser CV sampled.
   Pitch default is **ratchet** (a fresh draw that equals the held pitch reads as Tie; a
   different draw as Legato/NewNote — the existing engine rule, applied at the fine grid).
3. **GATE mode (B) + QUANTISER-gate mode (D) only.** NOT clock (A/C), NOT phase (E/F). Phase
   keeps PPQN; the doc's §"Phase mode" is explicit — do not wire subGate there.
4. **MONO first, then poly in the SAME branch.** Mono lands and verifies; poly (per-voice,
   correlated/reversible via Straits) + the Changi T2/T3 step outputs are folded into this
   branch as a later phase after mono is solid (not a separate branch).

## Branch
`feat/subgate-mono` off `master` (current work sits on `feat/raffles-cleanup-qmix`; this is a
clean, unrelated feature — branch from master so it merges independently).

---

## The core model (what actually changes in the engine)

Today (Mode B, [`SequencerEngine::executeModeB`](../src/dsp/engines/SequencerEngine.cpp:695)):
one **main-gate RISE** → `advancePlayhead()` + `executeStep()` (one step = one incoming gate).
The module layer ([`Monsoon.cpp:868-884`](../src/Monsoon.cpp:868)) then drives `gs.gateHeld`
from Gate 1 level every sample (IMPL 2b), so the gate WIDTH is the external gate's width.

With subGate patched, the two streams separate (spec §"Two edge streams"):
- **subGate edges** advance the playhead, define the fine grid, and are WHERE
  rest/legato/accent are evaluated + all pitch lanes are drawn (a `executeStep` per subGate
  onset).
- **main-gate edges** mark where note EVENTS begin and end in the incoming material — a note
  spanning N subGate cells is length N; rest can drop a cell; legato at each internal
  boundary decides tie vs re-articulate.

**Two tie scopes, both preserved (the subtle requirement, spec §"TWO tie scopes"):**
- *Intra-gate legato* — tie across subGate cells WITHIN one main gate (the subdivision case;
  the step-legato analogue).
- *Inter-gate legato* — tie across a MAIN-GATE boundary (the slur-across-notes the module
  already does in plain Mode B). **Must still work.**
- Rule: at a subGate edge inside a gate → intra-gate decision; at a main-gate edge →
  inter-gate decision. A note ENDS only when NEITHER says hold.
- **TRAP:** subGate must NOT force a re-articulation at every main-gate edge just because it
  is also a subGate boundary — that silently breaks inter-gate slurs (a today-capability).

```mermaid
flowchart LR
  subgraph inputs
    MG[main gate GATE1 edges]
    SG[subGate edges]
  end
  SG -->|advance playhead + evaluate rest/legato/accent + draw pitch| STEP[executeStep at fine grid]
  MG -->|note begins and ends, inter-gate legato| RECON[reconcile per cell]
  STEP --> RECON
  RECON --> GATE[gs.gateHeld note ends only when NEITHER holds]
```

### Alignment / robustness decisions (spec §"Alignment", baked in)
- **Off-grid main-gate edges:** quantise note start/end to the nearest subGate edge (the
  reason subGate was patched). Make it the explicit, only behaviour for v1.
- **subGate (Gate 3) stalls while main gates keep arriving:** playhead FREEZES (Gate 3 is the
  declared clock in B/D). Unpatch/repatch handover: when Gate 3 disconnects in B/D, revert to
  main-gate-drives-playhead on the next main-gate edge (clean fallback, no half-state).
- **Display:** the Sands/Lantern playhead must read the SAME resolved step the engine uses
  (published `stepIndex`/`gs` state), not a separately computed position — this is the
  display/engine race that bit the spread work. Under subGate it moves at the fine rate
  because the engine's `stepIndex` does.

### Accent (spec §"Accent") — no new output
Accent FOLLOWS THE ARTICULATION: asked only at ONSETS (fresh onset at a main-gate edge, or a
fresh onset at a subGate re-articulation). A tied continuation is not an onset → no re-accent.
This is automatic once `executeStep` runs at subGate onsets — no accent clock, **no
STEP_ACCENT output** (decided/declined in the spec).

### Pitch (spec §"Pitch") — ratchet default, Tie emergent
All Sands lanes draw at each subGate onset on the clock-mode step rules. Tie vs Legato is
pitch-equality (engine already does this — [`MonoDecision`](../src/dsp/engines/SequencerEngine.hpp:19)
Tie=same pitch, Legato=new pitch, NewNote=retrigger). The legato lane decides
retrigger-vs-not. Quantiser (Mode D) samples the incoming CV at each subGate onset, so a
repeated incoming pitch naturally yields a Tie.

### OPEN DESIGN QUESTION (Rodney, 2026-10) — should ratchet subgates RE-DRAW pitch, or HOLD it?
CURRENT BEHAVIOUR (verified in code): subgates advance the playhead on ANY rise
(mainGateRise || subGateRise || ghostRise → advancePlayhead; SequencerEngine.cpp:947), and ALL
pitch lanes (melody/octave/qmix) DRAW FRESH at each subgate (SequencerEngine.cpp:1015 "all pitch
lanes draw, Tie/Legato emergent from pitch equality"). So a ratchet is N independent fresh draws;
a Tie EMERGES only when a fresh draw happens to equal the previous pitch. This FELL OUT of the
"advance on any subgate" decision — applying step machinery one level down was simpler/consistent —
it was NEVER deliberately decided whether to hold pitch across a ratchet burst. (Ghost DOES redraw
because it's genuinely a new note; the ratchet-pitch question is the unconsidered one.)

TWO REGIMES (both legitimate, different instruments):
- RE-DRAW (current): a ratchet is a MELODIC FLOURISH — N hits, N (potentially) different pitches
  (arpeggiated/moving stutters). Pitch is a per-SUBGATE property.
- HOLD: a ratchet is a RHYTHMIC subdivision of ONE note — N hits, SAME pitch (drum-roll/tremolo).
  Pitch is the STEP's property; the ratchet divides TIME/articulation only, not pitch. Matches the
  common sequencer mental model ("ratchet subdivides the step; pitch is the step's").
A defensible default: HOLD (ratchet = time/articulation tool), with RE-DRAW as an opt-in
("re-pitch ratchets") — so the melodic-flourish is chosen, not a surprise to someone expecting a
tremolo. But it is a MUSICAL call (Rodney's), not an engineering one. The point: make it a DECISION,
not an inherited accident.

COUPLING-THEORY DEPENDENCY (OUTPUT_LAYER_DISTRIBUTION_MAPPING.md cross-lane note): this decision
picks which cross-lane coupling regime applies between a per-step lane (melody) and the per-subgate
Variation-driven-articulation:
- HOLD melody → melody is per-STEP, Variation per-SUBGATE → CROSS-RATE coupling: the step's melody
  atom gates the WHOLE ratchet burst's articulation ("accented step ⟺ its whole burst locks long").
- RE-DRAW melody → both per-subgate → SAME-RATE coupling: each ratchet hit's pitch couples to its own
  articulation. (The cross-lane note currently assumes the HOLD/cross-rate case — flag it contingent.)

CONSUMER-GROUP MODEL (Rodney — the uniform pattern; corrected + simplified):
ONE pattern repeated everywhere, no special lanes: a LANE produces a SHAPED draw (post-w / post-
distribution / post-clamp, out of the output layer) at its DECISION EVENT; GROUPS OF CONSUMER-KNOBS
each THRESHOLD that draw at their own event. Knobs CONSUME the shaped value (not feed it); NOTHING
changes on resolver/Sands (pure downstream read-side).

  LANE (draw source)      →  CONSUMER-KNOB GROUPS (each at its OWN event)
  ─────────────────────────────────────────────────────────────────────
  REST                    →  main-gate (base) · ghost-rest · ratchet-rest
  LEGATO                  →  main-gate (base) · ghost-legato · ratchet-legato
  VARIATION (gate mode)   →  pitch-hold (at main onset) · tie-across-rest (at rest-arrival)

Every cell is the SAME thing: a knob thresholding a lane's shaped draw at a specific event. No lane
hosts a compound/special value; no knob means two things.

WHY THE REQUIREMENT NEEDED THIS (not the overkill original): the point of independent subgate
legato/rest is a RATE independent of the main-gate level. The original idea (assign VARIATION to
subgate legato/rest) was OVERKILL — it added a whole separate draw when all you need is a separate
THRESHOLD on the main rest/legato lane. The consumer-group model gives the independence minimally.
And it FREES Variation for its best gate-mode use: PITCH-HOLD (hold vs re-draw pitch across a ratchet
burst) — the one NEW decision with no existing lane (rest/legato already have lanes; pitch-hold
doesn't), so it's the role that specifically WANTS Variation. More fundamental than the competitors
("tie across rests", "advance playhead on tie into rest") — those MODIFY existing behaviour; pitch-
hold adds a new DEGREE OF FREEDOM (ratchet = tremolo vs flourish). And "tie across rest" then becomes
just ANOTHER Variation consumer group (parallel to ratchet-rest), not a special second use.

INDEPENDENCE comes from DIFFERENT DRAW EVENTS + OWN KNOBS — the key timing fact:
- Ghost / ratchet / main advance the playhead at DIFFERENT moments → a ghost-rest knob and a ratchet-
  rest knob read the REST lane at DIFFERENT playhead steps = DIFFERENT, INDEPENDENT DRAWS (successive
  draws on a lane are independent). Same for Variation: pitch-hold samples at MAIN ONSET, tie-across-
  rest samples at REST-ARRIVAL → different events → independent.
- So same-lane-different-EVENT consumers are INDEPENDENT (not auto-correlated). CORRECTION to an
  earlier draft: the matrix does NOT "auto-correlate within a column" in general — that only holds
  where two consumers read the SAME SAMPLE (e.g. a ratchet subgate landing COINCIDENT with a main
  gate draw → same value). The robust coupling is CROSS-LANE at a SHARED draw event (rest-lane vs
  melody-lane at the same playhead position), per OUTPUT_LAYER_DISTRIBUTION_MAPPING.md.
- Because events differ, you need SEPARATE THRESHOLDS anyway to express different rates — so the
  consumer-knobs aren't adding fake independence, they EXPOSE control over already-distinct draws.

PHYSICAL HOME (Sands FULL):
- SANDS keeps the single per-lane knob (main level). User can leave subgate behaviour inheriting it.
- STRAITS = the consumer-group home: optional groups of SMALL set-and-forget threshold knobs (shrink
  OK — assignment controls, not performance controls). Add the groups you want (ghost/ratchet rest/
  legato, tie-across-rest, pitch-hold).
- CAUSEWAY must get matching mod coverage for any new Straits group — everything on Straits is
  Causeway-modulatable (the CV/mod layer, CHAT_HANDOFF:98); a group without it = the one that can't
  be modulated = a modulation-layer seam. Add the Straits group AND its Causeway mod path together.

GHOST pitch-hold cell: flagged to confirm — ghost is a gap/new-note, "hold" may be n/a there.
INVARIANT: within a SINGLE draw event, all consumers of that event read the SAME sampled shaped value
(no re-draw per consumer) — that is what makes SAME-EVENT consumers correlatable; DIFFERENT-event
consumers are independent.

---

## Where the code changes land (verified against the tree)

**No new jack / panel / input enum.** Gate 3 (`GATE3_MOD_INPUT`) is the subGate source.

| Area | File | Change |
|---|---|---|
| Gate 3 edge routing | [`Monsoon.cpp:683-690`](../src/Monsoon.cpp:683) (the `gate3Trig` block) | mode-dependent: in B/D, Gate 3 rise → subGate clock (advance playhead at fine grid); in A/C/E/F, Gate 3 rise → die action (unchanged). The `gate3Trig` edge detect already exists. |
| Dispatch gating | [`Monsoon.cpp:762`](../src/Monsoon.cpp:762) `shouldExecute` for modeSelect 1 (B) and 3 (D) | when Gate 3 connected in B/D: step on the subGate (Gate 3) rise instead of `gate1Rise`/`gate2Rise`; the main gate (Gate 1 in B, Gate 2 in D) is tracked as the note-event stream separately |
| Mode B/D step driver | [`ModeController::executeModeB/D`](../src/dsp/managers/MonsoonModeController.cpp:306) | route the fine-grid step through the SAME `executeStep` path; pass main-gate state so the engine reconciles note begin/end |
| Engine reconciliation | [`SequencerEngine::executeModeB`](../src/dsp/engines/SequencerEngine.cpp:695) + [`GateState`](../src/dsp/gates/GateState.hpp:42) | the two-edge-stream logic: advance on subGate; open/extend/close note from BOTH streams (intra vs inter tie scopes) |
| Module gate driver (IMPL 2b) | [`Monsoon.cpp:868-884`](../src/Monsoon.cpp:868) | extend the `gateOpen` expression so a note ends only when NEITHER the main gate NOR intra-gate legato holds; do not let subGate force re-articulation at main-gate edges |
| Display parity | Lantern / Sands playhead read of `stepIndex`/`gs` | verify it reads published engine state (no separate position) |

**No panel work:** Gate 3's jack, anchor, and bind already exist on the Monsoon panel. The
`cachedGate3Connected` flag already exists ([`Monsoon.cpp`](../src/Monsoon.cpp)). Reuse the
existing `gate3Trig` SchmittTrigger for the rising edge.

---

## Build phases (each ends compilable + `run_all.sh` green)

### Phase 0 — verify-before-building (spec §"Open question")
- Confirm in code what the engine reads TODAY at a step: `executeStep` runs rest→legato→
  accent→pitch draw→quantiser sample. Confirm step-legato currently only supplies boundaries
  WITHIN a committed slur (the extension is to let subGate govern ANY gate).
- Confirm the Mode B main-gate path (`executeModeB` + the IMPL 2b driver) is the SINGLE gate
  state source (it is — spec §5 SoT).
- Output: a short note in the plan confirming the entry points; NO code yet.

### Phase 1 — header-level engine test FIRST (spec §"Test", TDD)
Write `test/test_subgate.cpp` (synthetic edge streams, no Rack) BEFORE the engine change:
- (a) a main gate spanning N subGate cells → note length N; legato asked at N−1 internal
  boundaries.
- (b) in ONE pattern: a legato tie that HOLDS across a main-gate boundary AND one that
  re-articulates at a subGate boundary inside a gate — the test that catches subGate stomping
  inter-gate legato.
- (c) an off-grid main-gate edge quantising to the nearest cell.
- (d) unpatched subGate = byte-identical to today's Mode B (regression guard).
- Add to [`test/run_all.sh`](../test/run_all.sh) TESTS (companion: `$SE $GS $PE`).

### Phase 2 — engine: two-edge-stream reconciliation (mono)
- Add subGate-driven stepping to `executeModeB` (and the Mode D path via ModeController).
- Implement the intra-gate vs inter-gate tie-scope rule in the engine/GateState; a note ends
  only when neither holds.
- Off-grid quantise + subGate-stall freeze + unpatch handover.
- Make Phase 1 tests pass. `run_all.sh` green.

### Phase 3 — module wiring + panel
- `SUBGATE_INPUT` enum/config/edge-detect/InputState; dispatch gating; IMPL 2b `gateOpen`
  extension.
- Panel jack (generator + anchor + bind); anchor/bind audit green.
- Display parity check (playhead reads published state).

### Phase 4 — Rack verification (Rodney; container can't run Rack)
- Worked example: external seq mixing 1/8 + 1/16, send 1/16 clock to subGate → an 1/8 reads
  as two tied 1/16s; legato lane decides tie vs retrigger per the fine grid.
- Inter-gate slur across a main-gate boundary still works with subGate patched (the trap).
- Unpatched subGate = unchanged Mode B. Quantiser Mode D subdivides + samples CV per onset.
- Lantern shows the fine-rate playhead matching the audio.

### Phase 5 — docs
- Update the spec's "Open question" section to record the chosen answers (auto-raise; all
  Sands lanes at subGate onsets; gate+quantiser only; mono first; poly folded into this
  branch after mono lands).

---

## Explicitly OUT of scope (this branch)
- **Phase mode** — deliberately excluded (PPQN already is its resolution; spec §"Phase mode").
- **STEP_ACCENT output** — declined in the spec (derivable downstream if ever needed).
- **Internally-generated subdivision grid** (nothing patched) — rejected/deferred in the spec.
- **fresh-pitch ROLL opt-in** (vs ratchet default) — the per-voice/lane pitch fork; ratchet
  is the v1 default, the opt-in is a later refinement.

## Poly — folded into THIS branch after mono lands (phase order)
Mono phases (0-5) land and verify first. Then, in the same branch:
- **Phase P1 (DONE — no engine changes needed)** — the per-voice subdivision already flows
  through `postExecute_` → `executePolyVoices` (which runs per subGate onset because
  `result.stepped=true`). At each mono `NewNote` sub-cell, `monoGateStart=true` →
  `executePolyVoice` rolls each poly voice's own rest/legato/pitch via `polyRandomSrc`
  (the correlated + reversible payoff — WHICH voices split is seeded per-voice). Verified by
  3 poly test suites in `test/test_subgate.cpp` (per-voice rest/play, ratchet, tie-sustain).
  No `executeModeBSubdivided` change was needed — the mono sub-cell decisions drive the
  existing poly path.
- **Phase P2 (Rack-side)** — Straits `POLY_STEP_GATE_OUT` / `POLY_STEP_LEGATO_GATE_OUT` +
  Changi T2/T3 step outputs read `voices[i].gsStep.process()` (OutputGenerator:68), which
  re-triggers per sub-cell via `executePolyVoice`'s `gsStep.triggerNote()`. Verify in Rack
  that the poly step jacks emit at the fine grid under subGate.
- **Phase P3 (Rack-side)** — Rack verification: poly voices subdivide independently +
  reversibly; the Straits/Changi step outputs pulse at the subGate rate.

## Risks / watch-items
- **Inter-gate legato regression** — the headline trap; the Phase 1 (b) test exists to catch
  it. Do not force re-articulation at a coincident main-gate + subGate edge.
- **Display/engine race** — publish one resolved step; the playhead must not recompute
  position (the spread-work bug class).
- **Rate discipline** — subGate is a new edge source; ensure mods sampled on the right edge
  (see [`EXTERNAL_GATE_ARTICULATION_CHECK.md`](../docs/design/EXTERNAL_GATE_ARTICULATION_CHECK.md)
  and RATE_DISCIPLINE_UNIFICATION) — the same "which edge samples the mod?" question.
- **Naming** — user-facing label **subGate** everywhere; keep it distinct from the existing
  STEP_GATE OUTPUT family in code comments to prevent future conflation.

────────────────────────────────────────────────────────────────────────────────
## GLOBAL-vs-POLY SCOPE + SUBGATE CONTROL — the settled model (Rodney)

### CORRECTION ON RECORD (Claude error, Rodney caught):
"Global" does NOT mean "all voices get the identical outcome / comonotone / rho=1". That was WRONG.
GLOBAL = SHARED CONTROL PARAMETER, INDEPENDENT DRAWS. A global accent knob = all voices share the same
accent PROBABILITY but still draw independently (they don't accent on the same steps). A global
VARIATION knob = all voices share the same variation RANGE/amount but still pick DIFFERENT lengths
within it. So global-vs-poly is a CONTROL-SHARING choice, ORTHOGONAL to the copula/correlation layer
(spread/CA govern how DRAWS relate; global-vs-poly governs whether the KNOB is shared). A global-knob
lane can still be spread-correlated. Do NOT conflate "share a control" with "correlate the outcomes".
Also: MELODY/OCTAVE have NO Straits per-voice (poly) knobs → the global-vs-poly choice does NOT apply
to them; it applies to the Straits MAIN-GATE lanes that HAVE poly knobs (rest/accent/qmix/legato/
variation).

### THE MODEL
1. GLOBAL-vs-POLY per STRAITS MAIN-GATE LANE: each such lane gets a user choice — GLOBAL (one shared
   control knob for all voices) or POLY (the per-voice knob group). e.g. global accent + global qmix +
   poly rest + poly legato + global variation. Shared knob = same parameter, independent draws.
2. SUBGATE control = the SAME choice applied to ghost/ratchet decisions, with THREE ways to source it
   (highest engaged wins, per decision, uniform resolution):
   - FOLLOW MAIN (free): subgate inherits the lane's main knob (global or poly, whichever main is).
   - GLOBAL SUBGATE KNOB: a shared global knob gives the subgate its own character distinct from main,
     WITHOUT consuming the main level's modulation budget.
   - ASSIGNED POLY SUBGATE GROUP on STRAITS (opt-in, a FEW assignable groups, e.g. 3): full per-voice
     control for the ghost/ratchet decisions the user prioritises. Costs Straits/Causeway space; capped
     + assignable (user points the 3 at whatever they want poly). Decisions without an assigned group
     fall back to global-subgate or main (graceful — no decision is ever uncontrolled).
3. VARIATION in GATE MODE:
   - WITH subgates: Variation does PITCH-HOLD (hold vs re-draw pitch across the ratchet burst) — again
     a GLOBAL or POLY choice (poly reuses the idle gate-mode Variation per-voice knobs = free).
   - WITHOUT subgates: Variation is free for another gate-mode role (candidates: tie-across-rest,
     advance-playhead-on-tie-into-rest — see the open-question + consumer-group notes above).

### ADJUNCT TECHNIQUE (not a built tier): main-gate-state as a region modulation KEY
In poly/follow-main, a user CAN get region-independent ghost/ratchet levels by modulating the main
level with the MAIN GATE STATE as key (gate HIGH = ratchet region → one offset; gate LOW = ghost
region → another; step modulation). Gate state IS the region signal (no new source needed). COST: it
CONSUMES the main level's modulation path → can't also apply other modulation to that level. THAT is
why the dedicated GLOBAL SUBGATE KNOB (tier 2) exists — it gets region-independence WITHOUT spending
the main modulation budget. (VERIFY when building: is gate state accessible as a mod source at the
point the subgate rest/legato threshold is read.)

### SEAM-SAFETY
global-vs-poly is a uniform SCOPE flag per lane/decision (resolver reads a shared value vs a per-voice
bank for `Inputs.base` — upstream of resolution, algebra unchanged). Same mechanism everywhere, no
per-lane special-casing → seam-free. Subgate = the same flag at a finer event, housed on Straits
(Sands full). Assignable groups are the same structure wherever pointed.
