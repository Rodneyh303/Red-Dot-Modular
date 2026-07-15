# Plan: take the GUI out of the direction authority chain

Supersedes an earlier draft of this file, which was built on a false premise — see
"What the first draft got wrong" below. Read that first; it is the whole point.

## The actual model

Direction's store **already exists: it is the engine**, persisted in Monsoon's own JSON
(`laneDirV`, 15 voices × 6 strands, saved *and* restored into
`laneDirV_`/`laneDirVPending_`; mono in `laneDir`, with a legacy `laneSign`+`lanePendulum`
fallback). That is the right home, because:

- direction is a Monsoon/engine concept, not East's data — Monsoon's own context menu sets
  it (`MonsoonWidget.cpp`), and it must persist with **no East attached**;
- East / Macro / Mono are just *editors* of it, exactly like they are editors of the engine's
  mono strands.

Contrast with LOR / attenuverters / lane ownership / VAR/LEG delegation: those are **East's**
data, so East params are their home and `MonsoonExpanderManager::sync()` pushes them into
the engine. Direction is the other way round — the engine owns it, so nothing should push
it *from* East params.

## What the first draft got wrong

It claimed "direction is the only per-voice datum on East with no store" and proposed 96
new East params (`dirId(v,lane)`) plus a manager push. That was wrong on both counts:

- **It already has a store** (engine + Monsoon JSON, above). East params would have been a
  **second persisted home** for one datum — precisely the two-writers-for-one-datum bug this
  whole arc has been about, and the thing the plan claimed to prevent.
- **East cannot be the home anyway**: mono direction has to survive with no East in the rack.

The step was implemented, caught at the persistence layer, and reverted before landing.
Cost of the lesson: check where a datum is *persisted* before declaring it homeless.

## The real problem

Not "no store" — **the widget's per-frame push is unconditional**:

    step():  engine.laneDirVPending_[pv][strand] = params[dirDispId(lane)]   // every frame

That single line makes the display proxy an *authority*, and it is the direct cause of both
direction bugs this arc:

- a gate-mod writing the engine for the **displayed** voice is overwritten the next frame
  from the stale proxy ("works on every voice except the one I'm looking at");
- so the mod is forced to write the proxy instead — but the proxy holds only the *selected*
  target, so a mono (ch0) pulse then smears mono's direction onto whatever poly tab is open.

Every workaround (`selectedVoiceMirror`, the `selectedVoice == ch` rule) is a consequence of
that one unconditional write.

## The fix: change-detect the push

The proxy exists only because a Rack `ParamWidget` binds ONE `paramId` at construction, so a
per-voice control needs one widget + a proxy + a tab copy. Fine — but a *view* must not
overwrite its model on a timer. So:

    proxy changed since last frame ?  ->  user edit: push proxy -> engine (current voice)
    otherwise                        ->  pull engine -> proxy (display follows the truth)

That is the entire change. It yields:

1. **The engine is the single authority.** The proxy becomes a pure view.
2. **Gate-mods always write the engine, never the proxy, and never need the tab.** The
   `selectedVoice == ch` rule disappears; `applyGateMods()` can move back into the module
   (still on the `/8` divider) — so mods work with the GUI closed. *That collapse is the
   check that this is right.*
3. **No new params, no second persisted home, no migration.**

## Steps

1. **East** (`StraitsEastSandsVisualWidget::step()`): replace the unconditional
   proxy → engine push with the change-detect above. Keep a `lastDirDisp[6]` and a
   first-frame init that adopts the current value rather than treating it as an edit.
   Tab switch already writes the proxy from the engine — that path must set `lastDirDisp`
   too, or it will read as a user edit and push back.
2. **Macro / Mono**: same treatment — each currently re-implements the same unconditional
   push (`StraitsSandsMacroVisual.cpp`, `MonsoonSandsVisualExpander.cpp`).
3. **Simplify the mods**: drop the "displayed" test, always write the engine; move
   `applyGateMods()` back into the module.
4. **Guard it**: assert/verify that all-forward + no edits leaves `laneDirVPending_`
   untouched frame to frame (the push should be silent when nothing changed).

## Invariants

- Only `laneDir*Pending_` is ever pushed — never `laneSign*`. `advancePlayhead` derives the
  sign for Forward/Reverse and **owns** it for Pendulum/PingPong (flipping at the LOR
  endpoint); pushing a sign each frame would overwrite the bounce flip with
  `laneDirSign(Pendulum) = +1` and the lane would never turn around.
- A view never writes its model on a timer — only on a real edit.
- One datum, one persisted home. Check the persistence layer before adding a store.
