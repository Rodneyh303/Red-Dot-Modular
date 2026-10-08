# Range lane — panel fitting note (Sands East + Macro)

For the voice-range (cycle/absolute mode) control — a THIN half-height LED lane at the BOTTOM of the
Sands grid showing the voice range with constant-height blocks, animated with the playhead, reshaping
under LOR modulation. (See UNIFIED_ADDRESSING_SYSTEM.md for the behaviour; this is the PANEL fit.)

## Current geometry (gen_east_clean.py)
- Grid: ED_Y=14, ED_H=91, ED_LANES=7 -> ED_LANE_H = **13mm/lane**; grid spans y=14..105.
- Panel H=128.5mm; below grid (105..128.5 ~23.5mm) = Marina Bay waves (y112) + logo (y113) + footer.
- 48HP (W=243.84). Right strip: owner(205)/dir(212)/mod jacks/prob-out(236).

## The fit (Rodney)
- **Shave ~1mm off each lane height** (13 -> ~12mm): 7 lanes x 1mm = **~7mm freed**.
- **Add a HALF-HEIGHT range lane at the bottom** of the grid (~6.5mm = half a lane). Grid 14..98, range
  lane ~98..104.5. Waves/logo below largely intact.
- The range lane needs its **modulation inputs + knobs EXCEPT spread**, on the LED's LEFT side (LEN, OFF,
  ROT — no SPR, since range isn't spread).

## EAST range lane needs
- Delegation + direction controls + gate ins (as the other lanes have).
- NO probability-out. Reuse that jack for a **MODE modulation gate IN** (toggles CYCLE <-> ABSOLUTE).
- Manual mode switch: **click the range lane itself** to toggle cycle/absolute.

## MACRO range lane needs
- Direction switch + direction gate-mode in.
- NO probability-out. Reuse the jack for the **MODE switch** (gate in, cycle<->absolute).
- NO mix-in dials.

## Layout consequence
- Shaving 1mm/lane + a half-lane needs the knobs/jacks/buttons that require vertical height to be
  re-fit. **Move the logos and the Sands/helix (DNA) art around** to make room for the fit knobs/jacks/
  buttons. The art is repositionable; the controls need the height.
- Both East and Macro generators (gen_east_clean.py, gen_sands_macro*.py) + the widget draw (SandsGrid /
  SandsVisualEditorV4) must update together (lane height, the new half-lane, left-control rows, art).

## Status / related
- Behaviour spec: UNIFIED_ADDRESSING_SYSTEM.md (voice LOR, cycle vs absolute = output-gate).
- Transport sync fix (clock/phase KNOB -> light ring / Sands playheads; it was the knob) is on
  branch lane-expander-refactor (commit 23aa975b "fix: transport sync — reset step 16 flash, laneTick
  0-based, phase knob drift").
