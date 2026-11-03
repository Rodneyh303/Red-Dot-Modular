#!/usr/bin/env python3
# lint_array_guard.py — P-E GUARDRAIL (ARRAY_CONTAINER_AUDIT section 4 P-E): fail on any
# NEW bare C-array declaration in src/dsp/** and src/ui/** outside the allowlist.
#
# The audit converted the named files' member state to std::array (P-A/P-B/P-C). This gate
# keeps the codebase modern: any NEW `T name[N]` declaration in the audit's scope must be
# either (a) added to test/lint_array_guard_allowlist.txt with a reason, or (b) converted.
# Allowlist classes the audit itself exempts: char string buffers, label/NVGcolor tables.
#
# Usage: python3 test/lint_array_guard.py            # exit 1 on violations
#        ARRAY_GUARD_STRICT=0 python3 ...            # local debug hatch (repo precedent)
import os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SCOPE_DIRS = [os.path.join(ROOT, "src", "dsp"), os.path.join(ROOT, "src", "ui")]
ALLOWLIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "lint_array_guard_allowlist.txt")

# A bare C-array DECLARATION statement: TYPE name[dims...] (= init)? ;
# Not matched: parameters (a '(' or ')' occurs before the type on the line — heuristic that
# holds for member/local declarations, which never carry parens before the type), `char`
# string buffers, `new T[n]`, subscript EXPRESSIONS (a preceding token on the line), typedefs.
TYPES = r"(?:unsigned\s+|signed\s+|const\s+|static\s+|constexpr\s+|mutable\s+|inline\s+|alignas\([^)]*\)\s+)*" \
        r"(?:[A-Za-z_][\w]*(?:::[\w:]+)*)"
DECL = re.compile(
    r"^(?P<pre>(?!\s*//)(?![^/]*\()\s*)"          # statement start; no '(' before the type
    r"(?P<qual>(?:(?:static|constexpr|const|mutable|inline|alignas\([^)]*\)|unsigned|signed)\s+)*)"
    r"(?P<type>[A-Za-z_][\w]*(?:::[\w]+)*(?:\s+(?:int8_t|uint8_t|int16_t|uint16_t|int32_t|uint32_t|int64_t|uint64_t))?)"
    r"\s+(?P<name>[A-Za-z_]\w*)"
    r"(?P<dims>(?:\s*\[\s*[^\]]*\s*\])+)"
    r"\s*(?:=[^;]*)?;"
)
EXEMPT_TYPE = re.compile(r"^\s*(?:char\b|const\s+char\b|wchar_t\b|NVGcolor\b|std::array\b|auto\b)")
# expression subscripts like `buf[k++] = ...` — excluded because a name/type pair precedes them;
# the regex above already requires TYPE immediately before the name.

def load_allowlist():
    entries = set()
    if os.path.exists(ALLOWLIST):
        for ln in open(ALLOWLIST):
            ln = ln.split("#", 1)[0].strip()
            if ln:
                entries.add(ln)
    return entries

def scan():
    violations = []
    for base in SCOPE_DIRS:
        for dirpath, _, files in os.walk(base):
            for fn in sorted(files):
                if not fn.endswith((".hpp", ".cpp", ".h", ".cc")):
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
                for i, line in enumerate(open(path, errors="replace"), 1):
                    if EXEMPT_TYPE.match(line.strip()) and not re.match(r"\s*(?:static|constexpr|const)\s+uint", line):
                        continue
                    m = DECL.match(line.rstrip())
                    if not m:
                        continue
                    if EXEMPT_TYPE.match(m.group("type")) or m.group("type").startswith("char"):
                        continue
                    key = f"{rel}:{m.group('name')}"
                    violations.append((key, i, line.rstrip()))
    return violations

def main():
    if os.environ.get("ARRAY_GUARD_STRICT", "1") == "0":
        print("  array-guard: SKIPPED (ARRAY_GUARD_STRICT=0)")
        return 0
    allowed = load_allowlist()
    bad = [(k, i, l) for (k, i, l) in scan() if k not in allowed]
    if bad:
        print(f"  \033[31m✗\033[0m  array-guard: {len(bad)} bare C-array declaration(s) outside the allowlist "
              f"(ARRAY_CONTAINER_AUDIT P-E):")
        for key, i, l in bad[:12]:
            print(f"       {key}:{i}: {l.strip()[:90]}")
        if len(bad) > 12:
            print(f"       ... and {len(bad) - 12} more")
        print("       Fix: convert to std::array (P-B form) or add path:name to test/lint_array_guard_allowlist.txt with a reason.")
        return 1
    print("  \033[32m✓\033[0m  array-guard: no new bare C-array declarations (allowlist %d)" % len(allowed))
    return 0

if __name__ == "__main__":
    sys.exit(main())
