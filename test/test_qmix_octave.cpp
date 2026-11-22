// test_qmix_octave.cpp — coverage for QmixOctave (QmixOctave.hpp): the octave threshold Bank
// codec + the pure three-way resolver. No dedicated test before (coverage-gap fill 2026-11-22).
// The resolver's byte-identity DEFAULT and the persistence contract (absent/short/wrong-typed ->
// defaults) are the properties tested — NOT the in-flux quantiser semantics. jansson via the
// test/jansson.h shim (-Itest), the test_subgate_knobs pattern.
#include <cstdio>
#include <cmath>
#include "dsp/QmixOctave.hpp"

using namespace dotModular::QmixOctave;

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("  FAIL: %s\n", (m)); ++g_fail; } } while (0)

static bool bankEqual(const Bank& a, const Bank& b) {
    if (a.globalTh != b.globalTh || a.followDegree != b.followDegree) return false;
    for (int v = 0; v < VOICES; ++v)
        if (a.ownTh[v] != b.ownTh[v] || a.ownActive[v] != b.ownActive[v]) return false;
    return true;
}

int main() {
    std::printf("test_qmix_octave\n");

    // 1. Defaults: followDegree true, thresholds kDefaultThreshold, nothing claimed.
    {
        Bank b;
        CHECK(b.followDegree == true, "default followDegree");
        CHECK(b.globalTh == kDefaultThreshold, "default globalTh");
        for (int v = 0; v < VOICES; ++v) {
            CHECK(b.ownTh[v] == kDefaultThreshold, "default ownTh");
            CHECK(b.ownActive[v] == false, "default not claimed");
        }
    }

    // 2. resolve precedence + the byte-identity default.
    {
        // default (not claimed, follow-degree): resolve == the degree value EXACTLY, for any degree in [0,1]
        for (float d = 0.0f; d <= 1.0f; d += 0.1f)
            CHECK(resolve(false, 0.3f, true, 0.8f, d) == d, "follow-degree default == degree value exactly");
        // own claimed wins outright over both follow and global
        CHECK(resolve(true, 0.42f, true,  0.8f, 0.1f) == 0.42f, "own claim wins over follow");
        CHECK(resolve(true, 0.42f, false, 0.8f, 0.1f) == 0.42f, "own claim wins over global");
        // not claimed, follow OFF -> the global threshold
        CHECK(resolve(false, 0.42f, false, 0.8f, 0.1f) == 0.8f, "follow off -> global");
        // clamp to [0,1]
        CHECK(resolve(true, 5.0f, true, 0.5f, 0.5f) == 1.0f, "own over-range clamps to 1");
        CHECK(resolve(true, -5.0f, true, 0.5f, 0.5f) == 0.0f, "own under-range clamps to 0");
        CHECK(resolve(false, 0.f, false, 9.0f, 0.5f) == 1.0f, "global over-range clamps to 1");
    }

    // 3. Full round trip: author a non-default bank, serialize, parse, compare.
    {
        Bank in; in.reset();
        in.globalTh = 0.77f; in.followDegree = false;
        for (int v = 0; v < VOICES; ++v) { in.ownTh[v] = 0.01f * (float)v; in.ownActive[v] = (v % 3 == 0); }
        json_t* root = json_object();
        Bank::serialize(in, root, "qmixOct");
        Bank out; Bank::parse(root, "qmixOct", out);
        CHECK(bankEqual(in, out), "serialize->parse round trips exactly");
        json_decref(root);
    }

    // 4. Absent keys -> defaults (old-patch guarantee): empty object parses to a reset bank.
    {
        Bank out; out.globalTh = 0.9f; out.followDegree = false;   // dirty it first
        json_t* empty = json_object();
        Bank::parse(empty, "qmixOct", out);
        Bank def;
        CHECK(bankEqual(out, def), "absent keys -> full defaults");
        json_decref(empty);
    }

    // 5. Null / non-object root -> defaults, never a crash.
    {
        Bank out; out.globalTh = 0.3f;
        Bank::parse(nullptr, "qmixOct", out);
        Bank def;
        CHECK(bankEqual(out, def), "null root -> defaults");
    }

    // 6. Short Own array keeps the default tail; a wrong-typed entry keeps its default cell.
    {
        json_t* root = json_object();
        // Own with only 3 entries, the middle one a string (wrong type)
        json_t* own = json_array();
        json_array_append_new(own, json_real(0.11));
        json_array_append_new(own, json_string("oops"));   // wrong type -> default cell
        json_array_append_new(own, json_real(0.33));
        json_object_set_new(root, "qmixOctOwn", own);
        Bank out; Bank::parse(root, "qmixOct", out);
        CHECK(std::fabs(out.ownTh[0] - 0.11f) < 1e-6f, "own[0] read");
        CHECK(out.ownTh[1] == kDefaultThreshold, "wrong-typed own[1] keeps default");
        CHECK(std::fabs(out.ownTh[2] - 0.33f) < 1e-6f, "own[2] read");
        for (int v = 3; v < VOICES; ++v) CHECK(out.ownTh[v] == kDefaultThreshold, "short array default tail");
        json_decref(root);
    }

    if (g_fail == 0) std::printf("  OK\n");
    else             std::printf("  %d FAILURE(S)\n", g_fail);
    return g_fail ? 1 : 0;
}
