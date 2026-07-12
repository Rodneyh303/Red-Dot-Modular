// EAST_EXTRA_LANES Stage 3 — Rule 2 CONSUME (step 3): the poly landing consumes
// each voice's own forward-slur commitment.
//
// This models the CONSUME half only; the ROLL that produces prevSlur is tested in
// test_poly_slur_roll. Here prevSlur is INJECTED per landing so the consume logic
// is isolated. The model mirrors the branch structure this step adds to
// executePolyVoice, and is the source of truth the engine port must match.
//
// Governing predicate (§4d): at a landing (mono Legato/LegatoMax/Tie) a PARTICIPATING
// voice CONNECTS iff its own prevSlur fired; otherwise it RE-ARTICULATES a fresh note
// (clamped ≤ mono, handled elsewhere). A voice that RESTED at the chain onset is not
// participating and stays silent. `participating` — not the gate-held flag — is what
// distinguishes a rester from a voice that opted out and let its short note close.
//
// Proves:
//   1. perVoiceArticulation OFF → unchanged follow-mono (connect at every landing).
//   2. ON + delegated (prevSlur==mono's, i.e. true at each real landing) ≡ OFF.
//   3. ON + Local East with prevSlur false at some landings → re-articulates there.
//   4. An opted-out voice whose short note CLOSED still re-articulates next landing
//      (participating, not gateHeld, drives it) — the reason the latch is explicit.
//   5. A rested voice stays silent through the whole chain and re-latches on NewNote.
//
// Build: g++ -std=c++17 test/test_rule2_consume.cpp
#include <cstdio>
#include <string>

static int passed = 0, failed = 0;
static void check(bool ok, const char* what) {
    if (ok) ++passed; else { ++failed; std::printf("  FAIL: %s\n", what); }
}

enum class Mono { NewNote, Legato, Tie, MidNote, Rest };

struct V {
    bool gateHeld = false;
    bool participating = false;
    char last = '?';                 // last event, for the log
    // A landing re-articulate produces a note that may CLOSE before the next edge.
    // The test drives that via closeGate(); the engine models it with the pulse timer.
    void trigger() { gateHeld = true;  last = 'T'; }   // fresh onset (retrigger)
    void connect() { gateHeld = true;  last = 'C'; }   // slide / extendHold (no retrigger)
    void silent()  { gateHeld = false; last = '.'; }
    void hold()    {                   last = '-'; }   // MidNote: gate unchanged
};

// One edge. pva = perVoiceArticulation. plays = at a NewNote chain onset, this voice
// rolled to play (vs rest). prevSlur = this voice's own commitment consumed at a landing.
// held = the voice's own gate actually held into this landing (wasHeldPoly || hadPolyTail);
// a slur/tie can only connect into a held gate, else it re-articulates (no slur across a rest).
static void step(V& v, Mono m, bool pva, bool plays, bool prevSlur, bool held = true) {
    switch (m) {
        case Mono::NewNote:
            if (plays) { v.participating = true;  v.trigger(); }
            else       { v.participating = false; v.silent();  }
            return;
        case Mono::Rest:
            v.participating = false; v.silent(); return;   // chain ends
        case Mono::MidNote:
            v.hold(); return;                              // no edge decision
        case Mono::Legato:
        case Mono::Tie:
            // LANDING.
            if (!pva) {                                    // follow-mono (unchanged)
                if (v.participating) v.connect(); else v.silent();
                return;
            }
            if (!v.participating) { v.silent(); return; }  // rested → stay out
            if (prevSlur && held) v.connect();             // committed AND gate survived → connect
            else                  v.trigger();             // opted out, OR gate closed → re-articulate
            return;
    }
}

// Run a chain, return the event log. plays applies at each NewNote; prevSlurAt(i)
// gives the injected commitment consumed at landing step i.
template <class SlurFn>
static std::string run(const Mono* chain, int n, bool pva, bool plays, SlurFn prevSlurAt) {
    V v; std::string log;
    for (int i = 0; i < n; ++i) {
        step(v, chain[i], pva, plays, prevSlurAt(i));
        log += v.last;
    }
    return log;
}

