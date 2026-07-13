// test_legato_leading_edge.cpp
// Structural test of the (now sole) leading-edge legato DECISION LOGIC
// (SequencerEngine::executeStep, step 2). Replica-style like test_fractional_tail: mirrors
// the exact branch logic so the truth-table can be checked without the full Rack engine.
// Verifies:
//   1. The connect decision consumes the PREVIOUS note's onset commitment (prevSlur); the
//      joining onset does NOT re-roll. r_legato_tie is consumed only at the LEAD commitment.
//   2. Rest cancels an optional slur (rest branch wins) — the slur-vs-rest default.
//   3. A fractional-length note can NEVER lead a legato (noteCanLeadLegato).
//   4. LegatoMax forces connection.
//   5. 3+ note chains continue only through a lead-eligible (grid-aligned) note.
//   6. isolated-teal regression: prevSlur true but no held predecessor → NewNote.
//   7. "Rest beats legato" toggle.
// The reactive model (fresh roll at the join) and its legatoLeadingEdge toggle were removed;
// leading-edge is unconditional, so decide() no longer takes a mode flag.
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
static Out decide(bool prevSlur,             // previous note's onset commitment
                  float r_rest, float restProb,
                  float r_legato_tie, float legatoProb,
                  bool canRest,              // holdRemain<=0 && !hadTail
                  bool wasHeldOrTail,        // wasHeld || hadTail
                  bool samePitch,            // sem == lastSemitone
                  int  nvIdx,                // note length index (for lead eligibility)
                  bool restBeatsLegato = true)  // toggle: rest cancels slur (default) vs slur wins
{
    Out o{NEWNOTE,false};
    // Leading-edge is the only model: the join follows prevSlur, never a fresh roll.
    const bool legatoConnects = (legatoProb >= 0.999f) || prevSlur;
    const bool slurReachesHere    = legatoConnects && wasHeldOrTail;
    const bool slurSuppressesRest = !restBeatsLegato && slurReachesHere;

    if (legatoProb >= 0.999f) {
        o.dec = LEGATOMAX;
    } else if ((r_rest < restProb) && !slurSuppressesRest) {
        o.dec = canRest ? REST : MIDNOTE;   // rest wins (unless a committed slur suppresses it)
    } else if (legatoConnects && wasHeldOrTail) {
        // A slur can only CONNECT if the previous note actually held into this step. Intent
        // (prevSlur) without a held predecessor → NewNote (no slide-into-nothing).
        o.dec = samePitch ? TIE : LEGATO;
    } else {
        o.dec = NEWNOTE;
    }

    // This note's forward commitment (mirrors the LEAD instrument), used to chain. r_legato_tie
    // is consumed ONLY here — the sole roll, at the lead. Tie is included: a same-pitch
    // continuation is a sounding note that can carry a 3+ note chain onward (if grid-aligned
    // and it rolls forward). Matches the engine's leStarting.
    bool leStarting = (o.dec==NEWNOTE || o.dec==LEGATO || o.dec==LEGATOMAX || o.dec==TIE);
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

    // 1. The join consumes prevSlur and ignores this step's r_legato_tie.
    {
        // prevSlur true → connect even though this step's roll would MISS (0.9>0.8).
        Out o = decide(/*prevSlur*/true, 1.f,0.f, /*r_leg*/0.9f,0.8f,
                       true, true, false, I16);
        chk(o.dec==LEGATO, "prevSlur true → Legato (this-step roll ignored)");
        // prevSlur false → NewNote even though this step's roll would FIRE (0.1<0.8).
        Out o2 = decide(/*prevSlur*/false, 1.f,0.f, 0.1f,0.8f, true, true, false, I16);
        chk(o2.dec==NEWNOTE, "prevSlur false → NewNote (this-step roll ignored)");
        // prevSlur true, same pitch, was held → Tie.
        Out o3 = decide(true, 1.f,0.f, 0.9f,0.8f, true, true, /*samePitch*/true, I16);
        chk(o3.dec==TIE, "prevSlur true + same pitch + held → Tie");
    }

    // 2. Rest cancels slur: prevSlur true but a rest rolls (canRest) → Rest, not Legato.
    {
        Out o = decide(/*prevSlur*/true, /*r_rest*/0.1f,/*restProb*/0.8f,
                       0.9f,0.8f, /*canRest*/true, true, false, I16);
        chk(o.dec==REST, "rest roll cancels slur → Rest (slur-vs-rest default)");
        // but a fractional TAIL still outranks rest (canRest=false) → MidNote, not Rest.
        Out o2 = decide(true, 0.1f,0.8f, 0.9f,0.8f, /*canRest*/false, true, false, I16);
        chk(o2.dec==MIDNOTE, "fractional tail outranks rest → MidNote (tail>rest kept)");
    }

    // 3. A fractional-length note cannot LEAD: its slurForward is false even if it plays as a
    //    starting note with the legato roll firing.
    {
        Out o = decide(false, 1.f,0.f, /*r_leg fires*/0.1f,0.8f, true, false, false, I8T);
        chk(o.slurForward==false, "1/8T: cannot lead → slurForward false even when playing");
        Out o2 = decide(false, 1.f,0.f, 0.1f,0.8f, true, false, false, I16);
        chk(o2.slurForward==true, "1/16: can lead → slurForward true when legato roll fires");
    }

    // 4. LegatoMax forces connection.
    {
        Out o = decide(false, 1.f,0.f, 0.9f,/*legatoProb*/1.0f, true, true, false, I16);
        chk(o.dec==LEGATOMAX, "LegatoMax forces connect");
    }

    // 5. 3+ note chain: the chain continues past note 2 only if note 2 is itself lead-eligible
    //    (grid-aligned). Simulate note2 as a Tie that rolls legato:
    //    - note2 is 1/16 (can lead) + rolls forward → its slurForward=true → note3 connects.
    //    - note2 is 1/8T (cannot lead) → its slurForward=false → chain BREAKS, note3 = NewNote.
    {
        // note2 arrives as a Tie (prevSlur from note1 true, same pitch, held), 1/16, rolls fwd.
        Out note2 = decide(/*prevSlur*/true, 1.f,0.f, /*r_leg fires*/0.1f,0.8f,
                           /*canRest*/false, /*wasHeldOrTail*/true, /*samePitch*/true, I16);
        chk(note2.dec==TIE, "chain: note2 is a Tie");
        chk(note2.slurForward==true, "chain: grid-aligned Tie CAN lead onward (slurForward true)");
        // note3 consumes note2's slurForward → connects.
        Out note3 = decide(/*prevSlur=note2*/note2.slurForward, 1.f,0.f, 0.9f,0.8f,
                           false, true, true, I16);
        chk(note3.dec==TIE, "chain: note3 connects (chain continues through eligible note2)");

        // Now note2 is a fractional Tie (1/8T) — cannot lead onward.
        Out note2f = decide(true, 1.f,0.f, 0.1f,0.8f, false, true, true, I8T);
        chk(note2f.dec==TIE, "chain: fractional note2 still plays as Tie (valid destination)");
        chk(note2f.slurForward==false, "chain: fractional Tie CANNOT lead onward (slurForward false)");
        // note3 sees prevSlur=false → chain breaks, NewNote.
        Out note3b = decide(/*prevSlur=note2f*/note2f.slurForward, 1.f,0.f, 0.1f,0.8f,
                            true, false, false, I16);
        chk(note3b.dec==NEWNOTE, "chain: breaks at fractional note2 → note3 is NewNote");
    }

    // 6. REGRESSION (isolated-teal bug from the build): prevSlur true but the predecessor left
    //    NOTHING held (wasHeldOrTail=false) → must NOT slide into nothing. NewNote, not Legato.
    //    This is the bug the Lantern screenshot showed: teal cells with no predecessor.
    {
        Out o = decide(/*prevSlur*/true, /*r_rest*/1.f,0.f, 0.9f,0.8f,
                       /*canRest*/true, /*wasHeldOrTail*/false, /*samePitch*/false, I16);
        chk(o.dec==NEWNOTE, "isolated-slur: prevSlur true but nothing held → NewNote (not Legato)");
        Out o2 = decide(true, 1.f,0.f, 0.9f,0.8f, true, /*wasHeldOrTail*/true, false, I16);
        chk(o2.dec==LEGATO, "held predecessor + prevSlur → Legato (a real connection)");
    }

    // 7. "Rest beats legato" toggle.
    {
        // A committed slur reaches here (prevSlur, held predecessor) AND a rest rolls.
        // ON (default): rest wins → Rest.
        Out on = decide(/*prevSlur*/true, /*r_rest*/0.1f,/*restProb*/0.8f, 0.9f,0.8f,
                        /*canRest*/true, /*wasHeldOrTail*/true, /*samePitch*/false, I16,
                        /*restBeatsLegato*/true);
        chk(on.dec==REST, "rest-beats-legato ON: committed slur + rest → Rest (rest wins)");
        // OFF: slur wins → the rest is ignored, note plays as Legato.
        Out off = decide(true, 0.1f,0.8f, 0.9f,0.8f, true, true, false, I16,
                         /*restBeatsLegato*/false);
        chk(off.dec==LEGATO, "rest-beats-legato OFF: committed slur + rest → Legato (slur wins)");
        // OFF but NO committed slur reaching here (wasHeldOrTail=false): rest still wins (the
        // toggle only suppresses the rest when a genuine slur reaches the note).
        Out off2 = decide(true, 0.1f,0.8f, 0.9f,0.8f, true, /*wasHeldOrTail*/false, false, I16,
                          /*restBeatsLegato*/false);
        chk(off2.dec==REST, "rest-beats-legato OFF: no held predecessor → rest still wins");
    }

    printf(fails? "\n%d FAILED\n" : "\nALL PASS (leading-edge decision logic)\n", fails);
    return fails?1:0;
}
