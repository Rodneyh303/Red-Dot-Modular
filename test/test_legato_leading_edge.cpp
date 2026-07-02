// test_legato_leading_edge.cpp
// Structural test of the leading-edge legato DECISION LOGIC (SequencerEngine::executeStep,
// step 2). Replica-style like test_fractional_tail: mirrors the exact branch logic so the
// truth-table can be checked without the full Rack engine. Verifies:
//   1. legatoLeadingEdge=false reproduces the current model (fresh roll at the join).
//   2. legatoLeadingEdge=true consumes the PREVIOUS note's onset commitment (prevSlur).
//   3. Rest cancels an optional slur (rest branch wins) — the slur-vs-rest default.
//   4. A fractional-length note can NEVER lead a legato (noteCanLeadLegato).
// NOTE: this proves the LOGIC, not the musical result — the audible behaviour still needs an
// in-Rack build + ear (engine-core, timing-dependent).
#include "NoteValues.hpp"
#include <cstdio>
static int fails=0;
static void chk(bool c,const char*m){ if(!c){printf("FAIL: %s\n",m);++fails;} }

// Decision enum mirror (subset relevant to the join).
enum Dec { NEWNOTE, LEGATO, TIE, LEGATOMAX, REST, MIDNOTE };

// Replica of the executeStep cascade's connect/rest/new decision at a STARTING step.
// Inputs mirror the real ones. Returns the decision + this note's slurForward for chaining.
struct Out { Dec dec; bool slurForward; };
static Out decide(bool legatoLeadingEdge,
                  bool prevSlur,            // previous note's onset commitment
                  float r_rest, float restProb,
                  float r_legato_tie, float legatoProb,
                  bool canRest,             // holdRemain<=0 && !hadTail
                  bool wasHeldOrTail,       // wasHeld || hadTail
                  bool samePitch,           // sem == lastSemitone
                  int  nvIdx)               // note length index (for lead eligibility)
{
    Out o{NEWNOTE,false};
    const bool legatoConnects = (legatoProb >= 0.999f)
        || (legatoLeadingEdge ? prevSlur : (r_legato_tie < legatoProb));

    if (legatoProb >= 0.999f) {
        o.dec = LEGATOMAX;
    } else if (r_rest < restProb) {
        o.dec = canRest ? REST : MIDNOTE;   // rest wins over legato (rest-cancels-slur)
    } else if (legatoConnects) {
        o.dec = (samePitch && wasHeldOrTail) ? TIE : LEGATO;
    } else {
        o.dec = NEWNOTE;
    }

    // This note's forward commitment (mirrors the step-1 instrument), used to chain.
    bool leStarting = (o.dec==NEWNOTE || o.dec==LEGATO || o.dec==LEGATOMAX);
    o.slurForward = leStarting && noteCanLeadLegato(nvIdx)
                 && (legatoProb>=0.999f || r_legato_tie < legatoProb);
    return o;
}

int main(){
    const int I16 = 6;   // 1/16 (integer step → can lead)   [NoteValues index]
    const int I8T = 5;   // 1/8T (fractional → cannot lead)
    const int I4  = 2;   // 1/4  (integer step → can lead)

    // Sanity: lead-eligibility predicate.
    chk( noteCanLeadLegato(I16), "1/16 can lead");
    chk( noteCanLeadLegato(I4),  "1/4 can lead");
    chk(!noteCanLeadLegato(I8T), "1/8T cannot lead");

    // 1. Toggle OFF reproduces current model: connection follows r_legato_tie<legatoProb,
    //    regardless of prevSlur.
    {
        // legato roll fires (0.1<0.8) → connect; prevSlur false shouldn't matter when OFF.
        Out o = decide(false, /*prevSlur*/false, /*r_rest*/1.f,0.f, /*r_leg*/0.1f,0.8f,
                       /*canRest*/true, /*wasHeldOrTail*/true, /*samePitch*/false, I16);
        chk(o.dec==LEGATO, "OFF: legato roll fires → Legato (prevSlur ignored)");
        // legato roll misses (0.9>0.8) → NewNote even if prevSlur true.
        Out o2 = decide(false, /*prevSlur*/true, 1.f,0.f, 0.9f,0.8f, true, true, false, I16);
        chk(o2.dec==NEWNOTE, "OFF: legato roll misses → NewNote (prevSlur ignored)");
    }

    // 2. Toggle ON consumes prevSlur, ignoring this step's r_legato_tie.
    {
        // prevSlur true → connect even though this step's roll would MISS (0.9>0.8).
        Out o = decide(true, /*prevSlur*/true, 1.f,0.f, /*r_leg*/0.9f,0.8f,
                       true, true, false, I16);
        chk(o.dec==LEGATO, "ON: prevSlur true → Legato (this-step roll ignored)");
        // prevSlur false → NewNote even though this step's roll would FIRE (0.1<0.8).
        Out o2 = decide(true, /*prevSlur*/false, 1.f,0.f, 0.1f,0.8f, true, true, false, I16);
        chk(o2.dec==NEWNOTE, "ON: prevSlur false → NewNote (this-step roll ignored)");
        // prevSlur true, same pitch, was held → Tie.
        Out o3 = decide(true, true, 1.f,0.f, 0.9f,0.8f, true, true, /*samePitch*/true, I16);
        chk(o3.dec==TIE, "ON: prevSlur true + same pitch + held → Tie");
    }

    // 3. Rest cancels slur: prevSlur true but a rest rolls (canRest) → Rest, not Legato.
    {
        Out o = decide(true, /*prevSlur*/true, /*r_rest*/0.1f,/*restProb*/0.8f,
                       0.9f,0.8f, /*canRest*/true, true, false, I16);
        chk(o.dec==REST, "ON: rest roll cancels slur → Rest (slur-vs-rest default)");
        // but a fractional TAIL still outranks rest (canRest=false) → MidNote, not Rest.
        Out o2 = decide(true, true, 0.1f,0.8f, 0.9f,0.8f, /*canRest*/false, true, false, I16);
        chk(o2.dec==MIDNOTE, "ON: fractional tail outranks rest → MidNote (tail>rest kept)");
    }

    // 4. A fractional-length note cannot LEAD: its slurForward is false even if it plays as a
    //    starting note with the legato roll firing.
    {
        Out o = decide(false, false, 1.f,0.f, /*r_leg fires*/0.1f,0.8f, true, false, false, I8T);
        // decision may be Legato (it can be a target), but as a LEAD its slurForward must be false.
        chk(o.slurForward==false, "1/8T: cannot lead → slurForward false even when playing");
        Out o2 = decide(false, false, 1.f,0.f, 0.1f,0.8f, true, false, false, I16);
        chk(o2.slurForward==true, "1/16: can lead → slurForward true when legato roll fires");
    }

    // 5. LegatoMax forces connection in BOTH models.
    {
        Out off = decide(false, false, 1.f,0.f, 0.9f,/*legatoProb*/1.0f, true, true, false, I16);
        Out on  = decide(true,  false, 1.f,0.f, 0.9f,1.0f, true, true, false, I16);
        chk(off.dec==LEGATOMAX && on.dec==LEGATOMAX, "LegatoMax forces connect in both models");
    }

    printf(fails? "\n%d FAILED\n" : "\nALL PASS (leading-edge decision logic)\n", fails);
    return fails?1:0;
}
