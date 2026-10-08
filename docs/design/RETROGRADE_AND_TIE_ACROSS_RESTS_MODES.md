# Backward playhead (generative) + tie-across-rests across modes

STATUS: design notes / decisions. Release-scope.

## 1. Backward playhead is RELATIONAL-ONLY in a generative engine (Rodney)
Deterministic sequencers: backward playhead = play the recorded notes in reverse (meaningful, there's
a fixed sequence to reverse). OUR engine GENERATES as it reads — no pre-recorded sequence — so walking
the playhead backward through a probability field just reads positions in descending order, which for
generated draws is ANOTHER walk, not an audible "reverse of" the forward walk. **So backward playhead
buys little in ISOLATION.**
It buys something RELATIONALLY — in reference to other FORWARD playheads:
- A backward voice reads the SAME correlated field in opposite position-order, giving CONTRARY MOTION /
  convergence-divergence vs the forward voices (retrograde-canon-ish). Meaningful only because of the
  forward reference.
Implications:
- Value lives at the PER-VOICE direction level (voice direction, the addressing system) — contrary
  motion against forward voices. NOT at the global clock/gate level (global-everything-backward just
  gives a different global walk, no internal relationship change).
- **Phase mode:** backward is INHERENT to the drive model (position is the input; backward = drive the
  phase back). Native, not an added mode.
