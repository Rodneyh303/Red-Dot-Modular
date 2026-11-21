# ZAI PROMPT — Resolver Step 3: LOR as `<LorPolicy>`, delete combineLOR's duplicate algebra

Step 3 of the SandsModifierResolver build order (SANDS_MODIFIER_RESOLVER_DESIGN.md):
spread rode the skeleton (step 1, byte-identical), warp rode it and closed the
consumer gate (step 2, one deliberate change). Now **LOR** — the design doc's
"near-verbatim DUPLICATE (15 send/tap refs). CORRECT but duplicated" — routes
through the resolver, and the duplicate algebra in `MonsoonExpanderManager.cpp`
dies. **This is a step-1-style step: ZERO behaviour change, byte-identical.**
No manifest entries, no gates, no deliberate changes.

## THE TASK (one commit)

1. Add `LorPolicy` to `src/dsp/SandsModifierResolver.hpp` (POLICIES section,
   beside SpreadPolicy/WarpPolicy) + a header-side `resolveLorItem(...)` wiring
   helper (beside `resolveWarpCell`).
2. Rewrite `combineLOR` (MonsoonExpanderManager.cpp:266-297) as a thin wiring
   lambda: ownership ternary + `eastLorVal` call + `resolveLorItem`. The
   delegation algebra (ternary/tap-lerp/send-blend/end-clamp) moves to the
   header; the port reads stay call-site.
3. New `test/test_resolver_lor_bridge.cpp` — the step-1/2 bridge pattern:
   verbatim oracle of the OLD combineLOR algebra vs the new wiring, seeded fuzz,
   same-BITS (float) + same-INT outputs.
4. Gates: 68 suites green (67→68), UNIFORMITY_STRICT=1 → 1494/1494, manifest
   FROZEN (zero entries change), plugin build SUCCESS.

## THE SPEC

### Verified mapping (old combineLOR → skeleton), per (lane, item)

Old body (MonsoonExpanderManager.cpp:275-297, minus the eastLorVal call):

```cpp
float base = (ownerEast || !macroPresent) ? eastLorVal(lane,item,r,c,lo,hi)
                       : (macroVis->macroBase[lane][item] + macroVis->macroCVDelta[lane][item]);
float blend = 0.f;
if (macroPresent && ownerEast) {
    float send = mmOwn ? mmOwn->getMacroSend(slot, lane, item) : 0.f;
    const float tap = (mmOwn ? mmOwn->getMacroTap(slot, lane, (item == 3 ? 1 : 0)) : 1.f);
    blend = macroVis->tappedDelta(tap, lane, item) * send;
}
return (int)math::clamp(base + blend, lo, hi);
```

The resolver-shaped rewrite:

- `Value = float` — ONE item's value (the manager resolves per item; 15 call
  sites each pass their own `(lo,hi)`). NOT a len/off/rot triple — per-item
  send/tap/jack addressing is the call structure; a triple would fake a batch.
- `Extra { float lo, hi; }` — the per-item window, threaded to `constrain`.
- `scaleCv = identity` — LOR's CV-span math (`cv·att·(hi−lo)`, the
  INTERMEDIATE clamp) stays inside `eastLorVal` (:254-262) because it is
  interleaved with module port reads (`eastVisual->inputs[cvId(r,c)]`,
  `getMacroAtten`, `getPolyVoltage`). Port reads cannot enter a pure policy;
  the design doc never asked for a port-abstraction layer. `ownCv`/`eastCv`
  stay default-disconnected in the wiring — addCv/scaleCv exist to satisfy
  the concept, unused by LOR's wiring.
- `addSend(v, s) = v + s` — the blend term, NOT clamped (end-clamp-once law).
- `applyDelegated(mb, td, e) = mb + td` — the ceded branch.
- `constrain(v, e) = max(e.lo, min(e.hi, v))` — the max/min form (NaN
  convention, same as SpreadPolicy/WarpPolicy — one convention per file).
- `static_assert(SandsModifierPolicy<LorPolicy>)` — the concept bites here.

### `resolveLorItem` — the ONE wiring, header-side (beside resolveWarpCell)

