# Plan: one rule for lane-addressing data — the editor owns it, the engine derives it

Supersedes `lane_direction_store.md` and `lane_direction_authority.md`, both of which were
written on wrong premises (see "History of wrong turns" — it is the useful part).

## The rule

> Every lane-addressing datum's home is the **expander that edits it**. The engine is a
> **derived cache**, rebuilt each `MonsoonExpanderManager::sync()` by reset-then-push.
> Nothing lane-addressing lives in Monsoon's JSON.

Verified state of play:

| datum | home (persisted) | engine role |
|---|---|---|
| mono LOR / atten / spread | Mono expander params (`lenId`/`offId`/`rotId`/`attenId`/`sprId`) | derived (`lorRef` is only ever written in `reset()`) |
| poly LOR / atten / spread | East params | derived cache |
| lane delegation — `ownerId(v,lane)`, `monoOwnerId(lane)`, `varlegDelegId(v,lane)` | East params | derived (`setVarlegLocalEast`, topology) |
| **direction (mono + poly)** | **Monsoon JSON** ← the exception | **home** ← the exception |

Delegation is the model citizen. Direction is the only lane-addressing datum that never got
the treatment — it is engine-homed and Monsoon-persisted, while the thing that *authors* it
is an expander.

## Why that's a real bug, not a taste question

`sync()` resets delegation and LOR for all 15 voices before the East block, so pulling East
out of the rack drops them. **Nothing resets `laneDirV_`.** Pull East and poly direction
stays applied to the engine forever, and reloads from JSON next session — a ghost config
from a module that no longer exists. Mono direction has the same shape via `laneDir_`.

The Monsoon context-menu direction options are **pre-GUI dev scaffolding**, not a reason to
keep direction on the engine — and keeping them would be a second authority for one datum,
which is the bug this arc has now hit three times.

## Target

- **poly direction** → East params, `dirId(v,lane)` (v = 0..14 = V2..V16), manager
  reset-then-pushes into `laneDirVPending_` exactly as it does delegation.
- **mono direction** → Mono expander params, beside mono LOR.
- **`laneDir` + `laneDirV` come out of Monsoon's JSON.**
- **Delete the Monsoon direction context menu** (scaffolding).
- Gate-mods write the owning expander's param directly — module-side, tab-free. The
  `selectedVoice == ch` rule dies and `applyGateMods()` moves back into the module.
- The display proxy stops being an authority *for free* — the param is the home and the
  manager reads it, so the change-detect fix proposed in the previous draft is unnecessary.
  (That it becomes unnecessary is the check that this shape is right.)

## The V1 wrinkle

When East is the mono editor (no Mono attached, `tab1MonoMirror()` false), mono lane data
has no persisted home — East writes the engine's mono strands directly each frame, and by
its own comment "there is no per-voice bank to persist". Mono direction would inherit that.

**Fix:** give East a V1 direction slot, `monoDirId(lane)`, in the spare slot of its own
direction bank — precisely the trick `monoOwnerId` already uses so V1 *delegation* persists
in the patch like poly voices do. So the direction bank is 16 slots × 6 strands = 96:
slots 0..14 = V2..V16, slot 15 = V1. The manager then picks the mono source the same way the
editors already pick the mono authority: Mono expander if attached, else East's V1 slot.

## Steps

1. **East direction bank (inert).** `MonsoonIds::LANE_DIR_START`, 96 params = 16 slots × 6
   strands, appended at END so existing patches load. 4-state `configSwitch`
   (Forward/Reverse/Pendulum/PingPong), default Forward. Accessors `dirId(v,lane)`,
   `monoDirId(lane)`. Nothing reads it yet.
2. **East cells + gate-mods write the bank** (still inert — nothing reads it). `DirCell` is a
   `ParamWidget` bound to the proxy and cannot know the voice, so persist on change in
   `step()`; gate-mods write `dirId(ch-1, lane)` / `monoDirId(lane)` directly.
3. **Switchover (audition).** Manager reset-then-pushes the bank → `laneDirVPending_`;
   delete the widget's unconditional proxy → engine push; drop `laneDirV` from JSON.
4. **Mono half.** Direction params on the Mono expander beside its LOR; manager pushes them
   to `laneDirPending_`; drop `laneDir` from JSON; delete the Monsoon menu.
5. **Cleanup.** Drop the `selectedVoice == ch` rule; move `applyGateMods()` into the module.

**Ordering is load-bearing** (this is what the first draft got wrong): `sync()` runs at
control rate, far more often than the 60 Hz widget push, so step 3 before step 2 would push a
still-default bank over the widget every frame and kill direction outright. The bank must be
*correct* before anything reads it.

## Invariants

- Push **only** `laneDir*Pending_`, never `laneSign*`. `advancePlayhead` derives the sign for
  Forward/Reverse and **owns** it for Pendulum/PingPong (flipping at the LOR endpoint);
  pushing a sign each frame would overwrite the bounce flip with `laneDirSign(Pendulum)=+1`
  and the lane would never turn around.
- One datum, one persisted home.
- A view never writes its model on a timer.
- `dirId` is poly-bank indexed (v=0 → V2), like `ownerId`/`varlegDelegId` — *not* the 16-wide
  voice-slot scheme the atten banks use. Two parallel schemes is where off-by-ones breed.

## History of wrong turns (keep — the reasoning is the value)

1. **"Direction has no store; add 96 East params + a manager push."** False: it round-trips
   in Monsoon's JSON (`laneDirV`). Would have created a *second persisted home* — the exact
   bug it claimed to prevent. Reverted.
2. **"So the engine is the right home; just change-detect the push."** Half true. The engine
   is right for a datum Monsoon authors — but the Monsoon menu is dev scaffolding, so nothing
   Monsoon-side authors direction. Once that fell, the engine had no claim.
3. **The real question was never "does it have a store"** but **"who authors it, and does its
   lifetime match its editor's?"** Direction is expander-authored, so it must be
   expander-homed — like every other lane-addressing datum.

Lesson: check where a datum is *persisted*, and *who authors* it, before choosing its home.
