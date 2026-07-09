# Modulation clamping invariant — "sum all mods, clamp the end result once"

## The rule
When a value receives modulation from more than one source, **sum every term first, then clamp the
TOTAL exactly once**. Never clamp a partial sum and then add more on top.

```
// WRONG — order-dependent, silently discards headroom
v = clamp(base + eastCv);      // east pushes to 1.4 → truncated to 1.0
v = clamp(v + macroSend);      // negative macro now pulls down from 1.0, not 1.4

// RIGHT — order-independent, preserves the net
v = clamp(base + eastCv + macroSend);
```

Per-term clamping means `East-then-Macro != Macro-then-East`, and a mod that overshoots the range
loses the overshoot permanently, so an opposing mod can't pull it back.

**Legitimate clamps that are NOT violations:**
- Clamping a *source* to its own physical range (e.g. a ±5V input read as ±5V, a knob to its param range).
- A single end-clamp at the consumption point.
- Mutually-exclusive branches that each end-clamp (e.g. delegated vs owned).

## Audit (2026-07, after Rodney: "we should add mods and clamp end result always")

| Site | Sources summed | Status |
|---|---|---|
| `SpreadResolver::effective` (mono spread) | own base + own CV + East CV + Macro send | ✅ already end-clamped (`clampSpread` fix on master) |
| `ParameterManager::getNoteValue/Variation/Legato` | knob + CV2 + Junction | ✅ single clamp, nothing added after |
| Mono rest/accent | knob + CV2 + Junction + **Causeway ch0** | ❌→✅ **fixed**: was double-clamped; added `getRest/AccentUnclamped()`, single end-clamp in `getEffectiveMono*` |
| `SandsManager` mono LOR | base + East CV + Macro send | ❌→✅ **fixed**: `eastMix`/`macroMix` each clamped → now `eastDelta`/`macroDelta` return unclamped deltas, one end-clamp |
| `SandsManager::sprForLane` (mono spread, East-owned) | base + East CV + Macro send | ✅ already summed then end-clamped |
| `ExpanderManager::combineSpread` (poly spread ×4 lanes) | East CV + Macro blend | ❌→✅ **fixed**: East CV was clamped before `combineSpread` added the Macro blend and clamped again; intermediate clamp removed |
| `ExpanderManager::combineLOR` (poly LOR) | base + Macro blend | ✅ single end-clamp |
| CV2 / Junction offset **writers** | `cv * att` stored raw | ✅ unclamped at store; getters sum then clamp once |

## Why it keeps recurring
The codebase composes modulation across *layers* (param manager → expander manager → effective getters),
and each layer historically clamped defensively "to be safe". Defensive clamping is exactly wrong for a
value that a later layer will add to. The safe habit is: **only the layer that CONSUMES the value clamps
it.** Intermediate layers return unclamped sums (or deltas).

Prefer returning **deltas** (0 when the source is absent) from per-source helpers, so the caller can sum
them and clamp once — see `eastDelta`/`macroDelta` in `MonsoonSandsManager`.
