#include "test_stubs.hpp"
/**
 * test_macro_cv_resolver_pin.cpp
 * CA3 (CONCEPT_ODR_AUDIT) — pin the verbatim-copied `applyMacroCV` EQUIVALENT to the
 * SandsModifierResolver it bypasses, so the copy cannot silently drift from the authority.
 *
 * WHAT CA3 IS. MonsoonSandsManager's `applyMacroCV` (self-documented "copied VERBATIM from
 * processDNA's local helper") computes the Macro lane-row's CV-applied read by hand:
 *     applyMacroCV = connected ? clamp(base + (cvVolts/10)*att*(hi-lo), lo, hi) : base
 * The same "Macro's CV-applied value" is also what the resolver produces on its Macro-own
 * path — `SandsModifierResolver<LorPolicy>::effective` with only `ownCv` set, no send, not
 * delegated: `constrain(base + addCv(scaleCv(ownCv.contribution())), {lo,hi})`. "Macro's
 * CV-applied value" is therefore defined TWICE. The real fix (route the callers through the
 * resolver) is an engine change; this is the audit's lighter option — PIN THE TWO
 * EQUIVALENT — which is safe, test-only, and catches a drift in EITHER definition.
 *
 * This binds the copy to the REAL resolver (included, not re-transcribed): the oracle is a
 * verbatim copy of `applyMacroCV`; the other side calls the live
 * `SandsModifierResolver<LorPolicy>::effective`. If someone changes the resolver's compose
 * (the constrain, the addCv, the order) OR the manager's copy, the bits diverge and this
 * fails — which is exactly the ODR drift CA3 names.
 *
 * Compile: see test/run_all.sh (the registry is the source of truth).
 */
#include "dsp/SandsModifierResolver.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
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
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)

using LorR = redDot::SandsModifierResolver<redDot::LorPolicy>;

// clamp exactly as the manager's lambda uses it (std::max/std::min — same as constrain).
static float mgrClamp(float v, float lo, float hi){ return std::max(lo, std::min(hi, v)); }

// ─────────────────────────────────────────────────────────────────────────────
// ORACLE — a VERBATIM transcription of MonsoonSandsManager::applyMacroCV
// (src/dsp/managers/MonsoonSandsManager.cpp:996-1001). cvVolts is the raw jack voltage;
// the lambda reads getVoltage()/10 then base + cv*att*(hi-lo), clamped; disconnected -> base.
// ─────────────────────────────────────────────────────────────────────────────
static float oracleApplyMacroCV(float base, float lo, float hi, bool connected,
                                float cvVolts, float att) {
    if (!connected) return base;
    float cv = cvVolts / 10.f;
    return mgrClamp(base + cv * att * (hi - lo), lo, hi);
}

// ─────────────────────────────────────────────────────────────────────────────
// THE RESOLVER SIDE — the REAL SandsModifierResolver<LorPolicy>::effective on its
// Macro-own path. The resolver owns the COMPOSE (skip-if-absent / add / end-clamp); the CV
// TERM is the port-read site's job (LorPolicy::scaleCv is identity — "LOR's CV math stays
// inside eastLorVal"). So the caller computes the term and hands it in as the contribution.
//
// FLOATING-POINT ORDER MATTERS (the finding): applyMacroCV multiplies (cv/10)*att*(hi-lo) in
// THAT order. CvTerm.contribution() is unitVoltage*atten, so to stay BIT-identical the
// port-read site must compute the whole ordered term and pass it as unitVoltage with atten=1
// — NOT split it as unitVoltage=(cv/10)*(hi-lo), atten=att, which reorders the multiply and
// moves the low bits. This models the byte-preserving feed the eventual refactor must use.
// ─────────────────────────────────────────────────────────────────────────────
static float resolverMacroCV(float base, float lo, float hi, bool connected,
                             float cvVolts, float att) {
    LorR::Inputs in;
    in.base = base;
    in.extra.lo = lo;
    in.extra.hi = hi;
    in.delegated = false;      // Macro publishing its own: no delegation
    in.macroPresent = false;   // no macro-send-to-self
    in.ownCv.connected   = connected;
    in.ownCv.unitVoltage = (cvVolts / 10.f) * att * (hi - lo);  // the full ordered term
    in.ownCv.atten       = 1.f;                                 // already applied above
    return LorR::effective(in);
}

struct Rng { uint64_t s; explicit Rng(uint64_t x):s(x){}
    uint32_t n(){ s=s*6364136223846793005ULL+1442695040888963407ULL; return (uint32_t)(s>>33);}
    bool bit(){return (n()&1u)!=0;} float uf(float a,float b){return a+(b-a)*((float)(n()%100001)/100000.f);} };

