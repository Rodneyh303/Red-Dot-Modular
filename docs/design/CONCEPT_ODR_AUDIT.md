# Concept-level ODR Audit — Red-Dot-Modular (lane-expander-refactor)

**Lens:** not line-level DRY (the prior audit's line-level sections A–E of
`CODEBASE_REFACTOR_REVIEW.md` — A1 strand-mapping ×6, A2 ID helpers, A3 dead
`SpreadInterpolation.hpp`, C1/C2, E1–E3 — cover that; its F1–F9 are a separate RT-safety
category list) and not panel-local — *concepts* duplicated across
distant modules that deserve ONE formalized abstraction, the way
`SandsModifierResolver<Policy>` + the `SandsModifierPolicy` C++20 concept unified
Spread/Warp/LOR/Dir resolution.
**Method:** grep-driven, every finding read in context at `origin/lane-expander-refactor`. Confidence stated per item.
**Re-verified at `fix/bug6-stable-edge` (3172d5c1, 2026-11-18):** every cite below re-checked against the branch and refreshed where its commits moved them. Two branch events touch the findings and are marked inline: **BUG 6** (`b3ba2ea8` — the lane-CV commit edge re-defined to the landed phase) sharpened CA1's drift-history and changed CA5's edge cite; the **display-unify merge** (Stage A/B + BUGs 3–6 now ancestors of this branch) realized the dissolve CA3 predicted. One mis-citation found and fixed: the cross-reference section cited F-numbers that do not exist in the named docs (see that section).
**Reference precedent:** `src/dsp/SandsModifierResolver.hpp` — one skeleton (delegation + own-CV + East-add + Macro send-tap + end-clamp), four policies. The target shape for the findings below.

---

## CA1 — HIGH · The "latched-at-boundary control" lifecycle is copy-pasted per control

The commit TRIGGER is already unified: `MonsoonLockManager::queueFires() = (boundaryNow && !locked) || unlockNow` (`MonsoonLockManager.hpp:184`). But everything hanging off it — the {pending store, committed store, commit-copy, per-scope live-gate} — is re-rolled per control:

