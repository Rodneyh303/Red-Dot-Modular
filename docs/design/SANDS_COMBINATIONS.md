# Sands Module Combinations

Sands has three visual expanders: **Mono** (the 6-lane single-voice editor), **East** (the poly LOR/CV expander), and **Macro** (the poly blend/send expander). Each is 0 or 1 instance, giving 8 combinations.

---

## Module roles

| Module | Primary role |
|--------|-------------|
| **Sands Mono** | Edit the mono master strand (V1). 6 lanes: Melody, Octave, Rest, Accent, Variation, Legato. Per-lane LOR (Length/Offset/Rotation), spread base + CV + atten. Drives voice 1 only. |
| **Sands East** | Per-voice poly LOR editing. 4 lanes: Melody, Octave, Rest, Accent. Per-lane CV inputs + attens + spread per poly voice (V2-V16). Tab V1 mirrors Mono when attached. |
| **Sands Macro** | Per-voice poly blend/mix-in. 4 lanes. Global base LOR knobs + per-voice send 2×2 grids. Read-only probability display. Ownership latches determine whether a lane's base comes from Macro or East. |

---

## The 8 combinations

### 1. None attached
Sequencer runs with internal defaults. No visual editing. Pattern engine ticks; probability outputs zero.

---

### 2. Mono only
- **V1**: Full 6-lane editing. Melody/Octave/Rest/Accent/Variation/Legato visible and live.
- **V2-V16**: No visual editing. Poly voices run from internal defaults (uniform probability).
- Spread: Mono's per-lane spread applied to V1 only.
- No tab switching — Mono is a single-voice editor.

---

### 3. East only
- **V1 tab (mono tab)**: Editor shown but lanes are **read-only display** — mirrored from internal engine state, no Mono to edit from. LOR knobs on East are dimmed/locked on this tab.
- **V2-V16 tabs**: Full per-lane LOR editing. CV jacks + attens live. Spread trimpot per lane per voice. Owner latches inert (no Macro attached — East owns everything).
- Prob outs: 4 poly probability outputs (Melody/Octave/Rest/Accent).

> **Open question:** Should V1/mono tab allow LOR editing directly on East when no Mono is attached? Currently read-only (mirrors engine state). Could be made editable as a lightweight mono-lane editor.

---

### 4. Macro only
- **V1 tab**: Global base LOR knobs active. Mono mirror read-only (no Mono). Left attens hidden on V1 tab.
- **V2-V16 tabs**: Global base LOR shown. Per-voice send 2×2 mix-in grids editable. Prob display updates per tab.
- East not present → ownership concept meaningless. Macro's global base is the only LOR source. Mix-in sends active against that base.
- Spread: global per-lane spread trimpot applies.

> **Open question:** Same as East only — should V1 tab allow mono-lane LOR editing when Mono is not attached?

---

### 5. Mono + East (no Macro)
- **V1 tab on East**: Mirrors Sands Mono's LOR. East's left knobs dimmed/locked — Mono owns V1. Display-only.
- **V2-V16 tabs**: Full per-voice LOR editing on East. Independent spread per voice per lane.
- Owner latches on East: inert (no Macro, East owns all lanes on all poly voices).
- Prob outs: full 4-lane poly probability outputs.
- This is the most natural poly setup: Mono for V1, East for poly.

---

### 6. Mono + Macro (no East)
- **V1 tab on Macro**: Mirrors Sands Mono. Left attens hidden.
- **V2-V16 tabs**: Macro global base visible. Send grids active. No per-voice LOR editing (East absent).
- Without East, Macro's global base is the only LOR source for poly voices. Send mix-in modulates around it.
- Ownership latches: not shown (no East to transfer ownership to/from).

---

### 7. East + Macro (no Mono)
- **V1 tab**: Mirrors internal engine state (no Mono). East LOR dimmed. Macro attens hidden.
- **V2-V16 tabs on East**: Full per-voice LOR + CV editing. Owner latches live.
- **V2-V16 tabs on Macro**: Global base + send grids editable. Read-only prob display.
- Owner latches on East: **active**. Per lane per voice, toggling between Macro base (global LOR + sends) and East base (East's own LOR).
- This is the full poly editing setup without mono voice control.

> **Open question:** V1 tab — should East allow mono-lane LOR editing when Mono is absent? Macro's global base doesn't reach V1 under the current interp-Y design. East editing on V1 would give the mono strand some poly-style LOR control.

---

### 8. Mono + East + Macro (full)
- **V1 tab**: Mirrors Sands Mono on both East and Macro. East knobs dimmed. Macro attens hidden. Read-only display of Mono's lanes.
- **V2-V16 tabs on East**: Full per-voice LOR + CV editing. Owner latches live.
- **V2-V16 tabs on Macro**: Global base + per-voice send grids. Read-only prob display.
- Owner latches on East: per lane per voice — toggle between Macro-owned (global base + sends) and East-owned (East's LOR). Macro-owned lanes show the resolver-driven probability in East's editor (display overwrite). East-owned lanes show East's own edit values.
- Full system: Mono drives V1, East drives poly LOR per voice, Macro blends global vs per-voice LOR with send mixing.

---

## Open design questions

### A. Mono-lane editing on East/Macro V1 tab without Mono
Currently V1/mono tab on East and Macro is **read-only** when Mono is absent — it mirrors the internal engine state but offers no editing surface. In combinations 3, 4, and 7 (East-only, Macro-only, East+Macro) the user has no way to edit V1's lane patterns.

**Option 1 (current):** Keep V1 read-only. User must add Mono to edit voice 1.

**Option 2:** When Mono is absent, make East's V1 tab fully editable as a lightweight mono editor. East's LOR knobs activate on V1 tab; the values drive V1 directly (not just poly voices 2-16). This would make East+Macro alone a complete editing surface.

**Tradeoff:** Option 2 adds complexity to the "what does V1 edit?" question and the sync path. It also changes the relationship between East and Mono — East would become a superset of Mono's LOR editing capability.

---

### B. Lane ownership UX on East
Currently ownership (East vs Macro base per lane per voice) is toggled via a small latch button in the lower blend section of East. With 4 lanes × up to 15 poly voices this can be fiddly.

**Option 1 (current):** Latch buttons in the lower blend section.

**Option 2 (right-click on editor lane):** Right-clicking a lane row in East's visual editor opens a context menu with "Lane base: Macro / East" toggle for the current voice. More discoverable, less physical space needed.

**Option 3 (double-click on lane bar):** Double-clicking a lane in the editor toggles ownership for that lane on the current voice. Fast but harder to discover.

**Option 2 seems cleanest** — right-click is idiomatic in VCV Rack and keeps the panel uncluttered. The latch buttons could remain as visual indicators (read-only LED state) while right-click handles the toggle.

---

## Physical connection rules
All three expanders connect to Monsoon (the core sequencer), not to each other. They can be placed on either side. The expander manager (`cachedSandsVisualExpander`, `cachedMacroSandsVisual`, `cachedPolyVoiceExpander`) discovers each by type on either side at runtime.
