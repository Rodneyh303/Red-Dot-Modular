# SESSION LOG 2026-10-18 (b) — Phase 3: Distribution ownership, warp timing, send-tap bug, copula limits

Continuation of 2026-10-18 (a32cbaa). That session settled the warp STORAGE (mirror),
the boolean-lane proof, parameter modulation, and the copula corrections. This session
settles OWNERSHIP (where Distribution lives in per-voice state), the DISPLAY/ENGINE TIMING
model, finds and specs a real SEND-TAP BUG, and closes the tail-dependence question for good.

Working model unchanged: THIS chat = architecture + CC prompts; CC = tested/mechanical
execution; verify against code, never a plausible narrative.

────────────────────────────────────────────────────────────────────────────────
## 1. DISTRIBUTION OWNERSHIP — SETTLED (the last architectural blocker)

The tension: `Distribution` (src/dsp/Distribution.hpp) is POLYMORPHIC (pure-virtual
cdf/pdf/invCDF/typeName/toJson). Every per-voice lane bank in the codebase is a FLAT POD
float array indexed (v*7+lane) or (v*7+lane)*4+item: macroOwn[112], macroSend[448],
macroAtten[448], spread[112] (all Monsoon.hpp, SANDS CONSOLIDATION Step 1). These don't
compose — you can't put a vtable object in a float[], and unique_ptr<Distribution>[112]
breaks flat indexing, trivial-copy, memcpy-persistence, AND allocates on the control path
(modulated theta writes params per draw). variant<> still carries distinct types + wrecks
the flat array.

RESOLUTION — split identity+params (per-voice) from behaviour (not per-voice):
What varies per voice is WHICH distribution (an enum tag) + ITS PARAMS (≤4 doubles). The
BEHAVIOUR (TriangularDistribution::invCDF) is the SAME fn for every voice choosing that
kind. The polymorphism is over KIND, and kind is a TAG, not 112 objects.

Store per-voice the PLAIN DATA, flat-array style like everything else:

    struct LaneDist {               // POD, trivially copyable, memcpy-persistable
        uint8_t kind = KIND_UNIFORM;// enum: uniform/triangular/kumaraswamy/.../drawn
        float   p[4] = {};          // params; uniform ignores
        // drawn: index into a separate PCHIP pool, NOT inline
    };
    LaneDist laneDist[112];         // 16 × 7 — mirrors spread[112] EXACTLY

Evaluation = switch(kind) → the same closed-form math already in Distribution.hpp, as FREE
functions taking (params, x). No vtable, no heap, no alloc when theta modulates (overwrite
p[i]), flat indexing identical to spread[112], persistence = existing POD round-trip.

Distribution.hpp is NOT discarded — it stays as the AUTHORED / editor / set-time
representation (the natural object for the .dmcurve editor, toJson/fromJson, devhost,
MonotoneCurve::fromDistribution). ADD a one-way lowering `LaneDist lower(const Distribution&)`
at the seam where an authored curve enters per-voice engine state. SAME SHAPE as the
macroBase(authored) → published-deltas(hot) split in the tap path. Editor edits the rich
object; on commit it lowers to the flat bank; engine + mirror read ONLY the flat bank.

DRAWN CURVES are the one exception: a PCHIP curve is a control-point array, not ≤4 params.
It lives in a SEPARATE POOL; LaneDist holds an INDEX when kind==KIND_DRAWN. Per-voice state
stays small+flat; only voices using a drawn curve consume a pool slot. Modulation never
touches drawn curves (no scalar theta) → pool is set-time only, no control-path alloc.

THE RISK TO GUARD (lying-abstraction shape): two impls of each curve's math (the virtual
body AND the switch free fn) can silently diverge. DO NOT copy formulas. Factor each
curve's invCDF/cdf/pdf into ONE inline free fn that BOTH the Distribution subclass and the
LaneDist switch call → the existing 56,395-assertion battery covers both paths; add a
contract test `lower(d).invCDF(u) == d.invCDF(u)` across the palette to nail the lowering.