| Control | Pending store | Commit-copy site | Live-gate |
|---|---|---|---|
| Voice/Macro LOR | `voiceLorCvPend_` | `refreshVoiceLorLatches` (`SequencerEngine.cpp:694` @bug6) | `lorLive_` / `lorCommitGate_` |
| Lane LOR | `laneLorCvPend_` | `refreshLaneLatchesForVoice` (`:706` @bug6) | `lorLaneCommitGate_` |
| Lane direction | `laneDirPending_` / `laneDirVPending_` | `:455` + `:463` (StepEdge, mono+per-voice), `:520` + `:526` (Phrase) | `dirLive_(l)` |
| Lane sign | `laneSignPending_` / `laneSignVPending_` | alongside dir | (dir's gate) |
| Dice / CA / cantonment | own pending | own site — **"the fire rides lockManager.queueFires() exactly like CA's (\"copy CA exactly\")"** (`MonsoonExpanderManager.cpp:129`) | own |

(The `@bug6` tags: `b3ba2ea8` moved the LOR commit machinery — the cites shifted `:690→:694`, `:702→:706`, and the dir sites `:451→:455`, `:516→:520`, each now with a per-voice twin alongside.)

The comment `"copy CA exactly"` is the smoking gun: this lifecycle is **documented copy-paste**, not a shared abstraction.

**Why it matters — and this is not hypothetical.** The double-commit bug was precisely a *drift inside one copy* of this lifecycle: the LOR commit edge was checked against a `voiceFramePhase_` that got advanced between the two check sites, so the latch fired twice. A `LatchedAtBoundary<Policy>` skeleton — pending/committed storage + "commit on `queueFires()`" + a scope live-gate, with the per-control storage shape and the "what counts as my edge" as the policy — would have made that a single, pinned definition rather than something each control re-derives and can get subtly wrong. This is the same win the modifier resolver already bought for the resolution layer. And it happened AGAIN (the branch's BUG 6, `b3ba2ea8`): the edge itself was wrong — the head tested the *entry* phase (`voiceFramePhase_` before the frame advance), so every lane-CV commit fired one step after the wrap; the fix re-defined the edge for all three lane families at once as the **landed phase** `wrapMod(t0EffNow, walkPeriod(dir, L)) == 0` (`SequencerEngine.cpp:1060`/`:1080`/`:1100`). Two drift events in the same lifecycle, in two different places (the cadence, then the edge) — each fixed by hand across all the copies. That is exactly the "single pinned definition" the skeleton buys.

**Shape of the fix:** a `LatchedAtBoundary<Policy>` (Policy supplies: the value type, the edge predicate, the scope bit, the commit action) that LOR / dir / sign / dice / CA / cantonment all instantiate. Biggest payoff of the lot — it turns "the commit cadence" into one edit and one pin, exactly where the drift bugs live.
**Confidence:** HIGH that the duplication is real and documented; the abstraction is a design call (storage shapes differ: per-lane, per-voice-lane, scalar — templating handles it, as the resolver already proves).

## CA2 — MED · The per-axis pattern-engine lifecycle (rhythm / melody / qmix) is hand-unrolled ×3

`PatternEngine` writes the same lifecycle out three times, once per axis (all cites are `PatternEngine.hpp`, re-verified still exact — the header declares the trio/counter/zero state; the `.cpp` definitions sit at `:481`/`:602`/`:689` and `:771`/`:826`/`:871`):
- `recomputeEffectiveRhythm` / `…Melody` / `…Qmix` (`:620-622`) and the `…ForVoice` trio (`:703-705`).
- `rhythmDrawCtr` / `melodyDrawCtr` / `qmixDrawCtr` (`:901`); `zeroRhythmIndex` / `zeroMelodyIndex` / … (`:920-921`); the three draw caches.

The *content* differs per axis (rhythm = gates, melody = pitch, qmix = mix) but the *lifecycle* (draw → cache → recompute → counter → zero) is identical. Proof it templates cleanly: `pruneDrawCache_` is ALREADY a single template over all three (the bounded-cache work). The rest wasn't parameterized.

**Why it matters:** a cadence/cache change has to be applied in three places and the three can drift — the same failure class as CA1, one tier down. (The bounded-cache fix had to touch all three; a one-axis miss would have been a silent per-axis divergence.)

**Shape of the fix:** parameterize the axis — an `Axis` enum indexing arrays, or a small `PatternAxis<Policy>` holding {counter, cache, recompute} with the per-axis draw as the policy. Bigger surgery than CA1; worth it when the next pattern-engine change would otherwise be ×3.
**Confidence:** HIGH the duplication is real; MED on whether it's worth the surgery now vs a test that pins the three lifecycles equivalent.

## CA3 — MED · `applyMacroCV` is a verbatim-copied macro-CV composition that bypasses the resolver

The managers' per-lane resolve rides the skeleton (both under `src/dsp/managers/`) — `splitWriteLane` → `SandsModifierResolver<LorPolicy>` (`MonsoonExpanderManager.cpp:264` — the wiring comment; the lambda now at `:308`), spread → `SpreadPolicy` (`:333`), dir → `DirPolicy` (`:458`). Good. But `MonsoonSandsManager.cpp:996-1001` (comment `:994`) defines `applyMacroCV` — `clamp(base + cv·att·(hi−lo))` — **"copied VERBATIM from processDNA's local helper"** (its own comment). It hand-rolls the macro's CV-applied read instead of riding the skeleton's Macro-send branch.

This is the un-latched path we traced driving the Macro lane-row display (it writes `macroBase`/`macroCVDelta`, `:1062`). So "Macro's CV-applied value" is defined twice: once inside the resolver (the send-tap contract), once as this verbatim copy. The predicted partial dissolve **has happened**: the display-unify branch is now merged into *this* branch (Stage A/B + BUGs 1–6 are ancestors), and the lane-RANGE display reads ride the committed window (`disp*` / `effectiveLaneLor`, `SequencerEngine.hpp:1282`). What remains is the *manager-side* `applyMacroCV` composition (its TU untouched by the branch — last commit `d3703f54`) and the Macro lane-row display trace it still feeds.

**Shape of the fix:** route `applyMacroCV`'s callers through the `SandsModifierResolver` Macro branch (or document it as a deliberately-live diagnostic read, as the prior audit's F2 suggests for the fingerprint site).
**Confidence:** HIGH it's a verbatim copy (self-documented); the display-side half cleared with the display-unify merge, the manager-side copy remains.

## CA4 — LOW · The engine's `effective*Lor` read hand-rolls delegation + clamp the resolver formalizes (likely two legit stages — flagged, not asserted)

`SequencerEngine::effectiveVoiceLor` and family (inline in `SequencerEngine.hpp` — `:983+` at the audit baseline, `:1023`/`:1038` @bug6, with the lane member `effectiveLaneLor` at `:1282`) hand-roll {local-vs-macro branch, base + committed-CV delta, `lorRowClamp_`} and do **not** include/use `SandsModifierResolver<LorPolicy>` (grep re-run @bug6: still exactly ONE incidental reference in the whole engine TU — `SequencerEngine.hpp:1899`, the spread-term comment). The resolver also expresses a delegation branch (`delegated`) and a clamp (`constrain`).

**But** these are plausibly two legitimate pipeline stages, NOT a dup: the resolver composes the *committed base* (manager side, pre-store); the engine adds the *live/committed CV delta* on *read* (the C1b cadence). The overlap is only the delegation decision (local-vs-macro) and the clamp, applied at both layers. So this is a "share the primitives?" question, not a "collapse two copies" one. I am NOT calling it a violation — logging it so the question gets asked when CA1 is tackled (the `composeLor_` helper the decomposition note proposes, `:43`, is the natural shared primitive).
**Confidence:** LOW it's a true ODR; recorded for design judgement.

## CA5 — HIGH · The phase-holder maintenance skeleton is hand-rolled ×4 (×8 sites counting `anchorTransport`) with an incomplete shared key

The "maintain a bounded walk PHASE congruent to a driver coordinate, re-seed when the walk KEY changes" lifecycle is written out four times in `SequencerEngine.cpp::tickHolders` (`:963` @bug6), identical in shape — **and the same four key-writes are re-rolled AGAIN in `anchorTransport` (`:880`, the scrub/re-anchor path: `:907`/`:927`/`:942`/`:952`)**, so the skeleton is really two functions × four holders (re-verified @bug6):

| Phase holder | Stored key | Re-seed site |
|---|---|---|
| Macro frame | `macroFrameKey_` | `:1016` |
| Voice frame | `frameKey_[v]` | `:1078` |
| Per-voice lane | `laneKeyV_[v][s]` | `:1098` |
| Mono lane | `laneKey_[s]` | `:1116` |

Each copy is:
```
const int key = walkKeyOf(dir, len);
phase = (storedKey != key) ? wrapMod(t0(+advanceOffset), walkPeriod(dir,len))
                           : wrapMod(phase + kV, walkPeriod(dir,len));
storedKey = key;
```
The re-seed KEY is the one shared function `walkKeyOf(dir, len) = dir*64 + len`
(`SequencerEngine.hpp`) — the walk's OWN direction and length. **It does not include the
parent/global cycle.** So when a parent cycle changes under a holder without changing the
holder's own dir/len, the phase never re-seeds and silently keeps a stale alignment.

**Why it matters — not hypothetical (CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC.md).**
Setting the global transport below the voice length accumulates a per-voice
`advanceOffset_` divergence (`:632`, `foldPerVoiceAdvance_`); restoring the global length
does not clear it, and because `walkKeyOf` omits the global, the voice-frame phase never
re-seeds to correct it. Result: the probability-lane LOR commit (fired at the **landed phase** since BUG 6 `b3ba2ea8` — `wrapMod(t0EffNow, walkPeriod(dir,L)) == 0`, which fixed the wrap-STEP reference; before that fix the commit rode the entry phase and landed one step late) still lands at the wrong transport step once `advanceOffset_` has drifted: `voicePeriod − advanceOffset_` (the "step 6/7" bug — diagnosed on this branch, `d85f0655`, with the repro diags `diag_octave_step6.cpp` / `diag_advoffset_phase_shift.cpp`; NOT yet fixed). The voice frame and the per-voice lanes share the exact `advanceOffset_` dependency,
so the same drift is latent in the per-voice lanes; the macro and mono holders share the
key's blind spot and would drift if their parent changed under them (same shape, not yet
reproduced).

