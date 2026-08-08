# STEP 1 — Instrument the WriteLedger write sites

> Source: `plugins/Melodicer/docs/design/RATE_AND_DATAFLOW_ENTRY.md` STEP 1.
> Goal: turn every future single-writer violation on the multi-writer fields into a loud
> debug warning. **Debug-only, zero release-path cost** (compiles out under `NDEBUG`).
> Behaviour-preservation bar: 172-test suite green + WriteLedger silent.

---

## 0. What's already built (do NOT rebuild)

- `WriteLedger` struct + `WriteField` / `WriteRole` enums live in
  [`SequencerEngine.hpp:79`](plugins/Melodicer/src/dsp/engines/SequencerEngine.hpp:79).
- The ledger instance is `engine.writeLedger` ([`SequencerEngine.hpp:392`](plugins/Melodicer/src/dsp/engines/SequencerEngine.hpp:392)).
- **`beginBlock()` is already wired** — called at
  [`SequencerEngine.hpp:396`](plugins/Melodicer/src/dsp/engines/SequencerEngine.hpp:396)
  inside `beginStrandWriteBlock()`, which already runs at the top of every process block
  (same call site as the existing StrandLedger reset). No new block-reset plumbing needed.
- `noteWrite(role, field)` is **defined but never called** (0 call sites today). STEP 1 is
  purely: *call it at each write site + pick the right `WriteRole` per site*.

### Enum inventory

```
WriteField : RestProb=0, AccentProb, Wrapped, LastSelectedScale, StepGate, COUNT
WriteRole  : NONE, MONO, EAST, MACRO, POLY, ENGINE, EXPANDER, CVROUTER, SCALEMGR, PARAMMGR
```

