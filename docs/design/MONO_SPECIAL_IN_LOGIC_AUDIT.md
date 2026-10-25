# AUDIT — "mono is special" surviving in DECISION LOGIC (not just indexing) (Rodney)

Strong types killed mono-is-special at the INDEX level (warpRow(0)/voices[0] won't compile). But mono
still leaks into the DECISION LOGIC: poly voices' articulation (legato/rest/accent/tie) is structurally
GATED ON MONO's decision, not resolved per-voice. This is the next frontier of "drive out mono is
special" — deeper than indexing, because it's in the sequencing algebra.

## THE SYMPTOM (Rodney): poly legato is gated on mono legato
A poly voice can't make its OWN legato decision independently — its whole articulation window is derived
from mono's gate. So per-voice legato (the point of the categorical-articulation / atom work) is blocked.

## THE SITES (SequencerEngine.cpp — verified)
POLY PATH (executePolyVoice, :1112): the poly voice's decision is gated on MONO's decision:
- :39-42 `monoGateStart = (lastStepResult.decision == NewNote) || ((Legato||LegatoMax) && !wasHeldMono
  && !hadMonoTail)` — the poly voice only (re)decides AT A MONO GATE START. Its articulation window is
  mono's, not its own.
- :33-34 comment: "LOR is identity (delegated → reads mono's step)… the voice can never hold past mono's
  next event — articulation is subtractive." That's the DELEGATED contract — correct for delegated
  voices, WRONG for voices that should be independent.
So poly articulation timing = mono's gate timing. Fine when the voice is DELEGATED to mono; a MONO-SPECIAL
residue when the voice should be independent.

MONO PATH (executeStep, :493): reads voices[monoSlot].{legatoProb,restProb,accentProb,qmixLevel} and
lastStepResult.decision throughout (:562,613,623,628,679,696,…). For V1 this is CORRECT (it IS V1's
path). The issue is not these reads — it's that the POLY path DEFERS to them instead of having its own.

## THE DISTINCTION (what's a bug vs what's intended)
- DELEGATED poly voice (follows Macro/mono by design): reading mono's gate IS correct — delegation means
  "track the leader". Do NOT change this.
- INDEPENDENT poly voice (per-voice articulation wanted): deferring to mono's gate is the BUG. It should
  resolve its OWN legato/rest/accent on its OWN draw and its OWN gate window.
So the fix is NOT "never read mono" — it's "a poly voice reads mono ONLY when delegated; when independent,
it resolves its own articulation." The ownership/delegation state (already resolved by the SandsModifier
resolver) decides which — the SAME authority that governs spread/warp/direction delegation should govern
ARTICULATION delegation. Right now articulation is HARDCODED to always-delegate-to-mono.

