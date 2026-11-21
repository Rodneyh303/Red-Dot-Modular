# CC PROMPT — CADENCE C0 (prep): consolidate LOR/dir reads behind chokepoints (BIT-IDENTICAL)

Runs BEFORE `CC_PROMPT_CADENCE_C1_LOR_PHRASE_LATCH.md`. C1 latches the EFFECTIVE LOR at a phrase
edge — but today LOR/dir is read from the raw stores at ~a dozen scattered sites across TWO families
with uneven chokepoints, so a latch added in one place would leave the bypass sites reading unlatched
values (the "half-latched LOR" trap — same shape as the two-positions bug). This slice removes that
trap by routing EVERY derive-path LOR/dir read through a single accessor per family. It is a PURE
REFACTOR — BIT-IDENTICAL — so it lands under the old golden-identical discipline, and then C1 has
exactly TWO places to put the latch.

## CARDINAL RULE
VERIFY AGAINST CODE AND TESTS. BIT-IDENTICAL: the accessors return EXACTLY what each raw read returns
today (same delegation, same values). Goldens byte-identical; identity + reverse + phase oracles
green. No behaviour change — only WHERE the value is read from.

## BRANCH
Off `lane-expander-refactor`. Branch `refactor/cadence-c0-lor-read-consolidation`. PR into it.

## FAMILY 1 — VOICE LOR/dir: route ALL derive reads through `effectiveVoiceLor`
`effectiveVoiceLor(v, L,O,R,d)` (SequencerEngine.hpp:905) is already the per-voice chokepoint
(returns the local-East store or the macro fallback). Most of the derive uses it (:722/:798/:876/
:1093). These derive/holder-path sites BYPASS it and read the macro store raw — route them through
the accessor (add a thin macro-row form if the macro row isn't a `v`):
- `anchorTransport` ~:782 — `macroVoiceLor_[LOR_LEN]` (the seed).
- `tickHolders` ~:850-852 — the identity guard (`macroVoiceLor_[...] != cap || macroVoiceLorDir_ != Forward`).
- `deriveHolders` ~:1004-1006 — `int L = macroVoiceLor_[LOR_LEN]; O…; R…` (the macro-row read).
After this, the ONLY raw `voiceLorStore_`/`macroVoiceLor_`/`macroVoiceLorDir_` reads left are: the
WRITES (setup/param-read at :230/:236/:333-343), the accessor's own body, and `pushMaskEditor`
(:378/:395 — EDITOR/display path, NOT derive; leave it, flag for C4). Confirm with a grep.

## FAMILY 2 — STRAND/LANE LOR: introduce ONE effective accessor, route all `getStrandIdx` feeds through it
This family has NO chokepoint. The L/O/R fed to `getStrandIdx` comes from three raw groups:
- MONO strand: `lor(STRAND_…, LOR_LEN/OFF/ROT)` / `strandLen()` (:762/:831/:1383/:1387, getVariationStep
  :2822, getLegatoStep :2858).
- POLY lane: `polyLenE` / `polyOffE` / `polyRotE` / `polyLOR` (:2369-2463, :2521-2523, :1456/:1476).
- MACRO strand: `macroLOR_[s]` (:770/:838/:924) — a THIRD store.
Introduce a single effective accessor — e.g. `effectiveLaneLor(VoiceSlot, LaneId, int& L,O,R, LaneDir& d)`
(poly) and the mono form it delegates to — that returns EXACTLY today's value for that (voice,lane),
preserving the existing delegation (`varlegDelegated`, local-vs-macro, the engine↔editor permutation
`editorLane`/`polyLenE`). Route every `getStrandIdx(...)` call's L/O/R args through it. `getStrandIdx`
itself (the transform) stays; only its L/O/R inputs now come from the accessor.

## STRUCTURE / STAGING (keep goldens green at each step)
Land in two reviewable steps, each byte-identical: (1) FAMILY 1 (voice — small, the accessor
exists); (2) FAMILY 2 (strand/lane — the bigger one; may itself stage mono then poly then macro).
Each step: route reads, run goldens, confirm byte-identical, commit.

## WHAT THIS SETS UP (not in scope here — stated so the accessor shape fits)
C1 will add the phrase-latch INSIDE these two accessors: `effectiveVoiceLor` latched at the GLOBAL
phrase edge (`pos_.phrasePos()==0`), `effectiveLaneLor` at the VOICE-frame edge
(`voiceFramePhase_[v]==0`). So design the accessors as the single read-point they can latch behind —
but add NO latch here. Direction (`d`) rides the same accessors, so C3 benefits too.

## NON-SCOPE
- Any behaviour change (that's C1). The display/editor reads (`pushMaskEditor`) — C4, Rodney's leg.
- `getStrandIdx` internals, the tick/phase holders — untouched.

## ACCEPTANCE
1. `bash test/run_all.sh` — ALL green, march goldens (GOLDEN_A/B/BS, UA_NONDIV) BYTE-IDENTICAL,
   identity + reverse + phase oracles green. This is the whole proof — pure routing changes nothing.
2. Grep proof: after FAMILY 1, no derive/holder-path raw `voiceLorStore_`/`macroVoiceLor_[…]`/
   `macroVoiceLorDir_` read remains (only writes, the accessor body, and `pushMaskEditor`). After
   FAMILY 2, every `getStrandIdx(...)` L/O/R arg comes from the effective accessor, not a raw
   `lor()`/`polyLenE`/`macroLOR_` read.
3. `make RACK_DIR=…` clean (the accessors are shared engine/widget).
4. `test_16_triage` (GATE 1) green — no new anonymous literals.

## DELIVERABLE
On `refactor/cadence-c0-lor-read-consolidation`, PR into `lane-expander-refactor`. Two byte-identical
steps (voice, then strand/lane). This is prep: it unblocks C1's latch by giving it two chokepoints,
and de-risks C3 (direction rides the same accessors).
