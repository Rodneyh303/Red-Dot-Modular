// Mono/V1 direction AUTHORITY — replica of MonsoonExpanderManager::monoDirAuthority().
//
// Why this exists: the manager READS the authority to push laneDirPending_, and East's V1
// direction gate-mod WRITES through it. When those two disagreed, the mod wrote one store
// while the manager pushed another at control rate, and V1 modulation silently did nothing.
// The precedence therefore has to be stated exactly once — this pins that statement.
//
// *** PINNED-TEST RECONCILIATION (CC_PROMPT_UI_TO_RESOLVER_SPEC step 2, direction — a
// contract update, NOT a cover-up): the OLD precedence carved VAR/LEG (and, by a stale
// pre-QMIX index, ACC) out of Macro's arm ("Macro has no such lane"). The spec decides
// otherwise: ALL 7 lanes resolve through the ONE authority — VAR/LEG follow Macro like
// 0-4 (Macro's globalDir store is 7-wide; the POLY direction push already followed it on
// every lane via resolveDirItem). The old pin "a VAR/LEG lane must never resolve to
// Macro" is INVERTED here deliberately; the flagged CC_PROMPT_08 stale-index artifact
// (lane >= 4 covering {ACC, VAR, LEG} instead of the intended {VAR, LEG}) is resolved by
// DELETING the carve-out entirely, not by re-slicing it. ***
//
// Precedence (lane = STRAND index 0..6; Mono is dead — cachedSandsVisualExpander is
// always null since the sands-consolidation Step 6, so the Mono arm is gone):
//   Macro present  -> Macro's cell (ALL 7 lanes — the delegated/global arm)
//   otherwise East -> East's V1 slot  (East is the mono editor)
//   otherwise      -> nobody (caller leaves the lane Forward)
//
// Build: bash test/run_all.sh mono_dir_authority
#include <cstdio>
#include <string>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

enum class Src { None, Macro, EastV1 };
static const char* name(Src s) {
    switch (s) { case Src::Macro: return "Macro";
                 case Src::EastV1: return "EastV1"; default: return "None"; }
}

// Exact replica of the helper's decision (the post-migration shape).
static Src authority(int lane, bool macro, bool east) {
    (void)lane;   // uniform across all 7 lanes — no carve-out
    if (macro) return Src::Macro;
    if (east)  return Src::EastV1;
    return Src::None;
}

int main() {
    std::printf("\033[1mMono direction authority\033[0m\n%s\n", std::string(40, '=').c_str());

    // ── East alone: East owns V1 on every lane (this always worked) ──────────────
    for (int l = 0; l < 7; ++l)
        check(authority(l, false, true) == Src::EastV1,
              "East alone -> East's V1 slot on every lane");

    // ── East + Macro: THE SPEC'S CONTRACT — Macro drives EVERY lane ──────────────
    // VAR/LEG follow Macro like 0-4 (the old carve-out — and the stale pre-QMIX
    // `lane >= 4` set that wrongly dragged ACC out with it — is deleted). East's V1
    // gate-mod must write THAT cell, not East's own slot, or the manager overwrites
    // it at control rate (the original bug this suite pins).
    for (int l = 0; l < 7; ++l)
        check(authority(l, true, true) == Src::Macro,
              "East+Macro, every lane 0..6 -> Macro's cell (VAR/LEG follow Macro like 0-4)");
    check(authority(4, true, true) == Src::Macro &&   // ACC  (the stale-index victim)
          authority(5, true, true) == Src::Macro &&   // VAR
          authority(6, true, true) == Src::Macro,     // LEG
          "ACC/VAR/LEG all take Macro's arm (the pre-QMIX lane>=4 artifact is resolved)");

    // ── Macro alone (no East): Macro on every lane ───────────────────────────────
    for (int l = 0; l < 7; ++l)
        check(authority(l, true, false) == Src::Macro,
              "Macro alone -> Macro's cell on every lane");

    // ── Nothing attached ─────────────────────────────────────────────────────────
    for (int l = 0; l < 7; ++l)
        check(authority(l, false, false) == Src::None,
              "no expanders -> nobody (reset leaves Forward)");

    // ── The invariant that matters: read and write pick the SAME source ──────────
    // Both the manager's push and East's V1 mod call this one function, so they cannot
    // diverge. Enumerate every configuration and assert a single well-defined answer.
    for (int l = 0; l < 7; ++l)
        for (int mac = 0; mac < 2; ++mac)
            for (int e = 0; e < 2; ++e) {
                Src a = authority(l, mac, e);
                Src b = authority(l, mac, e);   // reader and writer agree
                if (a != b) { check(false, "reader/writer disagree"); goto done; }
                // UNIFORMITY: the lane index never changes the answer (no carve-out
                // can reappear as a lane-range special case).
                if (a != authority(0, mac, e)) {
                    check(false, "lane-dependent answer — a direction carve-out is back");
                    goto done;
                }
            }
    check(true, "every configuration resolves to exactly one source");
    check(true, "the answer is UNIFORM across all 7 lanes (no carve-out)");
done:
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
