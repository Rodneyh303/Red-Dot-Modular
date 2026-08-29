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

## Are the 24-EDO maqam presets mask rotations of each other? Shared tuning, DISTINCT masks (not rotations)

Checked cents[] + enabled[] across the 24-EDO maqam presets (Rast/Bayati/Hijaz/Kurd/Nahawand/Ajam):
- TUNING is SHARED: every one has identical cents[] = 0,50,100,...,1150 (all 24 equal quarter-tones of
  24-EDO). So they ARE all masks over the SAME 24-EDO grid -> in principle rotation-relatable.
- But the MASKS are NOT rotations of each other:
    Rast     100010010010001000100100
    Bayati   100010010010001000101000
    Hijaz    100010100000101000101000
    Kurd     100010100010001000101000
    Nahawand 100010100010001010001000
    Ajam     100010001010001000100010
  A rotation preserves the bit-count AND the circular gap-pattern (just offset). These differ in
  popcount and, more importantly, in the SEQUENCE OF GAPS (interval structure). All start at degree 0
  (same tonic) -- not shifted to different roots. Rast's step pattern (quarter-tone gaps ~4,3,3,4,4,3,3,
  half-flat thirds) is a DIFFERENT SHAPE from Hijaz's (augmented-2nd signature). You cannot rotate one
  onto another.

### So: shared tuning, distinct scale SHAPES -- and that's musically CORRECT
Maqamat are genuinely different scales, not modes of one another (unlike Western church modes, which ARE
rotations of the major scale). Arabic maqam theory builds them from different ajnas. So representing them
as SEPARATE masks over a shared 24-EDO tuning correctly captures "different shapes", not "rotations of
one shape". Siblings over a shared grid, not rotations of a parent.

### But this IS why the rotation feature is complementary
Because they share the 24-EDO tuning, the mask-ROTATION control lets a user take any one (e.g. Rast) and
rotate its mask to play a MODE of it ("Rast from the 4th degree") that ISN'T a shipped preset. So: the
presets = the named canonical maqamat (distinct shapes); rotation = the tool to explore modes of EACH.
Complementary -- presets are shapes, rotation explores each shape's modes.

### Caveat: the non-EDO jins presets may differ
The root-suffixed jins presets (Jins_Rast_C, Jins_Bayati_G, ...) have ACTUAL microtonal cents (not equal
24-EDO) and are named WITH roots -- some of THOSE may be genuine TRANSPOSITIONS (same jins shape+tuning,
different root), which is a different relationship (transposition, not mask rotation). Not checked here;
this finding is specifically the 24-EDO maqam set: shared tuning, distinct masks, not rotations.

Cross-ref: TONIC_TRANSPOSE_BUILD_BRIEF (presets = tuning+mask pairs, rotation = live transform; this
confirms the 24-EDO set shares a tuning so rotation applies within it), the rotation-family sections.
