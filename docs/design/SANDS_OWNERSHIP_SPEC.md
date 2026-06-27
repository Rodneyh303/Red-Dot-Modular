# Sands Ownership & Delegation — authoritative behaviour spec

This is the canonical spec for how lane ownership, delegation, control-locking,
and modulation display behave across every Sands module combination. It
supersedes the ownership sections of `SANDS_COMBINATIONS.md` and
`SANDS_LANE_OWNERSHIP_UX.md` where they conflict. Implementation is tackled in
pieces (see the checklist at the end).

---

## Vocabulary

- **Lane**: one of the per-strand controls. Poly lanes are MEL/OCT/REST/ACC.
  Mono additionally has VAR/LEG (mono-only, never delegable).
- **LOR**: the Length / Offset / Rotation triple per lane. "LORs" loosely also
  includes that lane's **spread** unless called out separately.
- **Owner cell**: the per-lane SRC cell (Option C, treatment A). Two display
  states: **outline** = local surface owns the lane; **filled** = delegated to
  Macro (global) owns the lane.
- **Delegate**: set a lane's owner cell so its base LOR+spread come from Macro's
  global base instead of the local (East/Mono) edit.
- **Inoperable**: the control is shown but cannot be changed by the user
  (read-only display); it tracks whatever drives it. Visually locked.
- **Mod arc**: the ring overlay around a knob showing the modulated (effective)
  value vs the set value.

### The owner cell has THREE possible conditions
1. **Outline, operable** — local owns; user edits freely. (No Macro present, or
   Macro present and lane not delegated.)
2. **Outline, locked (inoperable)** — local owns but the cell itself can't be
   toggled because there's no Macro to delegate to. Lane LOR still editable.
3. **Filled, locked (inoperable)** — delegated to Macro. Lane LOR+spread show
   Macro's values and are inoperable locally.

