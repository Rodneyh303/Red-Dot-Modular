# CC PROMPT 04-FIX-2 — warp has NO engine effect: the DRAW path bypasses the mirror

CONTEXT: feat/warp-ui. Rodney verified ON THE SCOPE (not just bars) that selecting a non-uniform
distribution with w>0 has NO effect on the audio output, for BOTH Macro and East. So this is an
ENGINE bug, not display. Traced to one line. Work on feat/warp-ui.

## THE BUG (pinned to a line — diagnosed, not guessed)
genPitchLive produces the melody pitch from a (r_semi, r_oct) uniform. There are TWO sites that
compute melodyPitchV, and only ONE was converted to read the warp mirror:
  - PatternEngine.cpp:886-889 (the `melodyChanged` DISPLAY-refresh path): reads
    warpRow(displayedVoice_, PL_MELODY/OCTAVE) → WARPED. ✓ (this is what CC_PROMPT_03 wired)
  - PatternEngine.cpp:787 (redrawMelody — THE DRAW PATH, runs on fresh draw / phrase boundary):
    `melodyPitchV[i] = genPitchLive(sem, in, slewedMelody[0][i], slewedOctave[0][i]);`
    → reads RAW slewedMelody[0] / slewedOctave[0], BYPASSING the mirror. ✗✗✗
redrawMelody is the PRIMARY path that generates the pattern (and its pitch the scope reads). It
feeds genPitchLive the RAW slewed uniform, so the drawn/sounding pattern is UNWARPED. Only the
886 display-refresh (when melodyChanged fires) applies warp — hence the inconsistent/absent
effect. Note line 787 is hardcoded `[0]` (mono/voice-0) — the "mono is special" residue: the
draw path was never converted to the per-voice mirror.


## *** UPDATE: this is a MONO/POLY split — there are TWO sites, the POLY one matters MORE ***
Audit (SequencerEngine.cpp) found the SAME residue in the actual POLY AUDIO path, not just the
display. voicePitch gets its melody/octave uniform from DIFFERENT sources for mono vs poly:
  - MONO (SequencerEngine.cpp:553-554, executeStep):
      voicePitch(0, ..., pe.warpRow(0, PL_MELODY)[...], pe.warpRow(0, PL_OCTAVE)[...])  → WARPED ✓
  - POLY (SequencerEngine.cpp:1147-1148, executePolyVoice):
      voicePitch(voiceIdx, ..., polyRandomSrc(voiceIdx, PL_MELODY)[melIdx], ...)        → RAW ✗✗✗
warpRow(int v, int lane) EXISTS for ALL voices (PatternEngine.hpp:181) — the mono path uses
warpRow(0,·), the poly path was left on raw polyRandomSrc. THIS is why poly voices get no warp.
This is the CLASSIC "mono is special" residue: warp wired for voice 0, not propagated to poly.
FIX THE POLY SITE: SequencerEngine.cpp:1147-1148 — replace polyRandomSrc(voiceIdx, PL_MELODY/OCTAVE)
with warpRow(voiceIdx, PL_MELODY/OCTAVE) for the pitch uniform (keep the refreshPolyRandomCell call
that updates random_ first — the mirror reads random_, so the cell must be refreshed THEN the
mirror rebuilt THEN warpRow read; confirm refreshPolyRandomCell triggers the per-cell mirror
rebuild per CC_PROMPT_03, else the warpRow read is stale). Octave likewise.
Both the :787 (display/mono draw) AND :1148 (poly audio) sites must be converted. The POLY one is
the one the scope hears for V2+.

## THE FIX (the DISPLAY/mono site :787 — do this too)
At PatternEngine.cpp:787 (redrawMelody's loop), feed genPitchLive the MIRROR value instead of the
raw slewedMelody[0]/slewedOctave[0] — exactly as the 886 site does:
    melodyPitchV[i] = genPitchLive(sem, in,
                        warpRow(displayedVoice_, PL_MELODY)[i],
                        warpRow(displayedVoice_, PL_OCTAVE)[i]);
(Use the SAME accessor + voice key the 886 site uses — warpRow(displayedVoice_,·), which already
handles the voice-0/mono degeneration in the accessor per the 886 comment. If redrawMelody has a
reason to key a different voice than displayedVoice_, match whatever the SOUNDING path for that
voice should warp — but the two melodyPitchV writers MUST agree, or draw and display disagree.)

VERIFY THE ORDERING (the 03 prompt's blessed rule): warpRow reads the mirror; the mirror must be
rebuilt from random_ AFTER recomputeEffectiveMelody's slew write-back. redrawMelody calls
recomputeEffectiveMelody() at :781 BEFORE the loop — confirm recomputeEffectiveMelody's end
rebuilds the mirror rows it touched (the 03 hook), so warpRow at :787 is fresh. If the rebuild
hook is missing from the recompute, add it (rebuild the voice's MEL/OCT mirror rows at the end of
recomputeEffectiveMelody, per CC_PROMPT_03's "hook rebuilds at the random_ writers").

## ALSO CHECK (same bug class — the OTHER draw paths)
The qmix twin of redrawMelody (the comment at :791 "q-mix twin of redrawMelody") and any
per-voice poly draw path may have the SAME raw-slewed bypass. Grep for genPitchLive and
slewedMelody/slewedOctave/slewed* reads that feed pitch/degree output and are NOT routed through
warpRow/the mirror. Melody + octave are the warped lanes (per the per-lane gate); convert every
genPitchLive-feeding DRAW site, not just :787. Do NOT warp rest/accent/qmix/variation/legato
(they read raw by the per-lane gate — unchanged).

## CONTRACT TEST (make the bug impossible to regress)
Extend test_warp_mirror.cpp: assert that after a DRAW (redrawMelody path), with w>0 and a
non-identity WarpF on (displayedVoice, MELODY), the resulting melodyPitchV / the uniform fed to
genPitchLive is the MIRROR value, NOT the raw slewedMelody. i.e. a draw with warp set produces a
DIFFERENT pattern than the same draw with w=0 — the thing the scope should have shown. This test
fails on current HEAD (787 raw) and passes after the fix.

## DONE =
- redrawMelody (:787) and all genPitchLive-feeding DRAW sites read the mirror (warpRow), not raw
  slewed, for MEL/OCT; draw and display melodyPitchV writers agree.
- mirror freshness confirmed (rebuild hook in recomputeEffectiveMelody).
- test_warp_mirror: draw-path-warp contract test, RED on HEAD → GREEN after.
- bash test/run_all.sh green; plugin builds.
- PR body: name the :787 raw-bypass, list every draw site converted, paste the new test.
  "needs Rack feel-test: scope should now show warp on the drawn pattern for the selected voice,
  Macro + East."

## NOT THIS PROMPT
- The popup z-order fix (separate prompt CC_PROMPT_04_FIX_popup).
- Boolean/qmix-lane warp (deferred — only MEL/OCT warp).
