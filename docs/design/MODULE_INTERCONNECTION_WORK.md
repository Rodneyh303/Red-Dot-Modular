# Module interconnection — expander search + connection fix (work record)

Substantial infrastructure built during the Straits refactor / V1 cascade. Logged so it's not lost.

## What was built
- **Lane-expander system:** StraitsBase + StraitsLaneExpander + StraitsR; REST/ACCENT/QMIX (and VAR/LEG
  to come) as pure DATA DESCRIPTORS dropping into the chain. Canonical lane order, hidden-registration,
  mono-default fallback, connect-marks, checkbox menu.
- **Expander discovery / scan:** isSuiteChainModel <-> scan if-else chain kept in LOCKSTEP (a lane
  expander between Monsoon and East must not hit `else break`). This was a bug source: missing models
  (modelStraitsLaneQMIX/REST/ACCENT/VARIATION/LEGATO, modelMonsoonShophouseMicro) broke the chain ->
  cachedEastSandsVisual null -> eastLOR null -> mmOwn null -> spread read 0. Fixed by adding the models.
- **Owner back-pointer:** replaced fragile findMonsoonEitherSide chain-walks with a direct `owner`
  member set in Monsoon::process() before sync(). The manager IS a member of Monsoon, so it always knows
  its owner — the chain walk could return null when any module between East and Monsoon acted as a
  boundary. This fixed the mmOwn-null cluster (spread / QMIX-arc / crash all stemmed from it).
- **Force-follow / drag:** setModulePosForce + onDragEnd so docked expanders track the host.
- **Transport sync:** clock/phase-knob -> light-ring / Sands playhead sync (reset step-16 flash, laneTick
  0-based, phase-knob drift).

## Why it was entangled with the V1 cascade
The owner/expander RESOLUTION bugs (eastLOR null, scan-missing-models, mmOwn null, the de-paramming
crash, the QMIX mod-arc checking the OLD Straits) were all the SAME class: pointers/resolution not
updated for StraitsR / the current modules. They surfaced AS V1 spread/display bugs but were really the
interconnection layer. The owner back-pointer + scan-lockstep fixes + the V1->voice-0 data-model collapse
together cleared the cluster.

## Remaining / related
- QMIX mod-arc glitch: patched-detection checked OLD Straits (cachedPolyVoiceExpander / QUANT_CV_INPUT)
  not the actually-attached StraitsR — grep siblings. (Check if still open post-merge.)
- Causeway: same lane-expander model, CV-mod-input lanes, inherits the wave/seam helpers + owner model.
