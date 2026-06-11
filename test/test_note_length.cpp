/**
 * test_note_length.cpp — Note-length duration + PPQN gating, from source truth.
 *
 * Investigates: "no notes faster than 1/8 on the scope from the note-length dial
 * OR variation, at any PPQN (1/4/24)."
 *
 * Two independent tables drive note length, and they DISAGREE:
 *
 *   NOTEVALS[8]      (SequencerEngine.cpp — gating + matches the dial LABELS):
 *       idx 0..7 = 1/1, 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/32
 *   GS_NOTE_FRACS[8] (GateState.cpp — the ACTUAL hold duration):
 *       idx 0..7 = 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32
 *
 * They are OFFSET BY ONE: a given nvIdx is gated/labelled as one value but held
 * for the duration of the *next-longer* slot. So selecting "1/16" (idx 6) is
 * gated as 1/16 but actually held as GS_NOTE_FRACS[6] = 1/24, etc. The label and
 * the audible length never agree above 1/8.
 *
 * Compile:  g++ -std=c++17 -I. test_note_length.cpp -o t && ./t
 */
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

static int s_pass=0,s_fail=0;
#define SUITE(n) std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n"
#define TEST(desc,...) do{bool _ok=true;std::string _m;try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
  if(_ok){++s_pass;std::cout<<"  \033[32m\u2713\033[0m "<<(desc)<<"\n";}else{++s_fail;std::cout<<"  \033[31m\u2717\033[0m "<<(desc);if(!_m.empty())std::cout<<" \u2014 "<<_m;std::cout<<"\n";}}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a,b) do{if((a)!=(b)){std::ostringstream _s;_s<<#a<<"="<<(a)<<" != "<<#b<<"="<<(b);throw std::runtime_error(_s.str());}}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream _s;_s<<#a<<"="<<(a)<<" not near "<<#b;throw std::runtime_error(_s.str());}}while(0)

// ── Source-truth replicas ───────────────────────────────────────────────────
struct NoteVal { float frac; int allowedPPQN; };
// SequencerEngine.cpp NOTEVALS[8] (gating; index matches the dial labels)
static const NoteVal NOTEVALS[8] = {
    {1.0f,1|2|4},{0.5f,1|2|4},{0.25f,1|2|4},{1.0f/6.0f,4},
    {0.125f,2|4},{1.0f/12.0f,4},{0.0625f,2|4},{0.03125f,4},
};
// dial configSwitch labels (MonsoonConfigurator.cpp)
static const float LABEL_FRAC[8] = {
    1.0f, 0.5f, 0.25f, 1.0f/6.0f, 0.125f, 1.0f/12.0f, 0.0625f, 0.03125f
};
// GateState.cpp GS_NOTE_FRACS[8] (actual hold duration)
static const float GS_NOTE_FRACS[8] = {
    0.5f, 0.25f, 1.f/6.f, 0.125f, 1.f/12.f, 0.0625f, 1.f/24.f, 0.03125f
};
static float gs_noteSteps(int nvIdx){
    if(nvIdx<0||nvIdx>7) return 1.f;
    return std::max(0.5f, GS_NOTE_FRACS[nvIdx]*16.f);
}
static int clampi(int v,int lo,int hi){return v<lo?lo:(v>hi?hi:v);}
static int computeNoteLengthIdx(int req,int mask){int i=clampi(req,0,7);while(i>0&&!(NOTEVALS[i].allowedPPQN&mask))i--;return i;}
static int ppqnMaskFor(int s){return (s==1)?1:(s==4)?2:4;}

int main(){
    std::cout<<"\033[1mNote-length duration / PPQN tests\033[0m\n";

    SUITE("Dial label matches the GATING table (NOTEVALS) — OK side");
    for(int i=0;i<8;++i){std::ostringstream d;d<<"label idx "<<i<<" == NOTEVALS frac";
        TEST(d.str().c_str(),{EXPECT_NEAR(LABEL_FRAC[i],NOTEVALS[i].frac,1e-6f);});}

    SUITE("Dial label vs ACTUAL hold duration (GS_NOTE_FRACS) — THE BUG");
    // This is what the user hears. It should match the label; it does NOT,
    // because GS_NOTE_FRACS is offset one slot shorter-of-list than NOTEVALS.
    for(int i=0;i<8;++i){std::ostringstream d;d<<"label idx "<<i<<" hold-duration matches label";
        TEST(d.str().c_str(),{EXPECT_NEAR(LABEL_FRAC[i],GS_NOTE_FRACS[i],1e-6f);});}

    SUITE("The two tables are mutually misaligned (not a clean offset)");
    TEST("GS_NOTE_FRACS does NOT equal NOTEVALS at the same idx (the defect)",{
        int mismatches=0;
        for(int i=0;i<8;++i) if(std::fabs(GS_NOTE_FRACS[i]-NOTEVALS[i].frac)>1e-6f) ++mismatches;
        EXPECT(mismatches>=6);   // 7 of 8 differ -> audible length != labelled length
    });
    TEST("GS_NOTE_FRACS is not even a clean +1 shift of NOTEVALS",{
        // if it were a clean offset, GS[i]==NOTEVALS[i+1] for all i. It is not:
        // GS[6]=1/24 but NOTEVALS[7]=1/32. So the tables are independently wrong.
        EXPECT(std::fabs(GS_NOTE_FRACS[6]-NOTEVALS[7].frac)>1e-6f);
    });

    SUITE("Hold length in 1/16-steps (step-grid behaviour)");
    // holdRemain is in 1/16-step units; gs_noteSteps clamps to >=0.5.
    TEST("'1/16' label (idx6) holds <1 step so cannot repeat faster than the grid",{
        float steps = gs_noteSteps(6);          // GS_NOTE_FRACS[6]=1/24 -> 0.67 steps
        EXPECT(steps < 1.0f);
    });
    TEST("'1/32' label (idx7) clamps to the 0.5-step floor",{
        EXPECT_NEAR(gs_noteSteps(7),0.5f,1e-6f); // 0.03125*16=0.5
    });
    // The crux: new notes only start on 1/16 'sixteenthEdge' steps (Monsoon.cpp
    // mode A). And the duration table gives label "1/8" (idx4) a hold of 1.33
    // steps — MORE than one 1/16 grid slot — so 1/8 already spans >1 step and
    // nothing shorter can repeat faster than the grid. This is why 1/8 is the
    // audible floor on the scope at every PPQN.
    TEST("label '1/8' (idx4) holds MORE than one 1/16 step (1.33) — pins the floor",{
        EXPECT(gs_noteSteps(4) > 1.0f);          // 1/12*16 = 1.333
    });
    TEST("labels faster than 1/8 (idx5,6,7) hold <=1 step, so never repeat faster",{
        for(int i=5;i<=7;++i) EXPECT(gs_noteSteps(i) <= 1.0f + 1e-6f);
    });

    SUITE("PPQN gating reach (the dial's gated ceiling)");
    TEST("ppqn=1 caps at 1/4 (idx2)",{EXPECT_EQ(computeNoteLengthIdx(7,ppqnMaskFor(1)),2);});
    TEST("ppqn=4 caps at idx6 (label '1/16')",{EXPECT_EQ(computeNoteLengthIdx(7,ppqnMaskFor(4)),6);});
    TEST("ppqn=24 reaches idx7 (label '1/32')",{EXPECT_EQ(computeNoteLengthIdx(7,ppqnMaskFor(24)),7);});

    std::cout<<"\n\033[1mResults: \033[32m"<<s_pass<<" passed\033[0m, "
             <<(s_fail?"\033[31m":"")<<s_fail<<" failed\033[0m\n";
    std::cout<<"\nDIAGNOSIS: NOTEVALS (gating/labels) and GS_NOTE_FRACS (actual hold)\n"
               "are mutually misaligned, so audible note length never matches the\n"
               "dial above 1/8; and new notes only start on 1/16 step edges, so\n"
               "shorter durations shorten the gate but never raise the note rate.\n";
    return s_fail?1:0;
}
