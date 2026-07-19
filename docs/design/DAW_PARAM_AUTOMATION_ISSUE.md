# Monsoon params don't appear in the DAW automation list — diagnosis

## Symptom
In Bitwig, moving VCO-1's FREQ instantly shows it in the modulation/automation list. Moving
Monsoon's VARIATION (or any Big-5 knob) does NOT show up. VCV-brand params auto-appear;
Monsoon's don't.

## Not the cause
- **Bad configParam**: VARIATION is a normal `configParam(VARIATION_PARAM, 0,1,0.5,"Variation…")`.
  Fine.
- **The 1024 host cap alone**: VARIATION is at enum index ~1 (very start), so it's far under any
  cap. If a simple overflow were the cause, the LAST params would drop, not VARIATION. (The
  overflow IS a real latent bug — see below — but it's not why VARIATION specifically fails.)
- **The map-to-slot workflow**: the A/B test settles this — VCO-1 auto-appears WITHOUT
  mapping, so Rack Pro does auto-expose params. Monsoon differs, so it's not "you forgot to
  map".

## The cause: NUM_PARAMS is ~1128, dominated by internal-state params
Rack Pro exposes each module's params into the host's FINITE parameter pool (the Surge XT
devs describe "burning" host VST params per instantiated module). Monsoon's `NUM_PARAMS` is
roughly:

    base individual + poly banks       ~200-300
    MACRO_OWN 64 + SEND 256 + OWN_DISP 4 + SEND_DISP 16 + ATTEN 256 + TAP 8   = 604
    VARLEG deleg 30 + disp 2 + atten 96                                        = 128
    LANE_DIR 96                                                                = 96
    ------------------------------------------------------------------------------
    NUM_PARAMS  ~= 1128

The vast majority (the ~700 MACRO/VARLEG/LANE_DIR entries) are NOT user-facing knobs. They
are per-voice-per-lane EDITOR STATE (owner ids, per-lane direction switches, attenuator
depths) stored as params so they persist in the patch and are editable on the Sands/East/
Macro EXPANDER panels. They are `configParam`/`configSwitch`'d (e.g.
StraitsEastSandsVisual.hpp:330 configSwitch LANE_DIR), so each claims a host-automation slot.

A module claiming ~1128 host slots either overflows the pool or makes the host's per-module
param registration behave badly — and the practical result is that the genuinely-automatable
controls (VARIATION etc.) don't register cleanly. VCO-1 (a handful of params) always fits.

## Verify on the build (confirms this vs a config bug)
In Bitwig with Monsoon loaded, check the automation list:
- If NO Monsoon params appear, or only the first N (up to some cap) appear and the rest are
  missing → pool exhaustion / NUM_PARAMS bloat (this diagnosis).
- If params appear but with wrong names/values → a different (display) issue.
Also: temporarily shrink NUM_PARAMS (stub out the MACRO/VARLEG/LANE_DIR ranges in a scratch
build) and see if VARIATION reappears. That isolates it conclusively.

## Fix options (need the build to validate; touches ~700 params)

### Option A — mark internal-state params non-automatable (preferred if Rack Pro honors it)
Rack Pro / the plugin layer may skip host-exposure for params flagged appropriately. Check
the SDK for a ParamQuantity flag that removes a param from the host pool (the Surge XT thread
"way to make params hidden in rack pro" is the same ask — confirm what shipped). If such a
flag exists, set it in the configurator loops for every MACRO/VARLEG/LANE_DIR param:

    auto* pq = m->getParamQuantity(id);
    pq->/*host-hidden flag*/ = true;   // exact member per SDK

This keeps them as params (persistence + expander widgets unchanged) but drops Monsoon's
host footprint from ~1128 to the ~80 real controls. Lowest-risk if the flag exists.

### Option B — move internal state out of params[] entirely
These ranges don't NEED to be params — they're storage with expander-side widgets. Convert
them to plain module fields (arrays) + JSON persistence (dataToJson/dataFromJson), and have
the expander widgets read/write those fields instead of params[]. Bigger change (touches the
expander editors and persistence) but it's the architecturally correct home for
per-voice-per-lane state, and it fixes the host-pool bloat at the root. Also removes ~700
entries from randomize/reset/preset handling where they never belonged.

