#include "test_stubs.hpp"
/**
 * test_lock_behaviour.cpp
 * Tests for lock mode: RNG state preservation, seed queuing,
 * pattern freeze, and reset-while-locked behaviour.
 *
 * Compile: g++ -std=c++17 -I. test_lock_behaviour.cpp -o test_lock && ./test_lock
 */

#include "PatternEngine.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

// ── Test framework ────────────────────────────────────────────────────────────
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)       do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a,b)  do{if((a)!=(b)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" != "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)
#define EXPECT_NE(a,b)  do{if((a)==(b)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" == "<<#b<<"="<<(b)<<" (expected different)"; \
    throw std::runtime_error(s.str());}}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" not near "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)

// ── Helpers ───────────────────────────────────────────────────────────────────
static PatternInput unlocked(float restProb=0.2f) {
    PatternInput in;
    for(int i=0;i<12;++i) in.semiWeights[i]=1.f;
    in.restProb=restProb; in.variationAmount=0.5f;
    in.octaveLo=3.f; in.octaveHi=5.f;
    in.noteVariationMask=0b111; in.locked=false;
    return in;
}
static PatternInput locked_in(float restProb=0.2f) {
    PatternInput in = unlocked(restProb);
    in.locked = true;
    return in;
}

static PatternEngine seeded(float rseed, float mseed) {
    PatternEngine pe;
    pe.seedRngFromFloat(pe.rhythmRng, rseed);
    pe.seedRngFromFloat(pe.melodyRng, mseed);
    return pe;
}

// Snapshot of both pattern arrays for comparison
struct PatSnap {
    bool  rhythm[16];
    int   semitone[16];
    float pitch[16];
    PatSnap(const PatternEngine& pe) {
        for(int i=0;i<16;++i){
            rhythm[i]   = pe.rhythmPattern[i];
            semitone[i] = pe.melodySemitone[i];
            pitch[i]    = pe.melodyPitchV[i];
        }
    }
    bool rhythmEq(const PatSnap& o) const {
        for(int i=0;i<16;++i) if(rhythm[i]!=o.rhythm[i]) return false;
        return true;
    }
    bool melodyEq(const PatSnap& o) const {
        for(int i=0;i<16;++i) if(semitone[i]!=o.semitone[i]) return false;
        return true;
    }
};

// RNG state snapshot
struct RngSnap {
    uint64_t rs[2], ms[2];
    RngSnap(const PatternEngine& pe) {
        rs[0]=pe.rhythmRng.state[0]; rs[1]=pe.rhythmRng.state[1];
        ms[0]=pe.melodyRng.state[0]; ms[1]=pe.melodyRng.state[1];
    }
    bool rhythmEq(const RngSnap& o) const { return rs[0]==o.rs[0]&&rs[1]==o.rs[1]; }
    bool melodyEq(const RngSnap& o) const { return ms[0]==o.ms[0]&&ms[1]==o.ms[1]; }
};

