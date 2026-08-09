#pragma once
// ── dotModular::ScalaFile — shared Scala (.scl) tuning-file parser ────────────────────────────
// One parser, many callers (Sikit Phase 1; Micro-12 / Micro-24 later). The PARSING is identical
// across callers; only the ACCEPT criterion (degree count) varies, so callers pass an `acceptFn`
// predicate. See docs/design/SCALA_FILE_AND_LOAD_UI.md.
//
// PURE C++: depends only on the standard library (<string>,<vector>,<sstream>,<cmath>,<functional>,
// <fstream>). NO Rack headers — so it is unit-testable in the container (test/test_ScalaFile.cpp)
// and reusable outside the plugin. The file-picker UI (osdialog) lives in the widget, NOT here.
//
// .scl format (https://www.huygens-fokker.org/scala/scl_format.html):
//   - Lines starting with '!' are COMMENTS (skipped entirely).
//   - First non-comment line: DESCRIPTION (free-form; may be empty).
//   - Second non-comment line: DEGREE COUNT (positive integer).
//   - Following non-comment lines: one pitch per degree, each either
//       * CENTS  — contains a decimal point, e.g. "701.955"
//       * RATIO  — "num/den" (or a bare integer = num/1), e.g. "3/2", "2/1", "2"
//   - Trailing content on a pitch line (after the value) is ignored (inline annotations).
//   - Root (degree 0) is implicit 0 cents and is NOT stored; centsFromRoot[] holds degrees 1..N.

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cmath>
#include <functional>
#include <cstdlib>

namespace dotModular {

struct ScalaFile {
    // Parsed contents. Units are CENTS from root; root (0 cents) is implicit and NOT stored.
    std::string description;            // the .scl description line (first non-comment line)
    std::vector<float> centsFromRoot;   // size = degree count; centsFromRoot[i] = degree (i+1)'s cents

    std::string sourcePath;             // populated by loadScala; empty for parse-from-string

    enum class Status { Ok, IoError, ParseError, RejectedByPredicate };
    Status status = Status::Ok;
    std::string errorMessage;           // human-readable, safe to show verbatim in a dialog

    bool ok() const { return status == Status::Ok; }
    int  degreeCount() const { return (int)centsFromRoot.size(); }
};

namespace scala_detail {

    // Strip leading/trailing whitespace (incl. CR for Windows line endings).
    inline std::string trim(const std::string& s) {
        const char* ws = " \t\r\n\f\v";
        const size_t b = s.find_first_not_of(ws);
        if (b == std::string::npos) return "";
        const size_t e = s.find_last_not_of(ws);
        return s.substr(b, e - b + 1);
    }

    // The first whitespace-delimited token of a (trimmed) line — the pitch value; the rest is
    // inline annotation to ignore.
    inline std::string firstToken(const std::string& s) {
        std::istringstream iss(s);
        std::string tok;
        iss >> tok;
        return tok;
    }