- **Clock mode backward:** easy if wanted (reuse phase's backward-advance + PPQN quantise) — but
  generatively buys little absolutely; only a reference frame for EXTERNAL forward modules.
- **Gate mode backward (if ever):** NOT reverse-time — time still forward, gates still rise/fall
  forward; just DECREMENT playhead on rising edge, stop on falling (unchanged), MIRROR the subgate
  boundary logic (reflect the straddle/coincidence/ghost rules). Bounded (forward-time model + legato
  untouched; only playhead-walk-dir + subgate-mirror change). Composes as the playhead sign in the
  three-factor direction product (playhead x voice x lane).
**Decision lean:** global backward playhead in clock/gate not pursued (relational value already met by
per-voice voice direction; phase covers driven-backward natively). Per-voice backward direction (the
addressing system) is where retrograde earns its place.

## 2. tie-across-rests should extend to clock/phase — CORRECTED
All THREE modes (gate, clock, phase) have BOTH rest types:
1. **Generated rests** — a step/event that is a rest (`generatedRestBeatsLegato` governs rolling these).
2. **Gap / incoming rests** — a gap before the next note (`tieAcrossRests` governs bridging these).

So clock/phase are NOT missing a rest type. The clock/phase **RESIDUAL GAP** (the leftover when an odd,
non-1/16 note is quantised to the 1/16 grid) is the **EQUIVALENT of gate mode's INCOMING REST = the gap
between gates** (Rodney). It is the same gap-type rest, and `tieAcrossRests` governs it identically.
(The 1/16 note-length restriction in clock/phase is a SEPARATE thing — it's merely the mechanism that
PRODUCES the residual gap; it doesn't change the rest model.)

**So both toggles apply to all three modes.** Clock and phase currently just DON'T ALLOW tie-across-gap
— the gap-bridge behaviour isn't enabled there, even though they already HAVE both gap types. The
extension is therefore close to "enable tie-across-rests (the gap-bridge logic) in clock/phase" — they
are already structured for it; they simply don't act on the gap-bridge toggle yet.

The two toggles are NESTED/AND-GATED, NOT parallel (Rodney):
- **`tieAcrossRests` = TRUE is the MASTER enable for bridging ANY rest** (incoming/residual OR
  generated). If false, no tie across any rest.
- **To tie across a GENERATED rest, you ADDITIONALLY need `generatedRestBeatsLegato` = FALSE**
  ("generated rest does NOT beat legato" => legato wins => tie). This extra gate applies ONLY to
  generated rests and only matters when tieAcrossRests is already true.

Truth table (given a slur intention was generated — the prerequisite):
| rest type | tieAcrossRests | generatedRestBeatsLegato | tie? |
| incoming/residual | FALSE | any | no |
| incoming/residual | TRUE  | any | YES |
| generated | FALSE | any | no |
| generated | TRUE  | TRUE (rest beats legato) | no |
| generated | TRUE  | FALSE (legato beats rest) | YES |

**The two interact and DEPEND ON THE MATERIAL:** whether a given gap is an incoming/residual rest or a
generated rest is determined by what the material produced (a note-length choice leaving a residual
gap, vs the engine generating a rest step). So the SAME toggle settings give different tie behaviour at
different gaps depending on which rest type occurred. Toggles define the policy; material picks the
branch.

### VERIFIED against gate-mode code (SequencerEngine.cpp) — toggles set ELIGIBILITY, material decides
The table above gives eligibility; the ACTUAL tie is further gated by material. From the code:
- **Generated rest:** `slurSuppressesRest = !generatedRestBeatsLegato && slurReachesHere`
  (SequencerEngine.cpp:521). So generatedRestBeatsLegato=FALSE lets a REACHING slur win — but
  `slurReachesHere` requires a genuine committed slur with a held predecessor (material-dependent).
- **FRACTIONAL TAIL override (:519):** a fractional tail ALWAYS outranks rest (canRest), REGARDLESS of
  the toggle. So tail-present is a material override that forces the tie-ish behaviour either way.
- **Incoming/residual rest bridge:** at the checkpoint, **generatedRestBeatsLegato is IRRELEVANT**
  (:719 — the incoming rest is already silent; only slur candidacy matters). Survival across it is
  gated by a LEGATO RE-DRAW at the rest checkpoint (:722: survives = legatoProb>=0.999 OR
  r_legato<legatoProb), under `tieAcrossRests && advanceOnTieIntoRest`. So "tieAcrossRests=TRUE =>
  YES" is really "=> ELIGIBLE; the slur bridges only if it SURVIVES the re-draw at the rest".
So: **toggles enable the POSSIBILITY; the material (committed-slur reach, fractional tails, the legato
re-draw at the incoming-rest checkpoint) decides the actuality.** The table = eligibility, not
guaranteed outcome. (This is gate mode, verified; clock/phase should use the ANALOGOUS logic — the
reason to derive theirs from gate's, per 'worked through gates'.)

Verify the tie-across-rests implementation is at the legato/rest-decision level (mode-agnostic per the
gate-mode discipline) so enabling it for clock/phase's gaps is clean.

### DEFAULT (Rodney) — tie only DIRECTLY-CONNECTED; residual gap is a REAL gap, not auto-tied
Correcting an over-reach: the residual gap is NOT "incidental so bridge by default". It is a GENUINE
gap (the odd note ended before the next grid position). Default behaviour parallels gate mode exactly:
- **Gate mode default:** auto-tie across gates DIRECTLY CONNECTED — separated only by the MINIMUM
  rise/fall that can distinguish two gates (the <=1-sample abutment / overlap case). Real gaps are NOT
  auto-tied; `tieAcrossRests` is the explicit opt-in for those.
- **Clock/phase default:** auto-tie across **1/16-MULTIPLE** notes (grid-adjacent, directly connected
  — the equivalent of gate's minimum-separation directly-connected case). The RESIDUAL GAP from a
  triplet/odd note is a real separation → NOT tied by default; `tieAcrossRests` is the opt-in to bridge
  it (and `generatedRestBeatsLegato` for generated rests).

Same principle across all modes: **auto-tie only the directly-connected / minimum-separation case;
anything with a real gap (residual OR structural/generated) requires the explicit tie-across-rest
toggle.** The residual gap is treated like any other real gap — default respect, opt-in bridge.

### "Auto-tie" gates REALIZATION of a generated slur intention — it does NOT create ties (Rodney)
A SLUR INTENTION must be GENERATED first (by the legato lane / generative legato decision) for any tie
to happen. "Auto-tie" is NOT "ties appear on their own" — it is: GIVEN a generated slur intention, the
directly-connected case lets it REALIZE automatically (no gap-toggle needed). The intention is the
prerequisite; the toggles gate whether an intention may bridge a real gap. Full logic:
- No slur intention -> NO tie, ever (even directly-connected; notes play separately).
- Slur intention + directly-connected (1/16-multiple / gate min-separation) -> tie (AUTO).
- Slur intention + real gap (residual/structural) -> tie only if the gap-toggle is on
  (`tieAcrossRests` for incoming/residual gaps, `generatedRestBeatsLegato` for generated rests).
- No slur intention + gap -> no tie (nothing to bridge).
So the toggles gate the REALIZATION of generated intentions across gaps; they never generate ties.
