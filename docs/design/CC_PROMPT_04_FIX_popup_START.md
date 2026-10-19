# CC PROMPT 04-FIX — warp UI popup fixes (z-order is the root cause of 3 symptoms)

CONTEXT: feat/warp-ui (PR-in-progress, 5 commits on lane-expander-refactor) built the warp UI —
w-level column, CRV popup-trigger button, WarpCurvePopup editor. Two feel-test fixes already
landed (fdc6a03 display-bars warp, 2cd9ea6 popup sizing/chrome). Rodney feel-tested in Rack and
found remaining bugs. Work ON feat/warp-ui (continue the branch), re-run the Rack feel-test after.

## READ FIRST
- CLAUDE.md, docs/design/CHAT_HANDOFF.md.
- The files you're fixing: src/ui/WarpCurvePopup.hpp, src/ui/WarpCurveButton.hpp,
  src/StraitsSandsMacroVisual.cpp (openWarpPopup ~L412), src/StraitsEastSandsVisual.cpp
  (openWarpPopup ~L560), src/ui/DistributionCurveWidget.hpp (the editor, onDragMove ~L354).
- You CANNOT feel-test Rack; the audit + build are your checks. State "needs Rack feel-test".

## THE ROOT CAUSE (diagnosed against the code — fix THIS first, it explains 3 symptoms)
openWarpPopup (StraitsSandsMacroVisual.cpp:420,430 and the East twin) does:
    addChild(scrim); ... addChild(pop);
i.e. the popup + scrim are added as CHILDREN OF THE MODULE WIDGET (`this`), NOT to the scene
overlay. A module-child widget draws and receives events WITHIN the module's own layer. Therefore:
  SYMPTOM 1 (controls show THROUGH the popup): other widgets drawn after it — the module's own
    later-drawn children, adjacent modules, cables — render on top. A module child is NOT a top layer.
  SYMPTOM 2 (popup param knobs can't be grabbed): events are dispatched in the scene's layer order;
    widgets in higher layers (or the module's later siblings) intercept the press before it reaches
    the popup's knobs, so onDragStart never arrives at them.
  SYMPTOM 3 (curve: can add points but not MOVE them): DistributionCurveWidget.hpp onButton (L315)
    sets dragPoint=hit on press; onDragMove (L354) moves the point IFF dragPoint>=0. "Add works,
    move doesn't" = the PRESS reaches the widget (so addPoint/hitPoint run) but the subsequent
    DragStart/DragMove sequence is intercepted by the wrong layer before it gets back to the curve.
    Same root cause as 2 — the popup isn't in a layer that owns the drag gesture.

### THE FIX: put the popup in the SCENE OVERLAY, not the module
Add the scrim+popup to the top layer so they draw above everything and own their events. The Rack-2
idiom: attach to `APP->scene->rack` is NOT enough (that's the module layer); use the overlay layer —
either APP->scene (as a direct child, positioned in scene coords) or a MenuOverlay-style host.
Concretely:
  - Position the popup in SCENE coordinates (convert from the module-local pos you use now via the
    widget's getAbsoluteOffset / box.pos chain), since it's no longer a child of the module.
  - Add scrim then popup to the scene overlay (scrim first → popup draws on top of scrim; scrim
    catches outside-clicks to close).
  - closeWarpPopup must removeChild from the SCENE (not the module) and delete.
  - The popup must survive the module's own redraws and NOT be clipped by the module's box.
Model this on how Rack's own context menus / a known overlay widget attach (createMenu / MenuOverlay
pattern) — that's the canonical "floats above everything, owns its events" host. If a full
MenuOverlay is heavy, the minimum is: child of APP->scene (not the ModuleWidget), scene-space pos,
and ensure it's added LAST so it's top of the scene's draw/event order.

VERIFY (as far as you can without Rack): the popup widget tree is parented to the scene, not the
ModuleWidget; closeWarpPopup detaches from the same parent; positions are scene-space. The build
links. Then: "needs Rack feel-test — confirm popup draws above all, knobs grab, curve points drag."

## SECONDARY — only if the curve drag is STILL broken after the z-order fix
If moving the popup to the overlay does NOT restore curve-point dragging, then the gesture is being
lost inside the popup's own event forwarding. Check WarpCurvePopup.hpp:49-55 — the popup forwards
onDragMove/onButton to DistributionCurveWidget. If the popup consumes the press (L340
"if PRESS consume") BEFORE the curve widget's onButton sets dragPoint, the curve never arms the
drag. Fix: let events on the curve sub-widget reach it FIRST (don't blanket-consume presses that
land inside the curve's box); only consume presses on the popup chrome (header/background), not on
the interactive children. The curve widget already consumes its own events correctly (it's proven —
it worked standalone before the popup hosted it), so the popup must not pre-empt them.

## DO NOT TOUCH (already fixed / out of scope)
- The displayed-bars warp (Macro-no-effect / voice-asymmetry): fdc6a03 added warpDisplayMix so the
  bars compose warp per-(voice,lane) like the mirror. Rodney: RE-TEST this in Rack before assuming
  it's still broken — it may already be fixed. If still wrong after re-test, that's a SEPARATE
  follow-up (warpDisplayMix voice/delegation keying), not this prompt.
- The warp engine (mirror, bank, consumer routing) — all merged + green, don't touch.
- Panel geometry/columns — keep audit_anchor_bind green; don't move columns.

## DONE =
- Popup + scrim attach to the SCENE overlay (not the ModuleWidget); scene-space positioning;
  closeWarpPopup detaches from the scene. Draws above all; owns its events.
- (if needed) popup doesn't pre-empt the curve widget's press/drag — curve points drag again.
- bash test/run_all.sh green; audit_anchor_bind green; plugin builds.
- PR body: state the z-order fix, and "needs Rack feel-test: popup on top, knobs grab, points drag;
  re-test the display-bars warp (fdc6a03) for the macro/voice behaviour."
