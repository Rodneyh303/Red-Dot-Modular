# CC PROMPT — subgate rest↔slur must honour tieAcrossRests like the MAIN gate (not hardcoded)

PROBLEM (Rodney): the step-advance draw-source fix (74897d64) moved ghost/ratchet candidacy to the subgate
REST decision (correct) but ALSO hardcoded how a RESTED subgate cell interacts with the slur — SequencerEngine
.cpp:1179 "a rested ghost does not [sustain the slur]" — ALWAYS opaque (rested ghost always breaks the slur).
That was NEVER requested. The MAIN gate makes this USER-SELECTABLE via the "Tie across rests" flag; the
subgate must honour the SAME flag, CONSISTENTLY.

## THE EXISTING FLAG (the authority — main gate already uses it)
engine.tieAcrossRests (context menu "Tie across rests", MonsoonWidget.cpp:1071; default ON). At the MAIN
gate (SequencerEngine.cpp:683-698):
- ON (default): a generated REST CANCELS a committed slur (N+1 silent) — rest beats legato.
- OFF: the slur IGNORES the rest roll — slur WINS, plays Legato/Tie ACROSS the rest.
Plus engine.advanceOnTieIntoRest (:1215, the paired flag): when tieAcrossRests && advanceOnTieIntoRest,
advance one step into the incoming-rest and re-draw legato there.
The user CHOOSES whether a rest breaks or bridges a slur. The main gate obeys that choice.

## THE BUG — the subgate HARDCODES one behaviour, ignoring the flag
SequencerEngine.cpp:1179 hardcodes "a rested ghost does not sustain" = ALWAYS the tieAcrossRests=ON
behaviour, with NO OFF path. So with the flag OFF, a rested ghost STILL breaks the slur — inconsistent with
the main gate (where OFF bridges it). CC's "retired the slur-preserving transparency" deleted exactly the
path that honoured the OFF case.

## THE FIX — subgate rest↔slur gated on the SAME flag(s), consistent with main
A rested SUBGATE cell (ghost AND ratchet) must interact with the slur EXACTLY as a rested MAIN step does,
gated on tieAcrossRests (and advanceOnTieIntoRest where it applies):
- tieAcrossRests ON: a rested ghost/ratchet cell CANCELS the slur (consistent with main — rest breaks slur
  everywhere). This is CC's current hardcoded behaviour — keep it, but as the ON branch, not hardcoded.
- tieAcrossRests OFF: the slur BRIDGES across a rested ghost/ratchet cell (consistent with main — slur
  survives rest everywhere). This is the path CC deleted — RESTORE it, gated on the flag.
- advanceOnTieIntoRest: if the main gate honours it at a rested step, the subgate should honour the
  analogous behaviour at a rested subgate cell (check whether it's meaningful per-subgate; keep consistent).
Reuse the MAIN gate's rest↔slur logic (the slurSuppressesRest / gapBridge pattern at SequencerEngine.cpp
:683-698 + Monsoon.cpp:1203-1215) for the subgate levels — do NOT write a separate hardcoded subgate rule.
Same flag, same logic, applied at the subgate rest decision. One rest↔slur authority, parameterised by level.

## KEEP (these parts of 74897d64 are correct)
- u = r_vary (the variation lane draw), zero new Philox draws, the held-rise assertion. ✓
- candidacy = the subgate REST decision (one rest authority, no double silencing). ✓
- variationRateProb retired; Variation's one consumer is step-advance. ✓
Only the rest↔slur HARDCODING is wrong — make it flag-gated and main-consistent.

## PINS
- The ghost/ratchet rest↔slur tests re-pinned to BOTH flag states (ON: rest breaks slur; OFF: slur bridges)
  — CC's current pins only cover ON. Add the OFF cases, matching the main-gate OFF behaviour.
- GOLDEN_A untouched (gate-mode-only guard); GOLDEN_B/BS re-pinned if the restored OFF path moves them.
- ADV_IDENTITY unchanged (this is rest↔slur, not the advance decision).

## DONE =
- Subgate (ghost/ratchet) rest↔slur gated on tieAcrossRests (+ advanceOnTieIntoRest), reusing the main
  gate's logic — NOT hardcoded. ON and OFF both behave consistently with the main gate.
- Tests pin both flag states at main AND subgate; consistency verified (same flag → same rest↔slur
  outcome at every level).
- run_all + strict flags green.

## WHY
"Tie across rests" is a GLOBAL gate-mode semantic (do rests break slurs?). It must apply uniformly at main
and subgate. Hardcoding one behaviour at the subgate breaks the user's choice and the main/subgate
consistency. One flag, one logic, every level.
