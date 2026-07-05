# Legato / Tie model — leading-edge decision + collapse (design note, FOR LATER)

Status: **design capture, not implemented.** Inspired by building Lantern (which made the
current timing visible) and checking the real Vermona meloDICER. This is a CORE ENGINE
behaviour change — it will alter how existing patterns sound — so it's captured for a
deliberate decision, not changed yet.

---

## Current model (Monsoon today, SequencerEngine::executeStep ~line 273)

Legato/tie is decided at the **step that continues** (the join), re-rolled every step:

    else if (r_legato_tie < legatoProb) {
        if (sem == lastSemitone && (wasHeld || hadTail)) { extendHold(); decision = Tie; }
        else                                             { slideNote();  decision = Legato; }
    }

So the FIRST note doesn't know it will be slurred; the NEXT step rolls the dice and, if it
wins, retroactively makes the join a tie (same pitch) or legato (different pitch). This is a
TRAILING-edge / reactive model.

Consumers of the Tie vs Legato distinction (audited):
- MonsoonOutputGenerator.cpp:94-96 — SEPARATE tie-gate and legato-gate OUTPUT JACKS. This is
  the only functional consumer.
- Poly voices (SequencerEngine.cpp:441) always slideNote() regardless — poly does NOT use
  the tie/legato label. Poly consistency needs only "gate held vs retriggered".
- Everything else (extendHold vs slideNote) is just mechanism (hold-same-pitch vs
  slide-to-new-pitch), determined by PITCH, not a separate decision.

---

## Proposed model (leading-edge + collapse)

### A. Leading-edge legato (matches the meloDICER hardware)
Legato is decided **up front at the note's onset** — a note commits to holding its gate open
past its natural end (the trailing edge slurs into whatever follows). The first note KNOWS it
is trying to legato. Not re-rolled at the join.

- **Legato eligibility:** only notes of length >= 1 step can INITIATE legato. Triplets and
  1/32 (fractional-length) notes end before the next step, leaving a gap the held gate can't
  bridge, so they are ineligible to start a legato.
- The note sets a "will hold gate" intent at onset; the gate stays high at its trailing edge.

### B. Collapse tie vs legato into ONE internal "held/slurred" state
Since the decision is just "hold my gate open", tie-vs-legato is not a separate decision —
it's DESCRIPTIVE, resolved by pitch at the join:
- next note SAME pitch  → reads as a **tie**
- next note DIFFERENT pitch → reads as a **legato slide**
- We only KNOW which when the next note's pitch is drawn ("strict legato vs tie known if/when
  the note changes").

Internally the engine makes one decision (hold gate). The tie/legato LABEL is DERIVED
post-hoc (did the pitch change?) purely for:
  - the two output jacks (keep them: tie-gate fires when held+same-pitch, legato-gate when
    held+different-pitch — a cheap derived read, not a decision), and
  - Lantern's colour (one "held" colour; tie-vs-legato emerges from whether the pitch bar
    changes height at the join — truthful, no engine label needed).
Poly voices just mirror "held" and draw their own pitch (already do — no change).

### C. Rest after the first note cancels legato
If a note commits to holding its gate but the NEXT step is a rest, there is nothing to slur
into. OPEN QUESTION: does the gate close at the rest (legato intent cancelled), or does the
legato-intent suppress the rest? Current guess: a following rest cancels the legato (gate
closes). Needs deciding.

---

## Why this is better
- Matches the hardware's actual gate behaviour (gate physically stays high across the
  boundary — a decision the sounding note makes about its own release, before it knows the
  next note).
- Simpler engine: one "hold" decision instead of a tie/legato branch.
- Simpler poly: mirror "held", no label to propagate (already the case).
- Simpler + more truthful Lantern: one held/slurred colour; pitch tells tie vs legato.

## Migration implications (why it's captured, not done)
- CORE ENGINE change: existing patterns will sound different (legato placed at onset, not
  reactively at joins; triplets/1/32 can no longer start legato).
- Keep the tie-gate / legato-gate output jacks working via the derived label so patches
  using them don't break silently.
- Accent already decided at onset + carried through (result.accented inheritance) — consistent
  with a leading-edge model, so accent needs no change.
- Confirm against the meloDICER manual / careful listening before committing (user "pretty
  sure" but not certain).

## Open questions
1. Rest-follows-legato: cancel (close gate) vs suppress the rest? (C above)
2. Keep two separate output jacks (derived label) or collapse to one "held" gate output too?
3. LegatoMax (legatoProb >= 0.999) — how does it fit the leading-edge model? (currently a
   separate always-slide path)
4. Does legato eligibility (>=1 step) interact with the note-value probability draw, or is it
   a hard gate applied after the length is known?
