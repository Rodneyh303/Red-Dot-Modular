// test/test_played_column_consumers.cpp — DESIGN_POSITION_UNIFICATION SLICE 3
// (CC_PROMPT_POSITION_RESOLVER_SLICE3_CONSUMERS.md): the CONSUMER-migration structural pin.
//
// THE LAW: the position CONSUMERS read pos_.playedColumn() (the ONE position authority's
// played column) and no longer read the bare stepIndex field; the AUTHORITY sites (the
// traversal's own working variable) keep theirs. test_position_resolver_identity pins
// playedColumn() == stepIndex at EVERY step (every window, direction, re-author, the
// phase snap), so the migration is BIT-IDENTICAL — THIS suite is the headless guard that
// a regression cannot silently re-introduce the field read at a consumer site (or strip
// the working variable from an authority site, which would be circular: pos_ advances
// ALONGSIDE stepIndex inside those functions).
//
// MIGRATED (consumers — pinned here): getStepLightBrightness, getBarLightBrightness,
// shouldTriggerStep, executeStep (result.forStep — the MidNote early-return AND the
// tail), MonsoonModeController::updateLastStepIndex, Monsoon's lock tick feed,
// MonsoonOutputGenerator's mask step (the two Rodney rulings: the lock feed is a
// one-token transport change, not a widget read; the mask read fits the consumer
// definition). LEFT (authority — pinned as still holding the working variable):
// setWindow, advancePlayhead, snapToPhaseStep (+ the gate/wander seed checks, not
// separately pinned). NOT HERE (Rodney's Rack leg, step 3b): the widget reads
// (Intertropical, Lantern, the Sands visuals, VisualExpanderHelpers, LanternRecordCore,
// Monsoon's remaining transport-internal reads, the Monsoon.hpp reference alias).
//
// SLICE 4 (CC_PROMPT_POSITION_RESOLVER_SLICE4_UNIFY_ADVANCE): the ADVANCE unification —
// GATE 5 pins stepIndex DERIVED from pos_ at the three authority sites (the hand walk
// and re-bound deleted, wrapped from pos_.phrasePos() == 0, the snap's landing a local).
//
// Reproduce-first: every MIGRATED gate FAILS on the pre-slice engine (the bodies read
// stepIndex); the AUTHORITY gates pass before and after. GATE 5 FAILS on the pre-slice-4
// engine (the hand walk is still there, the derives are not).
#include <cstdio>
#include <cctype>
#include <string>

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const std::string& what) {
    if (ok) { ++g_pass; }
    else    { ++g_fail; std::printf("  FAIL: %s\n", what.c_str()); }
}

