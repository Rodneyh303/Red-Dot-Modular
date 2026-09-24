# Spread target modes — graded correlation (Rodney)

STATUS: DESIGN, agreed in discussion. One open question (mode switching UI). Not built.

## The change in one line
Spread's interpolation TARGET is hardwired to voice 1. Make it selectable — and in particular let it
FOLLOW CA's pin map — so CA authors WHICH voices are related and spread grades HOW MUCH.

## Where it stands today
`SpreadInterp` interpolates each voice's draw toward the MONO (voice-1) draw of the SAME lane:
`r = (1-|a|)·d + |a|·t`, with `t = p` for positive spread and `t = 1-p` for negative (the mirror).
A voice targeting itself is a no-op, so voice 1 is a fixed anchor. Full spread = unison with V1;
full negative = exact complement of V1. Amount is already PER (voice, lane).
(The former AVERAGE_POLY target was removed — the mean of N iid draws concentrates at 0.5, so full
spread collapsed everything to mush. Targeting the mono draw gives unison, a real destination.)

## Three target modes
1. **Fixed V1** — today's behaviour; the degenerate case, keep as default.
2. **User-chosen fixed voice** — anchor = any voice k. Useful when CA isn't in the chain, and it lets
   spread agree with a CA collapse that nominated a leader other than V1 (today they disagree, so
   "partial collapse toward the CA leader" is not expressible).
3. **Follow CA** — anchor = `src[v]`, the voice CA says v is correlated to. Spread then means "how
   strongly do I adhere to MY OWN leader". This is the mode that pays off.

## Why follow-CA is the right shape
**CA sets WHO (binary, one source per voice). Spread sets HOW MUCH (continuous, signed).** Neither
gives graded correlation alone. Follow-CA gets per-voice targets WITHOUT Sands authoring a single voice
relationship — CA stays the sole author of the voice frame; spread supplies only the scalar.

## Reachable correlation structures (worked)
With `k(a) = a/√((1-a)² + a²)` (so a=0.5 → 0.71 — the mapping is concave; most travel happens early):
| CA state | Structure | Notes |
|---|---|---|
| Identity diagonal | Diagonal — no correlation | spread is a no-op (every voice targets itself) |
| Today (fixed V1) | **Signed rank-1 star** | one common factor; range already -1..+1 between any two voices |
| Collapse / manual pins | **Block-diagonal, rank-1 per block** | arbitrary PARTITION, per-voice strength+sign, zero across blocks |
| Scatter (cycles) | **Neighbour-only coupling** | e.g. all a=0.6 in a cycle → 0.46 adjacent, 0 otherwise (banded, not a star) |

**Not reachable, and that's correct:** an arbitrary correlation matrix. Within a block it stays rank-1,
so `corr(1,2)=0.8, corr(1,3)=0.8, corr(2,3)=0` is impossible. Reaching it needs per-voice MIXES of
sources (a weighted 16x16 matrix) — which is both mush and far too many params. Rank-1-within-a-block
encodes exactly what the ear tracks (grouping, leading/following, together/opposed), which is the shape
real ensembles have. The restriction is doing musical work.

**Cross-lane correlation stays ZERO and is unreachable by spread** — each lane targets its OWN lane's
anchor, and the streams are domain-separated in the RNG. Voices can become jointly identical across all
lanes (unison), but rhythm never becomes correlated with melody. Cross-lane coupling would need a shared
SOURCE between streams — a different mechanism. (Rodney's separate call: cross-stream correlation is not
wanted; see CHANGE_ALLEY_DESIGN.md.)

## Musical payoff
- **Ensemble tightness** — per voice, per lane, how strictly a player follows their section leader:
  unison → loose → independent → answering. A control real ensembles have and modular usually doesn't.
- **Antiphony (the headline)** — two independent choirs (e.g. V2,V3 → leader V1; V5,V6 → leader V4),
  zero correlation across. The call-and-response is NOT scripted: two uncorrelated patterns take turns
  occupying the bar. Not reachable today (one hub ⇒ every voice tied to V1, no second group to answer).
- **Interlocking parts from one number** — negative spread makes a voice play the complement of ITS OWN
  leader, so a section can hold adherents AND an answering voice with no extra mechanism.
- **Gradient/smear** — a scatter cycle gives similarity that decays with distance across the voice field
  (spatial if the voices are panned), rather than a hub-and-spokes texture.
- Soloist + accompanists with individually dialled adherence (the classic star, but anchored anywhere).

## Boundary: single anchor, NOT a per-voice target map
Spread gets ONE anchor per lane (or per module). A per-voice target map (v follows t(v)) IS the CA pin
matrix; rebuilding it in Sands would duplicate CA and break the one-authoritative-voice-frame rule.
Follow-CA gets the per-voice behaviour by READING CA, not by re-authoring it.

## Could spread have replaced CA? No.
At a=1 with per-voice targets spread reproduces the MAPPING, but not: the VERBS (collapse/rotate/
reflect/scatter — operations on the mapping), REVERSIBILITY (Philox counters, true-reverse replay),
the STAGE (CA reorders source assignment UPSTREAM so gates/rests/accents/pitch follow coherently and
Intertropical + Keppel inherit it; spread blends values late, per lane), or REACH (the correlation pairs
permute EXTERNAL poly CV; one CA can serve several Monsoons). They are orthogonal axes of one idea:
CA discrete/structural/upstream/authored-once; spread continuous/value-level/downstream/per-voice-lane.

## OPEN — how to switch mode (the one unresolved question)
PRECEDENT: the earlier 2-option spread target (Average Poly / Mono Draw) was a **Monsoon CONTEXT MENU**
item and the **single source of truth**, mirrored onto the engine so every visual SpreadManager read it
(replacing per-visual `interpUseMono` flags). When AVERAGE_POLY was deleted the field went with it —
`Monsoon.hpp:658` still carries the ORPHANED COMMENT describing it (clean that up).
Proposal: reuse that pattern — Monsoon context menu, single source of truth, mirrored to the engine.
Sub-questions:
- **Global or per-lane?** Global is simpler and matches the precedent; per-lane would allow different
  anchors per lane (e.g. melody follows CA, rhythm anchored to V1) at the cost of 4-5x the UI.
- Mode 2 needs a TARGET VOICE picker as well as a mode (submenu 1..16, or a param).
- Persist in JSON like the old flag did.

## Other OPEN items (small)
- **Which frame is a fixed target index in** — pre-CA (Straits voice) or post-CA? For composing with
  collapse it should be named in the same frame as CA's leader, or the anchor drifts when pins move.
- **Fallback** when the chosen target ≥ active voice count → fall back to voice 1 (today's behaviour).
- **Keep the anchor PRE-spread** (as the code does now): correlation is then one hop only and cycle-proof.
  Using the leader's POST-spread value would propagate transitively and become ill-defined the moment
  CA's pins form a cycle — which a permutation easily does. One hop is the deliberate choice; its cost is
  that a chain 1→2→3 gives corr(1,3)=0.
- Manual pins are overwritten when a verb fires on that stream — lock mode protects a hand-authored
  partition.
