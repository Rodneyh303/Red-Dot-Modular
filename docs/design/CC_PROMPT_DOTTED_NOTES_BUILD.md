# CC PROMPT — dotted note values (OPTION B) — interleave by duration + NAME every hardcoded index

Rodney chose OPTION B (interleave by duration for a natural dial order) and reverted the prior attempt. The
prior attempt broke subgate tests ONLY because index literals were hardcoded and the insertion shifted
them. Option B does it RIGHT: insert dotted by duration AND replace EVERY hardcoded note-value index with a
NAMED constant / flag, so the table order can change freely and nothing breaks.

## KEY FACT (Rodney is right — gate mode has NOTHING to do with note length)
SequencerEngine.cpp:1295 "Mode B: the note DURATION is Gate 1's width, so the INTERNAL note length is
NULLIFIED." Lines 1304 and 1430 set `nvIdx = 6 /*1/16 = 1 step*/` purely as a NULLIFICATION SENTINEL — "so
the internal countdown does not govern." Gate mode reacts to gate rise/fall; it does NOT use note length.
The `6` is a magic "disable the length countdown" value, and that is EXACTLY what broke the subgate tests
when indices shifted (6 stopped being the inert 1-step value). FIX IT PROPERLY: gate mode passes a NAMED
nullification constant, immune to table reordering.

