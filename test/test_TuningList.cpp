// test_TuningList.cpp — Shophouse Micro's TuningList (boundary-quantised tuning-slot list).
//
// Mirrors ScaleList's contract (pending→active commits only at the boundary) plus the 12/24-mode +
// no-mixed-N-slots invariant (SHOPHOUSE_MICRO_SPEC §66). A slot now carries cents + ENABLED (the scale
// mask), NOT weight (ENABLED_MASK_BUILD_BRIEF v2). Pure C++/no Rack (TuningList.hpp only pulls
// TuningTable.hpp), builds standalone. Registered in run_all.sh as "test_TuningList|".
//
// Build (standalone):
//   g++ -std=c++17 -Isrc test/test_TuningList.cpp -o /tmp/ttl && /tmp/ttl

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "dsp/TuningList.hpp"

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)
#define EXPECT_EQ(a,b) do { if((a)!=(b)) { std::ostringstream _s; \
    _s << "EXPECT_EQ(" #a "," #b ") : " << (long long)(a) << " != " << (long long)(b); \
    throw std::runtime_error(_s.str()); } } while(0)

static int g_pass = 0, g_fail = 0;

// Build a simple ascending cents payload of degree count n.
static std::vector<float> edoCents(int n) {
    std::vector<float> c(n);
    for (int i = 0; i < n; ++i) c[i] = (float)i * (1200.f / (float)n);
    return c;
}
// enabled mask (all in-scale). std::vector<bool> is a bitfield (no .data()), so use a bool buffer.
struct BoolMask {
    std::vector<unsigned char> v;
    explicit BoolMask(int n, bool on = true) : v((size_t)n, on ? 1u : 0u) {}
    const bool* data() const { return reinterpret_cast<const bool*>(v.data()); }
    void set(int i, bool on) { v[(size_t)i] = on ? 1u : 0u; }
};

