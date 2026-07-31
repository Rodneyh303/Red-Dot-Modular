# Lock-mode LOR display: swap solid/ghost (Rodney UX)

## Verified working
Direction + LOR + Owner LATCH is CONFIRMED in Rack (3-image test: before/during/after lock). Under
lock, LOR edits do NOT affect audio; they commit at unlock. The LATCH LOGIC is correct and done.

## The UX change (display only -- logic unchanged)
Currently under lock the SOLID/bright element is the OLD (playing) position and the user's movement
shows as a faint shadow. Rodney wants it INVERTED:
- LIVE handle (what the user is moving) -> SOLID/normal (it's under their hand, should feel real).
- OLD position (what the playhead still reads under lock) -> GHOST ECHO (faint trace of where
  playback is held).
Rationale: matches agency -- the control you're moving is solid; the ghost marks "playback is still
here, temporarily." More intuitive than the current inversion.

## When the ghost appears (Rodney)
ONLY once a control is MOVED under lock (i.e. live != committed). Before moving, live == committed,
no ghost. Unlocked, live == committed, no ghost. Ghost = the divergence indicator.

## Current architecture (SandsVisualEditorV4.hpp)
- drawHandles(vg, lane) draws currentState.lanes[lane] = the LIVE/edited position (handle ribbons,
  editStartBar/editEndBar). Alpha ~0.55.
- The probability bars / playhead show the DISPLAYED window = the ENGINE's committed position
  (frozen under lock) -- see comment :700-701.
- Editor HAS laneLockedFn (:180) so it knows lock state. It does NOT have the committed engine LOR
  position -- only currentState (live). THAT is what must be supplied.

## Implementation plan (moderate, render-path -- do carefully next session)
1. Give the editor the committed LOR per lane: a std::function<void(int lane, int lor[3])> getter
   (committedLorFn) the host registers, reading the ENGINE's frozen strand window (strandLen/Off/Rot
   or the store snapshot at lock-on). Editor calls it only when laneLocked(lane).
2. In drawHandles: if laneLocked(lane) AND committed != currentState (diverged):
   - draw the LIVE window (currentState) SOLID (raise alpha to ~1.0 / normal edge weight).
   - draw the COMMITTED window as a GHOST overlay (low alpha ~0.25, maybe dashed/outline) at the
     committed editStartBar/len.
   Else (unlocked or not diverged): current single-window draw, solid.
3. The playhead should keep travelling on the COMMITTED window (it reads engine position) -- verify
   the ghost aligns with where the playhead actually is, so the echo reads as "playback is here."

## Status
LATCH logic done + verified. This is a display refinement. Plan recorded; build next session on the
render path (needs the committedLorFn plumbing + drawHandles swap). Not urgent -- logic is correct;
this improves legibility of the prepare-silently state.