```cpp
// Returns the FINAL int (the old (int) cast — truncation, NOT round — stays
// part of the moved algebra so the call sites stay one-token thin).
inline int resolveLorItem(bool ownerEast, bool macroPresent,
                          float eastVal,          // the CALLER-computed eastLorVal result
                          float macroBase, float macroCvDelta,
                          float macroSend, float pre, float post, float tap,
                          float lo, float hi);
```

Body fills `SandsModifierResolver<LorPolicy>::Inputs` EXACTLY like the old
algebra, bit-for-bit:

- `in.delegated = !ownerEast && macroPresent;` — reproduces the old ternary's
  fallback EXACTLY: `ownerEast==false && macroPresent==false` must take the
  FREE path with `base = eastVal` (that is what
  `(ownerEast || !macroPresent) ? eastVal : ...` does). Do NOT write
  `delegated = !ownerEast` — that breaks the no-Macro case.
- free path: `in.base = eastVal;` and when `macroPresent`:
  `in.macroSend = send; in.macroPreDelta = pre; in.macroPostDelta = post;
  in.macroTap = tap;` → resolver does `addSend(v, tappedDelta(in)·send)`.
  The resolver's tap lerp (`pre + (post−pre)·tap`, skeleton :130-132) is
  BIT-IDENTICAL to `StraitsSandsMacroVisual::tappedDelta` (:318-323) — same
  expression order — so passing pre/post/tap separately reproduces the old
  `macroVis->tappedDelta(tap, lane, item)` bit-exactly. Do NOT call the
  Visual from the header (keep the header Rack-free; the CALLER reads the
  published pre/post arrays and this voice's tap).
- delegated path: `in.macroBase = macroBase;` and NEUTRALIZE the tap:
  `in.macroPreDelta = in.macroPostDelta = macroCvDelta;` (any tap). The
  resolver's lerp then yields exactly `macroCvDelta`: `(post−pre)==+0.0f`,
  `+0.0f·tap==+0.0f` (tap ≥ 0 from storage), `pre+0.0f==pre` — bit-exact.
  The old delegated delta is the POST delta (`macroCVDelta` directly, no
  lerp, no send) — this wiring reproduces it through the ONE shared
  applyDelegated instead of a special case.
- return `(int)LorPolicy-resolved value` — the `(int)` cast is TRUNCATION
  (toward zero) after the clamp. Preserve it exactly; the mono path rounds —
  that difference is why the cast lives in resolveLorItem, not in constrain.

### The combineLOR lambda after the rewrite (call-site, keeps)

```cpp
auto combineLOR = [&](int lane, int item, int r, int c, float lo, float hi)->int {
    const int elc = lane;
    const bool ownerEast = (topo.owner(v, elc) == dotModular::SandsTopology::Role::EAST);
    const float east = eastLorVal(lane, item, r, c, lo, hi);   // port reads + INTERMEDIATE clamp — VERBATIM, unchanged
    const bool mp = macroPresent;
    const float send  = (mp && ownerEast && mmOwn) ? mmOwn->getMacroSend(slot, lane, item) : 0.f;
    const float tap   = (mp && ownerEast && mmOwn) ? mmOwn->getMacroTap(slot, lane, (item == 3 ? 1 : 0)) : 1.f;
    return redDot::resolveLorItem(ownerEast, mp, east,
                                  macroVis ? macroVis->macroBase[lane][item] : 0.f,
                                  macroVis ? macroVis->macroCVDelta[lane][item] : 0.f,
                                  send, macroVis ? macroVis->macroPreDelta[lane][item] : 0.f,
                                         macroVis ? macroVis->macroPostDelta[lane][item] : 0.f,
                                  tap, lo, hi);
};
```

(Shape guide, not law — keep the 15 call sites and their (lo,hi) literals
UNTOUCHED; keep the guard semantics identical to the old body: send/tap were
read only inside the `macroPresent && ownerEast` blend branch, so reading them
under the same condition preserves the no-Macro / mmOwn-null behaviour
bit-for-bit. `eastLorVal`'s body, including its INTERMEDIATE
`base = clamp(base + cv·att·(hi−lo), lo, hi)`, must NOT be touched — that
mid-clamp is load-bearing: poly East-owned is
`clamp(clamp(base+cv) + blend)` — sum-once would differ, e.g. hi=16,
base+cv=21, blend=−3 → old 13, sum-once 16. The intermediate clamp survives
verbatim inside eastLorVal; the resolver's end-clamp-once applies to the
blend sum, exactly like the old final `clamp(base+blend)`.)

### The tap-index anomaly — PRESERVE VERBATIM, REPORT, DO NOT FIX

`(item == 3 ? 1 : 0)` inside combineLOR (:295) is always 0 (item ∈ {0,1,2}).
It is NOT a bug: `Monsoon::getMacroTap(v, lane, which)` (:976) stores TWO tap
slots per (voice,lane) — `which==1` is the SPREAD-kind tap (combineSpread
passes literal `1` at :325), `which==0` the LOR-kind tap. The ternary is a
copy-paste residue of a shared template where item 3 meant "the spread item".
The same dead ternary also lives at MonsoonSandsManager.cpp:306 and :534 (mono
copies, out of scope below). Keep all three verbatim (byte-identical law);
note them in the exit report as a future-cleanup candidate (rename to literal
`0` + a comment — Rodney-decidable, NOT this commit).

## THE BRIDGE TEST (test_resolver_lor_bridge.cpp)

Step-1/2 pattern, header-only, Rack-free, registered in run_all.sh as
`"test_resolver_lor_bridge|"`:

- ORACLE (frozen, verbatim from the OLD combineLOR :275-297, with the port
  parts factored to inputs): `lorOracle(ownerEast, macroPresent, eastVal,
  macroBase, macroCvDelta, send, tap, lo, hi) -> int` — the ternary, the
  `macroPresent && ownerEast` blend with
  `pre + (post−pre)·tap` then `·send`, the `(int)clamp(base+blend, lo, hi)`.
  The NEW-code side calls `resolveLorItem(...)` with the same inputs. Assert
  EQUALITY of the int AND same-BITS of the pre-cast float (expose the float
  via a second header helper or compare `(float)(int)` — pin BOTH layers).
- Named anchors: (a) REST/LEN literals (1,16) + (0,15) windows; (b) delegated
  with `macroBase+macroCVDelta` outside [lo,hi] → clamped (pins the delegated
  constrain); (c) `ownerEast && !macroPresent` → eastVal untouched then
  end-clamped; (d) `pre==post` tap-neutrality BIT-EXACT (any tap ⇒ same
  resolved value); (e) truncation pin: a resolved 15.999… → 15, −0.5 clamped
  to 0.0 → 0 (documents trunc-vs-round for the record); (f) tap=0 vs tap=1
  with pre≠post, send≠0 ⇒ DIFFERENT blends (the test must not be vacuous).
- Seeded fuzz ≥100k cases, coverage counters asserted at the end:
  ownerEast × macroPresent (all 4 combos incl. the no-Macro fallback),
  eastVal ∈ {lo, hi, mid, random-in-window}, macroBase ∈ {random, lo, hi},
  macroCvDelta ∈ {0, ±random, ±large (out-of-window)}, send ∈ {0, 1, mid,
  random}, tap ∈ {0, 1, mid, random}, (lo,hi) ∈ {(1,16),(0,15)} both orders
  exercised, plus the ±large-delta delegated-clamp cases. Print seed+coverage.
- Bit-identity note in the header comment: this bridge pins the MOVED algebra
  (ternary/lerp/blend/cast). `eastLorVal` does NOT move (port reads) and is
  NOT bridged — it stays manager-verbatim under the plugin-build gate, same
  as the spread/warp wiring lived under step 1/2's plugin builds.

## WHAT IS BIT-IDENTICAL (honesty block — copy its shape into the commit body)

BIT-IDENTICAL, all 15 call sites, both ownership branches, no-Macro fallback
included: the resolved int per (voice,lane,item). The bridge fuzzes the moved
algebra; the 15 call sites keep their (lo,hi) literals and jack addressing
untouched; `eastLorVal` untouched; U8/U9 harness checks untouched.
NO DELIBERATE CHANGES — this step is step-1 discipline, not step-2's gate flip.

## VERIFIED FACTS (grounded 2026-10-20 at 406847ca — re-verify if HEAD moved)

- combineLOR: MonsoonExpanderManager.cpp:266-297; eastLorVal :254-262
  (intermediate clamp at :259); tap ternary :295; 15 call sites:
  :332-334 (REST), :449-451 (MEL), :471-473 (OCT), :495-497 (ACC),
  :522-524 (QMIX) — each writes polyLen/Off/RotERef(v, lane).
- (lo,hi) per item: LEN (1,16), OFF (0,15), ROT (0,15) — every call site.
- Visual publish: StraitsSandsMacroVisual.hpp:313-324 — macroPreDelta[7][4] /
  macroPostDelta[7][4] public; tappedDelta lerp `pre+(post−pre)·tap`
  BIT-IDENTICAL to the skeleton's tappedDelta (Inputs) (:130-132).
- getMacroTap(v,lane,which): Monsoon.hpp:976, two slots `[(v*7+lane)*2+which]`;
  combineSpread passes literal 1 (:327) — the ternary's `==3` arm is dead
  residue, correct behaviour, preserve verbatim.
- Ownership: `topo.owner(v, elc) == EAST` — SandsTopology is the ONE authority;
  the resolver CONSUMES the boolean (membership rule, skeleton :22-25).
- Rig compiles ONLY GateState/PatternEngine/SequencerEngine/ClockEngine cpps
  (run_all.sh:45-48) — MonsoonExpanderManager.cpp and MonsoonSandsManager.cpp
  are rig-INVISIBLE: the plugin build is the ONLY production compile proof
  (step-1 rule). Both must compile; the include cycle
  (skeleton ⇄ WarpF.hpp) is proven safe in both entry orders (probes at
  406847ca verification) — managers include SpreadResolver.hpp early, which
  is the resolver-first order.
- Harness: U8 (topology owner matrix, :580-) + U9 (LOR store round-trip,
  :638-) are the LOR-adjacent checks — both unaffected; manifest FROZEN
  (no LOR seam entries exist; LOR was CORRECT-but-duplicated). STRICT exits 0
  today and MUST still exit 0 after this commit.
- test_send_tap.cpp pins the tap-lerp contract via the SpreadResolver shell +
  MacroTapBank round-trip — it must pass UNCHANGED (the lerp is shared; you
  are not touching it).

## HOW TO RUN

```
cd Red-Dot-Modular && git pull --ff-only
RACK_DIR=<sdk> bash test/run_all.sh                  # 68 suites, ALL green
UNIFORMITY_STRICT=1 RACK_DIR=<sdk> bash test/run_all.sh   # 1494/1494, exit 0
make RACK_DIR=<sdk> -j16                             # plugin build SUCCESS — MANDATORY (both manager cpps)
```

## COMMIT + EXIT REPORT

One commit: `refactor: LorPolicy — combineLOR rides SandsModifierResolver; the duplicate algebra dies (step 3, byte-identical)`.

Report: suite count + anchors (east 99 / macro 126 / keppel 19 / causeway 55),
STRICT result, bridge fuzz seed+coverage, plugin build result, the
tap-anomaly note (verbatim-preserved, 3 sites, future-cleanup candidate),
any fact that moved vs this brief.

## NOT THIS TASK

- The MONO LOR copies stay hand-rolled, OUT of scope, documented only:
  MonsoonSandsManager.cpp:287-314 (eastDelta/macroDelta — sum-once form) and
  :530-543 (addCV/sendBlend — ROUND not trunc, delegated branch UNclamped).
  Their divergences are why they are NOT byte-identical-extractable in this
  step; they are candidates for a future mono-policy mini-step (Rodney-decidable).
- No V1/mono behaviour edits, no θ/clamp policies, no concept-signature
  changes, no feat/warp-ui merge, no harness manifest edits, no "fix" of the
  tap ternary or of the pre-existing East-interp 0..1-vs-−1..1 range
  inconsistency (flagged in the combineSpread comment — separate issue).
