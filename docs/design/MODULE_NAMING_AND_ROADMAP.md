# Module naming + roadmap

Status: **design capture, for later.** Single reference for the module family naming and the
planned new modules / parent features. All POST the Sands topology consolidation
(`SANDS_TOPOLOGY_RESOLVER_PLAN.md`); the East/West signal split detail lives in
`STRAITS_EASTWEST_REDESIGN_NOTE.md`.

> Note: module **names + panels** have been (or will be) updated, but the **C++ files and
> classes have NOT** been renamed yet — low priority. A name→class mapping is needed for the
> eventual rename; see the mapping table at the bottom.

---

## The "Sands" substrate

**Sands** is not a single module — it's the **probability substrate**: chance/gambling (the
draw), the foundation Singapore is built on, the icon (Marina Bay Sands), and the grain (the
finest unit = a single voice's draw). The three visual editors are **lenses into Sands** at
different scopes. The code already uses "Sands" as this umbrella (`SandsTopology`,
`MonsoonSandsManager`, strands) — keep it as the subsystem term; the per-editor names below
are the user-facing modules.

---

## Probability / visual editors (Sands family)

Each editor is named **after its host module + "Sands"** — the qualifier encodes scope:

| Module | Role | Host logic |
|---|---|---|
| **Monsoon Sands** | mono-voice probability (single voice, the grain) | on Monsoon (the parent) |
| **Straits Sands** | detailed poly probability (per-voice, fine) | on Straits — narrow channel = fine detail |
| **Marina Sands** | broad poly probability (all voices, broad strokes) | wide bay = broad strokes |

(Old overloaded names — Straits Sands / Sands East / Sands West for both base AND editors —
are retired in favour of this clean scope-qualified set.)

---

## Signal / hardware expanders (transit family) — the poly path

Replaces the old Straits East/West base poly expanders. Theme: **transit infrastructure**
(things flow through and out). Detail in `STRAITS_EASTWEST_REDESIGN_NOTE.md`.

| Module | Role | Pun / logic |
|---|---|---|
| **Straits** | base poly: 32 knobs (REST+ACCENT, 4col×8row), houses 16 voices, poly-cable outs (16ch, ch1 = mono duplicate) | the channel the voices run through |
| **Causeway** | poly CV: modulates poly REST + ACCENT | "the link across" = modulation across the voices |
| **Changi** | poly output: per-voice mono outs (poly cable → individual jacks) | departures = per-voice outs |

Note: **Causeway is REVIVED** from the retired list, repurposed for a *different* role (poly
CV) than its original. **"Straits" is deliberately double-used** — the base signal module
**Straits** and the detailed editor **Straits Sands** (editor named after its host, exactly
like Monsoon→Monsoon Sands). Intentional, not a clash.

---

## New modules (planned)

### Shophouse — scale expander
Breaks the scale function **out of Monsoon's context menu** into its own module.
- **Non-destructive scale changes on faders** — faders set the scale without destroying the
  underlying state (revertible).
- Build a **small list of scales** and **modulate between them**, applied **only at the
  sequencer boundary** (quantised to the loop/bar edge, like slew — never a mid-pattern
  jolt). Scale change snaps musically at the sequence boundary.
- (Heritage "row of individual cells" image fits scale degrees; kept Shophouse for this.)

### Lantern — note-output visualiser (VIEW ONLY)
A debugging/▶display aid that shows what notes are actually coming out.
- **16 lanes** like the visual expanders, but **narrower lanes**, showing the actual notes.
- Per note: **note name** + **little cross-lines separating the parts of a tie**.
- Pure visualiser — no pass-through/processing.
- KNOWN CHALLENGE: representing **notes held over multiple bars** (a tie spanning bars). Flagged
  as solvable-later ("we'll find a way").

---

## Parent (Monsoon) feature additions (planned)

- **Phase knob** — mainly for **VST phase modulation** (host/DAW phase sync/offset). Lives on
  the Monsoon parent.
- **16th triplets** — considering adding this clock subdivision to Monsoon.

---

## Status of every name

| State | Names |
|---|---|
| **Unchanged (live)** | Monsoon (parent), Raffles (probability draws — "holds a raffle"), Junction + Interchange (mono-voice CV modulators; Junction is a core Monsoon modulator, out of scope) |
| **New editor names (Sands family)** | Monsoon Sands, Straits Sands, Marina Sands |
| **New signal modules (transit)** | Straits (base), Causeway (poly CV), Changi (output) |
| **New modules (planned)** | Shophouse (scale), Lantern (note visualiser) |
| **Revived** | Causeway (now poly CV, different role) |
| **Retired** | Surge; West (4×8 knob grid fits one Straits module in sensible HP → no voice-range split) |

---

## Name → current C++ class/file mapping (for the LOW-PRIORITY rename)

Fill in as the rename happens; classes/files keep old names until then.

| New module name | Current C++ class / file | Notes |
|---|---|---|
| Monsoon Sands | `MonsoonSandsVisualExpander` (src/MonsoonSandsVisualExpander.*) | mono visual |
| Straits Sands | `StraitsEastSandsVisual` (src/StraitsEastSandsVisual.*) | detailed poly visual |
| Marina Sands | `StraitsSandsMacroVisual` (src/StraitsSandsMacroVisual.*) | broad poly visual |
| Straits (base) | `MonsoonStraitsEastExpander` (+ retiring `MonsoonStraitWestExpander`) | base poly; West folds in |
| Causeway (poly CV) | (CV currently on East/West surfaces) | new module; extract CV path |
| Changi (output) | (poly outs currently on `MonsoonStraitsEastExpander`) | new module; per-voice outs |
| Shophouse (scale) | (scale currently in Monsoon context menu) | new module; extract scale |
| Lantern (visualiser) | (new) | new module |

> Watch for the **"Sands" collision**: keep "Sands" as the subsystem/umbrella term in code
> even though "…Sands" is also user-facing module names. Document the distinction in the
> rename PR so it isn't confused (module "Monsoon Sands" ⊂ the "Sands" probability subsystem).

---

Idea capture only — nothing actioned. Sequenced after the Sands topology work.
