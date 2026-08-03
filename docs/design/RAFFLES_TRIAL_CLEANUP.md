# Raffles / Monsoon trial-dice + live-source cleanup (SCOPED, do in own session)

Rodney: "Raffles needs to lose anything related to old trial dice. Live-mode source I think also
goes. And we have Monsoon trial cleanup to do."

Context: the DICE-SCRUB arc already removed the trial DieActions and DA_RESEED_ROLL from the shared
DieAction vocabulary (DieAction enum is now REDICE/LIVESTATIC/RESEED_RESTART/LASTDICE only -- no
DA_TRIAL_*). But the TRIAL params, Raffles gates, gate3 targets, panel labels, and the trialMode field
still remain. This is the follow-through removal. Trial = "roll a fresh candidate B against anchored A,
endless variations on a theme"; superseded by the dice-counter model (main dice + scrub + LastDice).

## Why LIVE SRC goes with trial
"LIVE SRC" (RAFFLES_GATE_LIVESRC_R/M + G3_TOGGLE_RHYTHM/MELODY_LIVESRC) toggles which dice the LIVE
mode drives per lane: false=main (promote, A walks) vs true=TRIAL (anchored A). That toggle EXISTS
ONLY to switch between main and trial. With trial gone there is only one source (main), so LIVE SRC is
meaningless and is removed too. The trialMode[] field (per-lane false=main/true=trial) is the state it
drove -- remove the field and hard-wire the main path.

## FULL footprint to remove (enumerate exhaustively -- prior cleanups left stragglers)
Grep the VALUE NAMES across the whole tree, not just the feature word, before deleting (the
DA_RESEED_ROLL lesson: a g3map / label ref slipped through twice).

Monsoon.hpp:
- ParamIds: DICE_TRIAL_R_PARAM, DICE_TRIAL_M_PARAM, LAST_TRIAL_R_PARAM, LAST_TRIAL_M_PARAM
- InputIds (Raffles gates): RAFFLES_GATE_TRIAL_R, RAFFLES_GATE_TRIAL_M, RAFFLES_GATE_LASTTRIAL_R,
  RAFFLES_GATE_LASTTRIAL_M, RAFFLES_GATE_LIVESRC_R, RAFFLES_GATE_LIVESRC_M
- Gate3Target: G3_TRIAL_RHYTHM, G3_TRIAL_MELODY, G3_TOGGLE_RHYTHM_LIVESRC, G3_TOGGLE_MELODY_LIVESRC
  (renumber the enum + fix any g3map[]/n3 menu that indexes it -- CHECK the g3 menu array length and
  every switch on Gate3Target)
- Field: trialMode[2] (per-lane). Remove; hard-wire main. Remove from dataToJson/dataFromJson.
- rafflesGateTrig[14] -> shrink to the surviving gate count; re-verify the index mapping used to fire
  DieActions (this array is indexed positionally -- removing gates shifts indices; audit the fire loop).
- gate3 processing: any "trial" / "livesrc" branch in the DieAction dispatch or G3 handler.

MonsoonRafflesExpander.cpp/.hpp:
- bindInput for input_RAFFLES_GATE_TRIAL_R/M, input_RAFFLES_GATE_LASTTRIAL_R/M, and the LIVESRC gate
  inputs.
- configInput labels: "Trial rhythm die", "Trial melody die", "Last rhythm trial", "Last melody
  trial", and LIVE SRC labels.
- Panel label array: const char* gl[5] = {"TRIAL","REDICE","LIVE SRC","LIVE/STAT","RESEED"} -> drop
  TRIAL and LIVE SRC -> gl[3] = {"REDICE","LIVE/STAT","RESEED"}; fix the y-position arrays and the
  paired Last-gate "L" markers that sit beside TRIAL/REDICE (LASTTRIAL markers go with TRIAL).
- RafflesLayout geometry constants for the removed gates.

PatternEngine (.hpp/.cpp):
- Any trial-specific draw path / anchored-A "trial B" logic distinct from the main dice-counter draw.
  (The reseedOnRoll trial branches were already removed; confirm none remain.)

Panel SVG (res/panels/*Raffles*): remove the TRIAL + LIVE SRC gate jack markers + text; re-flow the
column so the 3 surviving actions (REDICE / LIVE/STAT / RESEED) + their Last-siblings are evenly
spaced. Single-source geometry: panel is the source, widget reads it.

## Method (avoid the known failure modes)
1. Grep EACH value name (DICE_TRIAL_R_PARAM, RAFFLES_GATE_TRIAL_R, G3_TRIAL_RHYTHM, RAFFLES_GATE_LIVESRC_R,
   trialMode, ...) across ALL of src/ + res/ + test/ and list every hit BEFORE editing.
2. Removing an InputId/ParamId shifts the enum -> the positional rafflesGateTrig[] fire loop and any
   bindInput order must be re-audited. This is where an off-by-one will hide.
3. When editing the heavily-commented Monsoon.hpp, brace/index-match while SKIPPING braces in // comments.
4. Migrate persistence: old patches with trialMode / trial params should load without error (ignore
   unknown keys; default the removed state to the main path).
5. Commit when green; verify with git show HEAD:<file>. Rack-verify the Raffles panel (gates gone, no
   dead jacks, labels re-flowed) and that dice still roll (main + scrub + LastDice unaffected).

## Relation to other open items
- Independent of the CA reverse decision and the Lantern/IT branch. Do on master.
- Ties off the dice-scrub arc's deferred "Raffles re-audit" follow-up (DICE_SCRUB_FOLLOWUPS.md item 4).
