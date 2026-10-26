# RHYTHM DECISION MANAGER → FOREST RHYTHM DECISION — the two-stage plan

Authority chain: `VOICE1_CONTAINMENT_ALGORITHM.md` (the algorithm reference, filed 2026-10-26,
commit 1549ca89) <- `CC_PROMPT_CONTAINMENT_BUILD_PLAN.md` (the containment module, steps 1-4
executed, CI-green through ae88a36b) <- `CC_PROMPT_CONTAINMENT_MODULE_SPEC.md` (the spec).

Rodney brief (2026-10-26): "make a plan to generalise the containment decisions and connect it
to containment module? We need to build a 'rhythm decision manager' that centralises all the
decisions you documented. That's a pre-requisite to building a more flexible design. then plan
how we can build the 'forest rhythm decision' by applying the decisions though the containment
structure."

Status: FULLY RULED 2026-10-26 (Rodney, chat) — all seven decisions banked (§7). D4 = the
topologically-first sounding onsetting container (option i). The D3 union/intersection split
is confirmed PRINCIPLED (§3.9): containment is a STATE relation (states intersect); onsets
are EVENTS (events union) — and the intersection envelope is still in force at every onset
moment, so no sound ever escapes containment. Stage R and the whole F ladder are unblocked.

---

## 0. Why two stages, in this order

