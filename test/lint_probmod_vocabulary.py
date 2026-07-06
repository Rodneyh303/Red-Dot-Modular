#!/usr/bin/env python3
"""
lint_probmod_vocabulary.py — Step 5 of the probability-modifier unification
(docs/design/PROBABILITY_MODIFIER_MODEL.md).

Locks the vocabulary so the completed unifications can't silently drift back. Each unification
DELETED a scattered representation in favour of one authority; this lint fails if any of those
deleted forms reappear in LIVE code (comments and src/deprecated/ are exempt — the archive and
historical notes may still name the old things).

It only enforces locks for unifications that have LANDED on master. Locks for steps still on
branches (Step 4 switch collapse, Step 3c macro-spread) are listed as PENDING and activated by
flipping their `active` flag once merged — keeping the lint honest: it never fails on legitimate
mid-arc code.

Run:  python3 test/lint_probmod_vocabulary.py      (exit 0 = clean, 1 = a forbidden form returned)
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC  = os.path.join(REPO, "src")

# (name, regex, why, active) — active=False = documented-but-not-yet-enforced (step still on a branch)
LOCKS = [
    ("named LOR fields",
     r"\b(rhythm|variation|legato|accent|melody|octave)(Len|Off|Rot)\b",
     "LOR is one array lorStore_[16][6][3]; use lor()/lorRef()/polyLOR() — not named fields (Step 1/2).",
     True),
    ("raw poly LOR array access",
     r"\bpoly(Len|Off|Rot)\s*\[",
     "poly LOR lives in lorStore_; use polyLOR()/polyLORRef()/polyLenE() accessors — not raw arrays (Step 2).",
     True),
    ("LOR strand→field switch",
     r"case\s+dotModular::STRAND_\w+:\s*return\s+(rhythm|variation|legato|accent|melody|octave)(Len|Off|Rot)",
     "The strand→LOR-field switches were collapsed to the lorStore_ addressing (Step 1).",
     True),
    ("applyMonoSprCV",
     r"\bapplyMonoSprCV\b",
     "Mono spread CV is inside redDot::SpreadResolver now (Step 3b).",
     True),
    ("intermediate monoLOR name",
     r"\bmonoLOR\b",
     "monoLOR[6][3] was subsumed by lorStore_[16][6][3] as slot 0 (Step 2b-ii).",
     True),

    # ── Activated once their steps landed on master (Step 3c, Step 4) ──
    ("applyMacroSprCV",
     r"\bapplyMacroSprCV\b",
     "Macro spread CV is inside redDot::SpreadResolver now (Step 3c).",
     True),
    ("finalRandomByStrand switch",
     r"case\s+dotModular::STRAND_\w+:\s*return\s+\w*Random\[",
     "The strand→*Random switch was collapsed to a pointer-to-member table (Step 4).",
     True),
]

# Files/paths exempt entirely.
EXEMPT_DIRS = (os.path.join("src", "deprecated"),)
# The lint itself and the design doc obviously name the forbidden forms.
EXEMPT_FILES = set()

def strip_comments(line: str) -> str:
    # Drop // comments; crude but sufficient (we only need to avoid matching commentary).
    i = line.find("//")
    return line[:i] if i >= 0 else line

def iter_src():
    for root, _dirs, files in os.walk(SRC):
        rel = os.path.relpath(root, REPO)
        if any(rel.startswith(d) for d in EXEMPT_DIRS):
            continue
        for f in files:
            if f.endswith((".cpp", ".hpp", ".h")):
                yield os.path.join(root, f)

def main() -> int:
    violations = []
    block_comment = False
    for path in iter_src():
        with open(path, encoding="utf-8", errors="replace") as fh:
            block_comment = False
            for n, raw in enumerate(fh, 1):
                line = raw
                # skip /* ... */ block comment bodies (line-granular, good enough)
                if block_comment:
                    if "*/" in line: block_comment = False
                    continue
                if "/*" in line and "*/" not in line:
                    block_comment = True
                code = strip_comments(line)
                for name, pat, why, active in LOCKS:
                    if not active:
                        continue
                    if re.search(pat, code):
                        violations.append((path, n, name, why, raw.rstrip()))

    if violations:
        print("\033[31mprobmod vocabulary lint: FORBIDDEN forms returned to live code\033[0m\n")
        for path, n, name, why, text in violations:
            rel = os.path.relpath(path, REPO)
            print(f"  {rel}:{n}  [{name}]\n      {text.strip()}\n      → {why}\n")
        print(f"{len(violations)} violation(s). These forms were deleted by the probability-modifier "
              f"unification; route through the single authority instead.")
        return 1

    active = [l for l in LOCKS if l[3]]
    pending = [l for l in LOCKS if not l[3]]
    print(f"\033[32mprobmod vocabulary lint: clean\033[0m  "
          f"({len(active)} locks enforced, {len(pending)} pending merge)")
    for name, _p, _w, _a in pending:
        print(f"    pending: {name}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
