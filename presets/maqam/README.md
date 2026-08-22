# Maqam tuning + jins presets for dot.modular

A starter library of Arabic **maqam** tunings (`.scl`) and **jins** scale-masks (`.dmtune`)
for the dot.modular microtonal system (Colonnades / Duo / Emerald Hill / Shophouse).

## Attribution (important)
The **non-EDO cent values** (the `Rast_full_15pitch` set and its jins masks) are taken from the
**cents table published by Sami Abu Shumays** in *"A Guide to the Maqam Tuning Presets for Ableton
Live 12"* (https://tuning.ableton.com/arabic-maqam/maqam-guide). Sami Abu Shumays is an Arabic
violinist and vocalist; these values reflect **his own practice in the maqam tradition of Egypt and
Greater Syria**, developed by ear over decades. They are offered by him explicitly as *one
practitioner's version* — in his words, there is no universal maqam tuning, and anyone claiming
otherwise "is lying, or ignorant, or both."

We reproduce only the **cent values** (facts/data), reworked into dot.modular's format, with clear
credit. Please support the source: Ableton's tuning site, Abu Shumays' book *Inside Arabic Music*
(OUP), maqamworld.com, and maqamlessons.com.

## Honest scope
- **Maqam identity is melodic, not just the scale.** A tuning + a mask is NOT "playing a maqam" —
  the maqam lives in its melodic vocabulary, phrasing (sayr), and the habitual modulations between
  ajnas. These presets give you the *pitch material and the modulation targets*, not the music.
- **Regional/personal.** Egypt/Greater Syria, Abu Shumays' ear. Turkish, Persian, Iraqi, Gulf, and
  North African practice differ. Even Syrian vs Egyptian Rast differ (the E-half-flat sits higher in
  Syria). These are one honest reference point, not the reference.
- **The 24-EDO files are equal-tempered approximations** — a keyboard-player convenience, a
  simplification the tradition's own instruments (oud, violin, voice) do NOT use. Good for getting
  started and for ensemble work with 24-EDO keyboards; not "authentic tuning."

## What's here

### Rast family (real non-EDO cents, from the Abu Shumays table)
- `Rast_full_15pitch.scl` / `.dmtune` — the full 15-pitch Rast tuning enabling all common modulations
  (his "Rast 5 – all the modulations"). This is the **shared tuning** the jins masks sit over.
- `Jins_*.dmtune` — jins masks over that tuning, each selecting one jins' notes:
  Rast C, Nahawand C, Nikriz C, Sikah E½♭, Nahawand G, Upper Rast G, Hijaz G, Bayati G, Saba G,
  Hijazkar G, Saba Dalanshin A, Jiharkah C. **Switch between these** (in Shophouse / Emerald Hill
  slots) to perform the intiqal (modulation) between ajnas — the heart of maqam movement.

### 24-EDO family (equal-tempered approximations, keyboard reference)
- `_24EDO_full.scl` / `.dmtune` — the 24 equal quarter-tones (the shared 24-EDO grid).
- `Maqam_*_24EDO.dmtune` — scale masks for Rast, Bayati, Saba, Hijaz, Nahawand, Kurd, Ajam, Sikah
  over that grid.

## How to use (the dot.modular way)
1. Load a **tuning** (`Rast_full_15pitch` or `_24EDO_full`) as the base — the full pitch set.
2. Load **jins masks** into Shophouse / Emerald Hill slots — each slot a jins.
3. **Switch slots** at phrase boundaries to modulate jins→jins (intiqal). With Emerald Hill's 4
   slots you can hold tonic-jins + dominant-jins + two modulation targets.
4. The expressive core — degree emphasis, microtonal inflection, the *sayr* — lives in your playing
   and in the per-degree faders/cents (Colonnades/Duo), NOT in the slot switch. That's where the
   maqam actually speaks.

## TODO (fill from the individual Ableton preset pages, viewable in Live 12)
The non-EDO **Bayati / Saba / Hijaz** cent tables live on their own preset pages as interactive
embeds (not extractable without Live). When you have Live 12 open, transcribe their cents the same
way and add `Bayati_full`, `Saba_full`, `Hijaz_full` + their jins masks here. Only Rast's full table
was on the guide page.
