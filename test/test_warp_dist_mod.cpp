// test_warp_dist_mod.cpp — the distribution send-bus fold (the spec's decided
// design: "Macro distribution mods flow through per-voice sends; East blends
// global+per-voice"). Compile: see test/run_all.sh (header-only, rack-free —
// the fold lives in WarpF.hpp/WarpF's resolver wiring, both Rack-free).
//
// Under test: WarpBank's delta planes + the distDeltasActive gate, folded inside
// resolveWarpCell (SandsModifierResolver.hpp). Covered:
//   1. NEUTRALITY: flag-off banks are BIT-IDENTICAL to a clean bank even with
//      garbage in the planes (the U2-oracle/bridge-pin contract); flag-on with
//      all-zero planes is bit-identical too (the per-param guards skip).
//   2. GLOBAL (Macro-sent) deltas reach the voice's own cell — stage params add
//      with the fold's association, w clamps to [0,1] at the cell.
//   3. OWN (East-local) deltas reach ONLY the addressed voice's cell.
//   4. THE DELEGATED BRANCH: own ignored (bit-identical to no-own), global folded
//      into Macro's default — the resolver's standing law, mod edition.
//   5. P1/P2: the member re-mints through the shared window table (clamped);
//      identity/drawn are the spec's harmless no-op.
//   6. TILT EVAL CLAMP: an overshooting tilt delta composes exactly like the
//      eval-side clampTilt value (engine and display share the clamp).
//   7. SEND IDENTITY: the bank's distSend defaults (1.0) leave the fold
//      BIT-IDENTICAL to the pre-subpanel fold — including the nullptr-send form.
//   8. SEND SCALING: the Macro send subpanel's per-voice amounts scale the glob
//      term (0.5 halves it, 0 mutes it, the OWN term is never scaled, the scaling
//      is per-voice, and the DELEGATED branch scales too).
//   9. SEND PERSISTENCE: the DistSend key round-trips; a bank without the key
//      loads at the identity default.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Distribution.hpp"
#include "MonotoneCurve.hpp"
#include "WarpF.hpp"
#include "SandsModifierResolver.hpp"

using dotModular::WarpBank;
using dotModular::WarpF;
using dotModular::PowerWarp;
using dotModular::TriangularWarp;
using dotModular::IdentityWarp;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    if (ok) { ++g_pass; return; }
    std::printf("  FAIL: %s\n", what);
    ++g_fail;
}
static void checkBits(float got, float want, const char* what) {
    if (std::memcmp(&got, &want, sizeof(float)) == 0) { ++g_pass; return; }
    std::printf("  FAIL: %s (got %.9g, want %.9g)\n", what, got, want);
    ++g_fail;
}

