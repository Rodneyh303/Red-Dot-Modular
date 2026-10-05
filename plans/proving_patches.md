# Proving patches — one saved .vcv per functional identity (verify + demo + regression)

The claims are DOCUMENTED and CODE-VERIFIED. Sample patches are the THIRD leg: verify the claim against
REALITY (works in Rack, musically, as described), let anyone EXPERIENCE each identity, and serve as a
light regression net. Build as actual saved .vcv patches committed in the repo (examples/ or patches/)
— VCV plugins can ship example patches.

## Why patches, not just instructions
Some identities rest on CHAINS code-reading can't fully confirm COMBINE (esp. the arpeggiator: poly
chord -> per-voice lines -> arrangement redistribution -> sequenced out — four things that each exist;
"do they combine into something that sounds like a generative arp?" is a PATCH question). So patches
are the FINAL verification of composed behaviour. If a patch doesn't produce the claimed behaviour,
RE-SCOPE the claim (same discipline, last stage).

## The six patches (one per verified identity)
1. **Random SOURCE** — Monsoon generating; prob-outs -> scope + a Bernoulli gate, showing the field
   drives external chance. Proves: exposed probability field.
2. **Random ARRANGER** — voices through Intertropical, arrangement reshuffling the ensemble. Proves:
   arranging.
3. **CLOCK/GATE MODULATOR** — clock in -> GATE OUT + STEP GATE OUT -> drum/envelope, rhythm transformed.
   Proves: two-level outputs.
4. **PHASE-TO-GATE CONVERTER** — phase ramp in; note-value-only (clean) then dial variation (generative)
   — the SWEEP is the demo. Proves: optionally-randomising.
5. **SEQUENTIAL SWITCH** — CV into Change Alley; deterministic then dice-reshuffled routing to outputs;
   then reverse it (dice-reverse / true-reverse). Proves: det+stochastic, reversible two ways.
6. **ARPEGGIATION (patch technique)** — poly chord, SPLIT voices, sequential via Intertropical, SHORT
   master pattern length (1/2/4), MODULATE master offset into the 16 steps. A CONSTRUCTED generative
   arp, not a mode. THE key proving patch: confirms whether the recipe reads cleanly as an arp (fair
   claim) or is fiddly (weaker claim / small UX-win opportunity). Downgraded from 'arpeggiator' — honest
   it's a recipe. Also watch: is the recipe a few sensible settings or a precarious combination?

(The MIDI-loop, Burst+prob-outs, rests-become-fills patches from USE_CASE_MIDI_LOOP_VARIATIONS.md fit
under these too — reuse as demo material.)

## Build order — RISKIEST FIRST (not claim order)
Do the composed-behaviour ones FIRST, because those are where a patch might reveal a claim needs
scoping: **(6) arpeggiator, (5) sequential switch** first. Then the simpler, will-definitely-work ones
(1 source, 3 clock modulator), then (2 arranger, 4 phase). If a risky one doesn't work as claimed,
find out NOW and re-scope — better than shipping a claim a patch can't back.

## Triple duty
- VERIFY: each patch confirms its claim works in Rack.
- DEMO: ships as example patches; the chord-in-arp-out and phase-sweep ones are strong leads.
- REGRESSION: a future change that breaks "the arpeggiator patch" is noticed.

## Note (process)
This session had container/branch flakiness that left some docs out of sync (the canonical identities
list was stuck at three until fixed). PROOF-READ the final claim docs fresh before release; don't
assume every recorded edit landed. The patches are themselves a cross-check — if a patch can't be built
to match a documented claim, the doc is wrong.
