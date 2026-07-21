# NUM_PARAMS reduction — migrate internal-state params off configParam

Fixes the DAW automation failure (DAW_PARAM_AUTOMATION_ISSUE.md): five modules sized to
`MonsoonIds::NUM_PARAMS` (~1128) overflow the host parameter pool and export NOTHING. There
is NO host-hide flag in the Rack SDK (confirmed: every configParam is exported). So the ~700
internal-state params must leave `params[]` entirely.

## What moves (category 1 only) — the shared MonsoonIds storage ranges
These live in `MonsoonIds` (so any expander addresses them by id over the bus) but are pure
per-voice-per-lane STATE with no widget on the Monsoon panel:

    MACRO_OWN         64    owner id per (voice,lane)               MACRO_OWN_START + v*4 + lane
    MACRO_SEND       256    send depth per (voice,lane,item)        MACRO_SEND_START + (v*4+lane)*4 + item
    MACRO_OWN_DISP     4    display scratch (selected-voice mirror) MACRO_OWN_DISP_START + lane
    MACRO_SEND_DISP   16    display scratch                         MACRO_SEND_DISP_START + lane*4+item
    MACRO_ATTEN      256    atten depth per (voice, lane*4+col)     MACRO_ATTEN_START + v*16 + lane*4+c
    MACRO_TAP          8    LOR/spread tap per lane                 MACRO_TAP_START + lane*2 + {0,1}
    VARLEG_DELEG      30    deleg per (v=0..14, lane VAR/LEG)       VARLEG_DELEG_START + v*2 + lane
    VARLEG_DELEG_DISP  2    display scratch                         VARLEG_DELEG_DISP_START + lane
    VARLEG_ATTEN      96    varleg atten (v,lane,col)               VARLEG_ATTEN_START + v*6 + lane*3+col
    LANE_DIR          96    per-lane direction 0..3 (v=0..15)       LANE_DIR_START + v*6 + lane
    ----------------------------------------------------------------------------------------
    TOTAL            828    -> NUM_PARAMS ~1128 drops to ~300

