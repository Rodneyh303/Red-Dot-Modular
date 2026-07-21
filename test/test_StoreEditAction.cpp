/**
 * test_StoreEditAction.cpp — standalone replica test for the store-edit undo helper.
 * Compile: g++ -std=c++17 -Itest -Isrc/ui test/test_StoreEditAction.cpp -o /tmp/test_sea && /tmp/test_sea
 *
 * Exercises the de-param undo prerequisite (DAW_PARAM_AUDIT.md §5b) against the test
 * rack.hpp shim: push semantics (record-only, ownership), voice-correct undo under
 * selection change, safe no-op after module deletion, and drag coalescing.
 */
#include "StoreEditAction.hpp"
#include <iostream>
#include <string>

static int s_pass = 0, s_fail = 0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc, cond) do{ \
    if (cond) { ++s_pass; std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n"; } \
    else      { ++s_fail; std::cout<<"  \033[31m✗\033[0m "<<(desc)<<"\n"; } \
}while(0)

// ── Fake Monsoon: a per-voice store + a UI selection that undo must IGNORE ──────────
struct FakeMonsoon : rack::engine::Module {
    static constexpr int VOICES = 4, LANES = 2;
    float macroSend[VOICES][LANES] = {};
    int   selectedVoice = 0;          // the UI tab — must NOT affect undo targeting
};

// module registry seam (simulates APP->engine->getModule)
static FakeMonsoon* g_mon = nullptr;
static rack::engine::Module* lookup(int64_t id) {
    return (g_mon && g_mon->id == id) ? g_mon : nullptr;
}

int main() {
    auto* ctx = APP;
    ctx->engine->getModuleHook = lookup;
    auto& hist = ctx->history->actions;

    FakeMonsoon mon; mon.id = 42; g_mon = &mon;
    auto setSend = [](int voice, int lane) {
        return std::function<void(FakeMonsoon&, float)>(
            [voice, lane](FakeMonsoon& m, float v) { m.macroSend[voice][lane] = v; });
    };

    SUITE("push semantics (record-only, Rack convention)");
    {
        mon.macroSend[2][1] = 0.7f;   // caller already applied the edit
        redDot::pushStoreEdit<FakeMonsoon>(&mon, "edit macro send", setSend(2, 1), 0.3f, 0.7f);
        TEST("push records one action",                hist.size() == 1);
        TEST("push does not re-apply (store keeps caller's value)", mon.macroSend[2][1] == 0.7f);
        TEST("action label set",                       hist.back()->name == "edit macro send");

        hist.back()->undo();
        TEST("undo restores oldValue",                 mon.macroSend[2][1] == 0.3f);
        hist.back()->redo();
        TEST("redo restores newValue",                 mon.macroSend[2][1] == 0.7f);
    }

    SUITE("no zero-size undo steps");
    {
        size_t before = hist.size();
        redDot::pushStoreEdit<FakeMonsoon>(&mon, "noop", setSend(0, 0), 0.5f, 0.5f);
        TEST("equal old/new records nothing",          hist.size() == before);
        redDot::pushStoreEdit<FakeMonsoon>(nullptr, "null", setSend(0, 0), 0.f, 1.f);
        TEST("null module records nothing",            hist.size() == before);
    }

    SUITE("voice-correct undo (the point of the design)");
    {
        // Edit voice 3 while it is selected...
        mon.selectedVoice = 3;
        float oldV = mon.macroSend[3][0];
        redDot::applyAndPushStoreEdit<FakeMonsoon>(&mon, "edit macro send", setSend(3, 0), oldV, 0.9f);
        TEST("applyAndPush applies",                   mon.macroSend[3][0] == 0.9f);

        // ...then the user browses to voice 1. A param-proxy undo would restore into
        // voice 1 via the store-back. StoreEditAction must hit voice 3.
        mon.selectedVoice = 1;
        mon.macroSend[1][0] = 0.111f;   // sentinel on the NOW-selected voice
        hist.back()->undo();
        TEST("undo lands on the voice actually edited (3)", mon.macroSend[3][0] == oldV);
        TEST("newly selected voice (1) untouched",          mon.macroSend[1][0] == 0.111f);
    }

    SUITE("module deletion safety");
    {
        redDot::applyAndPushStoreEdit<FakeMonsoon>(&mon, "edit", setSend(0, 1), 0.f, 0.4f);
        auto* act = hist.back();
        g_mon = nullptr;               // module deleted from the rack
        act->undo();                   // must not crash, must not write anywhere
        g_mon = &mon;                  // module undo-restored under the SAME id
        TEST("undo with module gone is a safe no-op",  mon.macroSend[0][1] == 0.4f);
        act->undo();
        TEST("after module returns (same id), undo works again", mon.macroSend[0][1] == 0.f);
    }

    SUITE("drag coalescing (one action per gesture)");
    {
        redDot::StoreEditCoalescer coal;
        size_t before = hist.size();

        // A drag: begin at 0.2, many live writes, end at 0.8 → exactly one action.
        mon.macroSend[1][1] = 0.2f;
        coal.begin(mon.macroSend[1][1]);
        for (float v = 0.25f; v <= 0.8001f; v += 0.05f) mon.macroSend[1][1] = v;   // live writes
        coal.commit<FakeMonsoon>(&mon, "edit macro send", setSend(1, 1), mon.macroSend[1][1]);
        TEST("one action per drag",                    hist.size() == before + 1);
        hist.back()->undo();
        TEST("undo returns to drag START value",       std::abs(mon.macroSend[1][1] - 0.2f) < 1e-6f);
        hist.back()->redo();
        TEST("redo returns to drag END value",         std::abs(mon.macroSend[1][1] - 0.8f) < 1e-3f);

        // Unchanged drag records nothing.
        before = hist.size();
        coal.begin(0.5f);
        coal.commit<FakeMonsoon>(&mon, "x", setSend(1, 0), 0.5f);
        TEST("unchanged drag records nothing",         hist.size() == before);

        // Cancelled drag records nothing.
        coal.begin(0.1f);
        coal.cancel();
        coal.commit<FakeMonsoon>(&mon, "x", setSend(1, 0), 0.9f);
        TEST("cancelled drag records nothing",         hist.size() == before);

        // Commit without begin: defensive no-op.
        coal.commit<FakeMonsoon>(&mon, "x", setSend(1, 0), 0.9f);
        TEST("commit without begin records nothing",   hist.size() == before);
    }

    SUITE("integer cells round-trip through float");
    {
        // Separate module type to prove the template is not Monsoon-specific. No registry
        // needed: the round-trip claim is about the setter/value path, exercised directly.
        struct FakeOwner : rack::engine::Module { int owner[4] = {0, 0, 0, 0}; };
        FakeOwner own; own.id = 7;
        redDot::StoreEditAction<FakeOwner> act("owner", own.id,
            [](FakeOwner& m, float v) { m.owner[2] = (int)v; }, 0.f, 3.f);
        act.setter(own, act.newValue);
        TEST("int cell receives exact 3",              own.owner[2] == 3);
        act.setter(own, act.oldValue);
        TEST("int cell restores exact 0",              own.owner[2] == 0);
    }

    std::cout << "\n" << s_pass << " passed, " << s_fail << " failed\n";
    return s_fail ? 1 : 0;
}
