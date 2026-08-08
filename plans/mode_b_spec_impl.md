# Mode B (Seq + Gate) — implementation plan (per MODE_B_SPEC.md)

> Source spec: [`MODE_B_SPEC.md`](../plugins/Melodicer/docs/design/MODE_B_SPEC.md:1) — all design
> decisions RESOLVED by Rodney (MODEL 1, state-level fix). Debug history:
> [`MODE_B_GATE_REGRESSION.md`](../plugins/Melodicer/docs/design/MODE_B_GATE_REGRESSION.md:1).
> This plan supersedes the earlier GATE_OUTPUT-override approach (which the spec §5 says to remove).

---

## 0. Housekeeping carried in from the interrupted PHILOX task
The Philox key-derivation task (six code changes across `PhiloxRng.hpp`, `PatternEngine.hpp`,
`Monsoon.cpp`, `MonsoonModeController.cpp`, `MonsoonChangeAlleyV2.hpp`) is **code-complete** but its
FINAL build was never confirmed (the last `make` was interrupted by this redirect; the incomplete-type
error was fixed by adding `#include "MonsoonChangeAlleyV2.hpp"` to `Monsoon.cpp`, but that fix is
unbuilt). **Build once before starting Mode B** so a Philox compile error doesn't masquerade as a
Mode B problem. See [`philox_key_derivation_and_ca_seed.md`](philox_key_derivation_and_ca_seed.md:1).

---

## 1. The two reported symptoms and their ONE root cause

Rodney's report: (a) Mode B "never reviewed after Mode A work to update legato approach"; (b) the
Lantern doesn't show what Monsoon generates in Mode B.

Both are the SAME root cause: **Mode B still drives the gate from the internal note-length state
machine (`gs`), not from Gate 1.** The controller passes a fixed `noteVal=2.f` (1/4 note)
([`MonsoonModeController.cpp:182`](../plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:182)),
so on each `gate1Rise`, `executeStep` arms `gs.holdRemain≈4` steps. Consequences:

- **`executeStep` MidNote early-return** ([`SequencerEngine.cpp:426`](../plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:426)):
  `if (gs.holdRemain >= 1.f || gs.gatePulseRemain > 0) return MidNote;`. When gates arrive faster than
  a 1/4 note, the in-between rises are swallowed as MidNote — rest/legato/accent never re-roll.
- **Gate output** = `gs.process()` ([`MonsoonOutputGenerator.cpp:158`](../plugins/Melodicer/src/dsp/managers/MonsoonOutputGenerator.cpp:158))
  → the 1/4-note hold → "every note a long held note; REST/LEGATO have no effect".
- **Lantern** reads `eng.gs.gateHeld / holdRemain / gatePulseRemain / lastNoteType`
  ([`Lantern.cpp:42-45`](../plugins/Melodicer/src/Lantern.cpp:42), :347, :362, :430) — the SAME
  internal-length state → it shows the internal 1/4-note machine, not the Gate-1-driven reality.

So fixing the STATE (make `gs` follow Gate 1) fixes output AND Lantern together — this is exactly the
spec §5 "one source of truth" mandate.

---

## 2. The spec, distilled to invariants

- **§3 length:** note duration = Gate 1's high width. Internal Note Length / Variation FULLY nullified.
  `gs.holdRemain`/`gateHeld` must mean "gate open while Gate 1 high", not an nvIdx countdown.
- **§4 legato (MODEL 1):** decision UNCHANGED from Mode A — `gs.slurForward` set at each note's onset
  by the same `r_legato_tie < legatoProb` leading-edge roll. Only the DURATION model changes: at Gate 1
  FALL, if `slurForward` is set, DON'T drop the gate — bridge HIGH to the next Gate 1 RISE. Chains of
  3+ fall out naturally; break at the first note that doesn't commit or at a REST.
- **§4(b) REST wins:** a REST step forces gate LOW even if the previous note wanted to slur into it.
- **§5 one source of truth:** drive the gate STATE from Gate 1; REMOVE the separate GATE_OUTPUT
  override; every read-path (GATE_OUTPUT, STEP, poly, CV, Lantern) reads that one state.

---

## 3. Design — where the state gets driven from Gate 1

Key structural fact: today `executeModeB` only runs on a RISE
([`Monsoon.cpp:604`](../plugins/Melodicer/src/Monsoon.cpp:604) gates `shouldExecute` on
`gate1Rise`). **The engine never sees the gate FALL.** MODEL 1 needs the fall (to decide bridge-vs-drop)
and the continuous level (gate high only while Gate 1 high). So Mode B needs a **per-sample gate-level
follow**, not just per-rise stepping.