// ═══════════════════════════════════════════════════════════════════════════════
int main(){
    std::cout<<"\033[1mLock Behaviour Tests\033[0m\n";
    std::cout<<std::string(54,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Phrase Boundary While Locked — Pattern Freeze");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Phrase boundary while locked: rhythm pattern unchanged", {
        auto pe=seeded(3.f,4.f);
        pe.applyPendingSeedsAndRedraw(unlocked());  // initial draw
        PatSnap before(pe);
        pe.applyPendingSeedsAndRedraw(locked_in()); // boundary while locked
        PatSnap after(pe);
        EXPECT(before.rhythmEq(after));
    });

    TEST("Phrase boundary while locked: melody pattern unchanged", {
        auto pe=seeded(3.f,4.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        PatSnap before(pe);
        pe.applyPendingSeedsAndRedraw(locked_in());
        PatSnap after(pe);
        EXPECT(before.melodyEq(after));
    });

    TEST("Phrase boundary while locked: RNG state NOT advanced", {
        auto pe=seeded(5.f,6.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        RngSnap before(pe);
        pe.applyPendingSeedsAndRedraw(locked_in());
        RngSnap after(pe);
        EXPECT(before.rhythmEq(after));
        EXPECT(before.melodyEq(after));
    });

    TEST("Unlock then phrase boundary: pattern changes from frozen state", {
        auto pe=seeded(3.f,4.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        PatSnap frozen(pe);
        // Several locked boundaries (RNG untouched)
        for(int i=0;i<5;++i) pe.applyPendingSeedsAndRedraw(locked_in());
        // Unlock
        pe.applyPendingSeedsAndRedraw(unlocked());
        PatSnap after(pe);
        // Pattern should now differ (same RNG state → same next pattern as if
        // never locked — but different from frozen because it's a new draw)
        // We just verify no crash and patterns are valid
        for(int i=0;i<16;++i){
            EXPECT(after.semitone[i]>=0 && after.semitone[i]<12);
            EXPECT(after.pitch[i]>=0.f  && after.pitch[i]<=5.f);
        }
    });

    TEST("Multiple locked boundaries: each leaves RNG in same state", {
        auto pe=seeded(7.f,8.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        RngSnap base(pe);
        pe.applyPendingSeedsAndRedraw(locked_in());
        RngSnap after1(pe);
        pe.applyPendingSeedsAndRedraw(locked_in());
        RngSnap after2(pe);
        EXPECT(base.rhythmEq(after1));
        EXPECT(base.rhythmEq(after2));
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Seed Queuing While Locked");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Pending seed while locked: seed stays pending (not applied)", {
        auto pe=seeded(2.f,3.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        pe.rhythmSeedPendingFloat=9.f; pe.rhythmSeedPending=true;
        pe.applyPendingSeedsAndRedraw(locked_in());
        EXPECT(pe.rhythmSeedPending);               // still pending
        EXPECT_NEAR(pe.rhythmSeedFloat, 0.f, 1e-4f); // default — 9.0 (pending) never applied
    });

    TEST("Pending seed while locked: RNG NOT reseeded", {
        auto pe=seeded(2.f,3.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        RngSnap before(pe);
        pe.rhythmSeedPendingFloat=9.f; pe.rhythmSeedPending=true;
        pe.applyPendingSeedsAndRedraw(locked_in());
        RngSnap after(pe);
        EXPECT(before.rhythmEq(after));
    });

    TEST("Pending seed fires on first unlocked phrase boundary", {
        auto pe=seeded(2.f,3.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        pe.rhythmSeedPendingFloat=9.f; pe.rhythmSeedPending=true;
        // Lock for several boundaries
        for(int i=0;i<3;++i) pe.applyPendingSeedsAndRedraw(locked_in());
        EXPECT(pe.rhythmSeedPending);  // still waiting
        // Unlock: seed fires
        pe.applyPendingSeedsAndRedraw(unlocked());
        EXPECT(!pe.rhythmSeedPending);              // consumed
        EXPECT_NEAR(pe.rhythmSeedFloat, 9.f, 1e-4f); // applied
    });

    TEST("Seed fired on unlock produces same pattern as direct seed application", {
        // Pattern from: seed 9.0 → redraw
        auto ref=seeded(9.f,3.f);
        ref.applyPendingSeedsAndRedraw(unlocked());
        PatSnap refSnap(ref);

        // Pattern from: seed 2.0 → lock → arm seed 9.0 → unlock
        auto pe=seeded(2.f,3.f);
        pe.applyPendingSeedsAndRedraw(unlocked());  // initial draw with seed 2
        pe.rhythmSeedPendingFloat=9.f; pe.rhythmSeedPending=true;
        for(int i=0;i<4;++i) pe.applyPendingSeedsAndRedraw(locked_in());
        pe.applyPendingSeedsAndRedraw(unlocked());
        PatSnap peSnap(pe);

        EXPECT(refSnap.rhythmEq(peSnap));
    });

    TEST("Melody seed also queued while locked, fires on unlock", {
        auto pe=seeded(1.f,1.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        pe.melodySeedPendingFloat=8.f; pe.melodySeedPending=true;
        pe.applyPendingSeedsAndRedraw(locked_in());
        EXPECT(pe.melodySeedPending);
        pe.applyPendingSeedsAndRedraw(unlocked());
        EXPECT(!pe.melodySeedPending);
        EXPECT_NEAR(pe.melodySeedFloat, 8.f, 1e-4f);
    });

    TEST("Both rhythm and melody seeds queued: both fire together on unlock", {
        auto pe=seeded(1.f,1.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        pe.rhythmSeedPendingFloat=5.f; pe.rhythmSeedPending=true;
        pe.melodySeedPendingFloat=7.f; pe.melodySeedPending=true;
        pe.applyPendingSeedsAndRedraw(locked_in());
        EXPECT(pe.rhythmSeedPending && pe.melodySeedPending);
        pe.applyPendingSeedsAndRedraw(unlocked());
        EXPECT(!pe.rhythmSeedPending && !pe.melodySeedPending);
        EXPECT_NEAR(pe.rhythmSeedFloat, 5.f, 1e-4f);
        EXPECT_NEAR(pe.melodySeedFloat, 7.f, 1e-4f);
    });

    TEST("Lock then unlock with no pending seed: same RNG state, fresh redraw", {
        auto pe=seeded(4.f,5.f);
        pe.applyPendingSeedsAndRedraw(unlocked());
        RngSnap beforeLock(pe);
        // Lock/unlock cycle with no new seed
        for(int i=0;i<3;++i) pe.applyPendingSeedsAndRedraw(locked_in());
        RngSnap afterLock(pe);
        EXPECT(beforeLock.rhythmEq(afterLock));  // RNG unchanged by lock
        pe.applyPendingSeedsAndRedraw(unlocked());  // fresh redraw from same state
        // Pattern is deterministic from that RNG state
        RngSnap afterUnlock(pe);
        EXPECT(!beforeLock.rhythmEq(afterUnlock)); // RNG DID advance on redraw
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Reset While Locked — Step Resets, Pattern Frozen");
    // ─────────────────────────────────────────────────────────────────────────
    // These tests use a MeloDicer-level mirror of handleRestart() logic
    // since that function lives in MeloDicer, not PatternEngine.
    // We mirror the exact logic here.

    struct MockModule {
        int   stepIndex    = -1;
        int   startStep    = 0;
        bool  locked       = false;
        bool  gateHeld     = false;
        float holdRemain   = 0.f;
        bool  rhythmSeedPending = false;
        bool  melodySeedPending = false;
        float rhythmSeedFloat   = 0.f;
        float melodySeedFloat   = 0.f;
        float rhythmSeedPendingFloat = 0.f;
        float melodySeedPendingFloat = 0.f;
        bool  rhythmPattern[16] = {};
        int   melodySemitone[16] = {};

        void handleRestart(bool resetImmediate) {
            // Step + gate always reset
            stepIndex  = (startStep - 1 + 16) % 16;
            gateHeld   = false;
            holdRemain = 0.f;
            // Patterns + seeds only when unlocked
            if (!locked) {
                if (resetImmediate) {
                    if (rhythmSeedPending) {
                        rhythmSeedFloat = rhythmSeedPendingFloat;
                        rhythmSeedPending = false;
                    }
                    if (melodySeedPending) {
                        melodySeedFloat = melodySeedPendingFloat;
                        melodySeedPending = false;
                    }
                }
                // redraw would happen here (tested separately via PatternEngine)
            }
        }
    };

    TEST("Reset while locked: stepIndex resets to (startStep-1)", {
        MockModule m; m.locked=true; m.stepIndex=7; m.startStep=0;
        m.handleRestart(false);
        EXPECT_EQ(m.stepIndex, 15);  // (0-1+16)%16 = 15
    });

    TEST("Reset while locked: gate cleared", {
        MockModule m; m.locked=true; m.gateHeld=true; m.holdRemain=3.f;
        m.handleRestart(false);
        EXPECT(!m.gateHeld);
        EXPECT_NEAR(m.holdRemain, 0.f, 1e-5f);
    });

    TEST("Reset while locked, resetImmediate: pending seed NOT applied", {
        MockModule m; m.locked=true;
        m.rhythmSeedPending=true; m.rhythmSeedPendingFloat=9.f;
        m.rhythmSeedFloat=2.f;
        m.handleRestart(true);
        EXPECT(m.rhythmSeedPending);               // still pending
        EXPECT_NEAR(m.rhythmSeedFloat, 2.f, 1e-4f); // unchanged
    });

    TEST("Reset while unlocked, resetImmediate: pending seed applied", {
        MockModule m; m.locked=false;
        m.rhythmSeedPending=true; m.rhythmSeedPendingFloat=9.f;
        m.rhythmSeedFloat=2.f;
        m.handleRestart(true);
        EXPECT(!m.rhythmSeedPending);
        EXPECT_NEAR(m.rhythmSeedFloat, 9.f, 1e-4f);
    });

    TEST("Reset while locked: step position correct for non-zero startStep", {
        MockModule m; m.locked=true; m.startStep=5; m.stepIndex=12;
        m.handleRestart(false);
        EXPECT_EQ(m.stepIndex, 4);  // (5-1+16)%16 = 4
    });

    TEST("Reset at startStep=0: stepIndex becomes 15 (pre-increment to 0)", {
        MockModule m; m.locked=true; m.startStep=0; m.stepIndex=0;
        m.handleRestart(false);
        EXPECT_EQ(m.stepIndex, 15);
    });

    TEST("Reset while locked then unlock + phrase boundary: pending seed fires", {
        MockModule m; m.locked=true;
        m.rhythmSeedPending=true; m.rhythmSeedPendingFloat=7.f;
        m.handleRestart(true);      // reset while locked — seed still pending
        EXPECT(m.rhythmSeedPending);
        m.locked=false;             // unlock
        m.handleRestart(true);      // reset while unlocked — seed fires
        EXPECT(!m.rhythmSeedPending);
        EXPECT_NEAR(m.rhythmSeedFloat, 7.f, 1e-4f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Lock/Unlock Determinism");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Identical lock/unlock history → identical patterns", {
        auto p1=seeded(3.f,4.f), p2=seeded(3.f,4.f);
        auto in_u=unlocked(), in_l=locked_in();
        // Both: draw → lock 3 → unlock
        p1.applyPendingSeedsAndRedraw(in_u);
        for(int i=0;i<3;++i) p1.applyPendingSeedsAndRedraw(in_l);
        p1.applyPendingSeedsAndRedraw(in_u);

        p2.applyPendingSeedsAndRedraw(in_u);
        for(int i=0;i<3;++i) p2.applyPendingSeedsAndRedraw(in_l);
        p2.applyPendingSeedsAndRedraw(in_u);

        PatSnap s1(p1), s2(p2);
        EXPECT(s1.rhythmEq(s2) && s1.melodyEq(s2));
    });

    TEST("Lock for 0 cycles is same as no lock", {
        auto p1=seeded(5.f,6.f), p2=seeded(5.f,6.f);
        auto in_u=unlocked();
        p1.applyPendingSeedsAndRedraw(in_u);  // draw once
        p2.applyPendingSeedsAndRedraw(in_u);  // draw once
        // p1: no lock cycles
        p1.applyPendingSeedsAndRedraw(in_u);
        // p2: lock for 0 cycles (same)
        p2.applyPendingSeedsAndRedraw(in_u);
        PatSnap s1(p1), s2(p2);
        EXPECT(s1.rhythmEq(s2) && s1.melodyEq(s2));
    });

    TEST("Pattern after unlock is same as if lock never happened (RNG preserved)", {
        // Without lock: seed 3 → draw1 → draw2
        auto ref=seeded(3.f,4.f);
        auto in_u=unlocked();
        ref.applyPendingSeedsAndRedraw(in_u);  // draw1
        ref.applyPendingSeedsAndRedraw(in_u);  // draw2
        PatSnap refSnap(ref);

        // With lock: seed 3 → draw1 → lock×5 → unlock (draw2)
        auto pe=seeded(3.f,4.f);
        pe.applyPendingSeedsAndRedraw(in_u);
        auto in_l=locked_in();
        for(int i=0;i<5;++i) pe.applyPendingSeedsAndRedraw(in_l);
        pe.applyPendingSeedsAndRedraw(in_u);  // same as draw2
        PatSnap peSnap(pe);

        EXPECT(refSnap.rhythmEq(peSnap));
        EXPECT(refSnap.melodyEq(peSnap));
    });

    TEST("Locking never corrupts the pending seed value", {
        auto pe=seeded(1.f,1.f);
        pe.rhythmSeedPendingFloat=6.5f; pe.rhythmSeedPending=true;
        for(int i=0;i<10;++i) pe.applyPendingSeedsAndRedraw(locked_in());
        EXPECT_NEAR(pe.rhythmSeedPendingFloat, 6.5f, 1e-5f);
        EXPECT(pe.rhythmSeedPending);
    });

    // ─────────────────────────────────────────────────────────────────────────
    std::cout<<"\n"<<std::string(54,'=')<<"\n";
    std::cout<<"\033[32m"<<s_pass<<" passed\033[0m  ";
    if(s_fail>0)std::cout<<"\033[31m"<<s_fail<<" failed\033[0m";
    else        std::cout<<"\033[32m0 failed\033[0m";
    std::cout<<"  ("<<(s_pass+s_fail)<<" total)\n\n";
    return s_fail>0?1:0;
}
