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

## Scope: East (38) and Macro (60) — and why the order should be EAST FIRST

Censused from the id enums plus a read-site sweep of `src/dsp/` and `Monsoon.cpp`.

### The decisive difference: who READS the params

| | params | read by the ENGINE? |
|---|---|---|
| **East** | 38 | **NONE.** Zero engine-side reads of East's params. |
| **Macro** | 60 | **44 of 60**, every control cycle. |

Engine-side reads are all Macro's (`MonsoonSandsManager` / `Monsoon.cpp`):
`Macro::lorId(lane,0..2)` ×12, `Macro::macroAttenId(lane,·)` ×16, `Macro::sprId(lane)` ×4,
`Macro::tapIdForItem(lane,item)` ×8, `StraitsMacroVisualIds::dirDispId(el)` ×4.

**So East is the easier job despite looking similar, and Macro is the hardest in the
codebase.** East's params are entirely internal — its edits reach the engine INDIRECTLY
through store writes (`setLorBase` etc.) that already exist and do not change. Macro's are
read directly off the module's `params[]` array by the audio side, so de-paramming them is
the Causeway problem again — binding side and read side must move in lockstep — but with
44 sites instead of 12, and a wrong id is a SILENT wrong-value read, not a compile error.

### East (38) — all internal; 34 are pure selected-voice proxies
| Group | n | Kind |
|---|---|---|
| Spread display trimpots (SPREAD_R/M/O/A) | 4 | writes through to the store/engine path |
| Attenuverter display proxies (ATTEN_START) | 16 | pure proxy — real depth in MonsoonIds::MACRO_ATTEN_START |
| VAR/LEG CV-depth proxies | 6 | pure proxy — real depth in VARLEG_ATTEN_START |
| Direction display proxies | 6 | pure proxy — engine holds laneDir_/laneDirV_ |
| VAR/LEG delegation proxies | 2 | pure proxy — drives an OwnerCell |
| Owner display proxies | 4 | pure proxy — drives owner cells |

26 `params[...].setValue` sites in East.cpp must move to store writes (Lantern lesson 3).

### Macro (60)
| Group | n | Engine-read? |
|---|---|---|
| Spread display trimpots | 4 | **yes** (`sprId`) |
| Attenuverters (ATTEN_START) | 16 | **yes** (`macroAttenId`) |
| Direction display proxies | 4 | **yes** (`dirDispId`) — proxy-NAMED but engine-read |
| Send display proxies | 16 | no |
| PRE/POST tap params | 8 | **yes** (`tapIdForItem`) |
| Global LOR (GLOBAL_DNA_START) | 12 | **yes** (`lorId`) |

6 `params[...].setValue` sites in Macro.cpp.

### Recommended order and shape
1. **East first** — bigger proxy count but zero engine coupling, so it is widget+store work
   only and validates StoreBound at scale (38) after Lantern's 6.
2. **Mono Sands (54)** — its LOR/spread DO reach the engine, but via `setLorBase`, not via
   `params[]` reads, so it sits between East and Macro in difficulty.
3. **Macro last (60)** — needs an engine-side migration: every `macroVis->params[...]` read
   becomes a store read, moved in lockstep with the bindings. Treat like Causeway:
   behavioural build-verify, not just "it compiles". Get a working baseline first.

Caution for Macro: `dirDispId` is named a display proxy but IS read by the engine — do not
assume from the name. The census, not the naming, is the source of truth.

### WHY East and Macro differ on engine reads (it is not inconsistency)

A Rack `params[]` array can serve as STORAGE only when the data has no voice dimension.
Both modules index their params by LANE. The difference is what the data actually is:

- **Macro is GLOBAL scope** — one value per lane, no voice axis. A lane-indexed param array
  fits the data exactly, so the param IS the storage and the engine reads it straight off
  the module (`macroVis->params[Macro::lorId(lane,c)]`).
- **East is PER-VOICE scope** — 16 voices × lanes. Lane-indexed params physically cannot
  hold that, so the authoritative value goes to a voice-indexed store and the param is only
  a MIRROR of the selected voice.

East's own code shows the two-step directly:
```cpp
if (ch != 0) { m->setLaneDir(ch - 1, lane, nxt); }              // real value -> store (voice-indexed)
if (selectedVoice == ch) mod->params[dirDispId(lane)].setValue(nxt);  // mirror, only when selected
```

**Migration consequence — bigger than the read-count difference implies:**

| | what the params ARE | de-param work |
|---|---|---|
| East | a redundant DISPLAY MIRROR of data already stored elsewhere | **delete the mirror**; point widgets at the store that already exists; no engine change, no new storage, no new persistence |
| Macro | the DATA ITSELF (load-bearing storage) | **invent the storage**: new store fields, migrate 44 engine read sites, and add persistence that configParam was providing for free |

So East is a subtraction and Macro is a re-platforming. Do not treat them as the same job
because the panels look alike.

This also explains why the selected-voice proxies are incoherent to automate (the original
reason for the whole decision): a mirror of "whichever voice is selected" has no stable
meaning to a host. Macro's globals do have stable meaning — they are excluded for the
different reason that global LOR is just Macro's SCOPE and should not be privileged over
mono/East (see the decision above).