int main() {
    std::printf("\033[1mRule 2 consume (participating + prevSlur)\033[0m\n");
    std::printf("%s\n", std::string(46, '=').c_str());

    // A mono chain: onset, three slurred landings, a mid-note hold, then rest, then a
    // fresh onset. (Legato/Tie are the landings that consume prevSlur.)
    const Mono chain[] = {
        Mono::NewNote, Mono::Legato, Mono::Tie, Mono::Legato,
        Mono::MidNote, Mono::Rest, Mono::NewNote, Mono::Legato
    };
    const int N = sizeof(chain)/sizeof(chain[0]);

    auto allTrue  = [](int){ return true; };
    auto allFalse = [](int){ return false; };

    // 1. OFF: playing voice follows mono — connect at every landing, hold on MidNote,
    //    silent on Rest, retrigger on NewNote.
    std::string off = run(chain, N, /*pva=*/false, /*plays=*/true, allTrue);
    check(off == "TCCC-.TC", "OFF: T C C C - . T C (connect at every landing)");

    // 2. ON + delegated: prevSlur true at every real landing (delegated ⇒ matches mono,
    //    which connected here) ⇒ identical to OFF.
    std::string onDeleg = run(chain, N, /*pva=*/true, /*plays=*/true, allTrue);
    check(onDeleg == off, "ON+delegated == OFF (follow-mono guarantee)");

    // 3. ON + Local East: prevSlur FALSE at the landings ⇒ re-articulate (T) instead of
    //    connect (C) at each landing; onset/hold/rest unchanged.
    std::string onLocal = run(chain, N, /*pva=*/true, /*plays=*/true, allFalse);
    check(onLocal == "TTTT-.TT", "ON+LocalEast (no slur): re-articulates every landing");

    // 4. Opted-out voice whose short note CLOSED still re-articulates next landing.
    //    Model: gateHeld is irrelevant to the decision — force it false before a landing
    //    and confirm the voice still re-articulates (participating drives it).
    {
        V v; v.participating = true; v.gateHeld = true;
        step(v, Mono::Legato, true, true, /*prevSlur=*/false);  // opt out → re-articulate
        check(v.last == 'T' && v.gateHeld, "opt-out re-articulates (fresh gate)");
        v.gateHeld = false;                                     // its short note closed
        step(v, Mono::Legato, true, true, /*prevSlur=*/false);  // next landing
        check(v.last == 'T', "closed-gate opt-out STILL re-articulates (participating, not gateHeld)");
    }

    // 5. A rested voice stays silent through the chain, and a NewNote re-latches it.
    std::string rested = run(chain, N, /*pva=*/true, /*plays=*/false, allTrue);
    //  NewNote(rest→'.') Legato(not part→'.') Tie('.') Legato('.') MidNote('-') Rest('.')
    //  NewNote(rest→'.') Legato('.')   — plays=false throughout, so every onset rests.
    check(rested == "....-...", "rested voice: silent all chain (plays=false)");
    //  and if it PLAYS at the second onset only:
    {
        // step through manually: rest first onset, play the second.
        V v; std::string log;
        bool plays[N]     = {false,false,false,false,false,false,true,false};
        for (int i = 0; i < N; ++i) { step(v, chain[i], true, plays[i], /*prevSlur=*/true); log += v.last; }
        check(log == "....-.TC", "re-latches: silent until it plays a NewNote, then participates");
    }

    // 6. A voice that LED a slur (prevSlur=true) but whose short note CLOSED before the
    //    landing (held=false — a rest/gap) must RE-ARTICULATE, not connect: no slur across a
    //    rest, mirroring mono's "legato/tie requires the previous note still sounding". This is
    //    the case where a poly 1/16 lead + 1/16 rest was wrongly showing a teal/violet continuation.
    {
        V v; v.participating = true;
        step(v, Mono::NewNote, true, true, /*prevSlur=*/true);     // leads a slur
        check(v.last == 'T', "slur lead fires (trigger)");
        // its short note closed before the landing → held=false, even though prevSlur=true:
        step(v, Mono::Legato, true, true, /*prevSlur=*/true, /*held=*/false);
        check(v.last == 'T', "slur lead + closed gate (rest) -> RE-ARTICULATE, not connect");
        // contrast: same commitment but the gate DID survive → connect
        V v2; v2.participating = true;
        step(v2, Mono::NewNote, true, true, /*prevSlur=*/true);
        step(v2, Mono::Legato, true, true, /*prevSlur=*/true, /*held=*/true);
        check(v2.last == 'C', "slur lead + held gate -> CONNECT");
    }

    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
