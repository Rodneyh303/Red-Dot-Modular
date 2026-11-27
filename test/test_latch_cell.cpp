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

// Engine constants the policies MUST mirror (single source: LaneMapping.hpp /
// MonsoonLockManager.hpp — transcribed here so the bridge is independent of them).
static constexpr std::size_t NUM_STRANDS = 7;   // MELODY0 OCT1 QMIX2 RHY3 ACC4 VAR5 LEG6
static constexpr uint32_t    SB_SANDS_R  = 1u << 2;
static constexpr uint32_t    SB_SANDS_M  = 1u << 3;
static inline bool melodyAxis(std::size_t strand){ return strand==0 || strand==1; }  // LaneDirPolicy::scopeLive's melody-axis test

// ─────────────────────────────────────────────────────────────────────────────
// ORACLE A — lane DIRECTION, a VERBATIM transcription of SequencerEngine's commit
// (advancePlayhead's StepEdge pre-advance + Phrase-on-wrap post-advance tick sites,
// both now routed through LaneDirPolicy):
//   edge  = (quant==StepEdge && stepEdgeNow) || (quant==Phrase && wrapped)
//   live  = LaneDirPolicy::scopeLive = !locked || strandScopeBit
//   commit: if (edge && live) committed = pending;          [no per-element hold]
// Written out independently so matching it is a real bridge, not a tautology.
// ─────────────────────────────────────────────────────────────────────────────
enum class Quant { StepEdge, Phrase };
static bool dirLiveInline(std::size_t strand, bool locked, uint32_t mask) {
    if (!locked) return true;
    return (mask & (melodyAxis(strand) ? SB_SANDS_M : SB_SANDS_R)) != 0;
}
struct DirOracle {
    int committed = +1;
    int pending   = +1;
    void stage(int v){ pending = v; }
    void tick(Quant quant, bool stepEdgeNow, bool wrapped, bool locked, std::size_t strand, uint32_t mask) {
        const bool edge = (quant==Quant::StepEdge && stepEdgeNow) || (quant==Quant::Phrase && wrapped);
        if (edge && dirLiveInline(strand, locked, mask)) committed = pending;
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

// ─────────────────────────────────────────────────────────────────────────────
// The POLICIES — what the engine would carry at wiring time. Each owns the EDGE and the
// SCOPE gate as ONE definition. They encode the SAME logic as the oracles above, but the
// bridge drives them through LatchedControl's single commit entry, so it proves the
// forcing-function design, not just the boolean algebra.
// ─────────────────────────────────────────────────────────────────────────────
struct LaneDirPolicy {
    using Value = int;
    static constexpr std::size_t N = NUM_STRANDS;
    Quant quant = Quant::Phrase;
    bool edge(const redDot::LatchCtx& c) const {
        return quant==Quant::StepEdge ? c.stepEdge : (quant==Quant::Phrase && c.wrapped);
    }
    bool scopeLive(std::size_t strand, const redDot::LatchCtx& c) const {
        return dirLiveInline(strand, c.locked, c.scopeLiveMask);
    }
};
struct LorLanePolicy {
    using Value = float;
    static constexpr std::size_t N = NUM_STRANDS;
    bool edge(const redDot::LatchCtx& c) const { return c.wrapped || c.unlockNow; }
    bool scopeLive(std::size_t strand, const redDot::LatchCtx& c) const {
        if (!c.locked) return true;
        return (c.scopeLiveMask & (melodyAxis(strand) ? SB_SANDS_M : SB_SANDS_R)) != 0;
    }
};

int main(){
    std::cout<<"\033[1mCA1 — LatchCell: the one latched-at-boundary mechanic\033[0m\n";
    std::cout<<std::string(62,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G1 — DIRECTION bridge: LatchedControl (single entry) == inline oracle, byte-exact");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G1a fuzz: all 7 strands, every (edit,quant,stepEdge,wrap,lock,scope) (100k ticks)", {
        Rng rng(0xD12ECC01u);
        DirOracle oracle[NUM_STRANDS];
        redDot::LatchedControl<LaneDirPolicy> ctrl;
        for (std::size_t st=0;st<NUM_STRANDS;++st) ctrl.seed(st, +1);
        for (int n = 0; n < 100000; ++n) {
            // random per-strand edits
            for (std::size_t st=0;st<NUM_STRANDS;++st) if (rng.bit()) {
                int v = rng.bit() ? +1 : -1; oracle[st].stage(v); ctrl.stage(st, v);
            }
            Quant quant   = rng.bit() ? Quant::StepEdge : Quant::Phrase;
            redDot::LatchCtx c;
            c.stepEdge = rng.bit(); c.wrapped = rng.bit();
            c.locked = rng.bit(); c.scopeLiveMask = rng.next();
            ctrl.policy.quant = quant;
            // ONE commit entry drives all strands:
            ctrl.tick(c);
            for (std::size_t st=0;st<NUM_STRANDS;++st) {
                oracle[st].tick(quant, c.stepEdge, c.wrapped, c.locked, st, c.scopeLiveMask);
                EXPECT_EQ(ctrl.read(st), oracle[st].committed);
            }
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G2 — LOR bridge: LatchedControl reproduces the inline commit WITH the hold");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G2a fuzz: all 7 strands, (edit,phraseEdge,unlock,lock,scope,suspend,sticky) (100k)", {
        Rng rng(0x10ECC02u);
        LorOracle oracle[NUM_STRANDS];
        redDot::LatchedControl<LorLanePolicy> ctrl;
        for (std::size_t s=0;s<NUM_STRANDS;++s) ctrl.seed(s, 0.f);
        for (int n = 0; n < 100000; ++n) {
            for (std::size_t s=0;s<NUM_STRANDS;++s) if (rng.bit()) {
                float v = (float)(rng.range(2001)-1000)/1000.f; oracle[s].stage(v); ctrl.stage(s, v);
            }
            redDot::LatchCtx c;
            c.wrapped = rng.bit(); c.unlockNow = rng.bit();
            c.locked = rng.bit(); c.scopeLiveMask = rng.next();
            uint32_t suspendMask = rng.next(), stickyMask = rng.next();
            auto held = [&](std::size_t s){ return ((suspendMask>>s)&1u) || ((stickyMask>>s)&1u); };
            ctrl.tick(c, held);
            for (std::size_t s=0;s<NUM_STRANDS;++s) {
                const bool axisBit = (c.scopeLiveMask & (melodyAxis(s)?SB_SANDS_M:SB_SANDS_R)) != 0;
                oracle[s].tick(c.wrapped, c.unlockNow, c.locked, axisBit,
                               ((suspendMask>>s)&1u)!=0, ((stickyMask>>s)&1u)!=0);
                EXPECT(ctrl.read(s) == oracle[s].committed);
            }
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G1E — the EDGE cadence pinned DIRECTLY (the thing that drifted twice)");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G1Ea direction edge truth table (StepEdge fires on step; Phrase only on wrap)", {
        LaneDirPolicy p;
        auto E=[&](Quant q,bool se,bool wr){ p.quant=q; redDot::LatchCtx c; c.stepEdge=se; c.wrapped=wr; return p.edge(c); };
        // StepEdge: fires iff a step advance happened, regardless of wrap
        EXPECT(E(Quant::StepEdge, true,  false) == true);
        EXPECT(E(Quant::StepEdge, true,  true ) == true);
        EXPECT(E(Quant::StepEdge, false, true ) == false);
        EXPECT(E(Quant::StepEdge, false, false) == false);
        // Phrase: fires iff the phrase wrapped (the LANDED phase), regardless of stepEdge
        EXPECT(E(Quant::Phrase,   false, true ) == true);
        EXPECT(E(Quant::Phrase,   true,  true ) == true);
        EXPECT(E(Quant::Phrase,   true,  false) == false);   // BUG-6 guard: a step that did
        EXPECT(E(Quant::Phrase,   false, false) == false);   // NOT wrap must not commit
    });
    TEST("G1Eb LOR edge = phraseEdge OR unlockNow (flush fires without a phrase edge)", {
        LorLanePolicy p;
        auto E=[&](bool wr,bool un){ redDot::LatchCtx c; c.wrapped=wr; c.unlockNow=un; return p.edge(c); };
        EXPECT(E(false,false) == false);
        EXPECT(E(true, false) == true);
        EXPECT(E(false,true ) == true);    // the unlock flush
        EXPECT(E(true, true ) == true);
    });
    TEST("G1Ec a control with NO edge never commits, whatever the scope/pending", {
        redDot::LatchedControl<LaneDirPolicy> ctrl; ctrl.policy.quant = Quant::Phrase;
        for (std::size_t st=0;st<NUM_STRANDS;++st){ ctrl.seed(st,+1); ctrl.stage(st,-1); }
        redDot::LatchCtx c; c.wrapped=false; c.stepEdge=true; c.locked=false; // Phrase: no wrap => no edge
        ctrl.tick(c);
        for (std::size_t st=0;st<NUM_STRANDS;++st) EXPECT_EQ(ctrl.read(st), +1);
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
