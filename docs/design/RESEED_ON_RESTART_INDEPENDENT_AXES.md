# Reseed-on-restart: independent R / M / Q / CA axes (PARKED — pick up later)

**Status: DESIGN + DECISIONS captured; NOT started. Parked mid-discussion at Rodney's request.**
Cross-ref: SEEDER_EXPANDER_CONCEPT.md (adjacent seed-source territory), CA_DICE_COUNTER_MODEL.md
(reseed vs dice-reverse boundary), PHILOX_KEY_DERIVATION_AND_CA_SEED.md (one-seed-source →
per-stream deriveKey), LOCK_MODE_AUDIT.md (Reseed is a LATCH/lock-scoped control).

## The decision so far (Rodney)
Add INDEPENDENT reseed-on-restart for four axes: **rhythm, melody, q-mix, Change Alley**.
- R / M / Q: toggled (modulated) from **Raffles** gates.
- CA: the modulation toggle lives **directly on the Change Alley panel** (CA is itself a Monsoon
  expander, potentially serving MULTIPLE Monsoons).

## Current state of the code (verified, as of commits b5c4223 + d45edec)
- ONE global bool `reseedOnRestart` (Monsoon.hpp:622). Read at exactly ONE site: `handleRestart()`
  (Monsoon.cpp:386) inside `if (reseedOnRestart)`.
- The per-lane GATE already exists: `reseedR/M/Q = LockManager::liveNow(Control::Reseed, …)`
  (Monsoon.cpp:379-384) gate `setPending{Rhythm,Melody,Qmix}Seed` / `…ReseedRoll`.
- **CA reseed is currently UNCONDITIONAL** inside the policy block: `reseedCorrKeys(s)`
  (Monsoon.cpp:400) / `seedCorrKeysInternal()` (:408) fire whenever the block runs. Not gated by
  reseedR/M/Q, and no CA-specific policy bool.
- Toggled by `DA_RESEED_RESTART` (Monsoon.cpp:465), fired by Gate-3 g3map AND the (now-fixed)
  Raffles RESEED_RESTART gate. Menu bool "Reseed on restart" (MonsoonWidget.cpp:1162). Persisted
  key `reseedOnRestart` (PersistenceManager 38/255).
- `handleRestart()` callers (the restart events that consult the policy): TimingController 94/128/136
  (restart-now / restart-on-unmute / gate-2 rise), Monsoon.cpp:615 (RESET trigger), :955 (mute-unmute).
- **Phase engine does NOT use reseed-on-restart.** Mode E/F phrase-wraps reseed via a DIFFERENT
  path: onPhraseBoundary_() → applyPendingSeedsAndRedraw() (pending dice/seed flags), which never
  reads reseedOnRestart. So a policy split has ZERO phase-engine blast radius.

## Interaction safety (verified — independent R/M/Q/CA is safe)
- **Undo:** dice-undo capture keys ONLY on `*RollPending` (user dice press); reset/reseed are
  explicitly EXCLUDED (PatternEngine.cpp:455-462). Reseed is modulation-class → writes no undo by
  design. Per-lane changes nothing.
- **Lock:** reseed already lock-scoped (Control::Reseed; seeds freeze under lock). A per-lane POLICY
  bool only ANDs with that gate — can further restrict, never bypass the freeze.
- **True-reverse:** orthogonal. It replays committed PIN-STATE; reseed re-keys the SCATTER Philox
  streams (corrKey[]) — a different axis. Docs treat reseed as a SEPARATE gesture from structural
  reset. A CA-own reseed axis actually HELPS the intended "keep structure, reseed material".
- **Philox dice-reverse:** reverse needs a fixed key + addressable counter; reseed changes the key —
  inherently a fresh-entropy boundary (already why reseed lives on RESET, not roll: Monsoon.cpp:442).
  Per-lane NARROWS this (only opted lanes lose pre-reseed reverse-reproducibility; others keep theirs).
