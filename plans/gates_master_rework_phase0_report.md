# Phase 0 recon — code vs settled docs (feat/subgate-poly-width)

Recon only; no code changed. Authority: the SETTLED sections of
[`docs/design/LEGATO_GATE_GAP_BUG.md`](../docs/design/LEGATO_GATE_GAP_BUG.md:1) (CONTEXT-MENU
LAYOUT §412, RESOLVED §290, FINAL MODEL §161) and
[`docs/design/GATE_SUBDIVISION_STEP_GATE.md`](../docs/design/GATE_SUBDIVISION_STEP_GATE.md:1)
(TWO inputs no-normalling §271, straddle §347, coincidence §437, ghost-consumes-gap-slur §458,
correlated placement §398, mode-agnostic invariant §421, playhead §379). Where code and doc
conflict, the doc wins on intent (per the master plan). Disagreements + doc-vs-doc/plan conflicts
flagged for Rodney below.

## A. Legato context menu — code vs the SETTLED 3-toggle, two-section layout

**Settled (LEGATO_GATE_GAP_BUG.md §412):** two sections.
- *Legato behaviour mode (all modes):* **"Generated rest beats legato"** — default TRUE.
- *Gate-mode legato behaviour (gate mode only):*
  - **"Tie across rests"** — default TRUE. TRUE = BRIDGE (incoming-rest does NOT beat legato).
    Polarity-flipped rename of the old "incoming rest beats legato" FALSE mode.
  - **"Advance playhead on tie-into-rest"** — default FALSE. The FALSE-refinement checkpoint
    (advance one step on the falling edge into an incoming-rest, re-draw legato there). The SOLE
    violation of edge-driven "advance on onset only"; opt-in.
- DROPPED: "tie across abutting gates" (abutting/overlap always ties; not a toggle).

**Code ([`MonsoonWidget.cpp:1042`](../src/MonsoonWidget.cpp:1042)):** one flat section, two toggles:
- "Generated rest beats legato" → `generatedRestBeatsLegato` (default TRUE). ✅ matches (rename done;
  JSON key kept "restBeatsLegato" for save-compat — deliberate, consistent with the doc's
  "keep the old key or migrate" option).
- "Incoming rest beats legato" → `incomingRestBeatsLegato` (default FALSE). ❌ **MISMATCH**: this is
  the pre-settlement naming + the INVERSE polarity. Settled wants **"Tie across rests"** (default
  TRUE) where TRUE=bridge. `incomingRestBeatsLegato=FALSE` is *behaviourally* bridge (matches
  "Tie across rests=TRUE"), but the name and polarity are wrong, and the default reads opposite.
- ❌ **MISSING**: "Advance playhead on tie-into-rest" (default FALSE). Not implemented at all
  (the PROPOSED §345 refinement was never built). Phase 1 adds it.

**Net (Phase 1):** rename `incomingRestBeatsLegato` → `tieAcrossRests` (flip polarity: default TRUE,
TRUE=bridge), relabel "Tie across rests", gate-mode-only section; add `advanceOnTieIntoRest`
(default FALSE); split the menu into the two sections. Patch-JSON: keep "incomingRestBeatsLegato"
key for save-compat OR migrate (say which at build — lean migrate, pre-release).

## B. ms-timer + slurForward bridge — removal status (RESOLVED §290)

**Confirmed removed.** No `gate1LowSamples` / `gate1Adjacent` / `LEGATO_GRACE_S` / ms timer anywhere
(search clean). The `slurForward` bridge is NOT unconditionally on — it is now **conditional on
`incomingRestBeatsLegato`** ([`Monsoon.cpp:916`](../src/Monsoon.cpp:916)):
`gapBridge = incomingRestBeatsLegato ? prevGate1SchmittHigh : slurForward`.
- `incomingRestBeatsLegato=FALSE` (current default) → bridge = `slurForward` (tie across gaps). ✅
  matches settled "Tie across rests=TRUE". **No self-bound / no timer** — a committed slur ties into
  the next gate regardless of gap length; rest is the only brake. ✅ matches RESOLVED.
- `incomingRestBeatsLegato=TRUE` → bridge = `prevGate1SchmittHigh` (1-sample hold after the fall).