## THE FIX (future — scope it with the resolver/rate work, not now)
Route poly ARTICULATION through the delegation authority like every other lane:
- Independent voice → resolve legato/rest/accent/tie on its own draw + own gate window (its own
  monoGateStart-equivalent keyed on ITS decision, not lastStepResult/mono's).
- Delegated voice → track mono (current behaviour, preserved).
The decision of which = owner()/delegation (the resolver already knows). This makes articulation a
per-voice resolved quantity like spread/warp/direction — closing the LAST big mono-special in the logic.
CAUTION: the gate-window / slur-forward machinery (gs.slurForward, wasHeld/hadTail) is subtle; an
independent voice needs its OWN gate-state continuity, not mono's. This is real engine work (its own
red-first tests: independent poly voice legatos on ITS legato, not mono's; delegated still tracks).

## ANY OTHER GATE DECISIONS PREDICATED ON MONO (the broader audit Rodney asked for)
Same pattern to check/fix when articulation is un-delegated:
- REST: poly rest gated via monoGateStart / slurSuppressesRest keyed on mono (:628,680,697).
- ACCENT: result.accented at :679 reads voices[monoSlot].accentProb at the MONO decision; poly accent
  should be per-voice when independent.
- QMIX: :562,696 voices[monoSlot].qmixLevel — same.
- TIE/MidNote/LegatoMax: all flow from the mono `result.decision` cascade; poly inherits it.
GENERAL RULE (the "mono is special" eradication, logic edition): every per-voice ARTICULATION/GATE
decision should resolve through the delegation authority (own-draw when independent, mono when delegated)
— NEVER hardcoded to mono. Audit every `lastStepResult.decision` / `voices[monoSlot].*` read in the POLY
path; each is either (a) correctly delegated, or (b) a mono-special residue to route through ownership.

## STATUS
NOTE ONLY — not scheduled. Fits naturally AFTER the rate manager (the resolver + per-voice machinery it
builds on is the vehicle). Flagged here so it's not lost: poly articulation is the last big mono-special,
and it's in the LOGIC, which strong types can't catch — only a per-voice-resolved-articulation refactor
(+ tests) closes it.

────────────────────────────────────────────────────────────────────────────────
## REVISIT (after rate manager lands) — direction/delegation mod detection + step-boundary latching (Rodney)

Two related refinements for the dir-mod / deleg-mod CV-edge handling (StraitsEastSandsVisual, the
gateModDiv /8 scan + the dirModEdges/delegModEdges counters). NOTE ONLY — revisit when the rate manager
changes land (it will own gateModDiv as a detector-producer, so the right time to reconsider the whole
mechanism).

### 1. A more STANDARD edge-detection mechanism?
Current: a /8-divider scan (gateModDiv) walks each dir/deleg CV input, compares voltage>1V vs a per-
channel prev[], increments an edge COUNTER (dirModEdges[lane][ch]), and the consumer reads a wrap-safe
unsigned COUNTER-DIFF (n - dirModSeen) to consume accumulated edges. It works, but it's a hand-rolled
accumulate-and-diff. STANDARD alternatives to weigh:
- rack::dsp::SchmittTrigger / BooleanTrigger per channel (Rack's idiomatic edge detector — hysteresis,
  no manual prev[] bookkeeping). The reason it may not have been used: these are per-sample and there are
  EAST_LANES×16 of them (cost), hence the /8 counter approach. Under the rate manager, the detector is a
  Sample-tier producer anyway — so SchmittTrigger-per-channel at the detector tier may be both cleaner
  AND correctly placed.
- A single generalized "CV-edge-count" detector-producer in the rate manager that any lane/channel
  registers with — one mechanism, not a bespoke scan per expander (dir-mod, deleg-mod, and any future
  CV-edge input share it). This is the rate-manager-native version and the likely end state.
Decide when the rate manager owns the detector: keep the counter-diff, or move to SchmittTrigger /
a shared CV-edge producer. Preserve the gateModSeenInit settle (adopt counts on rebuild, no spurious burst).

### 2. Apply dir/deleg changes only at the STEP BOUNDARY (worst case)
Current: edges accumulate continuously (/8) and the counter-diff is consumed in process/draw, so a
direction FLIP or delegation CHANGE can take effect MID-STEP. That's musically incoherent — a lane
reversing direction or ceding/claiming delegation partway through a step. CONSTRAINT to add: a detected
dir/deleg edge should be LATCHED and APPLIED at the next STEP BOUNDARY (worst case), not mid-step — the
same cadence discipline as slew (a distributional param latched per phrase) and the gate-window rules.
So: detect continuously (catch the edge), but APPLY at the step boundary. Under the rate manager this is
natural: the detector is Sample-tier (catches the edge), the APPLICATION is a Step-tier consumer (reads
the latched edge at the step). i.e. detection rate ≠ application rate — exactly the tiered structure the
manager provides. Worth doing as part of the gateModDiv → detector-producer migration.

STATUS: note only; revisit when the rate manager migration reaches gateModDiv (it's in scope as a
detector-producer). Both refinements fit the manager's tiered model (Sample detect, Step apply) and the
shared-detector idea — so they're natural to fold in THEN rather than now.