    // Parse a single pitch token to cents. Sets ok=false on malformed input.
    inline float pitchToCents(const std::string& tok, bool& ok) {
        ok = true;
        if (tok.empty()) { ok = false; return 0.f; }
        if (tok.find('.') != std::string::npos) {
            // CENTS: floating-point value.
            try {
                size_t used = 0;
                float c = std::stof(tok, &used);
                if (used == 0) { ok = false; return 0.f; }
                return c;
            } catch (...) { ok = false; return 0.f; }
        }
        // RATIO: "num/den" or a bare integer ("2" == 2/1).
        const size_t slash = tok.find('/');
        long num = 0, den = 1;
        if (slash == std::string::npos) {
            char* end = nullptr;
            num = std::strtol(tok.c_str(), &end, 10);
            if (end == tok.c_str() || *end != '\0') { ok = false; return 0.f; }
        } else {
            const std::string ns = tok.substr(0, slash);
            const std::string ds = tok.substr(slash + 1);
            char* e1 = nullptr; char* e2 = nullptr;
            num = std::strtol(ns.c_str(), &e1, 10);
            den = std::strtol(ds.c_str(), &e2, 10);
            if (e1 == ns.c_str() || *e1 != '\0' || e2 == ds.c_str() || *e2 != '\0') { ok = false; return 0.f; }
        }
        if (num <= 0 || den <= 0) { ok = false; return 0.f; }
        return 1200.f * std::log2((float)num / (float)den);
    }

} // namespace scala_detail

// Parse from an already-loaded string. `acceptFn` (if provided) is called with the DECLARED degree
// count; if it returns false the result is RejectedByPredicate with `rejectMessage`. This lets each
// caller enforce its own degree-count constraint without duplicating parse code.
inline ScalaFile parseScala(
    const std::string& text,
    std::function<bool(int)> acceptFn = nullptr,
    const std::string& rejectMessage = "This tuning file has an unsupported degree count.") {

    using namespace scala_detail;
    ScalaFile sf;

    // State machine over non-comment lines. The description is positional (first non-comment line,
    // even if blank), so we do NOT skip blank lines while seeking it; for the count and pitches we
    // skip blank lines leniently (real files vary).
    enum { NEED_DESC, NEED_COUNT, NEED_PITCHES } state = NEED_DESC;
    int declaredCount = 0;

    std::istringstream stream(text);
    std::string raw;
    while (std::getline(stream, raw)) {
        const std::string line = trim(raw);
        if (!line.empty() && line[0] == '!') continue;   // comment

        switch (state) {
            case NEED_DESC:
                sf.description = line;   // may be empty
                state = NEED_COUNT;
                break;

            case NEED_COUNT: {
                if (line.empty()) break;   // lenient: skip blanks before the count
                const std::string tok = firstToken(line);
                char* end = nullptr;
                long n = std::strtol(tok.c_str(), &end, 10);
                if (end == tok.c_str() || *end != '\0' || n <= 0) {
                    sf.status = ScalaFile::Status::ParseError;
                    sf.errorMessage = "Could not parse .scl file: expected a positive integer degree "
                                      "count, got \"" + tok + "\". See "
                                      "https://www.huygens-fokker.org/scala/scl_format.html for format details.";
                    return sf;
                }
                declaredCount = (int)n;

                // Apply the caller's accept predicate on the declared count BEFORE reading pitches.
                if (acceptFn && !acceptFn(declaredCount)) {
                    sf.status = ScalaFile::Status::RejectedByPredicate;
                    sf.errorMessage = rejectMessage;
                    return sf;
                }
                sf.centsFromRoot.reserve(declaredCount);
                state = NEED_PITCHES;
                break;
            }

            case NEED_PITCHES: {
                if (line.empty()) break;   // lenient: skip blanks between pitches
                if ((int)sf.centsFromRoot.size() >= declaredCount) break;   // extras ignored (rule 8)
                const std::string tok = firstToken(line);
                bool pok = false;
                const float cents = pitchToCents(tok, pok);
                if (!pok) {
                    sf.status = ScalaFile::Status::ParseError;
                    sf.errorMessage = "Could not parse .scl file: malformed pitch value \"" + tok +
                                      "\". Expected cents (e.g. 701.955) or a ratio (e.g. 3/2). See "
                                      "https://www.huygens-fokker.org/scala/scl_format.html for format details.";
                    return sf;
                }
                sf.centsFromRoot.push_back(cents);
                break;
            }
        }
    }

    // Never saw a degree count (empty file / comments only / description only).
    if (state != NEED_PITCHES) {
        sf.status = ScalaFile::Status::ParseError;
        sf.errorMessage = "Could not parse .scl file: no degree count found. See "
                          "https://www.huygens-fokker.org/scala/scl_format.html for format details.";
        return sf;
    }

    // Too few pitches for the declared count (rule 7).
    if ((int)sf.centsFromRoot.size() < declaredCount) {
        sf.status = ScalaFile::Status::ParseError;
        std::ostringstream os;
        os << "Could not parse .scl file: declared " << declaredCount << " degrees but found only "
           << sf.centsFromRoot.size() << " pitch values.";
        sf.errorMessage = os.str();
        return sf;
    }

    sf.status = ScalaFile::Status::Ok;
    return sf;
}

// Load and parse from a file path. Wraps parseScala with file-IO error handling. UI thread only —
// never call from process() (no file I/O on the audio thread).
inline ScalaFile loadScala(
    const std::string& filePath,
    std::function<bool(int)> acceptFn = nullptr,
    const std::string& rejectMessage = "This tuning file has an unsupported degree count.") {

    ScalaFile sf;
    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        sf.status = ScalaFile::Status::IoError;
        sf.errorMessage = "Could not read file: " + filePath + ".";
        sf.sourcePath = filePath;
        return sf;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    sf = parseScala(ss.str(), acceptFn, rejectMessage);
    sf.sourcePath = filePath;
    return sf;
}

// ── Writing ──────────────────────────────────────────────────────────────────────────────────
// Serialise a list of per-degree cents (NOT including the implicit root 0) to standard .scl text.
// `centsFromRoot` holds degrees 1..N in cents (the same shape as ScalaFile::centsFromRoot). The
// period/octave is the caller's responsibility to include as the LAST entry (Scala convention);
// callers that store only within-octave degrees should append 1200.0 before calling. Cents are
// written with a decimal point so they parse back as cents (not ratios). description → line 1.
inline std::string writeScala(const std::vector<float>& centsFromRoot,
                              const std::string& description = "") {
    std::ostringstream os;
    os << "! Scala tuning exported by dot.modular\n";
    os << (description.empty() ? std::string("Exported tuning") : description) << "\n";
    os << " " << centsFromRoot.size() << "\n";
    os << "!\n";
    for (float c : centsFromRoot) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), " %.5f", (double)c);   // '.' → parsed as cents on reload
        os << buf << "\n";
    }
    return os.str();
}

// Write to a file path. Returns true on success. UI thread only (no file I/O on the audio thread).
inline bool saveScala(const std::string& filePath,
                      const std::vector<float>& centsFromRoot,
                      const std::string& description = "") {
    std::ofstream out(filePath, std::ios::binary);
    if (!out) return false;
    out << writeScala(centsFromRoot, description);
    return (bool)out;
}

} // namespace dotModular
