# East: per-voice LOR over the mono VARIATION / LEGATO probability

**Status: Stages 1–2 built on `integration/east-extra-lanes`; Stage 3 (Rule 2) designed here, not yet
built.** Dev lives on that branch until the idea is proven good by ear. Do not merge to master on the
strength of this document. Leading-edge legato is now the *only* legato model (reactive path removed on
`cleanup/remove-reactive-legato`); Rule 2 (§4d) depends on that and is described accordingly.

Origin: Rodney — *"allow per channel LOR on the mono probability for the extra lanes."* The kind of
thing a thoughtful user would ask for, which is a reason to take it seriously and a reason not to rush it.

---

## 1. The idea in one line

The two lanes East lacks (VARIATION, LEGATO) are **mono-only**. Rather than give each poly voice its own
probability data for them, give each voice its own **LOR window** (length / offset / rotation) onto the
*shared mono* probability array:

```
voice v, lane L ∈ {VAR, LEG}, step s:
    p = random_[0][L][ lorIndex( lorStore_[v][L], s ) ]
                ^ mono data          ^ per-voice view
```

No new probability storage. A per-voice **view** of a shared shape — the same trick that makes
length-6-against-length-12 produce multi-bar patterns, applied to articulation rather than pitch.

---

## 2. What already exists (verified, not assumed)

| Thing | State | Where |
|---|---|---|
| `lorStore_[16][6][3]` — 16 voices × 6 editor lanes × (len/off/rot) | **slots `[v][4]`, `[v][5]` allocated, unused** | `SequencerEngine.hpp:135` |
| `random_[16][6][16]` — same shape for probability | allocated; poly VAR/LEG unused | `PatternEngine.hpp:78` |
| *"editorLane 0..5 = MEL,OCT,REST,ACC,VAR,LEG (poly uses only 0..3; VAR/LEG are mono-only)"* | comment states it outright | `SequencerEngine.hpp:127` |
| Each `PolyVoice` owns its **own `GateState`** | yes — independent holds structurally supported | `SequencerEngine.hpp:36` |
| Editor `laneCount` is a plain field; `laneHeightF() = laneAreaH()/laneCount` | 6 lanes works with no editor change | `SandsVisualEditorV4.hpp:263,270` |

The probability-modifier unification (the `lorStore_`/`random_` arc) allocated these slots. This feature
is largely *claiming space the refactor already made*.

## 3. The panel fits exactly

After the `SandsGrid` unification (`src/ui/SandsGrid.hpp`, `LANE_H = 14`):

```
East / Macro lanes end at  70 mm
Mono lanes end at          98 mm
gap                        28 mm  =  2 × LANE_H   exactly
```

East's empty band below its lanes is **precisely two lanes tall**. Adding VAR + LEG makes East a 6-lane
editor on the identical grid to Mono (14..98). The space isn't being *filled*; it was the right size all
along. (Macro is different — its send group occupies that band. Macro would stay 4-lane.)

---

## 4. The two lanes are NOT equally safe

### 4a. CORRECTION — there is only ONE coherent option, and it is coupled to LEGATO

An earlier draft of this note posed a choice: per-voice `nvIdx`, or "variation-bias only". **The second
does not exist.** From source:

```cpp
float r_vary = pe.variationRandom[ getVariationStep() ];   // mono LOR-transformed step
int   nvIdx  = getNoteLenIdx(noteVal, input, r_vary);      // → pe.varyNoteIndex(baseIdx, in, r)
```

`varyNoteIndex(baseIdx, variationAmount, r)`: the **knob** (`in.variationAmount`) sets how WIDE the window
of reachable note-length indices is; the **array value** `r` picks WHICH variant inside it. VARIATION *is*
the note-length bias. There is no separate bias to move without moving `nvIdx`.

So the feature is exactly one thing, and it is a good shape:

