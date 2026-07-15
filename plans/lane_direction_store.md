# Plan: per-voice DIRECTION store — move the authority off the GUI

Direction is the only per-voice datum on East with **no store param**. LOR, attenuverters,
lane ownership and VAR/LEG delegation all have per-voice stores that
`MonsoonExpanderManager::sync()` reads module-side (from `Monsoon::process`). Direction
lives only in `engine.laneDirVPending_`, so the **widget** is the sole thing that can
populate it per voice — which is the root of both bugs in this arc and of the GUI
dependency.

## Why not a laneDirectionManager

Every bug here has been **two writers for one datum**: the widget's per-frame proxy push vs
the gate-mod write; mono's decision stamped on every voice; a replica pinned to a model the
engine had left behind. A new manager would be a *third* writer for data
`MonsoonExpanderManager` already owns — it is already the single "expander params → engine"
authority, already resets-then-pushes per voice, and already reads `ownerId(pv,eng)` and
`VARLEG_DELEG…`. Direction is the same shape of datum, so it belongs in the same push.
The fix is fewer authorities, not another class.

## Why the proxy exists (and must stay)

A Rack `ParamWidget` binds ONE `paramId` at construction — it cannot be rebound per tab. So
a per-voice control needs either 15 widgets or one widget on a display proxy plus a tab
copy. Hence `dirDispId(lane)` / `ownerDispId(lane)`. The proxy is not the problem; the
proxy being an **authority** is. After this plan it is a pure view.

## Target model

    store (per voice, persisted params)  ── manager sync() ──▶ engine   [module, audio thread]
        ▲                                        │
        │ cells / gate-mods write it             └─ never reads the proxy
        │
    proxy (display only) ◀── refreshed from the store for the selected voice each frame

Invariant: **the store is the truth for every voice; the proxy shows the selected one.**
Nothing pushes proxy → engine. Nothing special-cases "the current voice" outside the
refresh.

## Steps

1. **Add the store.** `MonsoonIds`: `LANE_DIR_START` = 96 params — `dirId(v,lane)` for
   15 voices × 6 lanes (v=0..14 = V2..V16), plus `monoDirId(lane)` for V1 in the spare
   slot v=15, mirroring `monoOwnerId`'s convention exactly. `configSwitch` 4-state
   (Forward/Reverse/Pendulum/PingPong), default Forward. Appended at END, as VARLEG_* were.
   NB the existing `dirDispId(lane)` (6) stays as the display proxy.

2. **Manager pushes it.** In `MonsoonExpanderManager::sync()`, beside the existing
   `setVarlegLocalEast` block: reset all lane dirs to Forward, then push store →
   `laneDirPending_[strand]` (from `monoDirId`) and `laneDirVPending_[v][strand]` (from
   `dirId(v,lane)`).
   **Push ONLY `laneDir*Pending_`, never `laneSign*`** — `advancePlayhead` derives the sign
   for Forward/Reverse and manages it internally for Pendulum/PingPong bounces; pushing the
   sign every frame would overwrite a bounce-induced flip with `laneDirSign(Pendulum)=+1`
   and undo the bounce. (This is why the widget push was already careful here.)

3. **Store becomes truth; proxy becomes a view.** Delete the per-frame proxy → engine push
   in `StraitsEastSandsVisualWidget::step()`. Replace with a refresh: store → proxy for the
   selected voice only. Same for Macro/Mono surfaces (each currently re-implements the push).

4. **Cells write the store.** `DirCell` writes `dirId(currentVoice, lane)` (or `monoDirId`
   on V1) directly; the proxy refresh follows. Precedent: `OwnerCell`'s all-voices action
   already writes `ownerId(v,lane)` for all 15 voices directly.

5. **Gate-mods write the store.** Then a mod needs no tab knowledge: the
   `selectedVoice == ch` rule disappears, and `applyGateMods()` can move back into the
   module (still on the `/8` divider) — mods work with the GUI closed. *That collapse is
   the check that the architecture is right.*

6. **Delete the special cases.** `saveVoiceMacro`/`loadVoiceMacro`'s direction parts and any
   "current voice is live-proxy" override for direction go away, since the store is truth.
   (Ownership keeps its override until step 4 is extended to it.)

## Order / gating

**The manager push is the switchover, NOT an inert add.** `sync()` runs at control rate from
`Monsoon::process`, far more often than the 60 Hz widget push — so if the manager pushed a
store that still defaulted to Forward while `DirCell` only wrote the proxy, it would clobber
the widget every frame and direction would stop working entirely. The store must be
*correct* before anything reads it. Hence:

- **A (inert)** — step 1 only: the store params exist, nothing reads them. Safe to land alone.
- **B (inert)** — step 4 + 5 as a *dual-write*: `DirCell` and the gate-mods write the store
  **in addition to** the proxy/engine they write today (the precedent is `OwnerCell`, which
  already writes proxy + all 15 `ownerId` slots). Still inert, because nothing reads the
  store yet — but now it is correct and agrees with the proxy. Also mirror store↔proxy in
  `loadVoiceMacro`/`saveVoiceMacro` so tab switching keeps them equal.
- **C (switchover)** — step 2 + 3: the manager pushes store → engine, and the widget's
  per-frame proxy → engine push is deleted. Safe by then, because store and proxy already
  agree, so the brief period where both write is last-writer-wins on the *same value*.
- **D (cleanup)** — step 6, plus dropping the now-dead dual-write of the proxy where the
  refresh covers it.

C is the one to audition. A and B are additive.

Do this **before** finalising direction+mod on Sands: Macro, Mono and East each
re-implement the proxy push today — three copies of the thing that broke twice; adding a
Sands surface on the current shape makes a fourth.

## Risks

- **Param count**: +96. Precedent — `MACRO_OWN` 64, `VARLEG_DELEG` 30, `VARLEG_ATTEN` 96.
  Appending at END keeps existing patches loadable.
- **Two indexing schemes**: `dirId` is poly-bank indexed (v=0 → V2) while the atten banks
  are 16-wide voice-slot indexed. This is exactly where off-by-ones breed — `dirId` follows
  `ownerId`/`varlegDelegId` (poly-bank), and V1 uses the spare slot like `monoOwnerId`.
- **Step 3 changes where edits land**; mostly mechanical but wants an ear/eye check.
