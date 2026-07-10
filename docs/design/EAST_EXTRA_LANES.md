# East: per-voice LOR over the mono VARIATION / LEGATO probability

**Status: DESIGN ONLY. Nothing implemented.** Dev lives on `integration/east-extra-lanes` until the
idea is proven good. Do not merge to master on the strength of this document.

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

### 4c. THE MIDDLE GROUND — per-voice articulation, clamped to the mono event grid

Each voice derives its own `nvIdx` through its own LOR window, **but `holdRemain` is clamped so a voice can
never hold past the next mono event.** The clock stays mono's. Legato's invariants never see a divergent
hold, because divergent holds cannot occur.

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

## 5. Staging

1. **Panel + editor only — DONE (this branch).** East → 6 lanes (`ED_H` 56 → 84, `setLaneCount(6)`), same
   14..98 band as Mono. Lanes 4/5 render but are **LOCKED / display-only**. (They previously fell through
   `laneLockedFn`'s `editorLane >= 4 → false`, i.e. *editable*, purely because they were never displayed —
   fixed here.) Zero data change, zero behaviour change. Answers the cheap question: does a 6-lane East
   look right?
2. **Clamped per-voice articulation (§4c).** Per-voice `nvIdx` via per-voice LOR, `holdRemain` clamped to
   the next mono event. Legato invariants untouched by construction. Default off; silent anyway at
   `variationAmount == 0.5`.
3. **Unclamped** — only if 2 sounds *nearly* right and you want voices to breathe across the bar. This is
   the model change. Re-derive `holdRemain >= 1.f`, `legatoLeadingEdge`, `restBeatsLegato`/`prevSlur`
   per voice. By ear, on this branch.
4. Merge only once 2 is proven by ear. Stage 3 may never be wanted, and that is a fine outcome.

Each stage is independently revertible. Stage 1 alone answers "does a 6-lane East look right", which is
the cheap question.

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

## 6c. Stage 1b will need NEW param banks (two silent traps found and closed)

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

- VARIATION: per-voice `nvIdx` (and therefore gate length), or variation-bias only? **Blocking.**
- Should Macro also gain the lanes? Its send group occupies the band; probably not.
- Does per-voice legato interact with the parked `StrandLedger CONFLICT MACRO then EAST` bug?
- Persistence: `lorStore_[v][4..5]` presumably already serialises (whole-array). Confirm; patch breaks
  don't matter (no users), but silent zeros would.