### Proposed approach: a Mode-B gate driver at the STATE layer

Introduce an explicit Mode B gate-state update that runs EVERY sample in Mode B (not only on rises),
sourced from `gate1High = input.gate1 >= 1.0f`. It sets `gs.gateHeld` (and the STEP mirror `gsStep`)
directly from Gate 1, with the legato bridge as the only modifier:

```
per sample in Mode B (modeSelect==1), after mc.executeMode():
    gate1High = input.gate1 >= 1.0f
    if lastStepResult.decision == Rest:
        gs.gateHeld = false                       // §4(b) rest wins
    else if gate1High:
        gs.gateHeld = true                        // note sounds while Gate 1 high
    else: // Gate 1 low (the gap)
        gs.gateHeld = gs.slurForward              // MODEL 1: bridge iff this note slurs forward
    // internal length nullified: do NOT let holdRemain/gatePulseRemain govern the gate in Mode B
```

- On a RISE, `executeModeB` still runs the decision (rest/legato/accent/pitch) exactly as Mode A —
  UNCHANGED. It sets `slurForward` at onset via the existing leading-edge roll.
- The per-sample driver then owns `gateHeld` from Gate 1 + `slurForward`, so length is Gate-1-width and
  legato bridges the gap. `holdRemain`/`gatePulseRemain` are no longer the gate authority in Mode B.

### The MidNote early-return (§3 "nullify internal length")
With `noteVal` no longer arming a multi-step hold in Mode B, the MidNote swallow must not eat rises.
Two implementation options — DECISION NEEDED, see §5:
- **(A) Don't arm the hold in Mode B**: make Mode B pass a length that arms exactly one step (or skip
  `gs.tick()/armGate` for Mode B) so `holdRemain < 1` at the next rise and `executeStep` runs fresh
  every rise. Minimal touch to `executeStep`.
- **(B) Bypass the MidNote guard for Mode B**: pass a `modeB` flag into `executeStep` so the
  `holdRemain>=1` early-return is skipped. More explicit but touches the shared hot path.

Lean: **(A)** — keep `executeStep` mode-agnostic; make Mode B simply not arm a multi-step internal
length. That is the literal reading of §3 ("Mode B bypasses the length countdown").

### Lantern length rendering (symptom b, second half)
The Lantern derives cell length from `gs.gatePulseRemain / p16` else `gs_noteSteps(nvIdx)`
([`Lantern.cpp:380-385`](../plugins/Melodicer/src/Lantern.cpp:380)). In Mode B, note length is the
Gate-1 width, which is not known in advance and isn't an nvIdx. Once `gateHeld` follows Gate 1, the
Lantern's SOUNDING test (`gs.gateHeld || holdRemain>0`, [:347](../plugins/Melodicer/src/Lantern.cpp:347))
will correctly show a note present per gate and its slur underline via `slurMember`. Exact bar-LENGTH
rendering for a live external gate is a display nicety, not the reported bug — the reported bug is
"doesn't show what's generated", which the correct `gateHeld`/`lastNoteType`/`slurMember` fixes.
**Assess in Rack after IMPL 2; only refine length rendering if it reads wrong.**

---

## 4. Change list (ordered)

- **IMPL 1 — remove the override (spec §5).** Delete the `if (modeSelect == 1) { … GATE_OUTPUT … }`
  block at [`Monsoon.cpp:697-744`](../plugins/Melodicer/src/Monsoon.cpp:697) (the whole restored
  slaving block + its comment). The state-level fix replaces it.
- **IMPL 2 — nullify length (Decision 1) + Gate-1-follow output with MODEL 1 bridge (Decision 2).**
  - **Decision 1:** in `executeModeB`
    ([`SequencerEngine.cpp:682`](../plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:682)) change
    `getNoteLenIdx(noteVal, …)` → `getNoteLenIdx(1.f, …)`. Arms a 1-step hold; `executeStep`/`triggerNote`
    stay mode-agnostic. Keeps the decision path (rest/legato/accent/pitch/`slurForward`) UNCHANGED.
  - **Decision 2:** at the GATE_OUTPUT write, branch Mode B to follow Gate 1 (+ `modeBBridging`), else
    `gs.process()`. Maintain `modeBBridging` from Gate 1's fall edge (set when
    `prevGate1High && !gate1High && gs.slurForward && gs.gateHeld`; cleared on `gate1Rise`).
  - **Where the output write lives:** GATE_OUTPUT is currently written by
    [`OutputGenerator::generateOutputs`](../plugins/Melodicer/src/dsp/managers/MonsoonOutputGenerator.cpp:158)
    (`gs.process()`), delegated from [`drive()`](../plugins/Melodicer/src/Monsoon.cpp:695) — NOT directly
    in `Monsoon::process`. So Decision 2's branch must be threaded into `generateOutputs` (pass
    `isModeB` + `gate1High` + `modeBBridging` in), OR applied as a post-`drive()` write in
    `Monsoon::process` sourced from the same state. Prefer threading into `generateOutputs` so STEP/poly/
    CV read the same decision (spec §5 "one source of truth"); confirm the exact seam during impl.
  - STEP mirror (`gsStep`) and poly: confirm they also follow Gate 1 width in Mode B (or are documented
    as riding the same 1-step arm) so no read-path diverges.