int main() {
    SUITE("construction + mode");
    TEST("default 4 slots, 12 degrees", {
        TuningList L;
        EXPECT_EQ(L.size(), 4);
        EXPECT_EQ(L.degrees(), 12);
        EXPECT(!L.anyLoaded());
    });
    TEST("24-mode ctor with 2 fronts", {
        TuningList L(2, 24);
        EXPECT_EQ(L.size(), 2);
        EXPECT_EQ(L.degrees(), 24);
    });
    TEST("setDegrees allowed while empty, blocked once loaded", {
        TuningList L(4, 12);
        EXPECT(L.setDegrees(24));               // empty → allowed
        EXPECT_EQ(L.degrees(), 24);
        BoolMask m(24);
        EXPECT(L.loadSlot(0, 24, edoCents(24).data(), m.data(), "maqam"));
        EXPECT(!L.setDegrees(12));              // loaded → blocked
        EXPECT_EQ(L.degrees(), 24);
    });

    SUITE("slot load — per-slot n, capacity-bounded (ROUND 10 full model)");
    TEST("empty list adopts a LARGER mode from the first load (up to that n)", {
        TuningList L(4, 12);
        BoolMask m(24);
        EXPECT(L.loadSlot(0, 24, edoCents(24).data(), m.data(), "A"));
        EXPECT_EQ(L.degrees(), 24);             // capacity bumped to fit the first (empty-adopt)
        EXPECT(L.slot(0).loaded);
        EXPECT_EQ(L.slot(0).n, 24);             // slot carries its OWN size
    });
    TEST("slots may DIFFER in n within the capacity (no mixed-N rejection anymore)", {
        TuningList L(2, 24);                    // 24-capacity (Duo-paired)
        BoolMask m24(24), m12(12), m17(17);
        EXPECT(L.loadSlot(0, 24, edoCents(24).data(), m24.data(), "maqam24"));
        EXPECT(L.loadSlot(1, 12, edoCents(12).data(), m12.data(), "just12"));  // smaller n → allowed
        EXPECT(L.slot(0).loaded); EXPECT(L.slot(1).loaded);
        EXPECT_EQ(L.slot(0).n, 24);
        EXPECT_EQ(L.slot(1).n, 12);             // per-slot sizes coexist
    });
    TEST("a load OVER capacity is rejected", {
        TuningList L(4, 12);                    // 12-capacity (Colonnades-paired), a slot already loaded
        BoolMask m12(12), m17(17);
        EXPECT(L.loadSlot(0, 12, edoCents(12).data(), m12.data(), "x"));
        EXPECT(!L.loadSlot(1, 17, edoCents(17).data(), m17.data(), "too-big"));  // 17 > cap 12 → reject
        EXPECT(!L.slot(1).loaded);
        EXPECT_EQ(L.degrees(), 12);
    });
    TEST("varied n loads succeed across slots", {
        TuningList L(4, 24);
        BoolMask m(24);
        for (int s = 0; s < 4; ++s)
            EXPECT(L.loadSlot(s, 12 + s, edoCents(12 + s).data(), m.data(), "s" + std::to_string(s)));
        EXPECT(L.slot(3).loaded);
        EXPECT_EQ(L.slot(3).n, 15);
    });
    TEST("clear() empties all + re-enables mode change", {
        TuningList L(4, 12);
        BoolMask m(12);
        L.loadSlot(0, 12, edoCents(12).data(), m.data(), "x");
        L.clear();
        EXPECT(!L.anyLoaded());
        EXPECT(L.setDegrees(24));
    });
    TEST("cents beyond n zeroed; enabled mask preserved within n", {
        TuningList L(2, 24);
        auto c = edoCents(24);
        BoolMask m(24); m.set(23, false);       // last degree masked out
        L.loadSlot(0, 24, c.data(), m.data(), "e");
        EXPECT(L.slot(0).cents[12] == c[12]);
        EXPECT(L.slot(0).enabled[0] == true);
        EXPECT(L.slot(0).enabled[23] == false);
    });

    SUITE("pending → active, boundary commit (ScaleList parity)");
    TEST("setPending does NOT change active until commit", {
        TuningList L(4, 12);
        BoolMask m(12);
        for (int s = 0; s < 4; ++s) L.loadSlot(s, 12, edoCents(12).data(), m.data(), "s");
        L.setPending(2);
        EXPECT_EQ(L.active(), 0);
        EXPECT_EQ(L.pending(), 2);
        L.commitAtBoundary();
        EXPECT_EQ(L.active(), 2);
    });
    TEST("commit returns false when active==pending", {
        TuningList L;
        EXPECT(!L.commitAtBoundary());
    });
    TEST("commit returns CONTENT-changed when cents differ", {
        TuningList L(4, 12);
        BoolMask m(12);
        L.loadSlot(0, 12, edoCents(12).data(), m.data(), "a");
        auto c = edoCents(12); c[1] = 150.f;    // different cents on degree 1
        L.loadSlot(1, 12, c.data(), m.data(), "b");
        L.setPending(1);
        EXPECT(L.commitAtBoundary());           // content differs → true
    });
    TEST("commit returns CONTENT-changed when the MASK differs", {
        TuningList L(4, 12);
        auto c = edoCents(12);
        BoolMask mA(12);
        BoolMask mB(12); mB.set(5, false);      // same cents, different enabled mask
        L.loadSlot(0, 12, c.data(), mA.data(), "a");
        L.loadSlot(1, 12, c.data(), mB.data(), "b");
        L.setPending(1);
        EXPECT(L.commitAtBoundary());           // enabled differs → re-publish
    });
    TEST("commit returns false when the two slots hold IDENTICAL content", {
        TuningList L(4, 12);
        auto c = edoCents(12);
        BoolMask m(12);
        L.loadSlot(0, 12, c.data(), m.data(), "same0");
        L.loadSlot(1, 12, c.data(), m.data(), "same1");   // name differs, content same
        L.setPending(1);
        EXPECT(!L.commitAtBoundary());          // same cents+enabled → no re-publish needed
        EXPECT_EQ(L.active(), 1);               // index still moved
    });
    TEST("stepPending wraps within the front count", {
        TuningList L(4, 12);
        L.setPending(3); L.stepPending(1);
        EXPECT_EQ(L.pending(), 0);              // 3 → wrap → 0
        L.stepPending(-1);
        EXPECT_EQ(L.pending(), 3);
    });

    std::cout << "\n-----\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
