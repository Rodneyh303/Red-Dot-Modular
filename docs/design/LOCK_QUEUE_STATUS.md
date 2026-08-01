# Lock mode QUEUE category status

## CA scatter QUEUE: DONE (via the keystone -- nothing left)
Verified the arm->fire path is complete:
- ARM: triggers (CV domTrig/codTrig, panel buttons btnTrig, scatter-back sBackDom/sBackCod) call
  latchRow() -> pendingRows[r].armed = true. NOT lock-gated -- arming always happens.
- FIRE: MonsoonExpanderManager::sync(engine, caQueueFires) calls v2->applyPendingTransforms() only
  when caQueueFires = (boundary && !locked) || unlock (from LockManager::queueFires()).
So a scatter triggered under lock ARMS and FIRES at the next phrase boundary / unlock = textbook
QUEUE. No work remaining. The keystone (boundary/unlock -> LockManager) already delivered this.

## Raffles dice-gates QUEUE: DEFERRED (in the dice-trial churn zone -- Rodney)
Raffles's dedicated gates route through Monsoon::fireDieAction(a) -- the single DRY dispatch for ALL
die actions (also called by the menu-routed G3 gate):
- ROLL-producing: DA_REDICE_R/M (diceRhythm/Melody), DA_TRIAL_R/M (setPendingTrial), DA_LASTDICE_R/M
  (setPendingLastRoll). These are the QUEUE candidates -- under lock they should arm-and-fire at the
  boundary via queueFires(), not fire live.
- NON-roll toggles: DA_LIVESRC, DA_RESEED_*, DA_LIVESTATIC. NOT rolls -- should not queue. So
  fireDieAction can't be blanket-gated; only the roll actions are QUEUE.

WHY DEFERRED: the trial / re-dice / last-dice distinction is exactly what the planned A/B rework
(A/B -> multi-dice scrub) will reshape. Rodney flagged trial dice as "the area most subject to
change." Wiring the dice-gate lock-queue onto the current trial mechanics now = rebuild later. The
QUEUE MECHANISM itself (LockManager::queueFires arm-and-fire) is stable and shared with scatter; the
coupling point is only "what arms the queue" (which die action, under the reworked scrub model). So
build Raffles dice-gate QUEUE AFTER the A/B/multi-dice-scrub rework, routing the reworked roll
trigger through the same queueFires() path scatter uses.

A/B mix itself: stays LATCH (Rodney: A/B "won't change as a control" -- the rework is the mechanism
underneath, multi-dice scrub, not the control surface). So ABMix LATCH ruling holds; no revisit.

## Net
QUEUE category is effectively COMPLETE for now (scatter done; Raffles dice-gates correctly deferred
to after the dice-scrub rework). Phase 2 remaining: expander threading (Causeway/Junction/
Interchange/Shophouse/Changi -> mostly already via gated spread/LOR paths), lock-scope menu (§7).
