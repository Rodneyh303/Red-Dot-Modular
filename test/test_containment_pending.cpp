// test_containment_pending.cpp — Containment step 3: the phrase-boundary pending machinery.
// Build plan row 3: "Pending + phrase-boundary apply (CAPendingOrder reuse; Rate::Phrase; owner
// guard)" -> this suite. The core (dsp/ContainmentPending.hpp) is rack-free, so the whole
// boundary path is testable without the SDK — the CA pending-suite discipline
// (test_ca_pending_order.cpp) carried to the containment verb rows.
//
// What is pinned, section by section:
//   P0  latch/stamp rules — stamp on unarmed->armed ONLY; re-arm keeps the original stamp; the
//       counter is monotonic across arms (the CA latchRow single-choke-point rule).
//   P1  hold-then-commit — the phrase-boundary timing core: latching NEVER touches the graph
//       (the hold); only applyAll — the host-driven boundary call — moves it. Between boundaries
//       the graph is the stable skeleton.
//   P2  chronological order — spaced arms apply in stamp order, and ORDER IS AUDIBLE: the
//       containment verb algebra is largely natural (invert/collapse commute — relation-algebra
//       fact (R^T)^2 == (R^2)^T, pinned here as documentation) but COLLAPSE-ALL x INVERT do NOT
//       commute, so the stamp order is observable in ears, exactly CA's lossy-composition story.
//   P3  same-sample tie-break — equal stamps (a CV burst / chord of presses) fall back to
//       ROW-INDEX order = the panel flank order (ROTATE, REFLECT, INVERT, COLLAPSE,
//       COLLAPSE-ALL, EXPAND) — nothing to regress, the CA tie rule.
//   P4  out-of-axis persistence — an apply with the plane's axis bit CLEAR commits nothing: rows
//       stay armed, stamps are KEPT, the counter does NOT reset; a later in-axis apply commits
//       them in their ORIGINAL chronological position (no jump-to-front) relative to arms made
//       meanwhile. This is the machinery the live-under-lock scope work will narrow onto.
//   P5  reset-when-empty — the stamp counter resets ONLY when the pending set is fully empty
//       (CAPendingOrder.hpp rule 4): resetting at every boundary would give new arms stamp 0 <
//       persistent rows' stamps -> they would jump to the FRONT of later commits.
//   P6  EXPAND trajectory (R3) — the ring of committed states: collapse-then-expand restores the
//       level the collapse removed (works from the FIRST commit — the pre-state push); EXPAND at
//       the buffer start STOPs (no wrap, state unchanged, row still consumed); static passages
//       (boundaries with nothing armed) consume NO depth; expand-then-new-verb branches (the
//       popped future is discarded); edits since the last commit are part of the popped level.
//   P7  ring serialise/deserialise — bounded-tail blob round-trips; a corrupt state in the blob
//       is REJECTED (the corrupt-save writer is why the graph guard exists — the step-1 lesson).
//   P8  rotate step config — ROTATE's <=1 param rides the pending row (default 1; clamped at
//       apply: 0 and span cannot select nothing, the clampStep contract).
//   P9  batch closure march — seeded-Philox random DAGs x random verb BATCHES applied at
//       simulated boundaries: after EVERY boundary commit the graph is acyclic, self-bit-free,
//       and a byte-identical autosave round-trip. The verbs' closure is proven per-verb in
//       test_containment_transforms; THIS pins the composition: a whole committed batch —
//       any number of verbs, any stamp order, any active count — is an endomorphism on DAGs.
//   P10 the RANDOM row (row 7; IDEAS_PARKED.md 2026-10-27) — the CA dice model on the row's
//       OWN Philox stream: the page counter advances per the armed direction; OUTRIGHT's
//       reverse-draw is the EXACT state restore (stateless reverse-as-undo); forward
//       re-derivation is exact; the latest arm owns the direction; SCATTER's reverse is the
//       previous page on the CURRENT graph — the CA scrub, NOT a state restore.
//
// Compile (registered in test/run_all.sh as header-only; PhiloxRng needs the simde include):
//   g++ -std=c++20 -Itest -Isrc -I${RACK_DIR}/dep/include test/test_containment_pending.cpp
#include "../src/dsp/ContainmentPending.hpp"
#include "../src/dsp/PhiloxRng.hpp"
#include <cstdio>
#include <cstring>
#include <memory>   // the rings are 2 MiB each — the test heap-holds them (main's frame must not)

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)

using namespace dotModular::ct;
using CP = ContainmentPending;