> Note the distinction between **the owner cell being inoperable** (can't change
> who owns) and **the lane controls being inoperable** (can't change the values).
> Case 2 = cell locked but lane editable. Case 3 = cell locked AND lane locked.

---

## Global rules (apply everywhere)

- **G1 — No hiding.** Never hide lane 1 / the V1 tab, and never hide spread
  controls, on any Sands surface. (Removes the old `tab1MonoMirror` hide and any
  spread-hiding.) Controls may be *locked/inoperable* but are always *visible*.
- **G2 — Owner cells require Macro.** Delegation only means anything when Macro
  is attached. With no Macro, owner cells are **outline + locked** (condition 2):
  there is nothing to delegate to.
- **G3 — Macro is always authoritative for its own view.** When Macro is
  present it always shows its own global LOR base + its own modulation; Macro's
  lane controls are always operable on Macro itself.
- **G4 — Mono wins V1 vs East.** For voice 1, Mono is the authority over East.
  East's V1 base follows Mono and is inoperable on East. (Only Macro can take V1
  from Mono, and only via Mono's own delegation — East can never delegate V1.)
- **G5 — Delegated lane = inoperable locally, tracks Macro.** A lane delegated
  to Macro shows Macro's LOR **and** spread, locked, on the delegating surface.
- **G6 — Mod arcs show the modulation *applied at that surface*.** Macro's arcs
  show the mod applied to Macro; a local surface's arcs show the mod applied
  locally; where modulation sums across surfaces, the surface that is the final
  owner shows the *total*. (See per-combo notes for V1.)

---

## The 8 attachment combinations
(Macro / East / Mono each 0-or-1.)

### 1. Mono only
- Mono: full 6-lane editing, all operable.
- **Owner cells: outline + LOCKED (inoperable)** — no Macro to delegate to (G2).
- Spread controls visible + operable.

### 2. (covered by G1 — no hiding anywhere; not a standalone combo)
*(The user's item 2 is the global no-hide rule G1, not a combination.)*

### 3. East only
- East: can edit all 4 poly lanes **including lane 1** (V1). With no Mono, East's
  V1 tab is operable (this resolves the old "read-only V1" open question — V1 is
  editable on East when neither Mono nor Macro claims it).
- **Owner cells: outline + LOCKED (inoperable)** — no Macro (G2).
- Spread visible + operable.

### 4. Macro only
- Macro: can control all 4 lanes; LOR base + spread operable on Macro.
- **Send controls have NO EFFECT** in this combo (nothing to send into — no East/
  Mono voices consuming the mix-in). Sends are shown but inert. *(This is the
  current intended behaviour; item 9 adds the PRE/POST tap to sends generally.)*

### 5. Macro + East
- East may **delegate any lane to Macro, including lane 1** (no Mono present, so
  East owns V1 and may cede it).
- Macro always shows Macro's LOR base + modulation, operable on Macro.
- **Delegated lanes on East**: LOR **and spread** show Macro's values, track
  Macro live, and are **inoperable** on East (G5). Owner cell filled+locked.
- Non-delegated lanes on East: outline, operable.

### 6. Macro + Mono
- Lane-1 handling mirrors Macro+East but for Mono's V1.
- Macro always shows Macro's LOR on lane 1 (and all lanes), operable on Macro.
- **Mono** shows Macro-or-Mono settings per lane based on its owner cell:
  - Delegated: Mono shows Macro's LOR + **spread**, inoperable, tracking Macro.
  - Not delegated: Mono shows its own, operable.
- **Mod arcs**: Macro's arcs show the modulation applied to Macro; Mono's arcs
  show the modulation applied to Mono (G6).

### 7. East + Mono (no Macro)
- **Both surfaces: owner cells outline + LOCKED (inoperable)** — no Macro (G2),
  so no delegation possible.
- **Mono wins V1** (G4): 
  - Mono edits V1 fully (all 6 lanes operable).
  - **East's V1 base LOR follows Mono and is inoperable on East.**
  - Any V1 LOR **modulation** (CV) arriving at East **adds to** the V1
    modulation (East CV is summed on top of Mono's base for V1).
  - **V1 mod arcs**: on **East** reflect the mod *coming into East*; on **Mono**
    reflect the *total* mod (Mono base mod + East's added CV) (G6).
- East V2–V16: fully operable as normal.

### 8. All three (Macro + East + Mono)
A composition of 5 + 6 + 7:
- Both East and Mono can delegate lanes to Macro, **but East can NOT delegate
  V1** (Mono owns V1; East's V1 always follows the V1 authority) (G4).
- East shows lane 1 as in combo 7 (follows Mono; V1 base inoperable on East; V1
  CV into East adds to V1 mod; arcs per G6).
- Macro lanes always show the Macro view; **Macro mod arcs reflect the mod on
  Macro**.
- A lane delegated to Macro — V1 via **Mono**, or V2–V16 via **East** — follows
  Macro's LOR **including spread** and is **inoperable** from Mono/East.

---

## 9. Macro send PRE/POST tap (separate feature)
Each Macro→voice mix-in send gets a **tap control** choosing whether the send
draws **PRE** or **POST** the left attenuverters:
- Option A: a 2-way switch (PRE / POST).
- Option B: a trimpot that **mixes** PRE↔POST continuously.
Decision pending; switch is simpler, trimpot is more expressive. Applies to the
send path in the Macro bottom send group.

---

## Cross-cutting display/lock matrix (quick reference)

| Combo | Owner cell state | V1 owner | Delegable lanes | Locked-inoperable lanes |
|-------|------------------|----------|-----------------|--------------------------|
| Mono | outline+locked | Mono | none | none (cells locked only) |
| East | outline+locked | East | none | none (cells locked only) |
| Macro | n/a (Macro view) | Macro | all (on Macro) | sends inert |
| Macro+East | toggleable | East→Macro ok | all incl V1 (East) | delegated lanes on East |
| Macro+Mono | toggleable | Mono→Macro ok | all incl V1 (Mono) | delegated lanes on Mono |
| East+Mono | outline+locked | Mono | none | East V1 base |
| All 3 | toggleable | Mono (East never V1) | East: V2-16; Mono: V1 | delegated lanes + East V1 |

---

## Implementation checklist (tackle in pieces)
Legend: [x] merged/verified · [~] implemented, pending final build-verify · [ ] todo.

- [x] **P1 — Global no-hide (G1).** Remove `tab1MonoMirror` hiding of V1/owner
  cells and any spread hiding on East/Mono/Macro. Controls stay visible.
- [x] **P2 — Owner-cell lock states.** Implement the 3 conditions (outline-operable,
  outline-locked, filled-locked). Cell is locked (inoperable) whenever no Macro
  is attached; filled-locked when delegated. Visual lock affordance.
- [x] **P3 — East V1 editable when standalone (combo 3).** Allow V1 LOR editing on
  East when no Mono present; make V1 base inoperable on East when Mono present
  (combo 7/8), following Mono.
- [x] **P4 — Delegated-lane lock + track (G5).** On East/Mono, a delegated lane's
  LOR+spread become inoperable and display Macro's values live.
- [~] **P5 — V1 East-follows-Mono + additive mod (combo 7/8, G4).** East V1 base =
  Mono's; East V1 CV adds to V1 mod; arcs split (East=incoming, Mono=total) (G6).
- [~] **P6 — Mod-arc sourcing (G6).** Ensure each surface's arcs read the mod
  applied at that surface; final-owner surface shows total.
- [x] **P7 — Macro sends inert when standalone (combo 4).** VERIFIED by code:
  macroActive = hasMacro && polyBaseActive, and polyBaseActive requires the East
  BASE expander (cachedPolyVoiceExpander) + numPolyVoices>=1. Standalone Macro →
  macroActive false → publishGlobal never runs → macroCVDelta stays 0 → every send
  (base + macroCVDelta*send) contributes 0. Sends shown but inert. No code needed.
- [x] **P8 — East cannot delegate V1 (combo 8, G4).** V1 owner cell on East is
  locked to "follow V1 authority"; only Mono's cell can delegate V1.
- [ ] **P9 — Send PRE/POST tap (item 9).** Switch or mix trimpot on Macro sends.

> Each P-item is a discrete, build-verifiable change. Recommend order:
> P1 → P2 → P3 → P4 → P5/P6 (together) → P8 → P7(verify) → P9.