Where each piece lives:
  - src/dsp/Distribution.hpp — UNCHANGED (authored/editor/set-time form).
  - src/dsp/LaneDist.hpp (NEW) — POD struct, switch-based free fns (sharing the hpp's exact
    formulas via inline helpers = one source of truth per curve), lower(const Distribution&).
  - Monsoon::editor — LaneDist laneDist[112] beside spread[112] + the drawn-curve pool.
    Delegation (Macro default / Sands override) is IDENTICAL to spread (same shape).
  - mirror rebuild — reads laneDist[v*7+lane], dispatches switch, writes float.

Open sub-decision (defer, measure first): switch per eval vs a fn-pointer set at lower-time.
Start with switch — kind is near-constant per voice across a rebuild → branch-predictable,
and it keeps LaneDist trivially copyable. Reach for the pointer only if a profile says so.

────────────────────────────────────────────────────────────────────────────────
## 2. DISPLAY / ENGINE TIMING — SETTLED, and it SIMPLIFIES the storage

Two DIFFERENT operations, different granularities. Conflating them is the trap.

### Display toggle (raw uniform ↔ warped) → IMMEDIATE, then DROPPED ENTIRELY
A toggle is a LENS on the same data, not an edit → quantising it to a boundary feels broken
(click, nothing, then it changes). BUT on reflection (Rodney) the toggle is between "the
w-mixture (what's happening)" and "full warp at w=1 (a hypothetical the user didn't ask to
generate)" — that's a plausible display of a value NOT in effect, the exact failure class
the mirror exists to prevent. So: NO display toggle. The display ALWAYS shows the effective
w-mixture. If the user wants to see full warp, turn w to 1 and look — then display+engine
agree and what they see is what they'll hear.

### Engine warp = w, latched at DRAW TIME (step) — and it's PRINCIPLED, not preference
"Warp on/off" is NOT a mode switch needing its own scheduler — it's w:1→0, and w is already
a per-voice, modulatable, draw-time-latched lane param (from session a). One quantity, one
latch, NO second source of truth about "is warp on" (which could desync from w).

