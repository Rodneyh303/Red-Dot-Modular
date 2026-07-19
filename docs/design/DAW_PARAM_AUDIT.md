# DAW Param Audit — what the collection exposes to host automation

Status: AUDIT (decisions pending per-group). Prompted by the Bitwig mix-in confusion:
selected-voice display proxies ("L1 mix-in 0 (selected voice)") appear in the host's
param list looking like stable per-voice controls, but retarget with UI selection and
store-back-smear across voices under host modulation.

## 1. The budget and the mechanism

- Rack Pro exposes **up to 1024 automatable params per plugin INSTANCE** — the whole
  patch shares one pool (VCV release notes: "parameter automation of up to 1024 knobs,
  sliders, and buttons in your patch"). Slots are assigned to module params as modules
  are created and reused on deletion (manual: "Rack may reuse automation slots").
- **Every `config(N,...)` claims N slots**, used or not. `configParam` naming just
  labels them; an unbound params[] entry is still a host param (unnamed).
- Therefore: display proxies, state stashes, and over-sized arrays all spend real
  budget and appear in host lists where they can only mislead.

## 2. Current claims (this branch)

| Module                | Allocated        | Actually bound/used   | Notes |
|-----------------------|------------------|-----------------------|-------|
| Monsoon               | 152              | 152                   | full map in §4 |
| Straits               | 153              | ~50 of shared + own   | reuses MonsoonIds (per-voice REST/ACC banks) + VOICE_COUNT above them |
| East Sands (visual)   | 152 (MonsoonIds) | 38 local ids          | **~114 slots dead**; 34 of the 38 are selected-voice/display proxies |
| Macro Sands (visual)  | 152 (MonsoonIds) | 48 local ids          | **~104 slots dead**; 28 of 48 are proxies (16 send disp, 4 dir disp, 8 tap*) |
| Causeway              | 152 (MonsoonIds) | 34 (32 atten + 2 glb) | **~118 slots dead** — but the 34 are MonsoonIds MOD_ATT ids (92–125), so shrinking needs an id remap |
| Sands Mono (visual)   | 54               | 54                    | 18 LOR + 4 SPR + 22 attens + 10 disp proxies |
| Lantern               | 7                | 7                     | view controls (VIEW/ZOOM/FOLLOW/ROLL/SCROLL/COLOR + theme) |
| Shophouse             | 10               | 10                    | 4 scale + 4 root + conservation + index att |
| Interchange           | 14               | 14                    | 12 semi attens + 2 oct attens |
| Junction              | 5                | 5                     | big-5 CV attenuverters |
| Raffles               | 4                | 4                     | slew/mix attens |
| Changi                | 0                | 0                     | outputs only — the model citizen |

**Full-system patch ≈ 855 slots (~84 % of 1024) before a single VCO, filter, or mixer.**
Dead allocation alone (East + Macro + Causeway over-sizing) ≈ **336 slots**.

Taxonomy that makes the audit tractable: apart from Monsoon and Straits, every expander
is either a MODULATOR of probabilities / the reaction surface (Junction, Interchange,
Raffles, Causeway attens, Shophouse) or a pure OUTPUT expander (Changi, Lantern's view
controls aside). Modulator params are attenuverters — legitimately params, rarely worth
host automation (the CV jack next to them is the modulation path). The problem class is
confined to the visual editors' proxies and the over-sized allocations.

## 3. Special-semantics params (the confusion inventory)

These are params whose HOST-VISIBLE meaning differs from what a DAW user would assume:

**A. Selected-voice proxies** — retarget with the UI's voice tab; host modulation writes
through to whichever voice is selected, and browsing voices smears the value (the
store-back runs every same-voice frame):
- Macro: 16 send disp ("L{lane} mix-in {item} (selected voice)")
- East: 16 atten disp + 6 VAR/LEG atten disp + 2 VAR/LEG deleg disp + 4 owner disp
- East + Macro + Mono Sands: direction display proxies (6/4/6)
- Mono Sands: 4 owner disp

**B. Display/derived proxies (not selection-dependent but not controls either)**:
Macro tap params (widgeted trimpots that are really tap config), owner cells.

**C. (CORRECTED) — no state stashes found.** `LAST_DICE_R/M` and `LAST_TRIAL_R/M` are
momentary BUTTONS (configButton, SchmittTrigger-processed): "recall the previous draw /
previous candidate" gestures. The stashed VALUES live in the engine's dice state, not in
params. They pair 1:1 with Raffles' LASTDICE/LASTTRIAL gate inputs — same gesture, button
vs CV form — and are part of the dice reaction surface. They STAY.

**D. Momentary buttons**: DNA scramble ×7, reset ×7, DICE R/M, RESET_BUTTON. Legitimate
params (host-triggerable gestures) but arguably better served by CV/gate inputs which
already exist (Raffles gates). Low priority either way.

## 4. Monsoon (152) — grouped, with automation-worthiness

Philosophy (agreed direction): **the DAW automates the surfaces through which we REACT
to generated probabilities; structural/config state stays in Rack, where CV modulation
already covers dynamic use.** The collection's own CV inputs (Junction, Interchange,
Raffles, Causeway, East/Macro CV jacks) make host automation of the same targets
redundant — patch a DC-coupled DAW output into the CV jack instead.

