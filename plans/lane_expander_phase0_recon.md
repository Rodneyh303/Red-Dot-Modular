# Lane Expander — Phase 0 Recon

## Current Straits structure (what to split)

**Module**: `MonsoonStraitsExpander` (slug `"Straits"`, 34HP)
**Widget**: `MonsoonStraitsExpanderWidget` in [`MonsoonStraitsExpander.cpp`](../src/MonsoonStraitsExpander.cpp)
**Panel gen**: [`gen_straits.py`](../panel_src/gen_straits.py) — 34HP, 3 banks side-by-side

### Lane banks (→ LaneExpander)
3 banks × 16 per-voice knobs = 48 knobs total:
- **REST**: `param_rest_<0..15>` → `REST_PARAM` (mono) + `POLY_REST_PARAM_1+i` (poly)
- **ACCENT**: `param_accent_<0..15>` → `ACCENT_KNOB` (mono) + `POLY_ACCENT_PARAM_1+i` (poly)
- **QMIX**: `param_qmix_<0..15>` → `QMIX_LEVEL_PARAM` (mono) + `POLY_QMIX_PARAM_1+i` (poly)

Each bank is IDENTICAL in structure — only the data differs (see Descriptors below).

### Frame + IO (→ base)
- **5 poly-cable outputs**: `POLY_GATE_OUT`, `POLY_STEP_GATE_OUT`, `POLY_STEP_LEGATO_GATE_OUT`, `POLY_CV_OUT`, `POLY_ACCENT_OUT`
- **1 poly CV input**: `QUANT_CV_INPUT` (16ch quantiser CV)
- **1 voice-count param**: `VOICE_COUNT_PARAM` (stepped 1..16)
- Screws, connect mark, panel frame
- `process()` is empty — Monsoon writes the poly outputs via cached pointer

### Panel layout (gen_straits.py)
- 3 banks, each = 3 cols × 6/6/4 rows (col-major), `BANK_W ≈ 9mm`, `GAP = 6mm`
- `GRID_TOP = 18mm`, `ROW_H = 14.77mm`, `KNOB_R = 4.5mm`
- `JACK_Y = 111.1mm` (outputs + input below the knob grid)
- Shared bg rect, red top stripe (1.2mm), per-bank wave fields + tint bands, divider rails in gutters

## Lane Descriptors (what differs per lane = pure data)

| Field | REST | ACCENT | QMIX |
|-------|------|--------|------|
| anchorPrefix | `"param_rest_"` | `"param_accent_"` | `"param_qmix_"` |
| monoParamId | `REST_PARAM` | `ACCENT_KNOB` | `QMIX_LEVEL_PARAM` |
| polyParamIdBase | `POLY_REST_PARAM_1` | `POLY_ACCENT_PARAM_1` | `POLY_QMIX_PARAM_1` |
| arcLane | 0 | 1 | 2 |
| monoBaseFn | `getMonoRestBase` | `getMonoAccentBase` | `getMonoQmixBase` |
| monoEffectiveFn | `getRestParam` | `getAccentParam` | `getQmixParam` |
| polyBaseFn | `getBasePolyRest` | `getBasePolyAccent` | `getBasePolyQmix` |
| polyEffectiveFn | `getEffectivePolyRest` | `getEffectivePolyAccent` | `getEffectivePolyQmix` |
| colours | `rest/restwave/restknob` | `acc/accwave/accknob` | `qmix/qmixwave/qmixknob` |

Everything else (knob type `Themed_Compact_Cog_Dim`, dim/lock-when-inactive, mod-arc overlay, theme following, voice-count interaction) is IDENTICAL across all 3 lanes.

## Pointer/discovery infra to reuse

- [`findMonsoonEitherSide()`](../src/ui/VisualExpanderHelpers.hpp:34) — both-sides host search via segment rule (already used by Straits)
- [`MonsoonDiscovery`](../src/ui/MonsoonDiscovery.hpp) — segment-rule walk, host classification
- `MonsoonExpanderManager` — caches expander pointers, runs per-block sync
- Lane expanders use the SAME `findMonsoonEitherSide` to find Monsoon (as current Straits does)
- Base resolves right-neighbour chain via `rightExpander` (plain Rack expander chaining)
- No new discovery infra needed — reuse existing

## Seamless-abutment mechanics