## 1. NAME THE INDICES (do this FIRST, before touching the table — it's a no-op refactor, pin goldens)
In NoteValues.hpp, add named indices (and/or a kind/flag on the struct). Then replace every hardcoded
literal. Sites (exhaustive):
- SequencerEngine.cpp:733 `pe_clamp(requestedIdx, 0, 7)` → `0, NUM_NOTE_VALUES-1`.
- SequencerEngine.cpp:741 `pe_clamp<int>(..., 0, 7)` → `0, NUM_NOTE_VALUES-1`.
- SequencerEngine.cpp:1304 `nvIdx = (modeSelect==1) ? 6 : ...` → `? NV_GATE_NULLIFY : ...` where
  NV_GATE_NULLIFY is a named constant = the index of a 1-step (1/16) value. BETTER: since gate mode
  nullifies length, nvIdx here should be "whatever index yields a 1-step inert countdown" BY NAME, not a
  magic number. (Even better long-term: gate mode shouldn't route through the note-length path at all —
  OUT OF SCOPE for dotted, but leave a TODO comment: "Mode B nullifies note length; routing through
  getNoteLenIdx is vestigial.")
- SequencerEngine.cpp:1430 `int nvIdx = 6;` → `= NV_GATE_NULLIFY;` (same nullification constant).
- PatternEngine.cpp:329-331 triplet gates `idx==3/5/7` → gate by a straight/triplet/dotted FLAG on the
  NoteValue struct (e.g. `NOTE_VALUES[idx].kind == TRIPLET && (mask & tripletBit(idx))`), NOT magic index.
GOLDEN PIN: this step is a PURE REFACTOR (named constants == current literals) → goldens BYTE-IDENTICAL
BEFORE any table change. Prove it. THEN change the table.

## 2. INSERT 4 dotted by DURATION (NoteValues.hpp), NUM_NOTE_VALUES 8 → 12
New order (slowest→fastest, dotted interleaved):
    1/1, 1/2, 1/2.(0.75), 1/4, 1/4.(0.375), 1/4T, 1/8, 1/8.(0.1875), 1/8T, 1/16, 1/16.(0.09375), 1/32
(= 12 entries. dotted-1/32 EXCLUDED — needs 48 PPQN, unsafe; see PPQN_48_96_NOT_SAFE.md.) fractions:
1/2.=0.75(12 steps), 1/4.=0.375(6), 1/8.=0.1875(3) integer; 1/16.=0.09375(1.5) fractional.
allowedPPQN: integer dotted wide; dotted-16th like 1/32 = 4 only. Set each NoteValue.kind =
STRAIGHT/TRIPLET/DOTTED so the flag-based gates (step 1) work regardless of order.
Because step 1 NAMED everything, this insertion shifts NOTHING that matters — NV_GATE_NULLIFY resolves to
the new 1/16 index, triplet gates follow the TRIPLET flag, bounds use NUM_NOTE_VALUES-1. Subgate tests stay
byte-identical.

## 3. DOTTED MASK BITS (per-dotted, Rodney) — bits 3-6, gated by the DOTTED flag + which dotted
allowed() for a DOTTED-kind value checks its own bit. Map each dotted to a bit (3=1/2., 4=1/4., 5=1/8.,
6=1/16.). Default OFF (noteVariationMask default stays 0b111). Base always active (existing i==baseIdx).

## 4. UI — the stepped NOTE VALUE knob: 12 detents, natural duration order (Option B's payoff)
- Because dotted are interleaved by duration in the TABLE, the dial (MonsoonWidget.cpp:860, draws
  NOTE_VALUES[i].label in index order) is ALREADY in duration order — NO display-sort needed. That's why
  Option B is cleaner for the dial.
- Bump the detent count to 12: param quantization rounds to nearest of 12; arcLabel loop is already
  `i<NUM_NOTE_VALUES`. Confirm the NOTE_VALUE param range/step = 12.
- Per-dotted submenu toggles in "Note Division and Clock" (MonsoonWidget.cpp:1459), mirroring
  add("Allow 1/4T",0): add("Allow 1/2.",3); add("Allow 1/4.",4); add("Allow 1/8.",5); add("Allow 1/16.",6);

## 5. LEGATO — FREE (noteCanLeadLegato integer-steps): dotted 1/2/1/4/1/8 lead, 1/16. can't. No change.

## PINS
- STEP 1 (naming) goldens BYTE-IDENTICAL (pure refactor). Prove before step 2.
- STEP 2+ with dotted bits DEFAULT OFF → ALL goldens byte-identical INCLUDING SUBGATE suites (gate mode
  nullifies note length via the NAMED constant, so it's immune to the reorder). If a subgate test moves, a
  literal was missed in step 1 — find it.
- NEW dotted verified; dial 12 in duration order; run_all + strict green incl subgate.

## DONE =
- Every hardcoded note-value index NAMED (NV_GATE_NULLIFY, NUM_NOTE_VALUES-1 bounds, triplet/dotted FLAG
  gates) — pure-refactor goldens bit-identical first.
- 4 dotted interleaved by duration (12 total); per-dotted mask bits 3-6 (flag-gated), default OFF; 4 submenu
  toggles; dial 12 detents natural duration order (no display-sort needed).
- Legato free; gate mode note-length nullification robust (named, TODO for the vestigial routing).
- Goldens byte-identical (dotted off) INCLUDING subgate; run_all + strict green.

════════════════════════════════════════════════════════════════════════════════
## THE TWO ORDERS — ENGINE (strict duration) vs DIAL (grouped). COPY VERBATIM, DO NOT SORT (Rodney)
CC kept mis-sorting. Here are the exact arrays. DO NOT re-sort them — copy them literally.

### ENGINE NOTE_VALUES[] — STRICT note-length order, longest → shortest (indices 0-11)
The engine REQUIRES strict monotonic-decreasing duration by index (the varyNoteIndex distance/legato length
math assumes index order == duration order). This order is STRICTLY DECREASING (verified):
    idx 0 : 1/1    (16.0 steps)
    idx 1 : 1/2.   (12.0)
    idx 2 : 1/2    ( 8.0)
    idx 3 : 1/4.   ( 6.0)
    idx 4 : 1/4    ( 4.0)
    idx 5 : 1/8.   ( 3.0)
    idx 6 : 1/4T   ( 2.6667)
    idx 7 : 1/8    ( 2.0)
    idx 8 : 1/16.  ( 1.5)
    idx 9 : 1/8T   ( 1.3333)
    idx 10: 1/16   ( 1.0)
    idx 11: 1/32   ( 0.5)
NOTE: 1/8. (3.0) is LONGER than 1/4T (2.667), and 1/16. (1.5) LONGER than 1/8T (1.333) — that's correct by
actual duration, so in STRICT order dotted-8th precedes quarter-triplet and dotted-16th precedes 8th-triplet.
This is the ENGINE table. fractions: 1/1=1.0, 1/2.=0.75, 1/2=0.5, 1/4.=0.375, 1/4=0.25, 1/8.=0.1875,
1/4T=1/6, 1/8=0.125, 1/16.=0.09375, 1/8T=1/12, 1/16=0.0625, 1/32=0.03125.

### DIAL — GROUPED order (reads naturally: each base flanked by its dotted/triplet)
The DIAL shows a DIFFERENT order via a permutation (so the engine keeps strict duration, the dial reads
nicely). DIAL order: 1/1, 1/2., 1/2, 1/4., 1/4, 1/4T, 1/8., 1/8, 1/8T, 1/16., 1/16, 1/32.
Implement as a fixed permutation (NOT a re-sort of the engine table):
    constexpr int DIAL_ORDER[12] = { 0, 1, 2, 3, 4, 6, 5, 7, 9, 8, 10, 11 };  // dial slot -> engine index
    // (inverse is the same array here: ENGINE_TO_DIAL[12] = { 0,1,2,3,4,6,5,7,9,8,10,11 })
- Dial draw: label at slot k = NOTE_VALUES[DIAL_ORDER[k]].label (MonsoonWidget.cpp:860 loop).
- Knob param → engine index: engineIdx = DIAL_ORDER[ round(param to slot) ].
- Stored engine index → knob position: slot = ENGINE_TO_DIAL[engineIdx].
Only TWO pairs swap between the orders (1/8.↔1/4T at engine 5/6, 1/16.↔1/8T at engine 8/9); everything else
is identity. The engine NEVER sees the dial order — it only ever uses its strict-duration indices.

### CC: the failure you keep making
"Sort the table longest to shortest" is the instruction you botch. DON'T SORT. COPY the ENGINE array above
verbatim (it's already strictly decreasing) and COPY the DIAL_ORDER permutation verbatim. Engine = strict
duration (array above). Dial = grouped (via DIAL_ORDER). They are two separate given lists; do not conflate
or re-derive them.
