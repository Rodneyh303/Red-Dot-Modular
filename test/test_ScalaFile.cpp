// test_ScalaFile.cpp — dotModular::ScalaFile (.scl) parser spec test (Sikit Phase 1, Step A).
//
// Pure C++ / no Rack: the parser is header-only and stdlib-only, so this test builds standalone.
// Covers SCALA_FILE_AND_LOAD_UI.md §".scl format reference" rules + the per-caller acceptFn
// predicate. Run via test/run_all.sh (registered as "test_ScalaFile|").
//
// Build (standalone):
//   g++ -std=c++17 -Isrc test/test_ScalaFile.cpp -o /tmp/tsf && /tmp/tsf

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

#include "tuning/ScalaFile.hpp"

using dotModular::ScalaFile;
using dotModular::parseScala;

#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT(e) do { if(!(e)) throw std::runtime_error("EXPECT(" #e ") failed"); } while(0)
#define EXPECT_EQ(a,b) do { if((a)!=(b)) { std::ostringstream _s; \
    _s << "EXPECT_EQ(" #a "," #b ") : " << (long long)(a) << " != " << (long long)(b); \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_NEAR(a,b,eps) do { double _d = std::fabs((double)(a)-(double)(b)); \
    if(_d > (eps)) { std::ostringstream _s; \
    _s << "EXPECT_NEAR(" #a "," #b ") : " << (double)(a) << " vs " << (double)(b) << " (|d|=" << _d << ")"; \
    throw std::runtime_error(_s.str()); } } while(0)

static int g_pass = 0, g_fail = 0;

// A canonical equal-division 12-TET .scl (cents form), last degree = octave 1200.
static const char* TET12 =
    "! equal.scl\n"
    "!\n"
    "12-tone equal temperament\n"
    " 12\n"
    "!\n"
    " 100.0\n"
    " 200.0\n"
    " 300.0\n"
    " 400.0\n"
    " 500.0\n"
    " 600.0\n"
    " 700.0\n"
    " 800.0\n"
    " 900.0\n"
    " 1000.0\n"
    " 1100.0\n"
    " 1200.0\n";