### Option C — accept it and document the map workflow
If neither is quick: the params still work via Rack's host-parameter MAP mode (click the knob
in map mode to bind a slot). Not auto-exposed, but reachable. Weakest option — VARIATION
being un-automatable out of the box is a real UX regression vs VCV-brand modules.

## Recommendation
Confirm with the shrink-NUM_PARAMS test, then Option A if the SDK has a host-hide flag, else
Option B. Either way the target is: only the ~80 genuine controls consume host slots; the
~700 internal-state entries do not. This ALSO fixes the latent >1024 overflow (params above
index 1024 currently can't be host-automated at all regardless).

---

## Update: "Sands visual params appear, Big-5 don't" — refines the diagnosis

New data: in Bitwig the Sands/East/Macro visual params (HIGH enum indices, ~300+) DO appear,
but the Big-5 core knobs (LOW indices, ~0-5) do NOT. This is the opposite of a simple
"pool fills up, later params drop" — so raw NUM_PARAMS overflow is not the whole story.

Ruled out in-container:
- **Missing widgets**: `bindParam` (SvgPanelKit.hpp:156) does createParamCentered + addParam
  exactly like stock Rack, BUT silently WARNs and creates NO widget if the panel marker isn't
  found. However, the Big-5 markers (param_VARIATION_PARAM etc.) ARE present in both live
  panels (dark + light), 1 each. So binding should succeed. Widget-presence theory: not
  confirmed from the files.

Two candidates remain, both needing the build to settle:

1. **Runtime bind failure despite the marker existing in the file** — the LOADED panel differs
   from the committed SVG (stale build, or a different panel variant loading at runtime). This
   is the most likely cause of "some appear, some don't" and is ONE LINE to confirm:
   → Open Rack's `log.txt` with Monsoon loaded. Look for:
        [SvgKit] param shape not found: param_VARIATION_PARAM
     (or any Big-5). If present, the runtime panel isn't the one on disk — rebuild clean /
     check which panel path loadPanel() actually resolves. If ABSENT, binding is fine and it's
     the host-pool/index issue below.

2. **Index/pool interaction** — high-index params exposing while low-index don't could be a
   fixed host window offset, or an enum arithmetic overlap (a mis-sized range making low IDs
   collide). Verify NUM_PARAMS and that every range's START==previous END with no gaps/overlaps;
   confirm PHASE_PARAM = LANE_DIR_END didn't shift anything. The shrink-NUM_PARAMS test still
   applies.

RECOMMENDED ORDER on the build:
  a. Check log.txt for the SvgKit WARN (settles candidate 1 immediately, one line).
  b. If clean, shrink NUM_PARAMS (stub MACRO/VARLEG/LANE_DIR) and see if Big-5 appear.
  c. Fix per Option A/B above.
Do NOT assume the NUM_PARAMS bloat is the cause until (a) is checked — the appear/not-appear
ordering points at a bind/panel-load mismatch first.

---

## RESOLVED (diagnosis): Monsoon-the-module is uniquely broken, for ALL its params

Decisive facts:
- Sands MONO params, tweaked on the SANDS panel, DO appear in Bitwig. Those are on the Sands
  MODULE (separate from Monsoon).
- Monsoon's own params (VARIATION etc.) do NOT appear — even though the knob is visible and
  works, so the widget bound fine (rules out any bind/panel-load failure).
- VCO-1 works. Sands/East/Macro work. Only Monsoon fails.

Conclusion: it is PER-MODULE, not per-param. Monsoon's ~1128-param block is too large for the
host to register, so the host drops the WHOLE module's params (that's why VARIATION at index 0
vanishes too, not just high indices). Every other module has a manageable block and exports
normally. The "Sands params appear" observation was a red herring — different module.

Why Monsoon is 1128: the MACRO/VARLEG/LANE_DIR ranges (~700 params) are per-voice-per-lane
editor STATE that lives in MONSOON's params[] (so it persists centrally and survives expander
detach), but is configured from and widgeted ONLY on the expander panels
(e.g. StraitsEastSandsVisual.hpp:330 configSwitch MonsoonIds::LANE_DIR_START...). They have
NO widget on the Monsoon panel and are never user-facing automation targets — yet each burns
a host slot and together they blow past the registrable limit.