Today the containment decisions are SCATTERED. The five mechanisms C1-C5 live in four files
(SequencerEngine.cpp's poly cascade, Monsoon.cpp's per-sample drive, PatternEngine's clamp,
ModeController's threshold feed), and the "parent" they all read is hardwired: voice 1, through
four bare scalars (`lastStepResult.decision`, `lastStepResult.nvIdx`, `wasHeldMono`/`hadMonoTail`,
`gs.gateHeld`). You cannot generalise a decision structure that has no single address — touching
any one of these sites without centralising first means re-hardwiring V1 in a new shape.

So:

```
Stage R — RHYTHM DECISION MANAGER (RDM)
  Centralise: every V1-relative permission decision (C1-C5) + the per-edge verdict tuple
  behind ONE rack-free decision core with ONE named parent-view type. Behaviour-identical:
  the acceptance gate for every commit is byte-identical gate/accent/pitch streams on
  seeded marches through all three modes. After R, the engine is a forest machine that
  happens to be running a star forest (every poly voice's parents = {V1}) — with the star
  declared as DATA, not implied by hardwired reads.

Stage F — FOREST RHYTHM DECISION (FRD)
  Re-point: the RDM's parent source from the declared legacy star to the REAL graph —
  PatternEngine::containmentParents[16], already staged every block by the containment
  module's publish contract (step 4), currently written and never read. Evaluate envelopes
  in topological order; generalise C1-C5 to the multi-parent DAG. Cantonment structures
  (nested chains, diamonds, free voices) become expressible; the containment module's
  16x16 pin plane becomes the authoring surface for rhythm containment.
```

Each stage is independently landable, independently CI-green, and each step within it is one
commit with one acceptance gate. Stage R is the prerequisite Rodney named; Stage F is the
flexible design it unlocks.

---

## 1. The decision inventory (what the RDM centralises)

Every site below verified on the current tree (2026-10-26). The reference doc's §6 table is
the semantic authority; this is the file-precise map.

| # | Decision | Current home (exact) | What it decides |
|---|---|---|---|
| M0 | Mono cascade verdict | `src/dsp/engines/SequencerEngine.cpp:493-731` (`executeStep`) | V1's own per-edge chain: MidNote/Rest/Tie/Legato/LegatoMax/NewNote + accent + LEAD commitment. NOT centralised — it stays as the ROOT's own decision producer. |
| C1 | Onset permission | `SequencerEngine.cpp:1128-1131` (`monoGateStart`) | A poly voice may only BEGIN a note at a V1 onset. |
| C2 | V1-rest close | `SequencerEngine.cpp:1270-1276`; `src/Monsoon.cpp:1170` (`!isRest` term) | V1 rests → voice force-closed, participating cleared, NO draws consulted. |
| C3 | Length clamp | `SequencerEngine.cpp:1499-1508` (`nvIdxForVoice`, the max() at 1507) | Voice hold ≤ V1's next event (NOTE_VALUES higher = shorter; max() picks shorter). |
| C4 | Hold-follow | `SequencerEngine.cpp:1352-1388` (the 1381 guard) | MidNote: voice follows V1's articulation SHAPE only inside its own armed window. |
| C5 | Envelope intersection | `src/Monsoon.cpp:1198-1213` (the AND at 1206), latch at `Monsoon.cpp:1011` | Gate mode: sounding gate = V1 envelope (`gateOpen`) AND voice's latched play decision. |
| W1 | Mono threshold feed | `src/dsp/managers/MonsoonModeController.cpp:66-137` (`updatePatternInput`) | Sole writer, mono slot. NOT centralised — unchanged. |
| W2 | Poly threshold feed | `MonsoonModeController.cpp:30-64` (`updatePolyVoiceRest_`), called at 237 before `executePolyVoices` at 240 | Sole writer, poly slots (Tier-V latch under lock). NOT centralised — unchanged. |
| O1 | Own draws | rest/accent/slur/var rolls via `pe.warpRow(voice, lane)` (SequencerEngine.hpp:145 `PatternEngine pe;` — direct member) | The voice's OWN rest/accent/pitch/slur draws. NOT containment — the permission structure only gates WHEN they are consulted. |
| O2 | VAR/LEG delegation | `SequencerEngine.cpp:1477-1497` (`getVariationStepForVoice`/`getLegatoStepForVoice`) | Whose VAR/LEG LOR cell the voice reads (V1's by default, own under Local-East). |

The ParentView insight (reference doc §7.3): C1-C4 all read V1 through exactly FOUR scalars.
C5 reads one more (`gateOpen`'s components). Centralising = naming those reads once.

---

## 2. Stage R — the Rhythm Decision Manager

### 2.1 Where it lives and what it is NOT

`src/dsp/engines/RhythmDecisionCore.hpp` — rack-free (no Rack headers; the mock battery
compiles it directly, same discipline as ContainmentModuleCore/ContainmentPending).
SequencerEngine composes it.

Deliberately NOT:
- **Not a state owner.** Gate state stays in `VoiceBank voices` (SequencerEngine.hpp:155) —
  one source of truth, the CA-style "engine renders what the state says" contract intact.
  The RDM computes PERMISSIONS from states; it holds none.
- **Not a new writer.** ModeController keeps W1/W2 (the sole-writer map, reference doc §2).
- **Not a behavioural change.** Every Stage-R commit is byte-identical on the differential
  marches (§2.5) or it does not land.

### 2.2 The interface (sketch)

```cpp
// The per-voice, per-edge verdict tuple the engine consumes. Assembled ONCE per voice
// per edge; the cascade reads the tuple instead of re-deriving permissions mid-flight.
struct RhythmDecision {
    bool        mayOnset;       // C1: a permission point exists this edge
    bool        envelopeOpen;   // C2+C5: the containment envelope is open (all containers sounding)
    bool        forcedRest;     // C2: a container rested -> close, consult no draws
    int         lengthCeiling;  // C3: NOTE_VALUES index ceiling (>= own nv after clamp)
    bool        holdFollow;     // C4: containers mid-gate -> follow the articulation shape
    MonoDecision gateType;      // D4: whose verdict names trigger vs slide at a shared onset
};

// The named parent read. Today's four scalars, promoted to a type.
struct ParentView {
    MonoDecision decision;      // the container's verdict this edge
    int          nvIdx;         // the container's note length this step
    bool         gateHeld, wasHeld, hadTail;
};

class RhythmDecisionCore {
public:
    // THE SEAM Stage F re-points. Stage R: LEGACY_STAR (word[v] = bit0 for v>=1, 0 for v=0).
    // Stage F: GRAPH (the staged containmentParents, topologically ordered).
    void        setParentSource(ParentSource src);
    uint16_t    parentsOf(int v) const;                    // bitmask, multi-parent native
    ParentView  viewOf(int u, const EdgeContext& e) const; // per-container read

    RhythmDecision decide(int v, const EdgeContext& e) const;   // composes C1-C5 over parents(v)
};
```

The five mechanisms become named pure predicates in this header — table-driven unit-testable
in isolation, composed by `decide()`. A one-parent ∧ IS today's code path; the star graph is
the degenerate forest, so nothing about the algebra is speculative.

### 2.3 The migration steps (each = one commit, each gated)

**R-1 — The reference oracle.** NEW `test/test_rhythm_decision_differential.cpp`: seeded
marches (the S8 march pattern, 60+ seeds) through all three modes with subgates, locks and
per-voice params varied; records per-voice per-edge `gateHeld` / `gsStep.gateHeld` /
`accented` / pitch streams to golden captures checked into the test. SequencerEngine-level
testing is established ground (test_per_voice_articulation, test_legato_leading_edge,
test_rule2_consume/integration, test_subgate, test_gate_mode_agnostic all compile it) — this
harness composes the same machinery with a capture layer. Deliverable: the oracle EXISTS
before any engine line moves.

**R-2 — ParentView extraction.** `ParentView` lands in RhythmDecisionCore.hpp;
`executePolyVoice`'s four scalar V1 reads (1128, 1507, the wasHeldMono/hadMonoTail captures
at 1400 + feeders, the MidNote branch's mono gate read) re-point through
`monoParentView()`. Pure rename-level refactor; marches byte-identical.

**R-3 — The five predicates.** C1-C5 extracted as named functions taking
(ParentView(s), own state): `onsetPermission`, `restClose`, `lengthCeiling`, `holdFollow`,
`envelopeAnd`. Call sites: the poly cascade and Monsoon.cpp's 1198-1213 loop. Marches
byte-identical.

**R-4 — The tuple assembly.** `RhythmDecision::decide()` composes the predicates; the
cascade consumes the tuple. THIS is the commit where "all decisions centralised" becomes
literally true — one address (the RDM) answers every permission question the engine asks.
Marches byte-identical.

**R-5 — The star made explicit.** `ParentSource::LEGACY_STAR` declared as DATA (constexpr
word[16]); `parentsOf()` returns from it; the C-compositions already loop over the bitmask
(a 1-parent loop is today's code). After R-5 the engine is a forest machine running a star
forest — and D2's legacy mapping is a toggle, not a rewrite. Marches byte-identical.

### 2.4 What Stage R deliberately does not do

Does not move gate state; does not touch W1/W2; does not change `executeStep` (the root's
own decision producer); does not change output rendering (it renders whatever `gs.gateHeld`
says); does not change the module, the transforms, the pending stack, or the panel.

### 2.5 Stage-R test plan

- The differential marches are the acceptance gate for EVERY Stage-R commit — no green
  marches, no land.
- NEW `test/test_rhythm_decision_core.cpp`: the five predicates table-driven over every
  MonoDecision × own-state combination; the star-graph data; tuple assembly vs the
  predicates' direct composition; R2-style seed marches for the pure layer.
- All existing V1-semantics suites stay green UNTOUCHED — they are the compat pin
  (test_per_voice_articulation, test_legato_leading_edge, test_poly_slur_roll,
  test_rule2_consume, test_rule2_integration, test_note_length, test_subgate,
  test_gate_mode_agnostic, test_voice_conventions, test_mono_dir_authority).

---

## 3. Stage F — Forest Rhythm Decision

### 3.1 The source re-point (F-1)

`ParentSource::GRAPH`: `parentsOf(v)` reads `pe.containmentParents[v]`
(src/dsp/engines/PatternEngine.hpp:295; staged every block by
`MonsoonSandsManager::processDNA`, src/dsp/managers/MonsoonSandsManager.cpp:95/97 — the
step-4 publish contract, today write-only).

D2 RULED (Rodney: "no need to worry about legacy patches. All following V1 (current
behaviour) just becomes a cantonment preset."):
- **Graph truth everywhere.** No module-presence switch, no engine-side legacy mode, no
  migration default. The RDM's parent source is `containmentParents` whenever a containment
  module publishes; all-zeros / absent module = FREE voices — intended.
- **The legacy sound is an authored preset**: the all-contained-in-V1 star graph, loadable
  from Cantonment/module state like any other structure.
- **Test seam (required by the ruling)**: the differential harness and the existing
  V1-semantics suites must inject the star EXPLICITLY (the RDM's parent source is settable
  in tests) — they keep pinning the legacy sound as one authored graph among many. Without
  this injection the old suites would break at F-1 for the right reason (voices free under
  an empty graph) and stop testing what they exist to test.

### 3.2 Topological evaluation (F-2)

The graph is a DAG (cycle-reject at the pin plane; invariants CI-marched on 600+ seeds —
test_containment_transforms, test_containment_pending P9, test_containment_module S8), so a
topological order exists and parents resolve before children.

- The order is computed from the 32 bytes of parent words and CACHED, invalidated only on
  word change (memcmp per block — the graph only moves on pin edits and transforms; the
  steady-state cost is one memcmp per block). 16 nodes: the sort is trivial, and it lives in
  the RDM, not the hot loop.
- **Per step edge**: `executePolyVoices` visits voices in topological order (today: slot
  order 1..15, which happens to be valid only because the star's parents are always slot 0 —
  under a general forest it must be the cached order).
- **Per sample** (gate mode, C5): Monsoon.cpp's 1198-1213 loop walks the same cached order,
  so each voice's final `gs.gateHeld` is complete before its children read it. Same AND
  shape, per-parent instead of implicit-V1.

### 3.3 The C1-C5 forest table (F-2/F-4)

With `parents(v) = {u : bit u set in containmentParents[v]}`, `E(v) = Own(v) ∧ ∧_{u} E(u)`:

| Mech | Forest form | Reduces to today when |
|---|---|---|
| C1 | `mayOnset(v)` = (∃ container onsetting) ∧ (all containers' envelopes open) — D3 RULED: UNION for onset permission, INTERSECTION for temporal extent (envelope + length) | parents = {V1} → exactly `monoGateStart` |
| C2 | `forcedRest(v)` = ∨ over containers of container-rested (the ∧ of envelopes closes on any rest) | exactly the 1270-1276 branch |
| C3 | `lengthCeiling(v)` = max over containers of `nvIdx` (higher = shorter; the SHORTEST container window wins) | exactly the 1507 clamp |
| C4 | `holdFollow(v)` = containers mid-gate ∧ own armed window; the SHAPE followed is the topologically-first sounding container (D4) | exactly the 1352-1388 branch |
| C5 | `envelopeOpen(v) = Own(v) ∧ ∧_{u} envelopeOpen(u)`, per sample, topo order | exactly the 1206 AND |
| Chain latch | `participating(v)` latches at every onset permission point, clears whenever the envelope closes (a superset of today's V1-rest clear) | exactly Rule-2 |
| Accent | own draw at every permission point; re-articulation draws fresh (1332-1338 rule, unchanged) | unchanged |
| Ghost (D5) | v's ghost candidacy = own VAR draw, hosted only in a gap of the INTERSECTION envelope (a gap for every container simultaneously), riding the host containers' ghost stream | exactly today's mono-gap ghost |

### 3.4 The Cantonment connection (what F unlocks)

- **The transport already exists.** containmentParents staged every block IS the interface
  between any graph producer and the engine. Cantonment-generated structures reach the
  engine by populating the module's graph — the pin plane by hand today.
- **Nested and multi-parent need zero per-structure code.** The ∧ over parents is
  transitively closed through the DAG; depth and diamonds are the same code path as the
  star. The property suite (§5) proves `E(v) ⊆ E(u)` for every edge of every parent edge on
  seeded random forests.
- **The tabled forest verb returns as the natural authoring path.** The parked "seed" verb
  (random forest generation, Rodney: "come back to that idea later") slots in AFTER F as a
  graph producer — engine-side it needs nothing new. The three deferred rulings (merge /
  strict forest / pool-scoped) can be taken up whenever.
- **The transform stack is already graph-generic.** Rotate/reflect/invert/collapse/expand
  are proven DAG-safe on 600-seed marches; they keep working on general forests unchanged —
  INVERT in particular produces V1-as-leaf graphs (full edge reversal of the star), which
  is exactly why D7 below is not optional.

### 3.5 The commit ladder

- **F-1** — source re-point (D2 RULED: graph truth everywhere; no switch, no engine legacy
  mode). The all-contained-in-V1 star ships as a Cantonment preset / module graph state.
  The differential harness + existing V1-semantics suites inject the star explicitly
  (§3.1 test seam). Star graphs byte-identical; a NEW suite covers free-voice graphs.
- **F-2** — topological order + the C1-C5 forest compose + participating generalisation.
  Star graphs byte-identical; nested/diamond suites land here.
- **F-3** — the mono drive inside the algebra (D7, §3.6).
- **F-4** — ghost hosting (D5) + the VAR/LEG delegation sweep under forests (D6 scope).
- **F-5** — the property suite + the full compat-pin run; branch ready for step 6 (PR).

Every F commit: full battery + step-0 smoke + UNIFORMITY_STRICT + anchor audit, CI-green on
all three platforms before the next lands — the standing discipline.

### 3.6 D7 — the mono voice inside the forest (NEW, needs a ruling)

The reference doc's D1-D6 assumed the forest contains voices 2..16. It need not: the pin
plane allows pinning voice 1 UNDER another voice, and INVERT (a proven-safe transform)
produces such graphs by construction — full edge reversal of the star puts V1 as the leaf
of every chain. Forbidding it would break a transform-safety result already proven in CI;
therefore the honest generalisation is:

- **D7 recommended**: V1 is INSIDE the algebra like everyone. The mono drive
  (Monsoon.cpp:1128-1196) ANDs its parents' envelopes into `gateOpen` — the same per-parent
  ∧ the poly drive already performs, applied to the root path. A parentless V1 (the star,
  the empty graph) is bit-identical to today.
- The alternative (V1 privileged-root, panel/transform constraint against V1-as-leaf)
  contradicts INVERT's proven closure and adds a special case to every mechanism. Not
  recommended.

### 3.7 Phasing scope (D6 — RULED)

Engine-internal. Rodney's note: "I see no connection to LOR which is about the
probabilities that feed the rhythm decisions" — exactly: the LOR/warp plumbing produces the
DRAWS, the decision layer consumes them; generalising the decision structure does not touch
the draw production. VAR/LEG delegation keeps reading the root's cell under the default.
Closed.

### 3.8 D4 refresher — how poly trigger/slide works TODAY (written for the ruling)

The three gate-type methods (src/dsp/gates/GateState.hpp:15-17): `triggerNote` = fresh
attack (retrigger, the 1 ms dip lives in process()); `slideNote` = legato (pitch glides to
the new note, NO retrigger, gate stays held); `extendHold` = tie (same pitch continues).

A poly voice's gate type is decided at TWO event classes:

**(a) Chain ONSET** — V1 starts a chain (verdict NewNote, or Legato/LegatoMax from a dead
gate); SequencerEngine.cpp:1216-1227:
- V1 NewNote → voice triggers FRESH. A new chain always opens with an attack (own pitch,
  own accent draw).
- V1 slurring AND the voice genuinely still sounding (wasHeldPoly || hadPolyTail) → voice
  SLIDES to its own newly drawn pitch.
- V1 slurring but the voice NOT sounding (it rested, never joined) → fresh trigger anyway
  — the isolated-teal guard: no slide from nothing.

**(b) LANDING** — V1 mid-chain (verdict Legato/LegatoMax/Tie); 1290-1350, Rule-2 consume:
- Not participating (rested at the chain onset) → silent.
- Participating: `connect = v.gs.slurForward && stillSounding` — the voice's OWN commitment
  (rolled at the PREVIOUS onset from its own legato draw; delegated → V1's LEG cell) AND
  the clamp-accurate still-sounding read.
  - connect && V1 Ties → `extendHold` own pitch.
  - connect && V1 Legato/LegatoMax → `slideNote` to own new pitch (keeps the chain's
    opening accent).
  - !connect (own slur didn't fire, or the short note already clamped closed) →
    RE-ARTICULATE `triggerNote` fresh with a NEW own accent draw — a re-struck note is not
    stuck with the accent the chain opened on.
- Either way the voice re-rolls its own slurForward for the next landing.

**THE STRUCTURE**: V1's verdict names the OPPORTUNITY at the edge (Tie → extend possible,
Legato → slide possible, NewNote → attack); the voice's OWN prior commitment +
still-sounding state decides whether it CONNECTS or re-strikes. The voice never slides
merely because V1 slides. Gate mode: identical shape at every gate rise (main/ratchet/
ghost streams all run the same cascade); the C5 per-sample AND holds the envelope width;
the gap bridge can carry a committed slur across structural gaps; ghost→main tie-through
is emergent.

**Why D4 exists under the union-onset ruling**: at a permission point, containers that did
NOT onset are mid-hold (no verdict to read). Exactly one container onsetting is
unambiguous — its verdict names the opportunity, matching today. The question is ONLY when
MULTIPLE containers onset on the same edge with different verdicts (one Ties, one Legatos,
one NewNotes): does the voice extend, slide, or re-strike?

**Options**: (i) the topologically-first SOUNDING onsetting container's verdict names the
opportunity — **RULED (Rodney, 2026-10-26: "Option (i) seems best")**: deterministic,
mode-independent, reduces exactly to V1 in the star, composes with nesting (the chain's
head dominates — "the voice tracks the shape of its container chain"); (ii) first in voice
order — arbitrary under INVERT-produced graphs; (iii) earliest onset in sample time —
ill-defined in clock/phase (all voices fire on one stepEdge) and unstable across gate
streams. Note the voice's OWN commitment still gates whether it connects: D4 only names
whose verdict DEFINES THE OPPORTUNITY TYPE.

### 3.9 Why the D3 split is principled (states intersect, events union)

Rodney's consistency challenge ("is it inconsistent to use intersection for length and
union for onset? should it be intersection throughout?") — answered, and the split stands:

- **Containment is a STATE relation.** Where the voice may BE SOUNDING is bounded by all
  containers — rests, gate width, hold extent, length clamp: all states, all intersection.
  The original asymmetry ("can rest when V1 rests, can't sound when V1 rests") is a state
  asymmetry and stays intersection-exact.
- **Onsets are EVENTS.** When the voice may RE-ARTICULATE is an opportunity question, not
  a boundary question: a container's attack offers a trigger, it does not bound the voice.
  In one formula: `mayOnset = (∨ container onsets) ∧ (∧ envelopes open)` — the intersection
  is STILL in force at the onset moment (the all-open gate is part of C1), so no sound ever
  escapes containment; the union only selects among events the intersection permits.
- **Intersection-throughout is coherent but severe**: with two containers of independent
  rhythms, simultaneous onsets are rare — the voice would be capable of sounding in the
  intersection yet only ever ATTACK at coincidences, holding rigidly between. Multi-parent
  containment collapses into a rhythm-lock novelty (v under a kick + a pad: union re-strikes
  every beat inside the pad's envelope; intersection speaks twice a phrase).
- **The clincher**: with a single parent, union-over-{V1} and intersection-over-{V1} are
  IDENTICAL — both reduce to today's `monoGateStart`. The rules differ only where
  multi-parent exists, and there union keeps it musically alive.
- Escape hatch: the strict coincidence-only texture stays expressible later as a per-voice
  onset-mode toggle — under the RDM centralisation that is a one-predicate change.

---

## 4. The end state (what exists after F that does not exist today)

1. **One address for rhythm decisions.** The RDM answers every permission question the
   engine asks; the four-scalar V1 hardwiring is gone; the decision layer is rack-free and
   mock-battery-covered.
2. **The forest is the semantics.** Containment is no longer "subtractive from voice 1" —
   it is `E(v) = Own(v) ∧ ∧_{u ∈ parents(v)} E(u)` over the authored DAG, of which
   voice-1 containment is the single-parent instance.
3. **The containment module is the authoring surface** for rhythm containment: 16x16
   multi-pin plane, store-backed undo, transforms, trajectory — all already landed and
   graph-generic — now audible through the whole engine, all three modes.
4. **Cantonment structures drive rhythm** by populating the graph (hand-pins now, the
   tabled seed verb later, JSON import if wanted) — nested chains, diamonds, free voices,
   no engine special cases.

---

## 5. Test architecture

| Layer | Suite | Content |
|---|---|---|
| Pure predicates | NEW test_rhythm_decision_core | Table-driven C1-C5 over every MonoDecision × own-state combo; star data; tuple assembly. |
| Differential | NEW test_rhythm_decision_differential | Seeded marches, all three modes + subgates + locks; golden per-voice gate/accent/pitch streams; THE acceptance gate for every Stage-R commit and the star-equivalence pin for every Stage-F commit. |
| Forest property | NEW test_forest_rhythm | `E(v) ⊆ E(u)` ∀u∈parents(v) at every edge, seeded random forests; nested depth ≥ 3; diamonds; free voices; INVERT-produced V1-as-leaf graphs; star-graph byte-identity vs the Stage-R engine. |
| Compat pin | the ten existing V1-semantics suites | Untouched and green from Stage R; at F-1 they run with the STAR graph injected explicitly (the legacy sound as an authored preset, D2) — they pin that graph's byte-identity. |
| Graph layer | the existing containment suites (transform/pending/module) | Already landed; unchanged; they keep proving the DAG invariants the algebra stands on. |

Discipline carried: the mock battery stays rack-free; any suite instantiating trajectory
rings heap-holds them (the 2 MiB / Windows-2MB-stack rule); R7 token hygiene in comments.

---

## 6. Risk register

| Risk | Mitigation |
|---|---|
| Behaviour break at D2 | RULED INTENDED — no mitigation required. The star preset preserves the legacy sound as an authored choice; existing suites inject the star so the compat pin stays meaningful (§3.1 test seam). |
| Topo-order cost in the per-sample loop | 16 nodes, order cached on graph change (one 32-byte memcmp per block); the loop's shape is unchanged from today's AND. |
| The differential harness misses a mode/lock interaction | Marches enumerate mode × subgate × lock × per-voice params as crossed dimensions (the S8 march skeleton), 60+ seeds; existing suites double as an independent pin. |
| INVERT-produced graphs surprise the engine | D7 recommended reading makes V1-as-leaf FIRST-CLASS; the property suite explicitly marches such graphs (they are in the proven transform closure already). |
| Tier-V latch interplay (locked poly thresholds under forests) | The RDM reads thresholds through the same voices[] state the latch governs — no new path; the differential marches include lock windows. |
| Scope creep into LOR-per-container (D6) | Explicitly deferred; F's diff surface stays in the permission scaffolding. |

---

## 7. The ruling table (Rodney's calls)

| # | Question | RULING (Rodney, 2026-10-26) |
|---|---|---|
| D1 | Multi-parent envelope semantics | **RULED: INTERSECTION** — "intersection seems OK (voice can only sound if parents are)". Matches the multi-pin row language and the CA pin precedent. |
| D2 | The zero-row / legacy mapping | **RULED: no legacy mode.** "No need to worry about legacy patches. All following V1 (current behaviour) just becomes a cantonment preset." Graph truth everywhere; absent/zero graph = free voices, intended; the all-contained-in-V1 star ships as a preset; test suites inject the star explicitly (§3.1). |
| D3 | Onset permission under multi-parent | **RULED: UNION for onset, INTERSECTION for extent** — "union for onset and intersection for length"; confirmed principled in §3.9 (states intersect, events union; the intersection envelope still gates every onset moment). A permission point = ANY container onsetting, subject to the intersection envelope open; C3 length clamp stays the SHORTEST container window. |
| D4 | Gate-type authority at a shared onset | **RULED: option (i)** — the topologically-first SOUNDING onsetting container's verdict names the opportunity ("Option (i) seems best"). Refresher in §3.8. |
| D5 | Ghost hosting under a forest | **RULED: INTERSECTION** — "agree intersection". A voice's ghost exists only in gaps of the intersection envelope; candidacy = its own VAR draw. |
| D6 | Phasing scope | **RULED: engine-internal.** "I see no connection to LOR which is about the probabilities that feed the rhythm decisions" — the draw production is not the decision structure. Closed. |
| D7 | The mono voice inside the forest | **RULED: V1 inside the algebra, no privilege.** "No reason for V1 to stay privileged and this is part of it. All voices could be contained in voice 2 to 16 as well." F-3 confirmed; INVERT-produced V1-as-leaf graphs are first-class. |

Stage R is blocked by NONE of these, and now neither is Stage F: all seven rulings are
banked. The full ladder R-1..R-5, F-1..F-5 is GO on Rodney's word.

---

## 8. What does not change

`executeStep` (the root's own cascade — MidNote guard, leading-edge slur capture, the
branch priority, accent inheritance, LEAD commitment); GateState and the pulse ticking;
the LOR/warp/spread plumbing; ModeController's sole-writer feed (W1/W2, Tier-V latching);
the output rendering (it renders whatever `gs.gateHeld` says); the containment module's
UI/store/undo/pending/trajectory stack (already graph-generic and CI-marched); the
transforms' proven safety set; the panel and its 18 anchors.
