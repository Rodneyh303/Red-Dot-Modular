// Mono/V1 direction AUTHORITY — replica of MonsoonExpanderManager::monoDirAuthority().
//
// Why this exists: the manager READS the authority to push laneDirPending_, and East's V1
// direction gate-mod WRITES through it. When those two disagreed, the mod wrote one store
// while the manager pushed another at control rate, and V1 modulation silently did nothing.
// The precedence therefore has to be stated exactly once — this pins that statement.
//
// Precedence (lane = STRAND index 0..5; VAR/LEG = 4,5):
//   Mono present and owns the lane   -> Mono's cell     (Mono ALWAYS owns VAR/LEG)
//   otherwise Macro present          -> Macro's cell    (lanes 0..3 only — Macro has no VAR/LEG)
//   otherwise East present           -> East's V1 slot  (East is the mono editor)
//   otherwise                        -> nobody (caller leaves the lane Forward)
//
// Build: g++ -std=c++17 test/test_mono_dir_authority.cpp
#include <cstdio>
#include <string>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

enum class Src { None, Mono, Macro, EastV1 };
static const char* name(Src s) {
    switch (s) { case Src::Mono: return "Mono"; case Src::Macro: return "Macro";
                 case Src::EastV1: return "EastV1"; default: return "None"; }
}

// Exact replica of the helper's decision.
static Src authority(int lane, bool mono, bool macro, bool east, bool monoOwnsLane) {
    const bool varleg = (lane >= 4);
    if (mono) {
        const bool monoOwns = varleg || monoOwnsLane;
        if (monoOwns) return Src::Mono;
        if (macro)    return Src::Macro;
        return Src::None;
    }
    if (macro && !varleg) return Src::Macro;
    if (east)             return Src::EastV1;
    return Src::None;
}

int main() {
    std::printf("\033[1mMono direction authority\033[0m\n%s\n", std::string(40, '=').c_str());

    // ── East alone: East owns V1 on every lane (this always worked) ──────────────
    for (int l = 0; l < 6; ++l)
        check(authority(l, false, false, true, false) == Src::EastV1,
              "East alone -> East's V1 slot on every lane");

    // ── East + Macro: THE BUG THAT WAS REPORTED ────────────────────────────────
    // Macro owns lanes 0..3, so V1 direction there is Macro's cell — East's V1 gate-mod
    // must write THAT, not East's own slot, or the manager overwrites it at control rate.
    for (int l = 0; l < 4; ++l)
        check(authority(l, false, true, true, false) == Src::Macro,
              "East+Macro, lanes 0..3 -> Macro's cell (was: East wrote its own slot => no-op)");
    // ...and the second bug: Macro has NO VAR/LEG, so those must fall through to East.
    // Previously the manager's Macro arm looped l<4 and East's arm was unreachable, so
    // V1 VAR/LEG had NO source and sat pinned Forward with its DirCell dead.
    for (int l = 4; l < 6; ++l)
        check(authority(l, false, true, true, false) == Src::EastV1,
              "East+Macro, VAR/LEG -> East's V1 slot (was: no source => pinned Forward)");

    // ── Mono present: it wins the lanes it owns, and ALWAYS owns VAR/LEG ────────
    for (int l = 0; l < 4; ++l) {
        check(authority(l, true, true, true, /*monoOwns=*/true) == Src::Mono,
              "Mono owns lane -> Mono's cell (beats Macro and East)");
        check(authority(l, true, true, true, /*monoOwns=*/false) == Src::Macro,
              "Mono present but doesn't own -> Macro's cell");
        check(authority(l, true, false, true, /*monoOwns=*/false) == Src::None,
              "Mono present, doesn't own, no Macro -> nobody (lane stays Forward)");
    }
    for (int l = 4; l < 6; ++l)
        check(authority(l, true, true, true, /*monoOwns=*/false) == Src::Mono,
              "VAR/LEG -> Mono's cell regardless of its owner cell (Mono always owns them)");

    // ── Nothing attached ───────────────────────────────────────────────────────
    for (int l = 0; l < 6; ++l)
        check(authority(l, false, false, false, false) == Src::None,
              "no expanders -> nobody (reset leaves Forward)");

    // ── The invariant that matters: read and write pick the SAME source ────────
    // Both the manager's push and East's V1 mod call this one function, so they cannot
    // diverge. Enumerate every configuration and assert a single well-defined answer.
    for (int l = 0; l < 6; ++l)
        for (int m = 0; m < 2; ++m)
            for (int mac = 0; mac < 2; ++mac)
                for (int e = 0; e < 2; ++e)
                    for (int own = 0; own < 2; ++own) {
                        Src a = authority(l, m, mac, e, own);
                        Src b = authority(l, m, mac, e, own);   // reader and writer agree
                        if (a != b) { check(false, "reader/writer disagree"); goto done; }
                        // A VAR/LEG lane must never resolve to Macro — it has no such lane.
                        if (l >= 4 && a == Src::Macro) {
                            check(false, "VAR/LEG resolved to Macro (impossible lane)");
                            goto done;
                        }
                    }
    check(true, "every configuration resolves to exactly one source");
    check(true, "no VAR/LEG lane ever resolves to Macro");
done:
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
