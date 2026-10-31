# CC PROMPT — dotted note values (REWRITE) — APPEND to keep indices 0-7 stable, no index shift

PRIOR ATTEMPT BROKE subgate + other tests because dotted values were INSERTED INTERLEAVED BY DURATION,
which SHIFTED the indices of existing values. Many places hardcode note-value indices by NUMBER, including
the GATE/SUBGATE path (which is why subgate tests broke even though subgates have nothing to do with
clock-mode note length). THE FIX: APPEND the 4 dotted values at the END (indices 8-11), so indices 0-7 are
UNCHANGED. No existing index moves → no hardcoded constant breaks → subgate/triplet/gate paths untouched.

## WHY INTERLEAVING BROKE IT (the hardcoded index sites — do NOT shift these)
Current NOTE_VALUES[0..7]: 1/1, 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/32. Hardcoded index refs:
- SequencerEngine.cpp:1304: `(modeSelect==1) ? 6 /*1/16 = 1 step*/ : ...` — GATE MODE fixed note length =
  index 6 = 1/16. **This is why subgate tests broke** — interleaving made 6 ≠ 1/16. MUST stay 6=1/16.
- SequencerEngine.cpp:741: `pe_clamp<int>(..., 0, 7)` — max base index 7. (Becomes 0,11 — see below.)
- PatternEngine.cpp:329-331: triplet gates idx==3 (1/4T), ==5 (1/8T), ==7 (1/32). MUST stay.
- SequencerEngine.cpp:1430 comment "nvIdx = 6 (1/16)".
APPENDING dotted at 8-11 leaves ALL of these correct (0-7 unchanged). This is the whole point.

## 1. APPEND 4 dotted to NOTE_VALUES (src/dsp/NoteValues.hpp), NUM_NOTE_VALUES 8 → 12
Add at the END (indices 8,9,10,11), keeping 0-7 EXACTLY as they are:
    // index 8-11: dotted (appended — order here is index order, NOT dial order; dial sorts by duration)
    {0.75f,     1|2|4, "1/2." },  // index 8  = 12 steps
    {0.375f,    1|2|4, "1/4." },  // index 9  =  6 steps
    {0.1875f,   2|4,   "1/8." },  // index 10 =  3 steps
    {0.09375f,  4,     "1/16."},  // index 11 = 1.5 steps (fractional → PPQN24 bit only, like 1/32)
(dotted-1/32 EXCLUDED — needs 48 PPQN which is unsafe; see PPQN_48_96_NOT_SAFE.md.)
allowedPPQN: mirror the comparable existing value (dotted-16th like 1/32 = 4 only; the integer ones wider).

## 2. UPDATE ONLY the max-index clamps (0,7 → 0,11) — nothing else
- SequencerEngine.cpp:741: `pe_clamp<int>(..., 0, 7)` → `..., 0, 11` (or `NUM_NOTE_VALUES-1`). Use
  NUM_NOTE_VALUES-1 so it never needs manual bumping again.
- Any other `0, 7` / `< 8` / `<= 7` note-index bound → NUM_NOTE_VALUES-1 / < NUM_NOTE_VALUES. GREP for `7`
  and `8` near note/NOTE_VALUES/nvIdx/baseIdx and fix the BOUNDS only. Do NOT touch the hardcoded 6 (1/16),
  3/5/7 (triplet gates) — those are VALUE references, still correct because 0-7 didn't move.

## 3. DOTTED MASK BITS — per-dotted (Rodney), bits 3-6, gating indices 8-11
PatternEngine.cpp varyNoteIndex allowed() currently gates idx 3/5/7 on mask bits 0/1/2. ADD (do not change
the existing three):
    if (idx == 8)  return (in.noteVariationMask & 0b001000) != 0; // 1/2.  (bit 3)
    if (idx == 9)  return (in.noteVariationMask & 0b010000) != 0; // 1/4.  (bit 4)
    if (idx == 10) return (in.noteVariationMask & 0b100000) != 0; // 1/8.  (bit 5)
    if (idx == 11) return (in.noteVariationMask & 0b1000000)!= 0; // 1/16. (bit 6)
All dotted bits DEFAULT OFF (noteVariationMask default stays 0b111, SequencerEngine.cpp:220). Base always
active (existing i==baseIdx rule) → a dotted BASE plays even with its bit off.

## 4. UI — the stepped NOTE VALUE knob gets the 4 extra detents + dial ORDER by duration (Rodney)
- The dial (MonsoonWidget.cpp:860) draws NOTE_VALUES[i].label in INDEX order around the knob. Appending
  dotted at 8-11 puts them at the END of the dial out of duration order (1/1 … 1/32, then 1/2. 1/4. 1/8.
  1/16.) — WRONG visually. Two choices (pick the cleaner):
  (a) DISPLAY-SORT: the dial/param mapping sorts by DURATION for display + maps the knob position to the
      correct INDEX via a sort table, so the knob reads 1/1 … 1/2. … 1/16. in duration order while the
      underlying index stays stable (8-11). This keeps indices stable AND the dial sensible. PREFERRED.
  (b) If display-sort is too invasive, at minimum add the 4 detents (12 total) in index order and accept the
      appended ordering for now, flagged for a later dial-sort pass.
- 12 detents total: the knob param range/step count goes 8→12 (NUM_NOTE_VALUES). Confirm the param
  quantization (round to nearest of 12) and arcLabel loop (already `i<NUM_NOTE_VALUES`) handle 12.
- Per-dotted toggles in the "Note Division and Clock" submenu (MonsoonWidget.cpp:1459), mirroring the
  triplet lines add("Allow 1/4T",0) etc:
      add("Allow 1/2.", 3); add("Allow 1/4.", 4); add("Allow 1/8.", 5); add("Allow 1/16.", 6);

## 5. LEGATO — FREE (noteCanLeadLegato, integer-steps): dotted 1/2(12)/1/4(6)/1/8(3) lead, dotted-16th(1.5)
can't (like triplets). No change.

## PINS (CRITICAL — this is how we know the index-shift is gone)
- indices 0-7 UNCHANGED → index 6 still = 1/16 → GATE/SUBGATE tests BYTE-IDENTICAL. The subgate breakage
  from the prior attempt MUST be gone (if a subgate test still moves, something still shifted — find it).
- dotted bits DEFAULT OFF → ALL goldens byte-identical (dotted is additive, off). If ANY existing golden
  moves, an index shifted or a bound broke — regression signal.
- NEW: each dotted base/variation reachable; dotted-16th no legato-lead; dial shows 12 (duration order if
  display-sort).
- run_all + strict green, INCLUDING the subgate suites.

## DONE =
- 4 dotted APPENDED at indices 8-11 (0-7 untouched); NUM_NOTE_VALUES 12; only max-index BOUNDS updated
  (0,7→NUM_NOTE_VALUES-1), hardcoded VALUE indices (6, 3/5/7) untouched and still correct.
- Per-dotted mask bits 3-6 gating idx 8-11; 4 submenu toggles; all default OFF.
- Dial 12 detents, duration-ordered for display (display-sort preferred); legato free.
- Goldens byte-identical (dotted off) INCLUDING subgate suites; new dotted verified. run_all + strict green.