### ⚠️ Doc-vs-PLAN conflict on the `<=1-sample` allowance (flag for Rodney)
- **LEGATO_GATE_GAP_BUG.md FINAL MODEL §161 + RESOLVED §290:** TRUE mode = "overlap OR **<=1-sample
  gap** ties; >=2-sample gap fresh." The `<=1-sample` allowance IS in the settled doc (1 sample is
  below every threshold; not a wait).
- **`plans/gates_master_rework.md` Phase-0 bullet (§27):** "strict is overlap-only with **NO
  <=1-sample allowance**."
- **Code:** the TRUE path uses `prevGate1SchmittHigh` → a 1-sample hold → **does** allow the
  `<=1-sample` tie (matches the DOC, conflicts with the PLAN).

The doc and the plan disagree. **RESOLVED (Rodney): the DOC wins — keep the `<=1-sample`
allowance** (TRUE mode: overlap OR <=1-sample gap ties; >=2-sample gap fresh). The plan bullet is
stale; the master plan's Phase-0 §27 "NO <=1-sample allowance" line is corrected to match the doc.

**Why the allowance is necessary, not just "free" (Rodney's signal-continuity point):** a continuous
gate signal cannot go high→low→high without at least ONE low sample between the two highs. So the
closest two DISTINCT onsets a real source can produce are: sample N (high), sample N+1 (low = the
first gate's end), sample N+2 (high again) — a 1-sample dip. Overlap (gate still high at the next
rise) is a single continuous gate = ONE note, not a tie between two onsets. Therefore WITHOUT the
`<=1-sample` allowance, no real source could ever produce a tie between two distinct onsets in
strict mode — strict would be unreachable. The `<=1-sample` allowance is the minimum that makes
strict-mode ties reachable at all (it is the dual-wire abutment case: same onset event, two edges).
So the doc's stance is forced by signal continuity, and the code (keep `prevGate1SchmittHigh`) is
correct. (The coincidence table in GATE_SUBDIVISION §448 also uses a `<=1-sample` "clear of the
boundary" rule, corroborating this.)

## C. Subgate rules — present vs missing (GATE_SUBDIVISION settled)

| Settled rule | Code state |
|---|---|
| **TWO inputs, NO normalling** (§271: SUBGATE_RATCHET + SUBGATE_GHOST, explicit, no inference) | ❌ **VIOLATES.** Code has ONE Gate 3 doing both jobs and **ghost normals to ratchet**: [`Monsoon.cpp:699`](../src/Monsoon.cpp:699) `ghostRise = (!inGate) ? (cachedGate3Connected ? gate3Rise : input.gate2Rise)` and `:700 ghostHigh = cachedGate3Connected ? gate3 : gate2`. This is the **mutual-normal scheme the doc explicitly DROPPED**. Gate 2 = ratchet, Gate 3 = ghost, but ghost falls back to Gate 2 when unpatched. Phase 3 removes the normalling. |
| Envelope classification (in-gate=ratchet, gap=ghost) | ⚠️ PARTIAL. Region-select by `inGate` exists ([`Monsoon.cpp:695-700`](../src/Monsoon.cpp:695)), but it is fused with the normalling. Once normalling is removed the classification is correct. |
| **Straddle clip** (§347: in-gate cell ends at the fall; ghost may tie through the rise via its own slurForward) | ❌ **MISSING.** `executeModeBSubdivided` is region-agnostic ([`SequencerEngine.cpp:794`](../src/dsp/engines/SequencerEngine.cpp:794)); no straddle-clip logic — a ratchet cell is not clipped at the main-gate fall. |
| **Boundary-COINCIDENCE table** (§437: ghost ignored on both main-gate edges, resumes >=1 sample into gap; ratchet included on rise, ignored on fall) | ❌ **MISSING.** No coincidence handling. |
| **Ghost-consumes-gap-slur** (§458: a sounding ghost ties the pending slur in; rested ghost transparent) | ❌ **MISSING.** Not implemented; the FALSE-mode bridge reaches straight for the next main gate. |
| **Correlated ghost placement** (§398: per-voice variation correlated to mono, [-1,+1] via copula; playhead mono) | ❌ **MISSING.** Ghost placement is variation-gated mono only ([`SequencerEngine.cpp:856`](../src/dsp/engines/SequencerEngine.cpp:856)); no per-voice correlated placement. |
| Playhead advances on every ratchet/ghost onset (edge-driven; §379) | ✅ Present. `executeModeBSubdivided` advances on any of the 3 edges ([`SequencerEngine.cpp:805`](../src/dsp/engines/SequencerEngine.cpp:805)). The ONE settled exception (advance-on-tie-into-rest) is the missing Phase-1 toggle. |
| **Mode-agnostic invariant** (§421: gate code is ONE path; q-mix only switches pitch source) | ⚠️ UNVERIFIED but likely OK — Mode B and Mode D both route through `executeModeB`/`executeModeBSubdivided`; q-mix is `quantiserPitchSource` inside `voicePitch`. No "if quantiser mode" seen in the gate path. Phase 2 adds the bit-identical test to lock it. |

**Net (Phases 3-5):** subgate is the bulk of the remaining work — remove normalling (Phase 3), add
straddle+coincidence+ghost-consumes-gap-slur (Phase 4), correlated placement (Phase 5). The current
subgate code is a provisional region-select scaffold, not the settled model.

## D. perVoiceArticulation — present, redundant (flag, do not remove now)

**Confirmed present and working** ([`SequencerEngine.hpp:341`](../src/dsp/engines/SequencerEngine.hpp:341),
default OFF, doubly-inert). It is the OLD binary per-voice VAR/LEG option. Per the master plan
(§51) and GATE_SUBDIVISION §398, the **Gaussian-copula correlation matrix supersedes it** (graded
per-voice variation correlated to mono, [-1,+1], replaces binary on/off). It is slated for REMOVAL
at the Sands consolidation, NOT now. Flag: redundant; do not build on it or extend it; confirm it
stays green. (Phase 5's correlated ghost placement uses the copula machinery, NOT this flag.)

## E. Reverse-safety

The `<=1-sample` decision is edge-timed (`prevGate1SchmittHigh`, reset while HIGH) →
direction-agnostic. The forward/reverse bitwise test in `test_mode_b_gap.cpp` is green. The
missing subgate rules (straddle/coincidence/placement) must be built reverse-safe from the start
(edge-timed, not index-timed) — flagged for Phases 3-5.

## Summary — what Phases 1-5 must do (hinged on this recon)

1. **Legato menu → settled 3-toggle, two-section layout.** Rename `incomingRestBeatsLegato` →
   `tieAcrossRests` (flip polarity, default TRUE=bridge); add `advanceOnTieIntoRest` (default FALSE,
   the sole edge-driven-playhead violation, opt-in). Migrate or keep the JSON key (say which).
   **Confirm the `<=1-sample` stance with Rodney (doc vs plan conflict, §B above) before building
   the TRUE path.**
2. **Mode-agnostic invariant + test.** Add a bit-identical gate/rest/legato/accent test across the
   q-mix range (locks the §421 invariant).
3. **Subgate inputs — remove normalling.** Two explicit inputs (SUBGATE_RATCHET + SUBGATE_GHOST),
   no Gate-3→Gate-2 fallback. Signal types: ghost=gate→sustained/trigger→blip; ratchet=trigger-or-gate onset-only.
4. **Subgate boundary rules.** Straddle clip + coincidence table + ghost-consumes-gap-slur (all
   missing; §C).
5. **Correlated ghost placement.** Per-voice variation correlated to mono via the copula; playhead
   mono; rest/legato/accent shape survivors per voice (§398).

Reverse-safe throughout; keep the forward/reverse bitwise test green.

## F. Doc-vs-doc note (resolved in Phase 3): ghost trigger-advance (§286 vs §379)
§286 ("Subgate signal types") says a trigger-ghost should "not advance the playhead"; §379
("PLAYHEAD ADVANCE for ghosts", headed *settled*) says "a ghost onset ALWAYS advances the
playhead, regardless of length... this dissolves the 'how do we not advance on a trigger'
question — we DO advance on it." §379 explicitly supersedes §286 on the advance question, so
**§379 wins: every ghost onset (trigger or gate) advances the playhead** (it must, to draw the
step's rest/legato/accent/pitch data). The signal-type rule that SURVIVES from §286 is the LENGTH
rule: gate-ghost = sustained (gate width), trigger-ghost = short blip (the trigger's own width).
No classification is needed — the ghost note's length already follows the ghost gate's high time
(a short gate IS a blip; a long gate IS sustained), which the existing `ghostHigh → ghostSounding`
in IMPL 2b already implements. Phase 3's only code change was removing the Gate3→Gate2 normalling
(§271); the signal-type behaviour was already correct.