WHY STEP, NOT PHRASE (Rodney's key observation): phrase boundary has been reserved for
items that BREAK Sklar or uniformity if done per step. The w-mixture (1-w)·id + w·F^-1 is
MONOTONE for every w → rank-preserving, copula-invariant, Sklar-SAFE → changing w per step
breaks NO invariant. So w passes the per-step test and belongs at step granularity BY THE
SAME RULE that puts the other items at phrase. The rule is "quantise to phrase IFF per-step
breaks an invariant"; w is explicitly NOT one of those items. Principled placement, stated
so the next person applies the test rather than copying the granularity.

Continuous w also gives a SMOOTH uniform→warped morph over a phrase (sweep w 0→1) — a far
better gesture than a hard mode flip, and it's the live-preview keystone's cousin.

### Storage simplification (vs session a's proposal)
Session a had warped_[16][7][16] as the FULL-WARP mirror + a toggle selecting it vs raw.
Dropping the toggle collapses this: the mirror holds the EFFECTIVE w-mixture — the single
value BOTH engine and display read. Raw random_ stays underneath as the w=0 basis the
mixture is built from. NO separate full-warp array. One array, one truth → the latched-vs-
live-theta hazard DISAPPEARS (only one value to show).

### The protected property: SIGHT LEADS SOUND BY ONE STEP
Edit → display rebuilds IMMEDIATELY (on curve/w dirty) → heard at NEXT step (draw-time
latch). The user perceives: "I edit → I see it instantly → I hear it next step." The visual
confirms the edit registered; audio follows at the musical grid. This ONLY holds if display
rebuild is immediate AND engine latch is step. DO NOT later quantise the display to the step
"to keep them in sync" — that breaks the feel (edit → see nothing until next step = reads as
lag). The slight lead is a FEATURE. Protected.

Net timing model:
  - Mirror = effective w-mixture (one array). random_ retained as w=0 basis.
  - Display: immediate rebuild on any curve/w edit. No raw↔warped toggle. Shows effective mix.
  - Engine: w latched at draw time (step). Sklar-safe per step → step is correct by the
    phrase-boundary rule, not an exception to it.
  - Edit seen instantly, heard next step. Sight-leads-sound-by-one-step is deliberate.
  - Phrase boundary stays for the invariant-breaking items only.

────────────────────────────────────────────────────────────────────────────────
## 3. SEND-TAP BUG — found by code-reading, specced for CC

Verifying the "Macro-shared-spread + per-voice-send + global-modulation" topology against
code (to ground the w/param send plan) surfaced a REAL regression:
  - SEND is per-voice: getMacroSend(slot,lane,item), macroSend[448] (Monsoon.hpp).
  - TAP (PRE/POST the attenuverter) is MACRO-GLOBAL: getGlobalTap(lane,which), globalTap[14].
So every voice receiving a lane's send is forced onto Macro's single PRE/POST choice. Since
the tap exists ONLY to shape the per-voice send, a global tap on it has no rationale — a
SANDS CONSOLIDATION regression. (Confirmed: no per-voice/East-side tap exists; globalTap[14]
= 7 lanes × 2 kinds, no slot dim; the UI shows per-voice send knobs but only global tap.)

CORRECTIONS to session-a claims about this (code beat narrative, as designed):
  - The tap is NOT per-voice today — I'd described it as if it were. It's global. Bug.
  - PRE/POST is a CONTINUOUS lerp effAtt = 1+(att-1)·tap, NOT a binary toggle
    (MonsoonSandsManager.cpp tappedDelta ~L631). So per-voice tap is a float, not a switch —
    voices can sit PARTWAY between anchored (PRE) and breathing (POST).
  - The §6 "some voices breathe, some anchored on the SAME lane" structure that session a
    called unreachable is EXACTLY what fixing this unlocks.

The non-mechanical heart: per-voice tap moves the PRE/POST choice DOWNSTREAM of the publish,
so Macro can't pre-apply it. Macro must publish BOTH endpoints (macroPreDelta =
raw·(hi-lo); macroPostDelta = cv·att·(hi-lo)) and each voice selects/lerps with its own tap.
Both endpoints are already computable at the publish site (they're the two ends of the
existing lerp). Full CC spec: docs/design/CC_PROMPT_per_voice_send_tap.md (store reshape
globalTap→macroTap[224], dual-delta publish, per-voice select at EVERY reader [enumerated,
grep-verified], SpreadResolver as the pure-fn test seam [confirmed compiles standalone],
4-assert contract test that fails on HEAD~1). Ships ALONE, off lane-expander-refactor, before
any Phase 3 — it hardens the publish-both-endpoints contract that the w-send and per-param
sends inherit for free.

