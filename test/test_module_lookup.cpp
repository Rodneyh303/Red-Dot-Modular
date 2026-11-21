// test_module_lookup.cpp — CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START: the module-lookup
// authority's contract suite.
//
//   L1 EPOCH: bumpTopologyEpoch moves the generation; lookupEpochStale fires exactly
//      once per generation (the explicit-cache guard).
//   L2 MEMO / ZERO-WALK: repeated lookups with a static rack do NOT re-walk (the
//      walk COUNTS — 1000 calls, 1 walk). This is the prompt's "walk-when-static ≈ 0"
//      acceptance, made headless.
//   L3 MEMO KEY: a parameterized question (followed-id) resolves per key.
//   L4 THE MULTI-HOP FAN-OUT (the design argument, executable): a topology change 2+
//      hops away that fires NO event on the walker leaves the memo legitimately stale
//      (Rack only dispatches onExpanderChange to the modules whose immediate slot
//      changed — SDK Module.hpp); the epoch bump (ANY module's handler) is what makes
//      the walker re-resolve. In Rack the bump happens synchronously inside the event,
//      before any process()/draw runs, so no consumer ever observes the stale window —
//      this test documents and pins exactly that contract.
//   L5 STRUCTURAL (the grep-gate): raw walk-helper call sites (findMonsoonEitherSide /
//      resolveFollowedT / findPairHubEitherSide / assignPairId / isConnectedAndClaimed /
//      findMonsoon — paren AND template-argument forms) are allowed ONLY in the service +
//      primitive headers, or on lines carrying the LOOKUP-AUTHORITY marker (an
//      onExpanderChange / epoch-guarded resolve). Everything else is a hot-path walk —
//      the anti-pattern this authority kills. FATAL BY DEFAULT since the module-lookup
//      TAIL migration finished (every in-build site converted or marked): a residual
//      FAILS the suite. MODULE_LOOKUP_STRICT=0 is a local debug hatch ONLY (never CI).
//
// Header-only: the core (ui/ModuleLookupCore.hpp) is deliberately lean — no rack, no
// Monsoon — exactly so this rig can drive the REAL epoch + memo (never a copy).
// The fake-node chain is the test_monsoon_discovery pattern.
#include "ModuleLookupCore.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static int s_pass = 0, s_fail = 0;
#define TEST(desc, ...) do { \
    bool _ok = true; std::string _msg; \
    try { __VA_ARGS__; } catch (const std::exception& _e) { _ok = false; _msg = _e.what(); } \
    if (_ok) { ++s_pass; std::cout << "  \033[32m✓\033[0m " << (desc) << "\n"; } \
    else { ++s_fail; std::cout << "  \033[31m✗\033[0m " << (desc); \
           if (!_msg.empty()) std::cout << " — " << _msg; std::cout << "\n"; } \
} while (0)
#define EXPECT(e) do { if (!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while (0)
#define EXPECTF(e, ...) do { if (!(e)) { char _b[256]; std::snprintf(_b, sizeof _b, __VA_ARGS__); \
    throw std::runtime_error(std::string("EXPECT(" #e ") failed: ") + _b); } } while (0)

// ── The fake-node chain (the test_monsoon_discovery pattern) ────────────────────
enum Tag { FOREIGN = 0, HOSTM = 1, SUITE = 2 };
struct Node {
    int tag = FOREIGN;
    struct Side { Node* module = nullptr; };
    Side leftExpander, rightExpander;
};
static void link(std::vector<Node>& v) {
    for (size_t i = 0; i + 1 < v.size(); ++i) {
        v[i].rightExpander.module = &v[i + 1];
        v[i + 1].leftExpander.module = &v[i];
    }
}
// The segment-rule walk shape (hop through SUITE, stop at FOREIGN, HOST is the hit).
static Node* walkHost(Node* n) {
    for (int side = 0; side < 2; ++side) {
        Node* curr = side ? n->leftExpander.module : n->rightExpander.module;
        for (int d = 0; curr && d < 12; ++d) {
            if (curr->tag == HOSTM) return curr;
            if (curr->tag != SUITE) break;
            curr = side ? curr->leftExpander.module : curr->rightExpander.module;
        }
    }
    return nullptr;
}

int main() {
    std::cout << "\033[1mModule-Lookup Authority (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START) Contract Tests\033[0m\n"
              << std::string(70, '=') << "\n";

    TEST("L1 EPOCH: bump moves the generation; lookupEpochStale fires once per generation", {
        redDot::bumpTopologyEpoch();
        const uint64_t before = redDot::topologyEpoch().load(std::memory_order_relaxed);
        uint64_t cache = before;
        EXPECT(!redDot::lookupEpochStale(cache));          // fresh cache → quiet
        redDot::bumpTopologyEpoch();
        EXPECT(redDot::topologyEpoch().load(std::memory_order_relaxed) == before + 1);
        EXPECT(redDot::lookupEpochStale(cache));           // moved → stale, exactly once
        EXPECT(!redDot::lookupEpochStale(cache));
    });

    TEST("L2 MEMO / ZERO-WALK: 1000 lookups on a static rack = exactly 1 walk (the acceptance, counted)", {
        redDot::bumpTopologyEpoch();                        // deterministic memo state
        std::vector<Node> row(4);
        row[0].tag = HOSTM; row[1].tag = SUITE; row[2].tag = SUITE; row[3].tag = SUITE;
        link(row);
        Node* self = &row[3];
        int walks = 0;
        auto countingWalk = [&](Node* n, int) { ++walks; return walkHost(n); };
        Node* first = redDot::lookupMemoizedT(self, 0, countingWalk);
        for (int i = 0; i < 1000; ++i)
            EXPECT(redDot::lookupMemoizedT(self, 0, countingWalk) == first);
        EXPECTF(walks == 1, "%d walks for 1001 static-rack lookups — the memo is re-walking", walks);
        EXPECT(first == &row[0]);
    });

    TEST("L3 MEMO KEY: a parameterized question resolves per key (the followed-id shape; the memo is SINGLE-ENTRY — a key change re-resolves, and alternating keys thrash, which production never does: one module asks one question with one key)", {
        redDot::bumpTopologyEpoch();
        std::vector<Node> row(3);
        row[0].tag = HOSTM; row[1].tag = SUITE; row[2].tag = SUITE;
        link(row);
        int walks = 0;
        auto countingWalk = [&](Node* n, int) { ++walks; return walkHost(n); };
        Node* a = redDot::lookupMemoizedT(&row[2], 7, countingWalk);   // key 7 → walk 1
        Node* a2 = redDot::lookupMemoizedT(&row[2], 7, countingWalk);  // key 7 again → CACHED
        Node* b = redDot::lookupMemoizedT(&row[2], 9, countingWalk);   // key 9 → walk 2 (new key)
        Node* b2 = redDot::lookupMemoizedT(&row[2], 9, countingWalk);  // key 9 again → CACHED
        Node* c = redDot::lookupMemoizedT(&row[2], 7, countingWalk);   // back to 7 → walk 3 (single-entry thrash)
        EXPECTF(walks == 3, "%d walks — the single-entry memo contract changed", walks);
        EXPECT(a == a2 && b == b2 && a == c && a == &row[0]);
    });

    TEST("L4 MULTI-HOP FAN-OUT: a 2-hop topology change fires no event on the walker — the epoch bump re-resolves it", {
        redDot::bumpTopologyEpoch();
        // [Host1][A][walker] — the walker resolves Host1 (through A).
        std::vector<Node> row(3);
        row[0].tag = HOSTM; row[1].tag = SUITE; row[2].tag = SUITE;
        link(row);
        int walks = 0;
        auto countingWalk = [&](Node* n, int) { ++walks; return walkHost(n); };
        EXPECT(redDot::lookupMemoizedT(&row[2], 0, countingWalk) == &row[0]);
        EXPECT(walks == 1);
        // Insert Host2 between Host1 and A. The walker's OWN slots are untouched —
        // Rack dispatches onExpanderChange only to Host1/A/Host2, NOT to the walker.
        // (Re-link by hand: the rack rewires expander pointers between process calls.)
        std::vector<Node> host2(1);
        host2[0].tag = HOSTM;
        host2[0].rightExpander.module = &row[1];
        row[1].leftExpander.module = &host2[0];
        row[0].rightExpander.module = &host2[0];
        host2[0].leftExpander.module = &row[0];
        // No bump yet: the memo legitimately still serves Host1 (the documented hole
        // of event-only caching; in Rack the bump happens inside the event, BEFORE
        // any consumer runs — this window is unobservable there).
        EXPECT(redDot::lookupMemoizedT(&row[2], 0, countingWalk) == &row[0]);
        EXPECT(walks == 1);
        // SOME module's onExpanderChange fired (Host1/A/Host2 got theirs) → the bump.
        redDot::bumpTopologyEpoch();
        // The walker's next read re-resolves: Host2 now — and only one more walk.
        EXPECT(redDot::lookupMemoizedT(&row[2], 0, countingWalk) == &host2[0]);
        EXPECT(walks == 2);
        // Static again: zero further walks.
        for (int i = 0; i < 1000; ++i)
            EXPECT(redDot::lookupMemoizedT(&row[2], 0, countingWalk) == &host2[0]);
        EXPECT(walks == 2);
    });

    // ── L5: the structural grep-gate (FATAL by default since the TAIL migration) ───
    TEST("L5 STRUCTURAL grep-gate: raw walk-helper call sites only in the service/primitive headers or LOOKUP-AUTHORITY-marked resolves (FATAL by default — MODULE_LOOKUP_STRICT=0 is the local debug hatch)", {
        static const char* const rawWalks[] = {
            "findMonsoonEitherSide(", "findPairHubEitherSide(", "resolveFollowedT(",
            "resolveFollowedIT(", "findIntertropicalEitherSide(", "assignPairId(",
            "assignPairIdT(", "isConnectedAndClaimed(", "findMonsoon(",
            // Template-argument forms — the plain patterns above cannot see foo<T>(...);
            // these close the hole (a raw resolveFollowedT<T> / assignPairIdT<T> /
            // findPairHubEitherSide<T> call site is the same anti-pattern).
            "resolveFollowedT<", "assignPairIdT<", "findPairHubEitherSide<",
        };
        static const char* const allowFiles[] = {
            "src/ui/ModuleLookup.hpp", "src/ui/ModuleLookupCore.hpp",
            "src/ui/VisualExpanderHelpers.hpp", "src/ui/IntertropicalPairing.hpp",
            "src/ui/MonsoonDiscovery.hpp",
        };
        auto isAllowedFile = [&](const std::string& rel) {
            for (const char* a : allowFiles)
                if (rel == a) return true;
            return false;
        };
        auto slurp = [](const std::filesystem::path& p) {
            std::ifstream f(p, std::ios::binary);
            std::ostringstream ss; ss << f.rdbuf();
            return ss.str();
        };
        const std::filesystem::path root = std::filesystem::current_path();
        struct Residual { std::string file; int line; std::string text; };
        std::vector<Residual> residuals;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "src")) {
            if (!entry.is_regular_file()) continue;
            const std::string ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".hpp" && ext != ".ipp") continue;
            const std::string rel = std::filesystem::relative(entry.path(), root).generic_string();
            if (isAllowedFile(rel)) continue;
            if (rel.rfind("src/deprecated/", 0) == 0) continue;   // dead code — not in the build
            const std::string src = slurp(entry.path());
            int lineNo = 0;
            size_t pos = 0;
            while (pos <= src.size()) {
                const size_t eol = src.find('\n', pos);
                const std::string line = src.substr(pos, (eol == std::string::npos ? src.size() : eol) - pos);
                ++lineNo;
                for (const char* raw : rawWalks)
                    if (line.find(raw) != std::string::npos
                        && line.find("LOOKUP-AUTHORITY") == std::string::npos) {
                        residuals.push_back({rel, lineNo, line});
                        break;
                    }
                if (eol == std::string::npos) break;
                pos = eol + 1;
            }
        }
        std::cout << "      residuals: " << residuals.size() << " raw walk-site line(s) outside the authority"
                  << " (expect 0 — the migration sweep is COMPLETE; any residual fails this suite)\n";
        for (size_t i = 0; i < residuals.size() && i < 12; ++i)
            std::cout << "        " << residuals[i].file << ":" << residuals[i].line << "\n";
        if (residuals.size() > 12)
            std::cout << "        … " << (residuals.size() - 12) << " more\n";
        // FATAL BY DEFAULT (the module-lookup TAIL migration is done — its final commit
        // flipped this). MODULE_LOOKUP_STRICT=0 is a LOCAL debug hatch (e.g. bisecting an
        // unrelated change with a knowingly-raw walk in flight); CI never sets it.
        const char* relax = std::getenv("MODULE_LOOKUP_STRICT");
        const bool enforce = !(relax && relax[0] == '0');
        if (enforce)
            EXPECTF(residuals.empty(),
                    "%zu raw walk sites remain outside the lookup authority", residuals.size());
    });

    std::cout << std::string(70, '=') << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail == 0 ? 0 : 1;
}