**Relationship to CA1 — adjacent, not the same.** CA1 is the commit *lifecycle* (the
consumer); its edge policy for the LOR lane is now the landed-phase test
(`wrapMod(t0EffNow, walkPeriod(dir,L)) == 0`, the BUG-6 correction). CA5 is the *producer*
of that phase. The double-commit (CA1's exemplar) was the seam between them (a phase advanced
between two commit sites); the `advanceOffset_` desync is one layer deeper — the phase is
mis-aligned before the commit reads it. Building `LatchedAtBoundary` (CA1) does NOT fix CA5.

**Shape of the fix:** a single `PhaseHolder` — the `POSITION_HOLDER_MODEL.md` machinery made
one templated skeleton, which is already the decomposition note's item 4 ("finish the
`PositionResolver` migration — migrate `laneTickFor` / `getStrandIdx` / the
`voiceFramePhase_`/holders behind `pos_`"). ONE key definition means the parent cycle is added
to the key once and all four holders re-seed correctly together, pinned by one test. The
point-fix (re-anchor `advanceOffset_` on a global-length change, or fold a global-generation
counter into `frameKey_`) stops the step-6/7 bug now; the `PhaseHolder` migration stops the
class. NB: re-seeding `voiceFramePhase_` alone does not fix it — `t0EffNow` already carries
`advanceOffset_`, so the accumulated offset itself must be corrected.
**Confidence:** HIGH the duplication is real (four verbatim copies in `tickHolders` plus `anchorTransport`'s second roll — eight key-write sites, one shared key); HIGH the
key's blind spot is the mechanism behind the reproduced desync.

---

## Examined and found NOT to be concept-dup
- The tiered LOR stores (`voiceLorStore_` / `macroVoiceLor_` / `lor()` / `polyLOR()`) — the design; the resolver + effective accessors are the chokepoints.
- `laneDir_ ← laneDirPending_` at StepEdge (`:455`) vs Phrase (`:520`) — two intentional quantizations, not copies (matches the prior audit's non-finding).
- The four resolver policies (Spread/Warp/LOR/Dir) — the unification itself, working as intended.

## Cross-reference with the prior (line-level) audit
**Attribution corrected @bug6:** this section originally cited "the prior audit's F3/F7" as
if they lived in `CODEBASE_REFACTOR_REVIEW.md` / `REFACTOR_PRIORITY.md` — they do not. Those
docs' F1–F9 are the RT-safety categories (their F3 = thread-safety of UI↔audio shared state,
F7 = param config completeness). The F-numbers below come from
`SEQUENCER_ENGINE_DECOMPOSITION_NOTE.md:42-48`, which cites a line-level audit whose F-list
is **not committed to the repo** (chat-delivered). The substantive claims are real and were
re-verified against this branch's code:
- The **direction-authority widget re-resolve** (the decomposition note's "line-level F3") still exists: `StraitsEastSandsVisual.cpp:1408` calls `expanderManager.monoDirAuthority(lane)` (was `:1441` at the audit baseline — the file was reworked by the branch's BUG-4/BUG-5 display commits). It is the *widget-side* instance of CA1's direction lifecycle — same family; do them together under the direction track.
- The **effective-LOR accessor family ×8** (the decomposition note's "prior audit F7 / concept audit CA4") is the line-level face of CA4; the `composeLor_` helper the decomposition note proposes (`:43`) is the shared primitive CA4 asks about.
- CA5 is the concept face of the decomposition note's **item 4** (`:33`, the `PositionResolver` migration) — cite verified.

## Suggested order
1. **CA1** — the `LatchedAtBoundary` skeleton. Highest value: it's where the commit-cadence drift *bugs* occur (the double-commit, then BUG 6's edge correction — two drift events, see above). Pair with the cadence pin the line-level audit proposed.
2. **CA5** — the `PhaseHolder` / `PositionResolver` migration. The other place drift *bugs* occur (the global<voice desync); the point-fix is cheap, the migration is the class fix.
3. **CA3** — route `applyMacroCV` through the resolver (or label it); the display-side half already rode in with the display-unify merge (an ancestor of this branch).
4. **CA2** — parameterize the pattern-engine axis, or at minimum pin the three lifecycles equivalent.
5. **CA4** — decide if the engine read shares the delegation+clamp primitive, alongside the decomposition note's `composeLor_` item.

*Produced 2026-11-17 (CA1–CA4), CA5 added 2026-11-18; line numbers originally at `origin/lane-expander-refactor`. Re-verified against `fix/bug6-stable-edge` @3172d5c1, 2026-11-18: all cites re-checked, stale ones refreshed (marked `@bug6`), the F3/F7 cross-reference attribution corrected, and the `anchorTransport` second copy added to CA5.*
