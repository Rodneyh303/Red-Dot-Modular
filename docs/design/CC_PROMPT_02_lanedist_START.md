# CC PROMPT 02 — Phase 3 step 1: LaneDist POD bank + tested lower() + shared-math refactor

PREREQUISITE: CC_PROMPT_01 (per-voice send-tap) is merged and green. This prompt does NOT
depend on the tap code, but it is the next build step and assumes a clean trunk.

## READ FIRST (same discipline as prompt 01)
1. `CLAUDE.md` (root) — auto-context + non-negotiables.
2. `docs/design/CHAT_HANDOFF.md` — primer + working model + current state.
3. `docs/design/SESSION_LOG_2026-10-18b.md` §1 — the FULL reasoning for this design (why POD
   not unique_ptr/variant, why lower(), why the shared-math refactor). Read it; this prompt is
   the build steps, that section is the why.
4. `src/dsp/Distribution.hpp` — the existing polymorphic palette you are lowering FROM.

Branch: off `lane-expander-refactor`, `git checkout -b feat/lanedist-bank`. PR into it.
Rules unchanged: verify against tests, red→green for any claim, header-only tests where
possible, you can't test Rack.

## GOAL
Give the engine a flat, allocation-free, per-voice distribution bank that mirrors the house
pattern (macroSend[448], spread[112]), WITHOUT touching the control path with heap or vtables.
Distribution.hpp stays as the AUTHORED/editor/set-time form; the engine reads a POD lowering.

## THE DESIGN (settled — SESSION_LOG_2026-10-18b.md §1)
`Distribution` is polymorphic (virtual cdf/pdf/invCDF). Per-voice banks are flat float arrays.
These don't compose. Resolution: store per-voice the TAG + PARAMS (plain data); dispatch
behaviour by switch to the SAME math. unique_ptr allocates on the modulation control path
(rejected); variant wrecks the flat array (rejected).

## STEPS (test-first; suite green after each)

### STEP 0 — the shared-math refactor (do this FIRST; it's the lying-abstraction guard)
Today each curve's invCDF/cdf/pdf lives in a Distribution subclass method. You are about to
add a SECOND evaluation path (the switch). Two copies of the same formula WILL diverge — that
is the exact failure class this project has paid weeks for. So BEFORE adding the second path:
factor each curve's math into ONE inline free function in a shared header, e.g.
`src/dsp/DistributionMath.hpp`:
    namespace redDot::distmath {
      inline double triangular_invCDF(double u, double mode) { ... }   // the body currently
      inline double triangular_cdf   (double x, double mode) { ... }   // in TriangularDistribution
      inline double triangular_pdf   (double x, double mode) { ... }
      // ... power, kumaraswamy, logitnormal, logitsigmoid, mobius, truncnormal
    }
Then make each Distribution subclass method CALL the free function (one-line bodies). This is a
PURE REFACTOR: `bash test/run_all.sh test_distribution` must stay green (all 56k+ asserts),
byte-identical. Commit as "refactor: distribution math to shared free fns (no behaviour change)".
Do NOT proceed until that is green — the shared fn is the single source of truth both paths use.

### STEP 1 — the POD struct + enum
`src/dsp/LaneDist.hpp` (NEW):
    enum DistKind : uint8_t { DK_UNIFORM=0, DK_TRIANGULAR, DK_POWER, DK_KUMARASWAMY,
                              DK_LOGITNORMAL, DK_LOGITSIGMOID, DK_MOBIUS, DK_TRUNCNORMAL,
                              DK_DRAWN };
    struct LaneDist {                 // POD, trivially copyable, memcpy-persistable
        uint8_t kind   = DK_UNIFORM;
        uint8_t drawId = 0;           // index into the drawn-curve pool when kind==DK_DRAWN
        float   p[4]   = {};          // params; DK_UNIFORM ignores
    };
    // switch-dispatched evaluation, calling distmath:: free fns:
    double laneInvCDF(const LaneDist&, double u, const class DrawnPool&);
    double laneCdf   (const LaneDist&, double x, const DrawnPool&);
    double lanePdf   (const LaneDist&, double x, const DrawnPool&);
DK_UNIFORM returns u/x identity (the w=0 / no-warp fast path — no math). DK_DRAWN indexes the
pool (STEP 3). Keep LaneDist trivially copyable: NO methods that allocate, NO std::function.

### STEP 2 — lower(): Distribution -> LaneDist (the tested seam)
In LaneDist.hpp (or a .cpp if it needs jansson): `LaneDist lower(const Distribution&)`.
Reads the distribution's typeName()/params, fills kind + p[]. Drawn curves: register the PCHIP
into the pool, store the returned index in drawId.
CONTRACT TEST `test/test_lanedist.cpp` (header-only, model on test_distribution.cpp):
  - For EVERY parametric kind, across a param sweep: `laneInvCDF(lower(d), u, pool) == d.invCDF(u)`
    to double tolerance, over a dense u grid. (Proves the switch path and the subclass path —
    now both calling distmath:: — agree.)
  - Same for laneCdf/lanePdf.
  - DK_UNIFORM: laneInvCDF == u exactly (identity).
  - A drawn curve: lower() then laneInvCDF matches the source MonotoneCurve::invCDF.
Register `"test_lanedist|"` in run_all.sh TESTS[] (header-only form). Must be GREEN.

### STEP 3 — the drawn-curve pool
A set-time pool (DK_DRAWN only; modulation never touches drawn curves → no control-path alloc).
Simplest correct form: a fixed-capacity array of MonotoneCurve in the engine state (or a
std::vector populated only at set-time / load). LaneDist.drawId indexes it. Only voices using a
drawn curve consume a slot. Document the capacity choice; assert drawId < capacity.

### STEP 4 — the per-voice bank in engine state
In Monsoon::editor (beside spread[112]): `LaneDist laneDist[112];` (16 voices × 7 lanes,
index v*7+lane — SAME scheme as spread/macroOwn). Add getLaneDist(v,lane)/setLaneDist(v,lane).
Delegation (Macro default / Sands override) works identically to spread — do NOT invent a new
delegation mechanism; reuse the owner()/topo pattern.
NO engine evaluation wired yet — that's prompt 03 (the mirror). This prompt just lands the
bank + lowering + tests, so the data structure exists and is proven before the hot path uses it.

### STEP 5 — persistence
laneDist[112] round-trips in MonsoonPersistenceManager. kind+params are trivial; drawn curves
persist their PCHIP (reuse the .dmcurve/MonotoneCurve JSON already in the codebase) keyed by
drawId. Missing key ⟹ DK_UNIFORM (no warp — old patches unchanged). Add a persistence assert
to test_lanedist (save→load a mixed bank: a parametric, a drawn, a uniform → all survive).

## DONE =
- distmath:: shared free fns; Distribution subclasses call them; test_distribution still green.
- LaneDist.hpp: POD struct + switch eval + lower(); test_lanedist green (agreement + uniform +
  drawn + persistence asserts).
- Drawn pool; laneDist[112] bank in editor with delegation; persistence round-trips.
- `bash test/run_all.sh` ALL GREEN. No mirror, no engine hot-path wiring, no UI (prompt 03+).
- PR body: paste test_lanedist output; confirm test_distribution byte-identical after the refactor.

## NOT THIS PROMPT
- The warped_ mirror / hot-path evaluation (prompt 03).
- w (depth) param, modulation, the UI columns, the popup editor (prompts 03/04).
- Melody/octave warp composition (prompt 03 — the genPitchLive seam).
