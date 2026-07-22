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
  Vermona manual's decoupling of incoming MIDI CC (MEX3) is the precedent.
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
| 26–39 | DNA scramble / reset buttons ×14 | — unreachable (never wired) | n/a | **DELETE** (LOCK_SEMANTICS §4) |
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
