# Dice-scrub arc -- deferred follow-ups (single pickup point)

The dice-scrub rework (feat/dice-scrub) is FUNCTIONALLY COMPLETE: steps 1-6 done, scrub-knob undo
done. What's left is polish/panel/adjacent-module work, collected here so it's one place to resume.

## 1. Monsoon PANEL cleanup (dice-specific)
General panel cleanup analysis lives in MONSOON_PANEL_CLEANUP.md (nanosvg gradient issues, overlapping
sub-panels, framing-in-SVG vs runtime). The DICE-SPECIFIC panel changes from this arc, NOT yet done:
- REMOVE the 4 trial buttons from the Monsoon UI (trial/audition was removed in steps 1-2; the
  physical DICE_TRIAL_*/LAST_TRIAL_* param IDs are orphaned-but-harmless and go when the panel is
  regenerated). Frees panel space.
- RELOCATE last-dice: move LAST_DICE_R/M buttons + their lights UP into the row with DICE_R/DICE_M.
  Desired layout: DICE_R -- [light] -- LAST_DICE_R  and  DICE_M -- [light] -- LAST_DICE_M
  (light between each dice/last-dice pair).
- SCRUB indicators (from step 5): tick marks / a draw-offset readout at the 7 draw positions around
  the repurposed MIX->scrub knobs, so you can eyeball-land on draws (helps magnetism + click-through).
  Pairs with this relayout. See DICE_SCRUB_STEP5_STATUS.md.
- Panel is edited via the generator + regenerated SVG; widget reads positions from named SVG anchors
  via bindParam in MonsoonWidget (~298-315). Single-source-geometry rule: panel art is the source.

## 2. Step 5 detent FEEL tuning (deferred by Rodney)
ScrubKnobT magnetism works but the detent feel isn't dialed in ("steppier" than wanted at MAGNET=0.9
+ smootherstep^2). Tunables in ScrubKnobT (src/ui/Controls.hpp): MAGNET (strength), DRAG_PIXELS,
STEP_PIXELS, and the warp SHAPE. Likely: lower MAGNET (~0.5-0.7) and/or single smoothstep for a
gentler settle. Decide by feel in a real patch. Full detail in DICE_SCRUB_STEP5_STATUS.md.

## 3. Step 7 -- dice ROLL undo (deferred: needs threading infra)
Counter-only undo (Rodney: dice undo = counter only; slew/scrub knobs have their own). The obstacle:
dice rolls are detected in process() (audio thread) via processDiceButtons(), but APP->history->push
is UI-thread only. Needs a deferred audio->UI history queue (lock-free ring the widget step() drains,
then pushes a counter-restore action). Do it as its own focused session with a test harness for the
queue BEFORE wiring into process() -- half a history queue is worse than none. Decision recorded in
DICE_SCRUB_SLEW_B2.md ("dice undo = COUNTER ONLY").

## 4. RAFFLES changes (adjacent module -- rides the reworked dice)
Raffles Place exposes the dice reaction surface as GATES (re-dice, last-dice, etc.), pairing 1:1 with
Monsoon's dice buttons (DAW_PARAM_AUDIT.md:92-93, 155). Since the dice model changed under it:
- Raffles dice-gate QUEUE (from the lock arc): Raffles dice-gates were DEFERRED as the QUEUE category
  work "rides the reworked dice, routes through fireDieAction." Now that dice is reworked + trial
  removed, revisit: the gates must map to the CURRENT DieAction set (trial gates gone; DA_RESEED_ROLL
  gone -> Raffles must not expose those). Re-audit Raffles' gate->action mapping against the new enum.
- A/B multi-dice-scrub candidate (DiscussionDiceAndCA.md:21): the Raffles A/B mix as a live crossfader
  between two rolled patterns is a DIFFERENT gesture from scrub; flagged as a future multi-dice-scrub
  candidate. Revisit after the core scrub is settled in use.
- Trial-gate removal on Raffles: trial dice are gone, so any Raffles TRIAL/LASTTRIAL gate inputs are
  now dead and should be removed/repurposed when Raffles is next touched.

## Order when resuming
Panel relayout (#1) + detent tuning (#2) pair naturally (both are feel/surface, need Rack). Step 7
(#3) is standalone infra. Raffles (#4) should follow the Monsoon panel since it mirrors the surface
and depends on the settled DieAction set.