int main() {
    std::printf("== WarpDistMod (the distribution send-bus fold) ==\n");
    constexpr int V0 = 3, V1 = 7, L = 2;

    // ── a reference bank with one audible cell: voice V0/L is Triangular{0.5} at
    //    w=0.6, tilt +0.1; Macro's global default for L is Power{2.0} at w=0.5.
    WarpBank ref;
    ref.warpF[WarpBank::index(V0, L)]   = TriangularWarp{0.5};
    ref.warpW[WarpBank::index(V0, L)]   = 0.6f;
    ref.warpTilt[WarpBank::index(V0, L)] = 0.1f;
    ref.globalWarpF[L] = PowerWarp{2.0};
    ref.globalWarpW[L] = 0.5f;

    // 1. NEUTRALITY ────────────────────────────────────────────────────────────
    {
        WarpBank garbage = ref;
        for (int l = 0; l < WarpBank::LANES; ++l)
            for (int p = 0; p < WarpBank::DPARS; ++p)
                garbage.distGlobalDelta[l][p] = 0.25f * (float)(p + 1);
        for (int v = 0; v < WarpBank::VOICES; ++v)
            for (int l = 0; l < WarpBank::LANES; ++l)
                for (int p = 0; p < WarpBank::DPARS; ++p)
                    garbage.distOwnDelta[v][l][p] = 0.5f * (float)(p + 1);
        // flag OFF: the planes are ignored WHOLESALE (garbage or not)
        bool same = true;
        for (int v = 0; v < 4 && same; ++v)
            for (int l = 0; l < 3 && same; ++l)
                for (int i = 0; i <= 8 && same; ++i) {
                    const double u = i / 8.0;
                    if (garbage.warpedValue(v, l, u, false) != ref.warpedValue(v, l, u, false)) same = false;
                    if (garbage.warpedValue(v, l, u, true)  != ref.warpedValue(v, l, u, true))  same = false;
                }
        check(same, "flag-off bank with garbage planes is bit-identical (both branches)");
        // flag ON but planes all zero: the per-param guards skip — still bit-identical
        WarpBank zeroed = ref;
        zeroed.distDeltasActive = true;
        same = true;
        for (int i = 0; i <= 8 && same; ++i) {
            const double u = i / 8.0;
            if (zeroed.warpedValue(V0, L, u, false) != ref.warpedValue(V0, L, u, false)) same = false;
            if (zeroed.warpedValue(V0, L, u, true)  != ref.warpedValue(V0, L, u, true))  same = false;
        }
        check(same, "flag-on with all-zero planes is bit-identical (guards skip)");
    }

    // 2. GLOBAL reaches the own cell — stage adds + w clamp ───────────────────
    {
        WarpBank b = ref;
        b.distDeltasActive = true;
        b.distGlobalDelta[L][2] = 0.2f;               // TILT +0.2
        b.distGlobalDelta[L][5] = 0.4f;               // WARP +0.4 (w 0.6 → 1.0)
        const auto cell = dotModular::resolveWarpCell(b, V0, L, false);
        checkBits(cell.tilt, 0.1f + (0.2f + 0.f), "global tilt delta adds (fold association)");
        checkBits(cell.w, 1.0f, "global warp delta clamps to 1 at the cell");
        const WarpBank& cb = b;
        check(cb.warpedValue(V0, L, 0.5, false) != ref.warpedValue(V0, L, 0.5, false),
              "the modded cell audibly differs from the unmodded one");
    }

    // 3. OWN reaches ONLY the addressed voice ──────────────────────────────────
    {
        WarpBank b = ref;
        b.distDeltasActive = true;
        b.distOwnDelta[V0][L][4] = 0.5f;              // MASS +0.5 on V0/L only
        check(b.warpedValue(V0, L, 0.5, false) != ref.warpedValue(V0, L, 0.5, false),
              "own mass delta moves the addressed voice's cell");
        bool v1Untouched = true;
        for (int i = 0; i <= 8; ++i)
            if (b.warpedValue(V1, L, i / 8.0, false) != ref.warpedValue(V1, L, i / 8.0, false))
                v1Untouched = false;
        check(v1Untouched, "a sibling voice's cell is bit-identical (own deltas are per-voice)");
    }

    // 4. THE DELEGATED BRANCH: own ignored, global folded into Macro's default ─
    {
        WarpBank b = ref;
        b.distDeltasActive = true;
        b.distGlobalDelta[L][2] = -0.05f;             // Macro-sent tilt
        b.distOwnDelta[V0][L][2] = 0.4f;              // East-local tilt — must NOT land
        const auto del  = dotModular::resolveWarpCell(b, V0, L, true);
        const auto delR = dotModular::resolveWarpCell(ref, V0, L, true);
        checkBits(del.tilt, ref.globalWarpTilt[L] + (-0.05f + 0.f),
                  "delegated cell folds ONLY the global delta");
        check(del.f.index() == delR.f.index(), "delegated member kind unchanged by the stage delta");
        // the own delta must be invisible on the delegated OUTPUT too
        bool same = true;
        WarpBank noOwn = b; noOwn.distOwnDelta[V0][L][2] = 0.f;
        for (int i = 0; i <= 8; ++i)
            if (b.warpedValue(V0, L, i / 8.0, true) != noOwn.warpedValue(V0, L, i / 8.0, true))
                same = false;
        check(same, "own delta is a no-op on delegated cells (bit-exact)");
    }

    // 5. P1/P2: re-mint through the shared window table; the no-param no-op ────
    {
        WarpBank b = ref;
        b.distDeltasActive = true;
        // own cell is Triangular{0.5}: p1 "mode" window [0,1] — a +10 delta clamps to 1
        b.distOwnDelta[V0][L][0] = 10.f;
        const WarpF eff = b.effectiveF(V0, L, false);
        const auto* tri = std::get_if<TriangularWarp>(&eff);
        check(tri && tri->mode == 1.0, "p1 delta re-mints the member, window-clamped to hi");
        // Macro default is Power{2.0}: p1 "k" window [0.2,5] — a huge negative clamps to lo.
        // The clamp target is the TABLE's float lo promoted to double — the same constant
        // the knob path binds (the 3e-9 float granularity is the window's own).
        b.distGlobalDelta[L][0] = -100.f;
        const WarpF effM = b.effectiveF(V0, L, true);
        const auto* pw = std::get_if<PowerWarp>(&effM);
        check(pw && pw->k == (double)dotModular::warpParamSpec((int)ref.globalWarpF[L].index(), 0).lo,
              "delegated p1 folds into Macro's default member, clamped to the window lo");
        // identity has no params: the spec's unused-param ruling — the harmless no-op
        WarpBank ib = ref;
        ib.distDeltasActive = true;
        ib.warpF[WarpBank::index(V1, L)] = IdentityWarp{};
        const WarpF before = ib.warpF[WarpBank::index(V1, L)];
        ib.distOwnDelta[V1][L][0] = 3.f;
        ib.distOwnDelta[V1][L][1] = -2.f;
        check(ib.effectiveF(V1, L, false).index() == before.index(),
              "identity member ignores p1/p2 deltas (kind untouched)");
    }

    // 6. TILT EVAL CLAMP: overshoot composes exactly like the clamped value ────
    {
        WarpBank b = ref;
        b.distDeltasActive = true;
        b.distGlobalDelta[L][2] = 5.f;                // tilt 0.1 + 5 → eval-clamped to kTiltMax
        WarpBank hand = ref;                          // an out-of-window authored tilt: the SAME
                                                      // eval-side clamp fires on both banks, so
                                                      // both compose at exactly the double kTiltMax
        hand.warpTilt[WarpBank::index(V0, L)] = 2.0f;
        bool same = true;
        for (int i = 0; i <= 8; ++i)
            if (b.warpedValue(V0, L, i / 8.0, false) != hand.warpedValue(V0, L, i / 8.0, false))
                same = false;
        check(same, "overshooting tilt delta composes bit-exact as the eval-side clamp");
    }

    // 7. SEND IDENTITY: the default amounts ARE the pre-subpanel fold ─────────
    {
        WarpBank b = ref;
        b.distDeltasActive = true;
        b.distGlobalDelta[L][2] = 0.2f;
        b.distGlobalDelta[L][4] = 0.3f;
        b.distOwnDelta[V0][L][2] = -0.05f;
        // fresh-bank defaults: distSend all 1.0 (the ctor fill)
        bool idDefault = true;
        for (int v = 0; v < 16; ++v)
            for (int l = 0; l < WarpBank::LANES; ++l)
                for (int p = 0; p < WarpBank::DPARS; ++p)
                    if (b.distSend[v][l][p] != WarpBank::kDistSendDefault) idDefault = false;
        check(idDefault, "every construction path starts at the identity send amounts");
        // the identity sends fold BIT-IDENTICALLY to the pre-subpanel arithmetic
        // (d = glob + own, x·1.0f is exact IEEE) — both branches, hand-computed
        const auto ownCell = dotModular::resolveWarpCell(b, V0, L, false);
        checkBits(ownCell.tilt, 0.1f + (0.2f + -0.05f),
                  "identity sends == the pre-subpanel own-branch fold");
        const auto delCell = dotModular::resolveWarpCell(b, V0, L, true);
        checkBits(delCell.tilt, ref.globalWarpTilt[L] + (0.2f + 0.f),
                  "identity sends == the pre-subpanel delegated-branch fold");
        // the direct fold: nullptr send == a pointer at 1.0s (bit-exact)
        const auto cellNull = dotModular::resolveWarpCell(b, V0, L, false);
        const float ones[WarpBank::DPARS] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
        WarpBank b2 = b;
        for (int p = 0; p < WarpBank::DPARS; ++p) b2.distSend[V0][L][p] = ones[p];
        const auto cellOnes = dotModular::resolveWarpCell(b2, V0, L, false);
        checkBits(cellNull.tilt, cellOnes.tilt, "nullptr send row == all-ones row (tilt)");
        checkBits(cellNull.atomMass, cellOnes.atomMass, "nullptr send row == all-ones row (mass)");
    }

    // 8. SEND SCALING: the subpanel's amounts scale the glob term, per voice ──
    {
        WarpBank b = ref;
        b.distDeltasActive = true;
        b.distGlobalDelta[L][2] = 0.2f;               // the Macro-sent tilt
        b.distOwnDelta[V0][L][4] = 0.1f;              // an East-local mass — NEVER scaled
        // half send: the global tilt arrives as glob*0.5, the own term untouched
        b.distSend[V0][L][2] = 0.5f;
        const auto half = dotModular::resolveWarpCell(b, V0, L, false);
        const float wantTilt = 0.1f + (0.2f * 0.5f + 0.f) + 0.f;
        checkBits(half.tilt, wantTilt, "half send scales ONLY the glob term (own untouched)");
        // zero send on the tilt axis: the global tilt vanishes for THIS voice…
        b.distSend[V0][L][2] = 0.f;
        const auto muted = dotModular::resolveWarpCell(b, V0, L, false);
        checkBits(muted.tilt, 0.1f, "zero send mutes the global delta for that voice");
        // …and ONLY for that voice (the sibling still takes the full glob)
        const auto sib = dotModular::resolveWarpCell(b, V1, L, false);
        checkBits(sib.tilt, 0.f + (0.2f * 1.0f + 0.f), "the sibling voice's send is unaffected");
        // the delegated branch scales too (the voice's amount shapes what its
        // cell receives even when the lane mirrors Macro's default)
        WarpBank d = ref;
        d.distDeltasActive = true;
        d.distGlobalDelta[L][2] = 0.2f;
        d.distSend[V0][L][2] = 0.25f;
        const auto del = dotModular::resolveWarpCell(d, V0, L, true);
        checkBits(del.tilt, ref.globalWarpTilt[L] + (0.2f * 0.25f + 0.f),
                  "the delegated branch folds the SCALED global delta");
    }

    // 9. SEND PERSISTENCE: the DistSend key round-trips; missing key ⟹ identity
    {
        WarpBank src = ref;
        src.distSend[V0][L][3] = 0.25f;
        src.distSend[V1][L][5] = 0.f;
        json_t* root = json_object();
        WarpBank::serialize(src, root, "editor");
        WarpBank dst;
        WarpBank::parse(root, "editor", dst);
        json_decref(root);
        bool rt = dst.distSend[V0][L][3] == 0.25f && dst.distSend[V1][L][5] == 0.f
               && dst.distSend[V0][L][2] == WarpBank::kDistSendDefault;
        check(rt, "the DistSend key round-trips (authored amounts survive)");
        json_t* bare = json_object();          // a pre-subpanel patch: no key at all
        WarpBank fresh;
        WarpBank::parse(bare, "editor", fresh);
        json_decref(bare);
        bool idAll = true;
        for (int v = 0; v < WarpBank::VOICES; ++v)
            for (int l = 0; l < WarpBank::LANES; ++l)
                for (int p = 0; p < WarpBank::DPARS; ++p)
                    if (fresh.distSend[v][l][p] != WarpBank::kDistSendDefault) idAll = false;
        check(idAll, "a patch without the DistSend key loads at the identity default");
    }

    if (g_fail == 0) { std::printf("ALL PASS (%d checks)\n", g_pass); return 0; }
    std::printf("%d FAILURE(S) of %d checks\n", g_fail, g_fail + g_pass);
    return 1;
}