// ── helpers (the step-2 suite's idiom, shared verbatim where meanings coincide) ──────────────
static bool sameState(const ContainmentGraph& a, const ContainmentGraph& b) {
    return std::memcmp(a.parents, b.parents, sizeof(a.parents)) == 0;
}
static bool noSelfBits(const ContainmentGraph& g) {
    for (int v = 0; v < VOICES; ++v)
        if ((g.parents[v] >> v) & 1u) return false;
    return true;
}
// The WARPDIAG contract extended to the boundary path: any state a commit produces must be a
// loadable save (the deserialize guard accepts it, byte-identical).
static bool roundTripOk(const ContainmentGraph& g) {
    uint16_t raw[VOICES];
    g.serializeTo(raw);
    ContainmentGraph h;
    return h.deserializeFrom(raw) && sameState(g, h);
}
// Random DAG built incrementally under the same edit checks (accepted addParent only).
static void buildRandomDag(ContainmentGraph& g, redDot::PhiloxRng& rng, int attempts) {
    for (int i = 0; i < attempts; ++i) {
        const int v = (int)(rng() & 15u);
        const int u = (int)(rng() & 15u);
        g.addParent(v, u);   // rejects (cycles/self) no-op — the graph stays a DAG by construction
    }
}

int main() {
    // ── P0. latch/stamp rules ─────────────────────────────────────────────────────────────────
    {
        CP p;
        p.latch(CP::V_INVERT);
        CHK(p.armed[CP::V_INVERT] && p.stamp[CP::V_INVERT] == 0 && p.counter == 1,
            "latch: first arm stamps 0, counter advances");
        p.latch(CP::V_COLLAPSE);
        CHK(p.stamp[CP::V_COLLAPSE] == 1 && p.counter == 2,
            "latch: second arm stamps 1 (chronological)");
        p.latch(CP::V_INVERT);   // re-arm an ALREADY-armed row
        CHK(p.armed[CP::V_INVERT] && p.stamp[CP::V_INVERT] == 0 && p.counter == 2,
            "latch: re-arming an armed row keeps its original stamp, counter unmoved");
        CHK(p.anyArmed(), "latch: anyArmed true with rows pending");
        // out-of-range verb is inert (defensive; the panel/step-4 UI never produces one)
        CP q;
        q.latch(-1); q.latch(99);
        CHK(!q.anyArmed() && q.counter == 0, "latch: out-of-range verb ignored");
    }

    // ── P1. hold-then-commit (the phrase-boundary timing core) ────────────────────────────────
    {
        ContainmentGraph g;
        CHK(g.addParent(0, 2) && g.addParent(2, 5), "hold: setup chain accepted");
        CP p;
        auto traj = std::make_unique<ContainmentTrajectory>();
        p.latch(CP::V_ROTATE);
        ContainmentGraph held = g;
        // ... time passes (samples, edits elsewhere, CV wiggles) — the graph is HELD ...
        CHK(sameState(g, held), "hold: latching never touches the graph (the stable skeleton)");
        const int applied = p.applyAll(g, 6, *traj);   // the host-driven boundary
        CHK(applied == 1, "hold: boundary commits exactly the one armed row");
        CHK(!sameState(g, held), "hold: the boundary moved the graph");
        CHK(!p.armed[CP::V_ROTATE] && !p.anyArmed() && p.counter == 0,
            "hold: row disarmed after commit; pending set fully empty -> counter reset");
        // the result is exactly the verb's oracle (rotate +1 within the pool of 6)
        ContainmentGraph oracle = held;
        rotateRoles(oracle, 6, 1);
        CHK(sameState(g, oracle), "hold: committed result == the pure verb applied once");
        // a boundary with NOTHING armed is a no-op — and consumes no trajectory depth
        const int depthBefore = traj->size();
        ContainmentGraph g2 = g;
        CHK(p.applyAll(g, 6, *traj) == 0 && sameState(g, g2) && traj->size() == depthBefore,
            "hold: empty boundary = no-op (no graph move, no ring depth consumed)");
        CHK(g.isValid() && noSelfBits(g) && roundTripOk(g), "hold: committed state is a valid loadable DAG");
    }

    // ── P2. chronological order (and where the order is audible) ──────────────────────────────
    {
        // Naturality note (pinned as documentation): INVERT and COLLAPSE commute — collapse is
        // relational squaring R;R, invert is transpose R^T, and (R^2)^T == (R^T)^2. The chain
        // example: both orders land on the same graph. (The CA pending suite's "order is audible"
        // example used collapse x rotate; containment's rotate is a CONJUGATION — natural too —
        // so the commuting pair to catch is COLLAPSE-ALL x INVERT below.)
        {
            ContainmentGraph g;
            CHK(g.addParent(2, 3) && g.addParent(3, 4), "order: setup chain accepted");
            CP p; auto traj = std::make_unique<ContainmentTrajectory>();
            p.latch(CP::V_INVERT);    // armed first
            p.latch(CP::V_COLLAPSE);  // armed later
            const int applied = p.applyAll(g, 16, *traj);
            CHK(applied == 2, "order: both rows committed at one boundary");
            ContainmentGraph a = g;   // invert-then-collapse (what applyAll just did)
            ContainmentGraph b;       // collapse-then-invert, from the SAME setup, fresh graph
            b.addParent(2, 3); b.addParent(3, 4);
            collapse(b); invert(b);
            CHK(sameState(a, b), "order: invert x collapse commute ((R^T)^2 == (R^2)^T) — documented");
        }
        // The NON-commuting pair: COLLAPSE-ALL then INVERT != INVERT then COLLAPSE-ALL.
        {
            ContainmentGraph g;
            CHK(g.addParent(0, 1) && g.addParent(1, 2) && g.addParent(5, 6),
                "order: setup chain + separate component accepted");
            CP p; auto traj = std::make_unique<ContainmentTrajectory>();
            p.latch(CP::V_COLLAPSE_ALL);   // stamp 0
            p.latch(CP::V_INVERT);         // stamp 1  -> applies COLLAPSE-ALL first
            const int applied = p.applyAll(g, 16, *traj);
            CHK(applied == 2, "order: non-commuting batch committed (2 rows)");
            // oracle: collapseAll FIRST, then invert
            ContainmentGraph oracle;
            oracle.addParent(0, 1); oracle.addParent(1, 2); oracle.addParent(5, 6);
            collapseAll(oracle); invert(oracle);
            CHK(sameState(g, oracle), "order: stamp order honoured (collapseAll-then-invert)");
            // the reverse arm order gives the OTHER composition — different graph
            ContainmentGraph g2;
            g2.addParent(0, 1); g2.addParent(1, 2); g2.addParent(5, 6);
            CP q; auto traj2 = std::make_unique<ContainmentTrajectory>();
            q.latch(CP::V_INVERT);         // armed first now
            q.latch(CP::V_COLLAPSE_ALL);
            q.applyAll(g2, 16, *traj2);
            ContainmentGraph oracle2;
            oracle2.addParent(0, 1); oracle2.addParent(1, 2); oracle2.addParent(5, 6);
            invert(oracle2); collapseAll(oracle2);
            CHK(sameState(g2, oracle2), "order: reversed arm order -> reversed composition");
            CHK(!sameState(g, g2),
                "order: ORDER IS AUDIBLE — collapseAll x invert do not commute, stamps decide");
            CHK(g.isValid() && g2.isValid() && roundTripOk(g) && roundTripOk(g2),
                "order: both compositions are valid loadable DAGs");
        }
    }

    // ── P3. same-sample tie-break (row index = the panel flank order) ─────────────────────────
    {
        // Two rows armed "in the same sample": equal stamps (a CV burst / chord of presses).
        // The API's latch always advances the counter, so the tie is written directly — the same
        // approach as test_ca_pending_order (orderRows is the unit under tie behaviour).
        ContainmentGraph g;
        CHK(g.addParent(0, 1) && g.addParent(1, 2) && g.addParent(5, 6), "tie: setup accepted");
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        p.armed[CP::V_INVERT]       = true; p.stamp[CP::V_INVERT] = 7;   // same stamp...
        p.armed[CP::V_COLLAPSE_ALL] = true; p.stamp[CP::V_COLLAPSE_ALL] = 7;   // ...same sample
        const int applied = p.applyAll(g, 16, *traj);
        CHK(applied == 2, "tie: both tied rows committed");
        // row-index tie-break: INVERT (row 2) applies BEFORE COLLAPSE-ALL (row 4).
        ContainmentGraph oracle;
        oracle.addParent(0, 1); oracle.addParent(1, 2); oracle.addParent(5, 6);
        invert(oracle); collapseAll(oracle);
        CHK(sameState(g, oracle), "tie: equal stamps -> row-index order (INVERT row 2 before COLLAPSE-ALL row 4)");
        // and the tie result must equal plain row-index iteration (nothing regressed vs verb-major):
        // it must NOT be the swapped composition.
        ContainmentGraph swapped;
        swapped.addParent(0, 1); swapped.addParent(1, 2); swapped.addParent(5, 6);
        collapseAll(swapped); invert(swapped);
        CHK(!sameState(g, swapped), "tie: result is the row-index composition, not the swapped one");
    }

    // ── P4. out-of-axis persistence (the live-under-lock machinery) ───────────────────────────
    {
        ContainmentGraph g;
        CHK(g.addParent(0, 1) && g.addParent(1, 2) && g.addParent(5, 6), "axis: setup accepted");
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        p.latch(CP::V_COLLAPSE_ALL);   // stamp 0 — armed "while locked", plane out-of-axis
        ContainmentGraph held = g;
        CHK(p.applyAll(g, 16, *traj, 0u) == 0,
            "axis: mask-0 apply commits nothing");
        CHK(sameState(g, held), "axis: graph untouched by the out-of-axis boundary");
        CHK(p.armed[CP::V_COLLAPSE_ALL] && p.stamp[CP::V_COLLAPSE_ALL] == 0,
            "axis: row stays armed and KEEPS its stamp");
        // an arm made while the plane is still out-of-axis gets a LATER stamp (counter not reset)
        p.latch(CP::V_INVERT);
        CHK(p.stamp[CP::V_INVERT] == 1,
            "axis: counter NOT reset by the deferred boundary (new arm stamps above the persistent row)");
        CHK(p.applyAll(g, 16, *traj, 0u) == 0 && sameState(g, held),
            "axis: second out-of-axis boundary still defers everything");
        // the plane comes back in-axis: BOTH rows commit, in the ORIGINAL chronological order
        const int applied = p.applyAll(g, 16, *traj);   // default mask = the plane
        CHK(applied == 2, "axis: the in-axis boundary commits both deferred rows");
        // order proof: COLLAPSE-ALL (stamp 0) BEFORE INVERT (stamp 1). The pair does not commute
        // (P2), so the resulting graph distinguishes the order.
        ContainmentGraph oracle;
        oracle.addParent(0, 1); oracle.addParent(1, 2); oracle.addParent(5, 6);
        collapseAll(oracle); invert(oracle);
        CHK(sameState(g, oracle),
            "axis: deferred rows commit in ORIGINAL stamp order — no jump-to-front");
        CHK(!p.anyArmed(), "axis: deferred rows disarmed after their late commit");
        CHK(g.isValid() && roundTripOk(g), "axis: late batch result is a valid loadable DAG");
    }

    // ── P5. reset-when-empty (CAPendingOrder rule 4) ──────────────────────────────────────────
    {
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        ContainmentGraph g;   // empty graph: verbs run but must stay valid
        p.latch(CP::V_REFLECT);
        p.applyAll(g, 16, *traj);                    // full commit -> set empty -> counter resets
        CHK(p.counter == 0 && !p.anyArmed(),
            "reset: full commit resets the stamp counter (next arm starts at 0)");
        p.latch(CP::V_ROTATE);
        CHK(p.stamp[CP::V_ROTATE] == 0, "reset: post-reset arm stamps 0");
        p.applyAll(g, 16, *traj);
        // the NOT-empty branch: a deferred (mask-0) boundary must NOT reset
        p.latch(CP::V_COLLAPSE);
        p.applyAll(g, 16, *traj, 0u);
        CHK(p.armed[CP::V_COLLAPSE] && p.counter != 0,
            "reset: deferred boundary keeps the counter (persisting row stays comparable)");
        p.latch(CP::V_EXPAND);
        CHK(p.stamp[CP::V_EXPAND] == p.stamp[CP::V_COLLAPSE] + 1,
            "reset: arms after a deferred boundary continue the monotonic sequence");
        p.applyAll(g, 16, *traj);
        CHK(p.counter == 0, "reset: counter resets only when the set drains fully");
    }

    // ── P6. EXPAND trajectory (R3: memory, not an inverse) ────────────────────────────────────
    {
        ContainmentGraph g;
        CHK(g.addParent(0, 1) && g.addParent(1, 2), "expand: setup chain accepted");
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        ContainmentGraph preCollapse = g;
        p.latch(CP::V_COLLAPSE);
        CHK(p.applyAll(g, 16, *traj) == 1, "expand: collapse committed");
        CHK(g.isContainedIn(0, 2) && !g.isContainedIn(0, 1) && !g.isContainedIn(1, 2),
            "expand: chain collapsed to the grandparent re-link (0<2)");
        CHK(traj->size() == 2,
            "expand: ring holds [pre-state, post-state] — the trajectory is complete from commit 1");
        // EXPAND restores the level the collapse removed — the feature works from the FIRST commit
        p.latch(CP::V_EXPAND);
        CHK(p.applyAll(g, 16, *traj) == 1, "expand: expand committed");
        CHK(sameState(g, preCollapse),
            "expand: the pre-collapse level is restored (trajectory memory, not a synthesized inverse)");
        CHK(traj->size() == 1,
            "expand: ring popped to the restored level (the popped future is discarded — branching)");
        // EXPAND at the buffer start: STOP, don't wrap — state unchanged, row still consumed
        p.latch(CP::V_EXPAND);
        CHK(p.applyAll(g, 16, *traj) == 1, "expand: expand-at-start still consumes its row");
        CHK(sameState(g, preCollapse), "expand: at the buffer start the state is left unchanged (no wrap)");
        // static passages consume NO depth
        const int d0 = traj->size();
        CHK(p.applyAll(g, 16, *traj) == 0 && traj->size() == d0,
            "expand: a boundary with nothing armed consumes no depth");
        // branching: expand, then a NEW verb — the popped future stays discarded
        p.latch(CP::V_INVERT);
        p.applyAll(g, 16, *traj);
        ContainmentGraph inverted = g;
        CHK(traj->size() == d0 + 1, "expand: the new commit pushed onto the branched trajectory");
        p.latch(CP::V_EXPAND);
        p.applyAll(g, 16, *traj);
        CHK(!sameState(g, inverted) && g.isValid(),
            "expand: stepping back lands on the pre-invert level of the branched trajectory");
        // edits since the last commit are part of the popped level (documented semantics)
        {
            ContainmentGraph g3;
            g3.addParent(0, 1); g3.addParent(1, 2);   // the chain again
            CP q; auto t2 = std::make_unique<ContainmentTrajectory>();
            q.latch(CP::V_COLLAPSE);
            q.applyAll(g3, 16, *t2);
            CHK(g3.isContainedIn(0, 2), "expand: (edit-ride) collapsed to {0<2}");
            const ContainmentGraph committed = g3;
            g3.removeParent(0, 2);                     // an uncommitted pin edit: 0 detached -> EMPTY
            CHK(g3.isRoot(0) && !sameState(g3, committed), "expand: (edit-ride) uncommitted edit changed the held graph");
            q.latch(CP::V_EXPAND);
            q.applyAll(g3, 16, *t2);
            CHK(sameState(g3, committed),
                "expand: expand steps the trajectory back one COMMIT — uncommitted edits ride the popped level");
        }
        CHK(g.isValid() && noSelfBits(g) && roundTripOk(g), "expand: trajectory landing is a valid loadable DAG");
    }

    // ── P7. ring serialise/deserialise (bounded tail, version guard) ──────────────────────────
    {
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        ContainmentGraph g;
        g.addParent(0, 1); g.addParent(1, 2);
        p.latch(CP::V_COLLAPSE); p.applyAll(g, 16, *traj);
        p.latch(CP::V_INVERT);   p.applyAll(g, 16, *traj);
        CHK(traj->size() == 3,
            "serial: two commits -> [pre, post1, post2] (commit 2's pre-push skipped: unchanged)");
        auto blob = traj->serialise();
        auto t2 = std::make_unique<ContainmentTrajectory>();
        CHK(t2->deserialise(blob) && t2->size() == traj->size(),
            "serial: blob round-trips (count preserved)");
        // walk the restored trajectory and confirm the states match the source ring's, step for
        // step, until both rings hit their start together (stepBack leaves the newest entry as
        // the current state, so a 3-entry ring yields 2 comparable backward steps)
        bool walkOk = true;
        int steps = 0;
        while (true) {
            ContainmentGraph a, b;
            const bool ra = traj->stepBack(a), rb = t2->stepBack(b);
            if (ra != rb) { walkOk = false; break; }   // different depths = wrong restore order
            if (!ra) break;
            if (!sameState(a, b)) walkOk = false;
            ++steps;
        }
        CHK(walkOk && steps == 2, "serial: stepBack walks the restored tail in the SAME order");
        // corrupt state in the blob -> REJECTED (the corrupt-save writer is why the guard exists)
        blob.states[1].parents[3] |= (uint16_t)(1u << 5);
        blob.states[1].parents[5] |= (uint16_t)(1u << 3);   // plant a 2-cycle
        auto t3 = std::make_unique<ContainmentTrajectory>();
        CHK(!t3->deserialise(blob), "serial: a blob carrying an invalid state is rejected");
        // version guard
        auto blob2 = traj->serialise(); blob2.version = 99;
        auto t4 = std::make_unique<ContainmentTrajectory>();
        CHK(!t4->deserialise(blob2), "serial: wrong-version blob rejected");
    }

    // ── P8. rotate step config (the <=1 param rides the pending row) ──────────────────────────
    {
        ContainmentGraph g;
        g.addParent(0, 2); g.addParent(2, 5);
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        p.rotateStep = 2;
        p.latch(CP::V_ROTATE);
        p.applyAll(g, 6, *traj);
        ContainmentGraph oracle;
        oracle.addParent(0, 2); oracle.addParent(2, 5);
        rotateRoles(oracle, 6, 2);
        CHK(sameState(g, oracle), "rotate-step: config step 2 honoured at the boundary");
        // step 0 cannot select nothing (the clampStep contract: 0 -> 1)
        const ContainmentGraph beforeClamp = g;
        CP q; auto t2 = std::make_unique<ContainmentTrajectory>();
        q.rotateStep = 0;
        q.latch(CP::V_ROTATE);
        q.applyAll(g, 6, *t2);
        ContainmentGraph oracle2 = beforeClamp;
        rotateRoles(oracle2, 6, 1);
        CHK(sameState(g, oracle2), "rotate-step: step 0 clamps to 1 (cannot select nothing)");
        CHK(g.isValid() && roundTripOk(g), "rotate-step: results valid + loadable");
    }

    // ── P9. batch closure march (the composition is an endomorphism on DAGs) ──────────────────
    {
        // The verbs are individually closure-proven (test_containment_transforms). This march
        // pins the BATCH: any number of verbs, any stamp order, any active count, EXPAND mixed
        // in — after EVERY boundary commit the graph is still an acyclic, self-bit-free,
        // byte-identically-loadable DAG. 400 seeds x up to 8 boundary rounds.
        int commits = 0;
        bool allValid = true, allRoundTrip = true, holdsRespected = true;
        int maxRing = 0, expandNoOps = 0;
        for (int seed = 0; seed < 400; ++seed) {
            redDot::PhiloxRng rng((uint64_t)seed * 0x9E3779B97F4A7C15ull + 0x1234ull);
            ContainmentGraph g;
            buildRandomDag(g, rng, (int)(rng() % 60u));
            if (!g.isValid()) { allValid = false; break; }   // generator sanity
            CP p; auto traj = std::make_unique<ContainmentTrajectory>();
            const int rounds = 1 + (int)(rng() % 8u);
            for (int r = 0; r < rounds; ++r) {
                // arm a random batch (1..3 latches; a repeated verb re-arms its row — the
                // re-arm-keeps-stamp rule — so the DISTINCT armed count is the commit target)
                const int latches = 1 + (int)(rng() % 3u);
                for (int b = 0; b < latches; ++b)
                    p.latch((int)(rng() % (unsigned)CP::N_VERBS));
                int batch = 0;
                for (int r = 0; r < CP::N_VERBS; ++r) batch += p.armed[r] ? 1 : 0;
                const int active = 1 + (int)(rng() % 16u);
                const int applied = p.applyAll(g, active, *traj);
                ++commits;
                if (applied != batch) holdsRespected = false;   // whole batch commits (hold respected)
                if (applied == 0) ++expandNoOps;   // an all-EXPAND batch at ring start no-ops
                if (!(g.isValid() && noSelfBits(g))) allValid = false;
                if (!roundTripOk(g)) allRoundTrip = false;
                if (traj->size() > maxRing) maxRing = traj->size();
                if (traj->size() > ContainmentTrajectory::DEPTH) allValid = false;
            }
        }
        CHK(holdsRespected, "march: whole batches commit at their boundary (hold respected)");
        CHK(allValid, "march: 400 seeds x mixed batches — DAG invariants hold at EVERY boundary");
        CHK(allRoundTrip, "march: every post-commit state is a byte-identical loadable save");
        CHK(commits > 1000, "march: coverage volume (boundary commit count)");
        (void)expandNoOps; (void)maxRing;   // diagnostics, not gates
    }

    // ── P10. the RANDOM row (row 7; IDEAS_PARKED.md 2026-10-27) ────────────────────────────
    // The CA dice model on containment's OWN stream: the counter is a signed addressable PAGE
    // index; the dispatch moves the pointer in the armed direction FIRST, then applies the
    // page — so OUTRIGHT's graph is always f(counter) and the reverse-draw IS the exact state
    // restore (the stateless model's reverse-as-undo), while SCATTER re-applies the previous
    // page to the CURRENT graph (the CA generative scrub — draw-reversible, NOT state-
    // restorable; the trajectory + undo bracket own the state side, requirements 3 and 4).
    {
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        ContainmentGraph g;
        redDot::PhiloxRng rr;                                  // the draw oracle: the SAME fixed domain
        rr.setKey(redDot::philoxMakeKey(RANDOM_STREAM_DOMAIN));

        // forward fire twice (OUTRIGHT, the default mode/density)
        p.latch(CP::V_RANDOM);
        CHK(!p.reverseArmed, "random: a plain latch arms FORWARD (no reverse request)");
        CHK(p.applyAll(g, 16, *traj) == 1, "random: the row commits at the boundary");
        CHK(p.randomCounter == 1, "random: forward fire advanced the page counter 0 -> 1");
        ContainmentGraph oracle;
        seedForest(oracle, rr, 1ull * RANDOM_PAGE_OUTRIGHT, p.randomDensity);
        CHK(sameState(g, oracle), "random: fire 1 == the outright page-1 forest (the draw oracle)");
        CHK(g.isValid() && noSelfBits(g) && roundTripOk(g), "random: the generated forest is a valid loadable DAG");
        p.latch(CP::V_RANDOM);
        p.applyAll(g, 16, *traj);
        CHK(p.randomCounter == 2, "random: fire 2 advanced to page 2");
        seedForest(oracle, rr, 2ull * RANDOM_PAGE_OUTRIGHT, p.randomDensity);
        CHK(sameState(g, oracle), "random: fire 2 == the outright page-2 forest");
        CHK(traj->size() >= 2, "random: the commits entered the trajectory (requirement 3's data rides every row)");

        // REVERSE-DRAW: the EXACT state restore (the stateless model's reverse-as-undo)
        const ContainmentGraph afterFire2 = g;
        p.latchRandomReverse();
        CHK(p.armed[CP::V_RANDOM] && p.reverseArmed,
            "random: the reverse arm holds the SAME row with the backward direction (one stamp, the lamp)");
        CHK(p.applyAll(g, 16, *traj) == 1, "random: the reverse commits at the boundary");
        CHK(p.randomCounter == 1, "random: reverse stepped the counter 2 -> 1");
        seedForest(oracle, rr, 1ull * RANDOM_PAGE_OUTRIGHT, p.randomDensity);
        CHK(sameState(g, oracle), "random: reverse-draw == the EXACT previous page state (reverse-as-undo)");
        CHK(!sameState(g, afterFire2), "random: the reverse moved the graph (an active gesture)");
        CHK(!p.reverseArmed, "random: the reverse request is consumed at the boundary (the lamp rule)");

        // forward again re-derives page 2 bit-for-bit (deterministic revisit)
        p.latch(CP::V_RANDOM);
        p.applyAll(g, 16, *traj);
        CHK(p.randomCounter == 2 && sameState(g, afterFire2),
            "random: forward re-derivation is exact (f(counter) is f(counter))");

        // direction = the LATEST arm (one pending row, one decision)
        p.latchRandomReverse();
        CHK(p.reverseArmed, "random: reverse arm set");
        p.latch(CP::V_RANDOM);
        CHK(p.armed[CP::V_RANDOM] && !p.reverseArmed,
            "random: the latest arm owns the direction (a forward latch clears the reverse request)");
        p.applyAll(g, 16, *traj);
        CHK(p.randomCounter == 3, "random: the cleared arm fired FORWARD (counter 2 -> 3)");

        // ISLANDS + POOL (path B + the polyphony ruling): the dial + the span ride the page
        // oracle; above-pool rows clear to free singles (the root count shows both effects).
        {
            CP q; auto t3 = std::make_unique<ContainmentTrajectory>();
            ContainmentGraph h;
            q.randomIslands = 4;
            q.randomDensity = 5;
            q.latch(CP::V_RANDOM);
            q.applyAll(h, 10, *t3);                            // polyphony 10 at draw
            CHK(h.isValid() && noSelfBits(h) && roundTripOk(h),
                "random: the islands draw is a valid loadable DAG");
            ContainmentGraph islOracle;
            seedForest(islOracle, rr, 1ull * RANDOM_PAGE_OUTRIGHT, q.randomDensity, q.randomIslands, 10);
            CHK(sameState(h, islOracle),
                "random: the islands dial + the pool span ride the page oracle (path B + polyphony)");
            int roots = 0;
            for (int v = 0; v < VOICES; ++v) if (h.isRoot(v)) ++roots;
            CHK(roots == 4 + 6,
                "random: 4 drawn islands + 6 cleared above-pool singles = the root count");
        }

        // SCATTER mode: fire = the page driven over the CURRENT graph; reverse = the previous
        // page on the CURRENT graph — the CA scrub, NOT the state restore.
        {
            CP q; auto t2 = std::make_unique<ContainmentTrajectory>();
            ContainmentGraph h;
            CHK(h.addParent(0, 1) && h.addParent(2, 5) && h.addParent(9, 10),
                "scatter: setup graph accepted (the state scatter builds ON)");
            q.randomMode = 1;                                  // the context-menu pick
            q.randomDensity = 8;
            const ContainmentGraph pre = h;
            q.latch(CP::V_RANDOM);
            q.applyAll(h, 16, *t2);
            CHK(q.randomCounter == 1 && h.isValid() && noSelfBits(h) && roundTripOk(h),
                "scatter: the pass is a valid checked-edit drive (counter advanced)");
            ContainmentGraph fireOracle = pre;
            scatterEdges(fireOracle, rr, 1ull * RANDOM_PAGE_SCATTER, 8);
            CHK(sameState(h, fireOracle), "scatter: fire == the page driven over the current graph (the oracle)");
            CHK(!sameState(h, pre), "scatter: the pass moved the graph (density 8 on a sparse start)");
            const ContainmentGraph post = h;
            q.latchRandomReverse();
            q.applyAll(h, 16, *t2);
            CHK(q.randomCounter == 0, "scatter: reverse stepped back to page 0");
            ContainmentGraph scrubOracle = post;
            scatterEdges(scrubOracle, rr, 0ull * RANDOM_PAGE_SCATTER, 8);
            CHK(sameState(h, scrubOracle),
                "scatter: reverse == the previous page on the CURRENT graph (the scrub oracle — NOT a restore)");
            CHK(!sameState(h, pre),
                "scatter: draw-reversible != state-restorable (the state side is the trajectory's job)");
            CHK(h.isValid() && roundTripOk(h), "scatter: the scrubbed state is a valid loadable DAG");
        }
    }

    // ── P11. the TRUE-REVERSE queue (the global-reverse consumer; the CA engine shape) ──────
    // The queue is NOT a verb row: no stamp, anyArmed() unaffected, applyAll does NOT consume it
    // (the consume lives in ContainmentModuleCore::applyAtBoundary, BEFORE the verb undo bracket
    // — the modulation-class rule). Here: the flag mechanics + the separation from the verb rows.
    {
        CP p; auto traj = std::make_unique<ContainmentTrajectory>();
        ContainmentGraph g;

        CHK(!p.trueRevRequested, "truerev: fresh pending — the queue is empty");
        p.latchTrueReverse();
        CHK(p.trueRevRequested, "truerev: latchTrueReverse arms the queue");
        p.latchTrueReverse();
        CHK(p.trueRevRequested, "truerev: idempotent while lit (the CA rule — one queued request)");
        CHK(!p.anyArmed(), "truerev: the queue is NOT a verb row (anyArmed unaffected)");
        CHK(p.counter == 0, "truerev: arming the queue consumed NO stamp");

        // applyAll is the WRONG consumer: the queue survives a verb-path boundary untouched.
        CHK(p.applyAll(g, 16, *traj) == 0, "truerev: applyAll applies nothing (no verb row armed)");
        CHK(p.trueRevRequested, "truerev: applyAll does NOT consume the queue (the core owns the consume)");

        // Separate queues: verb latches (including the RANDOM reverse arm) leave the queue alone.
        p.latch(CP::V_INVERT);
        CHK(p.trueRevRequested, "truerev: a verb latch leaves the queue armed (separate queues)");
        p.latchRandomReverse();
        CHK(p.trueRevRequested, "truerev: the RANDOM reverse arm leaves the queue armed (separate queues)");
        CHK(p.armed[CP::V_RANDOM] && p.reverseArmed, "truerev: (sanity) the RANDOM row is armed backward");
        p.applyAll(g, 16, *traj);   // drains the verb rows; the queue rides through
        CHK(!p.anyArmed(), "truerev: (sanity) the verb rows drained");
        CHK(p.trueRevRequested, "truerev: the queue rides through a verb-only boundary (the core consumes it)");

        // Reset clears the queue (a save/reset never carries a request — runtime-only).
        CP r;
        r.latchTrueReverse();
        CHK(r.trueRevRequested, "truerev: (reset setup) queue armed");
        r = CP{};
        CHK(!r.trueRevRequested, "truerev: a fresh pending clears the queue (runtime-only, the reverseArmed rule)");
    }

    std::printf("\n%d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
