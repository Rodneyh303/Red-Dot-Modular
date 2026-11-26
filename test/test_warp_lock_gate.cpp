// test/test_warp_lock_gate.cpp — G4 of the lock+undo sweep (audit #377).
//
// THE LAW: warp UI edits respect Lock. The UI writes the warp store immediately (knob +
// undo stay live), but the SOUNDING mirror rebuild (the warpUiScope_ drain) is deferred
// and commits on the phrase boundary via the SAME gate as the CA/Sands pending machinery
// — lockManager.queueFires() (fire at a boundary while unlocked, flush at the unlock
// edge, HOLD while locked between boundaries). Previously the drain ran unconditionally
// at the top of process(), ignoring lock entirely.
//
// This is a structural pin (pure source slurp, the test_played_column_consumers / D
// precedent — no engine units): it guards that the warpUiScope_ drain cannot silently
// regress to an ungated, pre-tick drain. The sounding/boundary-commit behaviour itself
// is a Rack feel-test.
//
// Reproduce-first: FAILS on the pre-G4 source (the drain was a bare `switch (warpUiScope_)`
// near setWarpBank, with no queueFires gate).
//
// Compile: see test/run_all.sh (empty group — source slurp only)
#include <cstdio>
#include <string>

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const std::string& what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what.c_str()); }
}

static bool slurpFile(const char* path, std::string& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[4096]; size_t n; out.clear();
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

// Strip // and /* */ comments so a gate in a comment can never satisfy an assertion.
static std::string stripComments(const std::string& src) {
    std::string out; out.reserve(src.size());
    bool line = false, block = false;
    for (size_t i = 0; i < src.size(); ++i) {
        const char c = src[i], nxt = (i + 1 < src.size()) ? src[i + 1] : '\0';
        if (!line && !block && c == '/' && nxt == '/') { line = true;  out += ' ';      continue; }
        if (!line && !block && c == '/' && nxt == '*') { block = true; out += ' '; ++i;  continue; }
        if (line && c == '\n')                         { line = false; out += '\n';      continue; }
        if (block && c == '*' && nxt == '/')           { block = false; out += ' '; ++i; continue; }
        if (!line && !block) out += c;
    }
    return out;
}

int main() {
    std::string src;
    if (!slurpFile("../../src/Monsoon.cpp", src) && !slurpFile("src/Monsoon.cpp", src)) {
        std::printf("  FAIL: could not open src/Monsoon.cpp\n");
        return 1;
    }
    const std::string code = stripComments(src);

    // 1. The warp drain switch appears EXACTLY ONCE (no duplicated / leftover early drain).
    size_t first = code.find("switch (warpUiScope_)");
    size_t second = (first == std::string::npos)
                        ? std::string::npos
                        : code.find("switch (warpUiScope_)", first + 1);
    check(first != std::string::npos, "warpUiScope_ drain switch present");
    check(second == std::string::npos, "warpUiScope_ drain switch is NOT duplicated (old early drain removed)");

    // 2. The drain is LOCK-GATED: a queueFires() gate on warpUiScope_ immediately precedes
    //    the switch (within a small window — the `if (... queueFires()) { switch ... }`).
    bool gated = false;
    if (first != std::string::npos) {
        const size_t windowStart = first > 160 ? first - 160 : 0;
        const std::string pre = code.substr(windowStart, first - windowStart);
        gated = pre.find("warpUiScope_ != WarpUiScope::NONE") != std::string::npos
             && pre.find("lockManager.queueFires()") != std::string::npos;
    }
    check(gated, "warp drain is gated on (warpUiScope_ != NONE && lockManager.queueFires())");

    // 3. The gate sits in the control-rate block AFTER lockManager.tick (so queueFires() is
    //    this cycle's edge), i.e. the gated drain follows the tick call in source order.
    const size_t tickPos = code.find("lockManager.tick(");
    check(tickPos != std::string::npos, "lockManager.tick(...) present");
    check(tickPos != std::string::npos && first != std::string::npos && first > tickPos,
          "warp drain follows lockManager.tick (reads this cycle's lock/boundary edges)");

    // 4. setWarpBank (the bank POINTER wire) stays — only the drain moved, not the wiring.
    check(code.find("setWarpBank(&editor.warp)") != std::string::npos,
          "setWarpBank pointer wire retained");

    std::printf("%s%d passed, %d failed%s\n", g_fail ? "" : "", g_pass, g_fail, "");
    return g_fail ? 1 : 0;
}