int main() {
    SUITE("parse: equal-division 12-TET (cents)");
    TEST("ok status, 12 degrees, description captured", {
        auto sf = parseScala(TET12);
        EXPECT(sf.ok());
        EXPECT_EQ(sf.degreeCount(), 12);
        EXPECT(sf.description == "12-tone equal temperament");
    });
    TEST("cents values are i*100 for degrees 1..12", {
        auto sf = parseScala(TET12);
        for (int i = 0; i < 12; ++i) EXPECT_NEAR(sf.centsFromRoot[i], (i + 1) * 100.f, 1e-3);
    });

    SUITE("parse: ratios convert to cents");
    TEST("3/2 -> 701.955 cents; 2/1 -> 1200", {
        const char* s = "just fifth+octave\n 2\n 3/2\n 2/1\n";
        auto sf = parseScala(s);
        EXPECT(sf.ok());
        EXPECT_EQ(sf.degreeCount(), 2);
        EXPECT_NEAR(sf.centsFromRoot[0], 701.955f, 1e-2);
        EXPECT_NEAR(sf.centsFromRoot[1], 1200.f,  1e-3);
    });
    TEST("bare integer is num/1 (2 -> octave 1200)", {
        const char* s = "octave only\n 1\n 2\n";
        auto sf = parseScala(s);
        EXPECT(sf.ok());
        EXPECT_NEAR(sf.centsFromRoot[0], 1200.f, 1e-3);
    });
    TEST("3-limit just intonation mixes ratios cleanly", {
        // 9/8 (major second) = 203.910 cents
        const char* s = "pythag\n 2\n 9/8\n 2/1\n";
        auto sf = parseScala(s);
        EXPECT(sf.ok());
        EXPECT_NEAR(sf.centsFromRoot[0], 203.910f, 1e-2);
    });

    SUITE("parse: comments + whitespace + inline annotations");
    TEST("leading comments and blank lines skipped; inline annotation after value ignored", {
        const char* s =
            "! header comment\n"
            "!\n"
            "well-tempered\n"
            "!\n"
            " 3\n"
            "\n"
            " 90.225   ! flat-ish minor second\n"
            "\t203.91\t! tab-separated annotation\n"
            " 2/1 octave\n";   // trailing token after ratio ignored
        auto sf = parseScala(s);
        EXPECT(sf.ok());
        EXPECT_EQ(sf.degreeCount(), 3);
        EXPECT_NEAR(sf.centsFromRoot[0], 90.225f, 1e-3);
        EXPECT_NEAR(sf.centsFromRoot[1], 203.91f, 1e-3);
        EXPECT_NEAR(sf.centsFromRoot[2], 1200.f,  1e-3);
    });
    TEST("empty description line is allowed (positional)", {
        const char* s =
            "\n"          // description is blank
            " 1\n"
            " 500.0\n";
        auto sf = parseScala(s);
        EXPECT(sf.ok());
        EXPECT(sf.description.empty());
        EXPECT_EQ(sf.degreeCount(), 1);
        EXPECT_NEAR(sf.centsFromRoot[0], 500.f, 1e-3);
    });

    SUITE("parse: extra pitches beyond count are ignored (rule 8)");
    TEST("declares 2, provides 3 -> keeps first 2, ok", {
        const char* s = "extra\n 2\n 100.0\n 200.0\n 300.0\n";
        auto sf = parseScala(s);
        EXPECT(sf.ok());
        EXPECT_EQ(sf.degreeCount(), 2);
        EXPECT_NEAR(sf.centsFromRoot[1], 200.f, 1e-3);
    });

    SUITE("malformed: clear ParseError, no crash");
    TEST("missing degree count (comments only) -> ParseError", {
        const char* s = "! only comments\n!\n";
        auto sf = parseScala(s);
        EXPECT(!sf.ok());
        EXPECT(sf.status == ScalaFile::Status::ParseError);
    });
    TEST("non-numeric degree count -> ParseError", {
        const char* s = "desc\n twelve\n 100.0\n";
        auto sf = parseScala(s);
        EXPECT(!sf.ok());
        EXPECT(sf.status == ScalaFile::Status::ParseError);
    });
    TEST("too few pitches (declared 12, gave 11) -> ParseError", {
        std::ostringstream os;
        os << "short\n 12\n";
        for (int i = 1; i <= 11; ++i) os << " " << (i * 100) << ".0\n";   // only 11
        auto sf = parseScala(os.str());
        EXPECT(!sf.ok());
        EXPECT(sf.status == ScalaFile::Status::ParseError);
    });
    TEST("junk pitch value -> ParseError", {
        const char* s = "junk\n 2\n 100.0\n banana\n";
        auto sf = parseScala(s);
        EXPECT(!sf.ok());
        EXPECT(sf.status == ScalaFile::Status::ParseError);
    });
    TEST("zero/negative ratio denominator -> ParseError", {
        const char* s = "bad\n 1\n 3/0\n";
        auto sf = parseScala(s);
        EXPECT(!sf.ok());
        EXPECT(sf.status == ScalaFile::Status::ParseError);
    });
    TEST("empty input -> ParseError, errorMessage non-empty", {
        auto sf = parseScala("");
        EXPECT(!sf.ok());
        EXPECT(sf.status == ScalaFile::Status::ParseError);
        EXPECT(!sf.errorMessage.empty());
    });

    SUITE("acceptFn predicate (per-caller degree constraint)");
    TEST("Sikit 12-only: 12-note file accepted", {
        auto sf = parseScala(TET12, [](int n){ return n == 12; },
                             "Sikit reads 12-note .scl files only.");
        EXPECT(sf.ok());
    });
    TEST("Sikit 12-only: 7-note file rejected with RejectedByPredicate + message", {
        std::ostringstream os;
        os << "penta\n 7\n";
        for (int i = 1; i <= 7; ++i) os << " " << (i * 150) << ".0\n";
        auto sf = parseScala(os.str(), [](int n){ return n == 12; },
                             "Sikit reads 12-note .scl files only.");
        EXPECT(!sf.ok());
        EXPECT(sf.status == ScalaFile::Status::RejectedByPredicate);
        EXPECT(sf.errorMessage == "Sikit reads 12-note .scl files only.");
    });
    TEST("predicate rejection happens BEFORE pitch parsing (no wasted work / no parse error masking)", {
        // A file whose pitches are junk but whose COUNT is rejected: must report predicate rejection,
        // not a pitch ParseError (predicate is checked at the count, before pitches).
        const char* s = "x\n 5\n junk\n junk\n junk\n junk\n junk\n";
        auto sf = parseScala(s, [](int n){ return n == 12; }, "need 12");
        EXPECT(sf.status == ScalaFile::Status::RejectedByPredicate);
    });
    TEST("Micro-style up-to-12 predicate accepts a 7-note file", {
        std::ostringstream os;
        os << "penta\n 7\n";
        for (int i = 1; i <= 7; ++i) os << " " << (i * 150) << ".0\n";
        auto sf = parseScala(os.str(), [](int n){ return n >= 1 && n <= 12; }, "up to 12");
        EXPECT(sf.ok());
        EXPECT_EQ(sf.degreeCount(), 7);
    });

    std::cout << "\n-----\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