The dependence payoff (why it's more than a bug fix): the per-voice loading vector onto the
shared Macro factor is what selects which UNCONDITIONAL MIXTURE you get. Global tap forces
homogeneous tap → constrained mixtures (all breathe or all anchored). Per-voice tap +
bipolar send frees the loading vector → the heterogeneous-loading cases from §6, INCLUDING
negative common-factor dependence (needs OPPOSED loadings, hence the send must be bipolar —
it is: combineSpread clamps [-1,1] so blend can reach negative, MonsoonExpanderManager.cpp).

────────────────────────────────────────────────────────────────────────────────
## 4. RICHER COPULA FROM THE SEND TOPOLOGY — what it is and isn't

The Macro-shared + per-voice-send(+tap) topology IS the one-factor model in signal routing:
    drive_v = g_v·M (shared) + own_v (idiosyncratic)
g_v (send gain) = the factor LOADING. Spans the whole family: all-on-Macro (pure common
factor) → own+send (one-factor w/ idiosyncratic) → own-no-send (independent). Parallels
cleanly to w and to each marginal param (each becomes a new "item" with its own send+tap;
"only some voices have both params" falls out of per-item send being independently zeroable).

Pre/post tap = whether a voice's loading TRACKS Macro's attenuverter (POST, "breathing") or
is ANCHORED to raw CV (PRE). The send LEVEL is deliberately NOT modulated — that keeps the
loading a NUMBER within a draw, so X_v = μ + loading·factor + resid stays a LINEAR factor
model (modulating the level → product of two stochastic signals → no longer linear, harder
to reverse). Macro's attenuverter α IS modulatable and post-tap voices inherit it — that
modulates the FACTOR'S amplitude (shared, still clean), not an individual loading. Keep the
"send level static" constraint and SAY WHY in the doc so nobody "improves" it later.

Unconditionally this is a ONE-FACTOR / COMMON-FACTOR copula (Krupskii–Joe = Harry Joe the
AUTHOR of the factor-copula framework — NOT the Joe Archimedean copula; name collision,
keep them apart in docs, never abbreviate to the surname). NOT Archimedean (that needs
conditional INDEPENDENCE given the factor; here voices stay Gaussian-coupled given theta),
EXCEPT the degenerate r=0 + PowerDistribution case → genuine Archimedean generator M_P^-1
(inverse MGF of the modulation's amplitude law), the CLAYTON route — but bounded knob range
kills the tail. CA is a SEPARATE layer (latent-assignment operator, different files, no
crossover) — CA and w-modulation are INDEPENDENT handles on dependence at different layers;
they COMPOSE, neither routes the other. (Session-a's "CA routes w's factor structure" claim
WITHDRAWN — it was never a modulation router.)

────────────────────────────────────────────────────────────────────────────────
## 5. TAIL DEPENDENCE — closed for good, from every angle tried

The question "any easy route to λ>0 in this rich structure?" — answer: NO, and here's the
full map of why, so it isn't re-litigated:

FACTOR COPULAS *CAN* give tail dependence (t-factor, heavy/asymmetric links). OURS can't,
and it's the LINKS + BOUNDED FACTOR, not the factor structure:
  (a) Conditional coupling is GAUSSIAN → every component tail-INDEPENDENT (r<1) → mixing
      tail-indep components over a BOUNDED factor can't make λ>0. (λ_U ≤ G_max^(κ-1) → 0,
      κ=2/(1+r)>1.)
  (b) The factor is BOUNDED (knob range) → posterior given a coordinate-extreme collapses.
      t gets λ>0 because χ²_ν mixing is UNBOUNDED with its OWN mass.

ROUTES TRIED AND CLOSED:
  - Modulated truncation / support-shift → continuous conditionals, G_max→0, λ=0. (WITHDRAWN
    from session a: this was wrongly offered as the route to λ>0.)
  - Reparametrise a bounded knob to an UNBOUNDED factor axis (tan/logit) → CANNOT create λ>0:
    tail dependence is INVARIANT under reparametrisation of the factor (∫g dP is coordinate-
    free). Stretching the axis to reach θ=∞ makes P put vanishing mass there — exact cancel.
    The only way the remap bites the OUTPUT is by creating ATOMS at the rail = the clamp,
    with rank-destruction + unisons. No free lunch.
  - "Make t fast": the variance-mixture form X_v = S_s·Z_v with a Φ (not t) readout —
    OFFERED in session a as fast/reversible/LUT-free, now WITHDRAWN. Checked against
    MovingAverageCopula.hpp: the MA is a weighted sum in NORMAL space with L2-NORMALISED
    weights (Σw²=1) then Φ → marginal is EXACTLY uniform BECAUSE s~N(0,1). A shared scale
    s'=S_s·s makes s'~N(0,S_s²); to keep the marginal uniform the readout MUST re-standardise
    (÷S_s) → the scale CANCELS identically for every voice → copula UNCHANGED (plain
    Gaussian). No re-standardise → marginal breaks. No middle setting. The t needs t_ν on
    the readout (no cancel) → straight back to the LUT wall (polynomial tails incompatible
    with the saturating PhiLUT). FAST ROUTE DOES NOT EXIST by this path.
  - NB the slew copula is a finite MA(K=64) geometric-weight FIR in normal space, stateless
    at readout, reversible (r enters ONLY at readout, never the chain) — NOT an OU/AR(1)
    (recursive, wouldn't reverse under slew mod — explicitly rejected). So there's no
    fractional-OU long-memory kernel to exploit; and the L2-normalisation that guarantees
    uniformity is EXACTLY what annihilates any per-step multiplicative scale.

WHAT *IS* AVAILABLE: shared-theta WARP modulation on the marginals (the frailty mixture).
Real, cheap, reversible, λ=0. BUT it gives co-movement UNIFORM across the range (it moves
WHERE THE MASS IS and voices co-move through shared mass), NOT concentrated at extremes.
Extreme-concentrated / tail-gated co-movement is a property of the COPULA (quantile-varying
coupling); warp modulation is a mixture of MARGINALS (coupling fixed). You can AIM the
co-movement at a chosen region by shaping where the marginal is dense (e.g. voices surge
together in the upper register) — finite-quantile control over WHERE cohesion lives — but
you cannot make the cohesion gate ON only at the extreme. That's fine: λ is the inaudible
knife-edge; the audible thing is finite-quantile and you have rich control of it.

TUNABLE-INTERPOLATION CONTROL IDEA (documented, NOT built — gated behind the above
cancellation finding which likely kills its clean form): a "Weight" knob fading Gaussian→
co-moving (the ν-analogue, inverted so 0=off=bit-identical current path) + an optional
"Tail" knob picking P(s)'s upper tail (soft co-movement vs occasional tutti-slam = the λ>0
door). Present by EFFECT ("ensemble surge over time"), distinct from Spread's static "how
correlated." RECOMMENDATION: if ever pursued, expose Weight only, tail fixed light (λ=0),
hold the Tail/λ door for a second phase behind a feel-test. BUT per the cancellation result
the Φ-readout version is a no-op on the copula — so this needs the t_ν readout to mean
anything, which is the LUT wall. Effectively shelved.

────────────────────────────────────────────────────────────────────────────────
## 6. NET POSITION / WHAT'S NEXT

PHASE 3 ARCHITECTURE now specified END TO END:
  - WHERE the distribution lives: flat POD LaneDist[112] (engine/hot) + Distribution.hpp
    (authored) + one-way tested lower(); drawn curves via an index into a set-time pool. (§1)
  - HOW it's stored hot: a SINGLE effective-w-mixture mirror; raw random_ as the w=0 basis.
    (§2 — simpler than session a's two-array+toggle.)
  - WHEN it updates: w latched at draw time (step, Sklar-safe); display rebuilt immediately;
    sight-leads-sound-by-one-step is protected; phrase boundary untouched. (§2)

IMMEDIATE CC WORK (self-contained, ships first, off lane-expander-refactor):
  - docs/design/CC_PROMPT_per_voice_send_tap.md — the send-tap fix + 4-assert contract test.
    Hardens the dual-delta publish contract the w/param sends will reuse. (§3)

STILL OPEN — verify in code before building on:
  1. Distribution ownership: ANSWERED (§1) — LaneDist POD bank + lower(). Was the last
     architectural blocker. Remaining work is the CC build + the two contract tests.
  2. melodySource[16][16]: written 4 places, read NOWHERE outside persistence. The degree
     map doesn't consume it. faderMap∘F^-1 is where melody warp lands → PIN THE SEAM before
     designing it. Still open. Do not guess.
  3. (CLOSED this session) CA independence — confirmed separate layer, no crossover.

────────────────────────────────────────────────────────────────────────────────
## 7. UI ATTACHMENT — where the warp controls + editor hang off Sands/Macro (SETTLED)

Grounded against the live modules (images), the panel generators (panel_src/
gen_east_clean.py, gen_macro_mono.py), and the anchor/bind audit (test/audit_anchor_bind.py).

### The visual editor is DISPLAY-ONLY; the warp editor is a POPUP on a separate object
The Sands visual editor (SandsVisualEditorV4) shows ONLY per-step lane probability bars (7
lanes × 16 steps, per-voice via V1..V16 tabs). The per-cell bar editing found in
src/deprecated/SandsVisualEditorV4_with_percell_editing.hpp was UNINTENTIONAL and never used
(Rodney) — NOT a rejected feature. So the warp curve is edited in a SEPARATE POPUP bound to
an upstream object (the lane's LaneDist), never by dragging the output bars. Three distinct
surfaces, each showing ONE thing: (a) bars = engine output (on the module), (b) curve editor
= the authored F (popup), (c) effective-distribution view = the warped marginal (on a
SEPARATE VISUALISER MODULE, not the Sands panel).

### The bar-data funnel — the warp-display injection point (ONE accessor)
Every probability bar in BOTH Sands UIs reads through a SINGLE accessor:
finalRandomByStrand(strand,step) → random_[0][strand][step] (PatternEngine.hpp:419), copied
into visualEditor->currentState.lanes[el].probabilities[st] (StraitsEastSandsVisual.cpp:576,
1115, 1418). So warped display attaches in ONE place: the mirror holds the effective
w-mixture, the bar-fill copy reads it, bars redistribute for free (the live-preview
keystone). The warp mirror MUST sit at the same post-everything stage this accessor reads
(comment at :421 "pin remap lives upstream in slewed"), or warped bars won't line up with
raw step-for-step. PROB OUTS already read finalRandomByStrand → they carry the w-mixed value
automatically once the mirror feeds that accessor. No extra work; same single source feeds
bars + CV outs + visualiser.

### Control layout — WIDE PANELS NOW, bars fixed, insert space around them
Panel geometry (mm, from the generators; grid/bars = ED_X=88, ED_W=111, both "matches hpp"):
  - EAST 48HP(243.84): left cluster JACK_X[6,15,24,33]+ATTEN_X[43,52,61,70]+spread~80 → ends
    ~84, grid 88..199; right strip OWNER205/DIR212/DELEG_MOD220/DIR_MOD228/PROB_OUT236 → edge.
  - MACRO 58HP(294.64): same left; right adds SEND_COL_X[240,249,258,267,276,285] (6 cols,
    9mm pitch) after the strip. Column pitch = 9mm everywhere.
HARD CONSTRAINT (Rodney): DO NOT change the probability-bar size. Insert column space to the
LEFT and RIGHT of the bars; the bars keep their width, everything around them shifts.

New controls per lane (full-size — NOT shrunk; shrinking jacks/knobs rejected, it would
hurt the live controls to spare the least-used ones, and has no Rack precedent):
  LEFT of bars (BOTH panels): w-level knob, w-mod-depth knob, w-mod jack, param1 jack,
    param1 attenuverter, param2 jack, param2 attenuverter = 7 cols ≈ 63mm.
  RIGHT of bars (MACRO ONLY): w-mod-send knob, w-mod-tap knob, after the existing 6-col send
    block ≈ 18mm.
Resulting widths (interim, accepted): EAST ≈ 60HP, MACRO ≈ 74HP. Big, but SHIPPABLE without
depending on the subpanel/adjacency work (below). param1/param2 are per-distribution-KIND
(a,b for Kumaraswamy, μ,σ for logit-normal, inert for 1-param kinds) so their knobs are
semantically kind-dependent — a reason they are eventual subpanel candidates, but they go on
the FACE now because the subpanel route is blocked.

### SEND/TAP are ALL on MACRO, authored per-voice via Macro's TAB (Rodney — corrects §3/§4)
Model: whatever voice is selected on Macro's tab, you set THAT voice's send levels (and
per-voice tap, once the tap fix lands) on Macro; the values are sent to East. East RECEIVES
only — NO send/tap controls on East. So the per-voice-ness is expressed through Macro's tab
selection, not through East copies. (Earlier guess that "East needs its own tap/send columns"
is WRONG and dropped — East's right strip does NOT grow for this.) One authoring location
(Macro, tab-selected), one receiver (East) = same single-source discipline as the mirror.
→ macroTap[224] is authored on Macro per selected voice, read by East, NEVER written by East.
Fold this into the send-tap CC prompt.

### Popup trigger button — near the delegation/direction cells, right of bars
Opens the distribution editor for: lane (MACRO — ALWAYS available, edits Macro's own curve
for the lane); lane+voice (EAST — available ONLY when the lane is DELEGATED to East, because
that's when the voice has its own LaneDist rather than inheriting Macro's). Gate = the same
topo.owner(v,lane) predicate the send-blend uses. (Rodney confirmed: the delegation gate is
the EAST rule only; Macro has no such restriction.) Popup shows the curve; the lane's bars on
the same module redistribute live as you drag (keystone). The effective-distribution view is
the SEPARATE visualiser module, not the popup.

### Geometry integrity is GUARANTEED BY TEST — the width change is checkable, not click-verified
The two-geometry-sources risk (grid x hardcoded independently in SVG + widget, silently
diverging under a width change) is EXACTLY the regression class test/audit_anchor_bind.py
kills. It asserts at CI: every generator anchor has a matching widget bind, every bind
resolves to an anchor, and anchor-family counts equal the SandsGrid lane constants. Wired
into test/run_all.sh:251 (CI-failing). BOTH target panels pass today: sands_east (99 anchors
all bound), sands_macro (126). So the width task is: (1) add controls in the Python generator
(new descriptive anchors), (2) add matching binds in the widget by the same names, (3) ADD
THE NEW FAMILIES TO THE AUDIT'S `counts` CONFIG, (4) run the audit — green = art + click-
target agree and counts match. Bars never move in width; only surrounding x shifts.
*** THE AUDIT GAP: `counts` is hand-maintained. Anchor↔bind divergence is caught
AUTOMATICALLY, but a WRONG/absent count is only caught if you added the count. So add the
`counts` entry IN THE SAME COMMIT as the new controls — else a control omitted from BOTH
generator and widget passes silently (the silent-omission class this project has paid for). ***

### Audit coverage status (Rodney) — eventual whole-fleet migration
Only 4 modules are in the audit today: sands_east(99), sands_macro(126), keppel(19),
causeway(55). UNMIGRATED = {} is EMPTY — the other panels aren't "tracked with a reason,"
they're simply ABSENT from MODULES. GOAL: migrate ALL panels to bind-by-name + the audit.
Not part of the warp feature, but the fleet-wide direction; the warp UI work keeps east+macro
green and is a model for the rest.

### Subpanels — the RIGHT long-term home, but BLOCKED; deferred, then done HOLISTICALLY
Optional mod subpanels (as started for Straits) are the natural home for the param-mod
jacks+attenuverters (they show all 7 lanes at once — which the popup can't, Rodney's correct
objection to popup-hosting them). BUT the adjacency bug (Rodney, and visible in the shared
image: Macro interspersed between Straits subpanels): subpanels FOLLOW the main panel, but
NOTHING STOPS THE USER INSERTING A PANEL BETWEEN them. Rack has no API to forbid insertion —
the engine can only DETECT a broken chain (neighbour ≠ expected subpanel) and degrade, not
prevent it. So the subpanel route depends on first DESIGNING graceful degradation of split
controls (chain broken → modulation inputs inert, core instrument still plays). Decision:
WIDE PANELS NOW (unblocked); migrate to subpanels LATER, once (a) the warp feature is proven
AND (b) the insertion-break degradation is designed — and when done, migrate the WHOLE
modulation tier at once (param mods + extra LOR jacks/attenuverters + w-mod columns), not just
the new controls, so the rule is coherent ("live controls on panel, modulation inputs on
subpanel") and degradation is uniform. A partial split would carry the adjacency risk AND an
arbitrary on/off-panel boundary.

### UI DESIGN — SETTLED SUMMARY
  - Bars FIXED size; columns inserted left (w + param) and right (Macro send/tap). Full-size.
  - Panels wide now (~60HP east / ~74HP macro), interim, with subpanel exit later.
  - Send/tap ALL on Macro, per-voice via Macro's tab; East receives only.
  - Popup editor: Macro always; East only when lane delegated to East.
  - Prob outs carry the w-mixed value via the shared mirror (automatic).
  - Geometry guaranteed by audit_anchor_bind.py; add `counts` with the controls.
  - Separate visualiser module shows the effective distribution (not the popup, not the bars).

────────────────────────────────────────────────────────────────────────────────
WORKING MODEL (reminder): this chat = architecture + CC prompts; CC = execution
(tested/mechanical only, can't verify Rack/behavioural). Verify against code, never a
plausible narrative — this session WITHDREW 3 session-a claims and found 1 live bug purely
by reading code instead of trusting the story.