### Decisive confirmation test (one scratch build)
Stub out the MACRO/VARLEG/LANE_DIR ranges so Monsoon's NUM_PARAMS drops to ~200, rebuild,
load in Bitwig. If VARIATION (and the Big-5) now appear → confirmed, done. (Expect yes.)

### Fix — Option A (preferred): host-hide the internal-state params
These ~700 params must STAY params (central persistence + expander widgets depend on it), but
should not be exported to the host. Set the host-hide flag on each in its config call. Check
the Rack Pro SDK for the ParamQuantity/plugin flag that removes a param from host export (the
Surge XT "hidden params in rack pro" request — confirm what shipped in your SDK version). Apply
in the loops that configSwitch/configParam the MACRO/VARLEG/LANE_DIR ids (in
StraitsEastSandsVisual / StraitsSandsMacroVisual where MonsoonIds::* ranges are configured, and
any in the Monsoon configurator). Result: Monsoon exports only its ~80 real controls; block
becomes registrable; VARIATION appears.

If the SDK has NO such flag → Option B (move the state out of params[] into Monsoon fields +
dataToJson, expander editors read/write the fields). Bigger, but removes the bloat at the root
and is the correct home for non-knob state.

### Also fixed for free
The latent >1024 overflow (LANE_DIR params sat above index 1024, unautomatable regardless) —
gone once the internal ranges stop consuming the low, registrable slots.

---

## CORRECTED ROOT CAUSE: it's every module sized to MonsoonIds::NUM_PARAMS, not Monsoon alone

The previous "Monsoon uniquely broken" conclusion was wrong. New data — which modules export:

  EXPORT (own small param namespace):   Lantern, Sands-mono (SandsMonoVisualIds), Shophouse
                                        (ShophouseIds), Junction (NUM_JUNCTION_PARAMS),
                                        Raffles (NUM_RAFFLES_PARAMS), Interchange
                                        (NUM_EXPANDER_PARAMS).
  FAIL (config sized to MonsoonIds::NUM_PARAMS ~1128):
                                        Monsoon, Straits (StraitsIds::NUM_PARAMS =
                                        MonsoonIds::NUM_PARAMS + 1), Causeway, East
                                        (StraitsEastSandsVisual), Macro (StraitsSandsMacroVisual).

The split is EXACT and it is the config() size. Straits/Causeway/East/Macro reuse Monsoon's
param IDs (they edit Monsoon's params over the expander bus), so each calls
`config(MonsoonIds::NUM_PARAMS, …)` to size its param array to the shared namespace. Every such
module claims a ~1128-slot host block and overflows -> the host drops ALL its params. The six
with their own modest namespaces register fine.

So it's not "the Monsoon module"; it's "the shared 1128-wide MonsoonIds param namespace",
inherited by 5 modules. Fixing the namespace size fixes all 5 at once.

### Why they inherit the full namespace
The MACRO/VARLEG/LANE_DIR editor-state params live in MonsoonIds so they persist centrally and
any expander can address them by the same id. Sizing each expander's param array to
NUM_PARAMS is how Rack gives every module a params[] big enough to hold those shared ids. The
cost: each module also EXPORTS all 1128 to the host.

### Fix (unchanged in kind, bigger in payoff)
Option A (host-hide the ~700 internal MACRO/VARLEG/LANE_DIR params) or B (move them out of
params[] into fields+JSON) now fixes Monsoon AND Straits AND Causeway AND East AND Macro
simultaneously — because all five shrink with the shared namespace. Confirmation test is the
same: shrink the internal ranges, rebuild, check VARIATION (Monsoon) AND a Straits/East knob
all appear.

Caveat for Option B: because the ids are SHARED (expanders address Monsoon's params by id over
the bus), moving them out of params[] means moving them to a shared, bus-addressable store —
more involved than a single module's fields. Option A (host-hide flag, keep the shared params)
is therefore even more strongly preferred here: it needs no change to the cross-module
addressing, only a flag on config.
