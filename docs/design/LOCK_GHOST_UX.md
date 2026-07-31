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

## CORRECTION (Rodney): both images already exist -- no plumbing, just swap which is solid
Over-scoped earlier. The editor ALREADY computes BOTH positions on each ProbabilityLane:
- editStartBar()/editEndBar() -- raw offset/length = the LIVE edit (your handle movement).
- startBar()/endBar() -- dispOffset/dispLength = the DISPLAY/committed window (engine, frozen under
  lock; synced from edit via syncDisplayToEdit() when unlocked). = the OLD playing position.
Under lock these DIVERGE (edit moves, display frozen) -- that divergence IS the real+ghost pair
already on screen. No committedLorFn needed.

What draws what NOW:
- Probability bars brighten by barInWindow(step) which uses startBar() (DISPLAY/committed) -> the
  bright "real" image currently follows the OLD position.
- drawHandles ribbons use editStartBar() (LIVE) at alpha ~0.55 -> the user's movement is the faint
  one.

The SWAP (only under lock AND diverged, i.e. edit != display):
- Make the LIVE (edit) window the SOLID/dominant one: handle ribbons full alpha; and the bar-window
  brightening should follow editStartBar() so the cells the user is setting look active.
- Make the COMMITTED (display) window the GHOST: draw the startBar() window as a low-alpha overlay
  (the echo of where playback still is).
- Playhead already travels on the committed/display position (engine) -> ghost aligns with it.
When unlocked or not diverged: edit==display, unchanged single solid draw.

Precise edit next session: in the bar-draw (~551 barInWindow) and drawHandles (~686), branch on
laneLocked(lane) && (editStartBar!=startBar || editLen!=dispLength): swap which window gets solid
vs ghost alpha. NOT a new feature -- a conditional alpha/target swap between two already-computed
windows. Care: keep the normal CV-modulation display (unlocked) untouched.

## Status
LATCH logic done + verified. This is a display refinement. Plan recorded; build next session on the
render path (needs the committedLorFn plumbing + drawHandles swap). Not urgent -- logic is correct;
this improves legibility of the prepare-silently state.
