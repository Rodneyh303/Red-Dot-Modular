# SCOPING — where should the output mask live? (NOT DECIDED)

Status: **scoping note only. No decision.** Raised by Rodney 2026-11-22: "the output gates don't really
belong on Sands as it is really all about reading probabilities; perhaps they belong on their own module."
Written from a read of the code and docs only (nothing built or run). Rodney also notes there are real
reasons to LEAVE IT ON SANDS — they are set out in full below and are as much a part of this note as the
case for moving it. Nothing here should be read as a recommendation to start.

## Interpretation (check this first)
"Output gates on Sands" is read here as the **output mask**: the per-voice play/mute mask authored on the
Sands Macro (global) and East (per-voice) panels. Sands carries no gate-length or gate-shaping controls
(its "gate" mentions are gate-MOD trigger inputs). If Rodney meant a different surface, this note does not
cover it.

## What the mask is today
- Three kinds (`SequencerEngine.hpp`, `MaskKind`): `MASK_CYCLE` (all on, default), `MASK_DYNAMIC`
  (the voice range [O, O+L) acts as the mask), `MASK_PAINTED` (a drawn pattern).
- Macro = the global mask, East rows = per-voice, delegation per voice (`maskLocalEast_`).
- **Already an output stage**: `MonsoonOutputGenerator` zeroes each voice's GATE, CV, ACCENT, STEP and SLEG
  terminals where the mask mutes the current global step (`outputMaskPasses`). Per the code comments it does
  not touch generation, correlation or the modulation CV inputs; marginals are untouched.
- **Engine-authoritative state**, authored from Sands (`setMaskMacroKind`, `setMaskMacroPatternBits`,
  `setMaskMacroCell` and the East twins), also read by Lantern (pre-mask / post-mask views).
  Same ownership pattern as Cantonment and Change Alley (module edits, engine owns).

## The case for moving it (Rodney's point)
Sands is the probability reader: draws, lanes, ranges, distribution. A mask only silences output after the
read; it adds no information to the probabilities. Keeping a downstream mute on the authoring surface for
the read blurs "what is generated" with "what is let through". A mask is also a natural thing to want to
patch, modulate or arrange independently.

## Candidate homes (unranked, none chosen)
1. **Cantonment** — owns per-voice gate support: a voice's gate is its own term ANDed with its containers'
   envelopes, applied in the same output drive (`Monsoon.cpp` ~1460). A time mask is one more ANDed term in
   that chain; it already applies changes at the phrase boundary and keeps state engine-side. Mismatch:
   Cantonment is about nesting BETWEEN voices, not time regions WITHIN the phrase, so its remit widens.
2. **Intertropical** — the arrangement / output layer, downstream of generation, routing voices to the poly
   outs. `UNIFIED_ADDRESSING_SYSTEM.md` already says voice time-structure is "available out of Straits
   (generation-level) and optionally out of Intertropical (arrangement-level)". Mismatch: its scenes are
   per-voice opt-in across phrases; a within-phrase step mask is a finer grain than it handles today.
3. **Straits Base / the Changi breakouts** — physically own the 16-channel outs and per-voice jacks. The
   natural home of the output STAGE, but not obviously an authoring surface.
4. **A new module** — only if the mask grows beyond what the above can host.

## Reasons to LEAVE IT ON SANDS
These are genuine and may outweigh the case for moving:
1. **The Dynamic kind IS the Sands range.** `MASK_DYNAMIC` derives the mask from the voice's O/L, which is
   authored and animated on the Sands range lane (the "thin extra lane" in the UAS design). Moving the mask
   would either leave Dynamic behind or make the new module a dependent view of a Sands setting.
2. **Rotation coupling (the anchor law).** The painted mask is pinned to the BASE rotation R and re-rotates
   when the base changes, so "the rhythm slides WITH the content" (`SequencerEngine.hpp` Stage 4 notes). R is
   a Sands lane setting; the mask and R are one gesture to the player.
3. **Shared delegation machinery.** Macro/East delegation, per-voice follow/own, locking and the inert states
   are Sands logic the mask already reuses. A new home would need its own Macro/East pairing or a second copy.
4. **The painted strips reuse Sands' lane rendering, animation and stroke-history/undo** (`onMaskToggle` /
   `onMaskStrokeEnd`). One surface where you see the probabilities and the mask over them is a genuine
   composing advantage; the Lantern pre/post-mask toggle exists because the two are viewed together.
5. **Cycle Phrase interaction.** The in-flight Cycle: Phrase / Custom work (O = 0, L = global length, R =
   global offset) makes the Dynamic mask a no-op by default; a move now would collide with that change and
   with its goldens.
6. **Cost vs benefit.** More modules means more adjacency, topology and panel space. The mask is already
   engine-side and an output-only mute, so the architectural purity gain is mostly conceptual, not a bug fix.

## Open questions if this is ever picked up
- Which surface exactly (strips, kind selector, the Macro/East split) and what does Dynamic become?
- Does the new home need Macro/East delegation or is a single per-voice mask enough?
- Where do the Lantern pre/post views and the mask history/undo live afterwards?
- Does the mask want to be modulated or arranged (the real reason to move it)? If not, leave it.

## Suggested trigger to revisit
When a concrete need appears that Sands cannot serve (patching or arranging the mask, or a module that wants
to own gate support), not for tidiness alone. Until then: leave on Sands.
