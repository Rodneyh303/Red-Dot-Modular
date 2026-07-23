# Param Classification — the single table behind BOTH the DAW-param reduction and LOCK

Both open pieces (DAW_PARAM_AUDIT actions 1–2, LOCK_SEMANTICS implementation) are driven
by ONE question about each param, so they get ONE table. Settled with Rodney 2026-07-22.

## The test

**What is this param's relationship to the random surface?**

| Category | Relationship | Under LOCK | Host-automatable? |
|---|---|---|---|
| **GENERATION** | *shapes how draws become notes* | **LATCHES** (frozen at lock-on) | **YES** — the headline surface |
| **TRANSPORT** | *drives* the surface, doesn't shape it | **LIVE** | YES (PHASE especially — it is the DAW's own timeline) |
| **CONFIG / PROXY** | neither — display, selection, structural plumbing | irrelevant | **NO** — store-backed |

So **LOCK-LATCHING ⊂ DAW-PARAMS**. Generation is both; transport is DAW-but-live; config
is neither. The two work-streams share this classification but are otherwise independent.

## The ONE lock rule (all modulation sources decouple together)

Under lock, a GENERATION param's engine-visible value is a **snapshot of its RESOLVED
value (knob + CV + host automation) taken at lock-on**. Every modulation source is
decoupled equally:

- **Knob** — turning it does nothing until unlock.
- **VCV CV** — incoming CV is ignored (Causeway/Junction/East var-leg etc.). Confirmed:
  latching the knob while CV still moved the value would defeat the purpose, and the
  Vermona manual's decoupling of incoming MIDI CC (MEX3) is the precedent — verbatim:
  "lock-mode also applies for incoming MIDI Control Change messages".
- **DAW automation** — MIDI CC is the DIRECT analogue (a host/sequencer remotely writing
  parameter values), so freezing host automation under lock is precedented, not inferred.

SCOPE OF THE PRECEDENT (stated precisely, because it was once mis-cited here): the manual
covers control elements of the rhythm/melody sections, FIRST/LAST STEP, and MIDI CC. It
does NOT address meloDICER's own CV inputs under lock — latching external CV is our
extension, resting on the coherence argument, not on a quotation. It also confirms
snap-on-unlock: leaving lock-mode overwrites the stored values with the current settings.
- **DAW automation** — same treatment, for the same reason. "Knob frozen, CV frozen, but
  the host can still move it" is incoherent. Host writes land in the param; the engine
  reads the snapshot.

On unlock the engine resumes reading live resolved values, so everything snaps to wherever
knobs/CV/automation currently sit. The lurch is inherent and accepted — it is identical to
what CV already does, and "prepare silently, commit on unlock" is the point of the feature.

### Consequence that DECOUPLES the two work-streams
Lock snapshots the **resolved** value, so it does not care whether an input to that
resolution is a Rack param or a store-backed field. De-paramming an attenuverter changes
nothing about lock's design. The DAW reduction can therefore proceed without waiting on
lock, and lock can be specified against categories rather than against a param list.

## Monsoon (152) — classification and fate

| Ids | Group | Category | Lock | Fate |
|-----|-------|----------|------|------|
| 0–4 | Big-5 (NOTE_VALUE, VARIATION, LEGATO, REST, ACCENT) | GENERATION | LATCH | KEEP param |
| 5 | TRANSPOSE | *judgment* — leans TRANSPORT (performance, like transposing any sequence) | LIVE (open) | KEEP param |
| 6–7 | PATTERN_LENGTH / OFFSET | GENERATION (Vermona FIRST/LAST STEP) | LATCH | KEEP param |
| 8–25 | DNA LOR 6×(LEN/OFF/ROT) | GENERATION | LATCH | KEEP param |
| ~~26–39~~ | DNA scramble / reset buttons ×14 | — unreachable (never wired) | n/a | **DONE — deleted** (cleanup/dna-scramble-params); also removed the 14 matching INPUTs |
| 40–51 | SEMI toggles ×12 | GENERATION (scale) | LATCH | KEEP param |
| 52–53 | OCT LO / HI | GENERATION (range) | LATCH | KEEP param |
| 54 | BPM | TRANSPORT | LIVE | KEEP param |
| 55–56 | DICE R / M | GESTURE (queues under lock, §3 — unchanged) | queue | KEEP param |
| 57–61 | LOCK / MUTE / MODE / RESET / RUN | TRANSPORT | LIVE | KEEP param |
| 62–91 | Per-voice REST/ACC ×30 | GENERATION | LATCH | KEEP param — the model per-voice pattern (fixed addressing, no selection dependence) |
| 92–125 | MOD attenuverters ×34 | CONFIG-ish (they scale CV into generation params) | contribute to the snapshot; not separately latched | **DE-PARAM candidate** — CV jack is the real path |
| 126–140 | GLOBAL_* DNA + INTERP ×15 | GENERATION (structural) | LATCH | KEEP param |
| 141–146 | Dice slew / mix / trial ×6 | GENERATION (shapes the A/B blend) | LATCH | KEEP param (Raffles surface) |
| 147–150 | LAST_DICE / LAST_TRIAL ×4 | GESTURE (recall buttons) | like dice | KEEP param |
| 151 | PHASE | TRANSPORT | LIVE | KEEP param — the designed DAW phase surface |

