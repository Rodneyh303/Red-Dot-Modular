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

### 4d. RULE 2 (LEGATO) — per-voice opt-in / opt-out — **DESIGNED, Stage 3**

Rule 1 gives each voice its own *length*. It leaves the LEGATO lane's 45 params wired to nothing. Rule 2
is what makes that lane earn them: each voice decides, per mono chain, whether to **slur with mono or
re-articulate against it**.

**The model.** At each mono gate start (`monoGateStart` — `NewNote`, or `Legato`-from-dead — i.e. the
chain start):

1. **Rest roll** — own array, own knob, as today. Rest ⇒ the voice is out for the *whole chain*.
2. **If playing and mono committed a slur** (`gs.slurForward` true at this onset): **legato roll** — mono's
   legato array read through the voice's own LEG LOR, against the *global* legato knob. One decision,
   latched for the whole chain.
   - **Opt in** ⇒ exactly today's behaviour: slide through, mono's chain extent, the voice's own re-drawn
     pitch.
   - **Opt out** ⇒ at this onset and every subsequent *connected* onset in the chain, **re-articulate**
     (`triggerNote`) with the voice's own length ≤ mono's (Rule 1's clamp). Fractional lengths allowed.

**Why identity falls out (the property Rodney asked for).** Same LEG LOR ⇒ same legato cell ⇒ same
comparison to the same global knob ⇒ the same boolean mono's own leading-edge roll produced. Mono slurred,
so that roll was *yes*, so the voice's roll is *yes*, so it opts in — and opt-in is master behaviour. Same
VAR LOR ⇒ Rule 1's `max(nv, nv)` no-op. So **identical LOR (all lanes) ⇒ bit-identical playback**, with no
special-casing. It falls out of the construction rather than being bolted on.

**Two invariants, both subtractive:**
- *A voice can never be smoother than mono.* It can only decline a slur, never invent one — poly slides
  only where mono slides AND the voice opts in. With Rule 1's clamp, the whole feature is purely *shorter
  and more detached*. Nothing else.
- *No new event times.* Opt-out re-strikes only at onsets mono already defines (`Legato`/`Tie` steps).
  Poly's gate strikes may exceed mono's (mono glides once where poly strikes at each onset), but the step
  clock is untouched. Decoration, not clock movement.

**Leading-edge removes the only wobble.** The single roll at the chain start is well-defined *because*
leading-edge commits mono's slur at the chain-start onset: at note 1, `gs.slurForward` is already set
(`executeStep` runs before `executePolyVoices`), so the voice can decide "join this chain?" with full
information. The old reactive path decided the slur at note 2's onset — *inside* the chain — which forced a
latch-vs-per-transition choice. That path is **gone** (`cleanup/remove-reactive-legato`), so there is
exactly one semantics: roll once at the start, latch, clear at chain end.

**restBeatsLegato is now mono-only, permanently.** Rest is decided once per chain; a participating voice
never re-rolls rest mid-chain; so a committed slur can never meet a rest *inside* a voice. The toggle stays
a mono concern and never needs a per-voice form. (This was a blocker; it is retired.)

**The one structural change.** Today the sustain branch is guarded by `gs.gateHeld && wasHeldPoly`. An
opt-out voice's short note has already released, so at the next mono slur onset `wasHeldPoly` is false and
it falls through to `gateHeld = false` — silent for the rest of the chain (staccato-then-*absent*, not
staccato). It needs a genuine new-onset `triggerNote` **inside a mono slur** — precisely the case
`monoGateStart` excludes (mid-chain, `wasHeldMono` is true). New branch in the sustain section:

```
participating && !optIn && decision ∈ {Legato, LegatoMax, Tie}
    → triggerNote(own pitch draw, own accent draw, nvIdxForVoice() clamp)
```

**Participation must become explicit latched state.** Today a rested voice and an opt-out voice with a
closed gate *both* read `wasHeldPoly == false`, and that coincidence is load-bearing — both stay silent.
Under Rule 2 they must diverge: rested ⇒ stay silent; opt-out ⇒ re-articulate. So `wasHeldPoly == false`
can no longer stand in for "not participating." Two latched per-voice flags, set at `monoGateStart`,
cleared at the next `monoGateStart` or a mono `Rest`:
- `participating` — the rest roll's result (playing vs out, whole chain).
- `optIn` — the legato roll's result (slide vs re-articulate, whole chain; only meaningful when mono slurs).

This is the second piece of per-voice state (after `laneTick_`) that this branch makes *persistent* rather
than a pure function of the tick. Worth stating loudly, given how much of the engine's correctness rests on
tick-purity: both flags are *derivable* from the mono decision stream, but caching them across a chain is
the price of one-roll-per-chain.

**Decision still owed — Tie (see §7).** Does opt-out re-articulate at a mono `Tie` (same pitch, extended)?
The `MonoDecision` enum today documents *Tie ⇒ poly holds its own notes*. Re-striking at a Tie is
consistent ("re-articulate at every connected onset") and musical (repeated-note articulation under mono's
tied note), but it *changes* that Tie semantics for opt-out voices. Recommend **yes, re-articulate at
Tie** — the honest reading of "more detached" — and update the enum comment to *Tie ⇒ opt-in holds,
opt-out re-strikes*. `MidNote` is never a re-articulation point (no new mono onset; the voice just ticks).

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
3. **Rule 2 — per-voice LEGATO opt-in / opt-out (§4d).** Two latched per-voice flags (`participating`,
   `optIn`), one new `triggerNote` branch in the sustain section for opt-out re-articulation, reusing
   Rule 1's clamp for the opt-out length. **Leading-edge only** — the reactive path is removed
   (`cleanup/remove-reactive-legato`), so there is a single semantics: no latch-vs-per-transition choice.
   This is what makes the LEGATO lane's 45 params live. Settle the Tie decision (§7) first. By ear, on the
   branch.
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

- **Rule 2 Tie semantics (§4d). Blocking Stage 3.** Does an opt-out voice re-articulate at a mono `Tie`
  (same pitch, extended)? Recommend yes; requires updating the `MonoDecision` enum comment to
  *Tie ⇒ opt-in holds, opt-out re-strikes*. Nothing else in Stage 3 is undecided.
- Does per-voice legato interact with the parked `StrandLedger CONFLICT MACRO then EAST` bug?
- ~~VARIATION: per-voice `nvIdx`, or variation-bias only?~~ **Resolved:** per-voice `nvIdx`, clamped
  (§4c, Rule 1, built). "Bias-only" does not exist — VARIATION *is* the note-length bias.
- ~~Should Macro also gain the lanes?~~ **Resolved: no** (§6b — Macro-ownership annihilates per-voice
  divergence, which *is* the content).
- ~~Persistence of `lorStore_[v][4..5]`.~~ **Resolved:** Stage 1b added the 90 explicit param banks and
  widened East save/load to 6 lanes (§6c).