`StepGate` is reserved (pre-added, not in the entry doc's target list) — leave it for a
later step; do not instrument it in STEP 1.

---

## 1. Architecture gap to resolve FIRST (blocks `LastSelectedScale`)

The `WriteLedger` lives on `SequencerEngine`, but `lastSelectedScale` lives on a **separate
object** — `MonsoonScaleManager` ([`MonsoonScaleManager.hpp:25`](plugins/Melodicer/src/dsp/managers/MonsoonScaleManager.hpp:25)).
Two of its three write sites are also **outside the audio block** (UI menu, JSON load),
which the per-block ledger model does not cover by default.

### Decision: route writes through a ScaleManager setter that holds a back-pointer to the ledger

1. Add `WriteLedger* writeLedger_ = nullptr;` to `MonsoonScaleManager`, wired during the
   normal engine/manager construction (the same place `scaleManager` gets its engine ref).
2. Add `void setLastSelectedScale(int idx, WriteRole role)`:
   ```cpp
   void setLastSelectedScale(int idx, WriteRole role) {
       lastSelectedScale = idx;
   #ifndef NDEBUG
       if (writeLedger_) writeLedger_->noteWrite(role, WriteField::LastSelectedScale);
   #endif
       updateScaleMask();
   }
   ```
3. **Out-of-block writes (UI menu, JSON load) are NOT instrumented** — they happen between
   audio blocks, so the per-block ledger would either false-fire (writer persists across
   `beginBlock`) or miss them. They are a *different rate domain* (the meta-lesson). Document
   them as intentionally excluded. The audio-block write site (shophouse boundary) is the
   real single-writer candidate and IS instrumented.

This keeps one unified ledger on the engine (cross-object conflicts still caught) and
matches the existing `setStrand(role, ...)` idiom.

---

## 2. Write-site inventory + role assignment

Each row = one `noteWrite(role, field)` call to add. `Field` storage location in brackets.

### restProb  →  `WriteField::RestProb`

| # | Site | Storage | Role | Notes |
|---|------|---------|------|-------|
| R1 | [`MonsoonModeController.cpp:19`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:19) `engine.voices[i].restProb = getEffectivePolyRest(i)` | `PolyVoice::restProb` | `POLY` | per-voice, poly path. `getEffectivePolyRest` is the intended sole writer (per entry doc). |
| R2 | [`MonsoonModeController.cpp:37`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:37) `currentPatternInput.restProb = getEffectiveMonoRest(...)` | `PatternInput::restProb` | `MONO` | mono path. Distinct storage from R1 — no cross-conflict expected; ledger confirms. |
| R3 | [`MonsoonExpanderManager.cpp:380`](plugins/Melodicer/src/dsp/managers/MonsoonExpanderManager.cpp:380) | `PolyVoice::restProb` | `EXPANDER` | **DEAD — commented out.** Do not instrument; note as evidence the expander poly-rest path was already removed. |

### accentProb  →  `WriteField::AccentProb`

Two distinct storage locations — keep them separate in the ledger (same `WriteField`, so a
same-block write to *both* mono and poly storage would surface as a conflict; that's
acceptable and informative — it'd flag a real rate-boundary crossing).

| # | Site | Storage | Role | Notes |
|---|------|---------|------|-------|
| A1 | [`Monsoon.cpp:887`](plugins/Melodicer/src/Monsoon.cpp:887) `engine.accentProb = getEffectiveMonoAccent(...)` | `SequencerEngine::accentProb` (mono, line 474) | `MONO` | mono path. |
| A2 | [`MonsoonModeController.cpp:112`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:112) `engine.accentProb = getEffectiveMonoAccent(...)` | mono | `MONO` | same role as A1, different call path (Mode E). Ledger confirms same-block = no conflict. |
| A3 | [`MonsoonModeController.cpp:135`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:135) `engine.accentProb = ...` | mono | `MONO` | ditto (another mode). |
| A4 | [`MonsoonModeController.cpp:20`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:20) `voices[i].accentProb = getEffectivePolyAccent(i)` | `PolyVoice::accentProb` | `POLY` | poly path. `getEffectivePolyAccent` is the intended sole writer. |

> The entry doc's "mono accentProb (2 write sites)" = A1 vs A2/A3. Same role → ledger
> should stay silent. If it fires, that's the bug.

### wrapped  →  `WriteField::Wrapped`

Single writer found — `ENGINE`, inside `advancePlayhead`:

| # | Site | Storage | Role | Notes |
|---|------|---------|------|-------|
| W1 | [`SequencerEngine.cpp:258`](plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:258) `wrapped = (prevStep != -1 && stepIndex == endStep)` | local → `StepResult::wrapped` | `ENGINE` | reverse-direction wrap. |
| W2 | [`SequencerEngine.cpp:267`](plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:267) `wrapped = (prevStep != -1 && stepIndex == startStep)` | local → `StepResult::wrapped` | `ENGINE` | forward-direction wrap. |

Propagation at [`SequencerEngine.cpp:636`](plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:636)
and [`:697`](plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:697) (`result.wrapped = wrapped`,
`lastStepResult = result`) are **reads of the local, not new writes** — do not instrument.
All readers (`Intertropical.cpp:95`, `Monsoon.cpp:564/611`, `MonsoonModeController.cpp:86/204/229`)
are reads.

> The entry doc lists `wrapped` as a multi-writer candidate, but the audit found a single
> writer (ENGINE). **Instrument to CONFIRM, not assume** — that's the whole point of STEP 1.
> Place `noteWrite(ENGINE, Wrapped)` once at the top of `advancePlayhead` after the wrap
> decision (one call covers both W1/W2 since only one branch runs per call).

### lastSelectedScale  →  `WriteField::LastSelectedScale`

Routed through the new `ScaleManager::setLastSelectedScale(idx, role)` (§1).

| # | Site | Role | In-block? | Action |
|---|------|------|-----------|--------|
| L1 | [`Monsoon.cpp:565`](plugins/Melodicer/src/Monsoon.cpp:565) shophouse boundary `scaleManager->lastSelectedScale = shopPendingScale_` | `SCALEMGR` | **yes** (process step, gated on `lastStepResult.wrapped`) | **Instrument** — replace with `setLastSelectedScale(shopPendingScale_, WriteRole::SCALEMGR)`. Also `scaleRoot` write on the next line: see note below. |
| L2 | [`MonsoonWidget.cpp:950`](plugins/Melodicer/src/ui/MonsoonWidget.cpp:950) UI menu `onAction` | — | no (UI thread) | Exclude (§1). Keep direct write; document. |
| L3 | [`MonsoonPersistenceManager.cpp:251`](plugins/Melodicer/src/dsp/managers/MonsoonPersistenceManager.cpp:251) JSON load | — | no (load) | Exclude (§1). Keep direct write; document. |

> `scaleRoot` is written alongside L1 ([`Monsoon.cpp:566`](plugins/Melodicer/src/Monsoon.cpp:566)).
> `scaleRoot` is **not** in the `WriteField` enum. Do not add it in STEP 1 (one concept per
> PR). If the ledger later shows `lastSelectedScale` is clean but `scaleRoot` drifts, add a
> `ScaleRoot` field in a follow-up.

### chosen voices[].*  →  `WriteField::StepGate` is the closest, but the entry doc means the per-step "chosen" fields

The `PolyVoice` "chosen" fields are `accented` ([`:45`](plugins/Melodicer/src/dsp/engines/SequencerEngine.hpp:45))
and `participating` ([`:52`](plugins/Melodicer/src/dsp/engines/SequencerEngine.hpp:52)). A
direct-assignment regex search (`voices[i].accented =`, `voices[i].participating =` outside
reset) **returned 0 hits** — the per-step writes likely go through a reference alias or
inside `VoiceResolver` ([`VoiceResolver.hpp:28`](plugins/Melodicer/src/dsp/VoiceResolver.hpp:28))
/ `executeStep`, not matching the simple `voices[idx].field =` pattern.

| # | Site | Role | Action |
|---|------|------|--------|
| V? | per-step `accented` / `participating` writes (location TBD) | `ENGINE` | **Locate first.** Search `VoiceResolver.hpp` and `executeStep` in `SequencerEngine.cpp` for writes via `PolyVoice&` references / pointers. Instrument once located. |

> This is the one open sub-task that needs a code search pass before instrumentation. It's
> isolated — do it as the last sub-step of STEP 1 so it doesn't block R/A/W/L.

---

## 3. Execution order (one concept per PR, per the guard rails)

Each bullet = one independently-shippable PR. Land R+A first (highest leverage, lowest
doubt), then W (confirmation), then L (needs the §1 plumbing), then V (needs the V?
search).

1. **PR-1: restProb + accentProb instrumentation** (R1, R2, A1–A4).
   - Pure `noteWrite` additions at 6 sites. No new plumbing. No behaviour change.
   - Verify: build debug, run 172-test suite, confirm ledger silent.
2. **PR-2: wrapped instrumentation** (W1/W2 → single `noteWrite(ENGINE, Wrapped)` in
   `advancePlayhead`).
   - Confirms single-writer. If it fires, that's a real finding → STOP, log it, do not
     "fix" inside this PR (one concept per PR).
3. **PR-3: ScaleManager back-pointer + `setLastSelectedScale` + L1** (§1 + L1).
   - Adds the `WriteLedger*` wiring + the setter + converts the one in-block write site.
   - Excludes L2/L3 with a code comment citing the rate-domain meta-lesson.
4. **PR-4: chosen voices[].* (V?)** — only after the V? search locates the write sites.

---

## 4. Verification protocol (per PR)

1. Build **debug** (`NDEBUG` undefined) — the ledger is inert in release, so a release
   build proves nothing.
2. Run the 172-test suite → must stay green.
3. Run with `WARN` defined (the ledger's preferred path,
   [`SequencerEngine.hpp:97`](plugins/Melodicer/src/dsp/engines/SequencerEngine.hpp:97)) and
   scan stderr for `[WriteLedger] CONFLICT`.
   - **Silent = pass.** Any CONFLICT = a real two-writer bug surfaced; file it (do not
     patch in the same PR).
4. Build **release** (`NDEBUG` defined) → confirm the calls compile out (no size/perf
   regression). Optional: objdump a translation unit to confirm zero `noteWrite` symbols.

---

## 5. Out-of-scope for STEP 1 (do not do here)

- `StepGate` field (reserved for later).
- `scaleRoot` (add only if a later audit shows drift).
- Any *fix* to a conflict the ledger surfaces — those are STEP 2/3/4 work. STEP 1 only
  *detects*.
- The live StrandLedger MACRO/EAST conflict — that's STEP 2.
- RATE_TABLE column audit / compute-on-read pulls — that's STEP 3.
- Hot per-sample cache single-writer refactors — that's STEP 4.

---

## 6. Risk assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| `noteWrite` call accidentally placed on a *read* path | low | Each site is an `=` write; review diff. |
| False CONFLICT from out-of-block writes (L2/L3) | medium | Excluded by design (§1); ledger resets each block. |
| §1 back-pointer introduced before engine fully constructed | medium | Wire the pointer at the same construction point as existing manager refs; null-guard in the setter. |
| V? write sites use reference aliases the regex missed | low | Dedicated search pass in PR-4; doesn't block PR-1..3. |
| Release-build perf regression | none | `noteWrite` compiles to nothing under `NDEBUG` by design. |

---

## 7. Acceptance criteria for STEP 1 (all four PRs merged)

- [ ] `noteWrite` called at R1, R2, A1, A2, A3, A4, W1/W2, L1, and V?.
- [ ] `ScaleManager::setLastSelectedScale` exists, wired to `engine.writeLedger`.
- [ ] L2 (UI) and L3 (JSON) documented as intentionally excluded.
- [ ] Debug build: 172 tests green, ledger silent.
- [ ] Release build: compiles, no perf regression.
- [ ] Any CONFLICT the ledger surfaced is filed as a STEP 2/3/4 ticket (not fixed here).

---

## 8. Implementation log (what was actually done)

### PR-1 — DONE (mono storage only)
Instrumented the writers that genuinely share storage:
- **A1** [`Monsoon.cpp:887`](plugins/Melodicer/src/Monsoon.cpp:887) `engine.accentProb` → `noteWrite(MONO, AccentProb)`.
- **A2** [`MonsoonModeController.cpp:112`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:112) `engine.accentProb` → `noteWrite(MONO, AccentProb)`.
- **A3** [`MonsoonModeController.cpp:135`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:135) `engine.accentProb` → `noteWrite(MONO, AccentProb)`.
- **R2** [`MonsoonModeController.cpp:37`](plugins/Melodicer/src/dsp/managers/MonsoonModeController.cpp:37) `currentPatternInput.restProb` → `noteWrite(MONO, RestProb)`.

**Deferred (false-conflict risk):** R1/A4 write `voices[].restProb/accentProb` — *different storage* from R2/A1-A3.
Under one shared `WriteField` they would fire MONO-vs-POLY every block on unrelated storage, making the
"ledger silent" bar unmeetable. They are single-writer (`getEffectivePoly*`); the expander write (R3,
[`MonsoonExpanderManager.cpp:380`](plugins/Melodicer/src/dsp/managers/MonsoonExpanderManager.cpp:380)) is
dead/commented. Defer to STEP 3 (expander audit + separate enum fields if needed).

### PR-2 — DONE
- **W1/W2** [`SequencerEngine.cpp`](plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:269) `advancePlayhead`:
  single `noteWrite(ENGINE, Wrapped)` after the if/else block (covers both dir branches). Audit confirmed
  `wrapped` is single-writer ENGINE; the entry doc's multi-writer suspicion is **not** borne out — ledger
  confirms.

### PR-3 — DONE (simplified from §1)
The plan's §1 back-pointer was **not needed**: all `lastSelectedScale` writes are *external* to
`ScaleManager` (it never writes the field itself). L1 is in `Monsoon.cpp` where `engine.writeLedger` is
already directly accessible.
- **L1** [`Monsoon.cpp:567`](plugins/Melodicer/src/Monsoon.cpp:567) shophouse-boundary write →
  `noteWrite(SCALEMGR, LastSelectedScale)`. In-block, gated on `lastStepResult.wrapped`.
- **L2** (UI menu, `MonsoonWidget.cpp:950`) and **L3** (JSON load, `MonsoonPersistenceManager.cpp:251`)
  excluded — out-of-block (different rate domain). Documented inline.

### PR-4 — AUDIT FINDING (no code; guard rail: do not over-correct)
Located the "chosen `voices[].*`" write sites in `executePolyVoice`
([`SequencerEngine.cpp:721`](plugins/Melodicer/src/dsp/engines/SequencerEngine.cpp:721)):
`v.accented` (745/762/889) and `v.participating` (790/829), plus resets at 610/672. **All single-writer
ENGINE** — no multi-writer drift class. No `WriteField` enum entry exists for them; `StepGate` is reserved
and the entry doc says don't instrument it in STEP 1. Adding enum entries + instrumentation for a
non-multi-writer field = over-correction, explicitly warned against by the guard rails. **No
instrumentation added.** If a future audit shows a second writer of `accented`/`participating`, add a
`VoiceAccented`/`VoiceParticipating` field then.

### Detection-mechanism note (informs all PRs)
`WriteLedger::noteWrite` warns **at write-time** (`prev != role` → WARN immediately), so the
`beginStrandWriteBlock()` reset position at [`Monsoon.cpp:892`](plugins/Melodicer/src/Monsoon.cpp:892)
(mid-block, after A1/R2) does **not** prevent detection — a conflicting second write fires its warning
inline before any reset wipes the slate. No reset repositioning was needed.