## Expanders

| Module | Now | Category of the bulk | Fate |
|---|---|---|---|
| East Sands Visual | 152 claimed | selected-voice PROXIES (send/atten/owner/deleg/dir disp) = CONFIG | right-size to local ids; **de-param the proxies** (audit action 2) |
| Macro Sands Visual | 152 claimed | same proxies | right-size; de-param proxies |
| Causeway | 152 (uses MonsoonIds MOD_ATT ids) | attenuverters = CONFIG-ish | remap to local 0–33 enum |
| Change Alley | 16 params (8 block knobs + 8 triggers) | block knobs = GENERATION (grain of a restructure); triggers = GESTURE | already audit-native; pins themselves are store-backed and LATCH |
| Raffles / Junction / Interchange / Straits | — | mixed | unchanged this pass |

## Execution order (and why)

1. **DAW reduction first.** Delete the 14 unreachable scramble params; right-size East /
   Macro / Causeway; de-param the selected-voice proxies via `StoreEditAction` (landed,
   20/20). This removes the proxies — exactly the params whose "which voice does this
   mean?" ambiguity would make lock semantics incoherent.
2. **Then lock.** Implemented against CATEGORIES, over a settled surface: one snapshot
   mechanism for GENERATION params, transport untouched, dice/gesture queuing unchanged.
3. Open judgment calls to close before step 2: TRANSPOSE (leans live), lane DIRECTION
   (leans latch).

## Naming rule (carried from the audit)

Any surviving param that would need a "(selected voice)" qualifier in a host list should
not be a param. Self-describing names are the acceptance test for step 1.


## DECISION — visual/editor expanders expose NOTHING to the host (settled 2026-07-22)

Rodney's call, after seeing the surface in Bitwig. Two rules:

**1. Editor expanders are not control surfaces.** East Sands Visual, Macro Sands Visual,
Mono Sands Visual and Lantern are EDITORS: they render and edit engine/patch state. A host
should automate the instrument's controls, not an editor's widgets. So NONE of their params
are host-exposed; all become store-backed (StoreEditAction for undo).
Independently decisive: East and Macro carry SELECTED-VOICE proxies, so automating them is
incoherent in principle — one automation lane would write to whichever voice the UI happens
to have selected, a state the host cannot see. No budget argument needed.

**2. Control expanders keep their params.** Raffles, Junction, Causeway, Interchange, Change
Alley are hardware-like surfaces: each knob/jack means one fixed thing, no selection
dependence. The codebase already names the split (`*SandsVisual*` vs the rest).

### Consequence: global LOR is NOT automatable
Global LOR is simply MACRO's SCOPE of LOR — it is not special or privileged relative to
mono LOR or East's per-voice LOR. Exposing one scope and not the others was an accident of
which happened to be implemented as params, not a decision. So:
- Macro's 12 global LOR params → store-backed.
- Monsoon's 18 dead DNA_*_{LEN,OFF,ROT}_PARAM ids → DELETE (do not wire them up; an earlier
  suggestion to do so is withdrawn — it would have re-created the same privilege).
- The VAR/LEG "gap" dissolves: there is nothing to be missing from.

NOT a loss of control: these remain fully modulatable in-rack via CV, and Rack's CV sources
can be clock/DAW-synced — which gives finer resolution than host automation and none of the
selected-voice ambiguity.

### Resulting automation surface (the whole of it)
Monsoon's big-5 + per-voice REST/ACC + SEMI/OCT + LENGTH/OFFSET + dice/gesture buttons +
transport (BPM/RUN/RESET/MODE/PHASE/MUTE), plus the control expanders. Everything else is
store-backed and modulated in-rack.


## De-param progress and CORRECTED module sizes

Earlier estimates counted `configParam` CALL SITES; several sit inside per-lane loops, so
the real counts are larger. Verified from the id enums:

| Module | Params | Status |
|---|---|---|
| Lantern | 6 | **DONE** (feat/lantern-deparam) — all pure view state; module now claims 0 params |
| Mono Sands Visual | **54** (was estimated 8) | next |
| East Sands Visual | 38 | after Mono |
| Macro Sands Visual | 60 | last (largest) |

Mono Sands' 54 = 18 LOR (6 lanes × LEN/OFF/ROT) + 4 spread + 18 LOR attenuverters +
4 spread attenuverters + 4 owner-display proxies + 6 direction-display proxies.

### Lessons from the Lantern migration (apply to the rest)
1. **configSwitch value labels are load-bearing.** De-paramming silently removes the
   ParamQuantity tooltip, so a multi-position trimpot becomes unreadable. StoreBound now
   carries `valueLabels` / `tooltipTextFn`; use them for every switch-like control.
2. **configParam also gave persistence free.** Every de-parammed field must be added to
   dataToJson/dataFromJson explicitly or view/edit state is lost on reload.
3. **Direct param writes elsewhere in the file must move too** — Lantern's piano-roll wheel
   scroll wrote the param directly; it now records a StoreEditAction (and gained undo it
   never had). Grep for `params[...].setValue` in each module before declaring it done.
4. Mono/East/Macro differ from Lantern in kind: their LOR and spread values REACH THE
   ENGINE (via setLorBase etc.), so those migrations must preserve the engine write path,
   not just the widget. Only the owner/direction/attenuator entries are pure proxies.