| Ids | Group | Verdict |
|-----|-------|---------|
| 0–5 | Big-5 probability knobs + transpose | **KEEP — prime automation surface** |
| 6–7 | Pattern length/offset | KEEP (musical structure moves) |
| 8–25 | DNA LOR 6×(LEN/OFF/ROT) | KEEP (probability-shape reaction) |
| 26–39 | Scramble/reset buttons ×14 | keep as params, not promoted; CV gates exist |
| 40–51 | SEMI toggles ×12 | keep (scale changes are musical), low priority |
| 52–54 | OCT LO/HI, BPM | keep |
| 55–56 | DICE R/M | keep (host-triggered re-dice is a real gesture) |
| 57–61 | LOCK/MUTE/MODE/RESET/RUN | KEEP (performance switches) |
| 62–91 | Per-voice REST/ACC ×15+15 | **KEEP — the GOOD per-voice pattern**: one param per voice, fixed addressing, no selection dependence. This is what per-voice DAW control should look like. |
| 92–125 | Per-voice + global MOD attenuverters ×34 | keep (bound on Causeway); attens are config-ish, rarely automated |
| 126–140 | GLOBAL_* DNA + INTERP ×15 (no ACCENT_INTERP) | keep; structural |
| 141–146 | Dice slew/mix/trial ×6 | keep (Raffles surface) |
| 147–150 | LAST_DICE/LAST_TRIAL ×4 | **KEEP — recall GESTURES (buttons), not stashes; the dice reaction surface. Raffles exposes the same as gates.** |
| 151 | PHASE_PARAM | **KEEP — explicitly designed as the DAW-automatable phase fallback** |

## 5. Recommended actions (ordered by value/effort)

1. **Right-size the over-allocated expanders** (~336 slots refunded):
   - East: `config(StraitsEastVisualIds::NUM_SPREAD_PARAMS=38, …)` — local ids only; verify no residual MonsoonIds-id param reads (comments referencing `MonsoonIds::MACRO_ATTEN_START` are stale post-migration; confirm).
   - Macro: `config(48, …)` — same verification.
   - Causeway: uses MonsoonIds MOD_ATT ids directly (92–125). Either remap to a local 0–33 enum (touch bindings + manager reads) or accept 152. Remap recommended; it's the slot-convention pattern VoiceResolver already established.
   - Patch back-compat of param ids is NOT a priority (established), so re-numbering is allowed; json param save/load is by id — note in release notes.
2. **De-param the selected-voice proxies** (Option A from the mix-in discussion, now
   generalized): send disp, atten disp, owner disp, deleg disp, dir disp across
   East/Macro/Mono Sands become store-backed widgets. Kills the retarget/smear
   confusion class and refunds ~50 more slots. The proxy-sync latches
   (`lastSendVoice` etc.) get deleted with it.
3. ~~Demote LAST_DICE/LAST_TRIAL~~ — WITHDRAWN on inspection: they are recall buttons
   (gestures), not value stashes; the values already live in engine state. They stay.
4. **Optional (Option C)**: add 16 stable "V*n* macro receive" params on Macro if
   per-voice DAW control of the macro bus earns its slots. Decide after 1–2.
5. **Rename what remains** so host lists self-describe: params that survive should
   never need "(selected voice)" qualifiers — if a param needs one, it should not be
   a param.

## 5b. Prerequisite for action 2: custom undo (the real cost of de-paramming)

What ParamWidget gives for free — undo/redo (ParamChange history), tooltip + typed
entry, host/MIDI mapping — is forfeited by store-backed widgets. Recoverable:

- **Undo**: Rack's history API is open (`APP->history->push()` takes any
  `history::Action`), the same mechanism cable/module edits already use. Add ONE shared
  helper to the redDot kit — a `StoreEditAction` capturing (voice, lane, item, oldValue,
  newValue) with setter-lambda `undo()`/`redo()` — and wire it into the ~5 de-parammed
  widget classes (send cells, atten cells, dir cells, owner/delegation cells). All are
  discrete click/drag edits with clear before/after. For continuous drags, mimic
  ParamWidget's coalescing: capture old on `onDragStart`, push one action on `onDragEnd`.
- **Correctness GAIN**: current proxy undo is subtly wrong — undoing after switching
  voices restores the param and the store-back writes it into the NEWLY selected voice.
  A StoreEditAction records the voice index, so undo lands on the voice actually edited
  regardless of current selection. Undo doesn't just survive; it becomes voice-correct
  for the first time.
- **Typed entry**: keep where it matters via a text field writing the store (the Sands
  editors already have custom text-entry patterns).
- **Host/MIDI mapping**: genuinely lost — which is the point of removing them.

So action 2's cost line: one shared undo helper + wiring into ~5 widget classes, in
exchange for the slot refund, deletion of the proxy-sync/smear bug class, and
voice-correct undo.

## 6. End-state target

The arithmetic floor of actions 1–3 is ≈ 560 slots (~55 %): Monsoon ≈ 148, Straits
≈ 149, East ≈ ~8, Macro ≈ ~20, Causeway 34, others unchanged. But 560 is the FLOOR of
the mechanical cleanup, not the target — the aim is to use a lot less. The same
scrutiny that killed the proxies applies to what remains: attenuverters whose CV jack
is the real modulation path, config-ish structural knobs, and buttons duplicated as
Raffles/CV gates are all candidates for staying params without needing to survive a
"would anyone automate this from a DAW" test — and where the answer is no and the
param serves no other purpose, it can go to the store too. End state:
every host-visible param stable, self-describing, and musically automatable — with the
probability-reaction surface (big-5, DNA LOR, per-voice REST/ACC, PHASE) as the
headline automation story, and everything else modulated in-rack via CV.
