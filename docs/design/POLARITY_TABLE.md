# POLARITY TABLE — the per-lane probability→outcome audit

Per CC_PROMPT_RHYTHMPATTERN_DELETE_AND_POLARITY.md ITEM 2: for each lane, what does
prob→0 produce, what does prob→1 produce, is it INTUITIVE or INVERTED, and does the
ENGINE vs any remaining DISPLAY use the SAME comparison. The POINT: the atom/categorical
work assumes prob=1 → the "named"/extreme event (the rail). Sites cited by content
(line numbers drift — re-locate by the quoted law).

## The table

| Lane | The engine law (the comparison) | prob → 0 | prob → 1 | Verdict | Engine ↔ display |
|---|---|---|---|---|---|
| REST | `r_rest < restTh` → REST. Mono: SequencerEngine.cpp `else if ((r_rest < restTh) && !slurSuppressesRest)`; poly: `if (r_rest < restTh)`. restTh = the subgate-resolved per-voice threshold (subRes[subLevel][REST] : voices[v].restProb) | never rest — every step a note | always rest — silent | INTUITIVE (knob up = more of the lane's name) | The only engine-side display comparison was the `rhythmPattern` LED cache — see the note below; it dies in ITEM 1 and its replacement uses the SAME threshold (the complement, by construction) |
| ACCENT | `r_accent < accTh` → accented (mono `result.accented = (r_accent < accTh)`; poly draws its own accent LOR vs its own accentProb at the chain onset + on re-strike) | no accents | every onset accented | INTUITIVE | Consistent (the poly seed comment's law `1.0 < accentProb is false → no accent` matches the decision sites) |
| LEGATO | `r_legato < legatoProb` connects; `legatoProb >= 0.999` (LegatoMax) forces (the slur checkpoint `survives` + the onset commit) | never tie — every note re-articulates | always tied | INTUITIVE | Consistent |
| QMIX | `r_qmix >= qmixLevel` → use GENERATED, else the quantised external CV (mono + poly + the qmixHit latch) | always quantised (external CV) | always generated (the lane's own material) | A LEVEL axis, not an event prob — the `>=` is the deliberate §29 fix ("this was `r < level`, i.e. high q-mix = generated — the opposite") | Consistent mono ↔ poly; the quantiser green flash (FADER_SEQ_QUANT_COLOURS) reads the same decision |
| VARIATION | BIPOLAR around 0.5 — `varyNoteIndex` expands the note-value window by the bias (`if (|var − 0.5| < ε) return baseIdx`); the gate-mode step-advance uses variationProb ∈ [−1, +1], 0.5/0 neutral | longest (window opens downward) | SHORTEST (0.5 = the base note value) | INVERTED-BY-DESIGN (documented, not a bug) — the 1-rail's named extreme is "shortest", and the axis is really bipolar: treat 0.5 as the named base, not 0 | Consistent (the editor shows the bipolar value) |
| MELODY | A VALUE-selection, not an event prob — the per-step value positions the draw within the ACTIVE SCALE MASK | the bottom degree of the mask | the top degree of the mask | Value axis — prob=1 → the top of the selection; the atom assumption holds with "named" = topmost | Consistent |
| OCTAVE | A VALUE-selection — the register within the octave range | the bottom octave | the top octave | Value axis — same as MELODY | Consistent |

## Conclusions

1. **No genuine engine-vs-engine polarity bug.** REST / ACCENT / LEGATO are all
   `draw < threshold` (uniform, intuitive); QMIX's `>=` is the deliberate §29 level
   semantics and is consistent across mono and poly; VARIATION is the one
   inverted-by-design axis (bipolar, 0.5-neutral, 1 = shortest).
2. **For the atom work:** every lane's 1-rail is its named extreme EXCEPT VARIATION —
   and VARIATION should be modelled bipolar (0.5 = the named base), not 0→1. MEL/OCT
   are value-selections (the rail = the top of the mask/range), not event probabilities.
3. **The rhythmPattern polarity note (feeds ITEM 1):** the CC prompt framed the fills'
   `>= restProb` as "the OPPOSITE comparison" from the audio's `<`. It is not — it is
   the correct COMPLEMENT: the audio rests when `draw < restTh`, so "this step is a
   note" is `draw >= restTh`, exactly what the fills computed (boundary-inclusive:
   `draw == restTh` is a note in both). The prompt's suggested replacement
   (`draw < restProb` for "likely a note") would have INVERTED the LEDs. The REAL
   defect is STALENESS — the fills captured the row + threshold at generation time,
   so live REST edits / LOR moves / subgate tiers lag or disagree with the rolled
   audio. ITEM 1's replacement (confirmed with Rodney) is the corrected live
   prediction: `isNote = warpRow(displayedVoice, REST)[dnaIdx] >= the live resolved
   restTh` — the complement of the audio's `<`, bright = note, dim = rest, tracking
   live edits.
