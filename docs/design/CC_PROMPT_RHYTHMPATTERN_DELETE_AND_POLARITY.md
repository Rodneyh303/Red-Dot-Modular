# CC PROMPT — delete vestigial rhythmPattern (kills polarity smell) + per-lane polarity audit

Two remaining live items from MONO_SPECIAL_IN_LOGIC_AUDIT.md (everything else there is BUILT by the RDM or
retired). Item 1 is a small safe deletion; item 2 is an audit (no code unless it finds a bug). Branch off
lane-expander-refactor.

═══════════════════════════════════════════════════════════════════════════════
## ITEM 1 — DELETE rhythmPattern (vestigial step-LED prediction cache)

### What it is (CONFIRMED by tracing, safe to delete)
rhythmPattern[16] is a precomputed note/rest MAP whose ONLY consumer is STEP-LED BRIGHTNESS:
- SequencerEngine.cpp:468 `bool isNote = pe.rhythmPattern[dnaIdx];` → :469 `baseActive = isNote ? 0.35 :
  0.07` — inside getStepLightBrightness(int) const, which returns an LED brightness float. LIGHTS ONLY —
  verified no gate/CV/audio consumer.
- It is PERSISTED (MonsoonPersistenceManager.cpp:166-169 save, :429-432 load) — pointless (a stale
  prediction the engine regenerates).
- It is filled by a PREDICTION that uses the OPPOSITE comparison from the live audio:
  PatternEngine.cpp:410/964/1079 `rhythmPattern[i] = (warpRow(displayedVoice_, PL_REST)[i] >= in.restProb)`
  — `>= restProb`, while the live audio rest decision uses `< restProb`. So the LEDs are a static
  prediction that can DISAGREE with what plays (the polarity smell). Deleting rhythmPattern removes the
  parallel `>=` comparison entirely → the smell dies by deletion, no `>=`/`<` reconciliation needed.

### The fix — delete it, light the step LEDs from the LIVE decision
1. REMOVE the member + mirror refs: PatternEngine.hpp:666 (rhythmPattern[16]), :748
   (cachedRhythmPattern[16]); Monsoon.hpp:563, :1125, :1152 (the member + the two reference aliases);
   PatternEngine.hpp:8 (comment).
2. REMOVE the fills: PatternEngine.cpp:136 (=true default), :410, :964, :1079 (the `>= restProb` writes),
   :1176/:1182 (the cached save/restore of it).
3. REPLACE the LED source at SequencerEngine.cpp:468: the step LED "is this step a note" should come from
   the LIVE rest decision for that step, NOT a precomputed array. Options (pick the cleanest that matches
   how the engine already exposes per-step state):
   - If the engine has (or can cheaply expose) the resolved rest decision per displayed step, read THAT
     (same `< restProb` logic / the RDM's rest predicate) so the LED matches the audio by construction.
   - A step LED is a PREDICTION of a probabilistic event anyway (the audio rolls live), so it is acceptable
     for the LED to show the resolved/expected rest state for the DISPLAYED voice using the SAME comparison
     as the audio (`< restProb`), i.e. "likely a note" = `warpRow(displayedVoice, REST)[i] < restProb`
     (note: `<`, matching audio — NOT the old `>=`). This keeps a prediction but with the CORRECT polarity,
     and removes the persisted array + the member. CONFIRM with Rodney which (live-decision read vs
     corrected-polarity inline prediction) — default to the corrected-polarity inline form if exposing the
     live decision is costly, since it is the minimal change that kills the smell.
4. PERSISTENCE: drop the save (PersistenceManager.cpp:166-169) and the member write on load. On LOAD of an
   OLD patch that still has a "rhythmPattern" JSON key: IGNORE it gracefully (do not read it into a deleted
   member, do not error) — leave the :429-432 block only as a no-op skip or remove it so an unknown key is
   simply ignored (json read of a now-unused key must not crash).

### Verify
- Builds clean; no rhythmPattern/cachedRhythmPattern symbols remain (grep = 0).
- Step LEDs still light sensibly (note-steps brighter than rest-steps) for the displayed voice, now using
  the `<` (audio-matching) polarity — turning REST up makes MORE steps show dim (rest), consistent with the
  audio (the old `>=` made them INCONSISTENT — this is the fix Rodney observed).
- Old patch with a "rhythmPattern" JSON key LOADS without error (key ignored).
- run_all.sh green; goldens unaffected (this is lights + dead-cache only, no audio path change).

═══════════════════════════════════════════════════════════════════════════════
## ITEM 2 — PER-LANE POLARITY AUDIT (audit, not a build — prerequisite for atom work)

For EACH lane (mel/oct/qmix/rest/accent/variation/legato) document, in a short table in
MONO_SPECIAL_IN_LOGIC_AUDIT.md (or a new POLARITY_TABLE.md): what does prob→0 produce, what does prob→1
produce, is it INTUITIVE or INVERTED, and does ENGINE vs any remaining DISPLAY use the SAME comparison.
- Known: REST audio uses `< restProb` → REST (prob up = more rest, intuitive). VARIATION is known-inverted
  by design (amount>0.5 = SHORTER). Confirm the rest by reading each lane's decision site.
- The POINT: the atom/categorical work assumes prob=1 → the "named"/extreme event (rail). Confirm each
  lane's rail (prob→1) actually IS that lane's extreme in the direction the atom semantics assume. If any
  lane is inverted relative to that assumption, FLAG it (don't fix — just document, so the atom build
  accounts for it).
- Output: the per-lane table. Only raise a code change if a genuine engine-vs-engine polarity BUG is found
  (not just an inverted-but-consistent lane like Variation).

## DONE =
- Item 1: rhythmPattern + cachedRhythmPattern fully removed; step LEDs lit from correct-polarity source;
  old-patch JSON key ignored; grep-clean; run_all green; goldens unaffected.
- Item 2: per-lane polarity table committed; any true polarity bug flagged (code change only if a real bug).
