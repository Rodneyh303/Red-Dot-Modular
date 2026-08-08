# Lock Scope menu — build spec (context-menu granularity for lock mode)

Phase 2 (whole-module lock) is complete: every control in LOCK_SEMANTICS §9 latches/queues correctly.
This spec covers the FINAL touch — a context-menu **Lock Scope** submenu that lets the user choose WHICH
generative surfaces the lock freezes, split by **rhythm** vs **melody** where the data allows.

Read LOCK_SEMANTICS.md §7 (scope-as-future-choice) and §8 (two-tier surface) for the design intent; this
is the build recipe. LockManager already reserves the slot: `LockScope scope` field, persisted + menu-
exposed, with only `WholeModule` functionally wired today.

---

## 1. The core realisation — rhythm/melody is the NATURAL seam

Almost every generative datum in Monsoon is ALREADY stored as separate rhythm and melody arrays/streams.
A rhythm-column / melody-column menu is therefore not fighting the architecture — it exposes a split that
already exists in the data. Verified in code:

| Datum | Rhythm home | Melody home |
|---|---|---|
| Big-5 vs scale/range | `rhythmLive` gate (REST/VAR/LEG/ACC/NOTE_VALUE) | `pitchLive`/`octLive` gates (semiWeights, OCT LO/HI) |
| Sands LOR | RHY/VAR/LEG/ACC strands (per-lane `readStrand(l)`) | MEL/OCT strands |
| A/B mix | `rhythmMix` | `melodyMix` |
| Reseed / dice streams | rhythm Philox stream, `setPendingRhythm*` | melody Philox stream, `setPendingMelody*` |
| Change Alley pins | `caRhythmSrc[]` / `v2->rhythmSrc[]` | `caMelodySrc[]` / `v2->melodySrc[]` |
| CA transform rows | `type = row % 2 == 0` | `type = row % 2 == 1` |
| Redraw | `redrawRhythm` / `shouldRedrawR` | `redrawMelody` / `shouldRedrawM` |

The consequence: most groups split for FREE or CHEAP. Only **Spread** and **Direction** currently gate as a
lane-iterating unit and need their loops made lane-aware to honour the R/M split.

---

## 1a. CV follows its target — AUTOMATIC, not a menu axis

**Confirmed (Rodney): anything that opts OUT of lock brings its modulation (CV) out with it — and anything
that stays locked freezes knob+CV together. This is automatic; the menu never treats CV separately.**

Why it's free: every lock gate sits DOWNSTREAM of CV folding. The CV is summed into the resolved value BEFORE
the gate decides to write/skip, so the gate operates on one number (knob+CV+modulation), not two. Freezing =
holding that resolved value; freeing = re-resolving it (which re-reads the live CV). Verified per group:

- **Big-5 / poly REST-ACCENT**: `getEffectiveMono{Rest,Accent}` / `getEffectivePoly*` sum knob + Junction +
  CV2 + Causeway, then the `rhythmLive`/poly gate reads the RESULT (MonsoonModeController.cpp:58,26). Gate
  frees → the effective value re-reads Causeway/Junction live. Gate freezes → the whole modulated value holds.
- **LOR / Spread / Direction (Sands DNA)**: the manager resolves base + East CV + Macro send/CV INTO
  `baseLen`/spread/dir, THEN the `liveNow` push gate writes or skips (MonsoonSandsManager.cpp:255-268, the
  SpreadResolver path, the direction push). The in-code comment already states it: *"skipping the write holds
  the pre-lock resolved value (base + latched CV)."* So the CV rides the base through the same gate.