> **The window stays global (mono VARIATION knob). The pick within it becomes per-voice.**
> *How much* variation is one musical decision; *which* variant each voice takes is where polyphony lives.
> At `variationAmount == 0.5` `varyNoteIndex` returns `baseIdx` unchanged — so the feature is SILENT until
> the knob is turned up. No toggle needed for backwards compatibility; old patches are bit-identical.

**THE COUPLING (this is the real finding).** `nvIdx` reaches `gs.slideMax(pitchV, semitone, nvIdx)`, which
sets `holdRemain`. Per-voice `nvIdx` ⇒ per-voice hold length. Legato/tie **rides on hold**
(`holdRemain >= 1.f` is the phrase-spanning guard; ties extend holds; `prevSlur` tracks committed slurs).

**Therefore VARIATION and LEGATO cannot be cleanly staged apart.** Doing VARIATION per-voice already
perturbs the quantity LEGATO depends on. The earlier claim "VARIATION is low risk, do it first" was wrong.

### 4b. What the current model actually is (and why it may be deliberate)

| shared from mono | per-voice |
|---|---|
| `nvIdx` (note length) | rest decision |
| `legatoProb` (tie/slur) | accent decision |
| `scale` | pitch draw |

Rest is *subtractive*. Accent is *decorative*. Pitch is *within the shared scale*. **None move the clock.**
`nvIdx` and `legatoProb` are the only two that do, because both feed `holdRemain`. The boundary looks like
a principle, not an oversight:

> Poly voices may subtract, decorate, and choose pitches within the shared key —
> but they may not move the clock or leave the scale.

That is what makes the poly voices read as *one line, thinned and coloured sixteen ways* rather than sixteen
sequencers. Note that the two lanes East lacks are **precisely the two structural ones**. Lifting the
boundary is a MODEL CHANGE, not a feature.

### 4c. RULE 1 (VARIATION) — per-voice articulation, clamped to the mono event grid — **BUILT (Stage 2)**

Each voice derives its own `nvIdx` through its own LOR window, **but `holdRemain` is clamped so a voice can
never hold past the next mono event.** The clock stays mono's. Legato's invariants never see a divergent
hold, because divergent holds cannot occur.

> Implemented as `nvIdxForVoice(bank, input)`: per-voice VAR LOR picks the variant, then
> `max(nv, mono nv)` (NOTE_VALUES is slowest→fastest, so `max` is the *shorter*) clamps to the mono grid.
> Subtractive by construction. On the branch, 2312 assertions passing; silent at `variationAmount == 0.5`.

You hear: attacks tight (all voices land together), releases scattered (each lets go at its own time).
Articulation stratification — sustain / tenuto / staccato across a chord. Gate length is what envelopes
see, so with Changi's 16 individual gate outs this becomes per-voice **timbre**.

Emergent, for free: the clamp is "hold until mono's next note", so **dense passages force every voice short
and sparse passages let voices stretch into the gaps.** Articulation follows rhythmic density with no
programming. That the safety mechanism produces a musical behaviour is a good sign it is the right
constraint.

It cannot do: independent lines, counterpoint, polyrhythm, sounding through a mono rest. It stays *one line,
sixteen articulations*.

**MPE framing (Rodney):** this is exactly per-note expression — one note event, per-voice articulation.
The clamped model is the shape MPE controllers and DAW note-expression already assume.

### 4d. RULE 2 (LEGATO) — per-voice, per-onset leading-edge slur — **DESIGNED, Stage 3**

Rule 1 gives each voice its own *length*. It leaves the LEGATO lane's cells wired to nothing. Rule 2 makes
that lane earn them: each voice runs **its own leading-edge legato, per note**, bounded by mono's chain and
clamped subtractive. This supersedes the earlier "one latched opt-in/opt-out per chain" sketch — per-onset
is cleaner (less latched state, fuller use of the lane, more principled identity), and is the settled design.

**The model.** Rest stays one decision per chain; legato is decided per note, exactly as mono decides its own:

1. **Rest roll (latched).** At each mono gate start (`monoGateStart` — `NewNote`, or `Legato`-from-dead), a
   participating roll: own array, own knob. Rest ⇒ the voice is out for the *whole chain*. One
   `participating` flag, cleared at the next `monoGateStart` or a mono `Rest`.
2. **Legato roll (per note, leading-edge).** At each onset a participating voice plays, it rolls its OWN
   forward-slur commitment — the per-voice analogue of mono's:
   ```
   poly.slurForward = leStarting && noteCanLeadLegato(nvV)
                    && (legatoMax || r_polyLegato < legatoProb)   // voice's own LEG LOR, this tick
   ```
   and consumes it at the next landing: `poly.prevSlur` true ⇒ connect (slide on `Legato` / `extendHold`
   on `Tie`); false ⇒ re-articulate (`triggerNote`, clamped `nvV`).

**The governing predicate — the whole feature in one line:**

> A participating voice **connects** (legato *or* tie) at an edge ⟺ **mono connects there AND the voice's
> own `slurForward` fired.** Otherwise it re-articulates a fresh note clamped ≤ mono's length.

It is an **AND of two independent leading-edge rolls** — mono's and the voice's, both committed at the
*previous* note and consumed at this landing. Poly can't slur where mono didn't (a mono `NewNote` boundary
is a `monoGateStart`, which discards poly's pending `slurForward` and forces a fresh trigger); poly can
decline where mono did (its own roll missed). So `poly slur = mono slur ∩ poly roll ∩ participating`,
subtractive on both counts. You never gate poly's roll on mono's — the AND falls out of consumption plus
the `monoGateStart` reset. (That reset half is *already wired*; only the roll and consume halves are new.)

**Read at the LEADING edge, not the landing — the identity linchpin.** In leading-edge, the slur into N+1
was committed at **N's onset** (mono set `slurForward` from its legato cell at tick `T_N`); the landing at
N+1 only consumes it. So the voice must roll its own `slurForward` at **its own onset** (`T_N`), from *its*
LEG LOR — the same cell mono read for that edge. Roll at the landing (`T_{N+1}`) instead and even an
identical LOR reads a *different* cell, and identity breaks. Rolling at the leading edge, identity falls out
**structurally**: same LEG LOR ⇒ same cell ⇒ same boolean ⇒ the voice slurs exactly where mono slurs. (The
latched sketch got identity by the weaker "opt-in happens to equal mono" coincidence and read the lane only
*once per chain*, discarding 15/16 of its cells. Per-onset uses every cell.)

**Pitch is a separate axis (already fixed in `fix/poly-midnote-pitch-hold`).** The slur decision (connect
vs re-strike) is orthogonal to the pitch source:
- `Legato` edge (mono moved pitch) ⇒ the voice draws its OWN new pitch (its melody/octave LOR).
- `Tie` edge (mono held pitch) ⇒ the voice holds its current pitch.
- `MidNote` (mono holding, no onset) ⇒ hold; never a slur decision, never a re-strike.

Both opt-in and opt-out use this same pitch rule; only connect-vs-re-strike differs. **`Tie` and `Legato`
are one rule** — a `Tie` note leads the next slur exactly as mono's does (its `slurForward` rolls
normally); the sole difference is held vs drawn pitch. (This is why the MidNote-hold fix had to land first:
without it, "follow mono through a held span" was a lie the engine told itself.)

**Invariants, all subtractive — now per-edge:**
- *Never smoother than mono.* The voice can only decline a slur, never invent one. What changed from the
  latched sketch is only that the subtraction is now *granular* — this edge slurred, that edge detached —
  rather than uniform across the phrase.
- *No new event times.* Opt-out re-strikes land only on mono onsets (a per-edge subset). Clock untouched.
- *Triplet-safe by construction.* `noteCanLeadLegato(nvV)` sits in the voice's own roll, so a fractional
  (triplet) clamped length can't lead — it forces `slurForward = false` ⇒ re-articulate. Fractional lengths
  appear only where they re-strike, never where they'd have to reach the next onset off-grid. Same
  guarantee, same mechanism as mono.