- **IMPL 3 — verify Lantern + length.** Confirm in Rack the Lantern now shows the generated notes
  (present per gate, rests as holes, slur underline for bridged notes). Refine length rendering only if
  it reads wrong.
- **IMPL 4 — Section 6 engine test.** `test/` container test (no Rack): one step per rise; `holdRemain`
  never spans multiple gates regardless of `noteVal`; REST→gate low; MODEL 1 legato bridge; rest wins;
  accent flag; **CRITICAL invariant: Lantern-read state (`gateHeld`/`holdRemain`/`lastNoteType`) ==
  output-path state.**

---

## 5. DECISIONS — RESOLVED by Rodney (MODE_B_SPEC.md §"IMPLEMENTATION DECISIONS")

Both matched my leans; the spec now gives concrete code:
1. **Decision 1 — nullify via nvIdx (lean A).** `getNoteLenIdx(1.f, …)` in `executeModeB`. `executeStep`
   untouched.
2. **Decision 2 — Gate-1-follow at the output write (lean module-layer), + `modeBBridging` for legato.**

### Two technical points I must VERIFY during implementation (Rodney has ruled; these are impl risks)
- **(V1 — highest risk) the MidNote guard's `gatePulseRemain` term.** The early-return is
  `if (gs.holdRemain >= 1.f || gs.gatePulseRemain > 0)`
  ([`SequencerEngine.cpp:426`](../plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:426)). Decision 1
  makes `holdRemain` a 1-step value (ticks to 0 by the next edge, good), but `armGate(1)` sets
  `gatePulseRemain = 1 × pulsesPer16th` (≈6), decremented ONLY by `tickPulse()` on CLOCK grid pulses.
  In external-gate Mode B, confirm `tickPulse()` actually runs (is there an internal clock ticking, or
  is the grid driven only by Gate 1?). If `gatePulseRemain` doesn't decay before the next rise, it will
  STILL swallow that rise as MidNote and Decision 1 alone won't work — the arm or the guard would need a
  Mode B tweak. **Verify this first in code before declaring IMPL 2 done.**
- **(V2) the bridge is OUTPUT-only; Lantern reads decision state.** Decision 2 applies `modeBBridging`
  to `gateV`, not to `gs.gateHeld`. The Lantern's legato render reads `slurForward`/`slurMember`/
  `lastNoteType` (set by `executeStep`), so "Lantern shows what's generated" is delivered by Decision 1
  (correct 1-step notes + articulation), independent of the bridge. Confirm the §6 invariant
  (Lantern state == output state) holds under this split; if any path needs the output set explicitly,
  source it from the SAME state (spec §5).

---

## 6. Risks
| Risk | Sev | Mitigation |
|------|-----|-----------|
| Removing the override regresses to long notes if IMPL 2 incomplete | high | Do IMPL 1+2 together; don't ship IMPL 1 alone |
| Per-sample `gateHeld` overwrite fights `gs.process()` retrigger pulse | med | Drive `gateHeld` only; let `process()` still emit the 1ms retrigger dip on re-articulation |
| STEP/poly diverge if only mono `gs` is driven | med | Drive `gsStep` (and poly gates if Mode B feeds poly) from the same Gate-1 state |
| MidNote change leaks into Mode A | high | Option A confines the change to Mode B's arming; Mode A path untouched |
| Lantern length looks wrong for live gates | low | Display-only; assess in Rack, refine only if needed |

## 7. Acceptance (spec §6)
- [ ] One step per Gate 1 rise; `holdRemain` never spans gates for ANY `noteVal`.
- [ ] REST → gate low that step (holes punched).
- [ ] MODEL 1 legato bridges Gate 1 fall→next rise when `slurForward` set; rest wins; 3-note chains hold.
- [ ] Accent flag set when accented.
- [ ] Lantern state == output state (engine test asserts it).
- [ ] GATE_OUTPUT follows Gate 1 WITHOUT the removed override (verify in Rack).
- [ ] Clean build.
