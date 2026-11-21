# CC PROMPT — merge the subgate base ONTO Straits base; strip mod jacks from the per-voice panels

The feat/subgate-panel-fix branch got the 8 per-voice panels structured right (one per {RATCHET,GHOST}×
{REST,LEGATO,ACCENT,ADVANCE}, 165-wide, option a). TWO things remain WRONG — fix both:

## WRONG 1 — a SEPARATE StraitsSubgateBase module (should be MERGED onto StraitsBase)
CC built StraitsSubgateBase as its OWN main module (its own global grid, its own context menu, its own
process/polyphony/output logic). This DUPLICATES the Straits base (grid, snapping, voice-select, Causeway,
persistence, panel-gen) = code duplication + complexity + two "main" panels for one instrument section. The
subgate globals + their logic are STRAITS decisions and belong ON THE EXISTING StraitsBase — NOT a second
module.

### The merge
- MOVE onto StraitsBase: the 8 subgate GLOBAL knobs ({RATCHET,GHOST}×{REST,LEGATO,ACCENT,ADVANCE} globals),
  the subgate CONTEXT MENU (StraitsSubgateBase.cpp:283-326 — the source SWITCHES "follow main / global
  knob" per decision, default follow-main; "Clear per-voice overrides"), and the subgate LOGIC
  (process(), the three-way resolution, the SubgateKnobs state).
- POLYPHONY / OUTPUTS: the subgate-base process logic that handles polyphony setting + outputs → fold into
  StraitsBase's existing polyphony/output handling (StraitsBase already owns poly + outputs for the main
  lanes; the subgate items join that, one owner).
- PANEL: keep the SUBGATE panel ARTWORK (the RATCHET/GHOST × REST/LEG/ADV/ACC global grid) and ADD what is
  currently on the StraitsBase panel to it — i.e. the merged StraitsBase panel carries BOTH the existing
  Straits-base controls AND the subgate global grid. (Reconcile the two base panels into one; the subgate
  grid becomes a section of the Straits base panel.)
- DELETE the StraitsSubgateBase MODULE (modelStraitsSubgateBase) once its params/menu/logic/panel are on
  StraitsBase. The 8 per-voice panel MODULES stay (they attach to the unified StraitsBase now).
- VOICE-1-TO-MONSOON: the subgate per-voice panels do NOT lock V1 to Monsoon — CORRECT, they have no
  direct Monsoon parallel (no Monsoon subgate knob). Leave that as-is. But the subgate items' POLYPHONY +
  OUTPUT handling must go through StraitsBase (the one base that owns poly/outputs), per the merge above.

## WRONG 2 — the per-voice panels have MOD INPUT JACKS (not asked for; remove)
Each per-voice subgate panel binds a modulation input: StraitsSubgatePanel.cpp:89
`bindInput<PJ301MPort>("input_sgmod", StraitsSubgateIds::MOD_PANEL_0)` — the jack at the bottom of each
RATCHET/GHOST panel in the rack. We did NOT ask for per-panel modulation. MODULATION COMES VIA CAUSEWAY,
not per-panel jacks.
- REMOVE the input_sgmod jack from the per-voice panels (the bindInput, the MOD_PANEL_* ids/ports, and the
  jack from each of the 8 SVGs).
- The per-voice panels must then look VISUALLY IDENTICAL to the existing lane expander subpanels EXCEPT for
  tint (tint is fine for now) — same 165-wide 6/6/4, same knob grid, NO extra jack row. They currently
  look different PRECISELY because of the unwanted mod jacks; removing them makes them match.

## DONE =
- StraitsSubgateBase MODULE deleted; its 8 global knobs + context-menu source-switches + "clear overrides"
  + subgate process/resolution logic merged onto StraitsBase; subgate poly/output handled by StraitsBase's
  existing poly/output path.
- The Straits base panel carries the existing Straits-base controls AND the subgate global grid (one base
  panel).
- The 8 per-voice subgate panel MODULES retained, now attaching to the unified StraitsBase; input_sgmod mod
  jacks REMOVED (bindInput + ids + SVG jack); panels visually identical to the lane subpanels except tint.
- V1-not-locked-to-Monsoon on the subgate panels left as-is (no Monsoon parallel). run_all + anchor audit
  green.

## WHY
Modulation = Causeway, not per-panel jacks. Subgate globals = Straits decisions = on the Straits base, not
a duplicate module. One base owns poly/outputs/snapping/voice-select; the subgate tier joins it. The per-
voice panels are just more Straits expanders — same dimensions, tint only. Reuse/merge, don't duplicate.
