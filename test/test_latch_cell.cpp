#include "test_stubs.hpp"
/**
 * test_latch_cell.cpp
 * CA1 — the "latched-at-boundary control" lifecycle, proven as ONE mechanic.
 *
 * WHAT THIS PINS. CONCEPT_ODR_AUDIT CA1: every latched control (voice/lane LOR, lane
 * direction, lane sign, dice/CA/cantonment) re-rolls the same {pending, committed,
 * commit-copy, scope live-gate} lifecycle. It drifted twice (the double-commit bug; BUG 6's
 * wrong edge), each a drift inside one hand-copy. `redDot::LatchCell` is the single
 * definition. This is the BYTE-IDENTICAL BRIDGE: for each control, a VERBATIM copy of the
 * engine's current inline commit logic is the oracle, and LatchCell must reproduce it over
 * a fuzz of every (edit, lock, scope, edge, hold) combination.
 *
 * It is NOT wired into the engine yet — that is the reviewed next step. This proves the
 * skeleton reproduces the TWO real lifecycle shapes (direction: two edges, no hold; LOR:
 * phrase/unlock edge, suspend/sticky hold) before any engine site is converted.
 *
 * Compile: see test/run_all.sh (the registry is the source of truth).
 */
#include "dsp/LatchCell.hpp"
#include <iostream>
#include <sstream>
#include <cstdint>

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

// A tiny deterministic LCG so the fuzz is reproducible and Rack-free.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed): s(seed) {}
    uint32_t next(){ s = s*6364136223846793005ULL + 1442695040888963407ULL; return (uint32_t)(s>>33); }
    bool bit(){ return (next() & 1u) != 0; }
    int range(int n){ return (int)(next() % (uint32_t)n); }
};

