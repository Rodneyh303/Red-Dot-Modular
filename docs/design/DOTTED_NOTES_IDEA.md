# Dotted note values — idea (try on a branch one day)

STATUS: idea to try on a branch, not scheduled. Low-risk, consistent with existing machinery.

## The idea
Add DOTTED note values (dotted half / quarter / eighth / sixteenth) to the NOTE VALUE set, gated in
VARIATION the same way triplets are. Only ~4 new positions (table grows 8 -> 12), so not much crowding.

## Two kinds of dotted — keep both
1. **Emergent (already exists, nothing to build):** a 1/8 legato-tied into a 1/16 sounds for a
   dotted-eighth duration. So dotted rhythms already emerge contextually from legato + note-value
   interaction. This is the elegant path and stays as a bonus on top of the explicit version.
2. **Explicit (the proposal):** dotted as selectable note values so you can get them
   DETERMINISTICALLY, not just by luck of which durations legato ties. Mirrors the TRIPLET decision
   (we added triplets explicitly + gated rather than leaving them emergent — same reasoning applies
   to dotted).

## How it fits the existing mechanism (PatternEngine::varyNoteIndex)
The note-value variation already works exactly the way dotted needs:
- Note values are indices (currently 0..7). At variation=50% only the BASE note plays; away from 50%
  weight spreads from the base toward shorter/longer values across the reachable range.
- **`allowed(idx)` gates which TRIPLET indices variation can reach** via `noteVariationMask` bits
  (0b001=1/4T idx3, 0b010=1/8T idx5, 0b100=1/32&1/32T idx7). Straight values always allowed; triplets
  opt-in per bit.
- **The BASE note is ALWAYS active regardless of the mask** (`if (i == baseIdx) weights[i]=1.0` is set
  BEFORE/independent of allowed()). So you can set any value as base — including a gated triplet — and
  it plays even if its variation bit is off. The mask only limits what variation ADDS, never what you
  can set as base.

## So dotted = same treatment
- Add the 4 dotted values to the note-value table (8 -> 12 indices).
- Add dotted mask bit(s) to `noteVariationMask`, gated in `allowed()` exactly like the triplet bits.
- Consequence (free, from "base always active"): a DOTTED BASE note always plays even with dotted
  variation off — i.e. "deterministic dotted base + opt-in dotted variation", the exact use case.
- Check: dotted + legato composes sensibly (dotted spans 1.5x a straight value, crosses the grid
  differently; a dotted note legato-tied could make double-dotted/odd durations — verify it adds up
  cleanly, same as verifying any new value against the grid/legato).

## Why low-risk
No new mechanism — add table entries + mask bits to the EXISTING varyNoteIndex gating. Mirrors triplets
exactly. Panel: the NOTE VALUE knob gets 4 more detents; a variation context-menu/mask gets dotted
toggle(s) like the triplet ones.