- **Inherent, unchanged property (not a bug):** reseeding a lane resets its counter, so philox-reverse
  cannot cross that reseed on THAT lane. Same as today, now per-lane.

## Implementation plan (3 parts, when resumed)

### Part 1 — Engine: 4 independent reseed bools + per-lane DieActions + migration (self-contained, testable)
- Replace `reseedOnRestart` with `reseedOnRestartR/M/Q` + `reseedOnRestartCA` (Monsoon.hpp).
- `handleRestart()` (Monsoon.cpp:385-410): gate each lane's `setPending*Seed`/`…ReseedRoll` on its
  policy bool AND its existing reseedR/M/Q lock gate; gate `reseedCorrKeys`/`seedCorrKeysInternal`
  (the CA path) on `reseedOnRestartCA`. Outer `if` becomes "any of the four".
- DieAction: split `DA_RESEED_RESTART` → `DA_RESEED_RESTART_R/M/Q` (+ a CA one if CA is gate-driven);
  repoint Gate-3 g3map + kRafflesGateAction. Keep the co-located-SoT discipline.
- Persistence: migrate old single `reseedOnRestart` key → all four bools (old patches keep reseeding
  everything). New default: all true (matches current behaviour).
- Context menu: replace the single "Reseed on restart" with R/M/Q(/CA) items under "Reseed Policy".
- Tests: extend/mirror in a header-lite test — assert each lane reseeds iff its bool is set (handleRestart
  is engine-testable; the phase path is untouched).

### Part 2 — Raffles gains a Q-MIX COLUMN + reseed-on-restart lane gates (PANEL-COUPLED, Option B)
- Raffles today has rhythm + melody columns only. Add a THIRD (q-mix) column mirroring the R/M rows,
  AND the reseed-on-restart lane gates so R/M/Q are gate-modulatable (currently only ONE usable reseed
  gate exists: RESEED_RESTART; RESEED_ROLL is inert).
- Lockstep change: RafflesInputIds enum additions, `rafflesGateTrig[]` resize, `kRafflesGateAction[]`
  extension + static_asserts, `bindInput` calls + draw labels (MonsoonRafflesExpander.cpp), AND panel
  regen: gen_raffles.py + panel_src/layouts/raffles.json + regenerate Raffles_panel_{dark,light}.svg +
  gen/RafflesLayout.gen.hpp. (Container CAN run the Python generator; USER must build + eyeball SVG.)
- This is also the natural time to reconsider Raffles "Option B" (physically removing the dead
  Trial/LiveSrc/reseed-roll jacks) since the panel is being regenerated anyway.

### Part 3 — Change Alley panel gets its OWN reseed-on-restart modulation toggle
- A control on CA V2 that drives `reseedOnRestartCA` on its bound Monsoon(s). CA can serve MULTIPLE
  Monsoons (shared-CA pairing), so the semantics need deciding:
  - (a) drive only the OWNER Monsoon (the one calling applyPendingTransforms), or
  - (b) all bound Monsoons, or
  - (c) a per-CA flag the owner reads at restart (no push into Monsoon state).
  AND: is the CA control a latching toggle-button, a momentary gate input, or both?
- CA panel is generator-owned (gen_change_alley_v2.py) — adding a control is another lockstep
  generator+SVG change.

## OPEN QUESTIONS to resolve on resume
1. Sequencing: recommend Part 1 first (pure logic, self-verifiable, immediately usable via menu),
   then Parts 2 + 3 (panel-coupled, build+eyeball). Confirm order.
2. CA-toggle semantics (Part 3 a/b/c above) + control type (toggle vs gate vs both).
3. Whether Part 2 folds in the Raffles dead-jack removal (Option B) while the panel is regenerated.
4. Does CA get a gate-driven DieAction too, or menu/panel-toggle only?

## Note on scope discipline
Part 1 is a clean standalone commit. Parts 2 and 3 are panel + multi-module changes that must each
be their own commit, built and eyeballed by Rodney (container cannot build the plugin or verify SVGs).
