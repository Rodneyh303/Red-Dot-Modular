# SPEC — SandsDistributionExpander (the distribution sub-expander, dual-role) — DESIGN, post-release

Design authority: REACHABLE_STRUCTURES_THEORY.md (the dependence layers), this doc. A NEW snapped
sub-expander (Straits-style resnap) carrying ALL distribution-shaping controls. Module slug:
SandsDistributionExpander (panel display name TBD). DESIGN SPEC for future build, not an immediate task.
Branch off lane-expander-refactor when scheduled.

## THE ORGANIZING PRINCIPLE — the subpanel IS the distribution layer (makes the panel express the theory)
Without the Distribution expander: you get the PURE moving-average GAUSSIAN COPULA — correlation structure,
per-voice values, λ=0, NO cross-lane/cross-voice tail dependence. A complete, usable instrument (the body).
WITH the Distribution expander: warp, tilt, atoms (ℓ,m), the shape params + mod jacks → unlocks the FULL
reachable-structures space (tilt-shaping, the dynamic-MO tail dependence, cross-lane/cross-voice extreme
coordination — all the copula variations in REACHABLE_STRUCTURES_THEORY.md). So adding the expander is
PHYSICALLY adding the dependence-structure machinery above the Gaussian body. ALL distribution-related
controls move here (warp included — warp is already "leaving pure Gaussian", so it belongs with the layer
the expander unlocks).

## PER-LANE CONTROLS (knobs + a MOD JACK each) — FIVE params, general across distributions
Every distribution has these (some distributions ignore one, see "unused param" below):
- PARAM1
- PARAM2
- TILT (always — every distribution has a tilt param; the MO rail tilt in warpStages)
- ATOM LOCATION (ℓ)
- ATOM SIZE (m)
Plus WARP (w-mix) per lane. Each with its own MOD JACK (CV in). Mod jacks are the POINT — the dynamic-MO
routes (shared atom-mass mod, dwelling warp mod) REQUIRE patchable modulation → that is why distribution
params live on a panel WITH JACKS (a popup can't be patched).
UNUSED PARAM OK: modulation sent to a param2 a given distribution doesn't use is HARMLESS — a no-op. Do NOT
special-case it; just let it do nothing.

## DUAL ROLE BY HOST (one panel design, role by attachment) — the key unification
ONE SandsDistributionExpander design that assumes CROSS-VOICE or PER-VOICE role depending on its host:
- Attached to MACRO → CROSS-VOICE role (Macro = the cross-voice machinery; one distribution structure
  across voices, the global/shared layer). The shared-ness here IS what creates cross-voice tail dependence
  (shared modulation = common factor) — keep that in mind, it's theory-load-bearing.
- Attached to SANDS/EAST → PER-VOICE role (East = the per-voice machinery; independent distribution per
  voice).
Controls are IDENTICAL in both roles; only the SCOPE of what they address differs (cross-voice vs
per-voice) — same pattern as resolver/strong-types (one mechanism, addressing by context). Host detection
rides the existing module-lookup topology authority (it already resolves "who am I attached to"). Design,
build, test ONCE; two roles by where it snaps.

## MACRO: distribution param mods BECOME per-voice sends (DECIDED)
On Macro, the distribution param modulation becomes part of the PER-VOICE SENDS (the Macro→East send bus).
Then EAST lanes BLEND global (Macro-sent) + per-voice (East-local) modulation. So a distribution-param mod
flows through the same per-voice send machinery as everything else Macro→East; East combines the global
send with its own per-voice mod. (This keeps one modulation path Macro→East, no second seam. The shared/
global component of the send is what carries the cross-voice tail-dependence coordination.)

## MACRO PER-VOICE SENDS → their own subpanel (Macro is big)
The Macro per-voice sends get their OWN snapped subpanel (Macro is already big; per-voice sends are
per-voice depth = subpanel material). So Macro may carry TWO snapped subpanels: the per-voice-sends
subpanel AND the Distribution expander (in cross-voice role). Both resnap like Straits.

## THE GRAPH VISUALISER = the enlarged popup editor, given a PERMANENT HOME on the expander
The DistributionCurveWidget (PDF/CDF + the atom Dirac bar, DistributionCurveWidget.hpp) — the ENLARGED
popup editor — gets a PERMANENT HOME on the Distribution expander (not a separate small monitor; the full
editor, always visible). So you WATCH the distribution get modulated live — the Dirac bar grows/shrinks
with atom-size mod, the curve breathes with the params. "Cool to see it get modulated" + immediate visual
feedback on what the modulation does. The popup still exists for a focused/enlarged view, but the expander
hosts a permanent instance.

## LANE/VOICE FOCUS BUTTONS (still needed)
Even with the permanent editor, need LANE and VOICE focus buttons to choose WHICH lane/voice the editor
shows/edits. VOICE-SELECT BUTTONS LIVE ON THE SANDS MAIN PANEL (existing) — the single source of truth is
VoiceResolver (dsp/VoiceResolver.hpp, "single source of truth for voice identity/addressing"). ALL
subpanels (Distribution expander included) must HONOUR that shared voice selection — read the selected
voice from the shared state, don't have their own independent voice-select. LANE focus is per-expander
(which lane's distribution the editor shows).

## EXISTING BEHAVIOUR CARRIES OVER
- Macro-sets-default / per-lane opt-in override: if Macro sets a distribution it becomes the DEFAULT, with
  per-lane OPT-IN to a different one (the existing default/override model in MonsoonSandsManager). Preserve
  this on the expander.
- All other existing distribution behaviour (the warpStages math, the resolver path, the popup editor)
  carries over unchanged — this is a UI/panel RELOCATION + mod-jack addition, not a math change.

## OPEN (not decided)
- (none blocking — the Macro-mod-becomes-per-voice-sends question is now DECIDED: yes.)

## DONE (when built) =
- SandsDistributionExpander: per-lane param1/param2/tilt/atom-ℓ/atom-m/warp knobs + mod jack each; dual
  role (cross-voice on Macro / per-voice on Sands/East) via host detection; permanent DistributionCurveWidget
  editor + lane/voice focus; honours Sands main voice-select (VoiceResolver); Macro default / per-lane
  opt-in preserved.
- Macro distribution mods flow through per-voice sends; East blends global+per-voice.
- Macro per-voice-sends own subpanel.
- Resnap hardening applies (snapped like Straits). Unused-param mod = harmless no-op.
- Base-alone = pure Gaussian copula (verify: no expander → no tilt/atom/warp → λ=0, body only).

## NOT THIS SPEC
- The dynamic-MO math (built — warpStages). The decision manager. The reachable-structures theory (done).
