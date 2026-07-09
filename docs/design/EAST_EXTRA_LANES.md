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

### 4a. VARIATION — moderate risk, do first

`PolyVoice` docstring: *"Owns its own GateState and rest probability; everything else (**nvIdx**,
legatoProb, scale) is shared from mono."*

**Open question that must be settled before coding:** `nvIdx` (note-value index) is passed into
`executeStep(...)` and reaches `gs.slideMax(pitchV, sem, nvIdx)` — i.e. it feeds **gate length**, not just
note choice. So "per-voice variation" is not cosmetic: voices would acquire independent gate lengths.

Decide explicitly which is meant:
1. **Per-voice note-value** — each voice reads the mono VARIATION array through its own LOR and derives
   its own `nvIdx`. Voices diverge in note value *and* gate length. Musically strong; touches gate timing.
2. **Per-voice variation-bias only** — the LOR window shifts which variation *probability* the voice sees,
   but `nvIdx` stays mono-derived. Safer; possibly a weaker effect.

Do not implement until (1) vs (2) is chosen. Guessing here would produce a plausible-looking feature with
the wrong musical meaning.

### 4b. LEGATO — high risk, gate behind a flag, do second

Poly currently **follows mono's legato decision**. From `SequencerEngine.cpp:522-529`:

> *Tie — mono held the same pitch; poly voices that are sounding extend their own hold. Legato /
> LegatoMax / NewNote — mono played something new; each poly voice independently rolls its own rest
> probability then draws its own pitch. **Gate type (retrigger vs slide) follows mono's decision.***

Per-voice legato means `lastStepResult.decision` **no longer drives poly gate type**. The following
invariants were all written assuming legato is shared, and each must be re-examined:

- `triggerStepEventForOffsetStep_` guards on `holdRemain >= 1.f` before firing a new note decision
  (`SequencerEngine.cpp:243`) — the multi-step phrase-spanning "secret sauce".
- `legatoLeadingEdge` (default **on**): the slur is governed by the *previous* note's onset
  (`SequencerEngine.hpp:84`).
- `restBeatsLegato` (default **on**): a committed slur (`prevSlur`) reaching N+1 that rolls a rest —
  rest wins, cancelling the slur (`SequencerEngine.hpp:90-94`).
- Poly `wasHeldPoly` / `hadPolyTail` are derived per voice but the *decision* is mono's.

Each `PolyVoice` already owning a `GateState` says independent holds are *feasible*. It does not say the
above invariants still hold when voices slur independently. **This is a behaviour change in the most
delicate part of the engine, not a wiring job.**

---

## 5. Staging

1. **Panel + editor only.** East → 6 lanes (`ED_H` 56 → 84, `laneCount` 4 → 6) on the shared grid. Lanes
   4/5 render, and their per-voice LOR is editable and persisted, but **nothing reads them**. Zero
   behaviour change. Ship-able, and it proves the UI.
2. **VARIATION**, after (1) vs (2) above is decided. Guard with a context-menu toggle, default **off**.
3. **LEGATO**, behind its own toggle, default **off**. Only after 2 is musically confirmed.
4. Merge to master only once 2 (and optionally 3) are proven by ear.

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

## 7. Open questions

- VARIATION: per-voice `nvIdx` (and therefore gate length), or variation-bias only? **Blocking.**
- Should Macro also gain the lanes? Its send group occupies the band; probably not.
- Does per-voice legato interact with the parked `StrandLedger CONFLICT MACRO then EAST` bug?
- Persistence: `lorStore_[v][4..5]` presumably already serialises (whole-array). Confirm; patch breaks
  don't matter (no users), but silent zeros would.
