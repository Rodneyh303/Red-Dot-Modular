# Fader lamp colours — seq vs quant hits (implementation plan)

Spec: `plans/mode_collapse_6_to_3.md` §"Fader lamp colours — seq vs quant hits".
Colours (Rodney): **red = seq, green = quant, blue = both** (poly only).

## The seam
Today every played note flashes its fader **red** (`semiPlayRemain[sem]` → red channel),
monochrome. The collapse makes a note EITHER generated (seq) OR quantised (quant) per voice,
so the flash must distinguish them. "Both" arises ONLY across different poly voices on the
same fader in one frame (one seq, one quant) — never in mono, never per-voice.

## Current light structure (recon)
- 12 semitone faders, each a `MonsoonLightSlider<GreenRedLight>` — a **2-channel** light
  occupying 2 consecutive `engine::Light` slots: `SEMI_LED_START + 2i` = **green** (the weight
  bar, dim = fader value), `SEMI_LED_START + 2i+1` = **red** (the flash).
- `SEMI_LED_END = SEMI_LED_START + 24` (2 × 12). `GreenRedLight` = 2 channels.
- Flash driven by `GateState::markSemi(sem, dur)` → `semiPlayRemain[sem]` (a per-degree timer);
  `semiLedBrightness(sem) = semiPlayRemain[sem] * 0.25`. `Monsoon.cpp:1173` aggregates max over
  mono `engine.gs` + all poly `engine.voices[v].gs`. `UIManager::updateSemitoneFlashLights`
  sets `lights[SEMI_LED_START + 2i+1].setBrightness(b)` (the red channel).
- Per-voice seq/quant signal EXISTS: `qmixUseGenerated` (engine) = whether the step used the
  generated pitch. seq = `!quantiserPitchSource || qmixUseGenerated`; quant = the converse.

## The conflict
Green is already the **weight bar**. A "green flash" for quant would fight the dim-green
weight → not a clearly distinct colour. Rodney's red/green/blue needs resolving against this.

## Approach (pending colour confirmation)
1. **Light type**: 2ch `GreenRedLight` → 3ch RGB (red/green/blue) per fader. SEMI_LED spacing
   2→3 (`SEMI_LED_END = +36`). Slot 0 = weight (keep green, or go neutral), slot 1 = red flash,
   slot 2 = blue flash. Panel anchors regenerate (semitone LED generator).
2. **Signal**: `GateState::markSemi(sem, dur, bool isQuant)` — a SECOND timer
   `semiQuantPlayRemain[MAXN]` parallel to `semiPlayRemain`. `triggerNote/slideNote/extendHold`
   pass `isQuant` from the engine (the voice's seq/quant decision this note).
3. **Aggregation** (`Monsoon.cpp:1173`): per degree, `seqB[i] = max(voices' semiLedBrightness)`,
   `quantB[i] = max(voices' semiQuantLedBrightness)`.
4. **3-colour logic** (`updateSemitoneFlashLights`): per degree —
   `seq>0 && quant>0 → blue`; `seq>0 → red`; `quant>0 → green`; else off.
5. **Composes with CV2 quantiser-in**: no quant source (cv2Mode≠5, no Straits) → no quant hits →
   faders stay red-only (exactly as today). New colours appear ONLY when quantising.

## Open question
Green is the weight bar today. Rodney wants green = quant flash. Options:
- (a) weight bar → neutral/white; flashes red/green/blue. Cleanest 3-colour separation.
- (b) keep weight green; quant flash = blue, both = magenta (red+blue). Keeps weight, swaps
  Rodney's green↔blue.
- (c) keep weight green dim; quant flash = green BRIGHT (weight is dim, flash is bright — reads
  as a green pulse). Risky (not clearly distinct from weight).