Each lane expander panel = ONE bank width (~9mm + gap). For seamless render:
- Same `bg` colour, same `GRID_TOP`, same `ROW_H`, same knob grid (3 cols × 6/6/4)
- Divider rail on the lane expander's LEFT edge
- Wave field + tint band continues across (each expander carries its own)
- Base panel = frame + IO + voice count (narrow, ~10-12HP)
- The per-voice knob-row Y is aligned by construction (same `GRID_TOP` + `ROW_H`)

## Mono-default read (no expander → mono broadcast)

Currently: voice-0 knob mirrors Monsoon's mono param; poly voices have their own params on Monsoon. Without Straits, params still exist (at defaults) and the engine reads them.

For the lane expander model: the lane expander's presence is detected via the base's right-neighbour chain. When present, per-voice params are used. When absent, the engine reads the mono param for ALL voices (mono-broadcast). This is analogous to the existing "delegated to mono" behavior in Sands VAR/LEG.

**Key insight**: the per-voice params (`POLY_REST_PARAM_1` etc.) live on Monsoon, not on Straits. The lane expander is purely a UI panel that binds to those params. So the "mono-default" is about whether the engine reads per-voice params or falls back to mono — this is a policy in `MonsoonExpanderManager`/`MonsoonSandsManager`, not in the lane expander itself.

## Gap: VARIATION / LEGATO lane expanders

No `POLY_VARIATION_PARAM` or `POLY_LEGATO_PARAM` exists in `Monsoon.hpp`. The Sands consolidation added per-voice VAR/LEG DRAWS (engine arrays), but not per-voice VAR/LEG probability KNOBS. Adding these would be engine work — deferred beyond Phase 1/2.

Phase 1: QMIX only (as doc specifies). Phase 2: REST + ACCENT (existing params). VARIATION/LEGATO: deferred until per-voice params exist.

## Minimal one-lane scaffold (Phase 1)

1. **`LaneDescriptor` struct** — the data table above as a constexpr/struct
2. **`LaneExpanderModule`** — minimal Module subclass (empty `process()`, same as current Straits)
3. **`LaneExpanderWidget`** — copies the per-lane knob binding loop from `MonsoonStraitsExpanderWidget`, parameterised by descriptor
4. **`StraitsBaseModule` + `StraitsBaseWidget`** — frame + IO + voice count, extracted from current Straits (remove lane knob bindings, keep IO + voice count + connect mark)
5. **Panel generators** — `gen_straits_base.py` (frame + IO) + `gen_straits_lane.py` (one bank, parameterised by lane colours)
6. **Registration** — new slugs: `"StraitsBase"` + `"StraitsLaneQMIX"` (or generic `"StraitsLane"` with a lane-id param)
7. **Old Straits stays registered** — `"Straits"` unchanged, safety net

## Phase 1 Plan

```mermaid
flowchart LR
    A[Copy Straits .cpp/.hpp] --> B[Split: base = frame+IO, lane = knob loop]
    B --> C[Extract LaneDescriptor from bank differences]
    C --> D[New panel gens: base + one-lane]
    D --> E[Register new slugs alongside old Straits]
    E --> F[Test in Rack: base+QMIX seamless, per-voice QMIX works]
    F --> G[STOP for approval]
```

### Steps
1. Create branch `lane-expander-refactor` off master
2. Copy `MonsoonStraitsExpander.cpp/.hpp` → `StraitsBase.cpp/.hpp` + `StraitsLaneExpander.cpp/.hpp`
3. In the base copy: remove all lane knob bindings (lines 124-194), keep IO + voice count + connect mark + step() logic
4. In the lane copy: extract the knob binding loop into a descriptor-parameterised form; keep ONLY the per-lane knob loop + mod-arc overlay
5. Extract `LaneDescriptor` struct (the data table above)
6. New panel gens: `gen_straits_base.py` (frame + IO, ~12HP) + `gen_straits_lane.py` (one bank, ~11HP, parameterised by lane colours/label)
7. Register new slugs in `plugin.cpp` + `plugin.json`: `"StraitsBase"` + `"StraitsLaneQMIX"`
8. Test in Rack: base+QMIX docked right = one seamless panel, per-voice QMIX knobs work, old Straits still there
9. STOP for approval before scaling to REST/ACCENT

### What NOT to do in Phase 1
- Don't touch the old Straits (`MonsoonStraitsExpander.cpp/.hpp`) — stays registered and working
- Don't touch Sands/engine code — additive only
- Don't write new per-voice-read or binding logic — COPY from the working Straits
- Don't add VARIATION/LEGATO yet — no params exist for them
- Don't implement context-menu add/remove — that's Phase 3