// ─────────────────────────────────────────────────────────────────────────────
// ORACLE A — lane DIRECTION, a VERBATIM transcription of SequencerEngine's inline
// commit (advancePlayhead, the StepEdge and Phrase-on-wrap sites):
//   edge  = (quant==StepEdge && stepEdgeNow) || (quant==Phrase && wrapped)
//   live  = dirLive_ = !locked || strandScopeBit            [SequencerEngine.hpp:460]
//   commit: if (edge && live) committed = pending;          [no per-element hold]
// This is written out independently so matching it is a real bridge, not a tautology.
// ─────────────────────────────────────────────────────────────────────────────
enum class Quant { StepEdge, Phrase };
struct DirOracle {
    int committed = +1;   // laneDir_ as a sign for the test (+1 fwd / -1 rev)
    int pending   = +1;
    void stage(int v){ pending = v; }
    void tick(Quant quant, bool stepEdgeNow, bool wrapped, bool locked, bool strandScopeBit) {
        const bool edge = (quant==Quant::StepEdge && stepEdgeNow) || (quant==Quant::Phrase && wrapped);
        const bool live = !locked || strandScopeBit;           // dirLive_
        if (edge && live) committed = pending;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ORACLE B — voice/lane LOR, a VERBATIM transcription of refreshVoiceLorLatches /
// refreshLaneLatchesForVoice + their gate:
//   edge  = atPhraseEdge || unlockNow                       [lorCommitGate_ / queueFires]
//   live  = lorLive_ = !locked || axisScopeBit              [SequencerEngine.hpp:1632]
//   held  = suspend || sticky                               [the per-item HOLD skips]
//   commit: if (edge && live && !held) committed = pending;
// ─────────────────────────────────────────────────────────────────────────────
struct LorOracle {
    float committed = 0.f;
    float pending   = 0.f;
    void stage(float v){ pending = v; }
    void tick(bool atPhraseEdge, bool unlockNow, bool locked, bool axisScopeBit, bool suspend, bool sticky) {
        const bool edge = atPhraseEdge || unlockNow;
        const bool live = !locked || axisScopeBit;
        const bool held = suspend || sticky;
        if (edge && live && !held) committed = pending;
    }
};

int main(){
    std::cout<<"\033[1mCA1 — LatchCell: the one latched-at-boundary mechanic\033[0m\n";
    std::cout<<std::string(62,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G1 — DIRECTION bridge: LatchCell reproduces the inline commit, byte-exact");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G1a fuzz: LatchCell.committed == DirOracle.committed at every step (200k)", {
        Rng rng(0xD12ECC01u);
        DirOracle oracle;
        redDot::LatchCell<int> cell; cell.seed(+1);
        for (int n = 0; n < 200000; ++n) {
            // random edit to pending (sometimes)
            if (rng.bit()) { int v = rng.bit() ? +1 : -1; oracle.stage(v); cell.stage(v); }
            // random transport/lock/scope context
            Quant quant   = rng.bit() ? Quant::StepEdge : Quant::Phrase;
            bool stepEdge = rng.bit();
            bool wrapped  = rng.bit();
            bool locked   = rng.bit();
            bool scopeBit = rng.bit();
            // oracle
            oracle.tick(quant, stepEdge, wrapped, locked, scopeBit);
            // skeleton: the caller derives edge+live exactly as the engine does, then
            // commits through the ONE mechanic.
            const bool edge = (quant==Quant::StepEdge && stepEdge) || (quant==Quant::Phrase && wrapped);
            const bool live = !locked || scopeBit;
            cell.commit(edge, live);
            EXPECT_EQ(cell.read(), oracle.committed);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G2 — LOR bridge: LatchCell reproduces the inline commit WITH the hold");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G2a fuzz: LatchCell.committed == LorOracle.committed at every step (200k)", {
        Rng rng(0x10ECC02u);
        LorOracle oracle;
        redDot::LatchCell<float> cell; cell.seed(0.f);
        for (int n = 0; n < 200000; ++n) {
            if (rng.bit()) { float v = (float)(rng.range(2001) - 1000) / 1000.f; oracle.stage(v); cell.stage(v); }
            bool atPhraseEdge = rng.bit();
            bool unlockNow    = rng.bit();
            bool locked       = rng.bit();
            bool axisBit      = rng.bit();
            bool suspend      = rng.bit();
            bool sticky       = rng.bit();
            oracle.tick(atPhraseEdge, unlockNow, locked, axisBit, suspend, sticky);
            const bool edge = atPhraseEdge || unlockNow;
            const bool live = !locked || axisBit;
            const bool held = suspend || sticky;
            cell.commit(edge, live, held);
            // bit-exact float compare (it is a copy, never arithmetic)
            EXPECT(cell.read() == oracle.committed);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G3 — the commit precedence, pinned directly");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G3a no edge → never commits (pending churns under a frozen committed)", {
        redDot::LatchCell<int> c; c.seed(5);
        c.stage(9);
        for (int i=0;i<10;++i) EXPECT(!c.commit(/*edge=*/false, /*live=*/true));
        EXPECT_EQ(c.read(), 5);
    });
    TEST("G3b edge but scope dead (locked, bit off) → never commits", {
        redDot::LatchCell<int> c; c.seed(5); c.stage(9);
        EXPECT(!c.commit(/*edge=*/true, /*live=*/false));
        EXPECT_EQ(c.read(), 5);
    });
    TEST("G3c edge + live but held → never commits (the suspend/sticky hold)", {
        redDot::LatchCell<int> c; c.seed(5); c.stage(9);
        EXPECT(!c.commit(/*edge=*/true, /*live=*/true, /*held=*/true));
        EXPECT_EQ(c.read(), 5);
    });
    TEST("G3d edge + live + not held → commits exactly once to the staged value", {
        redDot::LatchCell<int> c; c.seed(5); c.stage(9);
        EXPECT(c.commit(true, true));
        EXPECT_EQ(c.read(), 9);
        // committing again with no new stage is idempotent (re-copies the same value)
        EXPECT(c.commit(true, true));
        EXPECT_EQ(c.read(), 9);
    });
    TEST("G3e unlock flush commits even though a phrase edge has not arrived", {
        // the LOR case: edge = atPhraseEdge || unlockNow — unlockNow alone fires
        redDot::LatchCell<float> c; c.seed(0.f); c.stage(0.75f);
        const bool edge = /*atPhraseEdge=*/false || /*unlockNow=*/true;
        EXPECT(c.commit(edge, /*live=*/true));
        EXPECT(c.read() == 0.75f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G4 — LatchBank: one sweep commits every element with the same precedence");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G4a bank commitAll matches per-cell commit over a fuzz (per-element hold)", {
        constexpr std::size_t N = 7;   // NUM_STRANDS-shaped
        Rng rng(0xBA54u);
        redDot::LatchBank<float, N> bank;
        redDot::LatchCell<float> ref[N];
        for (std::size_t i=0;i<N;++i){ bank.seed(i, 0.f); ref[i].seed(0.f); }
        for (int n=0;n<50000;++n){
            // stage random edits
            for (std::size_t i=0;i<N;++i) if (rng.bit()) {
                float v=(float)(rng.range(201)-100)/100.f; bank.stage(i,v); ref[i].stage(v);
            }
            bool edge=rng.bit(), live=rng.bit();
            // per-element hold pattern
            uint32_t holdMask = rng.next();
            auto held = [&](std::size_t i){ return ((holdMask>>i)&1u)!=0; };
            bank.commitAll(edge, live, held);
            for (std::size_t i=0;i<N;++i){ ref[i].commit(edge, live, held(i)); EXPECT(bank.read(i)==ref[i].read()); }
        }
    });
    TEST("G4b the no-hold overload == commitAll with held==false", {
        constexpr std::size_t N = 4;
        redDot::LatchBank<int,N> a, b;
        for (std::size_t i=0;i<N;++i){ a.seed(i,0); b.seed(i,0); a.stage(i,(int)i+1); b.stage(i,(int)i+1); }
        a.commitAll(true, true);
        b.commitAll(true, true, [](std::size_t){ return false; });
        for (std::size_t i=0;i<N;++i) EXPECT_EQ(a.read(i), b.read(i));
    });

    std::cout<<"\n"<<std::string(62,'=')<<"\n";
    std::cout<<(s_fail==0?"\033[32m":"\033[31m")<<s_pass<<" passed, "<<s_fail
             <<" failed\033[0m\n";
    return s_fail==0 ? 0 : 1;
}

/* ── MUTATION LOG (to run once green) ─────────────────────────────────────────
 * Each mutant built to its own binary, build verified.
 *   M1  commit drops the `!held` term        → G2a + G3c FAIL (held elements commit).
 *   M2  commit drops the `scopeLive` term     → G1a + G2a + G3b FAIL (commits under a dead scope).
 *   M3  commit drops the `boundaryFires` term → G1a + G2a + G3a FAIL (commits off-edge).
 *   M4  commit copies BEFORE the gate (always committed=pending, returns gate) → every
 *       bridge FAILS (the drift-bug shape: the write escapes the gate).
 *   M5  LatchBank::commitAll(no-hold) passes held=true → G4b FAILS.
 * ─────────────────────────────────────────────────────────────────────────── */
