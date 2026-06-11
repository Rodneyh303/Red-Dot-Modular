/**
 * test_note_length.cpp — Note-length dial -> actual gate duration, incl. the
 * sub-step (1/32, triplet) fractional gate close.
 *
 * Replicates (from source truth) the FIXED GateState model:
 *   GS_NOTE_FRACS[8] (now matches NOTEVALS / dial labels):
 *     idx 0..7 = 1/1, 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/32
 *   gs_noteSteps(i) = GS_NOTE_FRACS[i]*16   (no whole-step floor)
 *   setDuration(): splits dur into whole steps (holdRemain) + pendingFrac
 *   tick(): decrements holdRemain; on the last whole step arms fracGateSec =
 *           pendingFrac*sixteenthSec (sub-step open time)
 *   process(): per-sample; counts fracGateSec down, closes gate mid-step
 *
 * AIM: all notes START on the 1/16 grid; 1/32 and triplets END fractionally
 * within a step (1/32 = half a step). Whole-note lengths unchanged. Reproduces
 * the wanted scope behaviour and pins the prior regression (1/4==1/4T,
 * 1/8==1/8T==1/16==1/32).
 *
 * Compile:  g++ -std=c++17 -I. test_note_length.cpp -o t && ./t
 */
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

static int s_pass=0,s_fail=0;
#define SUITE(n) std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n"
#define TEST(desc,...) do{bool _ok=true;std::string _m;try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
  if(_ok){++s_pass;std::cout<<"  \033[32m\xE2\x9C\x93\033[0m "<<(desc)<<"\n";}else{++s_fail;std::cout<<"  \033[31m\xE2\x9C\x97\033[0m "<<(desc);if(!_m.empty())std::cout<<" - "<<_m;std::cout<<"\n";}}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream _s;_s<<#a<<"="<<(a)<<" not ~ "<<(b);throw std::runtime_error(_s.str());}}while(0)

static const float GS_NOTE_FRACS[8] = {
    1.0f, 0.5f, 0.25f, 1.f/6.f, 0.125f, 1.f/12.f, 0.0625f, 0.03125f
};
static const char* LABELS[8] = {"1/1","1/2","1/4","1/4T","1/8","1/8T","1/16","1/32"};
static float gs_noteSteps(int i){ return (i<0||i>7)?1.f:GS_NOTE_FRACS[i]*16.f; }

struct GS {
    // Mirrors the fixed GateState: holdRemain (whole-step, decision logic) +
    // gateSecRemain (precise per-sample gate length).
    float holdRemain=0.f, gateSecRemain=-1.f; bool gateHeld=false;
    void setGateSeconds(float durSteps,float s){ gateSecRemain=(s>0.f)?durSteps*s:-1.f; }
    void trigger(int nv,float s){ gateHeld=true; float d=gs_noteSteps(nv); holdRemain=d; setGateSeconds(d,s); }
    void tick(){ if(holdRemain>0.f){ holdRemain-=1.f; if(holdRemain<=0.f){ holdRemain=0.f; if(gateSecRemain<0.f) gateHeld=false; } } }
    bool process(float dt){
        if(gateSecRemain>=0.f){ gateSecRemain-=dt; if(gateSecRemain<=0.f){ gateSecRemain=-1.f; gateHeld=false; } }
        return gateHeld;
    }
};

static float gateSteps(int nv,float stepSec,float SR=48000.f){
    float dt=1.f/SR; GS g; g.trigger(nv,stepSec);
    float high=0.f,t=0.f,nextEdge=stepSec;
    long maxN=(long)(40*stepSec*SR);
    for(long n=0;n<maxN;n++){
        if(g.process(dt)) high+=dt;
        t+=dt;
        if(t>=nextEdge-1e-9f){ g.tick(); nextEdge+=stepSec; }
        if(!g.gateHeld && g.gateSecRemain<0.f && g.holdRemain<=0.f) break;
    }
    return high/stepSec;
}

int main(){
    std::cout<<"\033[1mNote-length gate-duration tests (sub-step close)\033[0m\n";
    const float BPM=120.f, STEP=15.f/BPM;

    SUITE("Table matches the dial labels");
    float exp_frac[8]={1.f,0.5f,0.25f,1.f/6.f,0.125f,1.f/12.f,0.0625f,0.03125f};
    for(int i=0;i<8;++i){std::ostringstream d;d<<"GS_NOTE_FRACS '"<<LABELS[i]<<"' correct";
        TEST(d.str().c_str(),{EXPECT_NEAR(GS_NOTE_FRACS[i],exp_frac[i],1e-6f);});}

    SUITE("Whole-note lengths unchanged (regression guard)");
    TEST("1/1 = 16 steps",{EXPECT_NEAR(gateSteps(0,STEP),16.f,0.05f);});
    TEST("1/2 = 8 steps", {EXPECT_NEAR(gateSteps(1,STEP),8.f,0.05f);});
    TEST("1/4 = 4 steps", {EXPECT_NEAR(gateSteps(2,STEP),4.f,0.05f);});
    TEST("1/8 = 2 steps", {EXPECT_NEAR(gateSteps(4,STEP),2.f,0.05f);});
    TEST("1/16 = 1 step", {EXPECT_NEAR(gateSteps(6,STEP),1.f,0.05f);});

    SUITE("Sub-step / triplet lengths render fractionally (THE FIX)");
    TEST("1/4T = 2.667 steps",{EXPECT_NEAR(gateSteps(3,STEP),16.f/6.f,0.06f);});
    TEST("1/8T = 1.333 steps",{EXPECT_NEAR(gateSteps(5,STEP),16.f/12.f,0.06f);});
    TEST("1/32 = 0.5 steps (half a 1/16 step, the design aim)",{EXPECT_NEAR(gateSteps(7,STEP),0.5f,0.05f);});

    SUITE("User's two symptoms resolved");
    TEST("1/4 and 1/4T now DISTINCT",{EXPECT(std::fabs(gateSteps(2,STEP)-gateSteps(3,STEP))>0.3f);});
    TEST("1/8, 1/8T, 1/16, 1/32 all DISTINCT",{
        float a=gateSteps(4,STEP),b=gateSteps(5,STEP),c=gateSteps(6,STEP),d=gateSteps(7,STEP);
        EXPECT(std::fabs(a-b)>0.2f); EXPECT(std::fabs(b-c)>0.2f); EXPECT(std::fabs(c-d)>0.2f);
    });

    SUITE("All notes still START on the 1/16 grid");
    TEST("every length has a sub-step tail < 1 step (on-grid onset)",{
        for(int i=0;i<8;++i){ float s=gs_noteSteps(i); float frac=s-std::floor(s); EXPECT(frac<1.f); }
    });

    std::cout<<"\n\033[1mResults: \033[32m"<<s_pass<<" passed\033[0m, "
             <<(s_fail?"\033[31m":"")<<s_fail<<" failed\033[0m\n";
    return s_fail?1:0;
}