**State: one latch, not two.** The earlier sketch needed `participating` **and** `optIn` latched. Per-onset
drops `optIn` entirely: the legato decision is a pure per-tick function (the voice's LEG LOR read at each
onset), carried on `poly.slurForward` for exactly one note (onset→landing), surviving `MidNote` the same
way mono's does. The field **already exists** (`GateState::slurForward`, per voice) and is currently only
ever *cleared* (reset + wrap) — never rolled or consumed. So this wires up dormant infrastructure and
*restores* tick-purity: only `participating` is cached across a chain, and it must be — rest-per-onset would
let a voice go silent mid-chain, resurrecting the retired `restBeatsLegato`-for-poly problem. Legato
per-onset is free because opt-out still *sounds*.

**The one structural change.** The sustain branch is guarded by `gs.gateHeld && wasHeldPoly`. An opt-out
voice's short note has already released, so at the next mono slur onset `wasHeldPoly` is false and it falls
through to `gateHeld = false` — silent for the rest of the chain (staccato-then-*absent*). It needs a
genuine new-onset `triggerNote` **inside a mono slur** — the case `monoGateStart` excludes (mid-chain,
`wasHeldMono` true). New branch:
```
participating && !poly.prevSlur && decision ∈ {Legato, LegatoMax, Tie}
    → triggerNote(own pitch per the Legato/Tie rule, own accent draw, nvIdxForVoice() clamp)
```
And `participating` must be explicit: today a rested voice and an opt-out voice with a closed gate *both*
read `wasHeldPoly == false` and both stay silent; under Rule 2 they diverge (rested ⇒ silent, opt-out ⇒
re-strike), so `wasHeldPoly == false` can no longer stand in for "not participating."

**Additive and gated — no second implementation; follow-mono is the default.** Rule 2 does not replace the
follow-mono path; it *adds* the opt-out re-strike branch behind the existing `perVoiceArticulation` gate
(§4c — Rule 1 rides the same flag). With the gate off, the new branch is never taken and the existing
slide/hold/MidNote dispatch runs unchanged — which *is* follow-mono. So the original behaviour is the
gate-off default, reproduced by the same code, not a parallel path. This mirrors Rule 1 exactly:
`nvIdxForVoice` returns mono's `nvIdx` when the gate is off; Rule 2's per-voice slur likewise resolves to
mono's `slurForward` (equivalently, skips the new branch) when the gate is off. One gated addition per
rule; each degenerates to mono by *not firing*, never by a second implementation.

**The three gates (and one robustness fix owed):**
1. **`perVoiceArticulation`** — the context-menu toggle ("Per-voice articulation (East VARIATION)", widen to
   "…VARIATION/LEGATO" once Rule 2 lands), default **off**. The master choose. Off ⇒ follow-mono; the
   unwritten `LEN=0` store is never read.
2. **East presence** — the per-voice VAR/LEG LOR is sourced only inside `if (eastLOR)`; no East, no data.
   Today the toggle is *independent* of East, so `perVoiceArticulation` on with no East reads the `LEN=0`
   store — pinned to cell 0 by `getStrandIdx`'s `max(1,len)`, which is *not* identity and *not* silent.
   **Build task:** gate as `perVoiceArticulation && eastPresent`, or treat `LEN=0` as "inactive ⇒ follow
   mono," so "silent without East" holds by construction rather than by luck.
3. **Identity windows** — with the toggle on and East feeding, lanes default to `LEN=16`; the voice still
   mirrors mono until a lane is shortened/offset. Per-voice, per-lane engage.