int main(){
    std::cout<<"\033[1mCA3 — applyMacroCV == resolver Macro-own path\033[0m\n";
    std::cout<<std::string(60,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G1 — the three real lane windows applyMacroCV is called with");
    // ─────────────────────────────────────────────────────────────────────────
    // The actual call sites (MonsoonSandsManager.cpp:1015-1017): LEN {1,kLaneLengthMax},
    // OFF {0,kLaneOffsetMax}, ROT {0, len-1}. Use representative bounds.
    struct Win { const char* name; float lo, hi; };
    const Win wins[] = { {"LEN", 1.f, 32.f}, {"OFF", 0.f, 31.f}, {"ROT", 0.f, 15.f} };
    TEST("G1a across all three windows, every (base,cv,att,connected): bit-identical", {
        Rng rng(0xCA3u);
        for (const Win& w : wins) {
            for (int k=0;k<60000;++k){
                float base = rng.uf(w.lo, w.hi);
                bool  conn = rng.bit();
                float cv   = rng.uf(-5.f, 5.f);     // the ±5V jack range
                float att  = rng.uf(-1.f, 1.f);     // attenuverter
                float orc  = oracleApplyMacroCV(base, w.lo, w.hi, conn, cv, att);
                float res  = resolverMacroCV   (base, w.lo, w.hi, conn, cv, att);
                EXPECT(orc == res);   // bit-exact: it is a copy vs the authority, never arithmetic drift
            }
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("G2 — the specific contract points");
    // ─────────────────────────────────────────────────────────────────────────
    TEST("G2a disconnected CV -> base, both sides (the skip-if-absent term)", {
        float orc = oracleApplyMacroCV(7.f, 1.f, 32.f, /*connected=*/false, 3.3f, 0.9f);
        float res = resolverMacroCV   (7.f, 1.f, 32.f, /*connected=*/false, 3.3f, 0.9f);
        EXPECT(orc == 7.f);           // base untouched
        EXPECT(res == 7.f);
    });
    TEST("G2b +5V, full atten: 16 + 0.5*31 = 31.5, within [1,32], both sides", {
        float orc = oracleApplyMacroCV(16.f, 1.f, 32.f, true, 5.f, 1.f);
        float res = resolverMacroCV   (16.f, 1.f, 32.f, true, 5.f, 1.f);
        EXPECT(orc == 31.5f);         // 16 + (5/10)*1*31 = 31.5 (no clamp — hi is 32)
        EXPECT(res == orc);
    });
    TEST("G2b2 +5V from a high base clamps at hi (the end-clamp bites)", {
        float orc = oracleApplyMacroCV(30.f, 1.f, 32.f, true, 5.f, 1.f);  // 30+15.5=45.5
        float res = resolverMacroCV   (30.f, 1.f, 32.f, true, 5.f, 1.f);
        EXPECT(orc == 32.f);
        EXPECT(res == orc);
    });
    TEST("G2c full -5V, full atten clamps at lo", {
        float orc = oracleApplyMacroCV(4.f, 1.f, 32.f, true, -5.f, 1.f);
        float res = resolverMacroCV   (4.f, 1.f, 32.f, true, -5.f, 1.f);
        EXPECT(orc == 1.f);           // 4 - 0.5*31 = -11.5 -> clamp lo 1
        EXPECT(res == orc);
    });
    TEST("G2d negative attenuverter inverts the CV, both sides agree", {
        Rng rng(0x9Eu);
        for (int k=0;k<20000;++k){
            float base=rng.uf(0.f,15.f), cv=rng.uf(-5.f,5.f), att=rng.uf(-1.f,0.f);
            EXPECT(oracleApplyMacroCV(base,0.f,15.f,true,cv,att)
                 == resolverMacroCV   (base,0.f,15.f,true,cv,att));
        }
    });

    std::cout<<"\n"<<std::string(60,'=')<<"\n";
    std::cout<<(s_fail==0?"\033[32m":"\033[31m")<<s_pass<<" passed, "<<s_fail
             <<" failed\033[0m\n";
    return s_fail==0 ? 0 : 1;
}

/* ── MUTATION LOG (to run once green) ─────────────────────────────────────────
 * The resolver is shared, so a mutant there is a real drift the pin must catch; a mutant in
 * the ORACLE models the manager's copy drifting. Each built to its own binary, verified.
 *   M1  resolver effective: drop the `if (ownCv.connected)` skip (always add)
 *         → SURVIVES — STRUCTURAL, verified by reading back: CvTerm::contribution() ALREADY
 *           returns 0 when !connected, so the outer guard is redundant — addCv(v, 0f) == v
 *           bit-exact, and G2a (disconnected -> base) still holds. The guard is a
 *           short-circuit, not a correctness gate; it would only matter if contribution()
 *           stopped checking connected, which its own definition pins.
 *   M2  resolver LorPolicy::constrain: max/min swapped (clamp inverts)
 *         → G2b/G2c + G1a FAIL.
 *   M3  oracle applyMacroCV: drop the *(hi-lo) span (the manager's copy "fixed" wrongly)
 *         → G1a FAILS (the copy diverges from the resolver — the drift CA3 warns of).
 *   M4  oracle applyMacroCV: cv = cvVolts (forget the /10)
 *         → G1a FAILS.
 * ─────────────────────────────────────────────────────────────────────────── */