- **A/B mix**: the mix CV is folded before `latchMix`; the gate holds/frees the blended result.
- **This is exactly the §9 CV column rule** ("a control's CV latches IFF the control latches; snapshot the
  resolved knob+CV at lock-on"). The scope menu inherits it for free: put a group's bit ON and its CV goes live
  with it; leave it OFF and its CV freezes with it. There is NO separate "modulation scope" toggle and there
  must never be one — CV is bound to its target's scope by construction.

Consequence for the menu: the R/M checkboxes are the ONLY axis. CV is never its own row. A group that is
"kept live under lock" is live for knob AND CV together; a frozen group snapshots knob+CV together.

---

## 2. Semantics of a scope checkbox

Two DIFFERENT meanings depending on the control's lock category — this MUST be kept straight:

- **LATCH controls** (Big-5, LOR, Spread, Pins, A/B, scale/range, Direction/Owner):
  checkbox = "keep this LIVE under lock" — i.e. EXCLUDE it from the freeze.
  Off (default) = frozen under lock (today's whole-module behaviour). On = adjustments apply live under lock.

- **EVENT controls** (Dice, CA Scatter): a roll/scatter can't hold a value, it defers.
  checkbox = "let this FIRE live under lock" — i.e. the event is NOT queued, it commits immediately even
  while locked. Off (default) = queued, fires at unlock (today's behaviour). On = fires live under lock.

So one uniform UI verb — **"keep live under lock"** — reads correctly for both: a latch stays adjustable,
an event keeps firing. Whole-module lock (today) = every box OFF.

---

## 3. The menu matrix (Rodney's grouping, resolved)

A **Lock Scope** submenu. Rows = surface groups; columns = Rhythm / Melody (single checkbox where the
group is single-axis). Each checkbox = "keep live under lock".

| Row (menu label) | Rhythm | Melody | Notes |
|---|:---:|:---:|---|
| **Big-5 / articulation** | ☐ | — | REST, VARIATION, LEGATO, ACCENT, NOTE_VALUE — all rhythm-axis |
| **Scale & range** | — | ☐ | 12 scale sliders + OCT LO/HI — all melody-axis |
| **Poly REST + ACCENT** | ☐ | — | both rhythm; rides Big-5 rhythm or its own row (Rodney: both rhythm) |
| **Sands DNA** (LOR + spread + direction + owner) | ☐ | ☐ | Rodney: group as ONE row, split R/M |
| **Change Alley** (pins + transforms + scatter) | ☐ | ☐ | pins/transforms LATCH, scatter EVENT — all row-typed R/M |
| **A/B mix + Reseed** | ☐ | ☐ | separate rhythm/melody fields + streams |
| **Dice** | ☐ | ☐ | independent rhythm/melody dice + streams (see §6) |

Rodney's rulings folded in:
- "Everything Sands (LOR+spread+owner+direction) grouped as one, ideally split R/M" → the **Sands DNA** row,
  with R and M checkboxes. Its internal gates already key on strand, so the row's two boxes map to strand sets.
- "Big-5 separate from scale+oct gives melody/rhythm split" → the first two rows ARE that split, already
  two separate gates in code — free.
- "Poly rest+accent both rhythm" → single rhythm checkbox (or folded into Big-5 rhythm).
- "Change Alley pins/transforms/scatter split R/M if possible" → yes: every CA datum is R/M-typed.

---

## 4. Per-group feasibility (grounded in code)

| Group | Split cost | Why |
|---|---|---|
| Big-5 (rhythm) | **FREE** | already `rhythmLive` gate — MonsoonModeController.cpp:62 |
| Scale & range (melody) | **FREE** | already `pitchLive`/`octLive` gates — MonsoonModeController.cpp:34-37 |
| Poly REST/ACCENT (rhythm) | **FREE** | one gate, `updatePolyVoiceRest_` — MonsoonModeController.cpp:10 |
| Sands LOR (R/M) | **CHEAP** | `readStrand(l)` is per-lane — MonsoonSandsManager.cpp:264; group strands into R/M sets |
| A/B mix (R/M) | **CHEAP** | `rhythmMix`/`melodyMix` are separate fields — latchMix call, MonsoonModeController.cpp:65 |
| Reseed (R/M) | **CHEAP** | `setPendingRhythm*`/`setPendingMelody*` — separate streams |
| CA pins (R/M) | **CHEAP** | `rhythmSrc[]`/`melodySrc[]` separate — MonsoonSandsManager.cpp:35-60 |
| CA transforms/scatter (R/M) | **CHEAP** | row `type = row % 2` already selects `rhythmSrc`/`melodySrc` — MonsoonChangeAlleyV2.hpp:193 |
| Dice (R/M) | **FREE** | `diceRhythm`/`diceMelody`, independent `shouldRedrawR/M` — PatternEngine.cpp:374-375 |
| **Sands Spread (R/M)** | **MODERATE** | one loop gates all lanes — MonsoonSandsManager.cpp:342; needs per-lane check |
| **Direction/Owner (R/M)** | **MODERATE** | engine promotion loops over all strands — SequencerEngine.cpp:174,283; needs per-strand + engine sees mask |

Only the two MODERATE items need real (still mechanical) work; everything else is a bitmask read.

---

## 5. Wiring — one mask + four touch points

### 5.1 The scope mask
Replace the coarse `LockScope` enum's functional use with a **bitmask of (group × axis)** carried on the
LockManager (persist it exactly like the existing `scope` field; JSON already saves `lockScope`). Suggested:

```cpp
// group bits × 2 axes (R/M). "set" = KEEP LIVE under lock (exclude from freeze).
enum class ScopeBit : uint32_t {
    Big5_R = 1<<0, ScaleRange_M = 1<<1, PolyRA_R = 1<<2,
    SandsDNA_R = 1<<3, SandsDNA_M = 1<<4,
    CA_R = 1<<5, CA_M = 1<<6,
    ABReseed_R = 1<<7, ABReseed_M = 1<<8,
    Dice_R = 1<<9, Dice_M = 1<<10,
};
uint32_t scopeLiveMask = 0;   // 0 = whole-module lock (today). Persisted.
```

The predicate consults it. Extend `liveNow`:

```cpp
// A control is live if it's LIVE category, OR unlocked, OR its scope bit is set (excluded from freeze).
static bool liveNow(Control c, bool locked, uint32_t scopeLiveMask) {
    if (categoryOf(c) == LockCategory::LIVE) return true;
    if (!locked) return true;
    return (scopeLiveMask & scopeBitFor(c)) != 0;   // in-scope-to-freeze unless its bit says keep-live
}
```

Most call sites pass `Control` alone; for R/M-split controls the call site (which already knows its strand/
axis) selects the R or M bit. Keep the existing 2-arg `liveNow(c, locked)` as `liveNow(c, locked, 0)` for the
whole-module default so untouched sites compile unchanged.

### 5.2 The four touch points (things NOT gated by the plain `liveNow` predicate)

1. **Spread loop (MODERATE)** — MonsoonSandsManager.cpp spread blocks currently `if liveNow(Spread)` wrap the
   whole 16-step lane loop. Split the write into rhythm lanes (REST/ACC) vs melody lanes (MEL/OCT) and gate each
   with the matching `SandsDNA_R`/`SandsDNA_M` bit.

2. **Direction promotion (MODERATE, engine-side)** — SequencerEngine.cpp:174,283 promote `laneDirPending_→
   laneDir_` under `&& !locked`. The engine must see the mask (it already sees `locked`): promote a strand only
   if unlocked OR that strand's SandsDNA_{R|M} bit is set. Add a `uint32_t dirScopeMask` the engine reads
   (pushed from the manager each block, like other engine inputs).

3. **Dice redraw (engine-side)** — PatternEngine.cpp:353 `applyPendingSeedsAndRedraw` early-returns on
   `in.locked`, which is what currently QUEUES a dice roll under lock. To let a dice stream fire LIVE under lock,
   the per-stream redraw must run despite `locked`: gate `shouldRedrawR` with `Dice_R` and `shouldRedrawM` with
   `Dice_M` (pass the two bits on PatternInput alongside `locked`). NOTE: this only frees the DICE roll — the
   Big-5/scale freeze still applies to the drawn pattern's SHAPING, exactly as intended.

4. **Scatter queue (queueFires)** — MonsoonExpanderManager.cpp:107 fires the whole CA batch on
   `queueFires()`. To let scatter fire live under lock, OR the fire decision with the CA_{R|M} scope bits; then
   `applyPendingTransforms` must apply ONLY the rows whose `type` matches an in-scope axis (it already knows
   `type = row % 2`), leaving out-of-scope rows queued.

---

## 6. Dice — the performance feature (Rodney's question, resolved)

### 6.0 Baseline FIRST (correcting a common mis-recollection)
**Lock is absolute over dice; live mode does NOT take you out of lock.** Confirmed:
`applyPendingSeedsAndRedraw` early-returns on `in.locked` (PatternEngine.cpp:353) BEFORE the live-mode redraw
decision (`shouldRedraw = ... || (mode == 1)`, :374) is even reached. So under lock, EVERY dice mode — static,
live, trial, last-dice — freezes. LOCK_SEMANTICS §3 says so explicitly ("Dice, all modes… the engine hears
none of it until unlock"). `locked` is the outer gate; `mode` is inside it.

The "live mode is different" decision that DOES exist is about UNDO, not lock: dice undo is armed ONLY in static
mode (UNDO_ITEM4_DICE_BUILD_SPEC.md:148) because a live-mode draw is a continuous PROCESS, not a discrete edit.
That is a statement about undo granularity, NOT about escaping lock. (If you remembered "live mode and lock
interact," this undo ruling is likely what you were recalling — but it does not pull you out of lock.)

So the scope toggle below is a genuine NEW opt-out from the "all dice freeze under lock" baseline, not a
restoration of some pre-existing live-escapes-lock behaviour (there was none).

### 6.1 Why dice scope is the most interesting row
The two dice are INDEPENDENT streams:

- **Already R/M split, zero data work** — `diceRhythm()`/`diceMelody()`, separate Philox streams, separate
  `shouldRedrawR`/`shouldRedrawM`.
- **Current lock behaviour = FREEZE/QUEUE (all modes)**: static press → roll-pending held to unlock; live mode
  → per-cycle reroll suppressed entirely (the `mode==1` redraw never runs while locked). Both via the same
  `locked` early-return.
- **Scope toggle = opt the stream OUT of that freeze**: "keep dice live under lock" lets that stream's redraw
  run while locked (touch point §5.2.3). Two sub-behaviours fall out naturally from the existing mode:
  - **Static + scope-live**: each dice PRESS under lock fires immediately (deliberate reroll over a locked bed).
  - **Live + scope-live**: the stream RESUMES its continuous per-cycle reroll under lock — this is the headline
    "improvise fresh content over a locked groove" gesture. Live mode is the PRIMARY beneficiary: with the bit
    OFF (default) live stays frozen under lock (today's behaviour, the confirmed baseline); only with the bit ON
    does live's continuous reroll bypass the `locked` early-return for that stream.
- **The gesture this unlocks**: because R and M are independent, "**melody dice live, rhythm dice frozen**" (or
  vice-versa) is a genuine performance move. This is the single most compelling reason to build the R/M split
  rather than a flat per-group toggle.

Guard: freeing dice redraw under lock must NOT also free the Big-5/scale SHAPING — those stay frozen by their
own gates. The dice bit reaches only the two `shouldRedraw*` decisions, not the shaping reads. Verify in Rack:
melody-dice-live under lock rerolls pitch SELECTION but the frozen scale/range still BOUND it (a new melody
draw, still constrained to the locked scale).

---

## 7. Build order

1. Add `scopeLiveMask` (persist) + `scopeBitFor(Control)` + the 3-arg `liveNow`. Default mask 0 = today's
   behaviour, all tests green (pure no-op refactor).
2. Thread the FREE/CHEAP groups' call sites to pass their R or M bit: Big-5, scale/range, poly RA, LOR, A/B,
   Reseed, CA pins. Behaviour-neutral while mask stays 0.
3. Spread loop split (touch point 1) — lane-aware R/M gating.
4. Direction engine-scope (touch point 2) — push `dirScopeMask`, per-strand promotion.
5. Dice redraw-scope (touch point 3) — per-stream `shouldRedraw*` bits.
6. Scatter queue-scope (touch point 4) — axis-filtered `applyPendingTransforms`.
7. The menu: a **Lock Scope** submenu (checklist), each item flips its bit(s). Add a "Whole module (freeze
   all)" reset = mask 0, and optionally "Free all prep" = all bits set.

Guard rails:
- Steps 1–2 are behaviour-NEUTRAL with mask 0 (all lock tests stay green).
- Each subsequent step is behaviour-CHANGING ONLY when the user sets a bit → Rack-verify per group.
- Persistence: `scopeLiveMask` saved/loaded like `lockScope` is now; a pre-migration patch loads mask 0.

---

## 8. What is NOT feasible / out of scope

- **Per-lane (6 strands) or per-voice (15) scope** — technically possible (engine is per-voice) but a 15×N
  checkbox matrix nobody can reason about. The R/M split IS the musically meaningful granularity; stop there.
- **Splitting "Sands" from "DNA LOR/Spread/Direction"** — Sands editors have no own lock class; they write the
  SAME engine state through the SAME gates. "Sands" and "DNA" are one group (already Rodney's grouping).
- **Transpose / Clock / Mute / Display** — LIVE; never in scope (no freeze to opt out of).

---

## 9. Cross-refs
- LOCK_SEMANTICS.md §7 (scope as menu choice), §8 (two-tier surface), §9 (the control table).
- LOCK_PHASE2_BUILD_SPEC.md — the completed whole-module lock this builds on.
- src/dsp/managers/MonsoonLockManager.hpp — `LockScope scope`, `liveNow`, `categoryOf` (the extension points).
- MonsoonModeController.cpp:10,34,62 — Big-5/pitch/poly gates (the R/M split already half-present).
- MonsoonSandsManager.cpp:35,264,342 — CA pins, LOR readStrand, Spread loop (touch point 1).
- SequencerEngine.cpp:174,283 — Direction promotion (touch point 2).
- PatternEngine.cpp:353,374-375 — dice redraw gate (touch point 3).
- MonsoonExpanderManager.cpp:107 + MonsoonChangeAlleyV2.hpp:193 — scatter queue + row typing (touch point 4).

## Status
BUILT (Aug 2026) — all 7 steps landed AND all four combined-write groups fully split to EXACT per-axis:

| Group | R/M precision | How |
|---|---|---|
| Big-5 / scale / poly R-A | EXACT (native) | already separate gates |
| Sands LOR | EXACT (native) | per-strand `readStrand` / STRND[] |
| Sands Spread | EXACT | all 8 loops gated per lane-family (rhythm=REST/ACC/VAR/LEG, melody=MEL/OCT) |
| A/B mix | EXACT | `latchMix(...,applyRhythm,applyMelody)` gates each stream's latch+recompute |
| Reseed | EXACT | rhythm/melody seed-arms gated independently in handleRestart (CA corrKey fires if either) |
| CA Pins | EXACT | `remapSlewedByPins(doR,doM)` — families use separate src arrays + buffers |
| CA Scatter/transforms | EXACT | `applyPendingTransforms(...,axisMask)` filters rows by `type = row % 2` |
| Direction / Owner | EXACT | engine `dirLive_(strand)` per-strand promotion |
| Dice | EXACT | per-stream `shouldRedraw*` + `diceLiveR/M` bypass locked early-return |

No coarse "either bit frees both" gates remain. Bit-value hard-codes (engine `dirLive_`, ModeController dice
bits, ExpanderManager CA bits) are pinned to `ScopeBit` via `static_assert`. Persisted as `lockScopeLiveMask`.

### Superseded caveats (now resolved)
The earlier "combined write; split deferred" notes on A/B mix, Reseed, Sands Spread, and CA Pins are OBSOLETE —
each was split to exact per-axis in the same pass. §5.2's "combined-write groups free on either bit" is no
longer true for any group.