> **Sourcing sub-note (pre-existing, affects Rule 1 too):** VAR/LEG use *straight* East params (default
> `16/0/0`), unlike `REST`'s `combineLOR` (which blends the mono base). So a lane at neutral reads the base
> array *in order*, which equals mono only when mono's own LOR is also at `16/0/0`. If the intent is "a
> neutral poly lane tracks a *dialed* mono," the sourcing must become `combineLOR`-style (default ⇒ mono's
> LOR); if "neutral = base-array reference" is acceptable, leave it. Decide before Stage 3 ships — it sets
> what "silent" means for V2–V15.


## 5. Staging

1. **Panel + editor only — DONE.** East → 6 lanes (`ED_H` 56 → 84, `setLaneCount(6)`), same 14..98 band as
   Mono. Lanes 4/5 render but are **LOCKED / display-only**. (They previously fell through `laneLockedFn`'s
   `editorLane >= 4 → false`, i.e. *editable*, purely because they were never displayed — fixed here.) Zero
   data change, zero behaviour change. Answered the cheap question: does a 6-lane East look right?
1b. **Editable lanes 4/5 + new param banks — DONE (§6c).** Two 15-voice LOR banks
   (`POLY_VARIATION_VOICE_1_LEN…`, `POLY_LEGATO_VOICE_1_LEN…` = 2 × 15 × 3 = 90 params), `lorId` branches,
   East save/load loops widened to 6. Per-voice VAR/LEG LOR now editable, persisted, pushed to the engine;
   V1 mirrors mono.
2. **Rule 1 — clamped per-voice VARIATION (§4c). DONE.** Per-voice `nvIdx` via per-voice VAR LOR, clamped
   to the mono grid (`nvIdxForVoice`). Legato invariants untouched by construction. 2312 assertions
   passing; silent at `variationAmount == 0.5`, so old patches are bit-identical.
3. **Rule 2 — per-voice, per-onset leading-edge legato (§4d).** One latched flag (`participating`) plus a
   per-note `poly.slurForward` roll (the field already exists, currently only cleared) and one new
   `triggerNote` branch in the sustain section for opt-out re-articulation, reusing Rule 1's clamp for the
   length. **Leading-edge only** (`cleanup/remove-reactive-legato`) + **MidNote-hold**
   (`fix/poly-midnote-pitch-hold`) are prerequisites — this branch (`feat/east-rule2-legato`) is cut off
   both. Additive and behind `perVoiceArticulation` (§4d gates), so gate-off degenerates to follow-mono via
   the existing path — no second implementation. This is what makes the LEGATO lane's cells live. Do the
   `&& eastPresent` gate + sourcing decision (§7) first. By ear, on the branch.
4. Merge only once Rule 2 is proven by ear. Stage 3 may land inert-but-visible (LEGATO shown, no re-strike)
   or trimmed (variation only) if it doesn't earn its keep — both are fine outcomes.

Each stage is independently revertible. Stage 1 alone answered "does a 6-lane East look right", the cheap
question; Stages 1b–2 are the built substrate Rule 2 sits on.

---

## 6. Why this might be a genuinely good idea

Rodney's own report: setting LOR lane lengths to 6 and 12 with three poly voices yields multi-bar patterns
that repeat over long cycles. That effect comes from **coprime-ish rotation periods sliding past each
other**, not from randomness. Extending per-voice LOR to VARIATION does the same thing to *articulation*:
voices share one underlying shape but read it at different phases, so note-value and gate length
desynchronise over many bars while remaining musically related.

The strongest argument is economy — one shared probability array, sixteen views. The strongest argument
*against* is that LEGATO's invariants are load-bearing and were not written with this in mind.

---

## 6b. Decisions taken (Rodney)

**These lanes are LOR-ONLY. No spread control, no spread CV, ever.**
Spread distributes a *mono probability across voices*. Here the probability array stays mono **by design**
— only each voice's *reading position* differs. There is no per-voice VAR/LEG probability to spread into.
So VAR/LEG get: length, offset, rotation, and (later) LOR mod. Nothing else. This is why East's spread
control rows correctly stay `N_ROWS = 4` on lanes 0..3.

