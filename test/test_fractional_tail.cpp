// Regression: a fractional sub-step tail (triplet) must not be cut by a new note.
// Bug: executeStep's MidNote guard was `holdRemain >= 1`; on a triplet's final
// partial step holdRemain<1 while the gate timer was still open, so a new note
// fired and 1/4T played as 1/8, 1/8T as 1/16. Fix: also hold while gateSecRemain>0.
#include <cstdio>
static int fails=0;
static void chk(bool c,const char*m){ if(!c){printf("FAIL: %s\n",m);++fails;} }

// Minimal replica of the guard interaction (holdRemain + gateSecRemain + tick + process)
struct G {
    float holdRemain=0, gateSecRemain=-1, curStepSec=0; bool gateHeld=false;
    void armGate(float d){ gateSecRemain=(curStepSec>0)?d*curStepSec:-1; }
    void tick(float ss){ if(ss>0)curStepSec=ss; if(holdRemain>0){holdRemain-=1; if(holdRemain<=0){holdRemain=0; if(gateSecRemain<0)gateHeld=false;}} }
    void trigger(float d){ gateHeld=true; holdRemain=d; armGate(d); }
    void process(float dt){ if(gateSecRemain>=0){ gateSecRemain-=dt; if(gateSecRemain<=0){gateSecRemain=-1; gateHeld=false;} } }
};
// the FIXED guard
static bool midNote(const G& g){ return g.holdRemain>=1.f || g.gateSecRemain>0.f; }

static float measure(float durSteps){
    const float SR=48000.f, ss=0.125f, dt=1.f/SR; G g;
    g.tick(ss); g.trigger(durSteps);
    float high=0;
    for(int step=0; step<8; ++step){
        for(int n=0;n<(int)(ss*SR);++n){ bool was=g.gateHeld; g.process(dt); if(was)high+=dt; }
        g.tick(ss);
        if(!midNote(g)) break;   // engine would fire a new note -> this note ends
    }
    return high/ss; // gate-high length in steps
}
#define NEAR(a,b) (((a)-(b))<0.02f && ((b)-(a))<0.02f)
int main(){
    chk(NEAR(measure(4.0f),     4.000f), "1/4 = 4 steps");
    chk(NEAR(measure(16.f/6.f), 2.667f), "1/4T = 2.667 steps (not cut to 2)");
    chk(NEAR(measure(2.0f),     2.000f), "1/8 = 2 steps");
    chk(NEAR(measure(16.f/12.f),1.333f), "1/8T = 1.333 steps (not cut to 1)");
    chk(NEAR(measure(1.0f),     1.000f), "1/16 = 1 step");
    chk(NEAR(measure(0.5f),     0.500f), "1/32 = 0.5 steps");
    printf(fails? "\n%d FAILED\n" : "\nALL PASS (6 note lengths exact)\n", fails);
    return fails?1:0;
}