static bool slurpFile(const char* path, std::string& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char buf[4096];
    size_t n;
    out.clear();
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

// Strip // and /* */ comments (the house test_range_strip_gesture precedent).
static std::string stripComments(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool line = false, block = false;
    for (size_t i = 0; i < src.size(); ++i) {
        const char c   = src[i];
        const char nxt = (i + 1 < src.size()) ? src[i + 1] : '\0';
        if (!line && !block && c == '/' && nxt == '/') { line = true;  out += ' ';        continue; }
        if (!line && !block && c == '/' && nxt == '*') { block = true; out += ' '; ++i;   continue; }
        if (line && c == '\n')                          { line = false; out += '\n';      continue; }
        if (block && c == '*' && nxt == '/')            { block = false; out += ' '; ++i; continue; }
        if (!line && !block) out += c;
    }
    return out;
}

// Blank string/char literals (a token count must not see inside them).
static std::string stripStrings(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool inStr = false, inChr = false;
    for (size_t i = 0; i < src.size(); ++i) {
        const char c = src[i];
        if (inStr || inChr) {
            if (c == '\\' && i + 1 < src.size()) { out += "  "; ++i; continue; }
            if ((inStr && c == '"') || (inChr && c == '\'')) inStr = inChr = false;
            out += ' ';
            continue;
        }
        if (c == '"')  { inStr = true; out += ' '; continue; }
        if (c == '\'') { inChr = true; out += ' '; continue; }
        out += c;
    }
    return out;
}

// Word-boundary token count (the 16_triage grep-pin precedent: `stepIndex` must not match
// inside lastStepIndex / cachedMelodyStepIndex — the capital-S spellings differ, and an
// identifier-adjacent hit is a different name).
static int countToken(const std::string& hay, const std::string& tok) {
    int n = 0;
    size_t p = 0;
    while ((p = hay.find(tok, p)) != std::string::npos) {
        const bool  lo = (p == 0) || !std::isalnum((unsigned char)hay[p - 1]);
        const size_t e = p + tok.size();
        const bool  hi = (e >= hay.size()) || !std::isalnum((unsigned char)hay[e]);
        if (lo && hi) ++n;
        p = e;
    }
    return n;
}

static int countOccurrences(const std::string& hay, const std::string& needle) {
    int n = 0;
    size_t p = 0;
    while ((p = hay.find(needle, p)) != std::string::npos) { ++n; p += needle.size(); }
    return n;
}

// Extract a function body (from the comment/string-stripped source): the first '{' after
// the signature token, to its brace match.
static bool extractFn(const std::string& src, const std::string& sig, std::string& body) {
    const size_t p = src.find(sig);
    if (p == std::string::npos) return false;
    const size_t b = src.find('{', p);
    if (b == std::string::npos) return false;
    int depth = 0;
    for (size_t i = b; i < src.size(); ++i) {
        if (src[i] == '{') ++depth;
        else if (src[i] == '}') {
            if (--depth == 0) { body = src.substr(b, i - b + 1); return true; }
        }
    }
    return false;
}

int main() {
    std::string cppRaw, ctrl, mon, og, lock;
    const bool slurped = slurpFile("src/dsp/engines/SequencerEngine.cpp", cppRaw)
                      && slurpFile("src/dsp/managers/MonsoonModeController.hpp", ctrl)
                      && slurpFile("src/Monsoon.cpp", mon)
                      && slurpFile("src/dsp/managers/MonsoonOutputGenerator.cpp", og)
                      && slurpFile("src/dsp/managers/MonsoonLockManager.hpp", lock);
    check(slurped, "the sources slurp (run from the repo root)");
    if (!slurped) { std::printf("\n%d passed, %d failed\n", g_pass, g_fail); return 1; }

    const std::string cpp = stripStrings(stripComments(cppRaw));

    // ── GATE 1: the migrated consumer bodies carry NO bare stepIndex and DO read the
    // resolver (reproduce-first: each FAILED on the pre-slice engine) ─────────────────
    struct Cons { const char* sig; const char* name; };
    const Cons cons[] = {
        { "SequencerEngine::getStepLightBrightness(", "getStepLightBrightness (the ring lamp + trail)" },
        { "SequencerEngine::getBarLightBrightness(",  "getBarLightBrightness (the inner-disc bar LEDs)" },
        { "SequencerEngine::shouldTriggerStep(",      "shouldTriggerStep (the 1/4-beat trigger)" },
        { "SequencerEngine::executeStep(",            "executeStep (the whole body, forStep included)" },
    };
    for (const Cons& c : cons) {
        std::string body;
        const bool got = extractFn(cpp, c.sig, body);
        const int oldReads = got ? countToken(body, "stepIndex") : -1;
        const int newReads = got ? countToken(body, "pos_.playedColumn()") : 0;
        check(got && oldReads == 0 && newReads >= 1,
              std::string(c.name) + ": zero bare stepIndex reads, >=1 pos_.playedColumn()");
    }

    // ── GATE 2: executeStep tags BOTH results with the played column ─────────────────
    check(countOccurrences(cpp, "result.forStep = pos_.playedColumn();") == 2,
          "executeStep: result.forStep = pos_.playedColumn() at BOTH sites (MidNote + tail)");

    // ── GATE 3 (STEP 4-final): the AUTHORITY bodies carry NO stepIndex token at all —
    // the field is RETIRED; pos_ is the sole position authority ────────────────────
    const Cons auth[] = {
        { "SequencerEngine::setWindow(",       "setWindow (the window re-anchor)" },
        { "SequencerEngine::advancePlayhead(", "advancePlayhead (the traversal)" },
        { "SequencerEngine::snapToPhaseStep(", "snapToPhaseStep (the phase snap)" },
    };
    for (const Cons& c : auth) {
        std::string body;
        const bool got = extractFn(cpp, c.sig, body);
        check(got && countToken(body, "stepIndex") == 0,
              std::string(c.name) + ": ZERO stepIndex tokens (the field retired — pos_ is the position)");
    }

    // ── GATE 4: the manager feeds read the resolver (the two Rodney rulings) ─────────
    check(ctrl.find("lastStepIndex = engine.pos_.playedColumn()") != std::string::npos,
          "MonsoonModeController::updateLastStepIndex reads engine.pos_.playedColumn()");
    check(mon.find("lockManager.tick(engine.pos_.playedColumn())") != std::string::npos,
          "Monsoon's lock tick feeds engine.pos_.playedColumn() (the one-token transport change)");
    check(mon.find("lockManager.tick(engine.stepIndex)") == std::string::npos,
          "Monsoon's lock tick no longer feeds the bare stepIndex");
    check(og.find("const int maskStep = engine.pos_.playedColumn()") != std::string::npos,
          "MonsoonOutputGenerator's mask step reads engine.pos_.playedColumn()");
    check(og.find("maskStep = engine.stepIndex") == std::string::npos,
          "MonsoonOutputGenerator no longer reads the bare stepIndex for the mask");
    check(lock.find("void tick(int playedColumn)") != std::string::npos,
          "MonsoonLockManager::tick's parameter IS the played column (fed by the caller)");

    // ── GATE 5 (STEP 4-final — the retirement): the ENGINE sources carry NO stepIndex
    // token at all — the field, the mirror writes and the prevStep capture are gone;
    // wrapped reads wasStarted && pos_.atPhraseEdge(transportTick_) (C3 — the draw edge moved onto
    // the tick; see CC_PROMPT_POSITION_RESOLVER_C3_UNIT_OF_ABSOLUTENESS); the snap's landing is a local ──
    {
        std::string ap, sw, sp;
        const bool got = extractFn(cpp, "SequencerEngine::advancePlayhead(", ap)
                      && extractFn(cpp, "SequencerEngine::setWindow(", sw)
                      && extractFn(cpp, "SequencerEngine::snapToPhaseStep(", sp);
        check(got, "the three authority bodies extract");
        if (got) {
            check(ap.find("const bool wasStarted = (pos_.playedColumn() != -1);") != std::string::npos
                   && ap.find("const bool wrapped = (wasStarted && pos_.atPhraseEdge(transportTick_));") != std::string::npos,
                  "advancePlayhead: wasStarted captured before the advance + wrapped from wasStarted && pos_.atPhraseEdge(transportTick_)");
            check(countToken(ap, "stepIndex") == 0 && countToken(sw, "stepIndex") == 0
                   && countToken(sp, "stepIndex") == 0,
                  "the three authority bodies: ZERO stepIndex tokens (the field retired)");
            check(sp.find("int snapCol = phaseStep;") != std::string::npos,
                  "snapToPhaseStep: the window defence computes the LOCAL snapCol");
        }
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