NOTE the DISP (display-scratch) sub-ranges: MACRO_OWN_DISP, MACRO_SEND_DISP,
VARLEG_DELEG_DISP. These are "selected-voice mirror" values the editor writes for the
on-screen widget of the currently-selected voice. They are ALSO addressed via MonsoonIds and
also inflate the count. They move too, BUT some have a live widget on the EXPANDER panel
(they're what the visible knob binds to). Those specific ones must stay real params on the
EXPANDER's own namespace (category 2) OR keep a widget — see "display-scratch caveat" below.

## What does NOT move (category 2) — expander-local params, leave alone
Configured on the expander with the EXPANDER's own ids (StraitsEastVisualIds::,
StraitsMacroVisualIds::, or local helper ids that resolve into the expander's namespace):
ownerDispId, attenDispId, dirDispId, tapLorId, tapSprId, StraitsMacroVisualIds::attenId,
sendDispId, restInterpId/melodyInterpId/…, lorId, varlegAttDispId. These are in the
expander's small namespace, already export fine, and are the VISIBLE selected-voice knobs.
Leave them as configParam.

(Verify each helper's namespace before moving: `grep` the helper definition — if it returns
`MonsoonIds::X_START + …` it's category 1 (move); if `StraitsEastVisualIds::…` or a local
base it's category 2 (keep).)

## The design: a bus-addressable field store on Monsoon

Add to `struct Monsoon` (Monsoon.hpp), replacing the migrated param ranges with plain arrays:

    // Per-voice-per-lane editor state, formerly MonsoonIds param ranges. Bus-addressable:
    // expanders reach these through the Monsoon* they already hold, via the accessors below.
    // Persisted in dataToJson/dataFromJson (params[] auto-saved; plain fields do not).
    struct EditorState {
        float macroOwn   [64]  = {0};
        float macroSend  [256] = {0};
        float macroAtten [256] = {0};
        float macroTap   [8]   = {0};
        float varlegDeleg[30]  = {0};
        float varlegAtten[96]  = {0};
        float laneDir    [96]  = {0};
        // display-scratch: keep only if not widgeted on the expander (see caveat)
        float macroOwnDisp [4]  = {0};
        float macroSendDisp[16] = {0};
        float varlegDelegDisp[2]= {0};
    } editor;

Accessors on Monsoon (the ONE place the index math lives, mirroring the old helper ids):

    float  getLaneDir(int v,int lane) const { return editor.laneDir[v*6+lane]; }
    void   setLaneDir(int v,int lane,float x){ editor.laneDir[v*6+lane]=x; }
    // …one pair per range, same index expression as the old *_START + … helper.

## Call-site transform (~74 sites, mechanical)
Every consumer currently does one of:

    mod->params[ MonsoonIds::LANE_DIR_START + v*6 + lane ].getValue()
    mod->params[ dirId(v,lane) ].setValue(x)      // dirId() returns MonsoonIds::LANE_DIR_START+…

becomes:

    mod->getLaneDir(v,lane)
    mod->setLaneDir(v,lane,x)

The existing helper functions (dirId, ownerId, attId, sendId, varlegAttId, …) are the seam:
REPOINT each helper to call the accessor instead of returning an id. Two ways:
- (cleanest) delete the id helpers, sed the call sites to the accessors.
- (smallest diff) keep the helpers but change their bodies from `return MonsoonIds::X+…;` to
  forwarding — not possible for getValue/setValue asymmetry, so prefer the accessor sed.
Engine (src/dsp/) touches NONE of these, confirmed — only expander widgets do.

## Persistence (must add — this is what params[] gave for free)
In MonsoonPersistenceManager dataToJson: write each array (json array of floats). In
dataFromJson: read back, clamp, tolerate missing (old patches → defaults). ~10 arrays, one
loop each direction. WITHOUT this, per-voice-per-lane edits do not survive save/load — the
one regression risk of the whole migration, so test it explicitly.

## Config removal
Delete the configSwitch/configParam calls for the category-1 ids (the MonsoonIds:: ones):
StraitsEastSandsVisual.hpp:330 (LANE_DIR), varlegAttId (271), sendId, and any others whose
helper resolves to MonsoonIds. Then shrink the enum: delete the MACRO/VARLEG/LANE_DIR range
markers from MonsoonIds ParamIds so NUM_PARAMS recomputes to ~300. PHASE_PARAM currently
`= LANE_DIR_END`; repoint it to the new (smaller) end.

## Display-scratch caveat (the one subtlety)
MACRO_OWN_DISP / MACRO_SEND_DISP / VARLEG_DELEG_DISP are the selected-voice mirror the VISIBLE
expander knob binds to. If a knob binds to them, they must remain a real param on the
EXPANDER's OWN namespace (move the config from MonsoonIds to StraitsEastVisualIds/
StraitsMacroVisualIds), not a Monsoon field — because a widget needs a param. Check: does any
bindParam target a *_DISP id? If yes, re-home to the expander namespace (small, exports fine).
If no (pure scratch), move to the field store like the rest.

## Order of work (safest first, each independently buildable)
1. LANE_DIR (96, self-contained, no DISP, one config site @330, ~15 call sites) — do FIRST as
   the pilot: proves the field+accessor+JSON pattern end to end on the smallest clean range.
2. VARLEG_ATTEN (96) + VARLEG_DELEG (30) — no widgets, straightforward.
3. MACRO_OWN/SEND/ATTEN/TAP (584) — the bulk; same pattern, more call sites.
4. The DISP ranges — last, applying the caveat (re-home widgeted ones to expander namespace).
5. Shrink the enum + repoint PHASE_PARAM. Rebuild → NUM_PARAMS ~300 → all five modules export.
Each step: build, confirm nothing visually/behaviourally changed, confirm save/load round-trips
that range. Only after all five: confirm VARIATION + a Straits/East knob appear in Bitwig.