**Macro does NOT get this feature — and the reason is semantic, not spatial.**
Macro's role is *ownership*: it takes a lane and drives it for all voices. If Macro owned VARIATION, every
voice would receive the same LOR window → the same index → the same note length. **Macro-ownership does not
merely crowd the feature out, it annihilates it**, because per-voice divergence *is* the content. (Lack of
panel space is a true but secondary reason.)
> Parked idea, not vacuous: Macro sending a *common rotation offset* to all voices would slide the whole
> articulation field while preserving relative phase between voices. That is a **send**, not ownership, and
> could be musical. Out of scope for now.

## 6c. Stage 1b needed NEW param banks (two silent traps found and closed) — DONE

Two aliasing hazards were discovered while preparing to unlock the lanes. Both compiled cleanly and returned
plausible wrong values — the project's recurring failure mode:

1. `StraitsEastSandsVisual::lorId(v, lane, c)` fell through to **the OCTAVE bank** for any unrecognised
   lane. Unlocking VAR/LEG would have *corrupted every voice's octave LOR*, not written an unread store.
   → now returns `-1`; live callers loop `l < 4` so it is unreachable, i.e. a tripwire.
2. `SequencerEngine::editorLane(engLane)` masks `& 3` over a 4-entry table. Called with 4/5 it would alias
   **VAR→REST, LEG→MELODY**. It cannot be reached by correct code (`PL_LANES == 4`), and is now documented
   at the boundary. **For per-voice VAR/LEG LOR use the EDITOR-order accessors** `polyLOR` / `polyLORRef`
   (they mask `& 7` and index `lorStore_` directly — already 6-lane safe).

Therefore **stage 1b (making lanes 4/5 editable) is not a small change**: it needs two new 15-voice LOR
param banks (`POLY_VARIATION_VOICE_1_LEN…`, `POLY_LEGATO_VOICE_1_LEN…`) = 2 x 15 x 3 = **90 new params**,
plus `lorId` branches and East save/load loops widened to 6. Do it deliberately; do not bolt it on.

## 7. Open questions

- **`perVoiceArticulation && eastPresent` gate (§4d). Build task, Stage 3.** Today the toggle is
  independent of East; with it on and no East attached, the per-voice LOR reads the `LEN=0` zero-init store
  (pinned to cell 0, not silent). Gate on East presence, or treat `LEN=0` as "follow mono," so
  follow-mono-without-East holds by construction.
- **VAR/LEG sourcing: straight params vs `combineLOR` (§4d sub-note). Decide before Stage 3 ships.** Straight
  `16/0/0` default = base-array reference (mirrors mono only if mono is also at `16/0/0`); `combineLOR`-style
  = a neutral lane tracks a *dialed* mono. Sets what "silent at neutral" means for V2–V15. Affects Rule 1 too.
- ~~Rule 2 Tie semantics — does opt-out re-articulate at a mono `Tie`?~~ **Resolved: yes, tie = legato.**
  A `Tie` note leads its own slur exactly as mono's does; opt-out re-strikes, opt-in holds; the only Tie/Legato
  difference is held vs drawn pitch (§4d).
- Does per-voice legato interact with the parked `StrandLedger CONFLICT MACRO then EAST` bug?
- ~~VARIATION: per-voice `nvIdx`, or variation-bias only?~~ **Resolved:** per-voice `nvIdx`, clamped
  (§4c, Rule 1, built). "Bias-only" does not exist — VARIATION *is* the note-length bias.
- ~~Should Macro also gain the lanes?~~ **Resolved: no** (§6b — Macro-ownership annihilates per-voice
  divergence, which *is* the content).
- ~~Persistence of `lorStore_[v][4..5]`.~~ **Resolved:** Stage 1b added the 90 explicit param banks and
  widened East save/load to 6 lanes (§6c).
